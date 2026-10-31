// SPDX-License-Identifier: MIT
#include <xcb/present.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern xcb_generic_event_t *fixture_last_event;
extern xcb_present_complete_notify_event_t fixture_expected_event;
extern unsigned fixture_case;

int main(int argc, char **argv)
{
    assert(argc == 2 && (!strcmp(argv[1], "plain") || !strcmp(argv[1], "copy")));
    uint32_t extra = !strcmp(argv[1], "copy") ? XCB_PRESENT_OPTION_COPY : 0;
    xcb_connection_t *c = (xcb_connection_t *)(uintptr_t)1;
    xcb_special_event_t *special = (xcb_special_event_t *)(uintptr_t)2;
    const xcb_present_notify_t notify = {.window = 10, .serial = 11};
    // Every combination of existing low option bits; no flag may be cleared.
    for (uint32_t options = 0; options < 32; options++) {
        xcb_void_cookie_t cookie = xcb_present_pixmap(c, 2, 3, 4, 5, 6, -7, 8, 9,
            12, 13, options, UINT64_C(0x8000000112345678), UINT64_C(0x7000000223456789),
            UINT64_C(0x600000033456789a), 1, &notify);
        assert(cookie.sequence == (options | extra));
#if XCB_PRESENT_MINOR_VERSION >= 4
        cookie = xcb_present_pixmap_synced(c, 2, 3, 4, 5, 6, -7, 8, 9,
            14, 15, UINT64_C(0xf0000004456789ab), UINT64_C(0xe000000556789abc), options,
            UINT64_C(0x8000000112345678), UINT64_C(0x7000000223456789),
            UINT64_C(0x600000033456789a), 1, &notify);
        assert(cookie.sequence == (options | extra));
#endif
    }
    for (fixture_case = 0; fixture_case < 24; fixture_case++) {
        for (int wait = 0; wait < 2; wait++) {
            xcb_generic_event_t *result = wait ? xcb_wait_for_special_event(c, special)
                                               : xcb_poll_for_special_event(c, special);
            if (!fixture_case) assert(!result);
            else {
                assert(result == fixture_last_event);
                assert(!memcmp(result, &fixture_expected_event, sizeof(fixture_expected_event)));
                free(result);
            }
        }
    }
    printf("%s: options, 64-bit timing/fences, event identity/bytes and malformed-event guards passed (Present 1.%d)\n",
           argv[1], XCB_PRESENT_MINOR_VERSION);
    return 0;
}
