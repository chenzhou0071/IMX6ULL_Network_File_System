/*
 * IMX6ULL FTP Server - Log Module Implementation
 * Version: 1.0
 */

#include "log.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#define fileno _fileno
#else
#include <unistd.h>
#endif

/* ============================================================
 * 全局变量
 * ============================================================ */
static log_level_t g_log_level = LOG_LEVEL_INFO;
static FILE* g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_log_dir[256] = "./logs";
static int g_rotation_enabled = 1;

/* ============================================================
 * 级别字符串到枚举
 * ============================================================ */
static log_level_t parse_level(const char* level_str) {
    if (level_str == NULL) return LOG_LEVEL_INFO;

    if (strcmp(level_str, "debug") == 0 || strcmp(level_str, "DEBUG") == 0)
        return LOG_LEVEL_DEBUG;
    if (strcmp(level_str, "info") == 0 || strcmp(level_str, "INFO") == 0)
        return LOG_LEVEL_INFO;
    if (strcmp(level_str, "warn") == 0 || strcmp(level_str, "WARN") == 0 ||
        strcmp(level_str, "warning") == 0 || strcmp(level_str, "WARNING") == 0)
        return LOG_LEVEL_WARN;
    if (strcmp(level_str, "error") == 0 || strcmp(level_str, "ERROR") == 0)
        return LOG_LEVEL_ERROR;

    return LOG_LEVEL_INFO;
}

/* ============================================================
 * 获取级别字符串
 * ============================================================ */
static const char* level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

/* ============================================================
 * 获取当前日期字符串
 * ============================================================ */
static void get_date_string(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d", tm_info);
}

/* ============================================================
 * 打开日志文件（按日期滚动）
 * ============================================================ */
static int open_log_file(const char* log_dir) {
    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    /* 创建日志目录 */
#ifdef _WIN32
    mkdir(log_dir);
#else
    mkdir(log_dir, 0755);
#endif

    char filepath[512];
    char date_str[32];
    get_date_string(date_str, sizeof(date_str));

    snprintf(filepath, sizeof(filepath), "%s/server_%s.log", log_dir, date_str);

    g_log_file = fopen(filepath, "a");
    if (g_log_file == NULL) {
        fprintf(stderr, "[LOG] Failed to open log file: %s\n", filepath);
        return -1;
    }

    return 0;
}

/* ============================================================
 * 公共函数实现
 * ============================================================ */

void log_init(const char* level, const char* log_file) {
    pthread_mutex_lock(&g_log_mutex);

    g_log_level = parse_level(level);

    if (log_file != NULL) {
        /* 使用指定的日志文件路径 */
        char dir[256];
        strncpy(dir, log_file, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';

        /* 提取目录部分 */
        char* last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            strncpy(g_log_dir, dir, sizeof(g_log_dir) - 1);
        }

        g_log_file = fopen(log_file, "a");
        if (g_log_file == NULL) {
            fprintf(stderr, "[LOG] Failed to open log file: %s\n", log_file);
        }
    } else {
        /* 使用默认路径 ./logs/server_YYYY-MM-DD.log */
        open_log_file(g_log_dir);
    }

    pthread_mutex_unlock(&g_log_mutex);

    LOG_INFO("Log system initialized, level=%s", level_to_string(g_log_level));
}

void log_close(void) {
    pthread_mutex_lock(&g_log_mutex);

    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    pthread_mutex_unlock(&g_log_mutex);
}

void log_set_level(log_level_t level) {
    g_log_level = level;
}

void log_write(log_level_t level, const char* file, int line, const char* fmt, ...) {
    if (level < g_log_level) return;

    va_list args;
    char timestamp[32];
    char message[1024];

    /* 获取时间戳 */
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    /* 格式化消息 */
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 提取文件名 */
    const char* filename = strrchr(file, '/');
    if (filename == NULL) filename = strrchr(file, '\\');
    if (filename) filename++;
    else filename = file;

    /* 构建日志行 */
    char log_line[2048];
    int len = snprintf(log_line, sizeof(log_line),
                       "[%s] [%s] [%s:%d] %s\n",
                       timestamp, level_to_string(level), filename, line, message);

    pthread_mutex_lock(&g_log_mutex);

    /* 输出到控制台（stderr） */
    fwrite(log_line, 1, strlen(log_line), stderr);
    fflush(stderr);

    /* 输出到日志文件 */
    if (g_log_file != NULL) {
        fwrite(log_line, 1, strlen(log_line), g_log_file);
        fflush(g_log_file);
    }

    pthread_mutex_unlock(&g_log_mutex);
}