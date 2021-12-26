#ifndef _SYMBOL_H
#define _SYMBOL_H

#include <linux/kernel.h>
#include <linux/module.h>

extern struct task_struct;
extern void** syscall_table_ptr;
extern int (*kernel_clone_ptr)(void*);
extern void (*wake_up_new_task_ptr)(struct task_struct*);
extern struct workqueue_struct* system_wq;
void init_not_exported_symbol(void);

#endif
