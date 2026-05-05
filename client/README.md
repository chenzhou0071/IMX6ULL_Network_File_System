# IMX6ULL FTP 客户端

Qt 实现的嵌入式设备管理器客户端，用于远程管理和监控 IMX6ULL 设备。

## 功能特性

- **连接管理**: 支持 TCP 连接、登录认证
- **文件管理**: 浏览、上传、下载、删除、重命名、查看文件
- **系统监控**: 实时查看 CPU、内存、磁盘使用率和进程列表
- **传输监控**: 文件传输进度、速度显示
- **日志查看**: 远程查看服务器日志

## 环境要求

- Qt 6.5.3 或更高版本
- MinGW 编译器
- Windows 操作系统

## 目录结构

```
client/
├── include/           # 头文件
│   ├── protocolutil.h      # 协议工具类
│   ├── networkmanager.h    # 网络管理器
│   ├── mainwindow.h        # 主窗口
│   ├── settingsdialog.h    # 设置对话框
│   ├── filemanagerwindow.h # 文件管理器
│   ├── transferprogressdialog.h # 传输进度
│   ├── systemmonitorwindow.h   # 系统监控
│   └── logviewerwindow.h   # 日志查看器
├── src/               # 源文件
│   ├── main.cpp
│   ├── protocolutil.cpp
│   ├── networkmanager.cpp
│   ├── mainwindow.cpp
│   ├── settingsdialog.cpp
│   ├── filemanagerwindow.cpp
│   ├── transferprogressdialog.cpp
│   ├── systemmonitorwindow.cpp
│   └── logviewerwindow.cpp
├── ui/                # UI 文件
│   ├── mainwindow.ui
│   ├── settingsdialog.ui
│   ├── filemanagerwindow.ui
│   ├── transferprogressdialog.ui
│   ├── systemmonitorwindow.ui
│   └── logviewerwindow.ui
├── Makefile           # 构建脚本
└── README.md          # 本文件
```

## 编译说明

### 1. 配置 Qt 路径

编辑 `Makefile`，修改 Qt 安装路径：

```makefile
QT_DIR := E:/Qt/6.5.3/mingw1310_64
```

### 2. 编译

在 Qt 命令行或配置了 Qt 环境的终端中执行：

```bash
cd client
mingw32-make
```

或者使用 qmake：

```bash
qmake
make
```

### 3. 运行

编译成功后生成 `IMX6ULLClient.exe`：

```bash
./IMX6ULLClient.exe
```

## 使用说明

### 1. 连接服务器

启动客户端后，点击"系统设置"或菜单栏"连接"，填写：
- 服务器 IP 地址
- 端口（默认 8888）
- 用户名
- 密码

点击"连接"按钮建立连接。

### 2. 文件管理

- 点击"文件管理"按钮打开文件管理器
- 支持浏览远程文件系统
- 可上传、下载、删除、重命名文件
- 支持查看文本文件内容

### 3. 系统监控

- 点击"系统监控"按钮打开监控窗口
- **系统信息标签页**: 显示 CPU、内存、磁盘使用情况
- **进程列表标签页**: 显示运行的进程信息

### 4. 日志查看

- 点击"日志查看器"按钮
- 选择要查看的日志文件（server.log、error.log、access.log）
- 支持刷新、清空、保存日志

## 协议说明

客户端使用自定义二进制协议与服务器通信：

- **帧头**: 20 字节（魔数 + 版本 + 命令 + 序列号 + 长度 + 校验和）
- **校验**: CRC32 校验和数据完整性
- **认证**: MD5 密码哈希

支持的命令：
- `CMD_LOGIN`: 登录认证
- `CMD_FILE_LIST`: 获取文件列表
- `CMD_FILE_READ`: 读取文件内容
- `CMD_FILE_DELETE`: 删除文件
- `CMD_FILE_RENAME`: 重命名文件
- `CMD_FILE_EDIT`: 编辑文件
- `CMD_SYS_INFO`: 获取系统信息
- `CMD_PROC_LIST`: 获取进程列表
- `CMD_HEARTBEAT`: 心跳保活

## 依赖库

- Qt5Widgets
- Qt5Gui
- Qt5Network
- Qt5Core
- Winsock2 (ws2_32)
- Windows Multimedia (winmm)

## 故障排除

### 编译错误

1. **找不到 Qt 头文件**
   - 检查 `Makefile` 中的 `QT_DIR` 路径是否正确
   - 确保 Qt 已正确安装

2. **链接错误**
   - 确保 MinGW 编译器版本与 Qt 版本匹配
   - 检查 Qt 库文件是否存在

### 运行时错误

1. **无法连接服务器**
   - 检查服务器 IP 和端口是否正确
   - 确保服务器程序正在运行
   - 检查防火墙设置

2. **登录失败**
   - 检查用户名和密码是否正确
   - 确保服务器配置文件中存在该用户

## 开发者信息

- Qt 版本: 6.5.3
- 编译器: MinGW

## 版本历史

- v1.0.0 - 初始版本，实现基本功能
  - 连接管理
  - 文件管理
  - 系统监控
  - 日志查看
