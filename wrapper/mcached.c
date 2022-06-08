ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags)
{
    if (!in_segment) {
        return real_sendmsg(sockfd, msg, flags);
    }

    int idx = this_worker_id * RATIO + (sockfd % RATIO);
    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    /* check out of bound */
    msg_offset = msg_offset & MSG_MASK;
    iov_offset = iov_offset & IOV_MASK;

    int res = 0;
    struct iovec* iov = msg->msg_iov;
    int iovlen = msg->msg_iovlen, start_iov_offset = iov_offset;

    for (int k = 0; k < iovlen; k++) {
        int ll = iov[k].iov_len; // number of bytes

        /* handle string */
        if (pool_offset + (ull)(ll) > MAX_POOL_SIZE)
            pool_offset = 0;

        memcpy((void*)((unsigned char*)mpool + pool_offset), iov[k].iov_base, ll);

        iov_offset = iov_offset & IOV_MASK;
        iovpool[iov_offset].iov_base = (void*)((unsigned char*)mpool + pool_offset);
        iovpool[iov_offset].iov_len = ll;

        /* update index of iovec array */
        iov_offset++;

        /* update offset of character pool */
        pool_offset += (size_t)(ll);

        res += ll;
    }

    /* copy msghdr */
    msgpool[msg_offset].msg_name = NULL; // FIXME: should be more general
    msgpool[msg_offset].msg_namelen = 0;
    msgpool[msg_offset].msg_iov = (struct iovec*)(&iovpool[start_iov_offset]);
    msgpool[msg_offset].msg_iovlen = iovlen;
    msgpool[msg_offset].msg_control = NULL;
    msgpool[msg_offset].msg_controllen = 0;
    msgpool[msg_offset].msg_flags = msg->msg_flags;

    table[idx]->user_tables[i][j].sysnum = __ESCA_sendmsg;
    table[idx]->user_tables[i][j].nargs = 3;
    table[idx]->user_tables[i][j].args[0] = sockfd;
    table[idx]->user_tables[i][j].args[1] = (long)(&msgpool[msg_offset]);
    table[idx]->user_tables[i][j].args[2] = flags;

    msg_offset++;
    update_index(idx);

    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return res;
}
