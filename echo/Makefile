CFLAGS_user = -Wall -O2

all: rps io_uring_echo_server epoll_echo_server
rps: rps.c
	$(CC) -o $@ $(CFLAGS_user) -pthread $<

io_uring_echo_server: io_uring_echo_server.c
	$(CC) $< -o $@ ${CCFLAGS} -D_GNU_SOURCE -luring

epoll_echo_server: epoll_echo_server.c
	$(CC) $< -o $@ ${CCFLAGS} -D_GNU_SOURCE ../wrapper/libdummy.so

clean:
	rm rps io_uring_echo_server epoll_echo_server