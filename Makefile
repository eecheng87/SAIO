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

REDIS_SOURCE := https://github.com/redis/redis/archive/refs/tags/7.0-rc2.tar.gz
REDIS_NAME := redis-7.0-rc2
REDIS_ZIP_NAME := 7.0-rc2
REDIS_PATH := downloads/$(REDIS_NAME)
REDIS := redis

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
	find echo/ -iname *.h -o -iname *.c -type f | xargs clang-format -i -style=WebKit

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

$(REDIS):
	@echo "download redis..."
	wget $(REDIS_SOURCE)
	mkdir -p $(REDIS_PATH)
	tar -zxvf $(REDIS_ZIP_NAME).tar.gz -C $(OUT)
	rm $(REDIS_ZIP_NAME).tar.gz
	scripts/redis.sh $(REDIS_PATH)
	cd $(OUT) && patch -p1 < ../patches/redis_network.patch && patch -p1 < ../patches/redis_server.patch
	cd $(REDIS_PATH) && make -j$(nproc)

test-nginx-perf:
	./$(NGX_PATH)/objs/nginx

test-esca-nginx-perf:
	LD_PRELOAD=wrapper/preload.so ./$(NGX_PATH)/objs/nginx

test-lighttpd-perf:
	./$(LIGHTY_PATH)/src/lighttpd -D -f $(LIGHTY_PATH)/src/lighttpd.conf

test-esca-lighttpd-perf:
	LD_PRELOAD=wrapper/preload.so ./$(LIGHTY_PATH)/src/lighttpd -D -f $(LIGHTY_PATH)/src/lighttpd.conf #& \

test-redis-perf:
	./$(REDIS_PATH)/src/redis-server

test-esca-redis-perf:
	LD_PRELOAD=wrapper/preload.so ./$(REDIS_PATH)/src/redis-server

default-redis-benchmark:
	./$(REDIS_PATH)/src/redis-benchmark -t set,get -n 100000 -q -c 100 -P 16


ifeq ($(strip $(TARGET)),ngx)
TARGET = ngx
else ifeq ($(strip $(TARGET)),lighty)
TARGET = lighty
else ifeq ($(strip $(TARGET)),redis)
TARGET = redis
else ifeq ($(strip $(TARGET)),echo)
TARGET = echo
endif

config:
	ln -s $(shell pwd)/wrapper/$(TARGET).c wrapper/target-preload.c

kill-lighttpd:
	kill -9 $(shell ps -ef | awk '$$8 ~ /lighttpd/ {print $$2}')

clean-out:
	rm -rf $(OUT)
	rm -rf local

.PHONY: $(TOPTARGETS) $(SUBDIRS) $(NGX) $(LIGHTY)
