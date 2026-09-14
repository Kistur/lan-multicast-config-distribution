/*===============================================
 *   文件名称：db.c
 *   描    述：SQLite 异步写入（使用 sqlite3_exec）
 ================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <pthread.h>
#include "../include/db.h"
#include "../include/queue.h"

static sqlite3 *g_db = NULL;
static msg_queue_t g_db_queue;
static pthread_t g_db_thread;
static int g_db_running = 1;

// ========== 数据库写入线程（使用 sqlite3_exec） ==========
static void *db_writer_thread(void *arg) {
    sqlite3_stmt *stmt = NULL;

    // 预编译 INSERT 语句（? 是占位符）
    const char *sql =
        "INSERT INTO t_commands (cmd_type, priority, content, sender_ip) "
        "VALUES (?, ?, ?, ?);";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "预编译失败: %s\n", sqlite3_errmsg(g_db));
        return NULL;
    }

    while (1) {
        MsgPacket pkt;
        queue_pop(&g_db_queue, &pkt);

        if (pkt.cmd == -1) {
            break;
        }

        // 绑定参数（索引从 1 开始）
        sqlite3_bind_int(stmt, 1, pkt.cmd);
        sqlite3_bind_int(stmt, 2, pkt.priority);
        sqlite3_bind_text(stmt, 3, pkt.data, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, pkt.sender_ip, -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "插入失败: %s\n", sqlite3_errmsg(g_db));
        }

        // 重置语句，方便下一次复用
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);
    return NULL;
}

// ========== 初始化数据库 ==========
void db_init(void) {
    // 打开数据库
    if (sqlite3_open("commands.db", &g_db) != SQLITE_OK) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(g_db));
        return;
    }

    // 创建表（如果不存在）
    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS t_commands ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "cmd_type INTEGER,"
        "priority INTEGER,"
        "content TEXT,"
        "sender_ip TEXT,"
        "send_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");";

    char *errmsg = NULL;
    if (sqlite3_exec(g_db, create_sql, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "创建表失败: %s\n", errmsg);
        sqlite3_free(errmsg);
        return;
    }

    // 初始化数据库队列
    queue_init(&g_db_queue);

    // 创建数据库写入线程
    pthread_create(&g_db_thread, NULL, db_writer_thread, NULL);

    printf("[DB] 数据库线程启动，TID=%ld\n", g_db_thread);
}

// ========== 压入数据库队列 ==========
void db_push_record(int cmd, int priority, const char *data, const char *sender_ip) {
    MsgPacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.cmd = cmd;
    pkt.priority = priority;

    // 拷贝 data 并去掉末尾的 \r\n
    strncpy(pkt.data, data, sizeof(pkt.data) - 1);
    pkt.data[sizeof(pkt.data) - 1] = '\0';
    int len = strlen(pkt.data);
    while (len > 0 && (pkt.data[len - 1] == '\r' || pkt.data[len - 1] == '\n')) {
        pkt.data[--len] = '\0';
    }
    pkt.len = len;

    // sender_ip 也同样处理
    strncpy(pkt.sender_ip, sender_ip, sizeof(pkt.sender_ip) - 1);
    pkt.sender_ip[sizeof(pkt.sender_ip) - 1] = '\0';
    len = strlen(pkt.sender_ip);
    while (len > 0 && (pkt.sender_ip[len - 1] == '\r' || pkt.sender_ip[len - 1] == '\n')) {
        pkt.sender_ip[--len] = '\0';
    }

    queue_push(&g_db_queue, &pkt);
}

// ========== 销毁数据库资源 ==========
void db_destroy(void) {
    // 发送终止信号
    MsgPacket stop_pkt;
    stop_pkt.cmd = -1;
    queue_push(&g_db_queue, &stop_pkt);

    // 等待线程结束
    pthread_join(g_db_thread, NULL);

    // 关闭数据库
    sqlite3_close(g_db);
}