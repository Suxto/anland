#ifndef TEST_ANDROID_LOG_H
#define TEST_ANDROID_LOG_H
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_ERROR 6
static inline int __android_log_print(int level, const char *tag, const char *format, ...) {
    (void)level; (void)tag; (void)format; return 0;
}
#endif
