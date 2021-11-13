#include <generated/asm-offsets.h> /* __NR_syscall_max */
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kallsyms.h> /* kallsyms_lookup_name, __NR_* */
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/uaccess.h> /* copy_from_user put_user */
#include <linux/version.h>
#include <linux/wait.h>

#include "include/esca.h"
#include "include/symbol.h"
#include "include/util.h"
#include "syscall.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven Cheng");
MODULE_DESCRIPTION("Linux IO offloading");
MODULE_VERSION("0.1");

/* restore original syscall for recover */
void* syscall_register_ori;
void* syscall_exit_ori;
void* syscall_wait_ori;

typedef asmlinkage long (*sys_call_ptr_t)(long);

int main_pid; /* PID of main thread */

/* declare shared table */
struct page* table_pinned_pages[MAX_CPU_NUM][MAX_TABLE_LEN];
struct page* shared_info_pinned_pages[1];
esca_table_t* table;
short submitted[MAX_CPU_NUM];

static int worker(void* arg);
//struct task_struct *(*create_io_thread_ptr)(int (*)(void *), void *, int) = 0;

typedef asmlinkage long (*F0_t)(void);
typedef asmlinkage long (*F1_t)(long);
typedef asmlinkage long (*F2_t)(long, long);
typedef asmlinkage long (*F3_t)(long, long, long);
typedef asmlinkage long (*F4_t)(long, long, long, long);
typedef asmlinkage long (*F5_t)(long, long, long, long, long);
typedef asmlinkage long (*F6_t)(long, long, long, long, long, long);

static struct task_struct* worker_task;
static struct task_struct* worker_task2;
static struct task_struct* worker_task3;
static struct task_struct* worker_task4;

static DECLARE_WAIT_QUEUE_HEAD(wq);

//void (*wake_up_new_task_ptr)(struct task_struct *) = 0;

static inline long
indirect_call(void* f, int argc,
    long* a)
{ /* x64 syscall calling convention changed @4.17 to use
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
char* buf;
static int worker(void* arg)
{
    allow_signal(SIGKILL);
    int wpid = current->pid, cur_cpuid = current->pid - main_pid - 1;
    set_cpus_allowed_ptr(current, cpumask_of(cur_cpuid));

    printk("im in worker, pid = %d, bound at cpu %d, cur_cpupid = %d\n", current->pid, smp_processor_id(), cur_cpuid);

    while (1) {
        int i = table[cur_cpuid].head_table;
        int j = table[cur_cpuid].head_entry;
        int head_index, tail_index;

        while (smp_load_acquire(&table[cur_cpuid].tables[i][j].rstatus) == BENTRY_EMPTY) {
            if (signal_pending(current)) {
                printk("detect signal\n");
                goto exit_worker;
            }
            cond_resched();
        }

        head_index = (i * MAX_TABLE_ENTRY) + j;
        tail_index = (table[cur_cpuid].tail_table * 64) + table[cur_cpuid].tail_entry;
        submitted[cur_cpuid] = (tail_index >= head_index) ? tail_index - head_index : MAX_TABLE_ENTRY * MAX_TABLE_LEN - head_index + tail_index;

#if 0
		if(submitted[cur_cpuid] != 0)
			printk("submmitted[%d] = %hi\n", cur_cpuid, submitted[cur_cpuid]);
#endif

        while (submitted[cur_cpuid] != 0) {
            //printk("tail_index = %d, head_index = %d, tail_table = %d, tail_entry = %d\nBefore: submmitted[%d] = %hi\n", tail_index, head_index, table[cur_cpuid].tail_table, table[cur_cpuid].tail_entry, cur_cpuid, submitted[cur_cpuid]);

            table[cur_cpuid].tables[i][j].sysret = indirect_call(syscall_table_ptr[table[cur_cpuid].tables[i][j].sysnum],
                table[cur_cpuid].tables[i][j].nargs, table[cur_cpuid].tables[i][j].args);
            table[cur_cpuid].tables[i][j].rstatus = BENTRY_EMPTY;

#if 0
            printk(KERN_INFO "Index %d,%d do syscall %d : %d = (%d, %d, %ld, %d) at cpu%d\n", i, j,
                table[cur_cpuid].tables[i][j].sysnum, table[cur_cpuid].tables[i][j].sysret, table[cur_cpuid].tables[i][j].args[0],
                table[cur_cpuid].tables[i][j].args[1], table[cur_cpuid].tables[i][j].args[2],
                table[cur_cpuid].tables[i][j].args[3], smp_processor_id());
#endif

            if (j == MAX_TABLE_ENTRY - 1) {
                i = (i == MAX_TABLE_LEN - 1) ? 0 : i + 1;
                j = 0;
            } else {
                j++;
            }

            submitted[cur_cpuid]--;

            short done = 1;
            for (int k = 0; k < MAX_CPU_NUM; k++) {
                if (submitted[k] != 0) {
                    done = 0;
                    break;
                }
            }

            if (done == 1) {
                // TODO: make sure this only be executed one time
                wake_up_interruptible(&wq);
            }
        }
        table[cur_cpuid].head_table = i;
        table[cur_cpuid].head_entry = j;

        if (signal_pending(current)) {
            printk("detect signal\n");
            goto exit_worker;
        }
        cond_resched();
    }
exit_worker:
    printk("kernel thread exit\n");
    do_exit(0);
    return 0;
}

/* after linux kernel 4.7, parameter was restricted into pt_regs type */
asmlinkage long sys_lioo_register(const struct __user pt_regs* regs)
{
    int n_page, i, j, k;

    // TODO: make it more flexible
    unsigned long p1[MAX_CPU_NUM + 1] = { regs->si, regs->dx, regs->di };

    /* map batch table from user-space to kernel */
    for (i = 0; i < MAX_CPU_NUM; i++) {
        n_page = get_user_pages(
            (unsigned long)(p1[i]),
            MAX_TABLE_LEN,
            FOLL_FORCE | FOLL_WRITE,
            table_pinned_pages[i],
            NULL);
    }

    n_page = get_user_pages(
        (unsigned long)(p1[2]),
        1,
        FOLL_FORCE | FOLL_WRITE,
        shared_info_pinned_pages,
        NULL);

    table = (esca_table_t*)kmap(shared_info_pinned_pages[0]);
    printk("table=%p\n", table);

    for (i = 0; i < MAX_CPU_NUM; i++)
        for (j = 0; j < MAX_TABLE_LEN; j++) {
            table[i].tables[j] = (esca_table_entry_t*)kmap(table_pinned_pages[i][j]);
            printk("table[%d][%d]=%p\n", i, j, table[i].tables[j]);
        }

    /* initial entry status */
    for (i = 0; i < MAX_CPU_NUM; i++)
        for (j = 1; j < MAX_TABLE_LEN; j++)
            for (k = 0; k < MAX_TABLE_ENTRY; k++)
                table[i].tables[j][k].rstatus = BENTRY_EMPTY;

    for (i = 0; i < MAX_CPU_NUM; i++) {
        table[i].head_table = table[i].tail_table = 0;
        table[i].head_entry = table[i].tail_entry = 0;
    }

    for (i = 0; i < MAX_CPU_NUM; i++) {
        submitted[i] = 0;
    }

    main_pid = current->pid;
    printk("Main pid = %d\n", main_pid);

    worker_task = create_io_thread_ptr(worker, 0, -1);
    worker_task2 = create_io_thread_ptr(worker, 0, -1);
    //worker_task3 = create_io_thread_ptr(worker, 0, -1);
    //worker_task4 = create_io_thread_ptr(worker, 0, -1);
    wake_up_new_task_ptr(worker_task);
    wake_up_new_task_ptr(worker_task2);
    //wake_up_new_task_ptr(worker_task3);
    //wake_up_new_task_ptr(worker_task4);
    return 0;
}

asmlinkage void sys_lioo_exit(void)
{
    printk(KERN_INFO "syscall exit\n");
    if (worker_task)
        kthread_stop(worker_task);
    worker_task = NULL;
}

asmlinkage void sys_lioo_wait(void)
{
    //printk("in sleep\n");
    // TODO: make it more flexible
    wait_event_interruptible(wq, (submitted[0] == 0) && (submitted[1] == 0));

    //while(batch_table[0][1].sysnum != 0){
    //	cond_resched();
    //}
    //printk("awake\n");
    //cond_resched();
}

static int __init lioo_init(void)
{

    init_not_exported_symbol();

    /* allow write */
    allow_writes();
    /* backup */
    syscall_register_ori = (void*)syscall_table_ptr[__NR_lioo_register];
    syscall_exit_ori = (void*)syscall_table_ptr[__NR_lioo_exit];
    syscall_wait_ori = (void*)syscall_table_ptr[__NR_lioo_wait];

    /* hooking */
    syscall_table_ptr[__NR_lioo_register] = (void*)sys_lioo_register;
    syscall_table_ptr[__NR_lioo_exit] = (void*)sys_lioo_exit;
    syscall_table_ptr[__NR_lioo_wait] = (void*)sys_lioo_wait;

    /* dis-allow write */
    disallow_writes();

    printk("lioo init\n");
    return 0;
}
static void __exit lioo_exit(void)
{
    /* recover */
    allow_writes();
    syscall_table_ptr[__NR_lioo_register] = (void*)syscall_register_ori;
    syscall_table_ptr[__NR_lioo_exit] = (void*)syscall_exit_ori;
    syscall_table_ptr[__NR_lioo_wait] = (void*)syscall_wait_ori;
    disallow_writes();

    // if(worker_task)
    //  kthread_stop(worker_task);

    printk("lioo exit\n");
}
module_init(lioo_init);
module_exit(lioo_exit);
