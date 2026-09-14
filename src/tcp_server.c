/*===============================================
 *   文件名称：tcp_server.c
 *   描    述：TCP 管理通道（管理员连接下发指令）
 *   用    法：./tcp_server
 *   *********************************************
 *   说明：本模块独立运行，接收管理员指令后
 *         将其打印到控制台（后续版本会发送给组播）
 ================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#define MAX_EVENTS 10
#define TCP_PORT 9999

int tcp_server_init(int port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listenfd) {
        perror("socket");
        return -1;
    }

    // 端口复用
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (-1 == bind(listenfd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        return -1;
    }

    if (-1 == listen(listenfd, 10)) {
        perror("listen");
        return -1;
    }

    printf("TCP 管理端启动，端口 %d，等待管理员连接...\n", port);
    return listenfd;
}

int main() {
    int listenfd = tcp_server_init(TCP_PORT);
    if (-1 == listenfd) return 1;

    int epfd = epoll_create(1);
    if (-1 == epfd) {
        perror("epoll_create");
        return 1;
    }

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    while (1) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == listenfd) {
                // 新管理员连接
                struct sockaddr_in caddr;
                socklen_t len = sizeof(caddr);
                int confd = accept(listenfd, (struct sockaddr *)&caddr, &len);
                if (-1 == confd) continue;
                printf("管理员 %s:%d 已连接\n",
                       inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

                ev.events = EPOLLIN;
                ev.data.fd = confd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, confd, &ev);
            } else {
                // 收到管理员指令
                char buf[1024] = {0};
                int n = recv(fd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    printf("管理员断开连接\n");
                    close(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    continue;
                }
                buf[n] = '\0';
                printf("[管理员指令] %s\n", buf);
                // TODO: 这里将指令放入队列，由子线程发送组播
                // 当前版本只打印，留待后续对接
            }
        }
    }

    close(listenfd);
    return 0;
}