#ifndef _ESCA_H
#define _ESCA_H

/*
* THIS FILE CAN"T CONTAIN KERNEL DATA STRUCTURE,
* BECAUSE IT'S SHARED WITH USER LAND
*/

#include <stdatomic.h>

#define MAX_TABLE_ENTRY 64
#define MAX_TABLE_LEN 1
#define MAX_USR_WORKER 2
#define MAX_CPU_NUM 4
#define RATIO (MAX_CPU_NUM / MAX_USR_WORKER)
#define DEFAULT_IDLE_TIME 1500 /* in msec */

#define ESCA_WRITE_ONCE(var, val)                           \
    atomic_store_explicit((_Atomic __typeof__(var)*)&(var), \
        (val), memory_order_relaxed)
#define ESCA_READ_ONCE(var)                                \
    atomic_load_explicit((_Atomic __typeof__(var)*)&(var), \
        memory_order_relaxed)

#define esca_smp_store_release(p, v)                           \
    atomic_store_explicit((_Atomic __typeof__(*(p))*)(p), (v), \
        memory_order_release)

#define esca_smp_load_acquire(p)                         \
    atomic_load_explicit((_Atomic __typeof__(*(p))*)(p), \
        memory_order_acquire)

#ifndef esca_unlikely
#define esca_unlikely(cond) __builtin_expect(!!(cond), 0)
#endif

#ifndef esca_likely
#define esca_likely(cond) __builtin_expect(!!(cond), 1)
#endif

/* define flags */
#define ESCA_WORKER_NEED_WAKEUP (1U << 1)
#define ESCA_START_WAKEUP (1U << 2)

typedef struct esca_table_entry {
    unsigned pid;
    short nargs;
    short rstatus;
    unsigned sysnum;
    unsigned sysret;
    long args[6];
} esca_table_entry_t;

typedef struct esca_table {
    esca_table_entry_t* tables[MAX_TABLE_LEN]; // shared b/t kernel and user (in kernel address space)
    esca_table_entry_t* user_tables[MAX_TABLE_LEN]; // shared b/t kernel and user (in usr address space)
    short head_table; // entry for consumer
    short tail_table; // entry for producer
    short head_entry;
    short tail_entry;
    unsigned idle_time; // in jiffies
    unsigned int flags;
} esca_table_t;

typedef struct esca_wkr_args {
    int id;
} esca_wkr_args_t;

/* store in first entry of each esca_table */

#if 0
typedef struct esca_info {

} esca_info_t;

typedef struct esca_meta {
    esca_table_t table[MAX_CPU_NUM];
    esca_info_t info[MAX_CPU_NUM];
} esca_meta_t;
#endif

#endif
