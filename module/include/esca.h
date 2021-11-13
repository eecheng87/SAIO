#ifndef _ESCA_H
#define _ESCA_H
#include <stdatomic.h>

#define MAX_TABLE_ENTRY 64
#define MAX_TABLE_LEN 10
#define MAX_CPU_NUM 2

#define esca_smp_store_release(p, v) \
	atomic_store_explicit((_Atomic __typeof__(*(p)) *)(p), (v), \
			      memory_order_release)

typedef struct esca_table_entry {
    unsigned pid;
    short nargs;
    short rstatus;
    unsigned sysnum;
    unsigned sysret;
    long args[6];
} esca_table_entry_t;

typedef struct esca_table {
    esca_table_entry_t* tables[MAX_TABLE_LEN];
    short head_table; // entry for consumer
    short tail_table; // entry for producer
    short head_entry;
    short tail_entry;
} esca_table_t;

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
