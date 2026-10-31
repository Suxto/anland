// SPDX-License-Identifier: MIT
// Linked as a separate DSO so LD_PRELOAD/RTLD_NEXT ordering is exercised.
#include <xcb/present.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

xcb_generic_event_t *fixture_last_event;
xcb_present_complete_notify_event_t fixture_expected_event;
unsigned fixture_case;

static void common(xcb_connection_t *c, xcb_window_t window, xcb_pixmap_t pixmap,
    uint32_t serial, xcb_xfixes_region_t valid, xcb_xfixes_region_t update,
    int16_t x_off, int16_t y_off, xcb_randr_crtc_t target_crtc,
    uint64_t target_msc, uint64_t divisor, uint64_t remainder,
    uint32_t notifies_len, const xcb_present_notify_t *notifies)
{
    assert(c == (xcb_connection_t *)(uintptr_t)1);
    assert(window == 2 && pixmap == 3 && serial == 4 && valid == 5 && update == 6);
    assert(x_off == -7 && y_off == 8 && target_crtc == 9);
    assert(target_msc == UINT64_C(0x8000000112345678));
    assert(divisor == UINT64_C(0x7000000223456789));
    assert(remainder == UINT64_C(0x600000033456789a));
    assert(notifies_len == 1 && notifies && notifies[0].window == 10 && notifies[0].serial == 11);
}

xcb_void_cookie_t xcb_present_pixmap(xcb_connection_t *c, xcb_window_t window,
    xcb_pixmap_t pixmap, uint32_t serial, xcb_xfixes_region_t valid,
    xcb_xfixes_region_t update, int16_t x_off, int16_t y_off,
    xcb_randr_crtc_t target_crtc, xcb_sync_fence_t wait_fence,
    xcb_sync_fence_t idle_fence, uint32_t options, uint64_t target_msc,
    uint64_t divisor, uint64_t remainder, uint32_t notifies_len,
    const xcb_present_notify_t *notifies)
{
    common(c, window, pixmap, serial, valid, update, x_off, y_off, target_crtc,
           target_msc, divisor, remainder, notifies_len, notifies);
    assert(wait_fence == 12 && idle_fence == 13);
    return (xcb_void_cookie_t){.sequence = options};
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
    common(c, window, pixmap, serial, valid, update, x_off, y_off, target_crtc,
           target_msc, divisor, remainder, notifies_len, notifies);
    assert(acquire_syncobj == 14 && release_syncobj == 15);
    assert(acquire_point == UINT64_C(0xf0000004456789ab));
    assert(release_point == UINT64_C(0xe000000556789abc));
    return (xcb_void_cookie_t){.sequence = options};
}
#endif

const xcb_query_extension_reply_t *xcb_get_extension_data(xcb_connection_t *c, xcb_extension_t *ext)
{
    assert(c == (xcb_connection_t *)(uintptr_t)1 && ext);
    static const xcb_query_extension_reply_t reply = {.present = 1, .major_opcode = 150};
    return &reply;
}

static xcb_generic_event_t *event(xcb_connection_t *c, xcb_special_event_t *special)
{
    assert(c == (xcb_connection_t *)(uintptr_t)1);
    assert(special == (xcb_special_event_t *)(uintptr_t)2);
    if (fixture_case == 0) return NULL;
    memset(&fixture_expected_event, 0xa5, sizeof(fixture_expected_event));
    fixture_expected_event.response_type = fixture_case == 1 ? XCB_EXPOSE : (XCB_GE_GENERIC | 128);
    fixture_expected_event.extension = fixture_case == 2 ? 151 : 150;
    fixture_expected_event.length = fixture_case == 3 ? 0 : 2;
    fixture_expected_event.event_type = XCB_PRESENT_COMPLETE_NOTIFY;
    fixture_expected_event.kind = XCB_PRESENT_COMPLETE_KIND_PIXMAP;
    // Deliberately report FLIP: the probe must not fake a COPY completion.
    fixture_expected_event.mode = XCB_PRESENT_COMPLETE_MODE_FLIP;
    fixture_expected_event.serial = 0x76543210;
    fixture_last_event = malloc(sizeof(fixture_expected_event));
    assert(fixture_last_event);
    memcpy(fixture_last_event, &fixture_expected_event, sizeof(fixture_expected_event));
    return fixture_last_event;
}

xcb_generic_event_t *xcb_wait_for_special_event(xcb_connection_t *c, xcb_special_event_t *special)
{
    return event(c, special);
}

xcb_generic_event_t *xcb_poll_for_special_event(xcb_connection_t *c, xcb_special_event_t *special)
{
    return event(c, special);
}
