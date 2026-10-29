/* Real producer implementation, but only a private socketpair: no PipeWire core
 * connection, physical device, or recording application is created. */
#include <assert.h>
#include "../../libdisplay_producer/anland_audio.c"

static uint32_t receive_active(int fd)
{
    struct { struct audio_msg h; uint32_t active; } packet;
    assert(recv(fd, &packet, sizeof(packet), MSG_DONTWAIT) == (ssize_t)sizeof(packet));
    assert(packet.h.type == AUDIO_MSG_CAPTURE_ACTIVE && packet.h.size == sizeof(uint32_t));
    return packet.active;
}

int main(void)
{
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0);
    uint8_t ring[16]; memset(ring, 42, sizeof(ring));
    struct anland_audio a = {
        .audio_fd = pair[0], .pw_connected = true,
        .ring = ring, .ring_size = sizeof(ring), .ring_fill = sizeof(ring),
    };
    on_source_state_changed(&a, PW_STREAM_STATE_CONNECTING, PW_STREAM_STATE_PAUSED, NULL);
    assert(receive_active(pair[1]) == 0); /* enumeration/idle is not recording */
    assert(a.ring_fill == 0);
    for (size_t i=0; i<sizeof(ring); ++i) assert(ring[i] == 0);
    on_source_state_changed(&a, PW_STREAM_STATE_PAUSED, PW_STREAM_STATE_STREAMING, NULL);
    assert(receive_active(pair[1]) == 1);
    send_capture_request(&a, true, false);
    char extra;
    assert(recv(pair[1], &extra, 1, MSG_DONTWAIT) == -1 && errno == EAGAIN);
    a.capture_sent_ms = 0;
    send_capture_request(&a, true, false);
    assert(receive_active(pair[1]) == 1); /* bounded active-stream renewal */
    on_source_state_changed(&a, PW_STREAM_STATE_STREAMING, PW_STREAM_STATE_PAUSED, NULL);
    assert(receive_active(pair[1]) == 0);
    a.pw_connected = false;
    send_capture_request(&a, true, true);
    assert(receive_active(pair[1]) == 0); /* no active request on a dead core */
    teardown_pw(&a);
    assert(receive_active(pair[1]) == 0);
    detach_audio_fd_locked(&a);
    assert(receive_active(pair[1]) == 0 && a.audio_fd == -1);
    assert(fcntl(pair[0], F_GETFD) >= 0); /* never closes the borrowed owner */
    close(pair[0]); close(pair[1]);
    puts("producer capture state: PASS (private socket only)");
    return 0;
}
