SUBDIRS := module wrapper
TOPTARGETS := all clean

LIGHTY_SOURCE := https://github.com/lighttpd/lighttpd1.4/archive/refs/tags/lighttpd-1.4.56.tar.gz
LIGHTY_ZIP_NAME := lighttpd-1.4.56
LIGHTY_NAME := lighttpd1.4-lighttpd-1.4.56
LIGHTY_PATH := downloads/$(LIGHTY_NAME)
LIGHTY := lighttpd

NGX_SOURCE := http://nginx.org/download/nginx-1.21.4.tar.gz
NGX_NAME := nginx-1.21.4
NGX_PATH := downloads/$(NGX_NAME)
NGX := nginx
# NGX_CONFIG := "set basic configuration"
NGX_CONFIG_TLS := --with-http_ssl_module
NGX_CONFIG_kTLS := --with-http_auth_request_module \
					--with-http_secure_link_module \
					--with-http_slice_module \
					--with-http_ssl_module \
					--with-openssl=../openssl-3.0.0 \
					--with-openssl-opt=enable-ktls

OPENSSL_SOURCE := https://www.openssl.org/source/openssl-3.0.0.tar.gz
OPENSSL_NAME := openssl-3.0.0
OPENSSL_PATH := downloads/$(OPENSSL_NAME)
OPENSSL := openssl

REDIS_SOURCE := https://github.com/redis/redis/archive/refs/tags/7.0-rc2.tar.gz
REDIS_NAME := redis-7.0-rc2
REDIS_ZIP_NAME := 7.0-rc2
REDIS_PATH := downloads/$(REDIS_NAME)
REDIS := redis

MEMCACHED_SOURCE := http://www.memcached.org/files/memcached-1.6.15.tar.gz
MEMCACHED_NAME := memcached-1.6.15
MEMCACHED_ZIP_NAME := memcached-1.6.15
MEMCACHED_PATH := downloads/$(MEMCACHED_NAME)
MEMCACHED := memcached
MEMCACHED_CONFIG_TLS := --enable-tls

LIBEVENT_SOURCE := https://github.com/libevent/libevent/archive/refs/tags/release-2.1.12-stable.tar.gz
LIBEVENT_NAME := libevent-release-2.1.12-stable
LIBEVENT_ZIP_NAME := release-2.1.12-stable
LIBEVENT_PATH := downloads/$(LIBEVENT_NAME)
LIBEVENT := libevent

MEMTIER_SOURCE := https://github.com/RedisLabs/memtier_benchmark/archive/refs/tags/1.4.0.zip
MEMTIER_NAME := memtier_benchmark-1.4.0
MEMTIER_ZIP_NAME := 1.4.0
MEMTIER_PATH := downloads/$(MEMTIER_NAME)
MEMTIER := memtier

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
	wget $(OPENSSL_SOURCE) --no-check-certificate
	mkdir -p $(OPENSSL_PATH)
	tar -zxvf $(OPENSSL_NAME).tar.gz -C $(OUT)
	rm $(OPENSSL_NAME).tar.gz
	mkdir -p local
	cd $(NGX_PATH) && ./configure --prefix=$(PWD)/local $(NGX_CONFIG)
	scripts/crt.sh
	scripts/ngx.sh $(NGX_PATH) $(CONFIG)
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

$(LIBEVENT):
	@echo "download libevent..."
	wget $(LIBEVENT_SOURCE) --no-check-certificate
	mkdir -p $(LIBEVENT_PATH)
	tar -zxvf $(LIBEVENT_ZIP_NAME).tar.gz -C $(OUT)
	rm $(LIBEVENT_ZIP_NAME).tar.gz
	cd $(LIBEVENT_PATH) && mkdir -p local && ./autogen.sh && ./configure --prefix=$(PWD)/$(LIBEVENT_PATH)/local
	cd $(OUT) && patch -p1 < ../patches/libevent.patch

$(MEMCACHED): $(LIBEVENT)
	@echo "download memcached..."
	wget $(MEMCACHED_SOURCE)
	mkdir -p $(MEMCACHED_PATH)
	tar -zxvf $(MEMCACHED_ZIP_NAME).tar.gz -C $(OUT)
	rm $(MEMCACHED_ZIP_NAME).tar.gz
	cd $(MEMCACHED_PATH) && mkdir -p local && ./configure --prefix=$(PWD)/$(MEMCACHED_PATH)/local $(MEMCACHED_CONFIG_TLS)
	cd $(OUT) && patch -p1 < ../patches/memcached.patch
	scripts/libevent.sh $(LIBEVENT_PATH)
	cd $(LIBEVENT_PATH) && make -j$(nproc) && sudo make install
	scripts/memcached.sh $(MEMCACHED_PATH)
	cd $(MEMCACHED_PATH) && make -j$(nproc)

$(MEMTIER):
	@echo "download memtier_benchmark..."
	wget --no-check-certificate $(MEMTIER_SOURCE)
	mkdir -p $(MEMTIER_PATH)
	unzip $(MEMTIER_ZIP_NAME).zip -d $(OUT)
	rm $(MEMTIER_ZIP_NAME).zip
	cd $(MEMTIER_PATH) && autoreconf -ivf && ./configure
	cd $(OUT) && patch -p1 < ../patches/shard_connection.patch
	cd $(MEMTIER_PATH) && make -j$(nproc)

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

# set deployed target
ifeq ($(strip $(TARGET)),ngx)
TARGET = ngx
else ifeq ($(strip $(TARGET)),lighty)
TARGET = lighty
else ifeq ($(strip $(TARGET)),redis)
TARGET = redis
else ifeq ($(strip $(TARGET)),echo)
TARGET = echo
else ifeq ($(strip $(TARGET)),mcached)
TARGET = mcached
endif

# set configuration flag of Nginx
ifeq ($(strip $(CONFIG)),tls)
NGX_CONFIG += $(NGX_CONFIG_TLS)
else ifeq ($(strip $(CONFIG)),ktls)
NGX_CONFIG += $(NGX_CONFIG_kTLS)
else
endif
export NGX_CONFIG

config:
	sed -i "s#DEFAULT_CONFIG_PATH \".*\"#DEFAULT_CONFIG_PATH \"$(PWD)/esca\.conf\"#" module/include/esca.h
	ln -s $(shell pwd)/wrapper/$(TARGET).c wrapper/target-preload.c
ifneq ($(CONFIG),)
	@if [ $(CONFIG) = "tls" ]; then \
		echo ">> TLS is used"; \
		cat wrapper/ngx_tls.c >> wrapper/target-preload.c; \
	elif [ $(CONFIG) = "ktls" ]; then \
		echo ">> kTLS is used"; \
		cat wrapper/ngx_ktls.c >> wrapper/target-preload.c; \
	fi
endif

kill-lighttpd:
	kill -9 $(shell ps -ef | awk '$$8 ~ /lighttpd/ {print $$2}')

clean-out:
	rm -rf $(OUT)
	rm -rf local
	rm -rf auth

recover:
	git checkout HEAD -- configs/nginx.conf wrapper/ngx.c module/include/esca.h esca.conf

.PHONY: $(TOPTARGETS) $(SUBDIRS) $(NGX) $(LIGHTY)
