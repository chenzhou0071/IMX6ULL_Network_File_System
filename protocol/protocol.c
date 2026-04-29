/*
 * IMX6ULL FTP Server - Protocol Implementation
 * Version: 1.0
 */

#include "protocol.h"
#include <string.h>

/* ============================================================
 * 函数实现
 * ============================================================ */

/**
 * 计算CRC32校验和
 */
uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/**
 * 封装帧
 * @param cmd 命令字
 * @param seq 序列号
 * @param data 数据载荷
 * @param data_len 数据长度
 * @param out_buf 输出缓冲区
 * @param out_size 输出缓冲区大小
 * @return 0成功，-1失败
 */
int protocol_pack(uint16_t cmd, uint32_t seq,
                  const void* data, size_t data_len,
                  uint8_t* out_buf, size_t out_size) {
    if (out_buf == NULL) return -1;
    if (data_len > MAX_PAYLOAD_SIZE) return -1;
    if (out_size < sizeof(protocol_frame_t) + data_len) return -1;

    protocol_frame_t* frame = (protocol_frame_t*)out_buf;

    frame->magic = PROTOCOL_MAGIC;
    frame->version = PROTOCOL_VERSION;
    frame->cmd = cmd;
    frame->seq = seq;
    frame->length = (uint32_t)data_len;

    /* 计算校验和 */
    if (data && data_len > 0) {
        memcpy(out_buf + sizeof(protocol_frame_t), data, data_len);
        frame->checksum = crc32(data, data_len);
    } else {
        frame->checksum = 0;
    }

    return 0;
}

/**
 * 解帧
 * @param buf 输入数据
 * @param len 输入数据长度
 * @param frame 输出的帧头
 * @param payload 输出的载荷数据
 * @param payload_size 载荷缓冲区大小
 * @return 0成功，-1失败
 */
int protocol_unpack(const uint8_t* buf, size_t len,
                    protocol_frame_t* frame, uint8_t* payload,
                    size_t payload_size) {
    if (buf == NULL || frame == NULL) return -1;
    if (len < sizeof(protocol_frame_t)) return -1;

    protocol_frame_t* f = (protocol_frame_t*)buf;

    /* 检查魔数 */
    if (f->magic != PROTOCOL_MAGIC) return -1;

    /* 复制帧头 */
    memcpy(frame, f, sizeof(protocol_frame_t));

    /* 检查数据长度 */
    if (frame->length > MAX_PAYLOAD_SIZE) return -1;
    if (sizeof(protocol_frame_t) + frame->length > len) return -1;

    /* 复制载荷 */
    if (payload && payload_size >= frame->length && frame->length > 0) {
        memcpy(payload, buf + sizeof(protocol_frame_t), frame->length);

        /* 验证校验和 */
        uint32_t calc_checksum = crc32(payload, frame->length);
        if (calc_checksum != frame->checksum) return -1;
    }

    return 0;
}

/**
 * 获取错误描述
 */
const char* protocol_err_str(int err) {
    switch (err) {
        case ERR_SUCCESS:          return "Success";
        case ERR_FAILED:            return "Operation failed";
        case ERR_AUTH_FAILED:       return "Authentication failed";
        case ERR_PERMISSION_DENIED:  return "Permission denied";
        case ERR_NOT_AUTHENTICATED: return "Not authenticated";
        case ERR_FILE_NOT_FOUND:    return "File not found";
        case ERR_FILE_EXISTS:       return "File already exists";
        case ERR_FILE_TOO_LARGE:    return "File too large";
        case ERR_DISK_FULL:         return "Disk full";
        case ERR_INVALID_PATH:      return "Invalid path";
        case ERR_RESOURCE_BUSY:      return "Resource busy";
        case ERR_TRANSFER_ABORTED:  return "Transfer aborted";
        case ERR_SERVER_INTERNAL:   return "Server internal error";
        default:                    return "Unknown error";
    }
}

/**
 * 获取命令描述
 */
const char* protocol_cmd_str(uint16_t cmd) {
    switch (cmd) {
        case CMD_LOGIN:                   return "LOGIN";
        case CMD_LOGIN_RESP:               return "LOGIN_RESP";
        case CMD_LOGOUT:                   return "LOGOUT";
        case CMD_LOGOUT_RESP:              return "LOGOUT_RESP";
        case CMD_FILE_LIST:                return "FILE_LIST";
        case CMD_FILE_LIST_RESP:           return "FILE_LIST_RESP";
        case CMD_FILE_READ:                return "FILE_READ";
        case CMD_FILE_READ_RESP:           return "FILE_READ_RESP";
        case CMD_FILE_DELETE:              return "FILE_DELETE";
        case CMD_FILE_DELETE_RESP:         return "FILE_DELETE_RESP";
        case CMD_FILE_RENAME:              return "FILE_RENAME";
        case CMD_FILE_RENAME_RESP:         return "FILE_RENAME_RESP";
        case CMD_FILE_UPLOAD_START:        return "FILE_UPLOAD_START";
        case CMD_FILE_UPLOAD_START_RESP:   return "FILE_UPLOAD_START_RESP";
        case CMD_FILE_UPLOAD_DATA:         return "FILE_UPLOAD_DATA";
        case CMD_FILE_UPLOAD_DATA_RESP:     return "FILE_UPLOAD_DATA_RESP";
        case CMD_FILE_UPLOAD_PAUSE:        return "FILE_UPLOAD_PAUSE";
        case CMD_FILE_UPLOAD_PAUSE_RESP:   return "FILE_UPLOAD_PAUSE_RESP";
        case CMD_FILE_UPLOAD_RESUME:       return "FILE_UPLOAD_RESUME";
        case CMD_FILE_UPLOAD_RESUME_RESP:  return "FILE_UPLOAD_RESUME_RESP";
        case CMD_FILE_UPLOAD_END:          return "FILE_UPLOAD_END";
        case CMD_FILE_UPLOAD_END_RESP:     return "FILE_UPLOAD_END_RESP";
        case CMD_FILE_DOWNLOAD_START:      return "FILE_DOWNLOAD_START";
        case CMD_FILE_DOWNLOAD_START_RESP:  return "FILE_DOWNLOAD_START_RESP";
        case CMD_FILE_DOWNLOAD_DATA:        return "FILE_DOWNLOAD_DATA";
        case CMD_FILE_DOWNLOAD_DATA_RESP:   return "FILE_DOWNLOAD_DATA_RESP";
        case CMD_FILE_DOWNLOAD_PAUSE:      return "FILE_DOWNLOAD_PAUSE";
        case CMD_FILE_DOWNLOAD_PAUSE_RESP: return "FILE_DOWNLOAD_PAUSE_RESP";
        case CMD_FILE_DOWNLOAD_RESUME:      return "FILE_DOWNLOAD_RESUME";
        case CMD_FILE_DOWNLOAD_RESUME_RESP: return "FILE_DOWNLOAD_RESUME_RESP";
        case CMD_FILE_DOWNLOAD_END:        return "FILE_DOWNLOAD_END";
        case CMD_FILE_DOWNLOAD_END_RESP:   return "FILE_DOWNLOAD_END_RESP";
        case CMD_SYS_INFO:                 return "SYS_INFO";
        case CMD_SYS_INFO_RESP:            return "SYS_INFO_RESP";
        case CMD_PROC_LIST:                return "PROC_LIST";
        case CMD_PROC_LIST_RESP:           return "PROC_LIST_RESP";
        case CMD_HEARTBEAT:                return "HEARTBEAT";
        case CMD_HEARTBEAT_RESP:           return "HEARTBEAT_RESP";
        default:                           return "UNKNOWN";
    }
}