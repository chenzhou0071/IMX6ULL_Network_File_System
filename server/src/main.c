/*
 * IMX6ULL FTP Server - Main Entry Point
 * Version: 1.0
 */

#include "server.h"
#include <signal.h>

/* ============================================================
 * 全局变量
 * ============================================================ */
server_config_t* g_config = NULL;
thread_pool_t* g_thread_pool = NULL;
volatile int g_running = 1;

/* ============================================================
 * 信号处理
 * ============================================================ */
void signal_handler(int sig) {
    (void)sig;
    LOG_INFO("Received signal, shutting down...");
    g_running = 0;
}

/* ============================================================
 * 显示帮助信息
 * ============================================================ */
static void print_usage(const char* prog_name) {
    printf("IMX6ULL FTP Server v1.0\n");
    printf("Usage: %s [config_file]\n", prog_name);
    printf("  config_file  Configuration file path (default: server.conf)\n");
    printf("\nExample:\n");
    printf("  %s server.conf\n", prog_name);
    printf("\nBuild for ARM:\n");
    printf("  make CROSS_COMPILE=arm-linux-gnueabihf-\n");
}

/* ============================================================
 * 主程序
 * ============================================================ */
int main(int argc, char* argv[]) {
    const char* config_file = "server.conf";

    /* 解析命令行参数 */
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        config_file = argv[1];
    }

    printf("=========================================\n");
    printf("  IMX6ULL FTP Server v1.0\n");
    printf("=========================================\n\n");

    /* 初始化网络 */
    if (SOCKET_INIT() != 0) {
        fprintf(stderr, "Failed to initialize network\n");
        return 1;
    }

    /* 加载配置 */
    g_config = config_load(config_file);
    if (g_config == NULL) {
        fprintf(stderr, "Failed to load config: %s\n", config_file);
        SOCKET_CLEANUP();
        return 1;
    }

    /* 初始化日志 */
    log_init(g_config->log_level, NULL);
    LOG_INFO("Server starting...");

    /* 设置信号处理 */
#ifdef _WIN32
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#else
    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif

    /* 创建线程池 */
    g_thread_pool = thread_pool_create(4);
    if (g_thread_pool == NULL) {
        LOG_ERROR("Failed to create thread pool");
        config_free(g_config);
        log_close();
        SOCKET_CLEANUP();
        return 1;
    }

    /* 创建监听socket */
    socket_t listenfd = tcp_listen(g_config->port);
    if (listenfd == SOCKET_INVALID) {
        LOG_ERROR("Failed to create listen socket");
        thread_pool_destroy(g_thread_pool);
        config_free(g_config);
        log_close();
        SOCKET_CLEANUP();
        return 1;
    }

    LOG_INFO("Server listening on port %d", g_config->port);
    printf("Server listening on port %d\n", g_config->port);

    /* 主循环 */
    while (g_running) {
        struct sockaddr_in client_addr;
        socket_t clientfd = tcp_accept(listenfd, &client_addr);

        if (clientfd == SOCKET_INVALID) {
            if (g_running) {
                LOG_WARN("Failed to accept client connection");
            }
            continue;
        }

        /* 创建会话 */
        client_session_t* sess = session_create(clientfd, &client_addr);
        if (sess == NULL) {
            LOG_ERROR("Failed to create session");
            SOCKET_CLOSE(clientfd);
            continue;
        }

        /* 提交任务到线程池 */
        if (thread_pool_submit(g_thread_pool, handle_client, sess) != 0) {
            LOG_ERROR("Failed to submit client handler");
            session_destroy(sess);
            continue;
        }
    }

    /* 清理 */
    LOG_INFO("Server shutting down...");
    SOCKET_CLOSE(listenfd);
    thread_pool_destroy(g_thread_pool);
    config_free(g_config);
    log_close();
    SOCKET_CLEANUP();

    printf("Server stopped.\n");
    return 0;
}