/*
 * IMX6ULL FTP Server - Client Handler Implementation
 * Version: 1.0
 */

#include "handler.h"
#include "server.h"
#include "sysinfo.h"
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

/* ============================================================
 * 接收帧数据
 * ============================================================ */
static int recv_frame(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    uint8_t buf[MAX_PAYLOAD_SIZE + sizeof(protocol_frame_t)];
    int total = sizeof(protocol_frame_t);

    /* 先接收帧头 */
    int received = tcp_recv(sess->sockfd, buf, sizeof(protocol_frame_t), 5000);
    if (received <= 0) {
        LOG_DEBUG("Failed to receive frame header, received=%d", received);
        return -1;
    }

    // 打印收到的原始数据（十六进制）
    char hex_buf[256] = {0};
    char tmp[8];
    for (int i = 0; i < received && i < 64; i++) {
        snprintf(tmp, sizeof(tmp), "%02X ", buf[i]);
        strncat(hex_buf, tmp, sizeof(hex_buf) - strlen(hex_buf) - 1);
    }
    LOG_DEBUG("Received frame header %d bytes: %s", received, hex_buf);

    /* 先解析帧头获取数据长度（不验证校验和） */
    protocol_frame_t* f = (protocol_frame_t*)buf;
    if (f->magic != PROTOCOL_MAGIC) {
        LOG_WARN("Invalid magic: 0x%08X", f->magic);
        return -1;
    }

    uint32_t data_len = f->length;
    uint32_t frame_seq = f->seq;
    uint16_t frame_cmd = f->cmd;
    uint32_t frame_checksum = f->checksum;  // 直接从结构体读取checksum字段  // checksum在偏移12的位置

    LOG_DEBUG("Frame: cmd=0x%04X, seq=%u, len=%u, checksum=0x%08X",
              frame_cmd, frame_seq, data_len, frame_checksum);

    /* 接收完整数据：帧头(20) + 数据(len) */
    uint32_t need_len = sizeof(protocol_frame_t) + data_len;
    LOG_DEBUG("Need total %u bytes, received %d, need more %u", need_len, received, need_len - received);

    /* 如果数据不完整，继续接收 */
    if (need_len > (uint32_t)received) {
        int to_recv = need_len - received;
        int recved = tcp_recv(sess->sockfd, buf + received, to_recv, 5000);
        if (recved <= 0) {
            LOG_DEBUG("Failed to receive remaining data");
            return -1;
        }
        received += recved;
        LOG_DEBUG("Now have %d bytes", received);
    }

    /* 填充 frame 结构 */
    frame->magic = PROTOCOL_MAGIC;
    frame->version = 1;
    frame->cmd = frame_cmd;
    frame->seq = frame_seq;
    frame->length = data_len;
    frame->checksum = frame_checksum;

    /* 验证校验和 */
    uint8_t* payload_data = buf + sizeof(protocol_frame_t);

    LOG_WARN("Calculating CRC32 on %u bytes starting at offset %u", data_len, sizeof(protocol_frame_t));
    LOG_WARN("buf total size: %d, payload_data points to byte %d", received, (int)(payload_data - buf));

    uint32_t calc_checksum = crc32(payload_data, data_len);

    LOG_DEBUG("Payload data (first 32 bytes):");
    char debug_buf1[256] = {0};
    for (int i = 0; i < 64; i++) {
        snprintf(tmp, sizeof(tmp), "%02X ", payload_data[i]);
        strncat(debug_buf1, tmp, sizeof(debug_buf1) - strlen(debug_buf1) - 1);
    }
    LOG_WARN("Payload bytes 0-63: %s", debug_buf1);

    char debug_buf2[256] = {0};
    for (int i = 64; i < data_len; i++) {
        snprintf(tmp, sizeof(tmp), "%02X ", payload_data[i]);
        strncat(debug_buf2, tmp, sizeof(debug_buf2) - strlen(debug_buf2) - 1);
    }
    LOG_WARN("Payload bytes 64-127: %s", debug_buf2);

    if (calc_checksum != frame_checksum) {
        LOG_WARN("Checksum mismatch: expected 0x%08X, got 0x%08X", frame_checksum, calc_checksum);
        return -1;
    }

    /* 复制载荷到���出缓冲区 */
    if (payload && data_len > 0 && data_len > 0) {
        memcpy(payload, payload_data, data_len);
    }

    LOG_DEBUG("Frame parsed successfully");
    return 0;
}

/* ============================================================
 * 发送响应
 * ============================================================ */
int send_response(client_session_t* sess, uint16_t cmd, uint16_t seq,
                  uint8_t status, const void* data, size_t data_len) {
    uint8_t buf[MAX_PAYLOAD_SIZE + sizeof(protocol_frame_t)];

    /* 构建响应数据 */
    generic_response_t resp;
    resp.status = status;
    snprintf(resp.message, sizeof(resp.message), "%s", protocol_err_str(status));

    size_t total_len = sizeof(generic_response_t);
    if (data && data_len > 0) {
        total_len = data_len;
    }

    /* 封装帧 */
    if (protocol_pack(cmd, seq, data ? data : &resp, total_len, buf, sizeof(buf)) != 0) {
        LOG_ERROR("Failed to pack response frame");
        return -1;
    }

    /* 发送 */
    int sent = tcp_send(sess->sockfd, buf, sizeof(protocol_frame_t) + total_len);
    if (sent <= 0) {
        LOG_ERROR("Failed to send response");
        return -1;
    }

    return 0;
}

/* ============================================================
 * 发送错误响应
 * ============================================================ */
int send_error(client_session_t* sess, uint16_t resp_cmd, uint16_t seq,
               uint8_t err_code, const char* message) {
    generic_response_t resp;
    resp.status = err_code;
    snprintf(resp.message, sizeof(resp.message), "%s", message ? message : protocol_err_str(err_code));

    return send_response(sess, resp_cmd, seq, err_code, &resp, sizeof(resp));
}

/* ============================================================
 * 检查路径安全性
 * ============================================================ */
int check_path_safety(const char* path) {
    if (path == NULL) return -1;
    if (strstr(path, "..") != NULL) return -1;
    if (path[0] == '/' && strncmp(path, "/..", 3) == 0) return -1;
    return 0;
}

/* ============================================================
 * 拼接完整路径
 * ============================================================ */
void build_full_path(char* dest, size_t dest_size, const char* base, const char* relative) {
    if (relative[0] == '/') {
        /* 绝对路径 */
        snprintf(dest, dest_size, "%s%s", base, relative);
    } else {
        /* 相对路径 */
        snprintf(dest, dest_size, "%s/%s", base, relative);
    }
}

/* ============================================================
 * 客户端处理主函数
 * ============================================================ */
void handle_client(void* arg) {
    client_session_t* sess = (client_session_t*)arg;
    if (sess == NULL) return;

    LOG_INFO("Handling client: session_id=%u", sess->session_id);

    protocol_frame_t frame;
    uint8_t payload[MAX_PAYLOAD_SIZE];

    /* 循环处理请求 */
    while (g_running && sess->is_authenticated || !sess->is_authenticated) {
        /* 接收帧 */
        if (recv_frame(sess, &frame, payload) != 0) {
            break;
        }

        /* 更新活跃时间 */
        session_update_activity(sess);

        LOG_DEBUG("Received command: %s (0x%04X), seq=%u",
                  protocol_cmd_str(frame.cmd), frame.cmd, frame.seq);

        /* 根据命令字分发处理 */
        switch (frame.cmd) {
            case CMD_LOGIN:
                handle_login(sess, &frame, payload);
                break;

            case CMD_LOGOUT:
                handle_logout(sess, &frame, payload);
                break;

            case CMD_FILE_LIST:
                handle_file_list(sess, &frame, payload);
                break;

            case CMD_FILE_READ:
                handle_file_read(sess, &frame, payload);
                break;

            case CMD_FILE_DELETE:
                handle_file_delete(sess, &frame, payload);
                break;

            case CMD_FILE_RENAME:
                handle_file_rename(sess, &frame, payload);
                break;

            case CMD_FILE_EDIT:
                handle_file_edit(sess, &frame, payload);
                break;

            case CMD_FILE_UPLOAD_START:
                handle_upload_start(sess, &frame, payload);
                break;

            case CMD_FILE_UPLOAD_DATA:
                handle_upload_data(sess, &frame, payload);
                break;

            case CMD_FILE_UPLOAD_PAUSE:
                handle_upload_pause(sess, &frame, payload);
                break;

            case CMD_FILE_UPLOAD_RESUME:
                handle_upload_resume(sess, &frame, payload);
                break;

            case CMD_FILE_UPLOAD_END:
                handle_upload_end(sess, &frame, payload);
                break;

            case CMD_FILE_DOWNLOAD_START:
                handle_download_start(sess, &frame, payload);
                break;

            case CMD_FILE_DOWNLOAD_DATA:
                handle_download_data(sess, &frame, payload);
                break;

            case CMD_FILE_DOWNLOAD_PAUSE:
                handle_download_pause(sess, &frame, payload);
                break;

            case CMD_FILE_DOWNLOAD_RESUME:
                handle_download_resume(sess, &frame, payload);
                break;

            case CMD_FILE_DOWNLOAD_END:
                handle_download_end(sess, &frame, payload);
                break;

            case CMD_SYS_INFO:
                handle_sys_info(sess, &frame, payload);
                break;

            case CMD_PROC_LIST:
                handle_proc_list(sess, &frame, payload);
                break;

            case CMD_HEARTBEAT:
                handle_heartbeat(sess, &frame, payload);
                break;

            default:
                LOG_WARN("Unknown command: 0x%04X", frame.cmd);
                send_error(sess, frame.cmd + 1, frame.seq, ERR_FAILED, "Unknown command");
                break;
        }

        /* 登出后退出循环 */
        if (!sess->is_authenticated && frame.cmd != CMD_LOGIN) {
            break;
        }
    }

    /* 清理会话 */
    session_destroy(sess);
    LOG_INFO("Client handler finished: session_id=%u", sess->session_id);
}

/* ============================================================
 * 命令处理函数（阶段四实现）
 * ============================================================ */

void handle_login(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    login_request_t* req = (login_request_t*)payload;

    LOG_WARN("Login attempt: username='%s', password_md5='%s'", req->username, req->password);

    /* 验证用户 */
    permission_t perm;
    if (!config_verify_user(g_config, req->username, req->password, &perm)) {
        login_response_t resp;
        resp.status = ERR_AUTH_FAILED;
        resp.permission = PERM_NONE;
        snprintf(resp.message, sizeof(resp.message), "Authentication failed");

        send_response(sess, CMD_LOGIN_RESP, frame->seq, ERR_AUTH_FAILED, &resp, sizeof(resp));
        LOG_WARN("Login failed: username=%s", req->username);
        return;
    }

    /* 设置会话认证状态 */
    pthread_mutex_lock(&sess->lock);
    sess->is_authenticated = true;
    strncpy(sess->username, req->username, sizeof(sess->username) - 1);
    sess->perm = perm;
    pthread_mutex_unlock(&sess->lock);

    /* 发送成功响应 */
    login_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.permission = perm;
    snprintf(resp.message, sizeof(resp.message), "Login successful");

    send_response(sess, CMD_LOGIN_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
    LOG_INFO("User logged in: username=%s, perm=%d", req->username, perm);
}

void handle_logout(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 清理传输任务 */
    session_cleanup_tasks(sess);

    /* 清除认证状态 */
    pthread_mutex_lock(&sess->lock);
    sess->is_authenticated = false;
    sess->perm = PERM_NONE;
    memset(sess->username, 0, sizeof(sess->username));
    pthread_mutex_unlock(&sess->lock);

    /* 发送响应 */
    send_response(sess, CMD_LOGOUT_RESP, frame->seq, ERR_SUCCESS, NULL, 0);
    LOG_INFO("User logged out: session_id=%u", sess->session_id);
}

void handle_file_list(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_LIST_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    /* 解析请求路径 */
    file_request_t* req = (file_request_t*)payload;
    char full_path[512];

    if (req->path[0] == '\0' || strcmp(req->path, "/") == 0) {
        strncpy(full_path, g_config->root_dir, sizeof(full_path) - 1);
    } else {
        build_full_path(full_path, sizeof(full_path), g_config->root_dir, req->path);
    }

    /* 检查路径安全性 */
    if (check_path_safety(full_path) < 0) {
        send_error(sess, CMD_FILE_LIST_RESP, frame->seq, ERR_INVALID_PATH, "Invalid path");
        LOG_WARN("Invalid path: %s", req->path);
        return;
    }

    /* 打开目录 */
    DIR* dir = opendir(full_path);
    if (dir == NULL) {
        send_error(sess, CMD_FILE_LIST_RESP, frame->seq, ERR_FILE_NOT_FOUND, "Directory not found");
        LOG_WARN("Failed to open directory: %s", full_path);
        return;
    }

    /* 收集文件信息 */
    file_info_t files[255];
    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL && count < 255) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", full_path, entry->d_name);

        struct stat st;
        if (stat(file_path, &st) == 0) {
            strncpy(files[count].name, entry->d_name, sizeof(files[count].name) - 1);
            files[count].size = (uint32_t)st.st_size;
            files[count].mode = (uint32_t)st.st_mode;
            files[count].mtime = (uint32_t)st.st_mtime;
            files[count].is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
            count++;
        }
    }

    closedir(dir);

    /* 构建响应 */
    size_t resp_size = sizeof(file_list_response_t) + count * sizeof(file_info_t);
    uint8_t* resp_buf = malloc(resp_size);
    if (resp_buf == NULL) {
        send_error(sess, CMD_FILE_LIST_RESP, frame->seq, ERR_SERVER_INTERNAL, "Memory allocation failed");
        return;
    }

    file_list_response_t* resp = (file_list_response_t*)resp_buf;
    resp->status = ERR_SUCCESS;
    resp->count = count;
    snprintf(resp->message, sizeof(resp->message), "Found %d files", count);

    memcpy(resp_buf + sizeof(file_list_response_t), files, count * sizeof(file_info_t));

    send_response(sess, CMD_FILE_LIST_RESP, frame->seq, ERR_SUCCESS, resp_buf, resp_size);
    free(resp_buf);

    LOG_INFO("File list: path=%s, count=%d", full_path, count);
}

void handle_file_read(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_READ_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    /* 解析请求路径 */
    file_request_t* req = (file_request_t*)payload;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), g_config->root_dir, req->path);

    /* 检查路径安全性 */
    if (check_path_safety(full_path) < 0) {
        send_error(sess, CMD_FILE_READ_RESP, frame->seq, ERR_INVALID_PATH, "Invalid path");
        return;
    }

    /* 打开文件 */
    FILE* fp = fopen(full_path, "rb");
    if (fp == NULL) {
        send_error(sess, CMD_FILE_READ_RESP, frame->seq, ERR_FILE_NOT_FOUND, "File not found");
        LOG_WARN("Failed to open file: %s", full_path);
        return;
    }

    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* 检查文件大小（限制64KB） */
    if (file_size > 65536) {
        fclose(fp);
        send_error(sess, CMD_FILE_READ_RESP, frame->seq, ERR_FILE_TOO_LARGE, "File too large (max 64KB)");
        LOG_WARN("File too large: %s (%ld bytes)", full_path, file_size);
        return;
    }

    /* 读取文件内容 */
    size_t resp_size = sizeof(file_read_response_t) + file_size;
    uint8_t* resp_buf = malloc(resp_size);
    if (resp_buf == NULL) {
        fclose(fp);
        send_error(sess, CMD_FILE_READ_RESP, frame->seq, ERR_SERVER_INTERNAL, "Memory allocation failed");
        return;
    }

    file_read_response_t* resp = (file_read_response_t*)resp_buf;
    resp->status = ERR_SUCCESS;
    resp->size = (uint32_t)file_size;
    snprintf(resp->message, sizeof(resp->message), "File size: %u bytes", resp->size);

    size_t read_bytes = fread(resp_buf + sizeof(file_read_response_t), 1, file_size, fp);
    fclose(fp);

    if (read_bytes != file_size) {
        free(resp_buf);
        send_error(sess, CMD_FILE_READ_RESP, frame->seq, ERR_SERVER_INTERNAL, "Failed to read file");
        return;
    }

    send_response(sess, CMD_FILE_READ_RESP, frame->seq, ERR_SUCCESS, resp_buf, resp_size);
    free(resp_buf);

    LOG_INFO("File read: path=%s, size=%u", req->path, resp->size);
}

void handle_file_delete(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证和权限 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_DELETE_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    if (sess->perm != PERM_RW) {
        send_error(sess, CMD_FILE_DELETE_RESP, frame->seq, ERR_PERMISSION_DENIED, "Permission denied");
        return;
    }

    /* 解析请求路径 */
    file_request_t* req = (file_request_t*)payload;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), g_config->root_dir, req->path);

    /* 检查路径安全性 */
    if (check_path_safety(full_path) < 0) {
        send_error(sess, CMD_FILE_DELETE_RESP, frame->seq, ERR_INVALID_PATH, "Invalid path");
        return;
    }

    /* 检查文件/目录是否存在 */
    struct stat st;
    if (stat(full_path, &st) != 0) {
        send_error(sess, CMD_FILE_DELETE_RESP, frame->seq, ERR_FILE_NOT_FOUND, "File not found");
        return;
    }

    int result;
    if (S_ISDIR(st.st_mode)) {
        result = rmdir(full_path);
    } else {
        result = unlink(full_path);
    }

    if (result != 0) {
        send_error(sess, CMD_FILE_DELETE_RESP, frame->seq, ERR_FAILED, "Failed to delete");
        LOG_ERROR("Failed to delete: %s (%s)", full_path, strerror(errno));
        return;
    }

    send_response(sess, CMD_FILE_DELETE_RESP, frame->seq, ERR_SUCCESS, NULL, 0);
    LOG_INFO("File deleted: %s", full_path);
}

void handle_file_rename(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证和权限 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_RENAME_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    if (sess->perm != PERM_RW) {
        send_error(sess, CMD_FILE_RENAME_RESP, frame->seq, ERR_PERMISSION_DENIED, "Permission denied");
        return;
    }

    /* 解析请求（格式：old_path\0new_path） */
    char* req_str = (char*)payload;
    char* null_pos = strchr(req_str, '\0');
    if (null_pos == NULL || null_pos == req_str) {
        send_error(sess, CMD_FILE_RENAME_RESP, frame->seq, ERR_INVALID_PATH, "Invalid rename format");
        return;
    }

    char* old_path = req_str;
    char* new_path = null_pos + 1;

    char full_old[512], full_new[512];
    build_full_path(full_old, sizeof(full_old), g_config->root_dir, old_path);
    build_full_path(full_new, sizeof(full_new), g_config->root_dir, new_path);

    /* 检查路径安全性 */
    if (check_path_safety(full_old) < 0 || check_path_safety(full_new) < 0) {
        send_error(sess, CMD_FILE_RENAME_RESP, frame->seq, ERR_INVALID_PATH, "Invalid path");
        return;
    }

    /* 重命名 */
    if (rename(full_old, full_new) != 0) {
        send_error(sess, CMD_FILE_RENAME_RESP, frame->seq, ERR_FAILED, "Failed to rename");
        LOG_ERROR("Failed to rename: %s -> %s (%s)", full_old, full_new, strerror(errno));
        return;
    }

    send_response(sess, CMD_FILE_RENAME_RESP, frame->seq, ERR_SUCCESS, NULL, 0);
    LOG_INFO("File renamed: %s -> %s", old_path, new_path);
}

void handle_file_edit(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证和权限 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_EDIT_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    if (sess->perm != PERM_RW) {
        send_error(sess, CMD_FILE_EDIT_RESP, frame->seq, ERR_PERMISSION_DENIED, "Permission denied");
        return;
    }

    /* 解析请求 */
    file_edit_request_t* req = (file_edit_request_t*)payload;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), g_config->root_dir, req->path);

    /* 检查路径安全性 */
    if (check_path_safety(full_path) < 0) {
        send_error(sess, CMD_FILE_EDIT_RESP, frame->seq, ERR_INVALID_PATH, "Invalid path");
        return;
    }

    /* 检查文件是否存在，不存在则创建 */
    if (access(full_path, F_OK) != 0) {
        /* 文件不存在，检查父目录是否存在 */
        char dir_path[512];
        strncpy(dir_path, full_path, sizeof(dir_path) - 1);
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (access(dir_path, F_OK) != 0) {
                send_error(sess, CMD_FILE_EDIT_RESP, frame->seq, ERR_INVALID_PATH, "Parent directory not found");
                return;
            }
        }
        /* 文件不存在是正常的，继续创建 */
    }

    /* 限制文件大小（64KB） */
    if (req->content_len > 65536) {
        send_error(sess, CMD_FILE_EDIT_RESP, frame->seq, ERR_FILE_TOO_LARGE, "Content too large (max 64KB)");
        return;
    }

    /* 写入新内容 */
    FILE* fp = fopen(full_path, "wb");
    if (fp == NULL) {
        send_error(sess, CMD_FILE_EDIT_RESP, frame->seq, ERR_FAILED, "Failed to open file for writing");
        LOG_ERROR("Failed to open file for edit: %s", full_path);
        return;
    }

    size_t written = fwrite(req->new_content, 1, req->content_len, fp);
    fclose(fp);

    if (written != req->content_len) {
        send_error(sess, CMD_FILE_EDIT_RESP, frame->seq, ERR_SERVER_INTERNAL, "Failed to write file");
        return;
    }

    send_response(sess, CMD_FILE_EDIT_RESP, frame->seq, ERR_SUCCESS, NULL, 0);
    LOG_INFO("File edited: %s, size=%u", req->path, req->content_len);
}

void handle_upload_start(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证和权限 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_UPLOAD_START_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    if (sess->perm != PERM_RW) {
        send_error(sess, CMD_FILE_UPLOAD_START_RESP, frame->seq, ERR_PERMISSION_DENIED, "Permission denied");
        return;
    }

    /* 检查并发传输数 */
    if (sess->task_count >= g_config->max_concurrent_transfers) {
        send_error(sess, CMD_FILE_UPLOAD_START_RESP, frame->seq, ERR_RESOURCE_BUSY, "Too many concurrent transfers");
        return;
    }

    /* 解析请求 */
    transfer_start_request_t* req = (transfer_start_request_t*)payload;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), g_config->upload_dir, req->remote_path);

    /* 检查路径安全性 */
    if (check_path_safety(full_path) < 0) {
        send_error(sess, CMD_FILE_UPLOAD_START_RESP, frame->seq, ERR_INVALID_PATH, "Invalid path");
        return;
    }

    /* 创建文件 */
    FILE* fp = fopen(full_path, "wb");
    if (fp == NULL) {
        send_error(sess, CMD_FILE_UPLOAD_START_RESP, frame->seq, ERR_FAILED, "Failed to create file");
        LOG_ERROR("Failed to create file: %s", full_path);
        return;
    }

    /* 创建传输任务 */
    transfer_task_t* task = session_add_task(sess, TRANSFER_UPLOAD, req->remote_path, req->file_size, fp);
    if (task == NULL) {
        fclose(fp);
        send_error(sess, CMD_FILE_UPLOAD_START_RESP, frame->seq, ERR_SERVER_INTERNAL, "Failed to create task");
        return;
    }

    /* 发送响应 */
    transfer_start_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = task->task_id;
    resp.file_size = req->file_size;
    snprintf(resp.message, sizeof(resp.message), "Upload started");

    send_response(sess, CMD_FILE_UPLOAD_START_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
    LOG_INFO("Upload started: path=%s, task_id=%u, size=%u", req->remote_path, task->task_id, req->file_size);
}

void handle_upload_data(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_UPLOAD_DATA_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    /* 解析请求 */
    transfer_data_t* req = (transfer_data_t*)payload;

    /* 查找任务 */
    pthread_mutex_lock(&sess->lock);
    transfer_task_t* task = sess->tasks;
    while (task != NULL && task->task_id != req->task_id) {
        task = task->next;
    }
    pthread_mutex_unlock(&sess->lock);

    if (task == NULL) {
        send_error(sess, CMD_FILE_UPLOAD_DATA_RESP, frame->seq, ERR_FAILED, "Task not found");
        return;
    }

    /* 检查任务状态 */
    pthread_mutex_lock(&task->lock);
    if (task->status != TASK_RUNNING) {
        pthread_mutex_unlock(&task->lock);
        send_error(sess, CMD_FILE_UPLOAD_DATA_RESP, frame->seq, ERR_TRANSFER_ABORTED, "Task not running");
        return;
    }

    /* 写入文件 */
    size_t data_offset = sizeof(transfer_data_t);
    size_t data_len = req->data_len;

    fseek(task->file, req->offset, SEEK_SET);
    size_t written = fwrite(payload + data_offset, 1, data_len, task->file);

    if (written != data_len) {
        pthread_mutex_unlock(&task->lock);
        send_error(sess, CMD_FILE_UPLOAD_DATA_RESP, frame->seq, ERR_SERVER_INTERNAL, "Write failed");
        LOG_ERROR("Write failed: task_id=%u", req->task_id);
        return;
    }

    task->transferred += written;
    task->last_update = time(NULL);
    pthread_mutex_unlock(&task->lock);

    /* 发送响应 */
    transfer_control_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = req->task_id;
    resp.transferred = task->transferred;
    snprintf(resp.message, sizeof(resp.message), "Data received");

    send_response(sess, CMD_FILE_UPLOAD_DATA_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
    LOG_DEBUG("Upload data: task_id=%u, offset=%u, len=%u, total=%u",
              req->task_id, req->offset, req->data_len, task->transferred);
}

void handle_upload_pause(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_UPLOAD_PAUSE_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    uint32_t* task_id = (uint32_t*)payload;

    /* 查找任务并暂停 */
    pthread_mutex_lock(&sess->lock);
    transfer_task_t* task = sess->tasks;
    while (task != NULL && task->task_id != *task_id) {
        task = task->next;
    }
    pthread_mutex_unlock(&sess->lock);

    if (task == NULL) {
        send_error(sess, CMD_FILE_UPLOAD_PAUSE_RESP, frame->seq, ERR_FAILED, "Task not found");
        return;
    }

    session_pause_task(task);

    transfer_control_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = *task_id;
    resp.transferred = task->transferred;
    snprintf(resp.message, sizeof(resp.message), "Upload paused");

    send_response(sess, CMD_FILE_UPLOAD_PAUSE_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
    LOG_INFO("Upload paused: task_id=%u", *task_id);
}

void handle_upload_resume(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_UPLOAD_RESUME_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    uint32_t* task_id = (uint32_t*)payload;

    /* 查找任务并恢复 */
    pthread_mutex_lock(&sess->lock);
    transfer_task_t* task = sess->tasks;
    while (task != NULL && task->task_id != *task_id) {
        task = task->next;
    }
    pthread_mutex_unlock(&sess->lock);

    if (task == NULL) {
        send_error(sess, CMD_FILE_UPLOAD_RESUME_RESP, frame->seq, ERR_FAILED, "Task not found");
        return;
    }

    session_resume_task(task);

    transfer_control_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = *task_id;
    resp.transferred = task->transferred;
    snprintf(resp.message, sizeof(resp.message), "Upload resumed");

    send_response(sess, CMD_FILE_UPLOAD_RESUME_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
    LOG_INFO("Upload resumed: task_id=%u", *task_id);
}

void handle_upload_end(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_UPLOAD_END_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    uint32_t* task_id = (uint32_t*)payload;

    /* 查找任务 */
    pthread_mutex_lock(&sess->lock);
    transfer_task_t* task = sess->tasks;
    while (task != NULL && task->task_id != *task_id) {
        task = task->next;
    }
    pthread_mutex_unlock(&sess->lock);

    if (task == NULL) {
        send_error(sess, CMD_FILE_UPLOAD_END_RESP, frame->seq, ERR_FAILED, "Task not found");
        return;
    }

    /* 关闭文件 */
    pthread_mutex_lock(&task->lock);
    if (task->file != NULL) {
        fclose(task->file);
        task->file = NULL;
        task->status = TASK_COMPLETED;
    }
    pthread_mutex_unlock(&task->lock);

    /* 发送响应 */
    transfer_control_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = *task_id;
    resp.transferred = task->transferred;
    snprintf(resp.message, sizeof(resp.message), "Upload completed");

    send_response(sess, CMD_FILE_UPLOAD_END_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));

    /* 删除任务 */
    session_remove_task(sess, *task_id);

    LOG_INFO("Upload completed: task_id=%u, transferred=%u", *task_id, resp.transferred);
}

void handle_download_start(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_DOWNLOAD_START_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    /* 检查并发传输数 */
    if (sess->task_count >= g_config->max_concurrent_transfers) {
        send_error(sess, CMD_FILE_DOWNLOAD_START_RESP, frame->seq, ERR_RESOURCE_BUSY, "Too many concurrent transfers");
        return;
    }

    /* 解析请求 */
    transfer_start_request_t* req = (transfer_start_request_t*)payload;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), g_config->root_dir, req->remote_path);

    /* 检查路径安全性 */
    if (check_path_safety(full_path) < 0) {
        send_error(sess, CMD_FILE_DOWNLOAD_START_RESP, frame->seq, ERR_INVALID_PATH, "Invalid path");
        return;
    }

    /* 打开文件 */
    FILE* fp = fopen(full_path, "rb");
    if (fp == NULL) {
        send_error(sess, CMD_FILE_DOWNLOAD_START_RESP, frame->seq, ERR_FILE_NOT_FOUND, "File not found");
        LOG_WARN("Failed to open file: %s", full_path);
        return;
    }

    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    uint32_t file_size = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* 创建传输任务 */
    transfer_task_t* task = session_add_task(sess, TRANSFER_DOWNLOAD, req->remote_path, file_size, fp);
    if (task == NULL) {
        fclose(fp);
        send_error(sess, CMD_FILE_DOWNLOAD_START_RESP, frame->seq, ERR_SERVER_INTERNAL, "Failed to create task");
        return;
    }

    /* 发送响应 */
    transfer_start_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = task->task_id;
    resp.file_size = file_size;
    snprintf(resp.message, sizeof(resp.message), "Download started");

    send_response(sess, CMD_FILE_DOWNLOAD_START_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
    LOG_INFO("Download started: path=%s, task_id=%u, size=%u", req->remote_path, task->task_id, file_size);
}

void handle_download_data(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_DOWNLOAD_DATA_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    /* 解析请求 */
    transfer_data_t* req = (transfer_data_t*)payload;

    /* 查找任务 */
    pthread_mutex_lock(&sess->lock);
    transfer_task_t* task = sess->tasks;
    while (task != NULL && task->task_id != req->task_id) {
        task = task->next;
    }
    pthread_mutex_unlock(&sess->lock);

    if (task == NULL) {
        send_error(sess, CMD_FILE_DOWNLOAD_DATA_RESP, frame->seq, ERR_FAILED, "Task not found");
        return;
    }

    /* 检查任务状态 */
    pthread_mutex_lock(&task->lock);
    if (task->status != TASK_RUNNING) {
        pthread_mutex_unlock(&task->lock);
        send_error(sess, CMD_FILE_DOWNLOAD_DATA_RESP, frame->seq, ERR_TRANSFER_ABORTED, "Task not running");
        return;
    }

    /* 读取文件数据 */
    size_t buf_size = sizeof(transfer_data_t) + g_config->buffer_size;
    uint8_t* buf = malloc(buf_size);
    if (buf == NULL) {
        pthread_mutex_unlock(&task->lock);
        send_error(sess, CMD_FILE_DOWNLOAD_DATA_RESP, frame->seq, ERR_SERVER_INTERNAL, "Memory allocation failed");
        return;
    }

    transfer_data_t* data = (transfer_data_t*)buf;
    data->task_id = req->task_id;
    data->offset = task->transferred;

    fseek(task->file, task->transferred, SEEK_SET);
    size_t read_bytes = fread(buf + sizeof(transfer_data_t), 1, g_config->buffer_size, task->file);
    data->data_len = (uint32_t)read_bytes;

    task->transferred += read_bytes;
    task->last_update = time(NULL);
    pthread_mutex_unlock(&task->lock);

    /* 发送数据 */
    send_response(sess, CMD_FILE_DOWNLOAD_DATA_RESP, frame->seq, ERR_SUCCESS, buf, sizeof(transfer_data_t) + read_bytes);
    free(buf);

    LOG_DEBUG("Download data: task_id=%u, offset=%u, len=%u, total=%u",
              req->task_id, data->offset, read_bytes, task->transferred);
}

void handle_download_pause(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_DOWNLOAD_PAUSE_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    uint32_t* task_id = (uint32_t*)payload;

    /* 查找任务并暂停 */
    pthread_mutex_lock(&sess->lock);
    transfer_task_t* task = sess->tasks;
    while (task != NULL && task->task_id != *task_id) {
        task = task->next;
    }
    pthread_mutex_unlock(&sess->lock);

    if (task == NULL) {
        send_error(sess, CMD_FILE_DOWNLOAD_PAUSE_RESP, frame->seq, ERR_FAILED, "Task not found");
        return;
    }

    session_pause_task(task);

    transfer_control_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = *task_id;
    resp.transferred = task->transferred;
    snprintf(resp.message, sizeof(resp.message), "Download paused");

    send_response(sess, CMD_FILE_DOWNLOAD_PAUSE_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
    LOG_INFO("Download paused: task_id=%u", *task_id);
}

void handle_download_resume(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_DOWNLOAD_RESUME_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    uint32_t* task_id = (uint32_t*)payload;

    /* 查找任务并恢复 */
    pthread_mutex_lock(&sess->lock);
    transfer_task_t* task = sess->tasks;
    while (task != NULL && task->task_id != *task_id) {
        task = task->next;
    }
    pthread_mutex_unlock(&sess->lock);

    if (task == NULL) {
        send_error(sess, CMD_FILE_DOWNLOAD_RESUME_RESP, frame->seq, ERR_FAILED, "Task not found");
        return;
    }

    session_resume_task(task);

    transfer_control_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = *task_id;
    resp.transferred = task->transferred;
    snprintf(resp.message, sizeof(resp.message), "Download resumed");

    send_response(sess, CMD_FILE_DOWNLOAD_RESUME_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
    LOG_INFO("Download resumed: task_id=%u", *task_id);
}

void handle_download_end(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_FILE_DOWNLOAD_END_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    uint32_t* task_id = (uint32_t*)payload;

    /* 查找任务 */
    pthread_mutex_lock(&sess->lock);
    transfer_task_t* task = sess->tasks;
    while (task != NULL && task->task_id != *task_id) {
        task = task->next;
    }
    pthread_mutex_unlock(&sess->lock);

    if (task == NULL) {
        send_error(sess, CMD_FILE_DOWNLOAD_END_RESP, frame->seq, ERR_FAILED, "Task not found");
        return;
    }

    /* 关闭文件 */
    pthread_mutex_lock(&task->lock);
    if (task->file != NULL) {
        fclose(task->file);
        task->file = NULL;
        task->status = TASK_COMPLETED;
    }
    pthread_mutex_unlock(&task->lock);

    /* 发送响应 */
    transfer_control_response_t resp;
    resp.status = ERR_SUCCESS;
    resp.task_id = *task_id;
    resp.transferred = task->transferred;
    snprintf(resp.message, sizeof(resp.message), "Download completed");

    send_response(sess, CMD_FILE_DOWNLOAD_END_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));

    /* 删除任务 */
    session_remove_task(sess, *task_id);

    LOG_INFO("Download completed: task_id=%u, transferred=%u", *task_id, resp.transferred);
}

void handle_sys_info(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_SYS_INFO_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    /* 获取系统信息 */
    system_info_t info;
    if (get_system_info(&info) != 0) {
        send_error(sess, CMD_SYS_INFO_RESP, frame->seq, ERR_SERVER_INTERNAL, "Failed to get system info");
        return;
    }

    send_response(sess, CMD_SYS_INFO_RESP, frame->seq, ERR_SUCCESS, &info, sizeof(info));
    LOG_INFO("System info sent: cpu=%.1f%%, mem_free=%uKB", info.cpu_usage, info.mem_free);
}

void handle_proc_list(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* 检查认证 */
    if (!sess->is_authenticated) {
        send_error(sess, CMD_PROC_LIST_RESP, frame->seq, ERR_NOT_AUTHENTICATED, "Not authenticated");
        return;
    }

    /* 获取进程列表 */
    process_info_t proc_list[100];
    int count = get_process_list(proc_list, 100);

    if (count < 0) {
        send_error(sess, CMD_PROC_LIST_RESP, frame->seq, ERR_SERVER_INTERNAL, "Failed to get process list");
        return;
    }

    /* 构建响应 */
    size_t resp_size = sizeof(process_list_response_t) + count * sizeof(process_info_t);
    uint8_t* resp_buf = malloc(resp_size);
    if (resp_buf == NULL) {
        send_error(sess, CMD_PROC_LIST_RESP, frame->seq, ERR_SERVER_INTERNAL, "Memory allocation failed");
        return;
    }

    process_list_response_t* resp = (process_list_response_t*)resp_buf;
    resp->status = ERR_SUCCESS;
    resp->count = (uint16_t)count;

    memcpy(resp_buf + sizeof(process_list_response_t), proc_list, count * sizeof(process_info_t));

    send_response(sess, CMD_PROC_LIST_RESP, frame->seq, ERR_SUCCESS, resp_buf, resp_size);
    free(resp_buf);

    LOG_INFO("Process list sent: count=%d", count);
}

void handle_heartbeat(client_session_t* sess, protocol_frame_t* frame, uint8_t* payload) {
    /* TODO: 阶段四实现 */
    heartbeat_response_t resp;
    resp.server_time = (uint32_t)time(NULL);
    resp.status = ERR_SUCCESS;

    send_response(sess, CMD_HEARTBEAT_RESP, frame->seq, ERR_SUCCESS, &resp, sizeof(resp));
}