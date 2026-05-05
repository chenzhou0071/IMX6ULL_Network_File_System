/*
 * IMX6ULL FTP Server - Session Module Implementation
 * Version: 1.0
 */

#include "session.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 全局会话链表
 * ============================================================ */
static client_session_t* g_sessions = NULL;
static uint32_t g_next_session_id = 1;
static pthread_mutex_t g_sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================
 * 生成唯一会话ID
 * ============================================================ */
static uint32_t generate_session_id(void) {
    uint32_t id;
    pthread_mutex_lock(&g_sessions_mutex);
    id = g_next_session_id++;
    pthread_mutex_unlock(&g_sessions_mutex);
    return id;
}

/* ============================================================
 * 创建会话
 * ============================================================ */
client_session_t* session_create(socket_t sockfd, struct sockaddr_in* client_addr) {
    client_session_t* sess = calloc(1, sizeof(client_session_t));
    if (sess == NULL) {
        LOG_ERROR("Failed to allocate session");
        return NULL;
    }

    sess->session_id = generate_session_id();
    sess->sockfd = sockfd;
    if (client_addr) {
        sess->client_addr = *client_addr;
    }
    sess->is_authenticated = false;
    sess->perm = PERM_NONE;
    sess->last_activity = time(NULL);
    sess->connect_time = time(NULL);
    sess->tasks = NULL;
    sess->task_count = 0;
    pthread_mutex_init(&sess->lock, NULL);

    /* 添加到会话链表 */
    pthread_mutex_lock(&g_sessions_mutex);
    sess->next = g_sessions;
    g_sessions = sess;
    pthread_mutex_unlock(&g_sessions_mutex);

    LOG_INFO("Session created: id=%u, sockfd=%d", sess->session_id, sockfd);
    return sess;
}

/* ============================================================
 * 销毁会话
 * ============================================================ */
void session_destroy(client_session_t* sess) {
    if (sess == NULL) return;

    LOG_INFO("Session destroyed: id=%u", sess->session_id);

    /* 从会话链表中移除 */
    pthread_mutex_lock(&g_sessions_mutex);
    client_session_t** ptr = &g_sessions;
    while (*ptr != NULL) {
        if (*ptr == sess) {
            *ptr = sess->next;
            break;
        }
        ptr = &((*ptr)->next);
    }
    pthread_mutex_unlock(&g_sessions_mutex);

    /* 清理传输任务 */
    session_cleanup_tasks(sess);

    /* 关闭socket */
    if (sess->sockfd >= 0) {
        SOCKET_CLOSE(sess->sockfd);
    }

    pthread_mutex_destroy(&sess->lock);
    free(sess);
}

/* ============================================================
 * 根据会话ID查找会话
 * ============================================================ */
client_session_t* session_find_by_id(uint32_t session_id) {
    pthread_mutex_lock(&g_sessions_mutex);
    client_session_t* sess = g_sessions;
    while (sess != NULL) {
        if (sess->session_id == session_id) {
            pthread_mutex_unlock(&g_sessions_mutex);
            return sess;
        }
        sess = sess->next;
    }
    pthread_mutex_unlock(&g_sessions_mutex);
    return NULL;
}

/* ============================================================
 * 更新会话活跃时间
 * ============================================================ */
void session_update_activity(client_session_t* sess) {
    if (sess == NULL) return;
    pthread_mutex_lock(&sess->lock);
    sess->last_activity = time(NULL);
    pthread_mutex_unlock(&sess->lock);
}

/* ============================================================
 * 获取所有会话数量
 * ============================================================ */
int session_get_count(void) {
    int count = 0;
    pthread_mutex_lock(&g_sessions_mutex);
    client_session_t* sess = g_sessions;
    while (sess != NULL) {
        count++;
        sess = sess->next;
    }
    pthread_mutex_unlock(&g_sessions_mutex);
    return count;
}

/* ============================================================
 * 获取活跃会话数量
 * ============================================================ */
int session_get_active_count(void) {
    int count = 0;
    time_t now = time(NULL);
    pthread_mutex_lock(&g_sessions_mutex);
    client_session_t* sess = g_sessions;
    while (sess != NULL) {
        if (sess->is_authenticated) {
            count++;
        }
        sess = sess->next;
    }
    pthread_mutex_unlock(&g_sessions_mutex);
    return count;
}

/* ============================================================
 * 会话列表加锁
 * ============================================================ */
void session_lock_all(void) {
    pthread_mutex_lock(&g_sessions_mutex);
}

void session_unlock_all(void) {
    pthread_mutex_unlock(&g_sessions_mutex);
}

/* ============================================================
 * 添加传输任务
 * ============================================================ */
transfer_task_t* session_add_task(client_session_t* sess,
                                   transfer_direction_t direction,
                                   const char* remote_path,
                                   uint32_t file_size,
                                   FILE* file) {
    if (sess == NULL) return NULL;

    transfer_task_t* task = calloc(1, sizeof(transfer_task_t));
    if (task == NULL) {
        LOG_ERROR("Failed to allocate transfer task");
        return NULL;
    }

    static uint32_t g_task_id = 1;
    pthread_mutex_lock(&sess->lock);

    task->task_id = g_task_id++;
    if (remote_path) {
        strncpy(task->remote_path, remote_path, sizeof(task->remote_path) - 1);
    }
    task->file_size = file_size;
    task->transferred = 0;
    task->direction = direction;
    task->status = TASK_RUNNING;
    task->start_time = time(NULL);
    task->last_update = time(NULL);
    task->file = file;
    pthread_mutex_init(&task->lock, NULL);

    /* 添加到任务链表 */
    task->next = sess->tasks;
    sess->tasks = task;
    sess->task_count++;

    pthread_mutex_unlock(&sess->lock);

    LOG_INFO("Task added: id=%u, direction=%d, path=%s", task->task_id, direction, remote_path);
    return task;
}

/* ============================================================
 * 更新传输任务进度
 * ============================================================ */
void session_update_task(transfer_task_t* task, uint32_t transferred) {
    if (task == NULL) return;

    pthread_mutex_lock(&task->lock);
    task->transferred = transferred;
    task->last_update = time(NULL);
    pthread_mutex_unlock(&task->lock);
}

/* ============================================================
 * 暂停传输任务
 * ============================================================ */
void session_pause_task(transfer_task_t* task) {
    if (task == NULL) return;

    pthread_mutex_lock(&task->lock);
    task->status = TASK_PAUSED;
    task->last_update = time(NULL);
    pthread_mutex_unlock(&task->lock);

    LOG_INFO("Task paused: id=%u", task->task_id);
}

/* ============================================================
 * 恢复传输任务
 * ============================================================ */
void session_resume_task(transfer_task_t* task) {
    if (task == NULL) return;

    pthread_mutex_lock(&task->lock);
    task->status = TASK_RUNNING;
    task->last_update = time(NULL);
    pthread_mutex_unlock(&task->lock);

    LOG_INFO("Task resumed: id=%u", task->task_id);
}

/* ============================================================
 * 删除传输任务
 * ============================================================ */
void session_remove_task(client_session_t* sess, uint32_t task_id) {
    if (sess == NULL) return;

    pthread_mutex_lock(&sess->lock);
    transfer_task_t** ptr = &sess->tasks;
    while (*ptr != NULL) {
        if ((*ptr)->task_id == task_id) {
            transfer_task_t* task = *ptr;
            *ptr = task->next;

            /* 关闭文件句柄 */
            if (task->file != NULL) {
                fclose(task->file);
            }

            pthread_mutex_destroy(&task->lock);
            free(task);
            sess->task_count--;
            break;
        }
        ptr = &((*ptr)->next);
    }
    pthread_mutex_unlock(&sess->lock);
}

/* ============================================================
 * 计算传输进度百分比
 * ============================================================ */
float transfer_progress_percent(transfer_task_t* task) {
    if (task == NULL) return 0.0f;
    if (task->file_size == 0) return 100.0f;
    return (float)task->transferred / task->file_size * 100.0f;
}

/* ============================================================
 * 计算传输速度（字节/秒）
 * ============================================================ */
uint32_t transfer_speed_bps(transfer_task_t* task) {
    if (task == NULL) return 0;

    time_t elapsed = time(NULL) - task->start_time;
    if (elapsed == 0) return 0;
    return (uint32_t)(task->transferred / elapsed);
}

/* ============================================================
 * 清理会话的所有传输任务
 * ============================================================ */
void session_cleanup_tasks(client_session_t* sess) {
    if (sess == NULL) return;

    pthread_mutex_lock(&sess->lock);
    while (sess->tasks != NULL) {
        transfer_task_t* task = sess->tasks;
        sess->tasks = task->next;

        if (task->file != NULL) {
            fclose(task->file);
        }
        pthread_mutex_destroy(&task->lock);
        free(task);
        sess->task_count--;
    }
    pthread_mutex_unlock(&sess->lock);
}