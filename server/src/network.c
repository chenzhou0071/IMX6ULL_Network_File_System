/*
 * IMX6ULL FTP Server - Network Module Implementation
 * Version: 1.0
 */

#include "network.h"
#include "log.h"
#include <string.h>
#include <errno.h>

/* ============================================================
 * 初始化网络库
 * ============================================================ */
#ifdef _WIN32
static int g_wsa_initialized = 0;
#endif

int network_init(void) {
#ifdef _WIN32
    if (g_wsa_initialized) return 0;

    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        LOG_ERROR("WSAStartup failed: %d", result);
        return -1;
    }
    g_wsa_initialized = 1;
    LOG_INFO("WSA initialized");
#endif
    return 0;
}

/* ============================================================
 * 创建监听socket
 * ============================================================ */
socket_t tcp_listen(uint16_t port) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == SOCKET_INVALID) {
        LOG_ERROR("Failed to create socket: %d", SOCKET_ERROR_CODE);
        return SOCKET_INVALID;
    }

    /* 复用地址 */
    int opt = 1;
#ifdef _WIN32
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind socket to port %d: %d", port, SOCKET_ERROR_CODE);
        SOCKET_CLOSE(sock);
        return SOCKET_INVALID;
    }

    /* 监听 */
    if (listen(sock, 10) < 0) {
        LOG_ERROR("Failed to listen on port %d: %d", port, SOCKET_ERROR_CODE);
        SOCKET_CLOSE(sock);
        return SOCKET_INVALID;
    }

    LOG_INFO("Server listening on port %d", port);
    return sock;
}

/* ============================================================
 * 接受客户端连接
 * ============================================================ */
socket_t tcp_accept(socket_t listenfd, struct sockaddr_in* client_addr) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    socket_t clientfd = accept(listenfd, (struct sockaddr*)&addr, &addrlen);
    if (clientfd == SOCKET_INVALID) {
        return SOCKET_INVALID;
    }

    if (client_addr != NULL) {
        *client_addr = addr;
    }

    LOG_INFO("Client connected: %s:%d",
             inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    return clientfd;
}

/* ============================================================
 * 连接服务器
 * ============================================================ */
socket_t tcp_connect(const char* ip, uint16_t port) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == SOCKET_INVALID) {
        return SOCKET_INVALID;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        SOCKET_CLOSE(sock);
        return SOCKET_INVALID;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        SOCKET_CLOSE(sock);
        return SOCKET_INVALID;
    }

    return sock;
}

/* ============================================================
 * 发送数据
 * ============================================================ */
int tcp_send(socket_t sock, const void* buf, size_t len) {
    if (buf == NULL || len == 0) return 0;

    int total_sent = 0;
    const char* ptr = (const char*)buf;

    while (total_sent < (int)len) {
        int sent = send(sock, ptr + total_sent, len - total_sent, 0);
        if (sent < 0) {
            if (SOCKET_ERROR_CODE == EINTR || SOCKET_ERROR_CODE == EAGAIN) {
                continue;
            }
            return -1;
        }
        if (sent == 0) {
            break;
        }
        total_sent += sent;
    }

    return total_sent;
}

/* ============================================================
 * 接收数据
 * ============================================================ */
int tcp_recv(socket_t sock, void* buf, size_t len, int timeout_ms) {
    if (buf == NULL || len == 0) return 0;

    if (timeout_ms > 0) {
#ifdef _WIN32
        DWORD tv = timeout_ms;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    }

    int received = recv(sock, (char*)buf, len, 0);

    if (timeout_ms > 0) {
#ifdef _WIN32
        DWORD tv = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    }

    return received;
}

/* ============================================================
 * 关闭socket
 * ============================================================ */
void tcp_close(socket_t sock) {
    if (sock != SOCKET_INVALID) {
        SOCKET_CLOSE(sock);
    }
}

/* ============================================================
 * 设置非阻塞模式
 * ============================================================ */
int tcp_set_nonblocking(socket_t sock, bool enable) {
#ifdef _WIN32
    u_long mode = enable ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    if (enable) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(sock, F_SETFL, flags);
#endif
}

/* ============================================================
 * 设置TCP_NODELAY
 * ============================================================ */
int tcp_set_nodelay(socket_t sock, bool enable) {
    int opt = enable ? 1 : 0;
    return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));
}