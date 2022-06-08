#define _GNU_SOURCE

#define DEPLOY_TAGET 1
#define MAX_TABLE_SIZE 64
#define MAX_POOL_SIZE 900000000

/* optimize: order two */
#define MAX_POOL_IOV_SIZE 1024
#define MAX_POOL_MSG_SIZE 1024
#define IOV_MASK (MAX_POOL_IOV_SIZE - 1)
#define MSG_MASK (MAX_POOL_MSG_SIZE - 1)
#define POOL_UNIT 8

#include "../module/include/esca.h"
#include "../module/syscall.h"
#include <dlfcn.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>

typedef unsigned long long ull;

typedef long (*open_t)(const char* pathname, int flags, mode_t mode);
open_t real_open;
typedef long (*read_t)(int fd, void* buf, size_t count);
read_t real_read;
typedef long (*write_t)(unsigned int fd, const char* buf, size_t count);
write_t real_write;
typedef long (*close_t)(int fd);
close_t real_close;
typedef long (*writev_t)(int fd, const struct iovec* iov, int iovcnt);
writev_t real_writev;
typedef long (*shutdown_t)(int fd, int how);
shutdown_t real_shutdown;
typedef long (*sendfile_t)(int out_fd, int in_fd, off_t* offset, size_t count);
sendfile_t real_sendfile;
typedef long (*sendmsg_t)(int sockfd, const struct msghdr* msg, int flags);
sendmsg_t real_sendmsg;
