/*===============================================
 *   文件名称：db.h
 *   描    述：数据库操作接口
 ================================================*/
#ifndef _DB_H_
#define _DB_H_

#include "protocol.h"

// 初始化数据库（创建表）
void db_init(void);

// 异步写入指令历史（通过队列）
void db_push_record(int cmd, int priority, const char *data, const char *sender_ip);

// 销毁数据库资源
void db_destroy(void);

#endif