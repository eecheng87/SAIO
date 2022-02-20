#include "preload.h"
#include <sys/mman.h>

#if defined(__aarch64__)
#include "../module/include/aarch64_syscall.h"
#elif defined(__x86_64__)
#include "../module/include/x86_syscall.h"
#endif

int in_segment;
int main_pid;
int batch_num; /* number of busy entry */
int syscall_num; /* number of syscall triggered currently */
size_t pgsize;

void* mpool; /* memory pool */
ull pool_offset;
struct iovec* iovpool; /* pool for iovector */
ull iov_offset;

/* declare shared table, pin user space addr. to kernel phy. addr by kmap */
int this_worker_id;

/* user worker can't touch worker's table in diff. set */
esca_table_t* table[MAX_CPU_NUM];

void init_worker(int idx)
{
    in_segment = 0;
    batch_num = 0;
    syscall_num = 0;

    /* expect idx = 0 ~ MAX_USR_WORKER - 1 */
    this_worker_id = idx;

    printf("Create worker ID = %d, pid = %d\n", this_worker_id, getpid());

    if (idx >= MAX_USR_WORKER) {
        printf("[ERROR] Process exceed limit\n");
        goto init_worker_exit;
    }

    int set_index = 0;
    for (int i = idx * RATIO; i < idx * RATIO + RATIO; i++) {
        /* headers in same set using a same page */
        esca_table_t* header = NULL;
        if (i == idx * RATIO) {
            header = table[i] = (esca_table_t*)aligned_alloc(pgsize, pgsize);
        }
        table[i] = table[idx * RATIO] + (i - idx * RATIO);

        /* allocate tables */
        esca_table_entry_t* alloc = (esca_table_entry_t*)aligned_alloc(pgsize, pgsize * MAX_TABLE_LEN);
        if (!alloc) {
            printf("[ERROR] alloc failed\n");
            goto init_worker_exit;
        }

        /* pin tables to kernel */
        syscall(__NR_esca_register, header, alloc, i, set_index++);

        /* pin table from kernel to user */
        for (int j = 0; j < MAX_TABLE_LEN; j++) {
            table[i]->user_tables[j] = alloc + j * MAX_TABLE_ENTRY;
        }
    }

init_worker_exit:
    return;
}

long batch_start()
{
    int i = this_worker_id;
    in_segment = 1;

    for (int j = i * RATIO; j < i * RATIO + RATIO; j++) {
        if (esca_unlikely(ESCA_READ_ONCE(table[j]->flags) & ESCA_WORKER_NEED_WAKEUP)) {
            table[j]->flags |= ESCA_START_WAKEUP;
            syscall(__NR_esca_wakeup, j);
            table[j]->flags &= ~ESCA_START_WAKEUP;
        }
    }

    return 0;
}

long batch_flush()
{
    in_segment = 0;
    if (batch_num == 0)
        return 0;

    syscall(__NR_esca_wait, this_worker_id * RATIO);
    batch_num = 0;

    return 0;
}

void update_index(int idx)
{
    // avoid overwriting;
    // FIXME: need to consider more -> cross table scenario
    // FIXME: order of the head might be protected by barrier
    while ((table[idx]->tail_entry + 1 == table[idx]->head_entry) && (table[idx]->tail_table == table[idx]->head_table))
        ;

    if (table[idx]->tail_entry == MAX_TABLE_ENTRY - 1) {
        table[idx]->tail_entry = 0;
        table[idx]->tail_table = (table[idx]->tail_table == MAX_TABLE_LEN - 1) ? 0 : table[idx]->tail_table + 1;
    } else {
        table[idx]->tail_entry++;
    }
}

ssize_t shutdown(int fd, int how)
{
#if 1
    if (!in_segment) {
        return real_shutdown(fd, how);
    }
#endif
    // TODO: imple. hash function for table len. greater than 2 scenario
#if 0
    int idx = 0;
//#else
    int idx = fd & 1;
#endif
    int idx = this_worker_id * RATIO + (fd % RATIO);

    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    //printf("pid=%d, shutdown(%d)", getpid(), fd);

    table[idx]->user_tables[i][j].sysnum = __ESCA_shutdown;
    table[idx]->user_tables[i][j].nargs = 2;
    table[idx]->user_tables[i][j].args[0] = fd;
    table[idx]->user_tables[i][j].args[1] = how;

    update_index(idx);

    // status must be changed in the last !!
    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return 0;
}

#if 1
off_t off_arr[MAX_CPU_NUM][MAX_TABLE_ENTRY * MAX_TABLE_LEN + 1];
ssize_t sendfile64(int out_fd, int in_fd, off_t* offset, size_t count)
{
#if 1
    if (!in_segment) {
        return real_sendfile(out_fd, in_fd, offset, count);
    }
#endif

    int idx = this_worker_id * RATIO + (out_fd % RATIO);
    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;
    int off = (i << 6) + j;

    // printf("pid = %d, sendfile(%d, %d, %d) off_arr[%d][%d] at table[%d].tables[%d][%d]\n", getpid(), out_fd, in_fd, count, idx, i * MAX_TABLE_ENTRY + j, idx, i, j);

    off_arr[idx][i * MAX_TABLE_ENTRY + j] = *offset;

    table[idx]->user_tables[i][j].sysnum = __ESCA_sendfile;
    table[idx]->user_tables[i][j].nargs = 4;
    table[idx]->user_tables[i][j].args[0] = out_fd;
    table[idx]->user_tables[i][j].args[1] = in_fd;
    table[idx]->user_tables[i][j].args[2] = &off_arr[idx][i * MAX_TABLE_ENTRY + j];
    table[idx]->user_tables[i][j].args[3] = count;

    *offset = *offset + count;
    update_index(idx);
    // status must be changed in the last !!
    // maybe need to add barrier at here !!

    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return count;
}
#else
ssize_t writev(int fd, const struct iovec* iov, int iovcnt)
{

    if (!in_segment) {
        return real_writev(fd, iov, iovcnt);
    }
    // TODO: imple. hash function for table len. greater than 2 scenario

    int idx = this_worker_id * RATIO + (fd % RATIO);
    batch_num++;

    ull start_iov_index;
    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    int len = 0;

    if (iov_offset + iovcnt >= MAX_POOL_IOV_SIZE)
        iov_offset = 0;

    start_iov_index = iov_offset;

    for (int k = 0; k < iovcnt; k++) {
        int ll = iov[k].iov_len; // number of bytes

        /* handle string */
        if (pool_offset + (ull)(ll << 3) > MAX_POOL_SIZE)
            pool_offset = 0;

        memcpy((void*)((ull)mpool + pool_offset), iov[k].iov_base, ll);

        iovpool[iov_offset].iov_base = (void*)((ull)mpool + pool_offset);
        iovpool[iov_offset].iov_len = ll;

        /* update index of iovec array */
        iov_offset++;

        /* update offset of character pool */
        pool_offset += (ull)(ll << 3);

        len += ll;
    }

    table[idx]->user_tables[i][j].sysnum = __NR_writev;
    table[idx]->user_tables[i][j].nargs = 3;
    table[idx]->user_tables[i][j].args[0] = fd;
    table[idx]->user_tables[i][j].args[1] = (long)(&iovpool[start_iov_index]);
    table[idx]->user_tables[i][j].args[2] = iovcnt;

    update_index(idx);

    // status must be changed in the last !!
    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume always success */
    return len;
}
#endif

__attribute__((constructor)) static void setup(void)
{
    main_pid = getpid();
    pgsize = getpagesize();

    mpool = (void*)malloc(sizeof(unsigned char) * MAX_POOL_SIZE);
    pool_offset = 0;
    iovpool = (struct iovec*)malloc(sizeof(struct iovec) * MAX_POOL_IOV_SIZE);
    iov_offset = 0;

    /* store glibc function */
    real_open = real_open ? real_open : dlsym(RTLD_NEXT, "open");
    real_close = real_close ? real_close : dlsym(RTLD_NEXT, "close");
    real_write = real_write ? real_write : dlsym(RTLD_NEXT, "write");
    real_read = real_read ? real_read : dlsym(RTLD_NEXT, "read");
    real_writev = real_writev ? real_writev : dlsym(RTLD_NEXT, "writev");
    real_shutdown = real_shutdown ? real_shutdown : dlsym(RTLD_NEXT, "shutdown");
    real_sendfile = real_sendfile ? real_sendfile : dlsym(RTLD_NEXT, "sendfile");
}
