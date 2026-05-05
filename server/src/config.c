/*
 * IMX6ULL FTP Server - Configuration Module Implementation
 * Version: 1.0
 */

#include "config.h"
#include "log.h"
#include "md5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 512
#define MAX_USERS 32

/* ============================================================
 * 辅助函数：去除字符串首尾空白
 * ============================================================ */
static char* trim(char* str) {
    if (str == NULL) return NULL;

    /* 去除前导空白 */
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') {
        str++;
    }

    if (*str == '\0') return str;

    /* 去除尾部空白 */
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }

    return str;
}

/* ============================================================
 * 解析权限字符串
 * ============================================================ */
static permission_t parse_permission(const char* perm_str) {
    if (perm_str == NULL) return PERM_NONE;

    if (strcmp(perm_str, "rw") == 0 || strcmp(perm_str, "RW") == 0)
        return PERM_RW;
    if (strcmp(perm_str, "ro") == 0 || strcmp(perm_str, "RO") == 0)
        return PERM_RO;

    return PERM_NONE;
}

/* ============================================================
 * 加载配置
 * ============================================================ */
server_config_t* config_load(const char* filepath) {
    if (filepath == NULL) return NULL;

    FILE* fp = fopen(filepath, "r");
    if (fp == NULL) {
        LOG_ERROR("Failed to open config file: %s", filepath);
        return NULL;
    }

    server_config_t* config = calloc(1, sizeof(server_config_t));
    if (config == NULL) {
        fclose(fp);
        return NULL;
    }

    /* 设置默认值 */
    config->port = 8888;
    config->max_connections = 10;
    config->timeout = 30;  // 30秒心跳超时
    strcpy(config->log_level, "info");
    strcpy(config->root_dir, "/home/root");
    strcpy(config->upload_dir, "/home/root/uploads");
    strcpy(config->log_dir, "./logs");
    config->max_concurrent_transfers = 3;
    config->max_file_size = 1073741824; /* 1GB */
    config->buffer_size = 8192;

    /* 分配用户数组 */
    config->users = calloc(MAX_USERS, sizeof(user_t));
    config->user_count = 0;

    char line[MAX_LINE_LENGTH];
    char section[64] = "";

    while (fgets(line, sizeof(line), fp) != NULL) {
        char* trimmed = trim(line);

        /* 跳过空行和注释 */
        if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        /* 解析节名 */
        if (trimmed[0] == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
            }
            continue;
        }

        /* 解析键值对 */
        char* eq = strchr(trimmed, '=');
        char* key;
        char* value;

        if (eq != NULL) {
            *eq = '\0';
            key = trim(trimmed);
            value = trim(eq + 1);
        } else {
            key = NULL;
            value = trimmed;
        }

        if (strcmp(section, "server") == 0) {
            if (key && strcmp(key, "port") == 0) {
                config->port = (uint16_t)atoi(value);
            } else if (key && strcmp(key, "max_connections") == 0) {
                config->max_connections = atoi(value);
            } else if (key && strcmp(key, "timeout") == 0) {
                config->timeout = atoi(value);
            } else if (key && strcmp(key, "log_level") == 0) {
                strncpy(config->log_level, value, sizeof(config->log_level) - 1);
            }
        } else if (strcmp(section, "users") == 0) {
            /* 用户格式: username:password:perm */
            if (config->user_count < MAX_USERS) {
                char* colon1 = strchr(value, ':');
                if (colon1) {
                    user_t* user = &config->users[config->user_count];
                    *colon1 = '\0';
                    strncpy(user->username, value, sizeof(user->username) - 1);
                    char* colon2 = strchr(colon1 + 1, ':');
                    if (colon2) {
                        *colon2 = '\0';
                        char* plaintext_password = colon1 + 1;
                        md5_hex_string((uint8_t*)plaintext_password, strlen(plaintext_password), user->password);
                        user->perm = parse_permission(colon2 + 1);
                        LOG_WARN("Loaded user [%d]: username='%s', plaintext='%s', md5='%s', perm=%d",
                                 config->user_count, user->username, plaintext_password, user->password, user->perm);
                        config->user_count++;
                    }
                }
            }
        } else if (strcmp(section, "paths") == 0) {
            if (key && strcmp(key, "root_dir") == 0) {
                strncpy(config->root_dir, value, sizeof(config->root_dir) - 1);
            } else if (key && strcmp(key, "upload_dir") == 0) {
                strncpy(config->upload_dir, value, sizeof(config->upload_dir) - 1);
            } else if (key && strcmp(key, "log_dir") == 0) {
                strncpy(config->log_dir, value, sizeof(config->log_dir) - 1);
            }
        } else if (strcmp(section, "transfer") == 0) {
            if (key && strcmp(key, "max_concurrent") == 0) {
                config->max_concurrent_transfers = atoi(value);
            } else if (strcmp(key, "max_file_size") == 0) {
                config->max_file_size = (uint32_t)atoi(value);
            } else if (strcmp(key, "buffer_size") == 0) {
                config->buffer_size = atoi(value);
            }
        }
    }

    fclose(fp);

    LOG_INFO("Config loaded: port=%d, max_connections=%d, timeout=%d, users=%d",
             config->port, config->max_connections, config->timeout, config->user_count);

    return config;
}

/* ============================================================
 * 释放配置
 * ============================================================ */
void config_free(server_config_t* config) {
    if (config != NULL) {
        if (config->users != NULL) {
            free(config->users);
        }
        free(config);
    }
}

/* ============================================================
 * 验证用户
 * ============================================================ */
bool config_verify_user(const server_config_t* config,
                        const char* username,
                        const char* password_md5,
                        permission_t* perm) {
    if (config == NULL || username == NULL || password_md5 == NULL || perm == NULL) {
        return false;
    }

    LOG_WARN("Verifying user: input_username='%s', input_password_md5='%s'", username, password_md5);

    for (int i = 0; i < config->user_count; i++) {
        LOG_WARN("  Checking user[%d]: stored_username='%s', stored_password='%s'",
                 i, config->users[i].username, config->users[i].password);
        if (strcmp(config->users[i].username, username) == 0 &&
            strcmp(config->users[i].password, password_md5) == 0) {
            *perm = config->users[i].perm;
            LOG_WARN("  Match found! perm=%d", *perm);
            return true;
        }
    }

    LOG_WARN("  No match found");
    return false;
}

/* ============================================================
 * 获取根目录
 * ============================================================ */
const char* config_get_root_dir(const server_config_t* config) {
    return config ? config->root_dir : "/";
}

/* ============================================================
 * 获取上传目录
 * ============================================================ */
const char* config_get_upload_dir(const server_config_t* config) {
    return config ? config->upload_dir : "/tmp";
}