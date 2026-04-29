/*
 * IMX6ULL FTP Server - Protocol Header
 * Version: 1.0
 *
 * 协议定义：自定义二进制帧格式
 * 字节序：统一使用小端序（IMX6ULL和x86都是小端，无需转换）
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */
#define PROTOCOL_MAGIC          0x46545053  /* 'FTPS' */
#define PROTOCOL_VERSION        1
#define MAX_PAYLOAD_SIZE        8192        /* 最大载荷8KB */
#define MAX_FILENAME_LENGTH     256
#define MAX_PATH_LENGTH         512
#define MAX_USERNAME_LENGTH     64
#define MAX_PASSWORD_LENGTH     64

/* ============================================================
 * 帧头结构 (12字节)
 * ============================================================ */
typedef struct __attribute__((packed)) {
    uint32_t magic;      /* 魔数 0x46545053 */
    uint16_t version;    /* 协议版本 */
    uint16_t cmd;        /* 命令字 */
    uint32_t seq;        /* 序列号 */
    uint32_t length;     /* 数据长度（不含帧头） */
    uint32_t checksum;   /* CRC32校验和 */
} protocol_frame_t;

/* ============================================================
 * 命令字枚举
 * ============================================================ */
typedef enum {
    /* 认证相关 0x01xx */
    CMD_LOGIN             = 0x0101,
    CMD_LOGIN_RESP        = 0x0102,
    CMD_LOGOUT            = 0x0103,
    CMD_LOGOUT_RESP       = 0x0104,

    /* 文件操作 0x02xx */
    CMD_FILE_LIST         = 0x0201,
    CMD_FILE_LIST_RESP    = 0x0202,
    CMD_FILE_READ         = 0x0203,
    CMD_FILE_READ_RESP    = 0x0204,
    CMD_FILE_DELETE       = 0x0205,
    CMD_FILE_DELETE_RESP  = 0x0206,
    CMD_FILE_RENAME       = 0x0207,
    CMD_FILE_RENAME_RESP  = 0x0208,

    /* 文件传输 0x03xx */
    CMD_FILE_UPLOAD_START         = 0x0301,
    CMD_FILE_UPLOAD_START_RESP    = 0x0302,
    CMD_FILE_UPLOAD_DATA          = 0x0303,
    CMD_FILE_UPLOAD_DATA_RESP      = 0x0304,
    CMD_FILE_UPLOAD_PAUSE          = 0x0305,
    CMD_FILE_UPLOAD_PAUSE_RESP     = 0x0306,
    CMD_FILE_UPLOAD_RESUME         = 0x0307,
    CMD_FILE_UPLOAD_RESUME_RESP    = 0x0308,
    CMD_FILE_UPLOAD_END            = 0x0309,
    CMD_FILE_UPLOAD_END_RESP       = 0x030A,

    CMD_FILE_DOWNLOAD_START        = 0x030B,
    CMD_FILE_DOWNLOAD_START_RESP    = 0x030C,
    CMD_FILE_DOWNLOAD_DATA          = 0x030D,
    CMD_FILE_DOWNLOAD_DATA_RESP     = 0x030E,
    CMD_FILE_DOWNLOAD_PAUSE         = 0x030F,
    CMD_FILE_DOWNLOAD_PAUSE_RESP    = 0x0310,
    CMD_FILE_DOWNLOAD_RESUME        = 0x0311,
    CMD_FILE_DOWNLOAD_RESUME_RESP   = 0x0312,
    CMD_FILE_DOWNLOAD_END           = 0x0313,
    CMD_FILE_DOWNLOAD_END_RESP      = 0x0314,

    /* 系统信息 0x04xx */
    CMD_SYS_INFO         = 0x0401,
    CMD_SYS_INFO_RESP    = 0x0402,
    CMD_PROC_LIST        = 0x0403,
    CMD_PROC_LIST_RESP   = 0x0404,

    /* 心跳 0x05xx */
    CMD_HEARTBEAT        = 0x0501,
    CMD_HEARTBEAT_RESP   = 0x0502
} protocol_cmd_t;

/* ============================================================
 * 错误码枚举
 * ============================================================ */
typedef enum {
    ERR_SUCCESS              = 0,
    ERR_FAILED               = 1,
    ERR_AUTH_FAILED          = 10,
    ERR_PERMISSION_DENIED    = 11,
    ERR_NOT_AUTHENTICATED    = 12,
    ERR_FILE_NOT_FOUND       = 20,
    ERR_FILE_EXISTS          = 21,
    ERR_FILE_TOO_LARGE       = 22,
    ERR_DISK_FULL            = 23,
    ERR_INVALID_PATH         = 24,
    ERR_RESOURCE_BUSY        = 30,
    ERR_TRANSFER_ABORTED     = 31,
    ERR_SERVER_INTERNAL      = 99
} protocol_err_t;

/* ============================================================
 * 数据结构定义
 * ============================================================ */

/* 登录请求 */
typedef struct __attribute__((packed)) {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];  /* MD5值 */
} login_request_t;

/* 登录响应 */
typedef struct __attribute__((packed)) {
    uint8_t status;         /* 0成功，1失败 */
    uint8_t permission;     /* 0=ro, 1=rw */
    char message[64];        /* 状态消息 */
} login_response_t;

/* 文件信息 */
typedef struct __attribute__((packed)) {
    char name[MAX_FILENAME_LENGTH];
    uint32_t size;
    uint32_t mode;           /* 文件类型和权限 */
    uint32_t mtime;         /* 修改时间（Unix时间戳） */
    uint8_t is_dir;         /* 是否是目录 */
    uint8_t padding[3];     /* 对齐填充 */
} file_info_t;

/* 文件列表响应 */
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint8_t count;          /* 文件数量（最多255个） */
    char message[64];
    /* 后面跟着 file_info_t 数组 */
} file_list_response_t;

/* 文件读取响应 */
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint32_t size;          /* 文件大小 */
    char message[64];
    /* 后面跟着文件内容 */
} file_read_response_t;

/* 文件操作请求（删除/重命名） */
typedef struct __attribute__((packed)) {
    char path[MAX_PATH_LENGTH];
} file_request_t;

/* 文件操作响应 */
typedef struct __attribute__((packed)) {
    uint8_t status;
    char message[64];
} file_response_t;

/* 文件传输开始请求 */
typedef struct __attribute__((packed)) {
    char remote_path[MAX_PATH_LENGTH];
    uint32_t file_size;     /* 文件大小，0表示未知 */
} transfer_start_request_t;

/* 文件传输开始响应 */
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint32_t task_id;       /* 传输任务ID */
    uint32_t file_size;
    char message[64];
} transfer_start_response_t;

/* 文件传输数据 */
typedef struct __attribute__((packed)) {
    uint32_t task_id;
    uint32_t offset;        /* 数据偏移 */
    uint32_t data_len;      /* 数据长度 */
    /* 后面跟着实际数据 */
} transfer_data_t;

/* 文件传输控制响应 */
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint32_t task_id;
    uint32_t transferred;   /* 已传输字节数 */
    char message[64];
} transfer_control_response_t;

/* 系统信息 */
typedef struct __attribute__((packed)) {
    float cpu_usage;        /* CPU使用率 0-100% */
    uint32_t mem_total;    /* 总内存 KB */
    uint32_t mem_free;     /* 空闲内存 KB */
    uint32_t mem_available;
    uint32_t disk_total;   /* 磁盘总空间 KB */
    uint32_t disk_free;    /* 磁盘空闲 KB */
    uint32_t uptime;       /* 运行时间 秒 */
    uint8_t cpu_temp;      /* CPU温度（如果可获取） */
    uint8_t padding[3];
} system_info_t;

/* 进程信息 */
typedef struct __attribute__((packed)) {
    uint32_t pid;
    char name[64];
    char user[32];
    float cpu_usage;        /* CPU使用率 */
    float mem_usage;        /* 内存使用率 */
    uint8_t state;          /* 进程状态 */
    char state_name[8];
    uint32_t mem_size;      /* 内存大小 KB */
} process_info_t;

/* 进程列表响应 */
typedef struct __attribute__((packed)) {
    uint8_t status;
    uint16_t count;         /* 进程数量 */
    /* 后面跟着 process_info_t 数组 */
} process_list_response_t;

/* 心跳请求 */
typedef struct __attribute__((packed)) {
    uint32_t client_time;
} heartbeat_request_t;

/* 心跳响应 */
typedef struct __attribute__((packed)) {
    uint32_t server_time;
    uint8_t status;
} heartbeat_response_t;

/* ============================================================
 * 通用响应结构
 * ============================================================ */
typedef struct __attribute__((packed)) {
    uint8_t status;
    char message[64];
} generic_response_t;

/* ============================================================
 * 路径安全宏
 * ============================================================ */
#define IS_PATH_SAFE(path) (strstr((path), "..") == NULL)

/* ============================================================
 * 辅助函数声明
 * ============================================================ */

/* 封装帧 */
int protocol_pack(uint16_t cmd, uint32_t seq,
                  const void* data, size_t data_len,
                  uint8_t* out_buf, size_t out_size);

/* 解帧 */
int protocol_unpack(const uint8_t* buf, size_t len,
                    protocol_frame_t* frame, uint8_t* payload,
                    size_t payload_size);

/* CRC32校验 */
uint32_t crc32(const uint8_t* data, size_t len);

/* 获取错误描述 */
const char* protocol_err_str(int err);

/* 获取命令描述 */
const char* protocol_cmd_str(uint16_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */