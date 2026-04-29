/*
 * IMX6ULL FTP Server - Configuration Module Header
 * Version: 1.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 权限类型
 * ============================================================ */
typedef enum {
    PERM_NONE = 0,
    PERM_RO = 1,    /* 只读 */
    PERM_RW = 2     /* 读写 */
} permission_t;

/* ============================================================
 * 用户结构
 * ============================================================ */
typedef struct {
    char username[64];
    char password[64];       /* 存储MD5值 */
    permission_t perm;
} user_t;

/* ============================================================
 * 服务器配置结构
 * ============================================================ */
typedef struct {
    /* 服务器配置 */
    uint16_t port;
    int max_connections;
    int timeout;             /* 秒 */
    char log_level[16];

    /* 用户数量和数组 */
    int user_count;
    user_t* users;

    /* 路径配置 */
    char root_dir[256];
    char upload_dir[256];
    char log_dir[256];

    /* 传输配置 */
    int max_concurrent_transfers;
    uint32_t max_file_size;
    int buffer_size;
} server_config_t;

/* ============================================================
 * 配置加载/释放函数
 * ============================================================ */

/**
 * 加载配置文件
 * @param filepath 配置文件路径
 * @return 配置结构指针，失败返回NULL
 */
server_config_t* config_load(const char* filepath);

/**
 * 释放配置内存
 */
void config_free(server_config_t* config);

/**
 * 验证用户
 * @param config 配置
 * @param username 用户名
 * @param password_md5 密码的MD5值
 * @param perm 输出权限
 * @return true成功，false失败
 */
bool config_verify_user(const server_config_t* config,
                        const char* username,
                        const char* password_md5,
                        permission_t* perm);

/**
 * 获取根目录
 */
const char* config_get_root_dir(const server_config_t* config);

/**
 * 获取上传目录
 */
const char* config_get_upload_dir(const server_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */