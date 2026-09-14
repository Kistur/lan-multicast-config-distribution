/*===============================================
 *   文件名称：protocol.h
 *   描    述：公共协议定义（新增 priority）
 ================================================*/
#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define MULTICAST_IP   "239.1.1.1"
#define MULTICAST_PORT 8888
#define TCP_PORT       9999

// 指令类型
typedef enum {
    CMD_REBOOT = 1,
    CMD_UPDATE,
    CMD_SHUTDOWN,
    CMD_HEARTBEAT
} CmdType;

// 网络传输包（添加 priority 字段）
typedef struct {
    int cmd;            // 指令类型
    int len;            // 数据长度
    int priority;       // 优先级 1-3（1最低，3最高）
    char data[256];     // 指令内容
    char sender_ip[16]; // 记录send ip
} MsgPacket;

#endif