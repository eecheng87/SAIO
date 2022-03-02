SUBDIRS := module wrapper
TOPTARGETS := all clean

LIGHTY_SOURCE := https://github.com/lighttpd/lighttpd1.4/archive/refs/tags/lighttpd-1.4.56.tar.gz
LIGHTY_ZIP_NAME := lighttpd-1.4.56
LIGHTY_NAME := lighttpd1.4-lighttpd-1.4.56
LIGHTY_PATH := downloads/$(LIGHTY_NAME)
LIGHTY := lighttpd

LIBDUMMY_PATH := $(shell find $(shell pwd) -type f -name "libdummy.so") | sed 's_/_\\/_g'
PWD := $(shell pwd)
OUT := downloads

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

reload:
	sudo dmesg -C
	-sudo rmmod mlioo
	sudo insmod module/mlioo.ko

format:
	find module/ -iname *.h -o -iname *.c -type f | xargs clang-format -i -style=WebKit
	find wrapper/ -iname *.h -o -iname *.c -type f | xargs clang-format -i -style=WebKit

$(LIGHTY):
	@echo "download lighttpd..."
	wget $(LIGHTY_SOURCE)
	mkdir -p $(OUT) && tar -zxvf $(LIGHTY_ZIP_NAME).tar.gz -C $(OUT)
	rm $(LIGHTY_ZIP_NAME).tar.gz
	cd $(LIGHTY_PATH) && ./autogen.sh && ./configure
	scripts/lighttpd.sh $(LIGHTY_PATH)
	cd $(OUT) && patch -p1 < ../patches/lighttpd.patch
	cd $(LIGHTY_PATH) && sudo make -j$(nproc) install
	cp -f configs/lighttpd.conf $(LIGHTY_PATH)/src/lighttpd.conf

test-lighttpd-perf:
	./$(LIGHTY_PATH)/src/lighttpd -D -f $(LIGHTY_PATH)/src/lighttpd.conf

test-esca-lighttpd-perf:
	LD_PRELOAD=wrapper/preload.so ./$(LIGHTY_PATH)/src/lighttpd -D -f $(LIGHTY_PATH)/src/lighttpd.conf #& \

kill-lighttpd:
	kill -9 $(shell ps -ef | awk '$$8 ~ /lighttpd/ {print $$2}')

clean-out:
	rm -rf $(OUT)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
