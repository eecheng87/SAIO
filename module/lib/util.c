#include "../include/util.h"

#if defined(__x86_64__)
void allow_writes(void)
{
    unsigned long cr0 = read_cr0();
    clear_bit(16, &cr0);
    store_cr0(cr0);
}
void disallow_writes(void)
{
    unsigned long cr0 = read_cr0();
    set_bit(16, &cr0);
    store_cr0(cr0);
}
#endif
