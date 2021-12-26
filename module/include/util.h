#ifndef _UTIL_H
#define _UTIL_H

#include <linux/kernel.h>
#include <linux/module.h>

#if defined(__x86_64__)
extern unsigned long __force_order __attribute__((weak));
#define store_cr0(x) asm volatile("mov %0,%%cr0" \
                                  : "+r"(x), "+m"(__force_order))
#endif

void allow_writes(void);
void disallow_writes(void);

#endif
