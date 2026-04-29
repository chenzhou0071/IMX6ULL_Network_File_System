#ifndef PROTOCOLUTIL_H
#define PROTOCOLUTIL_H

#include <QByteArray>
#include <QString>
#include <stdint.h>

/**
 * 协议工具类
 * 实现CRC32校验、MD5哈希、帧封装/解析
 */

// 协议常量
#define PROTOCOL_MAGIC     0x46545053  // 'FTPS'
#define PROTOCOL_VERSION   1
#define FRAME_HEADER_SIZE  20

// 命令字
enum ProtocolCmd : uint16_t {
    CMD_LOGIN              = 0x0101,
    CMD_LOGIN_RESP        = 0x0102,
    CMD_LOGOUT            = 0x0103,
    CMD_FILE_LIST          = 0x0201,
    CMD_FILE_LIST_RESP     = 0x0202,
    CMD_FILE_READ          = 0x0203,
    CMD_FILE_READ_RESP     = 0x0204,
    CMD_FILE_DELETE        = 0x0205,
    CMD_FILE_DELETE_RESP   = 0x0206,
    CMD_FILE_RENAME        = 0x0207,
    CMD_FILE_RENAME_RESP   = 0x0208,
    CMD_FILE_EDIT          = 0x0209,
    CMD_FILE_EDIT_RESP     = 0x020A,
    CMD_FILE_UPLOAD_START  = 0x0301,
    CMD_FILE_UPLOAD_START_RESP = 0x0302,
    CMD_FILE_UPLOAD_DATA   = 0x0303,
    CMD_FILE_UPLOAD_DATA_RESP = 0x0304,
    CMD_FILE_UPLOAD_PAUSE  = 0x0305,
    CMD_FILE_UPLOAD_PAUSE_RESP = 0x0306,
    CMD_FILE_UPLOAD_RESUME = 0x0307,
    CMD_FILE_UPLOAD_RESUME_RESP = 0x0308,
    CMD_FILE_UPLOAD_END    = 0x0309,
    CMD_FILE_UPLOAD_END_RESP = 0x030A,
    CMD_FILE_DOWNLOAD_START = 0x030B,
    CMD_FILE_DOWNLOAD_START_RESP = 0x030C,
    CMD_FILE_DOWNLOAD_DATA = 0x030D,
    CMD_FILE_DOWNLOAD_DATA_RESP = 0x030E,
    CMD_FILE_DOWNLOAD_PAUSE = 0x030F,
    CMD_FILE_DOWNLOAD_PAUSE_RESP = 0x0310,
    CMD_FILE_DOWNLOAD_RESUME = 0x0311,
    CMD_FILE_DOWNLOAD_RESUME_RESP = 0x0312,
    CMD_FILE_DOWNLOAD_END  = 0x0313,
    CMD_FILE_DOWNLOAD_END_RESP = 0x0314,
    CMD_SYS_INFO           = 0x0401,
    CMD_SYS_INFO_RESP      = 0x0402,
    CMD_PROC_LIST          = 0x0403,
    CMD_PROC_LIST_RESP     = 0x0404,
    CMD_HEARTBEAT          = 0x0501,
    CMD_HEARTBEAT_RESP     = 0x0502
};

// 错误码
enum ProtocolErr {
    ERR_SUCCESS            = 0,
    ERR_FAILED             = 1,
    ERR_AUTH_FAILED         = 10,
    ERR_PERMISSION_DENIED   = 11,
    ERR_NOT_AUTHENTICATED   = 12,
    ERR_FILE_NOT_FOUND      = 20,
    ERR_FILE_EXISTS        = 21,
    ERR_FILE_TOO_LARGE     = 22,
    ERR_DISK_FULL          = 23,
    ERR_INVALID_PATH       = 24,
    ERR_RESOURCE_BUSY      = 30,
    ERR_TRANSFER_ABORTED   = 31,
    ERR_SERVER_INTERNAL    = 99
};

// 帧头结构
#pragma pack(push, 1)
struct ProtocolFrame {
    uint32_t magic;      // 魔数 0x46545053
    uint16_t version;   // 版本
    uint16_t cmd;       // 命令字
    uint32_t seq;       // 序列号
    uint32_t length;    // 数据长度
    uint32_t checksum;  // CRC32校验和
};
#pragma pack(pop)

class ProtocolUtil
{
public:
    // CRC32校验（逐位算法，与Python客户端一致）
    static uint32_t crc32(const uint8_t* data, size_t len);
    static uint32_t crc32(const QByteArray& data);

    // MD5哈希
    static QByteArray md5(const QString& str);
    static QByteArray md5(const uint8_t* data, size_t len);
    static QString md5Hex(const QString& str);

    // 帧封装
    static QByteArray packFrame(uint16_t cmd, uint32_t seq, const QByteArray& data);
    static QByteArray packLogin(const QString& username, const QString& password);

    // 帧解析
    static bool unpackFrame(const QByteArray& data, ProtocolFrame& frame, QByteArray& payload);

    // 辅助函数
    static bool isValidMagic(uint32_t magic);
    static QString cmdToString(uint16_t cmd);
    static QString errToString(uint8_t err);
};

#endif // PROTOCOLUTIL_H
