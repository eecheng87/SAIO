#ifndef SYSCALL_H
#define SYSCALL_H

#include <asm/unistd.h>

#include "include/esca.h"
#define __NR_esca_register 400
#define __NR_esca_wakeup 401
#define __NR_esca_wait 402
#define __NR_esca_config 403

/* batch table entry info */
#define BENTRY_EMPTY 0
#define BENTRY_BUSY 1

#ifndef __KERNEL__
#include <errno.h> /* needed by syscall macro */
#ifndef syscall
#include <unistd.h> /* syscall() */
#endif

static inline long
lioo_register(esca_table_t* table,
    esca_table_entry_t* e1,
    esca_table_entry_t* e2)
{
    syscall(__NR_esca_register, table, e1, e2);
}

static inline long
lioo_wait()
{
    syscall(__NR_esca_wait);
}

static inline long
lioo_init_conf(esca_config_t* conf)
{
    syscall(__NR_esca_config, conf);
}

#define scall0(N, FL, Z, P) ((syscall_t) { __NR_##N, FL, Z, P, 0 })
#define scall1(N, FL, Z, P, A) ((syscall_t) { __NR_##N, FL, Z, P, 1, (long)(A) })
#define scall2(N, FL, Z, P, A, B) \
    ((syscall_t) { __NR_##N, FL, Z, P, 2, (long)(A), (long)(B) })
#define scall3(N, FL, Z, P, A, B, C) \
    ((syscall_t) { __NR_##N, FL, Z, P, 3, (long)(A), (long)(B), (long)(C) })
#define scall4(N, FL, Z, P, A, B, C, D) \
    ((syscall_t) {                      \
        __NR_##N, FL, Z, P, 4, (long)(A), (long)(B), (long)(C), (long)(D) })
#define scall5(N, FL, Z, P, A, B, C, D, E) \
    ((syscall_t) { __NR_##N,               \
        FL,                                \
        Z,                                 \
        P,                                 \
        5,                                 \
        (long)(A),                         \
        (long)(B),                         \
        (long)(C),                         \
        (long)(D),                         \
        (long)(E) })
#define scall6(N, FL, Z, P, A, B, C, D, E, F) \
    ((syscall_t) { __NR_##N,                  \
        FL,                                   \
        Z,                                    \
        P,                                    \
        6,                                    \
        (long)(A),                            \
        (long)(B),                            \
        (long)(C),                            \
        (long)(D),                            \
        (long)(E),                            \
        (long)(F) })
#endif /* __KERNEL__ */
#endif /* SYSCALL_H */
