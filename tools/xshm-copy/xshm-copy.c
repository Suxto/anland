// SPDX-License-Identifier: MIT
// Private DS3 experiment: retain Mesa's completed CPU image, replace only large
// X11 PutImage payloads with MIT-SHM. Never exports GPU buffers or changes policy.
#define _GNU_SOURCE
#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>

typedef xcb_void_cookie_t (*put_fn)(xcb_connection_t *, uint8_t, xcb_drawable_t,
    xcb_gcontext_t, uint16_t, uint16_t, int16_t, int16_t, uint8_t, uint8_t,
    uint32_t, const uint8_t *);
typedef void (*disconnect_fn)(xcb_connection_t *);
static put_fn original_put;
static disconnect_fn original_disconnect;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct {
    xcb_connection_t *connection;
    xcb_shm_seg_t segment;
    int id;
    uint8_t *memory;
    size_t capacity;
} cache = {.id = -1};
static uint64_t copied_calls, copied_bytes, fallback_calls;
static int disabled;

static void drop_mapping(void)
{
    if (cache.memory) shmdt(cache.memory);
    if (cache.id >= 0) shmctl(cache.id, IPC_RMID, NULL);
    memset(&cache, 0, sizeof(cache));
    cache.id = -1;
}

static void release_cache(void)
{
    if (cache.connection && cache.segment && !xcb_connection_has_error(cache.connection)) {
        xcb_void_cookie_t cookie = xcb_shm_detach_checked(cache.connection, cache.segment);
        free(xcb_request_check(cache.connection, cookie));
    }
    drop_mapping();
}

static void before_fork(void) { pthread_mutex_lock(&lock); }
static void after_fork_parent(void) { pthread_mutex_unlock(&lock); }
static void after_fork_child(void)
{
    // The server attachment belongs to the parent's connection. Drop only this
    // child's inherited local mapping; IPC_RMID was already set by the creator.
    if (cache.memory) shmdt(cache.memory);
    memset(&cache, 0, sizeof(cache));
    cache.id = -1;
    copied_calls = copied_bytes = fallback_calls = 0;
    pthread_mutex_unlock(&lock);
}

__attribute__((constructor)) static void initialize(void)
{
    original_put = (put_fn)dlsym(RTLD_NEXT, "xcb_put_image");
    original_disconnect = (disconnect_fn)dlsym(RTLD_NEXT, "xcb_disconnect");
    if (!original_put || !original_disconnect) {
        fputs("ANLAND_XSHM_COPY: incompatible libxcb; aborting private probe\n", stderr);
        _exit(127);
    }
    disabled = getenv("ANLAND_XSHM_DISABLE") != NULL;
    if (pthread_atfork(before_fork, after_fork_parent, after_fork_child) != 0)
        disabled = 1;
}

// Called only under lock. Attach read-only for the X server, mode 0600, and mark
// for removal after attachment. Normal exit, crash, and server loss release it.
static int prepare_cache(xcb_connection_t *c, size_t size)
{
    if (cache.connection == c && cache.capacity >= size) return 1;
    release_cache();
    const xcb_query_extension_reply_t *extension = xcb_get_extension_data(c, &xcb_shm_id);
    if (!extension || !extension->present || xcb_connection_has_error(c)) return 0;
    cache.id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
    if (cache.id < 0) return 0;
    void *address = shmat(cache.id, NULL, 0);
    if (address == (void *)-1) {
        drop_mapping();
        return 0;
    }
    cache.memory = address;
    xcb_shm_seg_t segment = xcb_generate_id(c);
    xcb_void_cookie_t request = xcb_shm_attach_checked(c, segment, cache.id, 1);
    xcb_generic_error_t *error = xcb_request_check(c, request);
    if (error || xcb_connection_has_error(c)) {
        free(error);
        drop_mapping();
        return 0;
    }
    cache.connection = c;
    cache.segment = segment;
    cache.capacity = size;
    if (shmctl(cache.id, IPC_RMID, NULL) != 0) {
        release_cache();
        return 0;
    }
    cache.id = -1; // already marked; do not operate on a potentially reused id
    return 1;
}

static int supported(xcb_connection_t *c, uint8_t format, uint8_t depth,
                     uint8_t pad, uint16_t width, uint16_t height, uint32_t size)
{
    if (disabled || !c || pad || format != XCB_IMAGE_FORMAT_Z_PIXMAP ||
        (depth != 24 && depth != 32) || size < 65536 || size > 64U * 1024U * 1024U ||
        !width || !height || (uint64_t)width * height * 4 != size)
        return 0;
    const xcb_setup_t *setup = xcb_get_setup(c);
    if (!setup || setup->image_byte_order != XCB_IMAGE_ORDER_LSB_FIRST) return 0;
    for (xcb_format_iterator_t it = xcb_setup_pixmap_formats_iterator(setup); it.rem; xcb_format_next(&it))
        if (it.data->depth == depth)
            return it.data->bits_per_pixel == 32 && it.data->scanline_pad == 32;
    return 0;
}

xcb_void_cookie_t xcb_put_image(xcb_connection_t *c, uint8_t format,
    xcb_drawable_t drawable, xcb_gcontext_t gc, uint16_t width, uint16_t height,
    int16_t x, int16_t y, uint8_t pad, uint8_t depth, uint32_t size, const uint8_t *data)
{
    pthread_mutex_lock(&lock);
    if (data && supported(c, format, depth, pad, width, height, size) && prepare_cache(c, size)) {
        memcpy(cache.memory, data, size);
        xcb_void_cookie_t request = xcb_shm_put_image_checked(c, drawable, gc,
            width, height, 0, 0, width, height, x, y, depth, format, 0, cache.segment, 0);
        // The checked round-trip is the reuse barrier. Never overwrite the SHM
        // until the server has consumed the image; do not rely on queued status.
        xcb_generic_error_t *error = xcb_request_check(c, request);
        if (!error && !xcb_connection_has_error(c)) {
            ++copied_calls;
            copied_bytes += size;
            if (copied_calls == 1)
                fputs("ANLAND_XSHM_COPY active: checked shared-memory image transfer\n", stderr);
            pthread_mutex_unlock(&lock);
            return request;
        }
        free(error);
        release_cache();
    }
    ++fallback_calls;
    pthread_mutex_unlock(&lock);
    // Small/unsupported/error cases retain the original PutImage behaviour.
    return original_put(c, format, drawable, gc, width, height, x, y, pad, depth, size, data);
}

void xcb_disconnect(xcb_connection_t *c)
{
    pthread_mutex_lock(&lock);
    if (cache.connection == c) {
        release_cache();
        fprintf(stderr, "ANLAND_XSHM_COPY copied=%llu bytes=%llu fallback=%llu\n",
            (unsigned long long)copied_calls, (unsigned long long)copied_bytes,
            (unsigned long long)fallback_calls);
    }
    pthread_mutex_unlock(&lock);
    original_disconnect(c);
}

// No destructor: exit_group releases local mappings and the X connection closes
// its server attachments. IPC_RMID was set at creation. Unmapping in an exit
// handler could race a still-running rendering thread.
