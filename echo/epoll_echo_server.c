// import from https://github.com/frevib/epoll-echo-server
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLOG 512
#define MAX_EVENTS 1024
#define MAX_MESSAGE_LEN 91280

void error(char* msg);

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Please give a port number: ./epoll_echo_server [port]\n");
        exit(0);
    }

    // some variables we need
    int portno = strtol(argv[1], NULL, 10);
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    char buffer[MAX_MESSAGE_LEN];
    memset(buffer, 0, sizeof(buffer));

    // setup socket
    int sock_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_listen_fd < 0) {
        error("Error creating socket..\n");
    }

    memset((char*)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portno);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // bind socket and listen for connections
    if (bind(sock_listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        error("Error binding socket..\n");

    if (listen(sock_listen_fd, BACKLOG) < 0) {
        error("Error listening..\n");
    }
    printf("epoll echo server listening for connections on port: %d\n", portno);

    struct epoll_event ev, events[MAX_EVENTS];
    int new_events, sock_conn_fd, epollfd;

    epollfd = epoll_create(MAX_EVENTS);
    if (epollfd < 0) {
        error("Error creating epoll..\n");
    }
    ev.events = EPOLLIN;
    ev.data.fd = sock_listen_fd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_listen_fd, &ev) == -1) {
        error("Error adding new listeding socket to epoll..\n");
    }
    init_worker(0);
    while (1) {
        new_events = epoll_wait(epollfd, events, MAX_EVENTS, -1);

        if (new_events == -1) {
            error("Error in epoll_wait..\n");
        }
        batch_start();
        for (int i = 0; i < new_events; ++i) {
            if (events[i].data.fd == sock_listen_fd) {
                sock_conn_fd = accept4(sock_listen_fd, (struct sockaddr*)&client_addr, &client_len, SOCK_NONBLOCK);
                if (sock_conn_fd == -1) {
                    error("Error accepting new connection..\n");
                }

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = sock_conn_fd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_conn_fd, &ev) == -1) {
                    error("Error adding new event to epoll..\n");
                }
            } else {
                int newsockfd = events[i].data.fd;
                int bytes_received = recv(newsockfd, buffer, MAX_MESSAGE_LEN, 0);
                if (bytes_received <= 0) {
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, newsockfd, NULL);
                    shutdown(newsockfd, SHUT_RDWR);
                } else {
                    send(newsockfd, buffer, bytes_received, 0);
                }
            }
        }
        batch_flush();
    }
}

void error(char* msg)
{
    perror(msg);
    printf("erreur...\n");
    exit(1);
}