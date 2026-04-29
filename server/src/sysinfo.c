/*
 * IMX6ULL FTP Server - System Information Implementation
 * Version: 1.0
 */

#include "sysinfo.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

#ifdef _WIN32
/* Windows 不支持 /proc，返回模拟数据 */
int get_system_info(system_info_t* info) {
    if (info == NULL) return -1;

    info->cpu_usage = 25.0f;
    info->mem_total = 8192000;
    info->mem_free = 4096000;
    info->mem_available = 4096000;
    info->disk_total = 104857600;
    info->disk_free = 52428800;
    info->uptime = 3600;
    info->cpu_temp = 45;

    return 0;
}

int get_process_list(process_info_t* list, int max_count) {
    if (list == NULL || max_count <= 0) return -1;
    return 0;
}

#else
/* Linux 实现 */

/* ============================================================
 * 获取系统信息
 * ============================================================ */
int get_system_info(system_info_t* info) {
    if (info == NULL) return -1;

    memset(info, 0, sizeof(system_info_t));

    /* CPU 使用率：读取 /proc/stat 两次采样 */
    FILE* fp = fopen("/proc/stat", "r");
    if (fp) {
        unsigned long cpu_user, cpu_nice, cpu_system, cpu_idle, cpu_iowait;
        fscanf(fp, "cpu %lu %lu %lu %lu %lu", &cpu_user, &cpu_nice, &cpu_system, &cpu_idle, &cpu_iowait);
        fclose(fp);

        unsigned long total1 = cpu_user + cpu_nice + cpu_system + cpu_idle + cpu_iowait;
        unsigned long idle1 = cpu_idle;

        /* 等待1秒 */
        sleep(1);

        fp = fopen("/proc/stat", "r");
        if (fp) {
            fscanf(fp, "cpu %lu %lu %lu %lu %lu", &cpu_user, &cpu_nice, &cpu_system, &cpu_idle, &cpu_iowait);
            fclose(fp);

            unsigned long total2 = cpu_user + cpu_nice + cpu_system + cpu_idle + cpu_iowait;
            unsigned long idle2 = cpu_idle;

            unsigned long total_diff = total2 - total1;
            unsigned long idle_diff = idle2 - idle1;

            if (total_diff > 0) {
                info->cpu_usage = 100.0f * (total_diff - idle_diff) / total_diff;
            }
        }
    }

    /* 内存信息：读取 /proc/meminfo */
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line, "MemTotal: %u", &info->mem_total);
            } else if (strncmp(line, "MemFree:", 8) == 0) {
                sscanf(line, "MemFree: %u", &info->mem_free);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                sscanf(line, "MemAvailable: %u", &info->mem_available);
            }
        }
        fclose(fp);
    }

    /* 磁盘信息：statvfs */
    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        info->disk_total = (uint32_t)(vfs.f_blocks * vfs.f_frsize / 1024);
        info->disk_free = (uint32_t)(vfs.f_bfree * vfs.f_frsize / 1024);
    }

    /* 运行时间：读取 /proc/uptime */
    fp = fopen("/proc/uptime", "r");
    if (fp) {
        float uptime;
        fscanf(fp, "%f", &uptime);
        info->uptime = (uint32_t)uptime;
        fclose(fp);
    }

    /* CPU温度：读取 thermal zone */
    fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp) {
        int temp;
        fscanf(fp, "%d", &temp);
        info->cpu_temp = (uint8_t)(temp / 1000);
        fclose(fp);
    }

    return 0;
}

/* ============================================================
 * 获取进程列表
 * ============================================================ */
int get_process_list(process_info_t* list, int max_count) {
    if (list == NULL || max_count <= 0) return -1;

    DIR* proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        LOG_ERROR("Failed to open /proc");
        return -1;
    }

    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(proc_dir)) != NULL && count < max_count) {
        /* 只处理数字目录 */
        if (entry->d_type != DT_DIR) continue;
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;

        /* 读取进程信息 */
        char path[256];
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);

        FILE* fp = fopen(path, "r");
        if (fp == NULL) continue;

        /* 解析 stat 文件 */
        unsigned long utime, stime;
        long rss;
        char comm[64], state;

        fscanf(fp, "%d (%s) %c", &list[count].pid, comm, &state);
        list[count].state = state;

        /*跳过剩余字段直到找到utime和stime */
        for (int i = 0; i < 11; i++) {
            unsigned long tmp;
            fscanf(fp, "%lu", &tmp);
        }
        fscanf(fp, "%lu %lu", &utime, &stime);

        /* 获取内存信息 */
        fclose(fp);

        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        fp = fopen(path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    unsigned int uid;
                    sscanf(line, "Uid: %u", &uid);
                    struct passwd* pw = getpwuid(uid);
                    if (pw) {
                        strncpy(list[count].user, pw->pw_name, sizeof(list[count].user) - 1);
                    }
                }
            }
            fclose(fp);
        }

        /* 计算CPU使用率（简化） */
        list[count].cpu_usage = 0.0f;  /* 需要两次采样 */

        /* 获取内存大小 */
        snprintf(path, sizeof(path), "/proc/%d/statm", pid);
        fp = fopen(path, "r");
        if (fp) {
            unsigned long vsize, rss_pages;
            fscanf(fp, "%lu %lu", &vsize, &rss_pages);
            list[count].mem_size = rss_pages * 4;  /* 页数转KB */
            list[count].mem_usage = 0.0f;
            fclose(fp);
        }

        strncpy(list[count].name, comm, sizeof(list[count].name) - 1);
        snprintf(list[count].state_name, sizeof(list[count].state_name),
                 state == 'R' ? "Running" : state == 'S' ? "Sleeping" : state == 'D' ? "Disk sleep" : "Unknown");

        count++;
    }

    closedir(proc_dir);
    return count;
}

#endif