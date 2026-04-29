# IMX6ULL FTP Server

嵌入式设备管理器服务器端，运行在 IMX6ULL Linux 平台。

## 项目简介

基于自定义二进制协议的轻量级 FTP 服务器，提供：
- 用户认证与权限管理
- 文件浏览、上传、下载、删除
- 系统监控（CPU、内存、磁盘、进程）
- 多线程并发处理
- 断点续传支持

## 目录结构

```
server/
├── include/              # 头文件目录
│   ├── server.h          # 主头文件
│   ├── log.h             # 日志模块
│   ├── config.h          # 配置模块
│   ├── network.h         # 网络模块
│   ├── thread_pool.h     # 线程池
│   ├── session.h         # 会话管理
│   ├── handler.h         # 客户端处理
│   └── sysinfo.h         # 系统信息采集
│
├── src/                  # 源文件目录
│   ├── main.c            # 主程序入口
│   ├── log.c             # 日志实现
│   ├── config.c          # 配置解析
│   ├── network.c         # Socket 封装
│   ├── thread_pool.c     # 线程池实现
│   ├── session.c         # 会话管理实现
│   ├── handler.c         # 命令处理实现
│   └── sysinfo.c         # 系统信息采集实现
│
├── Makefile              # 编译脚本
├── server.conf           # 配置文件示例
└── README.md             # 本文档
```

## 编译说明

### 本地编译（Windows x86 测试）

使用 MinGW GCC 编译测试版本：

```bash
cd server
make
```

生成的可执行文件：`server.exe`（Windows）或 `server`（Linux）

### 交叉编译（ARM 平台）

#### 1. 安装 ARM 工具链

下载 Windows 版 ARM Linux 工具链：
https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

选择：`AArch32 GNU/Linux target with hard float (arm-none-linux-gnueabihf)`

解压到 `C:\toolchain\`

#### 2. 编译

```bash
cd server
make CROSS_COMPILE=arm-none-linux-gnueabihf-
```

或使用完整路径：

```bash
make CROSS_COMPILE=/c/toolchain/gcc-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
```

生成 ARM 可执行文件：`server`

#### 3. 验证编译结果

```bash
file server
# 输出: server: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV) ...
```

### 清理

```bash
make clean
```

## 配置文件

配置文件路径：`server.conf`（默认）

### 配置示例

```ini
[server]
port = 8888               # 监听端口
max_connections = 10      # 最大连接数
timeout = 300             # 超时时间（秒）
log_level = info          # 日志级别: debug/info/warn/error

[users]
admin:admin123:rw         # 用户名:密码MD5:权限（rw=读写，ro=只读）
guest:guest123:ro

[paths]
root_dir = /home/root     # 根目录
upload_dir = /home/root/uploads  # 上传目录
log_dir = ./logs          # 日志目录

[transfer]
max_concurrent = 3        # 最大并发传输数
max_file_size = 1073741824  # 最大文件大小（1GB）
buffer_size = 8192        # 传输缓冲区大小（8KB）
```

### 权限说明

- `rw`：读写权限，可以上传、下载、删除、重命名文件
- `ro`：只读权限，仅可浏览、下载文件

### 密码格式

密码字段存储 **MD5 值**（32位十六进制字符串）。

生成方法：
```bash
echo -n "admin123" | md5sum
# 输出: 0192023a7bbd73250516f069df18b500
```

## 运行说明

### 启动服务器

```bash
./server                  # 使用默认配置 server.conf
./server myconfig.conf    # 使用自定义配置文件
```

### 后台运行

```bash
nohup ./server > /dev/null 2>&1 &
```

### 停止服务器

发送 SIGINT 或 SIGTERM 信号：

```bash
kill -SIGTERM <pid>
# 或
Ctrl+C（前台运行时）
```

## 功能列表

### 认证功能

| 命令 | 说明 |
|------|------|
| CMD_LOGIN (0x0101) | 用户登录，返回权限 |
| CMD_LOGOUT (0x0103) | 用户登出，清理会话 |

### 文件操作

| 命令 | 说明 | 权限要求 |
|------|------|---------|
| CMD_FILE_LIST (0x0201) | 查询文件列表 | ro/rw |
| CMD_FILE_READ (0x0203) | 查看文件内容（≤64KB） | ro/rw |
| CMD_FILE_DELETE (0x0205) | 删除文件/目录 | rw |
| CMD_FILE_RENAME (0x0207) | 重命名文件 | rw |

### 文件传输

| 命令 | 说明 | 权限要求 |
|------|------|---------|
| CMD_FILE_UPLOAD_START (0x0301) | 开始上传 | rw |
| CMD_FILE_UPLOAD_DATA (0x0303) | 上传数据块 | rw |
| CMD_FILE_UPLOAD_PAUSE (0x0305) | 暂停上传 | rw |
| CMD_FILE_UPLOAD_RESUME (0x0307) | 恢复上传 | rw |
| CMD_FILE_UPLOAD_END (0x0309) | 结束上传 | rw |
| CMD_FILE_DOWNLOAD_START (0x030B) | 开始下载 | ro/rw |
| CMD_FILE_DOWNLOAD_DATA (0x030D) | 下载数据块 | ro/rw |
| CMD_FILE_DOWNLOAD_PAUSE (0x030F) | 暂停下载 | ro/rw |
| CMD_FILE_DOWNLOAD_RESUME (0x0311) | 恢复下载 | ro/rw |
| CMD_FILE_DOWNLOAD_END (0x0313) | 结束下载 | ro/rw |

### 系统监控

| 命令 | 说明 |
|------|------|
| CMD_SYS_INFO (0x0401) | 查询系统信息（CPU/内存/磁盘） |
| CMD_PROC_LIST (0x0403) | 查询进程列表 |

### 心跳机制

| 命令 | 说明 |
|------|------|
| CMD_HEARTBEAT (0x0501) | 心跳请求，更新活跃时间 |

## 协议说明

### 帧格式（二进制，小端序）

```
帧头（12字节）:
  uint32_t magic      # 魔数 0x46545053 ('FTPS')
  uint16_t version    # 协议版本 1
  uint16_t cmd        # 命令字
  uint32_t seq        # 序列号
  uint32_t length     # 数据长度
  uint32_t checksum   # CRC32校验

数据载荷:
  根据命令字不同，载荷结构不同（见 protocol/protocol.h）
```

### 最大限制

- 单帧最大载荷：8KB
- 文件查看最大：64KB
- 最大并发传输：3 个文件
- 最大文件大小：1GB（可配置）
- 最大连接数：10（可配置）

## 日志系统

### 日志级别

- `debug`：调试信息（开发阶段）
- `info`：正常操作信息（生产环境）
- `warn`：警告信息
- `error`：错误信息

### 日志输出

- **文件**：`./logs/server_YYYY-MM-DD.log`
- **终端**：stderr

### 日志滚动

按日期自动滚动，每天一个日志文件。

## 安全机制

### 路径安全检查

所有文件操作都会检查路径是否包含 `..`，防止目录穿越攻击。

### 权限检查

- 未认证用户：拒绝所有操作
- `ro` 用户：仅允许浏览、下载
- `rw` 用户：允许所有操作

### 超时机制

客户端超时（默认300秒）后自动断开连接，清理会话。

## 性能参数

### 线程配置

- 工作线程数：4（固定）
- 线程池任务队列：无限制

### 传输性能

- 传输缓冲区：8KB（可配置）
- 单次最大传输：8KB 数据块
- 并发传输限制：3 个文件

## 部署建议

### IMX6ULL 平台

1. **交叉编译**：
   ```bash
   make CROSS_COMPILE=arm-none-linux-gnueabihf-
   ```

2. **传输到板子**：
   ```bash
   scp server root@192.168.1.100:/usr/bin/
   scp server.conf root@192.168.1.100:/etc/ftpserver/
   ```

3. **创建必要目录**：
   ```bash
   mkdir -p /home/root/uploads
   mkdir -p /var/log/ftpserver
   ```

4. **设置权限**：
   ```bash
   chmod +x /usr/bin/server
   chmod 600 /etc/ftpserver/server.conf  # 保护密码
   ```

5. **启动服务**：
   ```bash
   /usr/bin/server /etc/ftpserver/server.conf
   ```

### 开机自启动

创建 systemd 服务文件 `/etc/systemd/system/ftpserver.service`：

```ini
[Unit]
Description=IMX6ULL FTP Server
After=network.target

[Service]
Type=simple
ExecStart=/usr/bin/server /etc/ftpserver/server.conf
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
```

启用服务：
```bash
systemctl enable ftpserver
systemctl start ftpserver
```

## 常见问题

### 1. 端口被占用

修改 `server.conf` 中的 `port` 字段。

### 2. 权限不足

检查用户权限配置和文件系统权限。

### 3. 连接超时

检查网络连接，调整 `timeout` 配置。

### 4. 传输中断

支持断点续传，客户端可恢复传输。

### 5. 日志文件过大

日志按日期滚动，可定期清理旧日志。

## 开发说明

### 添加新命令

1. 在 `protocol/protocol.h` 中定义命令字和数据结构
2. 在 `server/include/handler.h` 中添加处理函数声明
3. 在 `server/src/handler.c` 中实现处理逻辑
4. 在 `handle_client()` 函数中添加命令分发

### 模块依赖

```
main.c
  ├─ config.c (配置加载)
  ├─ log.c (日志系统)
  ├─ thread_pool.c (线程池)
  ├─ network.c (网络通信)
  └─ handler.c (命令处理)
       ├─ session.c (会话管理)
       └─ sysinfo.c (系统信息)
```

## 版本历史

- **v1.0** (2026-04-29): 完整功能实现
  - 用户认证与权限
  - 文件操作
  - 文件传输（断点续传）
  - 系统监控
  - 多线程处理

## 作者

嵌入式设备管理器项目

## 许可证

MIT License