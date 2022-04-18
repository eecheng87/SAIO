#ifndef _ESCA_REDIS_H
#define _ESCA_REDIS_H

#include <stdatomic.h>

#define read_barrier()	__asm__ __volatile__("lfence":::"memory")
#define write_barrier()	__asm__ __volatile__("sfence":::"memory")

#define CPU_NUM_LIMIT 100
#define TABLE_LEN_LIMIT 10
#define TABLE_ENT_LIMIT 64

#define esca_smp_store_release(p, v)                           \
    atomic_store_explicit((_Atomic __typeof__(*(p))*)(p), (v), \
        memory_order_release)

#define esca_smp_load_acquire(p)                         \
    atomic_load_explicit((_Atomic __typeof__(*(p))*)(p), \
        memory_order_acquire)

typedef struct esca_table_entry {
    int len;
    short nargs;
    short rstatus;
    unsigned sysnum;
    unsigned sysret;
    long args[5]; // don't support syscall with six arguments
    unsigned long long user_data;
} esca_table_entry_t;

typedef struct esca_table {
    esca_table_entry_t* tables[TABLE_LEN_LIMIT]; // shared b/t kernel and user (in kernel address space)
    esca_table_entry_t* user_tables[TABLE_LEN_LIMIT]; // shared b/t kernel and user (in usr address space)
    short head_table; // entry for consumer
    short tail_table; // entry for producer
    short head_entry;
    short tail_entry;
    unsigned idle_time; // in jiffies
    unsigned int flags;
} esca_table_t;

typedef struct user_data {
    unsigned long long conn;
    int qlen;
} user_data_t;

#endif