/* Test-only AAudio implementation: no Android service, device or audio capture. */
#ifndef TEST_AAUDIO_H
#define TEST_AAUDIO_H
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef int aaudio_result_t;
typedef int aaudio_direction_t;
typedef int aaudio_stream_state_t;
typedef int aaudio_data_callback_result_t;
#define AAUDIO_OK 0
#define AAUDIO_UNSPECIFIED 0
#define AAUDIO_DIRECTION_OUTPUT 0
#define AAUDIO_DIRECTION_INPUT 1
#define AAUDIO_FORMAT_PCM_I16 1
#define AAUDIO_SHARING_MODE_SHARED 1
#define AAUDIO_USAGE_MEDIA 1
#define AAUDIO_CONTENT_TYPE_MUSIC 1
#define AAUDIO_PERFORMANCE_MODE_NONE 0
#define AAUDIO_PERFORMANCE_MODE_LOW_LATENCY 1
#define AAUDIO_INPUT_PRESET_VOICE_RECOGNITION 1
#define AAUDIO_STREAM_STATE_STARTED 4
#define AAUDIO_STREAM_STATE_STARTING 3
#define AAUDIO_STREAM_STATE_DISCONNECTED 13
#define AAUDIO_ERROR_DISCONNECTED -1
#define AAUDIO_ERROR_INVALID_STATE -2
#define AAUDIO_CALLBACK_RESULT_CONTINUE 0
#define AAUDIO_CALLBACK_RESULT_STOP 1

typedef struct { int direction, channels, rate, state; } AAudioStream;
typedef AAudioStream AAudioStreamBuilder;
extern atomic_int fake_opens, fake_active, fake_closes, fake_play_opens;
extern atomic_bool fake_input_error;
static inline int AAudio_createStreamBuilder(AAudioStreamBuilder **b) {
    *b = calloc(1, sizeof(**b)); return *b ? 0 : -1;
}
static inline void AAudioStreamBuilder_setDirection(AAudioStreamBuilder *b, int v) { b->direction=v; }
static inline void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder *b, int v) { b->channels=v; }
static inline void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder *b, int v) { b->rate=v; }
#define IGNORE_SETTER(name) static inline void name(AAudioStreamBuilder *b, int v) { (void)b; (void)v; }
IGNORE_SETTER(AAudioStreamBuilder_setFormat)
IGNORE_SETTER(AAudioStreamBuilder_setSharingMode)
IGNORE_SETTER(AAudioStreamBuilder_setUsage)
IGNORE_SETTER(AAudioStreamBuilder_setContentType)
IGNORE_SETTER(AAudioStreamBuilder_setPerformanceMode)
IGNORE_SETTER(AAudioStreamBuilder_setInputPreset)
static inline void AAudioStreamBuilder_setDataCallback(AAudioStreamBuilder *b,
        int (*cb)(AAudioStream *, void *, void *, int32_t), void *data) {
    (void)b; (void)cb; (void)data;
}
static inline void AAudioStreamBuilder_setErrorCallback(AAudioStreamBuilder *b,
        void (*cb)(AAudioStream *, void *, int), void *data) {
    (void)b; (void)cb; (void)data;
}
static inline int AAudioStreamBuilder_openStream(AAudioStreamBuilder *b, AAudioStream **s) {
    if (b->direction == AAUDIO_DIRECTION_INPUT && fake_input_error) return -1;
    *s = malloc(sizeof(**s)); if (!*s) return -1; **s = *b;
    if (b->direction == AAUDIO_DIRECTION_INPUT) {
        (*s)->rate=44100; (*s)->channels=2; atomic_fetch_add(&fake_opens,1);
    } else { (*s)->rate=48000; atomic_fetch_add(&fake_play_opens,1); }
    return 0;
}
static inline void AAudioStreamBuilder_delete(AAudioStreamBuilder *b) { free(b); }
static inline int AAudioStream_getSampleRate(AAudioStream *s) { return s->rate; }
static inline int AAudioStream_getChannelCount(AAudioStream *s) { return s->channels; }
static inline int AAudioStream_getState(AAudioStream *s) { return s->state; }
static inline int AAudioStream_requestStart(AAudioStream *s) {
    if (s->direction==AAUDIO_DIRECTION_INPUT && s->state!=AAUDIO_STREAM_STATE_STARTED)
        atomic_fetch_add(&fake_active,1);
    s->state=AAUDIO_STREAM_STATE_STARTED; return 0;
}
static inline int AAudioStream_requestStop(AAudioStream *s) {
    if (s->direction==AAUDIO_DIRECTION_INPUT && s->state==AAUDIO_STREAM_STATE_STARTED)
        atomic_fetch_sub(&fake_active,1);
    s->state=0; return 0;
}
static inline int AAudioStream_close(AAudioStream *s) {
    if (s->direction==AAUDIO_DIRECTION_INPUT) atomic_fetch_add(&fake_closes,1);
    free(s); return 0;
}
static inline int AAudioStream_read(AAudioStream *s, void *buf, int frames, int64_t timeout) {
    (void)timeout; usleep(5000);
    if (fake_input_error) return -1;
    memset(buf,0,(size_t)frames*s->channels*sizeof(int16_t)); return frames;
}
static inline const char *AAudio_convertResultToText(int v) { (void)v; return "mock"; }
static inline const char *AAudio_convertStreamStateToText(int v) { (void)v; return "mock"; }
#endif
