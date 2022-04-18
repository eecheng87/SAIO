#include <sys/socket.h>
ssize_t send(int sockfd, const void* buf, size_t len, int flags)
{
#if 1
    if (!in_segment) {
        return real_send(sockfd, buf, len, flags);
    }
#endif

    int idx = this_worker_id * RATIO + (sockfd % RATIO);

    batch_num++;

    int i = table[idx]->tail_table;
    int j = table[idx]->tail_entry;

    table[idx]->user_tables[i][j].sysnum = __ESCA_sendto;
    table[idx]->user_tables[i][j].nargs = 6;
    table[idx]->user_tables[i][j].args[0] = sockfd;
    table[idx]->user_tables[i][j].args[1] = buf;
    table[idx]->user_tables[i][j].args[2] = len;
    table[idx]->user_tables[i][j].args[3] = flags;
    table[idx]->user_tables[i][j].args[4] = NULL;
    table[idx]->user_tables[i][j].args[5] = 0;

    update_index(idx);

    esca_smp_store_release(&table[idx]->user_tables[i][j].rstatus, BENTRY_BUSY);

    /* assume success */
    return len;
}