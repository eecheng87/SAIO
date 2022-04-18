#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define TARGET_HOST "127.0.0.1"
#define TARGET_PORT 12345
#define BENCH_COUNT 10
#define BENCHMARK_RESULT_FILE "bench.txt"

/* length of unique message (TODO below) should shorter than this */
#define MAX_MSG_LEN 1000
#define MAX_THREAD 1000

static int conf_port = 12345;
static int conf_conn = 100;
static int conf_size = 1000;
static int conf_dur = 5;

static char* msg;

static pthread_t pt[MAX_THREAD];

/* block all workers before they are all ready to benchmarking kecho */
static bool ready;
static bool shut;

static pthread_mutex_t res_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t worker_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t worker_wait = PTHREAD_COND_INITIALIZER;

unsigned long long total_success = 0;

static const char short_options[] = "p:c:d:s:";

static const struct option long_options[] = {
    { "concurrency", 1, NULL, 'c' }, { "port", 1, NULL, 'p' },
    { "duration", 1, NULL, 'd' }, { "size", 1, NULL, 's' }, { NULL, 0, NULL, 0 }
};

static void init()
{
    msg = (char*)malloc(sizeof(char) * conf_size);
    for (int i = 0; i < conf_size; i++) {
        msg[i] = 'a';
    }
    msg[conf_size - 1] = '\0';
}

static void* bench_worker(__attribute__((unused)))
{
    int sock_fd;
    char dummy[conf_size];
    unsigned long long local_success = 0;

    /* wait until all workers created */
    pthread_mutex_lock(&worker_lock);
    while (!ready)
        if (pthread_cond_wait(&worker_wait, &worker_lock)) {
            puts("pthread_cond_wait failed");
            exit(-1);
        }
    pthread_mutex_unlock(&worker_lock);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        exit(-1);
    }

    struct sockaddr_in info = {
        .sin_family = PF_INET,
        .sin_addr.s_addr = inet_addr(TARGET_HOST),
        .sin_port = htons(conf_port),
    };

    if (connect(sock_fd, (struct sockaddr*)&info, sizeof(info)) == -1) {
        perror("connect");
        exit(-1);
    }

    while (!shut) {
        ssize_t rret;

        send(sock_fd, msg, strlen(msg), 0);
        rret = recv(sock_fd, dummy, conf_size, 0);

        if (rret < 0)
            printf("recv error \n");
        else
            local_success++;
    }

    pthread_mutex_lock(&res_lock);
    total_success += local_success;
    pthread_mutex_unlock(&res_lock);

    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);

    pthread_exit(NULL);
}

static void create_worker(int thread_qty)
{
    for (int i = 0; i < thread_qty; i++) {
        if (pthread_create(&pt[i], NULL, bench_worker, NULL)) {
            puts("thread creation failed");
            exit(-1);
        }
    }
}

static void bench(void)
{
    ready = false;
    shut = false;

    create_worker(conf_conn);

    pthread_mutex_lock(&worker_lock);

    ready = true;

    /* all workers are ready, let's start bombing kecho */
    pthread_cond_broadcast(&worker_wait);
    pthread_mutex_unlock(&worker_lock);

    /* duration */
    sleep(conf_dur);
    shut = true;

    /* waiting for all workers to finish the measurement */
    for (int x = 0; x < MAX_THREAD; x++)
        pthread_join(pt[x], NULL);

    free(msg);

    /* summarize result */
    printf("Benchmarking: %s:%d\n", TARGET_HOST, conf_port);
    printf("%d clients, running %d bytes.\n\n", conf_conn, conf_size);

    printf("%lld requests in %d sec.\n", total_success, conf_dur);
    printf("Requests/sec: %.2lf\n", (double)(total_success) / conf_dur);
}

int main(int argc, char* argv[])
{
    int next_option;
    do {
        next_option = getopt_long(argc, argv, short_options, long_options, NULL);

        switch (next_option) {
        case 'p':
            conf_port = atoi(optarg);
            break;
        case 'c':
            conf_conn = atoi(optarg);
            if (conf_conn > MAX_THREAD) {
                printf("connection cannot be greater than 1000\n");
                return 0;
            }
            break;
        case 'd':
            conf_dur = atoi(optarg);
            break;
        case 's':
            conf_size = atoi(optarg);
            break;
        case -1:
            break;
        default:
            printf("Unexpected argument: '%c'\n", next_option);
            return 0;
        }
    } while (next_option != -1);

    init();
    bench();
    return 0;
}
