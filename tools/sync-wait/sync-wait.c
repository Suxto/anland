// SPDX-License-Identifier: MIT
// Private DS3 diagnostic: wait on the existing producer sync-file at import.
// Never modifies pixel data, consumes/closes a caller FD, or invents a signal.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef int (*drm_ioctl_fn)(int, unsigned long, void *);
static drm_ioctl_fn next_ioctl;
static int wait_enabled;
static atomic_uint calls_logged, pending_logged, errors_logged;
// Linux LP64 UAPI, identical on aarch64 and x86-64; checked against headers in tests.
#define IMPORT_SYNC_FILE 0x40086203UL
struct import_sync_file { uint32_t flags; int32_t fd; };
_Static_assert(sizeof(struct import_sync_file) == 8, "sync-file UAPI size");

static int budget(atomic_uint *counter)
{
    return atomic_load_explicit(counter, memory_order_relaxed) < 8 &&
           atomic_fetch_add_explicit(counter, 1, memory_order_relaxed) < 8;
}

__attribute__((constructor)) static void initialize(void)
{
    next_ioctl = (drm_ioctl_fn)dlsym(RTLD_NEXT, "drmIoctl");
    const char *mode = getenv("ANLAND_WSI_SYNC_MODE");
    if (!next_ioctl || (mode && strcmp(mode, "observe") && strcmp(mode, "wait"))) {
        fputs("ANLAND_SYNC invalid private probe configuration\n", stderr);
        _exit(127);
    }
    wait_enabled = mode && !strcmp(mode, "wait");
}

static int64_t monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int wait_fence(int fd, int *pending, int64_t *elapsed)
{
    struct pollfd descriptor = {.fd = fd, .events = POLLIN};
    int64_t start = monotonic_ms();
    if (start < 0) return -1;
    int timeout = 0;
    for (;;) {
        int result = poll(&descriptor, 1, timeout);
        if (result > 0) {
            if (descriptor.revents & (POLLERR | POLLNVAL | POLLHUP)) {
                errno = EIO;
                return -1;
            }
            if (descriptor.revents & POLLIN) {
                int64_t end = monotonic_ms();
                if (end < 0) return -1;
                *elapsed = end - start;
                return 0;
            }
        } else if (result < 0 && errno != EINTR) {
            return -1;
        }
        *pending = 1;
        int64_t now = monotonic_ms();
        if (now < 0) return -1;
        *elapsed = now - start;
        if (!wait_enabled) return 0;
        if (*elapsed >= 1000) {
            errno = ETIMEDOUT;
            return -1;
        }
        timeout = (int)(1000 - *elapsed);
    }
}

int drmIoctl(int fd, unsigned long request, void *argument)
{
    int result = next_ioctl(fd, request, argument);
    int saved_errno = errno;
    if (request != IMPORT_SYNC_FILE || !argument) return result;
    if (result) {
        if (budget(&calls_logged))
            fprintf(stderr, "ANLAND_SYNC pid=%ld import_failed ret=%d errno=%d\n",
                    (long)getpid(), result, saved_errno);
        errno = saved_errno;
        return result;  // EFAULT, for example, does not give us readable arguments.
    }
    struct import_sync_file import;
    memcpy(&import, argument, sizeof(import));  // ioctl payloads need not be aligned.
    if (budget(&calls_logged))
        fprintf(stderr, "ANLAND_SYNC pid=%ld import ret=0 flags=%u mode=%s\n",
                (long)getpid(), import.flags, wait_enabled ? "wait" : "observe");
    if (import.fd >= 0 && (import.flags & 2)) {
        int pending = 0;
        int64_t elapsed = 0;
        int waited = wait_fence(import.fd, &pending, &elapsed);
        int wait_errno = errno;
        if (pending && budget(&pending_logged))
            fprintf(stderr, "ANLAND_SYNC pid=%ld pending=1 waited_ms=%lld ret=%d\n",
                    (long)getpid(), (long long)elapsed, waited);
        if (waited && wait_enabled) {
            if (budget(&errors_logged))
                fprintf(stderr, "ANLAND_SYNC pid=%ld wait_failed errno=%d; refusing incomplete frame\n",
                        (long)getpid(), wait_errno);
            errno = wait_errno;
            return -1;
        }
    }
    errno = saved_errno;
    return result;
}
