/* Instead of sendfile64, kTLS use sendfile as system call wrapper */
ssize_t sendfile(int out_fd, int in_fd, off_t* offset, size_t count)
{
    if (!in_segment) {
        return real_sendfile(out_fd, in_fd, offset, count);
    }

    int idx = this_worker_id * RATIO + (in_fd % RATIO);
    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    table[idx]->user_tables[i][j].sysnum = __ESCA_sendfile;
    table[idx]->user_tables[i][j].nargs = 4;
    table[idx]->user_tables[i][j].args[0] = out_fd;
    table[idx]->user_tables[i][j].args[1] = in_fd;
    table[idx]->user_tables[i][j].args[2] = 0;
    table[idx]->user_tables[i][j].args[3] = count;

    update_index(idx);

    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return count;
}
