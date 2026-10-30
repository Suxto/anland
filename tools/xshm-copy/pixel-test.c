#include <xcb/xcb.h>
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

static int benchmark_mode;

static void check(xcb_connection_t *c, xcb_void_cookie_t cookie)
{
    xcb_generic_error_t *error = xcb_request_check(c, cookie);
    if (error || xcb_connection_has_error(c))
        fprintf(stderr, "X11 error=%u connection=%d\n", error ? error->error_code : 0, xcb_connection_has_error(c));
    assert(!error && !xcb_connection_has_error(c));
    free(error);
}

static void frames(xcb_connection_t *c, unsigned width, unsigned height, unsigned count)
{
    const xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    assert(screen && screen->root_depth == 24);
    const uint32_t length = width * height * 4;
    // Mesa asks this before PutImage, enabling BIG-REQUESTS for full frames.
    assert((uint64_t)length + 64 < (uint64_t)xcb_get_maximum_request_length(c) * 4);
    uint32_t *pixels = malloc(length);
    assert(pixels);
    xcb_pixmap_t pixmap = xcb_generate_id(c);
    xcb_gcontext_t gc = xcb_generate_id(c);
    check(c, xcb_create_pixmap_checked(c, 24, pixmap, screen->root, width, height));
    check(c, xcb_create_gc_checked(c, gc, pixmap, 0, NULL));
    struct timespec start, end;
    assert(!clock_gettime(CLOCK_MONOTONIC, &start));
    for (unsigned frame = 0; frame < count; ++frame) {
        unsigned changed = benchmark_mode && frame ? 64 : width * height;
        for (unsigned i = 0; i < changed; ++i)
            pixels[i] = ((i * 16777619U) ^ (frame * 0x73519U)) & 0xffffff;
        xcb_void_cookie_t sent = xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP,
            pixmap, gc, width, height, 0, 0, 0, 24, length, (const uint8_t *)pixels);
        xcb_discard_reply(c, sent.sequence);
        if (benchmark_mode && frame + 1 < count) {
            xcb_get_geometry_reply_t *done = xcb_get_geometry_reply(c, xcb_get_geometry(c, pixmap), NULL);
            assert(done && !xcb_connection_has_error(c));
            free(done);
            continue;
        }
        xcb_generic_error_t *error = NULL;
        xcb_get_image_reply_t *reply = xcb_get_image_reply(c,
            xcb_get_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, 0, 0, width, height, UINT32_MAX), &error);
        assert(!error && reply && reply->depth == 24);
        assert(xcb_get_image_data_length(reply) == (int)length);
        const uint32_t *got = (const uint32_t *)xcb_get_image_data(reply);
        for (unsigned i = 0; i < width * height; ++i) {
            if ((got[i] & 0xffffff) != pixels[i]) {
                fprintf(stderr, "pixel mismatch frame=%u offset=%u got=%x want=%x\n", frame, i, got[i], pixels[i]);
                abort();
            }
        }
        free(reply);
        free(error);
    }
    assert(!clock_gettime(CLOCK_MONOTONIC, &end));
    if (benchmark_mode) {
        const double ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
        printf("BENCH width=%u height=%u frames=%u elapsed_ms=%.3f per_frame_ms=%.3f\n", width, height, count, ms, ms / count);
    }
    check(c, xcb_free_gc_checked(c, gc));
    check(c, xcb_free_pixmap_checked(c, pixmap));
    free(pixels);
}

static void *thread_frames(void *connection)
{
    frames(connection, 320, 180, 12);
    return NULL;
}

int main(int argc, char **argv)
{
    xcb_connection_t *c = xcb_connect(NULL, NULL);
    assert(c && !xcb_connection_has_error(c));
    if (argc == 2 && strcmp(argv[1], "--bench") == 0) {
        benchmark_mode = 1;
        frames(c, 1440, 900, 120);
        frames(c, 1920, 1080, 120);
        xcb_disconnect(c);
        return 0;
    }
    assert(argc == 1);
    frames(c, 31, 19, 3); // original small-image fallback
    frames(c, 640, 360, 8);
    frames(c, 1920, 1080, 3); // cache growth / old attachment cleanup
    frames(c, 320, 180, 8); // smaller image using the same allocation
    pthread_t first, second;
    assert(!pthread_create(&first, NULL, thread_frames, c));
    assert(!pthread_create(&second, NULL, thread_frames, c));
    assert(!pthread_join(first, NULL));
    assert(!pthread_join(second, NULL));
    const pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        // xcb_disconnect may shutdown the shared open-file description after a
        // fork. Close only this inherited descriptor; do not shut down the parent.
        close(xcb_get_file_descriptor(c));
        c = xcb_connect(NULL, NULL);
        assert(c && !xcb_connection_has_error(c));
        frames(c, 320, 180, 5);
        xcb_disconnect(c);
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    frames(c, 640, 360, 5);
    xcb_disconnect(c);
    puts("PIXEL_TEST_PASS: exact RGB, fallback, resize, threads, fork, reconnect");
    return 0;
}
