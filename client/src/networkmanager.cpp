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
    , m_downloadFile(nullptr)
    , m_downloadTotal(0)
    , m_downloadReceived(0)
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
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // 路径 (512字节)
    QByteArray pathBytes = path.toUtf8();
    pathBytes.resize(512);
    payload.append(pathBytes);

    // 文件大小 (4字节)
    stream << fileSize;

    sendFrame(CMD_FILE_UPLOAD_START, payload);
}

void NetworkManager::sendFileUploadData(const QString& path, quint32 offset, const QByteArray& data)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // 路径 (512字节)
    QByteArray pathBytes = path.toUtf8();
    pathBytes.resize(512);
    payload.append(pathBytes);

    // 偏移量 (4字节)
    stream << offset;

    // 数据
    payload.append(data);

    sendFrame(CMD_FILE_UPLOAD_DATA, payload);
}

void NetworkManager::sendFileUploadEnd(const QString& path)
{
    QByteArray payload;
    payload.resize(512);
    QByteArray pathBytes = path.toUtf8();
    memcpy(payload.data(), pathBytes.constData(), qMin(pathBytes.size(), 512));

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
    qDebug() << "Disconnected from server";
    m_heartbeatTimer->stop();
    m_authenticated = false;
    emit disconnected();
}

void NetworkManager::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    qDebug() << "Socket error:" << m_socket->errorString();
    emit connectionError(m_socket->errorString());
}

void NetworkManager::onReadyRead()
{
    m_recvBuffer.append(m_socket->readAll());

    // 解析帧
    while (m_recvBuffer.size() >= FRAME_HEADER_SIZE) {
        ProtocolFrame frame;
        QByteArray payload;

        if (!ProtocolUtil::unpackFrame(m_recvBuffer, frame, payload)) {
            // 数据不完整，等待更多数据
            break;
        }

        // 从缓冲区移除已处理的帧
        m_recvBuffer.remove(0, FRAME_HEADER_SIZE + frame.length);

        qDebug() << "Received frame:" << ProtocolUtil::cmdToString(frame.cmd)
                 << "seq:" << frame.seq << "len:" << frame.length;

        processResponse(payload);
    }
}

void NetworkManager::processResponse(const QByteArray& payload)
{
    if (payload.isEmpty()) return;

    switch (m_lastCmd) {
        case CMD_FILE_LIST:
            parseFileList(payload);
            break;
        case CMD_FILE_READ:
            parseFileRead(payload);
            break;
        case CMD_FILE_DELETE:
        case CMD_FILE_RENAME:
        case CMD_FILE_EDIT:
        case CMD_FILE_UPLOAD_START:
        case CMD_FILE_UPLOAD_DATA:
        case CMD_FILE_UPLOAD_END:
        case CMD_FILE_DOWNLOAD_END:
            parseFileOperation(payload);
            break;
        case CMD_FILE_DOWNLOAD_START:
        case CMD_FILE_DOWNLOAD_DATA:
            parseDownloadData(payload);
            break;
        case CMD_SYS_INFO:
            parseSysInfo(payload);
            break;
        case CMD_PROC_LIST:
            parseProcList(payload);
            break;
        case CMD_LOGIN:
            parseLogin(payload);
            break;
        default:
            emit dataReceived(payload);
            break;
    }
}

void NetworkManager::parseLogin(const QByteArray& payload)
{
    if (payload.size() < 2) return;

    quint8 status = payload[0];
    QString message = QString::fromUtf8(payload.mid(1, 63)).trimmed();
    int perm = payload[64];

    bool success = (status == ERR_SUCCESS);
    if (success) {
        m_authenticated = true;
        m_permission = perm;
    }

    emit loginResult(success, perm, message);
}

void NetworkManager::parseFileOperation(const QByteArray& payload)
{
    if (payload.isEmpty()) return;

    quint8 status = payload[0];
    QString message;

    // 获取错误消息（如果有）
    if (payload.size() > 1) {
        message = QString::fromUtf8(payload.mid(1, 63)).trimmed();
    }

    emit fileOperationResult(status == ERR_SUCCESS, message);
}

void NetworkManager::parseFileList(const QByteArray& payload)
{
    QList<FileInfo> files;

    if (payload.size() < 67) return;

    quint8 count = payload[1];

    for (int i = 0; i < count && i < 255; i++) {
        int offset = 66 + i * 272;

        if (offset + 272 > payload.size()) break;

        FileInfo info;
        info.name = QString::fromUtf8(payload.mid(offset, 256)).trimmed();
        info.size = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset + 256);
        info.mode = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset + 260);
        info.mtime = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset + 264);
        info.isDir = payload[offset + 268] != 0;

        files.append(info);
    }

    emit fileListResult(files);
}

void NetworkManager::parseFileRead(const QByteArray& payload)
{
    if (payload.size() < 70) {
        emit fileReadResult(false, "", "Response too short");
        return;
    }

    quint8 status = payload[0];
    quint32 size = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 1);

    QString message = QString::fromUtf8(payload.mid(69, 64)).trimmed();

    if (status == ERR_SUCCESS) {
        QString content = QString::fromUtf8(payload.mid(69, size));
        emit fileReadResult(true, content, message);
    } else {
        emit fileReadResult(false, "", message);
    }
}

// 解析下载数据
void NetworkManager::parseDownloadData(const QByteArray& payload)
{
    if (payload.size() < 13) return;

    quint8 status = payload[0];
    QString message = QString::fromUtf8(payload.mid(5, 64)).trimmed();

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

    // 检查是否是下载开始响应（有文件大小信息）
    if (m_downloadTotal == 0) {
        // 下载开始响应 - 获取文件大小
        m_downloadTotal = qFromLittleEndian<quint32>((const uchar*)payload.constData() + 5);
        // 继续请求下载数据
        if (m_downloadFile) {
            sendFileDownloadData(m_downloadPath, 0);
        }
        return;
    }

    // 下载数据响应
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
    if (payload.size() < 28) return;

    SysInfo info;
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> info.cpuUsage;
    stream >> info.memTotal;
    stream >> info.memFree;
    // skip 4 bytes
    stream >> info.diskTotal;
    stream >> info.diskFree;
    stream >> info.uptime;

    emit sysInfoResult(info);
}

void NetworkManager::parseProcList(const QByteArray& payload)
{
    QList<ProcessInfo> processes;

    if (payload.size() < 3) return;

    quint16 count = qFromLittleEndian<quint16>((const uchar*)payload.constData() + 1);

    // process_info_t 结构体布局 (121 bytes packed):
    for (int i = 0; i < count && i < 100; i++) {
        int offset = 3 + i * 121;

        if (offset + 121 > payload.size()) break;

        ProcessInfo info;
        info.pid = qFromLittleEndian<quint32>((const uchar*)payload.constData() + offset);
        info.name = QString::fromUtf8(payload.mid(offset + 4, 64)).trimmed();
        info.user = QString::fromUtf8(payload.mid(offset + 68, 32)).trimmed();
        info.cpuUsage = 0;
        info.memUsage = 0;
        info.state = QString::fromUtf8(payload.mid(offset + 109, 8)).trimmed();

        processes.append(info);
    }

    emit procListResult(processes);
}

void NetworkManager::onHeartbeat()
{
    if (isConnected()) {
        sendFrame(CMD_HEARTBEAT, QByteArray());
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
    payload.resize(512);
    QByteArray pathBytes = path.toUtf8();
    memcpy(payload.data(), pathBytes.constData(), qMin(pathBytes.size(), 512));

    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << offset;

    sendFrame(CMD_FILE_DOWNLOAD_DATA, payload);
}

// 分片下载结束
void NetworkManager::sendFileDownloadEnd(const QString& path)
{
    QByteArray payload;
    payload.resize(512);
    QByteArray pathBytes = path.toUtf8();
    memcpy(payload.data(), pathBytes.constData(), qMin(pathBytes.size(), 512));

    sendFrame(CMD_FILE_DOWNLOAD_END, payload);
}
