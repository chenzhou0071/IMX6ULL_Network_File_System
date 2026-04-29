/*
 * IMX6ULL FTP Server - Protocol Implementation
 * Version: 1.0
 */

#include "protocol.h"
#include <string.h>

/* ============================================================
 * CRC32 表
 * ============================================================ */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xc1bb67f1, 0xb6bc5767, 0x2fb506dd, 0x58b2364b,
    0xc80d2bda, 0xbf0a1b4c, 0x26034af6, 0x51047a60,
    0xcf60efc3, 0xb867df55, 0x216e8eef, 0x5669be79,
    0xdb61b38c, 0xac66831a, 0x356fd2a0, 0x4268e236,
    0xdc0c7795, 0xab0b4703, 0x320216b9, 0x4505262f,
    0xd5ba3bbe, 0xa2bd0b28, 0x3bb45a92, 0x4cb36a04,
    0xd2d7ffa7, 0xa5d0cf31, 0x3cd99e8b, 0x4bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/* ============================================================
 * 函数实现
 * ============================================================ */

/**
 * 计算CRC32校验和
 */
uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
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