#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include "protocolutil.h"

struct FileInfo {
    QString name;
    quint32 size;
    quint32 mode;
    quint32 mtime;
    bool isDir;
};

struct SysInfo {
    float cpuUsage;
    quint32 memTotal;
    quint32 memFree;
    quint32 diskTotal;
    quint32 diskFree;
    quint32 uptime;
};

struct ProcessInfo {
    quint32 pid;
    QString name;
    QString user;
    float cpuUsage;
    float memUsage;
    QString state;
    quint32 memSize;  // 内存大小 KB
};

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    // 连接状态
    bool isConnected() const { return m_socket && m_socket->state() == QAbstractSocket::ConnectedState; }
    bool isAuthenticated() const { return m_authenticated; }
    int permission() const { return m_permission; }

    // 连接/断开
    void connectToServer(const QString& ip, quint16 port);
    void disconnect();

    // 登录/登出
    void sendLogin(const QString& username, const QString& password);
    void sendLogout();

    // 文件操作
    void sendFileList(const QString& path);
    void sendLogFileList(const QString& path);
    void sendFileRead(const QString& path);
    void sendFileDelete(const QString& path);
    void sendFileRename(const QString& oldPath, const QString& newPath);
    void sendFileEdit(const QString& path, const QString& content);

    // 分片上传
    void sendFileUploadStart(const QString& path, quint32 fileSize);
    void sendFileUploadData(const QString& path, quint32 offset, const QByteArray& data);
    void sendFileUploadEnd(const QString& path);
    void uploadFileChunked(const QString& localPath, const QString& remotePath);

    // 分片下载
    void startDownload(const QString& remotePath, const QString& localPath);
    void sendFileDownloadStart(const QString& path);
    void sendFileDownloadData(const QString& path, quint32 offset);
    void sendFileDownloadEnd(const QString& path);

    // 系统信息
    void sendSysInfo();
    void sendProcList();

    // 心跳
    void startHeartbeat(int intervalMs = 30000);
    void stopHeartbeat();
    void sendHeartbeat();

signals:
    // 连接状态
    void connected();
    void disconnected();
    void connectionError(const QString& error);

    // 登录结果
    void loginResult(bool success, int perm, const QString& message);

    // 文件操作结果
    void fileListResult(const QList<FileInfo>& files);
    void logFileListResult(const QList<FileInfo>& files);
    void fileReadResult(bool success, const QString& content, const QString& message);
    void fileOperationResult(bool success, const QString& message);

    // 分片传输进度
    void uploadProgress(const QString& fileName, quint32 transferred, quint32 total);
    void downloadProgress(const QString& fileName, quint32 transferred, quint32 total);
    void downloadDataReceived(const QString& fileName, const QByteArray& data);
    void transferComplete(const QString& fileName, bool success);

    // 系统信息结果
    void sysInfoResult(const SysInfo& info);
    void procListResult(const QList<ProcessInfo>& processes);

    // 收到数据
    void dataReceived(const QByteArray& data);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void onHeartbeat();

private:
    void sendFrame(uint16_t cmd, const QByteArray& data);
    void processResponse(uint16_t cmd, const QByteArray& data);
    void parseLogin(const QByteArray& payload);
    void parseFileOperation(const QByteArray& payload);
    void parseFileList(const QByteArray& payload);
    void parseFileRead(const QByteArray& payload);
    void parseDownloadData(const QByteArray& payload);
    void parseTransferStart(const QByteArray& payload);
    void parseSysInfo(const QByteArray& payload);
    void parseProcList(const QByteArray& payload);

private:
    QTcpSocket* m_socket;
    QTimer* m_heartbeatTimer;
    uint32_t m_seq;
    uint16_t m_lastCmd;  // 最后发送的命令，用于解析响应
    bool m_authenticated;
    int m_permission;

    // 接收缓冲区
    QByteArray m_recvBuffer;

    // 日志请求标记
    bool m_requestingLogFiles;

    // 下载状态
    QString m_downloadPath;
    QString m_downloadFileName;
    QString m_downloadSavePath;
    QFile* m_downloadFile;
    quint32 m_downloadTotal;
    quint32 m_downloadReceived;
    quint32 m_uploadTaskId;
    quint32 m_downloadTaskId;
};

#endif // NETWORKMANAGER_H
