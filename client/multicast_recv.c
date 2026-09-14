/*===============================================
 *   文件名称：multicast_recv.c
 *   描    述：组播接收客户端（打印 priority）
 *   用    法：./recv
 ================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>
#include "../include/protocol.h"

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (-1 == sock) {
        perror("socket error");
        return 1;
    }

    // 允许端口复用
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt reuse");
        close(sock);
        return 1;
    }

    // 加入组播组（必须！）
    struct ip_mreqn mreqn;
    inet_pton(AF_INET, MULTICAST_IP, &mreqn.imr_multiaddr);
    mreqn.imr_address.s_addr = htonl(INADDR_ANY);
    mreqn.imr_ifindex = 0;
    if (-1 == setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn))) {
        perror("setsockopt join");
        close(sock);
        return 1;
    }
    printf("已加入组播组 %s:%d\n", MULTICAST_IP, MULTICAST_PORT);

    // 绑定端口
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MULTICAST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        close(sock);
        return 1;
    }

    MsgPacket pkt;
    while (1) {
        memset(&pkt, 0, sizeof(pkt));
        int n = recvfrom(sock, &pkt, sizeof(pkt), 0, NULL, NULL);
        if (n > 0) {
            printf("[终端] 收到指令 [优先级 %d]: %s\n", pkt.priority, pkt.data);
            if (pkt.priority >= 2) {
                printf("   -> 高优先级指令，立即执行！\n");
            }
        }
    }

    close(sock);
    return 0;
}