/*
 * IMX6ULL FTP Server - Network Module Header
 * Version: 1.0
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Windows 兼容
 * ============================================================ */
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    typedef int socklen_t;
    #define SOCKET_INVALID INVALID_SOCKET
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define SOCKET_CLOSE closesocket
    #define SOCKET_INIT() network_init()
    #define SOCKET_CLEANUP() WSACleanup()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    typedef socklen_t socklen_t;
    #define SOCKET_INVALID -1
    #define SOCKET_ERROR_CODE errno
    #define SOCKET_CLOSE close
    #define SOCKET_INIT() (0)
    #define SOCKET_CLEANUP()
#endif

/* ============================================================
 * Socket 操作函数
 * ============================================================ */

/**
 * 初始化网络库（Windows WSAStartup）
 * @return 0成功，-1失败
 */
int network_init(void);

/**
 * 创建监听socket
 * @param port 端口号
 * @return 监听socket，失败返回-1
 */
socket_t tcp_listen(uint16_t port);

/**
 * 接受客户端连接
 * @param listenfd 监听socket
 * @param client_addr 客户端地址（可为NULL）
 * @return 客户端socket，失败返回-1
 */
socket_t tcp_accept(socket_t listenfd, struct sockaddr_in* client_addr);

/**
 * 连接服务器
 * @param ip 服务器IP
 * @param port 服务器端口
 * @return socket，失败返回-1
 */
socket_t tcp_connect(const char* ip, uint16_t port);

/**
 * 发送数据
 * @param sock socket
 * @param buf 数据缓冲区
 * @param len 数据长度
 * @return 发送字节数，失败返回-1
 */
int tcp_send(socket_t sock, const void* buf, size_t len);

/**
 * 接收数据
 * @param sock socket
 * @param buf 数据缓冲区
 * @param len 缓冲区长度
 * @param timeout_ms 超时毫秒，0表示阻塞
 * @return 接收字节数，0表示连接关闭，-1表示错误
 */
int tcp_recv(socket_t sock, void* buf, size_t len, int timeout_ms);

/**
 * 关闭socket
 */
void tcp_close(socket_t sock);

/**
 * 设置socket为非阻塞模式
 */
int tcp_set_nonblocking(socket_t sock, bool enable);

/**
 * 设置TCP_NODELAY
 */
int tcp_set_nodelay(socket_t sock, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_H */