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
#include <generated/asm-offsets.h> /* __NR_syscall_max */
#include <linux/kallsyms.h> /* kallsyms_lookup_name, __NR_* */
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/uaccess.h> /* copy_from_user put_user */
#include <linux/smp.h>
#include <linux/wait.h>

#include "include/symbol.h"
#include "include/util.h"
#include "syscall.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven Cheng");
MODULE_DESCRIPTION("Linux IO offloading");
MODULE_VERSION("0.1");

/* restore original syscall for recover */
void *syscall_register_ori;
void *syscall_exit_ori;
void *syscall_wait_ori;

typedef asmlinkage long (*sys_call_ptr_t)(long);

struct page *pinned_pages[MAX_THREAD_NUM];

struct batch_entry *batch_table[MAX_THREAD_NUM];
int table_size = 64;
int start_index[MAX_THREAD_NUM];
int main_pid; /* PID of main thread */

static int worker(void *arg);
//struct task_struct *(*create_io_thread_ptr)(int (*)(void *), void *, int) = 0;


typedef asmlinkage long (*F0_t)(void);
typedef asmlinkage long (*F1_t)(long);
typedef asmlinkage long (*F2_t)(long, long);
typedef asmlinkage long (*F3_t)(long, long, long);
typedef asmlinkage long (*F4_t)(long, long, long, long);
typedef asmlinkage long (*F5_t)(long, long, long, long, long);
typedef asmlinkage long (*F6_t)(long, long, long, long, long, long);

static struct task_struct *worker_task;
static struct task_struct *worker_task2;
static struct task_struct *worker_task3;
static struct task_struct *worker_task4;

static DECLARE_WAIT_QUEUE_HEAD(wq);

//void (*wake_up_new_task_ptr)(struct task_struct *) = 0;

static inline long
indirect_call(void *f, int argc,
              long *a) { /* x64 syscall calling convention changed @4.17 to use
                            struct pt_regs */
    struct pt_regs regs;
    memset(&regs, 0, sizeof regs);
    switch (argc) {
    case 6:
        regs.r9 = a[5];
    case 5:
        regs.r8 = a[4];
    case 4:
        regs.r10 = a[3];
    case 3:
        regs.dx = a[2];
    case 2:
        regs.si = a[1];
    case 1:
        regs.di = a[0];
    }
    return ((F1_t)f)((long)&regs);
}



int fd;
char *buf;
static int worker(void *arg) {
    allow_signal(SIGKILL);
    int wpid = current->pid, cur_cpuid = current->pid - main_pid;
    set_cpus_allowed_ptr(current, cpumask_of(cur_cpuid - 1));
    
    batch_table[0][1].sysnum = 0; // tmp use -> record # of done req.
    printk("im in worker, pid = %d, bound at cpu %d\n", current->pid, smp_processor_id());
    
    while (1) {
	    int gi = start_index[cur_cpuid];
	    int gj = cur_cpuid;
#if 0
printk(KERN_INFO "In kt, Start flushing at cpu%d, started from  [%d][%d]\n", smp_processor_id(), gj, gi);
#endif
		while (batch_table[gj][gi].rstatus == BENTRY_BUSY) {

#if 0
printk(KERN_INFO "Index %d do syscall %d (%d %d) at cpu%d\n", gi,
		   batch_table[gj][gi].sysnum, gj, gi, smp_processor_id());
#endif
			batch_table[gj][gi].sysret =
				indirect_call(syscall_table_ptr[batch_table[gj][gi].sysnum],
				              batch_table[gj][gi].nargs, batch_table[gj][gi].args);
			batch_table[gj][gi].rstatus = BENTRY_EMPTY;
			if(gi == MAX_ENTRY_NUM - 1){
				if(gj == MAX_THREAD_NUM -1){
				    gj = 1;
				}else
				{
				    gj++;
				}
				gi = 1;
			}else
			{
				gi++;
			}
			batch_table[cur_cpuid - 1][1].sysnum -= 1;
			//printk("[%d] num=%d\n", cur_cpuid - 1, batch_table[cur_cpuid - 1][1].sysnum);
			if(batch_table[cur_cpuid - 1][1].sysnum == 0){
				//printk("wake from worker\n");
				wake_up_interruptible(&wq);
			}
			//printk("num = %d\n", ttt);
		}
    	start_index[cur_cpuid] = gi;
        if (signal_pending(current)){
            printk("detect signal\n");
            break;
        }
        cond_resched();
    }
    printk("kernel thread exit\n");
    do_exit(0);
    return 0;
}

/* after linux kernel 4.7, parameter was restricted into pt_regs type */
asmlinkage long sys_lioo_register(const struct __user pt_regs *regs) {
    printk(KERN_INFO "Start register, address at regs is %p\n", regs);
    int n_page, i, j;
    unsigned long p1 = regs->di;

    /* map batch table from user-space to kernel */
    n_page = get_user_pages(
        (unsigned long)(p1), /* Start address to map */
        MAX_THREAD_NUM, /* Number of pinned pages. 4096 btyes in this machine */
        FOLL_FORCE | FOLL_WRITE, /* Force flag */
        pinned_pages,            /* struct page ** pointer to pinned pages */
        NULL);

    for (i = 0; i < MAX_THREAD_NUM; i++)
        batch_table[i] = (struct batch_entry *)kmap(pinned_pages[i]);

    /* initial table status */
    for (j = 0; j < MAX_THREAD_NUM; j++)
        for (i = 0; i < MAX_ENTRY_NUM; i++)
            batch_table[j][i].rstatus = BENTRY_EMPTY;

    for (i = 0; i < MAX_THREAD_NUM; i++)
        start_index[i] = 1;

    main_pid = current->pid;
    printk("Main pid = %d\n", main_pid);


    worker_task = create_io_thread_ptr(worker, 0, -1);
    //worker_task2 = create_io_thread_ptr(worker, 0, -1);
    //worker_task3 = create_io_thread_ptr(worker, 0, -1);
    //worker_task4 = create_io_thread_ptr(worker, 0, -1);
    wake_up_new_task_ptr(worker_task);
    //wake_up_new_task_ptr(worker_task2);
    //wake_up_new_task_ptr(worker_task3);
    //wake_up_new_task_ptr(worker_task4);
    return 0;
}

asmlinkage void sys_lioo_exit(void) {
    printk(KERN_INFO "syscall exit\n");
    if (worker_task)
        kthread_stop(worker_task);
    worker_task = NULL;
}

asmlinkage void sys_lioo_wait(void) {
	//printk("in sleep\n");
	wait_event_interruptible(wq, batch_table[0][1].sysnum == 0);
	//printk("awake\n");
	//cond_resched();
}

static int __init lioo_init(void) {

    init_not_exported_symbol();

    /* allow write */
    allow_writes();
    /* backup */
    syscall_register_ori = (void *)syscall_table_ptr[__NR_lioo_register];
    syscall_exit_ori = (void *)syscall_table_ptr[__NR_lioo_exit];
    syscall_wait_ori = (void *)syscall_table_ptr[__NR_lioo_wait];

    /* hooking */
    syscall_table_ptr[__NR_lioo_register] = (void *)sys_lioo_register;
    syscall_table_ptr[__NR_lioo_exit] = (void *)sys_lioo_exit;
    syscall_table_ptr[__NR_lioo_wait] = (void *)sys_lioo_wait;

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
    syscall_table_ptr[__NR_lioo_wait] = (void *)syscall_wait_ori;
    disallow_writes();

    // if(worker_task)
    //  kthread_stop(worker_task);

    printk("lioo exit\n");
}
module_init(lioo_init);
module_exit(lioo_exit);
