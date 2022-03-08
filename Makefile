SUBDIRS := module wrapper
TOPTARGETS := all clean

LIGHTY_SOURCE := https://github.com/lighttpd/lighttpd1.4/archive/refs/tags/lighttpd-1.4.56.tar.gz
LIGHTY_ZIP_NAME := lighttpd-1.4.56
LIGHTY_NAME := lighttpd1.4-lighttpd-1.4.56
LIGHTY_PATH := downloads/$(LIGHTY_NAME)
LIGHTY := lighttpd

NGX_SOURCE := http://nginx.org/download/nginx-1.20.0.tar.gz
NGX_NAME := nginx-1.20.0
NGX_PATH := downloads/$(NGX_NAME)
NGX := nginx

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

$(NGX):
	@echo "download nginx..."
	wget $(NGX_SOURCE)
	mkdir -p $(NGX_PATH)
	tar -zxvf $(NGX_NAME).tar.gz -C $(OUT)
	rm $(NGX_NAME).tar.gz
	mkdir -p local
	cd $(NGX_PATH) && ./configure --prefix=$(PWD)/local
	scripts/ngx.sh $(NGX_PATH)
	cd $(OUT) && patch -p1 < ../patches/ngx_process.patch && patch -p1 < ../patches/ngx_epoll_module.patch
	cd $(NGX_PATH) && make -j$(nproc) && make install
	cp -f configs/nginx.conf local/conf/nginx.conf

test-nginx-perf:
	./$(NGX_PATH)/objs/nginx

test-esca-nginx-perf:
	LD_PRELOAD=wrapper/preload.so ./$(NGX_PATH)/objs/nginx

test-lighttpd-perf:
	./$(LIGHTY_PATH)/src/lighttpd -D -f $(LIGHTY_PATH)/src/lighttpd.conf

test-esca-lighttpd-perf:
	LD_PRELOAD=wrapper/preload.so ./$(LIGHTY_PATH)/src/lighttpd -D -f $(LIGHTY_PATH)/src/lighttpd.conf #& \

kill-lighttpd:
	kill -9 $(shell ps -ef | awk '$$8 ~ /lighttpd/ {print $$2}')

clean-out:
	rm -rf $(OUT)
	rm -rf local

.PHONY: $(TOPTARGETS) $(SUBDIRS) $(NGX) $(LIGHTY)
