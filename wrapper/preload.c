#include "preload.h"
int curindex[MAX_THREAD_NUM];
int table_size = 64;
int main_thread_pid;
int in_segment;
void *mpool; /* memory pool */
int pool_offset;
struct iovec *iovpool; /* pool for iovector */
int iov_offset;
int batch_num;   /* number of busy entry */
int syscall_num; /* number of syscall triggered currently */
int predict_res; /* 1: do batch; 0: do normal */
int predict_state;

/*
long batch_start() {
    in_segment = 1;
    batch_num = 0;
    return 0;
}

long batch_flush() {
    in_segment = 0;

    if (batch_num == 0)
        return 0;
    batch_num = 0;
    return syscall(__NR_batch_flush);
}*/

ssize_t shutdown(int fd, int how) {

    syscall_num++;
    if (!in_segment) {
        return real_shutdown(fd, how);
    }
    batch_num++;

    //int off, toff = 0;
    //off = 1 << 6; /* 6 = log64 */

    int off, toff = (fd % 4) + 1;
    off = toff << 6;

//while(btable[off + curindex[toff]].rstatus == BENTRY_BUSY);
    btable[off + curindex[toff]].sysnum = __NR_shutdown;
    btable[off + curindex[toff]].rstatus = BENTRY_BUSY;
    btable[off + curindex[toff]].nargs = 2;
    btable[off + curindex[toff]].args[0] = fd;
    btable[off + curindex[toff]].args[1] = how;
    btable[off + curindex[toff]].pid = main_thread_pid + off;
//printf("fill shutdown at %d\n", off + curindex[toff]);
    if (curindex[toff] == MAX_TABLE_SIZE - 1) {
	curindex[toff] = 1;
    } else {
        curindex[toff]++;
    }

    /* assume success */
    return 0;
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
    syscall_num++;
    if (!in_segment) {
        return real_sendfile(out_fd, in_fd, offset, count);
    }
    batch_num++;

    int off, toff = (out_fd % 4) + 1;
    //off = 1 << 6; /* 6 = log64 */
    off = toff << 6;

//while(btable[off + curindex[toff]].rstatus == BENTRY_BUSY);
    btable[off + curindex[toff]].sysnum = __NR_sendfile;
    btable[off + curindex[toff]].rstatus = BENTRY_BUSY;
    btable[off + curindex[toff]].nargs = 4;
    btable[off + curindex[toff]].args[0] = out_fd;
    btable[off + curindex[toff]].args[1] = in_fd;
    btable[off + curindex[toff]].args[2] = offset;
    btable[off + curindex[toff]].args[3] = count;
    btable[off + curindex[toff]].pid = main_thread_pid + off;
//printf("fill shutdown at %d\n", off + curindex[toff]);
    if (curindex[toff] == MAX_TABLE_SIZE - 1) {
        curindex[toff] = 1;
    } else {
        curindex[toff]++;
    }
    /* assume success */
    return 0;
}

#if 0
ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    syscall_num++;
    if (!in_segment) {
        return real_writev(fd, iov, iovcnt);
    }
    batch_num++;

    int off, toff = 0, len = 0, i;
    off = 1 << 6; /* 6 = log64 */

    for(i = 0; i < iovcnt; i++){
        int ll = iov[i].iov_len;

        /* handle string */
        if (pool_offset + (ll / POOL_UNIT) > MAX_POOL_SIZE)
            pool_offset = 0;
        else
            pool_offset += (ll / POOL_UNIT);

        /* handle iovec */
        if (iov_offset + 1 >= MAX_POOL_IOV_SIZE)
            iov_offset = 0;
        else
            iov_offset++;
        memcpy(mpool + pool_offset, iov[i].iov_base, ll);

        iovpool[iov_offset].iov_base = mpool + pool_offset;
        iovpool[iov_offset].iov_len = ll;

        len += iov[i].iov_len;
    }
//while(btable[off + curindex[toff]].rstatus == BENTRY_BUSY);
    btable[off + curindex[toff]].sysnum = __NR_writev;
    btable[off + curindex[toff]].rstatus = BENTRY_BUSY;
    btable[off + curindex[toff]].nargs = 3;
    btable[off + curindex[toff]].args[0] = fd;
    btable[off + curindex[toff]].args[1] = (long)(iovpool + iov_offset - iovcnt + 1);
    btable[off + curindex[toff]].args[2] = iovcnt;
    btable[off + curindex[toff]].pid = main_thread_pid + off;
//printf("fill writev at %d\n", off + curindex[toff]);
    if (curindex[toff] == MAX_TABLE_SIZE - 1) {
#if 0
     	    if (curindex[1] == MAX_THREAD_NUM - 1) {
            curindex[1] = 1;
        } else {
            curindex[1]++;
        }
#endif
        curindex[toff] = 1;
    } else {
        curindex[toff]++;
    }
    /* assume always success */
    //printf("-> %d\n", len);
    return len;

}
#endif

__attribute__((constructor)) static void setup(void) {
    int i;
    size_t pgsize = getpagesize();
    in_segment = 0;
    batch_num = 0;
    syscall_num = 0;
    predict_state = PREDICT_S1;
#if DYNAMIC_PRE_ENABLE
    predict_res = 0;
#else
    predict_res = 1;
#endif
    /* init memory pool */
    mpool = (void *)malloc(sizeof(unsigned char) * MAX_POOL_SIZE);
    pool_offset = 0;

    iovpool = (struct iovec*)malloc(sizeof(struct iovec) * MAX_POOL_IOV_SIZE);
    iov_offset = 0;
    /* get pid of main thread */
    main_thread_pid = syscall(186);
    btable =
        (struct batch_entry *)aligned_alloc(pgsize, pgsize * MAX_THREAD_NUM);

    /* store glibc function */
    real_open = real_open ? real_open : dlsym(RTLD_NEXT, "open");
    real_close = real_close ? real_close : dlsym(RTLD_NEXT, "close");
    real_write = real_write ? real_write : dlsym(RTLD_NEXT, "write");
    real_read = real_read ? real_read : dlsym(RTLD_NEXT, "read");
    real_sendto = real_sendto ? real_sendto : dlsym(RTLD_NEXT, "sendto");
    real_writev = real_writev ? real_writev : dlsym(RTLD_NEXT, "writev");
    real_shutdown = real_shutdown ? real_shutdown : dlsym(RTLD_NEXT, "shutdown");

    syscall(__NR_lioo_register, btable);

    for (i = 0; i < MAX_THREAD_NUM; i++)
        curindex[i] = 1;
}