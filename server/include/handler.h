/*
 * IMX6ULL FTP Server - Client Handler Module Header
 * Version: 1.0
 */

#ifndef HANDLER_H
#define HANDLER_H

#include "session.h"
#include "../protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 客户端处理函数
 * ============================================================ */

/**
 * 处理客户端连接
 * @param arg client_session_t* 参数
 */
void handle_client(void* arg);

/* ============================================================
 * 命令处理函数
 * ============================================================ */
void handle_login(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_logout(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_file_list(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_file_read(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_file_delete(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_file_rename(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_upload_start(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_upload_data(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_upload_pause(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_upload_resume(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_upload_end(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_download_start(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_download_data(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_download_pause(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_download_resume(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_download_end(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_sys_info(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_proc_list(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);
void handle_heartbeat(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload);

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * 发送响应
 */
int send_response(client_session_t* sess, uint16_t cmd, uint16_t seq,
                  uint8_t status, const void* data, size_t data_len);

/**
 * 发送错误响应
 */
int send_error(client_session_t* sess, uint16_t resp_cmd, uint16_t seq,
               uint8_t err_code, const char* message);

/**
 * 检查路径安全性
 */
int check_path_safety(const char* path);

/**
 * 拼接完整路径
 */
void build_full_path(char* dest, size_t dest_size, const char* base, const char* relative);

#ifdef __cplusplus
}
#endif

#endif /* HANDLER_H */