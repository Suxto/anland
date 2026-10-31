/* Compile the actual patched submission implementation, not a reimplementation. */
#include "test-stubs.h"
#include "src/freedreno/drm/kgsl/kgsl_ringbuffer_sp.c"
#include <dirent.h>
#include <sys/resource.h>
#include <sys/wait.h>

static unsigned exported, published, submitted, deleted, expected_shared;
static const char *failure;
static bool enabled;
static int new_fd(void) { int fd=open("/dev/null", O_RDONLY|O_CLOEXEC); assert(fd>=0); return fd; }
static bool fail(const char *stage) { if (failure && !strcmp(failure, stage)) { errno=EIO; return true; } return false; }
static int sync_merge(const char *name, int a, int b) {
    assert(!strcmp(name,"anland-kgsl") && fcntl(a,F_GETFD)>=0 && fcntl(b,F_GETFD)>=0);
    return fail("merge") ? -1 : new_fd();
}
static int kgsl_pipe_safe_ioctl(int fd, unsigned long request, void *arg) {
    assert(fcntl(fd,F_GETFD)>=0);
    if (request==DMA_BUF_IOCTL_EXPORT_SYNC_FILE) {
        assert(!submitted && !deleted);
        struct dma_buf_export_sync_file *p=arg;
        assert(p->flags==DMA_BUF_SYNC_RW);
        if (fail("export")) return -1;
        p->fd=new_fd(); exported++; return 0;
    }
    if (request==IOCTL_KGSL_GPU_COMMAND) {
        struct kgsl_gpu_command *p=arg;
        assert(!deleted && exported==(enabled?expected_shared:0));
        assert(p->context_id==7 && p->numcmds==2);
        if (p->numsyncs) {
            struct kgsl_command_syncpoint *s=(void *)(uintptr_t)p->synclist;
            struct kgsl_cmd_syncpoint_fence *f=(void *)(uintptr_t)s->priv;
            assert(p->numsyncs==1 && s->type==KGSL_CMD_SYNCPOINT_TYPE_FENCE);
            assert(fcntl(f->fd,F_GETFD)>=0);
        } else assert(!expected_shared || !enabled);
        if (fail("submit")) return -1;
        p->timestamp=42; submitted++; return 0;
    }
    if (request==IOCTL_KGSL_TIMESTAMP_EVENT) {
        struct kgsl_timestamp_event *p=arg;
        assert(submitted==1 && !deleted && p->timestamp==42 && p->context_id==7);
        if (fail("timestamp")) return -1;
        *(int *)p->priv=new_fd(); return 0;
    }
    if (request==DMA_BUF_IOCTL_IMPORT_SYNC_FILE) {
        struct dma_buf_import_sync_file *p=arg;
        assert(submitted==1 && !deleted && p->flags==DMA_BUF_SYNC_RW);
        assert(fcntl(p->fd,F_GETFD)>=0);
        if (fail("publish")) return -1;
        published++; return 0;
    }
    abort();
}
static void fd_submit_del(struct fd_submit *s) {
    assert(s && submitted==1 && published==(enabled?expected_shared:0)); deleted++;
}
static void list_del(struct list_head *node) {
    assert(node->list->first==node->owner);
    node->list->first=node->owner->next;
}
static unsigned fd_count(void) {
    DIR *dir=opendir("/proc/self/fd"); assert(dir); unsigned n=0;
    while (readdir(dir)) n++;
    closedir(dir); return n;
}
static void run(bool active, bool shared, bool output_fd, bool explicit_input) {
    unsigned before=fd_count();
    exported=published=submitted=deleted=0;
    enabled=active; expected_shared=shared?2:0;
    struct kgsl_device dev={.base.fd=new_fd(), .implicit_sync=active};
    struct kgsl_pipe pipe={.base.dev=&dev.base, .queue_id=7};
    struct kgsl_bo bo={.base={.alloc_flags=shared?FD_BO_SHARED:0,.handle=9,.iova=4096}, .bo_type=KGSL_BO_IMPORT,.import_fd=new_fd()};
    struct fd_bo *bos[]={&bo.base};
    struct cmd cmd={.ring_bo=&bo.base,.size=16};
    struct fd_ringbuffer_sp ring={.u={.nr_cmds=1,.cmds=&cmd}};
    struct fd_fence out={.use_fence_fd=output_fd,.fence_fd=-1};
    int borrowed=explicit_input?new_fd():-1;
    struct fd_submit_sp a={.base={.pipe=&pipe.base,.primary=&ring.base},.nr_bos=1,.bos=bos,.in_fence_fd=-1,.out_fence=&out};
    struct fd_submit_sp b=a;
    b.in_fence_fd=explicit_input?dup(borrowed):-1;
    struct list_head list={.first=&a.base,.last=&b.base};
    a.base.next=&b.base;
    a.base.node=(struct list_head){.owner=&a.base,.list=&list};
    b.base.node=(struct list_head){.owner=&b.base,.list=&list};
    assert(flush_submit_list(&list)==0);
    assert(deleted==1 && out.kfence==42 && b.in_fence_fd==-1);
    assert(exported==(active?expected_shared:0) && published==exported);
    if(output_fd) { assert(fcntl(out.fence_fd,F_GETFD)>=0); close(out.fence_fd); }
    else assert(out.fence_fd==-1);
    if(borrowed>=0) { assert(fcntl(borrowed,F_GETFD)>=0); close(borrowed); }
    close(bo.import_fd); close(dev.base.fd);
    assert(fd_count()==before);
}
int main(void) {
    struct rlimit limit={0,0}; assert(setrlimit(RLIMIT_CORE,&limit)==0);
    for(unsigned mask=0;mask<16;mask++) run(mask&1,mask&2,mask&4,mask&8);
    const char *stages[]={"export","merge","submit","timestamp","publish"};
    for(unsigned i=0;i<5;i++) {
        pid_t pid=fork(); assert(pid>=0);
        if(!pid) { failure=stages[i]; run(true,true,true,true); _exit(99); }
        int status; assert(waitpid(pid,&status,0)==pid);
        assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
    }
    puts("PASS: real submission body: ownership/order, merged submits, explicit fences, mode-off, nonshared, FD lifetime, fail-closed handoffs");
}
