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

long batch_start() {
    in_segment = 1;
    //btable[1].sysnum = batch_num = 0;
    return 0;
}

long batch_flush() {
// FIXME: rename later -> purpose of this: waiting for completion (blocking)
	//printf("btable[1]=%d\n", btable[1].sysnum);
	//return 0;
    in_segment = 0;
    if (batch_num == 0)
        return 0;
	printf("waiting for %d requests completion\n", batch_num);
	// sysnum is for temp usage -> record current batch num
    //while(btable[1].sysnum > 0){
    	//printf("batch_num=%d btable[0].sysnum=%d\n", batch_num, btable[1].sysnum);
    //}

    //btable[1].sysnum = batch_num = 0;
    syscall(__NR_lioo_wait);
    batch_num = 0;
    btable[1].sysnum = 0;
    printf("Completion\n");
	return 0;
}

ssize_t shutdown(int fd, int how) {
#if 1
    if (!in_segment) {
        return real_shutdown(fd, how);
    }
#endif
	batch_num++;
	btable[1].sysnum++;
    //int off, toff = 0;
    //off = 1 << 6; /* 6 = log64 */
	
    int off, toff = /*(fd % 2)*/ + 1;
    off = toff << 6;

	// ensure no override
	//while(btable[off + curindex[toff]].rstatus == BENTRY_BUSY);
	while(batch_num >= 60){printf("-");};
    btable[off + curindex[toff]].sysnum = __NR_shutdown;
    btable[off + curindex[toff]].nargs = 2;
    btable[off + curindex[toff]].args[0] = fd;
    btable[off + curindex[toff]].args[1] = how;
    btable[off + curindex[toff]].pid = main_thread_pid + off;
    
    // status must be changed in the last !!
    btable[off + curindex[toff]].rstatus = BENTRY_BUSY;
    //while(btable[off + curindex[toff]].rstatus == BENTRY_BUSY);
	//printf("fill shutdown at [%d][%d]\n", toff, curindex[toff]);
    if (curindex[toff] == MAX_TABLE_SIZE - 1) {
		curindex[toff] = 1;
    } else {
        curindex[toff]++;
    }

    /* assume success */
    return 0;
}

off_t off_arr[300];
ssize_t sendfile64(int out_fd, int in_fd, off_t *offset, size_t count) {
#if 1
    if (!in_segment) {
        return real_sendfile(out_fd, in_fd, offset, count);
    }
#endif
	batch_num++;
	btable[1].sysnum++;
	
    int off, toff = /*(out_fd % 2)*/ + 1;
    //off = 1 << 6; /* 6 = log64 */
    off = toff << 6;
	
	// ensure no override
	//while(btable[off + curindex[toff]].rstatus == BENTRY_BUSY);
	while(batch_num >= 60){printf(".");};
	printf("sendfile(%d, %d, %d) off_arr[%d]\n", out_fd, in_fd, count, off + curindex[toff]);
	off_arr[off + curindex[toff]] = *offset;
	//printf("-> %d\n", off+curindex[toff]);
    btable[off + curindex[toff]].sysnum = 40;
    btable[off + curindex[toff]].nargs = 4;
    btable[off + curindex[toff]].args[0] = out_fd;
    btable[off + curindex[toff]].args[1] = in_fd;
    btable[off + curindex[toff]].args[2] = &off_arr[off + curindex[toff]];//(unsigned long)offset; // maybe: cast is super important
    btable[off + curindex[toff]].args[3] = count;
    btable[off + curindex[toff]].pid = main_thread_pid + off;
	
	
	// status must be changed in the last !!
	// maybe need to add barrier at here !!
	*offset = *offset + count;
	btable[off + curindex[toff]].rstatus = BENTRY_BUSY;
	
    if (curindex[toff] == MAX_TABLE_SIZE - 1) {
        curindex[toff] = 1;
    } else {
        curindex[toff]++;
    }
    
    /* assume success */
    return count;
}

#if 0
ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    syscall_num++;
    if (!in_segment) {
        return real_writev(fd, iov, iovcnt);
    }
    batch_num++;
    btable[1].sysnum++;

    /*int off, toff = 0, len = 0, i;
    off = 1 << 6;*/

	int off, toff = /*(out_fd % 2)*/ + 1;
    //off = 1 << 6; /* 6 = log64 */
    off = toff << 6;

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
    real_sendfile = real_sendfile ? real_sendfile : dlsym(RTLD_NEXT, "sendfile");

    syscall(__NR_lioo_register, btable);

    for (i = 0; i < MAX_THREAD_NUM; i++)
        curindex[i] = 1;
        
	btable[1].sysnum = 0;
}
