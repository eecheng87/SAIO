#include "preload.h"
#include <sys/mman.h>

#if defined(__aarch64__)
#include "../module/include/aarch64_syscall.h"
#elif defined(__x86_64__)
#include "../module/include/x86_syscall.h"
#endif

// FIXME: not reliable
#define MAX_THREAD_NUM 20
int main_pid;
size_t pgsize;

void* mpool[MAX_THREAD_NUM]; /* memory pool */
struct iovec* iovpool[MAX_THREAD_NUM]; /* pool for iovector */
struct msghdr* msgpool[MAX_THREAD_NUM]; /* pool for msgpool */
__thread ull pool_offset;
__thread ull iov_offset;
__thread ull msg_offset;

/* Global configurable variable */
int ESCA_LOCALIZE;
int MAX_TABLE_ENTRY;
int MAX_TABLE_LEN;
int MAX_USR_WORKER;
int MAX_CPU_NUM;
int RATIO;
int DEFAULT_IDLE_TIME;
int AFF_OFF;

/* declare shared table, pin user space addr. to kernel phy. addr by kmap */
int main_thread_pid;
__thread int this_worker_id;
__thread int in_segment;
__thread int batch_num; /* number of busy entry */
__thread int syscall_num; /* number of syscall triggered currently */

/* user worker can't touch worker's table in diff. set */
esca_table_t* table;

void init_worker(int idx)
{
    in_segment = 0;
    batch_num = 0;
    syscall_num = 0;

    this_worker_id = idx;
    printf("Create worker ID = %ld, tid = %d, pid = %d\n", this_worker_id, gettid(), getpid());

    if (idx >= MAX_USR_WORKER) {
        printf("[ERROR] Process exceed limit\n");
        goto init_worker_exit;
    }

    for (int i = idx * RATIO; i < idx * RATIO + RATIO; i++) {
        /* allocate tables */
        esca_table_entry_t* alloc = (esca_table_entry_t*)aligned_alloc(pgsize, pgsize * MAX_TABLE_LEN);
        if (!alloc) {
            printf("[ERROR] alloc failed\n");
            goto init_worker_exit;
        }

        /* pin tables to kernel */
        syscall(__NR_esca_register, NULL, alloc, i, 0);

        /* pin table from kernel to user */
        for (int j = 0; j < MAX_TABLE_LEN; j++) {
            table[i].user_tables[j] = alloc + j * MAX_TABLE_ENTRY;
        }
    }

    mpool[idx] = (void*)malloc(sizeof(unsigned char) * MAX_POOL_SIZE);
    pool_offset = 0;
    iovpool[idx] = (struct iovec*)malloc(sizeof(struct iovec) * MAX_POOL_IOV_SIZE);
    iov_offset = 0;
    msgpool[idx] = (struct msghdr*)malloc(sizeof(struct msghdr) * MAX_POOL_MSG_SIZE);
    msg_offset = 0;

init_worker_exit:
    return;
}

long batch_start()
{
    int i = this_worker_id;
    in_segment = 1;

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

void toggle_region()
{
    in_segment ^= 1;
}

void update_index(int idx)
{
    // avoid overwriting;
    // FIXME: need to consider more -> cross table scenario
    // FIXME: order of the head might be protected by barrier
    while ((table[idx].tail_entry + 1 == table[idx].head_entry) && (table[idx].tail_table == table[idx].head_table))
        ;

    if (table[idx].tail_entry == MAX_TABLE_ENTRY - 1) {
        table[idx].tail_entry = 0;
        table[idx].tail_table = (table[idx].tail_table == MAX_TABLE_LEN - 1) ? 0 : table[idx].tail_table + 1;
    } else {
        table[idx].tail_entry++;
    }
}

#include "target-preload.c"

void init_config(esca_config_t* c)
{
    ESCA_LOCALIZE = c->esca_localize;
    MAX_TABLE_ENTRY = c->max_table_entry;
    MAX_TABLE_LEN = c->max_table_len;
    MAX_USR_WORKER = c->max_usr_worker;
    MAX_CPU_NUM = c->max_ker_worker;
    RATIO = (MAX_CPU_NUM / MAX_USR_WORKER);
    DEFAULT_IDLE_TIME = c->default_idle_time;
    AFF_OFF = c->affinity_offset;

    printf("\033[0;33m");
    printf(" Localize: \033[0;37m%s\033[0;33m\n", ESCA_LOCALIZE ? "Enable" : "Disable");
    printf(" MAX_TABLE_ENTRY: \033[0;37m%d\033[0;33m\n", MAX_TABLE_ENTRY);
    printf(" MAX_TABLE_LEN: \033[0;37m%d\033[0;33m\n", MAX_TABLE_LEN);
    printf(" MAX_USR_WORKER: \033[0;37m%d\033[0;33m\n", MAX_USR_WORKER);
    printf(" MAX_KER_WORKER: \033[0;37m%d\033[0;33m\n", MAX_CPU_NUM);
    printf(" AFF_OFF: \033[0;37m%d\033[0;33m\n", AFF_OFF);

    if (ESCA_LOCALIZE)
        printf(" # of K-worker per CPU: \033[0;37m%d\n", RATIO);
    printf("\033[0m");
}

__attribute__((constructor)) static void setup(void)
{
    FILE* fp;
    main_pid = getpid();
    pgsize = getpagesize();

    /* store glibc function */
    real_open = real_open ? real_open : dlsym(RTLD_NEXT, "open");
    real_close = real_close ? real_close : dlsym(RTLD_NEXT, "close");
    real_write = real_write ? real_write : dlsym(RTLD_NEXT, "write");
    real_read = real_read ? real_read : dlsym(RTLD_NEXT, "read");
    real_writev = real_writev ? real_writev : dlsym(RTLD_NEXT, "writev");
    real_shutdown = real_shutdown ? real_shutdown : dlsym(RTLD_NEXT, "shutdown");
    real_sendfile = real_sendfile ? real_sendfile : dlsym(RTLD_NEXT, "sendfile");
    real_sendmsg = real_sendmsg ? real_sendmsg : dlsym(RTLD_NEXT, "sendmsg");

    /* configuration */
    config = malloc(sizeof(esca_config_t));

    fp = fopen(DEFAULT_CONFIG_PATH, "r+");
    if (!fp) {
        printf("\033[0;31mCould not open configuration file: %s\n\033[0mUsing default configuration ...\n", DEFAULT_CONFIG_PATH);
        config = &default_config;
    } else {
        while (1) {
            config_option_t option;
            if (fscanf(fp, "%s = %d", option.key, &option.val) != 2) {
                if (feof(fp)) {
                    break;
                }
                printf("Invalid format in config file\n");
                continue;
            }
            if (strcmp(option.key, "esca_localize") == 0) {
                config->esca_localize = option.val;
            } else if (strcmp(option.key, "max_table_entry") == 0) {
                config->max_table_entry = option.val;
            } else if (strcmp(option.key, "max_table_len") == 0) {
                config->max_table_len = option.val;
            } else if (strcmp(option.key, "max_usr_worker") == 0) {
                config->max_usr_worker = option.val;
            } else if (strcmp(option.key, "max_ker_worker") == 0) {
                config->max_ker_worker = option.val;
            } else if (strcmp(option.key, "default_idle_time") == 0) {
                config->default_idle_time = option.val;
            } else if (strcmp(option.key, "affinity_offset") == 0) {
                config->affinity_offset = option.val;
            } else {
                printf("Invalid option: %s\n", option.key);
            }
        }
    }
    init_config(config);
    syscall(__NR_esca_config, config);
    main_thread_pid = getpid();

    for(int i = 0; i < MAX_THREAD_NUM; i++){
        mpool[i] = NULL;
        iovpool[i] = NULL;
        msgpool[i] = NULL;
    }

    /* pin header */
    table = (esca_table_t*)aligned_alloc(pgsize, pgsize);
    syscall(__NR_esca_register, table, NULL, config->max_ker_worker, 0);
}
