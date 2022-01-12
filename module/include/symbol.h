#ifndef _SYMBOL_H
#define _SYMBOL_H

#include <linux/kernel.h>
#include <linux/module.h>

extern void** syscall_table_ptr;
extern void (*wake_up_new_task_ptr)(void*);
extern struct task_struct* (*create_io_thread_ptr)(int (*)(void*), void*, int);
extern struct workqueue_struct* system_wq;
void init_not_exported_symbol(void);

#endif
