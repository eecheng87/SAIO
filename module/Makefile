CC = gcc
PWD := $(shell pwd)
MODULE_NAME = mlioo
obj-m := mlioo.o
mlioo-objs := lioo.o /lib/util.o /lib/symbol.o
INC := include
LIBPATH := lib
LIBOBJS := $(patsubst %.c, %.o, $(wildcard lib/*.c))
CFLAGS := -O1 -I$(INC)
ccflags-y := -std=gnu99

all: systab.h $(LIBOBJS)
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

# record address of system call table
systab.h:
	(grep -w sys_call_table /boot/System.map-`uname -r` | \
	  sed 's/\([^ ]*\) .*/#define scTab ((char *)0x\1UL)/'; \
      grep -w copy_process /boot/System.map-`uname -r` | \
      sed 's/\([^ ]*\) .*/#define copy_process_base ((char *)0x\1UL)/'; \
	  grep -w wake_up_new_task /boot/System.map-`uname -r` | \
      sed 's/\([^ ]*\) .*/#define wake_up_new_task_base ((char *)0x\1UL)/'; \
      grep -w kernel_clone /boot/System.map-`uname -r` | \
      sed 's/\([^ ]*\) .*/#define kernel_clone_base ((char *)0x\1UL)/'; \
	  grep -w init_mm /boot/System.map-`uname -r` | \
      sed 's/\([^ ]*\) .*/#define sysMM ((char *)0x\1UL)/'; \
	  grep -w system_wq  /boot/System.map-`uname -r` | \
	  sed 's/\([^ ]*\) .*/#define sysWQ ((char *)0x\1UL)/') > include/systab.h
ins:
	sudo insmod $(MODULE_NAME).ko
rm:
	sudo rmmod $(MODULE_NAME)

clean:
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean
	rm include/systab.h
