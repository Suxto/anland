// SPDX-License-Identifier: MIT
#include <errno.h>
int fake_result, fake_errno, fake_fd;
unsigned long fake_request;
void *fake_argument;
int drmIoctl(int fd, unsigned long request, void *argument)
{
    fake_fd = fd;
    fake_request = request;
    fake_argument = argument;
    errno = fake_errno;
    return fake_result;
}
