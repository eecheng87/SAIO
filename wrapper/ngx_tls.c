ssize_t write(int fd, const void* buf, size_t count)
{
    if (!in_segment) {
	return real_write(fd, buf, count);
    }

    int idx = this_worker_id * RATIO + (fd % RATIO);

    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    if (pool_offset + (ull)(count) > MAX_POOL_SIZE)
        pool_offset = 0;

    memcpy((void*)((ull)mpool + pool_offset), buf, count);

    table[idx]->user_tables[i][j].sysnum = __ESCA_write;
    table[idx]->user_tables[i][j].nargs = 3;
    table[idx]->user_tables[i][j].args[0] = fd;
    table[idx]->user_tables[i][j].args[1] = (void*)((ull)mpool + pool_offset);
    table[idx]->user_tables[i][j].args[2] = count;

    update_index(idx);

    /* update offset of character pool */
    pool_offset += (ull)(count);

    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return count;
}