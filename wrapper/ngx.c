int close(int fd)
{
#if 1
    if (!in_segment) {
        return real_close(fd);
    }
#endif
    int idx = this_worker_id * RATIO + (fd % RATIO);

    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    table[idx]->user_tables[i][j].sysnum = __ESCA_close;
    table[idx]->user_tables[i][j].nargs = 1;
    table[idx]->user_tables[i][j].args[0] = fd;

    update_index(idx);

    // status must be changed in the last !!
    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return 0;
}

//off_t off_arr[MAX_CPU_NUM][MAX_TABLE_ENTRY * MAX_TABLE_LEN + 1];
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

    // printf("pid = %d, sendfile(%d, %d, %d, %d) at table[%d].tables[%d][%d]\n", getpid(), out_fd, in_fd, offset, count, idx, i, j);

    off_arr[i * MAX_TABLE_ENTRY + j] = *offset;

    table[idx]->user_tables[i][j].sysnum = __ESCA_sendfile;
    table[idx]->user_tables[i][j].nargs = 4;
    table[idx]->user_tables[i][j].args[0] = out_fd;
    table[idx]->user_tables[i][j].args[1] = in_fd;
    table[idx]->user_tables[i][j].args[2] = 0; //&off_arr[i * MAX_TABLE_ENTRY + j];
    table[idx]->user_tables[i][j].args[3] = count;

    //*offset = *offset + count;
    update_index(idx);
    // status must be changed in the last !!
    // maybe need to add barrier at here !!

    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return count;
}
