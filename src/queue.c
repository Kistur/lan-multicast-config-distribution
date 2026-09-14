/*===============================================
 *   文件名称：queue.c
 *   描    述：消息队列实现
 ================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

void queue_init(msg_queue_t *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_destroy(msg_queue_t *q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

void queue_push(msg_queue_t *q, const MsgPacket *pkt) {
    pthread_mutex_lock(&q->mutex);
    // 队列满则等待
    while (q->count == QUEUE_SIZE) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    // 写入数据
    q->buffer[q->tail] = *pkt;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    // 通知消费者
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

void queue_pop(msg_queue_t *q, MsgPacket *pkt) {
    pthread_mutex_lock(&q->mutex);
    // 队列空则等待
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    // 读取数据
    *pkt = q->buffer[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    // 通知生产者
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}