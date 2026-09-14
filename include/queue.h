/*===============================================
 *   文件名称：queue.h
 *   描    述：线程安全的消息队列（生产者-消费者）
 ================================================*/
#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>
#include "protocol.h"

#define QUEUE_SIZE 128

typedef struct {
    MsgPacket buffer[QUEUE_SIZE];  // 环形缓冲区
    int head;                      // 读位置
    int tail;                      // 写位置
    int count;                     // 当前元素个数
    pthread_mutex_t mutex;         // 互斥锁
    pthread_cond_t not_empty;      // 非空条件变量
    pthread_cond_t not_full;       // 非满条件变量
} msg_queue_t;

// 初始化队列
void queue_init(msg_queue_t *q);

// 销毁队列
void queue_destroy(msg_queue_t *q);

// 入队（生产者调用，阻塞直到有空间）
void queue_push(msg_queue_t *q, const MsgPacket *pkt);

// 出队（消费者调用，阻塞直到有数据）
void queue_pop(msg_queue_t *q, MsgPacket *pkt);

#endif