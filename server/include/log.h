/*
 * IMX6ULL FTP Server - Log Module Header
 * Version: 1.0
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 日志级别
 * ============================================================ */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

/* ============================================================
 * 初始化和清理
 * ============================================================ */

/**
 * 初始化日志系统
 * @param level 日志级别字符串: debug/info/warn/error
 * @param log_file 日志文件路径，为NULL则只输出到stderr
 */
void log_init(const char* level, const char* log_file);

/**
 * 关闭日志系统
 */
void log_close(void);

/**
 * 设置日志级别
 */
void log_set_level(log_level_t level);

/* ============================================================
 * 日志宏
 * ============================================================ */
#define LOG_DEBUG(...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_write(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

/* ============================================================
 * 日志输出函数
 * ============================================================ */
void log_write(log_level_t level, const char* file, int line, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */