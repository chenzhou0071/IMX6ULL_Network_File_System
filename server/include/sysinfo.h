/*
 * IMX6ULL FTP Server - System Information Module
 * Version: 1.0
 */

#ifndef SYSINFO_H
#define SYSINFO_H

#include "../protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 系统信息采集函数
 * ============================================================ */

/**
 * 获取系统信息
 * @param info 输出的系统信息结构
 * @return 0成功，-1失败
 */
int get_system_info(system_info_t* info);

/**
 * 获取进程列表
 * @param list 输出的进程列表数组
 * @param max_count 最大进程数
 * @return 实际进程数，-1失败
 */
int get_process_list(process_info_t* list, int max_count);

#ifdef __cplusplus
}
#endif

#endif /* SYSINFO_H */