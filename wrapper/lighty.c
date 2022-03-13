ssize_t shutdown(int fd, int how)
{
#if 1
    if (!in_segment) {
        return real_shutdown(fd, how);
    }
#endif
    // TODO: imple. hash function for table len. greater than 2 scenario

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
off_t off_arr[CPU_NUM_LIMIT][TABLE_ENT_LIMIT * TABLE_LEN_LIMIT + 1];
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
        if (pool_offset + (ull)(ll) > MAX_POOL_SIZE)
            pool_offset = 0;

        memcpy((void*)((ull)mpool + pool_offset), iov[k].iov_base, ll);

        iovpool[iov_offset].iov_base = (void*)((ull)mpool + pool_offset);
        iovpool[iov_offset].iov_len = ll;

        /* update index of iovec array */
        iov_offset++;

        /* update offset of character pool */
        pool_offset += (ull)(ll);

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