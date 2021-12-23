#include "preload.h"
int in_segment;
int batch_num; /* number of busy entry */
int syscall_num; /* number of syscall triggered currently */

/* declare shared table, pin user space addr. to kernel phy. addr by kmap */
esca_table_t* table;

long batch_start()
{
    in_segment = 1;

    for (int i = 0; i < MAX_CPU_NUM; i++) {
        if (esca_unlikely(ESCA_READ_ONCE(table[i].flags) & ESCA_WORKER_NEED_WAKEUP)) {
            table[i].flags |= ESCA_START_WAKEUP;
            syscall(__NR_esca_wakeup, i);
            table[i].flags &= ~ESCA_START_WAKEUP;
        }
    }

    return 0;
}

long batch_flush()
{

    in_segment = 0;
    if (batch_num == 0)
        return 0;
    //printf("waiting for %d requests completion\n", batch_num);
    // sysnum is for temp usage -> record current batch num
    //while(btable[1].sysnum > 0){
    //printf("batch_num=%d btable[0].sysnum=%d\n", batch_num, btable[1].sysnum);
    //}

    syscall(__NR_esca_wait);
    batch_num = 0;
    //printf("Completion\n");
    return 0;
}

void update_index(int idx)
{
    // avoid overwriting;
    // TODO: need to consider more -> cross table scenario
    // TODO: order of the head might be protected by barrier
    while ((table[idx].tail_entry + 1 == table[idx].head_entry) && (table[idx].tail_table == table[idx].head_table))
        ;

    if (table[idx].tail_entry == MAX_TABLE_ENTRY - 1) {
        table[idx].tail_entry = 0;
        table[idx].tail_table = (table[idx].tail_table == MAX_TABLE_LEN - 1) ? 0 : table[idx].tail_table + 1;
    } else {
        table[idx].tail_entry++;
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
    int idx = fd & 1;
    batch_num++;

    int i = table[idx].tail_table;
    int j = table[idx].tail_entry;

    //printf("fill shutdown at table[%d].tables[%d][%d]\n", idx, i, j);

    table[idx].tables[i][j].sysnum = __NR_shutdown;
    table[idx].tables[i][j].nargs = 2;
    table[idx].tables[i][j].args[0] = fd;
    table[idx].tables[i][j].args[1] = how;

    update_index(idx);

    // status must be changed in the last !!
    esca_smp_store_release(&table[idx].tables[i][j].rstatus, BENTRY_BUSY);
    //printf("fill shutdown at [%d][%d]\n", toff, curindex[toff]);

    /* assume success */
    return 0;
}

#if 0
off_t off_arr[MAX_CPU_NUM][MAX_TABLE_ENTRY * MAX_TABLE_LEN + 1];
ssize_t sendfile64(int out_fd, int in_fd, off_t* offset, size_t count)
{
#if 1
    if (!in_segment) {
        return real_sendfile(out_fd, in_fd, offset, count);
    }
#endif
    int idx = out_fd & 1;
    batch_num++;

    int i = table[idx].tail_table;
    int j = table[idx].tail_entry;
    int off = (i << 6) + j;

    //printf("sendfile(%d, %d, %d) off_arr[%d][%d] at table[%d].tables[%d][%d]\n", out_fd, in_fd, count, idx, i * MAX_TABLE_ENTRY + j, idx, i, j);

    off_arr[idx][i * MAX_TABLE_ENTRY + j] = *offset;
    //off_arr[i * MAX_TABLE_ENTRY + j] = *offset;

    table[idx].tables[i][j].sysnum = 40;
    table[idx].tables[i][j].nargs = 4;
    table[idx].tables[i][j].args[0] = out_fd;
    table[idx].tables[i][j].args[1] = in_fd;
    table[idx].tables[i][j].args[2] = &off_arr[idx][i * MAX_TABLE_ENTRY + j];
    table[idx].tables[i][j].args[3] = count;

    *offset = *offset + count;
    update_index(idx);
    // status must be changed in the last !!
    // maybe need to add barrier at here !!

    esca_smp_store_release(&table[idx].tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return count;
}
#endif

#if 1
ssize_t writev(int fd, const struct iovec* iov, int iovcnt)
{

    if (!in_segment) {
        return real_writev(fd, iov, iovcnt);
    }
    // TODO: imple. hash function for table len. greater than 2 scenario
    int idx = fd & 1;
    batch_num++;

    int i = table[idx].tail_table;
    int j = table[idx].tail_entry;

    int len = 0;
    for (int i = 0; i < iovcnt; i++) {
        len += iov[i].iov_len;
    }
#if 0
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
#endif
    //while(btable[off + curindex[toff]].rstatus == BENTRY_BUSY);
    table[idx].tables[i][j].sysnum = __NR_writev;
    table[idx].tables[i][j].rstatus = BENTRY_BUSY;
    table[idx].tables[i][j].nargs = 3;
    table[idx].tables[i][j].args[0] = fd;
    table[idx].tables[i][j].args[1] = iov; //(long)(iovpool + iov_offset - iovcnt + 1);
    table[idx].tables[i][j].args[2] = iovcnt;

    update_index(idx);

    // status must be changed in the last !!
    esca_smp_store_release(&table[idx].tables[i][j].rstatus, BENTRY_BUSY);
    /* assume always success */
    //printf("-> %d\n", len);
    return len;
}
#endif

__attribute__((constructor)) static void setup(void)
{
    int i;
    size_t pgsize = getpagesize();
    esca_table_entry_t* alloc_head1;
    esca_table_entry_t* alloc_head2;
    in_segment = 0;
    batch_num = 0;
    syscall_num = 0;

    table = (esca_table_t*)aligned_alloc(pgsize, pgsize);

    // TODO: make this more flexible
    alloc_head1 = (esca_table_entry_t*)aligned_alloc(pgsize, pgsize * MAX_TABLE_LEN);
    alloc_head2 = (esca_table_entry_t*)aligned_alloc(pgsize, pgsize * MAX_TABLE_LEN);

    // printf("alloc_head1 = %p, alloc_head2 = %p\n", alloc_head1, alloc_head2);

    /* store glibc function */
    real_open = real_open ? real_open : dlsym(RTLD_NEXT, "open");
    real_close = real_close ? real_close : dlsym(RTLD_NEXT, "close");
    real_write = real_write ? real_write : dlsym(RTLD_NEXT, "write");
    real_read = real_read ? real_read : dlsym(RTLD_NEXT, "read");
    real_writev = real_writev ? real_writev : dlsym(RTLD_NEXT, "writev");
    real_shutdown = real_shutdown ? real_shutdown : dlsym(RTLD_NEXT, "shutdown");
    real_sendfile = real_sendfile ? real_sendfile : dlsym(RTLD_NEXT, "sendfile");

    syscall(__NR_esca_register, table, alloc_head1, alloc_head2);

    /* Need to be assigned after kmap from kernel */
    /* TODO: maybe this can be shorten */
    //table[0].tables[0] = alloc_head1;
    //table[1].tables[0] = alloc_head2;

#if 1
    for (i = 0; i < MAX_TABLE_LEN; i++) {
        table[0].tables[i] = alloc_head1 + i * MAX_TABLE_ENTRY;
    }
    for (i = 0; i < MAX_TABLE_LEN; i++) {
        table[1].tables[i] = alloc_head2 + i * MAX_TABLE_ENTRY;
    }
#endif
}
