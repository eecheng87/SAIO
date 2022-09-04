#define MAX_IOV_LEN 1000
ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags)
{
    if (!in_segment) {
        return real_sendmsg(sockfd, msg, flags);
    }

    int idx = this_worker_id;
    batch_num++;

    int i = table[idx].tail_table;
    int j = table[idx].tail_entry;

    /* check out of bound */
    msg_offset = msg_offset & MSG_MASK;
    iov_offset = iov_offset & IOV_MASK;

    int res = 0;
    int start_iov_offset = 0;
    struct iovec* iov = msg->msg_iov;
    int iovlen = msg->msg_iovlen;

    // minor optimization; only works if we guarantee offset always is one
#if 0
    if(iov_offset + MAX_IOV_LEN > MAX_POOL_IOV_SIZE){
        start_iov_offset = iov_offset = 0;
    }else{
        start_iov_offset = iov_offset;
    }
#endif
    for (int k = 0; k < iovlen; k++) {
        int ll = iov[k].iov_len; // number of bytes

        /* handle string */
        if (pool_offset + (ull)(ll) > MAX_POOL_SIZE)
            pool_offset = 0;

        memcpy((void*)((unsigned char*)mpool[idx] + pool_offset), iov[k].iov_base, ll);

        iov_offset = iov_offset & IOV_MASK;
        iovpool[idx][iov_offset].iov_base = (void*)((unsigned char*)mpool[idx] + pool_offset);
        iovpool[idx][iov_offset].iov_len = ll;

        /* update index of iovec array */
        iov_offset++;

        /* update offset of character pool */
        pool_offset += (size_t)(ll);

        res += ll;
    }

    /* copy msghdr */
    msgpool[idx][msg_offset].msg_name = NULL; // FIXME: should be more general
    msgpool[idx][msg_offset].msg_namelen = 0;
    msgpool[idx][msg_offset].msg_iov = (struct iovec*)(&iovpool[idx][start_iov_offset]);
    msgpool[idx][msg_offset].msg_iovlen = iovlen;
    msgpool[idx][msg_offset].msg_control = NULL;
    msgpool[idx][msg_offset].msg_controllen = 0;
    msgpool[idx][msg_offset].msg_flags = msg->msg_flags;

    table[idx].user_tables[i][j].sysnum = __ESCA_sendmsg;
    table[idx].user_tables[i][j].nargs = 3;
    table[idx].user_tables[i][j].args[0] = sockfd;
    table[idx].user_tables[i][j].args[1] = (long)(&msgpool[idx][msg_offset]);
    table[idx].user_tables[i][j].args[2] = flags;

    msg_offset++;
    update_index(idx);

    esca_smp_store_release(&table[idx].user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return res;
}


// TLS
ssize_t write(int fd, const void* buf, size_t count)
{
    // FIXME: propose better solution; don't hard code it
    if (!in_segment || count != 27) {
        return real_write(fd, buf, count);
    }

    int idx = this_worker_id * RATIO + (fd % RATIO);//this_worker_id;
    batch_num++;

    int i = table[idx].tail_table;
    int j = table[idx].tail_entry;

    if (pool_offset + (ull)(count) > MAX_POOL_SIZE)
        pool_offset = 0;

    memcpy((void*)((ull)mpool[idx] + pool_offset), buf, count);

    table[idx].user_tables[i][j].sysnum = __ESCA_write;
    table[idx].user_tables[i][j].nargs = 3;
    table[idx].user_tables[i][j].args[0] = fd;
    table[idx].user_tables[i][j].args[1] = (void*)((ull)mpool[idx] + pool_offset);
    table[idx].user_tables[i][j].args[2] = count;

    update_index(idx);

    /* update offset of character pool */
    pool_offset += (ull)(count);

    esca_smp_store_release(&table[idx].user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return count;
}

