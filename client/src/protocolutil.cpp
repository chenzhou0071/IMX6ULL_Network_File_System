#include "protocolutil.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QDebug>
#include <cstring>

// ============== CRC32 实现 ==============
uint32_t ProtocolUtil::crc32(const uint8_t* data, size_t len)
{
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

uint32_t ProtocolUtil::crc32(const QByteArray& data)
{
    return crc32((const uint8_t*)data.constData(), data.size());
}

// ============== MD5 实现 ==============
QByteArray ProtocolUtil::md5(const QString& str)
{
    QByteArray data = str.toUtf8();
    return QCryptographicHash::hash(data, QCryptographicHash::Md5);
}

QByteArray ProtocolUtil::md5(const uint8_t* data, size_t len)
{
    return QCryptographicHash::hash(QByteArray((const char*)data, len), QCryptographicHash::Md5);
}

QString ProtocolUtil::md5Hex(const QString& str)
{
    return QString(md5(str).toHex());
}

// ============== 帧封装 ==============
QByteArray ProtocolUtil::packFrame(uint16_t cmd, uint32_t seq, const QByteArray& data)
{
    // 计算校验和
    uint32_t checksum = crc32(data);
    uint32_t magic = PROTOCOL_MAGIC;
    uint16_t version = PROTOCOL_VERSION;
    uint32_t len = data.size();

    // 写入帧头 (小端序) - 使用memcpy保证字节序
    QByteArray frame;
    frame.reserve(FRAME_HEADER_SIZE + data.size());

    char buf[20];
    memcpy(buf, &magic, 4);
    memcpy(buf + 4, &version, 2);
    memcpy(buf + 6, &cmd, 2);
    memcpy(buf + 8, &seq, 4);
    memcpy(buf + 12, &len, 4);
    memcpy(buf + 16, &checksum, 4);

    frame.append(buf, 20);
    frame.append(data);

    return frame;
}

QByteArray ProtocolUtil::packLogin(const QString& username, const QString& password)
{
    // 用户名64字节 + 密码64字节(MD5) = 128字节
    QByteArray payload;

    // 用户名（64字节，不足补0）
    QByteArray usernameBytes = username.toUtf8();
    usernameBytes.resize(64);
    payload.append(usernameBytes);

    // 密码MD5（64字节）
    QByteArray passwordMd5 = md5Hex(password).toUtf8();
    passwordMd5.resize(64);
    payload.append(passwordMd5);

    return payload;
}

// ============== 帧解析 ==============
bool ProtocolUtil::unpackFrame(const QByteArray& data, ProtocolFrame& frame, QByteArray& payload)
{
    if (data.size() < FRAME_HEADER_SIZE) {
        return false;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint32_t magic, length, checksum;
    uint16_t version, cmd;
    uint32_t seq;

    stream >> magic;
    stream >> version;
    stream >> cmd;
    stream >> seq;
    stream >> length;
    stream >> checksum;

    // 检查魔数
    if (magic != PROTOCOL_MAGIC) {
        qDebug() << "Invalid magic:" << QString::number(magic, 16);
        return false;
    }

    // 检查数据长度
    if (FRAME_HEADER_SIZE + length > (uint32_t)data.size()) {
        qDebug() << "Data incomplete, need" << length << "bytes";
        return false;
    }

    // 提取载荷
    payload = data.mid(FRAME_HEADER_SIZE, length);

    // 验证校验和
    uint32_t calcChecksum = crc32(payload);
    if (calcChecksum != checksum) {
        qDebug() << "Checksum mismatch: expected" << QString::number(checksum, 16)
                 << "got" << QString::number(calcChecksum, 16);
        return false;
    }

    // 填充帧结构
    frame.magic = magic;
    frame.version = version;
    frame.cmd = cmd;
    frame.seq = seq;
    frame.length = length;
    frame.checksum = checksum;

    return true;
}

// ============== 辅助函数 ==============
bool ProtocolUtil::isValidMagic(uint32_t magic)
{
    return magic == PROTOCOL_MAGIC;
}

QString ProtocolUtil::cmdToString(uint16_t cmd)
{
    switch (cmd) {
        case CMD_LOGIN:            return "LOGIN";
        case CMD_LOGIN_RESP:       return "LOGIN_RESP";
        case CMD_LOGOUT:           return "LOGOUT";
        case CMD_FILE_LIST:        return "FILE_LIST";
        case CMD_FILE_LIST_RESP:   return "FILE_LIST_RESP";
        case CMD_FILE_READ:        return "FILE_READ";
        case CMD_FILE_READ_RESP:   return "FILE_READ_RESP";
        case CMD_FILE_DELETE:      return "FILE_DELETE";
        case CMD_FILE_DELETE_RESP: return "FILE_DELETE_RESP";
        case CMD_FILE_RENAME:      return "FILE_RENAME";
        case CMD_FILE_RENAME_RESP: return "FILE_RENAME_RESP";
        case CMD_FILE_EDIT:        return "FILE_EDIT";
        case CMD_FILE_EDIT_RESP:   return "FILE_EDIT_RESP";
        case CMD_SYS_INFO:         return "SYS_INFO";
        case CMD_SYS_INFO_RESP:    return "SYS_INFO_RESP";
        case CMD_PROC_LIST:        return "PROC_LIST";
        case CMD_PROC_LIST_RESP:   return "PROC_LIST_RESP";
        case CMD_HEARTBEAT:        return "HEARTBEAT";
        case CMD_HEARTBEAT_RESP:   return "HEARTBEAT_RESP";
        default:                    return QString("UNKNOWN(0x%1)").arg(cmd, 4, 16, QChar('0'));
    }
}

QString ProtocolUtil::errToString(uint8_t err)
{
    switch (err) {
        case ERR_SUCCESS:            return "Success";
        case ERR_FAILED:             return "Operation failed";
        case ERR_AUTH_FAILED:         return "Authentication failed";
        case ERR_PERMISSION_DENIED:  return "Permission denied";
        case ERR_NOT_AUTHENTICATED:  return "Not authenticated";
        case ERR_FILE_NOT_FOUND:      return "File not found";
        case ERR_FILE_EXISTS:        return "File already exists";
        case ERR_FILE_TOO_LARGE:     return "File too large";
        case ERR_DISK_FULL:          return "Disk full";
        case ERR_INVALID_PATH:       return "Invalid path";
        case ERR_RESOURCE_BUSY:      return "Resource busy";
        case ERR_TRANSFER_ABORTED:   return "Transfer aborted";
        case ERR_SERVER_INTERNAL:     return "Server internal error";
        default:                      return QString("Unknown(%1)").arg(err);
    }
}
