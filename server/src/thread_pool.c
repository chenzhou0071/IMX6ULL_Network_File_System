/*
 * IMX6ULL FTP Server - Thread Pool Module Implementation
 * Version: 1.0
 */

#include "thread_pool.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================================================
 * 工作线程函数
 * ============================================================ */
static void* worker_thread(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;

    while (1) {
        pthread_mutex_lock(&pool->mutex);

        /* 等待任务 */
        while (pool->task_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        /* 检查是否需要退出 */
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        /* 取出一个任务 */
        if (pool->task_count > 0) {
            task_node_t* task = pool->task_queue;
            pool->task_queue = task->next;
            if (pool->task_queue == NULL) {
                pool->task_queue_end = NULL;
            }
            pool->task_count--;
            pool->active++;

            pthread_mutex_unlock(&pool->mutex);

            /* 执行任务 */
            if (task->func != NULL) {
                task->func(task->arg);
            }

            free(task);

            pthread_mutex_lock(&pool->mutex);
            pool->active--;
            pthread_mutex_unlock(&pool->mutex);
        } else {
            pthread_mutex_unlock(&pool->mutex);
        }
    }

    return NULL;
}

/* ============================================================
 * 创建线程池
 * ============================================================ */
thread_pool_t* thread_pool_create(int thread_count) {
    if (thread_count <= 0) thread_count = 4;

    thread_pool_t* pool = calloc(1, sizeof(thread_pool_t));
    if (pool == NULL) {
        LOG_ERROR("Failed to allocate thread pool");
        return NULL;
    }

    pool->thread_count = thread_count;
    pool->task_queue = NULL;
    pool->task_queue_end = NULL;
    pool->task_count = 0;
    pool->shutdown = 0;
    pool->active = 0;

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);

    /* 分配线程数组 */
    pool->threads = calloc(thread_count, sizeof(pthread_t));
    if (pool->threads == NULL) {
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->cond);
        free(pool);
        return NULL;
    }

    /* 创建工作线程 */
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            LOG_ERROR("Failed to create worker thread %d", i);
            /* 清理已创建的线程 */
            pool->shutdown = 1;
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->cond);
            free(pool);
            return NULL;
        }
    }

    LOG_INFO("Thread pool created with %d workers", thread_count);
    return pool;
}

/* ============================================================
 * 销毁线程池
 * ============================================================ */
void thread_pool_destroy(thread_pool_t* pool) {
    if (pool == NULL) return;

    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    /* 等待所有线程退出 */
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    /* 清��剩余任务 */
    pthread_mutex_lock(&pool->mutex);
    while (pool->task_count > 0) {
        task_node_t* task = pool->task_queue;
        pool->task_queue = task->next;
        free(task);
        pool->task_count--;
    }
    pthread_mutex_unlock(&pool->mutex);

    free(pool->threads);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    free(pool);

    LOG_INFO("Thread pool destroyed");
}

/* ============================================================
 * 提交任务
 * ============================================================ */
int thread_pool_submit(thread_pool_t* pool, task_func_t func, void* arg) {
    if (pool == NULL || func == NULL) return -1;

    task_node_t* task = calloc(1, sizeof(task_node_t));
    if (task == NULL) {
        LOG_ERROR("Failed to allocate task");
        return -1;
    }

    task->func = func;
    task->arg = arg;
    task->next = NULL;

    pthread_mutex_lock(&pool->mutex);

    if (pool->task_queue_end == NULL) {
        pool->task_queue = task;
        pool->task_queue_end = task;
    } else {
        pool->task_queue_end->next = task;
        pool->task_queue_end = task;
    }
    pool->task_count++;

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

/* ============================================================
 * 获取活跃线程数
 * ============================================================ */
int thread_pool_get_active_count(thread_pool_t* pool) {
    if (pool == NULL) return 0;

    pthread_mutex_lock(&pool->mutex);
    int active = pool->active;
    pthread_mutex_unlock(&pool->mutex);

    return active;
}

/* ============================================================
 * 等待所有任务完成
 * ============================================================ */
void thread_pool_wait(thread_pool_t* pool) {
    if (pool == NULL) return;

    while (1) {
        pthread_mutex_lock(&pool->mutex);
        int pending = pool->task_count;
        int active = pool->active;
        pthread_mutex_unlock(&pool->mutex);

        if (pending == 0 && active == 0) {
            break;
        }

        /* 短暂睡眠，避免忙等待 */
        usleep(10000);
    }
}