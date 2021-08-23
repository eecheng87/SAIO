PWD := $(shell pwd)
MODULE_NAME = lioo
obj-m := $(MODULE_NAME).o

all: systab.h
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

# record address of system call table
systab.h:
	(grep -w sys_call_table /boot/System.map-`uname -r` | \
	  sed 's/\([^ ]*\) .*/#define scTab ((char *)0x\1UL)/'; \
	 grep -w system_wq      /boot/System.map-`uname -r` | \
	  sed 's/\([^ ]*\) .*/#define sysWQ ((char *)0x\1UL)/') > systab.h
ins:
	sudo insmod $(MODULE_NAME).ko
rm:
	sudo rmmod $(MODULE_NAME)

format:
	clang-format -style="{IndentWidth: 4}" -i *.c

clean:
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean
	rm systab.h