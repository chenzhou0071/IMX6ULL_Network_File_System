# IMX6ULL 网络文件管理系统

基于 IMX6ULL ARM Linux 开发板的自定义 TCP 协议网络文件服务器，使用 C 语言实现服务器端，Qt5 实现 Windows 客户端。

## 功能特性

### 服务器端
- 自定义二进制协议（20字节帧头 + CRC32校验）
- 用户认证登录（MD5密码验证）
- 文件管理：列表、查看、编辑、删除、重命名
- 分片文件上传下载（8KB分片）
- 系统信息监控（CPU、内存、磁盘）
- 进程列表查看
- 心跳保活机制

### Qt客户端
- 文件管理器：上传、下载、删除、重命名
- 系统监控：实时CPU/内存/磁盘信息、进程列表
- 日志查看器
- 分片传输显示进度

## 协议格式

```
帧头 (20字节):
+--------+--------+--------+--------+--------+
| Magic  | Ver    | Cmd    | Seq    | Length |
| 4B     | 2B     | 2B     | 4B      | 4B     |
+--------+--------+--------+--------+--------+
| Checksum (4B)                           |
+-----------------------------------------+

数据: 自定义长度
```

## 目录结构

```
.
├── protocol/          # 协议定义头文件
├── server/            # C语言服务器
│   ├── src/           # 源代码
│   └── include/       # 头文件
├── client/            # Qt5客户端
│   ├── src/           # 源代码
│   ├── include/       # 头文件
│   └── ui/            # UI文件
└── tests/             # Python测试脚本
```

## 编译

### 服务器（ARM交叉编译）
```bash
cd server
make CROSS_COMPILE=arm-linux-gnueabihf-
```

### Qt客户端（Windows）
```bash
cd client
E:\Qt\6.5.3\mingw_64\bin\qmake IMX6ULLClient.pro
mingw32-make
```

## 使用

1. 服务器部署到 IMX6ULL 开发板运行
2. Windows客户端连接服务器IP:8888
3. 默认账号: admin / admin

## 技术栈

- 服务器: C语言、Linux socket、多线程
- 客户端: Qt6、TCP Socket、自定义协议
- 工具链: ARM GNU Toolchain、MinGW、qmake
