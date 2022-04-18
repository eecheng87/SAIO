ssize_t write(int fd, const void* buf, size_t count)
{
#if 1
    if (!in_segment) {
        return real_write(fd, buf, count);
    }
#endif
    int idx = this_worker_id * RATIO + (fd % RATIO);

    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    table[idx]->user_tables[i][j].sysnum = __ESCA_write;
    table[idx]->user_tables[i][j].nargs = 3;
    table[idx]->user_tables[i][j].args[0] = fd;
    table[idx]->user_tables[i][j].args[1] = buf;
    table[idx]->user_tables[i][j].args[2] = count;

    update_index(idx);

    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return count;
}

ssize_t read(int fd, void* buf, size_t count)//, unsigned long long user_data)
{
#if 1
    if (!in_segment) {
        return real_read(fd, buf, count);
    }
#endif

    int idx = this_worker_id * RATIO + (fd % RATIO);

    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    table[idx]->user_tables[i][j].sysnum = 0;
    table[idx]->user_tables[i][j].nargs = 3;
    table[idx]->user_tables[i][j].args[0] = fd;
    table[idx]->user_tables[i][j].args[1] = buf;
    table[idx]->user_tables[i][j].args[2] = count;

    update_index(idx);

    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return 0;
}