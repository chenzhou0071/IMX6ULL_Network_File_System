/*
 * IMX6ULL FTP Server - Thread Pool Module Header
 * Version: 1.0
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 任务类型
 * ============================================================ */
typedef void (*task_func_t)(void* arg);

/* ============================================================
 * 任务节点
 * ============================================================ */
typedef struct task_node {
    task_func_t func;
    void* arg;
    struct task_node* next;
} task_node_t;

/* ============================================================
 * 线程池结构
 * ============================================================ */
typedef struct {
    pthread_t* threads;           /* 工作线程数组 */
    int thread_count;            /* 线程数量 */
    task_node_t* task_queue;     /* 任务队列 */
    task_node_t* task_queue_end;  /* 队列尾指针 */
    int task_count;              /* 当前任务数 */

    pthread_mutex_t mutex;        /* 互斥锁 */
    pthread_cond_t cond;          /* 条件变量 */
    int shutdown;                 /* 关闭标志 */
    int active;                   /* 活跃线程数 */
} thread_pool_t;

/* ============================================================
 * 线程池函数
 * ============================================================ */

/**
 * 创建线程池
 * @param thread_count 工作线程数量
 * @return 线程池指针，失败返回NULL
 */
thread_pool_t* thread_pool_create(int thread_count);

/**
 * 销毁线程池
 * @param pool 线程池指针
 */
void thread_pool_destroy(thread_pool_t* pool);

/**
 * 提交任务
 * @param pool 线程池指针
 * @param func 任务函数
 * @param arg 任务参数
 * @return 0成功，-1失败
 */
int thread_pool_submit(thread_pool_t* pool, task_func_t func, void* arg);

/**
 * 获取活跃线程数
 */
int thread_pool_get_active_count(thread_pool_t* pool);

/**
 * 等待所有任务完成
 */
void thread_pool_wait(thread_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif /* THREAD_POOL_H */