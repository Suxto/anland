/* Compile the actual Android audio implementation against fake AAudio APIs. */
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

atomic_int fake_opens, fake_active, fake_closes, fake_play_opens;
atomic_bool fake_input_error;
struct display_ctx { int audio_fd; };
#include "../../consumers/anland_v5/android_consumer/app/src/main/jni/native_audio.c"
int get_audio_fd(display_ctx *ctx) { return ctx->audio_fd; }

static void wait_active(int expected, int milliseconds)
{
    uint64_t until = now_ms() + milliseconds;
    while (atomic_load(&fake_active) != expected && now_ms() < until)
        usleep(5000);
    assert(atomic_load(&fake_active) == expected);
}

static void request(int peer, uint32_t active)
{
    struct { struct audio_msg header; uint32_t active; } packet = {
        .header = { .type = AUDIO_MSG_CAPTURE_ACTIVE, .size = sizeof(uint32_t) },
        .active = active,
    };
    assert(send(peer, &packet, sizeof(packet), 0) == (ssize_t)sizeof(packet));
}

int main(void)
{
    int pair[2];
    assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0);
    display_ctx ctx = { .audio_fd = pair[0] };
    audio_bridge *b = audio_create();
    assert(b);
    audio_set_ctx(b, &ctx);
    audio_set_mic_enabled(b, true);
    audio_start(b);
    usleep(80000);
    assert(fake_opens == 0); /* connected + user-enabled is NOT recording demand */
    assert(fake_play_opens > 0);

    request(pair[1], 1);
    wait_active(1, 800);
    assert(b->cap_rate == 44100 && b->cap_channels == 2); /* actual device format */
    int opens = fake_opens;
    request(pair[1], 1);
    usleep(80000);
    assert(fake_opens == opens); /* renewing does not reopen a live stream */
    request(pair[1], 0);
    wait_active(0, 800);
    assert(fake_closes == opens);

    request(pair[1], 1);
    wait_active(1, 800);
    audio_set_mic_enabled(b, false);
    wait_active(0, 800);
    request(pair[1], 1);
    usleep(80000);
    assert(fake_active == 0); /* protocol cannot bypass user permission/consent */
    audio_set_mic_enabled(b, true);
    wait_active(1, 800);
    wait_active(0, AUDIO_CAPTURE_LEASE_MS + 800); /* STOP lost / producer crash */

    request(pair[1], 1);
    wait_active(1, 800);
    request(pair[1], 2); /* unknown active enum fails closed */
    wait_active(0, 800);

    request(pair[1], 1);
    wait_active(1, 800);
    struct { struct audio_msg h; uint32_t value; } malformed = {
        .h = { .type = AUDIO_MSG_CAPTURE_ACTIVE, .size = 1 }, .value = 1,
    };
    assert(send(pair[1], &malformed, sizeof(malformed), 0) == (ssize_t)sizeof(malformed));
    wait_active(0, 800);
    request(pair[1], 1);
    wait_active(1, 800);
    malformed.h.size = sizeof(uint32_t);
    assert(send(pair[1], &malformed, sizeof(struct audio_msg) + 1, 0)
           == (ssize_t)(sizeof(struct audio_msg) + 1));
    wait_active(0, 800);

    struct audio_connection stale = current_connection(b);
    audio_set_ctx(b, &ctx); /* same descriptor, new generation */
    capture_request(b, stale, 1);
    assert(!capture_wanted(b, stale));
    close(stale.fd);
    usleep(80000);
    assert(fake_active == 0);

    /* Two windows own separate bridges: releasing one must not close the other. */
    int other[2];
    assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, other) == 0);
    display_ctx other_ctx = { .audio_fd = other[0] };
    audio_bridge *second = audio_create();
    audio_set_ctx(second, &other_ctx);
    audio_set_mic_enabled(second, true);
    audio_start(second);
    request(pair[1], 1);
    request(other[1], 1);
    wait_active(2, 800);
    request(pair[1], 0);
    wait_active(1, 800);
    audio_destroy(second);
    wait_active(0, 800);
    close(other[0]); close(other[1]);

    /* Playback remains functional after capture stops. */
    struct { struct audio_msg h; int16_t pcm[64]; } playback = {
        .h = { .type = AUDIO_MSG_PCM, .size = sizeof(int16_t) * 64 },
    };
    assert(send(pair[1], &playback, sizeof(playback), 0) == (ssize_t)sizeof(playback));
    usleep(80000);
    assert(fake_active == 0 && b->play != NULL);

    request(pair[1], 1);
    wait_active(1, 800);
    close(pair[1]); /* peer disconnect */
    wait_active(0, 800);
    audio_destroy(b);
    close(pair[0]);
    assert(fake_active == 0 && fake_closes == fake_opens);
    puts("native microphone lifecycle: PASS (fake AAudio, no recording)");
    return 0;
}
