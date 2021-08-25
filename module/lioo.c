#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "include/symbol.h"
#include "include/util.h"
#include "syscall.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven Cheng");
MODULE_DESCRIPTION("Linux IO offloading");
MODULE_VERSION("0.1");

static struct task_struct *worker_task;

/* restore original syscall for recover */
void *syscall_register_ori;
void *syscall_exit_ori;

typedef asmlinkage long (*sys_call_ptr_t)(long);

int fd;
char *buf;
static int worker(void *arg) {
    allow_signal(SIGKILL);
    printk("im in worker!\n");
    while (1) {
        struct pt_regs reg;
        void *f = (void *)syscall_table_ptr[1];
        memset(&reg, 0, sizeof(reg));
        reg.di = fd;
        reg.si = (unsigned long)buf;
        reg.dx = 9;
        //((sys_call_ptr_t *)f)((long)&reg);
        printk("fd=%d buf=%lu\n", fd, (unsigned long)buf);
        printk("wn=%ld\n", ((long (*)(long))syscall_table_ptr[1])(&reg));
        ssleep(1);
        if (signal_pending(worker_task))
            break;
    }
    do_exit(0);
    return 0;
}

/* after linux kernel 4.7, parameter was restricted into pt_regs type */
asmlinkage long sys_lioo_register(const struct __user pt_regs *regs) {
    printk(KERN_INFO "syscall register\n");
    fd = regs->di;
    buf = regs->si;

    // worker_task = kthread_create(worker, (void*)777,"worker");
    worker_task = create_io_thread_ptr(worker, 0, -1);
    if (worker_task)
        printk("task created successfully\n");
    else
        printk("fail to create task\n");

    // wake_up_process(worker_task);
    wake_up_new_task_ptr(worker_task);

    return 0;
}

asmlinkage void sys_lioo_exit(void) {
    printk(KERN_INFO "syscall exit\n");
    if (worker_task)
        kthread_stop(worker_task);
    worker_task = NULL;
}

static int __init lioo_init(void) {

    init_not_exported_symbol();

    /* allow write */
    allow_writes();
    /* backup */
    syscall_register_ori = (void *)syscall_table_ptr[__NR_lioo_register];
    syscall_exit_ori = (void *)syscall_table_ptr[__NR_lioo_exit];

    /* hooking */
    syscall_table_ptr[__NR_lioo_register] = (void *)sys_lioo_register;
    syscall_table_ptr[__NR_lioo_exit] = (void *)sys_lioo_exit;
    /* dis-allow write */
    disallow_writes();

    printk("lioo init\n");
    return 0;
}
static void __exit lioo_exit(void) {
    /* recover */
    allow_writes();
    syscall_table_ptr[__NR_lioo_register] = (void *)syscall_register_ori;
    syscall_table_ptr[__NR_lioo_exit] = (void *)syscall_exit_ori;
    disallow_writes();

    // if(worker_task)
    //  kthread_stop(worker_task);

    printk("lioo exit\n");
}
module_init(lioo_init);
module_exit(lioo_exit);
