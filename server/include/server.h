/*
 * IMX6ULL FTP Server - Main Header
 * Version: 1.0
 */

#ifndef SERVER_MAIN_H
#define SERVER_MAIN_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "../protocol/protocol.h"
#include "log.h"
#include "config.h"
#include "network.h"
#include "thread_pool.h"
#include "session.h"
#include "handler.h"

/* ============================================================
 * 全局配置
 * ============================================================ */
extern server_config_t* g_config;
extern thread_pool_t* g_thread_pool;
extern volatile int g_running;

/* ============================================================
 * 信号处理
 * ============================================================ */
void signal_handler(int sig);

#endif /* SERVER_MAIN_H */