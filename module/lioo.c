#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/version.h>

#include "systab.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven Cheng");
MODULE_DESCRIPTION("Linux IO offloading");
MODULE_VERSION("0.1");

static void **syscall_table = 0;

/* restore original syscall for recover */
void *syscall_register_ori;
void *syscall_exit_ori;

typedef long (*sys_call_ptr_t)(const struct __user pt_regs *);

extern unsigned long __force_order __weak;
#define store_cr0(x) asm volatile("mov %0,%%cr0" : "+r"(x), "+m"(__force_order))
static void allow_writes(void) {
    unsigned long cr0 = read_cr0();
    clear_bit(16, &cr0);
    store_cr0(cr0);
}
static void disallow_writes(void) {
    unsigned long cr0 = read_cr0();
    set_bit(16, &cr0);
    store_cr0(cr0);
}

static int __init lioo_init(void) {

    /* hooking system call */

    /* avoid effect of KALSR, get address of syscall table by adding offset */
    syscall_table = (void **)(scTab + ((char *)&system_wq - sysWQ));

    /* allow write */
    allow_writes();

    /* dis-allow write */
    disallow_writes();
    printk("lioo init\n");
    return 0;
}
static void __exit lioo_exit(void) {
    /* recover */
    allow_writes();
    disallow_writes();
    printk("lioo exit\n");
}
module_init(lioo_init);
module_exit(lioo_exit);