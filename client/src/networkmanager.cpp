#include "networkmanager.h"
#include <QDataStream>
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QtEndian>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_seq(0)
    , m_lastCmd(0)
    , m_authenticated(false)
    , m_permission(0)
    , m_requestingLogFiles(false)
    , m_downloadFile(nullptr)
    , m_downloadTotal(0)
    , m_downloadReceived(0)
    , m_uploadTaskId(0)
    , m_downloadTaskId(0)
{
    // 连接信号
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &NetworkManager::onError);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);

    // 心跳定时器
    connect(m_heartbeatTimer, &QTimer::timeout, this, &NetworkManager::onHeartbeat);
}

NetworkManager::~NetworkManager()
{
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
    }
    disconnect();
}

void NetworkManager::connectToServer(const QString& ip, quint16 port)
{
    qDebug() << "Connecting to" << ip << ":" << port;
    m_socket->connectToHost(ip, port);
}

void NetworkManager::disconnect()
{
    m_heartbeatTimer->stop();
    m_authenticated = false;
    m_permission = 0;
    m_recvBuffer.clear();
    m_socket->disconnectFromHost();
}

void NetworkManager::sendLogin(const QString& username, const QString& password)
{
    QByteArray payload = ProtocolUtil::packLogin(username, password);
    sendFrame(CMD_LOGIN, payload);
}

void NetworkManager::sendLogout()
{
    sendFrame(CMD_LOGOUT, QByteArray());
    m_authenticated = false;
    m_permission = 0;
}

void NetworkManager::sendFileList(const QString& path)
{
    QByteArray payload = path.toUtf8();
    payload.resize(512);  // 固定512字节
    sendFrame(CMD_FILE_LIST, payload);
}

void NetworkManager::sendLogFileList(const QString& path)
{
    m_requestingLogFiles = true;
    QByteArray payload = path.toUtf8();
    payload.resize(512);  // 固定512字节
    sendFrame(CMD_FILE_LIST, payload);
}

void NetworkManager::sendFileRead(const QString& path)
{
    QByteArray payload = path.toUtf8();
    payload.resize(512);
    sendFrame(CMD_FILE_READ, payload);
}

void NetworkManager::sendFileDelete(const QString& path)
{
    QByteArray payload = path.toUtf8();
    payload.resize(512);
    sendFrame(CMD_FILE_DELETE, payload);
}


void NetworkManager::sendFileEdit(const QString& path, const QString& content)
{
    QByteArray contentBytes = content.toUtf8();
    QByteArray payload;
    payload.reserve(516 + contentBytes.size());

    // 路径 (512字节)
    QByteArray pathBytes = path.toUtf8();
    pathBytes.resize(512);
    payload.append(pathBytes);

    // 内容长度 (4字节)
    QDataStream stream(&payload, QIODevice::Append);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << (quint32)contentBytes.size();

    // 内容
    payload.append(contentBytes);

    sendFrame(CMD_FILE_EDIT, payload);
}

void NetworkManager::sendFileRename(const QString& oldPath, const QString& newPath)
{
    // 格式: old_path\0new_path
    QByteArray payload;
    payload.append(oldPath.toUtf8());
    payload.append('\0');
    payload.append(newPath.toUtf8());

    sendFrame(CMD_FILE_RENAME, payload);
}

void NetworkManager::sendSysInfo()
{
    sendFrame(CMD_SYS_INFO, QByteArray());
}

void NetworkManager::sendFileUploadStart(const QString& path, quint32 fileSize)
{
    QByteArray payload;
    payload.resize(516);  // 512 (path) + 4 (file_size)

    // 路径 (512字节，UTF-8编码)
    QByteArray pathBytes = path.toUtf8();
    memcpy(payload.data(), pathBytes.constData(), qMin(pathBytes.size(), 512));

    // 文件大小 (4字节，小端序)
    quint32 le = fileSize;
    memcpy(payload.data() + 512, &le, 4);

    sendFrame(CMD_FILE_UPLOAD_START, payload);
}

void NetworkManager::sendFileUploadData(const QString& path, quint32 offset, const QByteArray& data)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // task_id (4字节)
    stream << m_uploadTaskId;
    // 偏移量 (4字节)
    stream << offset;
    // 数据长度 (4字节)
    stream << (quint32)data.size();
    // 数据
    payload.append(data);

    sendFrame(CMD_FILE_UPLOAD_DATA, payload);
}

void NetworkManager::sendFileUploadEnd(const QString& path)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // task_id (4字节)
    stream << m_uploadTaskId;

    sendFrame(CMD_FILE_UPLOAD_END, payload);
}

void NetworkManager::sendProcList()
{
    sendFrame(CMD_PROC_LIST, QByteArray());
}

void NetworkManager::sendFrame(uint16_t cmd, const QByteArray& data)
{
    if (!isConnected()) {
        qDebug() << "Not connected";
        return;
    }

    m_lastCmd = cmd;  // 保存命令用于解析响应
    QByteArray frame = ProtocolUtil::packFrame(cmd, ++m_seq, data);
    m_socket->write(frame);
    m_socket->flush();
}

void NetworkManager::onConnected()
{
    qDebug() << "Connected to server";
    emit connected();
}

void NetworkManager::onDisconnected()
{
    qDebug() << "Disconnected from server, buffer:" << m_recvBuffer.size() << "bytes";
    m_heartbeatTimer->stop();
    m_authenticated = false;
    emit disconnected();
}

void NetworkManager::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "Socket error:" << error << m_socket->errorString();
    emit connectionError(m_socket->errorString());
}

void NetworkManager::onReadyRead()
{
    QByteArray newData = m_socket->readAll();
    m_recvBuffer.append(newData);
    qDebug() << "onReadyRead: received" << newData.size() << "bytes, buffer now" << m_recvBuffer.size();

    // 打印前20字节（帧头）
    if (newData.size() >= 20) {
        QString hex;
        for (int i = 0; i < qMin(20, newData.size()); i++) {
            hex += QString("%1 ").arg((uchar)newData[i], 2, 16, QChar('0')).toUpper();
        }
        qDebug() << "First 20 bytes:" << hex;
    }

    // 解析帧
    while (m_recvBuffer.size() >= FRAME_HEADER_SIZE) {
        ProtocolFrame frame;
        QByteArray payload;

        if (!ProtocolUtil::unpackFrame(m_recvBuffer, frame, payload)) {
            // 可能是数据不完整或校验失败，跳出等待更多数据
            qDebug() << "Unpack failed, buffer remains:" << m_recvBuffer.size();
            break;
        }

        // 从缓冲区移除已处理的帧
        m_recvBuffer.remove(0, FRAME_HEADER_SIZE + frame.length);

        qDebug() << "Received frame:" << ProtocolUtil::cmdToString(frame.cmd)
                 << "seq:" << frame.seq << "len:" << frame.length;

        // 使用收到的命令字来解析响应
        processResponse(frame.cmd, payload);
    }
}

void NetworkManager::processResponse(uint16_t cmd, const QByteArray& payload)
{
    if (payload.isEmpty()) return;

    switch (cmd) {
        case CMD_LOGIN_RESP:
            parseLogin(payload);
            break;
        case CMD_FILE_LIST_RESP:
            parseFileList(payload);
            break;
        case CMD_FILE_READ_RESP:
            parseFileRead(payload);
            break;
        case CMD_FILE_DELETE_RESP:
        case CMD_FILE_RENAME_RESP:
        case CMD_FILE_EDIT_RESP:
        case CMD_FILE_UPLOAD_DATA_RESP:
        case CMD_FILE_UPLOAD_END_RESP:
        case CMD_FILE_DOWNLOAD_END_RESP:
            parseFileOperation(payload);
            break;
        case CMD_FILE_UPLOAD_START_RESP:
        case CMD_FILE_DOWNLOAD_START_RESP:
            parseTransferStart(payload);
            break;
        case CMD_FILE_DOWNLOAD_DATA_RESP:
            parseDownloadData(payload);
            break;
        case CMD_SYS_INFO_RESP:
            parseSysInfo(payload);
            break;
        case CMD_PROC_LIST_RESP:
            parseProcList(payload);
            break;
        case CMD_HEARTBEAT_RESP:
            // 心跳响应，无需处理
            break;
        default:
            emit dataReceived(payload);
            break;
    }
}

void NetworkManager::parseLogin(const QByteArray& payload)
{
    // 服务器结构：status(1) + permission(1) + message(64) = 66 bytes
    if (payload.size() < 66) return;

    quint8 status = payload[0];
    quint8 permission = payload[1];
    QString message = QString::fromUtf8(payload.mid(2, 64)).trimmed();

    bool success = (status == ERR_SUCCESS);
    if (success) {
        m_authenticated = true;
        m_permission = permission;
    }

    emit loginResult(success, permission, message);
}

void NetworkManager::parseFileOperation(const QByteArray& payload)
{
    // 服务器结构：status(1) + message(64) = 65 bytes
    if (payload.size() < 65) return;

    quint8 status = payload[0];
    QString message = QString::fromUtf8(payload.mid(1, 64)).trimmed();

    emit fileOperationResult(status == ERR_SUCCESS, message);
}

void NetworkManager::parseFileList(const QByteArray& payload)
{
    QList<FileInfo> files;

    // 即使 payload 很小（空目录），也要处理
    if (payload.size() < 2) return;

    quint8 count = payload[1];

    for (int i = 0; i < count && i < 255; i++) {
        int offset = 66 + i * 272;

        if (offset + 272 > payload.size()) break;

        FileInfo info;
        // 只取 null 字符之前的内容
        QByteArray nameBytes = payload.mid(offset, 256);
        int nullPos = nameBytes.indexOf('\0');
        if (nullPos > 0) {
            nameBytes = nameBytes.left(nullPos);
        }
        info.name = QString::fromUtf8(nameBytes);
        info.size = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset + 256);
        info.mode = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset + 260);
        info.mtime = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset + 264);
        info.isDir = payload[offset + 268] != 0;

        files.append(info);
    }

    qDebug() << "parseFileList: count=" << count << "files parsed=" << files.size();

    if (m_requestingLogFiles) {
        m_requestingLogFiles = false;
        emit logFileListResult(files);
    } else {
        emit fileListResult(files);
    }
}

void NetworkManager::parseFileRead(const QByteArray& payload)
{
    // 服务器结构：status(1) + size(4) + message(64) + content... = 69+ bytes
    if (payload.size() < 69) {
        emit fileReadResult(false, "", "Response too short");
        return;
    }

    quint8 status = payload[0];
    quint32 size = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 1);

    // message 从偏移 5 开始，64 字节
    QString message = QString::fromUtf8(payload.mid(5, 64)).trimmed();

    if (status == ERR_SUCCESS) {
        // content 从偏移 69 开始
        QString content = QString::fromUtf8(payload.mid(69, size));
        emit fileReadResult(true, content, message);
    } else {
        emit fileReadResult(false, "", message);
    }
}

// 解析传输开始响应
void NetworkManager::parseTransferStart(const QByteArray& payload)
{
    // 服务器结构：status(1) + task_id(4) + file_size(4) + message(64) = 73 bytes
    if (payload.size() < 73) return;

    quint8 status = payload[0];
    quint32 taskId = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 1);
    quint32 fileSize = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 5);
    QString message = QString::fromUtf8(payload.mid(9, 64)).trimmed();

    if (status == ERR_SUCCESS) {
        // 根据最后发送的命令判断是上传还是下载
        if (m_lastCmd == CMD_FILE_UPLOAD_START) {
            m_uploadTaskId = taskId;
        } else if (m_lastCmd == CMD_FILE_DOWNLOAD_START) {
            m_downloadTaskId = taskId;
            m_downloadTotal = fileSize;
            m_downloadReceived = 0;
        }
    }

    emit fileOperationResult(status == ERR_SUCCESS, message);
}

// 解析下载数据
void NetworkManager::parseDownloadData(const QByteArray& payload)
{
    // 服务器结构：status(1) + task_id(4) + offset(4) + data_len(4) + data...
    if (payload.size() < 13) return;

    quint8 status = payload[0];

    if (status != ERR_SUCCESS) {
        // 下载失败
        if (m_downloadFile) {
            m_downloadFile->close();
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        emit transferComplete(m_downloadFileName, false);
        return;
    }

    // 下载数据响应：task_id(4) + offset(4) + data_len(4)
    quint32 offset = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 5);
    quint32 dataLen = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 9);

    if (payload.size() > 13 && dataLen > 0) {
        QByteArray data = payload.mid(13, dataLen);
        if (m_downloadFile && m_downloadFile->isOpen()) {
            m_downloadFile->write(data);
            m_downloadReceived += dataLen;
            emit downloadProgress(m_downloadFileName, m_downloadReceived, m_downloadTotal);
        }
    }

    // 检查是否下载完成
    if (m_downloadReceived >= m_downloadTotal) {
        if (m_downloadFile) {
            m_downloadFile->close();
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        sendFileDownloadEnd(m_downloadPath);
        emit transferComplete(m_downloadFileName, true);
    } else {
        // 请求下一块数据
        if (m_downloadFile) {
            sendFileDownloadData(m_downloadPath, m_downloadReceived);
        }
    }
}

void NetworkManager::parseSysInfo(const QByteArray& payload)
{
    // 服务器发送 system_info_t 结构体 (32字节):
    // float cpu_usage(4) + uint32 mem_total(4) + mem_free(4) + mem_available(4)
    // + disk_total(4) + disk_free(4) + uptime(4) + cpu_temp(1) + padding(3)
    if (payload.size() < 32) {
        qDebug() << "parseSysInfo: payload too short" << payload.size() << "< 32";
        return;
    }

    SysInfo info;

    // 手动解析，避免 QDataStream 的问题
    // 偏移 0-3: float cpu_usage
    float cpu;
    memcpy(&cpu, payload.constData(), 4);
    info.cpuUsage = cpu;

    // 偏移 4-7: uint32 mem_total
    info.memTotal = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 4);

    // 偏移 8-11: uint32 mem_free
    info.memFree = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 8);

    // 偏移 12-15: uint32 mem_available
    // 偏移 16-19: uint32 disk_total
    info.diskTotal = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 16);

    // 偏移 20-23: uint32 disk_free
    info.diskFree = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 20);

    // 偏移 24-27: uint32 uptime
    info.uptime = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 24);

    qDebug() << "parseSysInfo: cpu=" << info.cpuUsage << "%, mem=" << info.memTotal << "/" << info.memFree
             << ", disk=" << info.diskTotal << "/" << info.diskFree << ", uptime=" << info.uptime;

    emit sysInfoResult(info);
}

void NetworkManager::parseProcList(const QByteArray& payload)
{
    QList<ProcessInfo> processes;

    // 响应格式: status(1) + count(2) + process_info_t数组
    if (payload.size() < 3) {
        qDebug() << "parseProcList: payload too short" << payload.size() << "< 3";
        return;
    }

    quint8 status = payload[0];
    quint16 count = qFromLittleEndian<quint16>((const uchar*)payload.constData() + 1);

    qDebug() << "parseProcList: status=" << status << ", count=" << count;

    // process_info_t 结构体 (121字节)
    // pid(4) + name(64) + user(32) + cpu_usage(4) + mem_usage(4) + state(1) + padding(1) + state_name(8) + mem_size(4) = 122? 不对
    // 让我看看实际结构: pid(4) + name(64) + user(32) + cpu_usage(4) + mem_usage(4) + state(1) + state_name(8) + mem_size(4) = 121
    const int PROC_INFO_SIZE = 121;

    for (int i = 0; i < count && i < 100; i++) {
        int offset = 3 + i * PROC_INFO_SIZE;

        if (offset + PROC_INFO_SIZE > payload.size()) break;

        ProcessInfo info;
        info.pid = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset);
        info.name = QString::fromUtf8(payload.mid(offset + 4, 64)).trimmed();
        info.user = QString::fromUtf8(payload.mid(offset + 68, 32)).trimmed();
        info.cpuUsage = qFromLittleEndian<float>((const uchar*)payload.constData() + offset + 100);
        info.memUsage = qFromLittleEndian<float>((const uchar*)payload.constData() + offset + 104);
        info.state = QString::fromUtf8(payload.mid(offset + 109, 8)).trimmed();
        info.memSize = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset + 117);

        processes.append(info);
    }

    qDebug() << "parseProcList: parsed" << processes.size() << "processes";
    emit procListResult(processes);
}

void NetworkManager::onHeartbeat()
{
    if (isConnected()) {
        sendFrame(CMD_HEARTBEAT, QByteArray());
    }
}

void NetworkManager::startHeartbeat(int intervalMs)
{
    m_heartbeatTimer->setInterval(intervalMs);
    m_heartbeatTimer->start();
    qDebug() << "Heartbeat started, interval:" << intervalMs << "ms";
}

void NetworkManager::stopHeartbeat()
{
    m_heartbeatTimer->stop();
    qDebug() << "Heartbeat stopped";
}

void NetworkManager::sendHeartbeat()
{
    if (isConnected()) {
        sendFrame(CMD_HEARTBEAT, QByteArray());
        qDebug() << "Heartbeat sent";
    }
}

// 分片上传文件
void NetworkManager::uploadFileChunked(const QString& localPath, const QString& remotePath)
{
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit transferComplete(localPath, false);
        return;
    }

    quint32 fileSize = file.size();
    QString fileName = QFileInfo(localPath).fileName();

    // 发送开始请求
    sendFileUploadStart(remotePath, fileSize);

    // 分片上传数据 (每片 8KB)
    const quint32 CHUNK_SIZE = 8 * 1024;
    quint32 offset = 0;

    while (offset < fileSize) {
        quint32 remaining = fileSize - offset;
        quint32 chunkSize = qMin(remaining, CHUNK_SIZE);

        QByteArray data = file.read(chunkSize);
        if (data.isEmpty()) break;

        sendFileUploadData(remotePath, offset, data);
        offset += chunkSize;

        // 发送进度
        emit uploadProgress(fileName, offset, fileSize);
    }

    file.close();

    // 发送结束请求
    sendFileUploadEnd(remotePath);

    emit transferComplete(fileName, true);
}

// 分片下载开始
void NetworkManager::startDownload(const QString& remotePath, const QString& localPath)
{
    // 初始化下载状态
    m_downloadPath = remotePath;
    m_downloadFileName = QFileInfo(remotePath).fileName();
    m_downloadSavePath = localPath;
    m_downloadTotal = 0;
    m_downloadReceived = 0;

    // 关闭之前的下载文件
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
    }

    // 打开文件准备写入
    m_downloadFile = new QFile(localPath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit transferComplete(m_downloadFileName, false);
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

    // 发送下载开始请求
    sendFileDownloadStart(remotePath);
}

void NetworkManager::sendFileDownloadStart(const QString& path)
{
    QByteArray payload;
    payload.resize(512);
    QByteArray pathBytes = path.toUtf8();
    memcpy(payload.data(), pathBytes.constData(), qMin(pathBytes.size(), 512));

    // 文件大小设为0表示未知
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << (quint32)0;

    sendFrame(CMD_FILE_DOWNLOAD_START, payload);
}

void NetworkManager::sendFileDownloadData(const QString& path, quint32 offset)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // task_id (4字节)
    stream << m_downloadTaskId;
    // 偏移量 (4字节)
    stream << offset;

    sendFrame(CMD_FILE_DOWNLOAD_DATA, payload);
}

// 分片下载结束
void NetworkManager::sendFileDownloadEnd(const QString& path)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // task_id (4字节)
    stream << m_downloadTaskId;

    sendFrame(CMD_FILE_DOWNLOAD_END, payload);
}
