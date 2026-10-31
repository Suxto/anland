// SPDX-License-Identifier: MIT
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern int fake_result, fake_errno, fake_fd;
extern unsigned long fake_request;
extern void *fake_argument;
int drmIoctl(int, unsigned long, void *);
struct import_sync_file { uint32_t flags; int32_t fd; };
#define IMPORT_SYNC_FILE 0x40086203UL

static void *signal_later(void *argument)
{
    int fd = *(int *)argument;
    usleep(100000);
    assert(write(fd, "x", 1) == 1);
    return NULL;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    int wait_mode = !strcmp(argv[1], "wait");
    fake_result = 7;
    fake_errno = EBUSY;
    assert(drmIoctl(42, 123, (void *)(uintptr_t)1) == 7 && errno == EBUSY);
    assert(fake_fd == 42 && fake_request == 123 && fake_argument == (void *)(uintptr_t)1);
    fake_result = -1;
    fake_errno = EFAULT;
    // Failed ioctls must not dereference the caller's invalid pointer.
    assert(drmIoctl(43, IMPORT_SYNC_FILE, (void *)(uintptr_t)1) == -1 && errno == EFAULT);
    fake_result = 0;
    fake_errno = EBUSY;
    int descriptors[2];
    assert(pipe(descriptors) == 0);
    assert(fcntl(descriptors[0], F_SETFL, O_NONBLOCK) == 0);
    struct import_sync_file data = {.flags = 1, .fd = descriptors[0]};
    assert(drmIoctl(44, IMPORT_SYNC_FILE, &data) == 0 && errno == EBUSY);  // READ is not a producer wait.
    data.flags = 3;
    if (wait_mode) {
        pthread_t writer;
        assert(pthread_create(&writer, NULL, signal_later, &descriptors[1]) == 0);
        // Intentionally unaligned, as the Linux ioctl API permits byte payloads.
        unsigned char unaligned[sizeof(data) + 1];
        memcpy(unaligned + 1, &data, sizeof(data));
        assert(drmIoctl(45, IMPORT_SYNC_FILE, unaligned + 1) == 0 && errno == EBUSY);
        char value;
        assert(read(descriptors[0], &value, 1) == 1 && value == 'x');  // Not consumed by the probe.
        assert(!memcmp(unaligned + 1, &data, sizeof(data)));
        assert(pthread_join(writer, NULL) == 0);
        assert(drmIoctl(46, IMPORT_SYNC_FILE, &data) == -1 && errno == ETIMEDOUT);
        assert(fcntl(descriptors[0], F_GETFD) >= 0);  // Still caller-owned after timeout.
        data.fd = -1;
        assert(drmIoctl(47, IMPORT_SYNC_FILE, &data) == 0 && errno == EBUSY);
    } else {
        assert(drmIoctl(48, IMPORT_SYNC_FILE, &data) == 0 && errno == EBUSY);
    }
    close(descriptors[0]);
    close(descriptors[1]);
    printf("%s: passthrough, errno, bad pointer, alignment, readiness, ownership, timeout passed\n", argv[1]);
    return 0;
}
