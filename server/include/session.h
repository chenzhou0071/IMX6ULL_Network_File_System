/*
 * IMX6ULL FTP Server - Session Module Header
 * Version: 1.0
 */

#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include "config.h"
#include "network.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 传输方向
 * ============================================================ */
typedef enum {
    TRANSFER_NONE = 0,
    TRANSFER_UPLOAD = 1,
    TRANSFER_DOWNLOAD = 2
} transfer_direction_t;

/* ============================================================
 * 传输任务状态
 * ============================================================ */
typedef enum {
    TASK_IDLE = 0,
    TASK_WAITING = 1,
    TASK_RUNNING = 2,
    TASK_PAUSED = 3,
    TASK_COMPLETED = 4,
    TASK_FAILED = 5
} task_status_t;

/* ============================================================
 * 传输任务结构
 * ============================================================ */
typedef struct transfer_task {
    uint32_t task_id;
    char remote_path[256];
    FILE* file;                   /* 传输文件句柄 */
    uint32_t file_size;           /* 文件总大小 */
    uint32_t transferred;          /* 已传输字节数 */
    transfer_direction_t direction;   /* 传输方向 */
    task_status_t status;         /* 任务状态 */
    time_t start_time;
    time_t last_update;
    pthread_mutex_t lock;
    struct transfer_task* next;
} transfer_task_t;

/* ============================================================
 * 客户端会话结构
 * ============================================================ */
typedef struct client_session {
    uint32_t session_id;          /* 会话ID */
    socket_t sockfd;              /* 客户端socket */
    struct sockaddr_in client_addr;   /* 客户端地址 */
    char username[64];            /* 用户名 */
    bool is_authenticated;        /* 是否已认证 */
    permission_t perm;            /* 权限 */
    time_t last_activity;          /* 最后活跃时间 */
    time_t connect_time;           /* 连接时间 */
    transfer_task_t* tasks;       /* 传输任务链表 */
    int task_count;               /* 当前传输任务数 */
    pthread_mutex_t lock;
    struct client_session* next;
} client_session_t;

/* ============================================================
 * 会话管理函数
 * ============================================================ */

/**
 * 创建会话
 * @param sockfd 客户端socket
 * @param client_addr 客户端地址
 * @return 会话指针
 */
client_session_t* session_create(socket_t sockfd, struct sockaddr_in* client_addr);

/**
 * 销毁会话
 * @param sess 会话指针
 */
void session_destroy(client_session_t* sess);

/**
 * 根据会话ID查找会话
 * @param session_id 会话ID
 * @return 会话指针，未找到返回NULL
 */
client_session_t* session_find_by_id(uint32_t session_id);

/**
 * 更新会话活跃时间
 * @param sess 会话指针
 */
void session_update_activity(client_session_t* sess);

/**
 * 获取所有会话数量
 */
int session_get_count(void);

/**
 * 获取活跃会话数量
 */
int session_get_active_count(void);

/**
 * 会话列表加锁
 */
void session_lock_all(void);

/**
 * 会话列表解锁
 */
void session_unlock_all(void);

/* ============================================================
 * 传输任务管理函数
 * ============================================================ */

/**
 * 添加传输任务
 * @param sess 会话指针
 * @param direction 传输方向
 * @param remote_path 远程路径
 * @param file_size 文件大小
 * @param file 文件句柄
 * @return 任务指针
 */
transfer_task_t* session_add_task(client_session_t* sess,
                                   transfer_direction_t direction,
                                   const char* remote_path,
                                   uint32_t file_size,
                                   FILE* file);

/**
 * 更新传输任务进度
 * @param task 任务指针
 * @param transferred 已传输字节数
 */
void session_update_task(transfer_task_t* task, uint32_t transferred);

/**
 * 暂停传输任务
 * @param task 任务指针
 */
void session_pause_task(transfer_task_t* task);

/**
 * 恢复传输任务
 * @param task 任务指针
 */
void session_resume_task(transfer_task_t* task);

/**
 * 删除传输任务
 * @param sess 会话指针
 * @param task_id 任务ID
 */
void session_remove_task(client_session_t* sess, uint32_t task_id);

/**
 * 计算传输进度百分比
 * @param task 任务指针
 * @return 进度百分比 0-100
 */
float transfer_progress_percent(transfer_task_t* task);

/**
 * 计算传输速度（字节/秒）
 * @param task 任务指针
 * @return 速度
 */
uint32_t transfer_speed_bps(transfer_task_t* task);

/**
 * 清理会话的所有传输任务
 * @param sess 会话指针
 */
void session_cleanup_tasks(client_session_t* sess);

#ifdef __cplusplus
}
#endif

#endif /* SESSION_H */