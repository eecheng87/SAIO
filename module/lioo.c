#include <generated/asm-offsets.h> /* __NR_syscall_max */
#include <linux/anon_inodes.h>
#include <linux/blkdev.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kallsyms.h> /* kallsyms_lookup_name, __NR_* */
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/uaccess.h> /* copy_from_user put_user */
#include <linux/version.h>
#include <linux/wait.h>

#include "include/esca.h"
#include "include/symbol.h"
#include "include/systab.h"
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
//esca_table_t* local_table[MAX_CPU_NUM];
short submitted[MAX_CPU_NUM];

static int worker(void* arg);

// TODO: encapsulate them
wait_queue_head_t worker_wait[MAX_CPU_NUM];
wait_queue_head_t wq[MAX_CPU_NUM];
// int flags[MAX_CPU_NUM]; // need_wake_up; this might be shared with user space (be aware of memory barrier)

typedef asmlinkage long (*F0_t)(void);
typedef asmlinkage long (*F1_t)(long);
typedef asmlinkage long (*F2_t)(long, long);
typedef asmlinkage long (*F3_t)(long, long, long);
typedef asmlinkage long (*F4_t)(long, long, long, long);
typedef asmlinkage long (*F5_t)(long, long, long, long, long);
typedef asmlinkage long (*F6_t)(long, long, long, long, long, long);

static struct task_struct* worker_task[MAX_CPU_NUM];

// void (*wake_up_new_task_ptr)(struct task_struct *) = 0;

static inline long
indirect_call(void* f, int argc,
    long* a)
{
    struct pt_regs regs;
    memset(&regs, 0, sizeof regs);
    switch (argc) {
#if defined(__x86_64__)
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
#elif defined(__aarch64__)
    case 6:
        regs.regs[5] = a[5];
    case 5:
        regs.regs[4] = a[4];
    case 4:
        regs.regs[3] = a[3];
    case 3:
        regs.regs[2] = a[2];
    case 2:
        regs.regs[1] = a[1];
    case 1:
        regs.regs[0] = a[0];
#endif
    }
    return ((F1_t)f)((long)&regs);
}

#if defined(__aarch64__)

static struct mm_struct* init_mm_ptr;
// From arch/arm64/mm/pageattr.c.
struct page_change_data {
    pgprot_t set_mask;
    pgprot_t clear_mask;
};

// From arch/arm64/mm/pageattr.c.
static int change_page_range(pte_t* ptep, unsigned long addr, void* data)
{
    struct page_change_data* cdata = data;
    pte_t pte = READ_ONCE(*ptep);

    pte = clear_pte_bit(pte, cdata->clear_mask);
    pte = set_pte_bit(pte, cdata->set_mask);

    set_pte(ptep, pte);
    return 0;
}

// From arch/arm64/mm/pageattr.c.
static int __change_memory_common(unsigned long start, unsigned long size,
    pgprot_t set_mask, pgprot_t clear_mask)
{
    struct page_change_data data;
    int ret;

    data.set_mask = set_mask;
    data.clear_mask = clear_mask;

    ret = apply_to_page_range(init_mm_ptr, start, size, change_page_range, &data);

    flush_tlb_kernel_range(start, start + size);
    return ret;
}

// Simplified set_memory_rw() from arch/arm64/mm/pageattr.c.
static int set_page_rw(unsigned long addr)
{
    vm_unmap_aliases();
    return __change_memory_common(addr, PAGE_SIZE, __pgprot(PTE_WRITE), __pgprot(PTE_RDONLY));
}

// Simplified set_memory_ro() from arch/arm64/mm/pageattr.c.
static int set_page_ro(unsigned long addr)
{
    vm_unmap_aliases();
    return __change_memory_common(addr, PAGE_SIZE, __pgprot(PTE_RDONLY), __pgprot(PTE_WRITE));
}

void allow_writes(void)
{
    set_page_rw((unsigned long)(syscall_table_ptr + __NR_esca_register) & PAGE_MASK);
}
void disallow_writes(void)
{
    set_page_ro((unsigned long)(syscall_table_ptr + __NR_esca_register) & PAGE_MASK);
}
static void enable_cycle_counter_el0(void* data)
{
    u64 val;
    /* Disable cycle counter overflow interrupt */
    asm volatile("msr pmintenset_el1, %0"
                 :
                 : "r"((u64)(0 << 31)));
    /* Enable cycle counter */
    asm volatile("msr pmcntenset_el0, %0" ::"r" BIT(31));
    /* Enable user-mode access to cycle counters. */
    asm volatile("msr pmuserenr_el0, %0"
                 :
                 : "r"(BIT(0) | BIT(2)));
    /* Clear cycle counter and start */
    asm volatile("mrs %0, pmcr_el0"
                 : "=r"(val));
    val |= (BIT(0) | BIT(2));
    isb();
    asm volatile("msr pmcr_el0, %0"
                 :
                 : "r"(val));
    val = BIT(27);
    asm volatile("msr pmccfiltr_el0, %0"
                 :
                 : "r"(val));
}

static void disable_cycle_counter_el0(void* data)
{
    /* Disable cycle counter */
    asm volatile("msr pmcntenset_el0, %0" ::"r"(0 << 31));
    /* Disable user-mode access to counters. */
    asm volatile("msr pmuserenr_el0, %0"
                 :
                 : "r"((u64)0));
}
#endif

char* buf;

static int worker(void* arg)
{
    allow_signal(SIGKILL);
    int cur_cpuid = ((esca_wkr_args_t*)arg)->id;
    set_cpus_allowed_ptr(current, cpumask_of(cur_cpuid));
    unsigned long timeout = 0;

    init_waitqueue_head(&wq[cur_cpuid]);

    DEFINE_WAIT(wait);

    printk("im in worker, pid = %d, bound at cpu %d, cur_cpupid = %d\n",
        current->pid, smp_processor_id(), cur_cpuid);

    while (1) {
        int i = table[cur_cpuid].head_table;
        int j = table[cur_cpuid].head_entry;
        int head_index, tail_index;

        while (smp_load_acquire(&table[cur_cpuid].tables[i][j].rstatus) == BENTRY_EMPTY) {
            if (signal_pending(current)) {
                printk("detect signal\n");
                goto exit_worker;
            }
            // FIXME:
            if (!time_after(jiffies, timeout)) {
                // still don't need to sleep
                cond_resched();
                continue;
            }

            prepare_to_wait(&worker_wait[cur_cpuid], &wait, TASK_INTERRUPTIBLE);
            WRITE_ONCE(table[cur_cpuid].flags, table[cur_cpuid].flags | ESCA_WORKER_NEED_WAKEUP);

            if (smp_load_acquire(&table[cur_cpuid].tables[i][j].rstatus) == BENTRY_EMPTY) {
                schedule();
                // wake up by `wake_up` in batch_start
                finish_wait(&worker_wait[cur_cpuid], &wait);

                // clear need_wakeup
                // FIXME: // need write barrier?
                WRITE_ONCE(table[cur_cpuid].flags, table[cur_cpuid].flags & ~ESCA_WORKER_NEED_WAKEUP);
                continue;
            }

            // condition satisfied, don't schedule
            finish_wait(&worker_wait[cur_cpuid], &wait);
            WRITE_ONCE(table[cur_cpuid].flags, table[cur_cpuid].flags & ~ESCA_WORKER_NEED_WAKEUP);
        }

        head_index = (i * MAX_TABLE_ENTRY) + j;
        tail_index = (table[cur_cpuid].tail_table * MAX_TABLE_ENTRY) + table[cur_cpuid].tail_entry;
        submitted[cur_cpuid] = (tail_index >= head_index)
            ? tail_index - head_index
            : MAX_TABLE_ENTRY * MAX_TABLE_LEN - head_index + tail_index;

        while (submitted[cur_cpuid] != 0) {
            table[cur_cpuid].tables[i][j].sysret = indirect_call(
                syscall_table_ptr[table[cur_cpuid].tables[i][j].sysnum],
                table[cur_cpuid].tables[i][j].nargs,
                table[cur_cpuid].tables[i][j].args);

            // FIXME: need barrier?
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
#if 0
            short done = 1;
            for (int k = 0; k < MAX_CPU_NUM; k++) {
                if (submitted[k] != 0) {
                    done = 0;
                    break;
                }
            }
#endif
            if (submitted[cur_cpuid] == 0) {
                // TODO: make sure this only be executed one time
                wake_up_interruptible(&wq[cur_cpuid]);
            }

            timeout = jiffies + table[cur_cpuid].idle_time;
        }
        table[cur_cpuid].head_table = i;
        table[cur_cpuid].head_entry = j;

        if (signal_pending(current)) {
            printk("detect signal\n");
            goto exit_worker;
        }
    }
exit_worker:
    printk("kernel thread exit\n");
    do_exit(0);
    return 0;
}

static int esca_mmap(struct file* file, struct vm_area_struct* vma)
{
    loff_t offset = (loff_t)vma->vm_pgoff << PAGE_SHIFT;
    unsigned long sz = vma->vm_end - vma->vm_start;
    esca_table_t* table = file->private_data;
    unsigned long pfn;
    struct page* page;
    void* ptr = table;

    page = virt_to_head_page(ptr);
    if (sz > (PAGE_SIZE << compound_order(page)))
        return -EINVAL;

    pfn = virt_to_phys(ptr) >> PAGE_SHIFT;
    return remap_pfn_range(vma, vma->vm_start, pfn, sz, vma->vm_page_prot);
}

static const struct file_operations esca_fops = {
    .mmap = esca_mmap
};

/* after linux kernel 4.7, parameter was restricted into pt_regs type */
asmlinkage long sys_esca_register(const struct __user pt_regs* regs)
{
    // regs should contain: table, alloc_head[id], id
#if defined(__x86_64__)
    unsigned long p1[3] = { regs->di, regs->si, regs->dx };
#elif defined(__aarch64__)
    unsigned long p1[3] = { regs->regs[0], regs->regs[1], regs->regs[2] };
#endif

    // FIXME: check if p1[0] is needed
    struct file* file;
    int n_page, id = p1[2], fd;
    esca_wkr_args_t* args;

    args = (esca_wkr_args_t*)kmalloc(sizeof(esca_wkr_args_t), GFP_KERNEL);
    args->id = id;

    if (p1[0] == 0 && p1[1] == 0) {
        /* allocate pages for table */
        gfp_t gfp_flags = GFP_KERNEL | __GFP_ZERO | __GFP_NOWARN | __GFP_COMP | __GFP_NORETRY;
        /* in virtual address space */
        table = (esca_table_t*)__get_free_pages(gfp_flags, get_order(PAGE_SIZE));

        /* create anonymous fd and be visible backing of an esca instance */
        fd = get_unused_fd_flags(O_RDWR | O_CLOEXEC);
        file = anon_inode_getfile("[ESCA]", &esca_fops, table, O_RDWR | O_CLOEXEC);
        fd_install(fd, file);

        return fd;
    }

    /* map batch table from user-space to kernel */
    n_page = get_user_pages((unsigned long)(p1[1]), MAX_TABLE_LEN,
        FOLL_FORCE | FOLL_WRITE, table_pinned_pages[id],
        NULL);
    printk("Pin %d pages in worker %d\n", n_page, id);

    for (int j = 0; j < MAX_TABLE_LEN; j++) {
        table[id].tables[j] = (esca_table_entry_t*)kmap(table_pinned_pages[id][j]);
        printk("table[%d][%d]=%p\n", id, j, table[id].tables[j]);
    }

    /* initial entry status */
    for (int j = 0; j < MAX_TABLE_LEN; j++)
        for (int k = 0; k < MAX_TABLE_ENTRY; k++)
            table[id].tables[j][k].rstatus = BENTRY_EMPTY;

    // TODO: merge them
    table[id].head_table = table[id].tail_table = 0;
    table[id].head_entry = table[id].tail_entry = 0;

    // TODO: merge them
    submitted[id] = 0;
    table[id].flags = 0;
    table[id].idle_time = msecs_to_jiffies(DEFAULT_IDLE_TIME);
    init_waitqueue_head(&worker_wait[id]);

    // closure is important
    worker_task[id] = create_io_thread_ptr(worker, args, -1);
    wake_up_new_task_ptr(worker_task[id]);

    return 0;
}

asmlinkage void sys_lioo_exit(void)
{
    printk(KERN_INFO "syscall exit\n");
    for (int i = 0; i < MAX_CPU_NUM; i++) {
        if (worker_task[i])
            kthread_stop(worker_task[i]);
        worker_task[i] = NULL;
    }
}

asmlinkage void sys_esca_wakeup(const struct __user pt_regs* regs)
{
#if defined(__x86_64__)
    int i = regs->di;
#elif defined(__aarch64__)
    int i = regs->regs[0];
#endif

    if (likely(READ_ONCE(table[i].flags) & ESCA_START_WAKEUP)) {
        wake_up(&worker_wait[i]);
    }
}

asmlinkage void sys_esca_wait(const struct __user pt_regs* regs)
{
#if defined(__x86_64__)
    int idx = regs->di;
#elif defined(__aarch64__)
    int idx = regs->regs[0];
#endif
    wait_event_interruptible(wq[idx], 1);
}

static int __init lioo_init(void)
{

    init_not_exported_symbol();

#if defined(__aarch64__)
    init_mm_ptr = (struct mm_struct*)(sysMM + ((char*)&system_wq - sysWQ));
    on_each_cpu(enable_cycle_counter_el0, NULL, 1);
#endif

    /* allow write */
    allow_writes();
    /* backup */
    syscall_register_ori = (void*)syscall_table_ptr[__NR_esca_register];
    syscall_exit_ori = (void*)syscall_table_ptr[__NR_esca_wakeup];
    syscall_wait_ori = (void*)syscall_table_ptr[__NR_esca_wait];

    /* hooking */
    syscall_table_ptr[__NR_esca_register] = (void*)sys_esca_register;
    syscall_table_ptr[__NR_esca_wakeup] = (void*)sys_esca_wakeup;
    syscall_table_ptr[__NR_esca_wait] = (void*)sys_esca_wait;

    /* dis-allow write */
    disallow_writes();

    printk("lioo init\n");
    return 0;
}
static void __exit lioo_exit(void)
{
    /* recover */
    allow_writes();
    syscall_table_ptr[__NR_esca_register] = (void*)syscall_register_ori;
    syscall_table_ptr[__NR_esca_wakeup] = (void*)syscall_exit_ori;
    syscall_table_ptr[__NR_esca_wait] = (void*)syscall_wait_ori;
    disallow_writes();

#if defined(__aarch64__)
    init_mm_ptr = (struct mm_struct*)(sysMM + ((char*)&system_wq - sysWQ));
    on_each_cpu(disable_cycle_counter_el0, NULL, 1);
#endif

    // if(worker_task)
    //  kthread_stop(worker_task);

    printk("lioo exit\n");
}
module_init(lioo_init);
module_exit(lioo_exit);
