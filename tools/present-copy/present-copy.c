// SPDX-License-Identifier: MIT
// DS3-only diagnostic: preserve DMA-BUF rendering and all fence/timing fields,
// but request the standard Present COPY path instead of a direct pixmap flip.
// Rejected in live DS3 testing: real COPY completions still showed severe twitching.
#define _GNU_SOURCE
#include <xcb/present.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static __typeof__(xcb_present_pixmap) *next_pixmap;
#if XCB_PRESENT_MINOR_VERSION >= 4
static __typeof__(xcb_present_pixmap_synced) *next_synced;
#endif
static __typeof__(xcb_wait_for_special_event) *next_wait;
static __typeof__(xcb_poll_for_special_event) *next_poll;
static atomic_uint requests, completions;

static void missing_entry(void)
{
    fputs("ANLAND_PRESENT_COPY missing libxcb entry; stopping private probe\n", stderr);
    _exit(127);
}

__attribute__((constructor)) static void initialize(void)
{
    next_pixmap = (__typeof__(next_pixmap))dlsym(RTLD_NEXT, "xcb_present_pixmap");
#if XCB_PRESENT_MINOR_VERSION >= 4
    // An older native helper's libxcb may not expose the synced API. It is only
    // required if that API is actually invoked, not merely when this DSO loads.
    next_synced = (__typeof__(next_synced))dlsym(RTLD_NEXT, "xcb_present_pixmap_synced");
#endif
    next_wait = (__typeof__(next_wait))dlsym(RTLD_NEXT, "xcb_wait_for_special_event");
    next_poll = (__typeof__(next_poll))dlsym(RTLD_NEXT, "xcb_poll_for_special_event");
    if (!next_pixmap || !next_wait || !next_poll) missing_entry();
}

static uint32_t copy_options(uint32_t options, uint32_t serial, int synced)
{
    if (atomic_load_explicit(&requests, memory_order_relaxed) < 16 &&
        atomic_fetch_add_explicit(&requests, 1, memory_order_relaxed) < 16)
        fprintf(stderr, "ANLAND_PRESENT_COPY request serial=%u synced=%d options=%u->%u\n",
                serial, synced, options, options | XCB_PRESENT_OPTION_COPY);
    return options | XCB_PRESENT_OPTION_COPY;
}

xcb_void_cookie_t xcb_present_pixmap(xcb_connection_t *c, xcb_window_t window,
    xcb_pixmap_t pixmap, uint32_t serial, xcb_xfixes_region_t valid,
    xcb_xfixes_region_t update, int16_t x_off, int16_t y_off,
    xcb_randr_crtc_t target_crtc, xcb_sync_fence_t wait_fence,
    xcb_sync_fence_t idle_fence, uint32_t options, uint64_t target_msc,
    uint64_t divisor, uint64_t remainder, uint32_t notifies_len,
    const xcb_present_notify_t *notifies)
{
    return next_pixmap(c, window, pixmap, serial, valid, update, x_off, y_off,
        target_crtc, wait_fence, idle_fence, copy_options(options, serial, 0),
        target_msc, divisor, remainder, notifies_len, notifies);
}

#if XCB_PRESENT_MINOR_VERSION >= 4
xcb_void_cookie_t xcb_present_pixmap_synced(xcb_connection_t *c, xcb_window_t window,
    xcb_pixmap_t pixmap, uint32_t serial, xcb_xfixes_region_t valid,
    xcb_xfixes_region_t update, int16_t x_off, int16_t y_off,
    xcb_randr_crtc_t target_crtc, xcb_dri3_syncobj_t acquire_syncobj,
    xcb_dri3_syncobj_t release_syncobj, uint64_t acquire_point,
    uint64_t release_point, uint32_t options, uint64_t target_msc,
    uint64_t divisor, uint64_t remainder, uint32_t notifies_len,
    const xcb_present_notify_t *notifies)
{
    if (!next_synced) missing_entry();
    return next_synced(c, window, pixmap, serial, valid, update, x_off, y_off,
        target_crtc, acquire_syncobj, release_syncobj, acquire_point, release_point,
        copy_options(options, serial, 1), target_msc, divisor, remainder, notifies_len, notifies);
}
#endif

static xcb_generic_event_t *observe_completion(xcb_connection_t *c, xcb_generic_event_t *event)
{
    if (!event || (event->response_type & 127) != XCB_GE_GENERIC ||
        atomic_load_explicit(&completions, memory_order_relaxed) >= 16)
        return event;
    const xcb_ge_generic_event_t *generic = (const xcb_ge_generic_event_t *)event;
    if (generic->event_type != XCB_PRESENT_COMPLETE_NOTIFY || generic->length < 2)
        return event;
    const xcb_query_extension_reply_t *extension = xcb_get_extension_data(c, &xcb_present_id);
    if (!extension || !extension->present || generic->extension != extension->major_opcode)
        return event;
    const xcb_present_complete_notify_event_t *done = (const xcb_present_complete_notify_event_t *)event;
    if (done->kind == XCB_PRESENT_COMPLETE_KIND_PIXMAP &&
        atomic_fetch_add_explicit(&completions, 1, memory_order_relaxed) < 16)
        fprintf(stderr, "ANLAND_PRESENT_COPY complete serial=%u mode=%u (copy=0 flip=1 skip=2)\n",
                done->serial, done->mode);
    // Never consume, rewrite or free events belonging to the application's WSI.
    return event;
}

xcb_generic_event_t *xcb_wait_for_special_event(xcb_connection_t *c, xcb_special_event_t *special)
{
    return observe_completion(c, next_wait(c, special));
}

xcb_generic_event_t *xcb_poll_for_special_event(xcb_connection_t *c, xcb_special_event_t *special)
{
    return observe_completion(c, next_poll(c, special));
}
