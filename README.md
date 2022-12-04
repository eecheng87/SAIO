# SAIO (Scalable Asynchronous I/O Extension)
This project is considered as the insight into general solutions to scalability issues in AIO (asynchronous I/O) model.
We proposed a offloading framework for I/O-intensive applications, using zero-copy scheme and M on-N threading system, as a generic scalable solution along with its adoption process of event-driven servers.
This work extends our previous work [ESCA](https://github.com/eecheng87/dBatch/tree/preview) into asynchronous I/O model, which provides better scalability.
With SAIO, HTTP/HTTPS servers have been demonstrated to outperform typical configurations in the aspects of throughput and latency.
Moreover, we exhibit SAIO’s potential impact on TLS in kernel (kTLS).

## Prerequisite
For Nginx and wrk:
```
sudo apt install build-essential libpcre3 libpcre3-dev zlib1g zlib1g-dev
sudo apt install libssl-dev libgd-dev libxml2 libxml2-dev uuid-dev
sudo apt install autoconf automake libtool
```

## Download project
```
git clone https://github.com/eecheng87/SAIO.git
cd SAIO

# download testbench
git submodule update --init
```

## Build from source
SAIO has been deployed to lighttpd, Nginx (TLS+kTLS), Redis and memcached (TLS). Also, SAIO shows the impact on all of these targets

### Nginx
```
# download and build Nginx with tls
make config TARGET=ngx CONFIG=tls
sudo make
make nginx CONFIG=tls

# download and build Nginx with ktls
sudo modprobe tls
make config TARGET=ngx CONFIG=ktls
sudo make
make nginx CONFIG=ktls
```

### Memcached
> Support for memcached is only available in the branch `mcache`, which is designed with multi-threading model.
```
# download and build memcached
make config TARGET=mcached
sudo make

# download and build the benchmarking tool
make memtier

# sample for launching memcached
LD_PRELOAD=/path/to/preload.so ./memcached -t 1

# sample for launching memtier_benchmarking
./memtier_benchmark -p 11211 --protocol=memcache_text --clients=100 --threads=5 --ratio=1:1 --key-pattern=R:R --key-minimum=16 --key-maximum=16 --data-size=128 --test-time=5
```

