#include "../include/symbol.h"
#include "../include/systab.h"

void** syscall_table_ptr = 0;
struct task_struct* (*create_io_thread_ptr)(int (*)(void*), void*, int) = 0;
void (*wake_up_new_task_ptr)(void*) = 0;

void init_not_exported_symbol()
{
    /* avoid effect of KALSR, get address of syscall table by adding offset */
    syscall_table_ptr = (void**)(scTab + ((char*)&system_wq - sysWQ));
    create_io_thread_ptr = (struct task_struct * (*)(int (*)(void*), void*, int))(
        create_io_thread_base + ((char*)&system_wq - sysWQ));
    wake_up_new_task_ptr = (void (*)(void*))(
        wake_up_new_task_base + ((char*)&system_wq - sysWQ));
}
