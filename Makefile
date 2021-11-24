SUBDIRS := module wrapper
TOPTARGETS := all clean format

$(TOPTARGETS): $(SUBDIRS) 

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
	
reload:
	sudo dmesg -C
	-sudo rmmod mlioo
	sudo insmod module/mlioo.ko

.PHONY: $(TOPTARGETS) $(SUBDIRS)
