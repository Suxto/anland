/* Test-only Mesa scaffolding; use the real Linux/KGSL UAPI structures. */
#define KGSL_PRIV_H
#define FREEDRENO_RINGBUFFER_SP_H_
#define __user
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include "src/freedreno/vulkan/msm_kgsl.h"
#define FD_BO_SHARED 1
#define KGSL_BO_IMPORT 1
struct fd_device { int fd; };
struct kgsl_device { struct fd_device base; bool implicit_sync; unsigned sync_logs; };
struct fd_pipe { struct fd_device *dev; };
struct kgsl_pipe { struct fd_pipe base; unsigned queue_id; };
struct fd_bo { unsigned alloc_flags, handle; uint64_t iova; };
struct kgsl_bo { struct fd_bo base; int bo_type, import_fd; };
struct fd_fence { unsigned kfence; bool use_fence_fd; int fence_fd; };
struct fd_ringbuffer { int unused; };
struct cmd { struct fd_bo *ring_bo; unsigned size; };
struct fd_ringbuffer_sp { struct fd_ringbuffer base; unsigned offset; struct { unsigned nr_cmds; struct cmd *cmds; } u; };
struct fd_submit;
struct list_head { struct fd_submit *first, *last, *owner; struct list_head *list; };
struct fd_submit { struct fd_pipe *pipe; struct fd_ringbuffer *primary; struct fd_submit *next; struct list_head node; };
struct fd_submit_sp { struct fd_submit base; unsigned nr_bos; struct fd_bo **bos; int in_fence_fd; struct fd_fence *out_fence; };
#define to_kgsl_device(x) ((struct kgsl_device *)(x))
#define to_kgsl_pipe(x) ((struct kgsl_pipe *)(x))
#define to_kgsl_bo(x) ((struct kgsl_bo *)(x))
#define to_fd_submit_sp(x) ((struct fd_submit_sp *)(x))
#define to_fd_ringbuffer_sp(x) ((struct fd_ringbuffer_sp *)(x))
#define foreach_submit(v,l) for (struct fd_submit *v=(l)->first; v; v=v->next)
#define foreach_submit_safe(v,l) for (struct fd_submit *v=(l)->first, *next=v?v->next:NULL; v; v=next, next=v?v->next:NULL)
#define last_submit(l) ((l)->last)
#define MESA_TRACE_FUNC() ((void)0)
#define ERROR_MSG(...) ((void)0)
#define DEBUG_MSG(...) ((void)0)
#define submit_offset(bo,offset) (offset)
static int kgsl_pipe_safe_ioctl(int, unsigned long, void *);
static int sync_merge(const char *, int, int);
static void fd_submit_del(struct fd_submit *);
static void list_del(struct list_head *);
static struct fd_submit *fd_submit_sp_new(struct fd_pipe *p, int (*fn)(struct list_head *)) { (void)p; (void)fn; return NULL; }
