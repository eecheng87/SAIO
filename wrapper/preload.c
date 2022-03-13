#include "preload.h"
#include <sys/mman.h>

#if defined(__aarch64__)
#include "../module/include/aarch64_syscall.h"
#elif defined(__x86_64__)
#include "../module/include/x86_syscall.h"
#endif

int in_segment;
int main_pid;
int batch_num; /* number of busy entry */
int syscall_num; /* number of syscall triggered currently */
size_t pgsize;

void* mpool; /* memory pool */
ull pool_offset;
struct iovec* iovpool; /* pool for iovector */
ull iov_offset;

/* Global configurable variable */
int ESCA_LOCALIZE;
int MAX_TABLE_ENTRY;
int MAX_TABLE_LEN;
int MAX_USR_WORKER;
int MAX_CPU_NUM;
int RATIO;
int DEFAULT_IDLE_TIME;

/* declare shared table, pin user space addr. to kernel phy. addr by kmap */
int this_worker_id;

/* user worker can't touch worker's table in diff. set */
esca_table_t* table[CPU_NUM_LIMIT];

void init_worker(int idx)
{
    in_segment = 0;
    batch_num = 0;
    syscall_num = 0;

    /* expect idx = 0 ~ MAX_USR_WORKER - 1 */
    this_worker_id = idx;

    printf("Create worker ID = %d, pid = %d\n", this_worker_id, getpid());

    if (idx >= MAX_USR_WORKER) {
        printf("[ERROR] Process exceed limit\n");
        goto init_worker_exit;
    }

    int set_index = 0;
    for (int i = idx * RATIO; i < idx * RATIO + RATIO; i++) {
        /* headers in same set using a same page */
        esca_table_t* header = NULL;
        if (i == idx * RATIO) {
            header = table[i] = (esca_table_t*)aligned_alloc(pgsize, pgsize);
        }
        table[i] = table[idx * RATIO] + (i - idx * RATIO);

        /* allocate tables */
        esca_table_entry_t* alloc = (esca_table_entry_t*)aligned_alloc(pgsize, pgsize * MAX_TABLE_LEN);
        if (!alloc) {
            printf("[ERROR] alloc failed\n");
            goto init_worker_exit;
        }

        /* pin tables to kernel */
        syscall(__NR_esca_register, header, alloc, i, set_index++);

        /* pin table from kernel to user */
        for (int j = 0; j < MAX_TABLE_LEN; j++) {
            table[i]->user_tables[j] = alloc + j * MAX_TABLE_ENTRY;
        }
    }

    mpool = (void*)malloc(sizeof(unsigned char) * MAX_POOL_SIZE);
    pool_offset = 0;
    iovpool = (struct iovec*)malloc(sizeof(struct iovec) * MAX_POOL_IOV_SIZE);
    iov_offset = 0;

init_worker_exit:
    return;
}

long batch_start()
{
    int i = this_worker_id;
    in_segment = 1;

    for (int j = i * RATIO; j < i * RATIO + RATIO; j++) {
        if (esca_unlikely(ESCA_READ_ONCE(table[j]->flags) & ESCA_WORKER_NEED_WAKEUP)) {
            table[j]->flags |= ESCA_START_WAKEUP;
            syscall(__NR_esca_wakeup, j);
            table[j]->flags &= ~ESCA_START_WAKEUP;
        }
    }

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

void update_index(int idx)
{
    // avoid overwriting;
    // FIXME: need to consider more -> cross table scenario
    // FIXME: order of the head might be protected by barrier
    while ((table[idx]->tail_entry + 1 == table[idx]->head_entry) && (table[idx]->tail_table == table[idx]->head_table))
        ;

    if (table[idx]->tail_entry == MAX_TABLE_ENTRY - 1) {
        table[idx]->tail_entry = 0;
        table[idx]->tail_table = (table[idx]->tail_table == MAX_TABLE_LEN - 1) ? 0 : table[idx]->tail_table + 1;
    } else {
        table[idx]->tail_entry++;
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

    printf("\033[0;33m");
    printf(" Localize: \033[0;37m%s\033[0;33m\n", ESCA_LOCALIZE ? "Enable" : "Disable");
    printf(" MAX_TABLE_ENTRY: \033[0;37m%d\033[0;33m\n", MAX_TABLE_ENTRY);
    printf(" MAX_TABLE_LEN: \033[0;37m%d\033[0;33m\n", MAX_TABLE_LEN);
    printf(" MAX_USR_WORKER: \033[0;37m%d\033[0;33m\n", MAX_USR_WORKER);
    printf(" MAX_KER_WORKER: \033[0;37m%d\033[0;33m\n", MAX_CPU_NUM);

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
            } else {
                printf("Invalid option: %s\n", option.key);
            }
        }
    }
    init_config(config);
    syscall(__NR_esca_config, config);
}