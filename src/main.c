/*===============================================
 *   文件名称：main.c
 *   描    述：服务器主程序（增加权限等级 + 双队列）
 ================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <pthread.h>
#include "../include/protocol.h"
#include "../include/queue.h"
#include "../include/db.h"

#define MAX_EVENTS 10
#define MAX_CLIENTS 1024

// ========== 全局变量 ==========
msg_queue_t g_queue;          // 组播发送队列
int g_udp_sock;               // UDP组播发送套接字
struct sockaddr_in g_mcast_addr; // 组播目标地址

// 管理员等级表（fd -> level）
int g_client_level[MAX_CLIENTS];

// ========== 子线程1：UDP组播发送（消费者） ==========
void *udp_sender_thread(void *arg) {
    MsgPacket pkt;
    while (1) {
        queue_pop(&g_queue, &pkt);
        int ret = sendto(g_udp_sock, &pkt, sizeof(pkt), 0,
                         (struct sockaddr *)&g_mcast_addr, sizeof(g_mcast_addr));
        if (ret > 0) {
            printf("[组播发送] 指令: %s (优先级 %d)\n", pkt.data, pkt.priority);
        } else {
            perror("sendto");
        }
    }
    return NULL;
}

// ========== TCP管理初始化 ==========
int tcp_server_init(int port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listenfd) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (-1 == bind(listenfd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        close(listenfd);
        return -1;
    }

    if (-1 == listen(listenfd, 10)) {
        perror("listen");
        close(listenfd);
        return -1;
    }

    printf("[TCP管理] 端口 %d，等待管理员连接...\n", port);
    return listenfd;
}

// ========== UDP组播发送初始化 ==========
int udp_multicast_init() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == sock) {
        perror("socket UDP");
        return -1;
    }

    memset(&g_mcast_addr, 0, sizeof(g_mcast_addr));
    g_mcast_addr.sin_family = AF_INET;
    g_mcast_addr.sin_port = htons(MULTICAST_PORT);
    inet_pton(AF_INET, MULTICAST_IP, &g_mcast_addr.sin_addr);

    return sock;
}

// ========== 获取管理员等级（简单验证） ==========
int get_admin_level(int confd) {
    char buf[16] = {0};
    send(confd, "Please enter your level (1-3): ", 30, 0);
    int n = recv(confd, buf, sizeof(buf)-1, 0);
    if (n <= 0) return 1; // 默认等级1
    buf[n] = '\0';
    int level = atoi(buf);
    if (level < 1 || level > 3) level = 1;
    return level;
}

// ========== 主线程 ==========
int main() {
    // 初始化等级表
    for (int i = 0; i < MAX_CLIENTS; i++) g_client_level[i] = 1;

    // 初始化队列
    queue_init(&g_queue);

    // 初始化数据库（创建表并启动 db 线程）
    db_init();

    // 初始化 UDP 组播
    g_udp_sock = udp_multicast_init();
    if (-1 == g_udp_sock) {
        fprintf(stderr, "UDP初始化失败\n");
        return 1;
    }

    // 创建子线程1（组播发送）
    pthread_t tid_sender;
    pthread_create(&tid_sender, NULL, udp_sender_thread, NULL);
    pthread_detach(tid_sender);

    // 初始化 TCP 管理
    int listenfd = tcp_server_init(TCP_PORT);
    if (-1 == listenfd) return 1;

    // epoll 事件循环
    int epfd = epoll_create(1);
    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    printf("[服务器] 启动成功\n");

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

                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &caddr.sin_addr, ip, sizeof(ip));
                printf("[管理员] %s:%d 已连接\n", ip, ntohs(caddr.sin_port));

                // 验证等级
                int level = get_admin_level(confd);
                g_client_level[confd] = level;
                char msg[64];
                sprintf(msg, "Level %d confirmed. Send commands.\n", level);
                send(confd, msg, strlen(msg), 0);

                // 加入 epoll 监控
                ev.events = EPOLLIN;
                ev.data.fd = confd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, confd, &ev);
            } else {
                // 收到管理员指令
                char buf[1024] = {0};
                int n = recv(fd, buf, sizeof(buf)-1, 0);
                if (n <= 0) {
                    printf("[管理员] 断开连接\n");
                    close(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    g_client_level[fd] = 1;
                    continue;
                }
                buf[n] = '\0';
                buf[strcspn(buf, "\n")] = '\0';

                // 获取客户端 IP
                struct sockaddr_in peer_addr;
                socklen_t peer_len = sizeof(peer_addr);
                char ip[INET_ADDRSTRLEN] = "unknown";
                if (getpeername(fd, (struct sockaddr *)&peer_addr, &peer_len) == 0) {
                    inet_ntop(AF_INET, &peer_addr.sin_addr, ip, sizeof(ip));
                }

                MsgPacket pkt;
                pkt.cmd = CMD_REBOOT;
                pkt.len = strlen(buf);
                pkt.priority = g_client_level[fd];
                strncpy(pkt.sender_ip, ip, sizeof(pkt.sender_ip)-1);
                pkt.sender_ip[sizeof(pkt.sender_ip)-1] = '\0';
                strncpy(pkt.data, buf, sizeof(pkt.data)-1);
                pkt.data[sizeof(pkt.data)-1] = '\0';

                printf("[管理员指令] %s (等级 %d, IP %s)\n", buf, pkt.priority, ip);

                // 放入组播队列
                queue_push(&g_queue, &pkt);
                // 同时放入数据库队列（异步落盘）
                db_push_record(pkt.cmd, pkt.priority, pkt.data, ip);
            }
        }
    }

    close(listenfd);
    close(g_udp_sock);
    queue_destroy(&g_queue);
    db_destroy();
    return 0;
}