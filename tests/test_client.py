#!/usr/bin/env python3
"""
IMX6ULL FTP Server - Python 测试客户端
"""

import socket
import struct
import hashlib
import time

# ============================================================
# 配置
# ============================================================
SERVER_IP = "192.168.183.176"
SERVER_PORT = 8888

# 协议常量
PROTOCOL_MAGIC = 0x46545053  # 'FTPS'
PROTOCOL_VERSION = 1
MAX_PAYLOAD_SIZE = 8192

# 命令字
CMD_LOGIN = 0x0101
CMD_LOGIN_RESP = 0x0102
CMD_LOGOUT = 0x0103
CMD_FILE_LIST = 0x0201
CMD_FILE_LIST_RESP = 0x0202
CMD_FILE_READ = 0x0203
CMD_FILE_READ_RESP = 0x0204
CMD_FILE_DELETE = 0x0205
CMD_FILE_DELETE_RESP = 0x0206
CMD_FILE_RENAME = 0x0207
CMD_FILE_EDIT = 0x0209
CMD_FILE_EDIT_RESP = 0x020A
CMD_FILE_UPLOAD_START = 0x0301
CMD_FILE_UPLOAD_START_RESP = 0x0302
CMD_FILE_UPLOAD_END = 0x0309
CMD_FILE_DOWNLOAD_START = 0x030B
CMD_FILE_DOWNLOAD_END = 0x0313
CMD_SYS_INFO = 0x0401
CMD_SYS_INFO_RESP = 0x0402
CMD_PROC_LIST = 0x0403
CMD_PROC_LIST_RESP = 0x0404
CMD_HEARTBEAT = 0x0501
CMD_HEARTBEAT_RESP = 0x0502

# 错误码
ERR_SUCCESS = 0
ERR_AUTH_FAILED = 10
ERR_NOT_AUTHENTICATED = 12
ERR_FILE_NOT_FOUND = 20


# ============================================================
# CRC32 计算
# ============================================================
def crc32(data):
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc ^ 0xFFFFFFFF


# ============================================================
# Socket 封装
# ============================================================
class FTPSocket:
    def __init__(self, ip, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((ip, port))
        self.seq = 0
        print(f"[+] 连接到 {ip}:{port}")

    def close(self):
        self.sock.close()

    def _pack_frame(self, cmd, data):
        """封装帧"""
        self.seq += 1
        data_bytes = data if isinstance(data, bytes) else data.encode('utf-8')
        length = len(data_bytes)

        # 计算校验和
        checksum = crc32(data_bytes)

        # 帧头 (20字节) + 数据
        header = struct.pack('<IHHIII',
            PROTOCOL_MAGIC,   # 魔数 (4字节)
            PROTOCOL_VERSION, # 版本 (2字节)
            cmd,              # 命令 (2字节)
            self.seq,         # 序列号 (4字节)
            length,           # 数据长度 (4字节)
            checksum          # 校验和 (4字节)
        )

        return header + data_bytes

    def send_frame(self, cmd, data=b''):
        """发送帧"""
        frame = self._pack_frame(cmd, data)
        self.sock.sendall(frame)
        return self.seq

    def recv_frame(self, timeout=5):
        """接收帧"""
        self.sock.settimeout(timeout)

        # 先接收帧头 (20字节: magic(4) + version(2) + cmd(2) + seq(4) + length(4) + checksum(4))
        header = self.sock.recv(20)
        if len(header) < 20:
            return None, None, None

        magic, version, cmd, seq, length, checksum = struct.unpack('<IHHIII', header)
        if magic != PROTOCOL_MAGIC:
            print(f"[!] 无效的魔数: {hex(magic)}")
            return None, None, None

        # 接收数据 + 校验和
        total = length + 4
        data = b''
        while len(data) < length:
            chunk = self.sock.recv(length - len(data))
            if not chunk:
                break
            data += chunk

        payload = data

        return cmd, seq, payload

    def login(self, username, password):
        """登录"""
        # 计算密码MD5
        password_md5 = hashlib.md5(password.encode()).hexdigest()

        # 构建请求
        username_bytes = username.encode('utf-8').ljust(64, b'\x00')[:64]
        password_bytes = password_md5.encode('utf-8').ljust(64, b'\x00')[:64]

        data = username_bytes + password_bytes

        self.send_frame(CMD_LOGIN, data)
        cmd, seq, payload = self.recv_frame()

        if cmd == CMD_LOGIN_RESP:
            status = payload[0]
            if status == ERR_SUCCESS:
                perm = payload[1]
                message = payload[2:66].decode('utf-8', errors='ignore').rstrip('\x00')
                print(f"[+] 登录成功: {message}, 权限: {'读写' if perm == 2 else '只读'}")
                return True
            else:
                print("[!] 登录失败: " + payload[2:66].decode('utf-8', errors='ignore').rstrip('\x00'))
                return False
        return False

    def file_list(self, path="/"):
        """列出文件"""
        data = path.encode('utf-8').ljust(512, b'\x00')
        self.send_frame(CMD_FILE_LIST, data)
        cmd, seq, payload = self.recv_frame()

        if cmd == CMD_FILE_LIST_RESP:
            count = payload[1]
            print(f"[+] 文件列表 ({count} 个文件)")

            for i in range(count):
                offset = 66 + i * 272
                name = payload[offset:offset+256].decode('utf-8', errors='ignore').rstrip('\x00')
                size = struct.unpack('<I', payload[offset+256:offset+260])[0]
                is_dir = payload[offset+268]
                print(f"    {'[DIR] ' if is_dir else '[FILE]'} {name} ({size} bytes)")

    def file_read(self, path):
        """读取文件"""
        data = path.encode('utf-8').ljust(512, b'\x00')
        self.send_frame(CMD_FILE_READ, data)
        cmd, seq, payload = self.recv_frame()

        if cmd == CMD_FILE_READ_RESP:
            status = payload[0]
            if status == ERR_SUCCESS:
                size = struct.unpack('<I', payload[1:5])[0]
                content = payload[69:69+size].decode('utf-8', errors='ignore')
                print(f"[+] 文件内容 ({size} bytes):")
                print(content[:500])  # 限制显示前500字节
                return content
            else:
                print("[!] 读取失败: " + payload[69:133].decode('utf-8', errors='ignore').rstrip('\x00'))
        return None

    def file_edit(self, path, content):
        """编辑文件"""
        content_bytes = content.encode('utf-8')
        content_len = len(content_bytes)

        # 构建请求: path(512) + content_len(4) + content
        path_bytes = path.encode('utf-8').ljust(512, b'\x00')
        len_bytes = struct.pack('<I', content_len)
        data = path_bytes + len_bytes + content_bytes

        self.send_frame(CMD_FILE_EDIT, data)
        cmd, seq, payload = self.recv_frame()

        if cmd == CMD_FILE_EDIT_RESP:
            status = payload[0]
            if status == ERR_SUCCESS:
                print(f"[+] 文件已保存: {path}")
                return True
            else:
                print("[!] 保存失败: " + payload[1:65].decode('utf-8', errors='ignore').rstrip('\x00'))
        return False

    def sys_info(self):
        """获取系统信息"""
        self.send_frame(CMD_SYS_INFO)
        cmd, seq, payload = self.recv_frame()

        if cmd == CMD_SYS_INFO_RESP:
            cpu = struct.unpack('<f', payload[0:4])[0]
            mem_total = struct.unpack('<I', payload[4:8])[0]
            mem_free = struct.unpack('<I', payload[8:12])[0]
            disk_total = struct.unpack('<I', payload[16:20])[0]
            disk_free = struct.unpack('<I', payload[20:24])[0]
            uptime = struct.unpack('<I', payload[24:28])[0]

            print(f"[+] 系统信息:")
            print(f"    CPU使用率: {cpu:.1f}%")
            print(f"    内存: {mem_free}/{mem_total} KB")
            print(f"    磁盘: {disk_free}/{disk_total} KB")
            print(f"    运行时间: {uptime} 秒")

    def proc_list(self):
        """获取进程列表"""
        self.send_frame(CMD_PROC_LIST)
        cmd, seq, payload = self.recv_frame()

        if cmd == CMD_PROC_LIST_RESP:
            count = struct.unpack('<H', payload[1:3])[0]
            print(f"[+] 进程列表 ({count} 个)")

            for i in range(min(count, 10)):  # 只显示前10个
                offset = 3 + i * 172
                pid = struct.unpack('<I', payload[offset:offset+4])[0]
                name = payload[offset+4:offset+68].decode('utf-8', errors='ignore').rstrip('\x00')
                cpu = struct.unpack('<f', payload[offset+92:offset+96])[0]
                mem = struct.unpack('<f', payload[offset+96:offset+100])[0]
                state = payload[offset+100:offset+108].decode('utf-8', errors='ignore').rstrip('\x00')
                print(f"    PID:{pid:6d} {name:20s} CPU:{cpu:5.1f}% MEM:{mem:5.1f}% {state}")


# ============================================================
# 测试函数
# ============================================================
def test_basic():
    """基础测试"""
    sock = FTPSocket(SERVER_IP, SERVER_PORT)

    # 登录
    if not sock.login("admin", "admin123"):
        sock.close()
        return

    # 文件列表
    sock.file_list("/")

    # 系统信息
    sock.sys_info()

    # 进程列表
    sock.proc_list()

    sock.close()
    print("\n[+] 测试完成!")


def test_file_edit():
    """测试文件编辑"""
    sock = FTPSocket(SERVER_IP, SERVER_PORT)

    if not sock.login("admin", "admin123"):
        sock.close()
        return

    # 先读取文件
    content = sock.file_read("/test.txt")

    # 修改内容
    if content:
        new_content = content + "\n# Edited by Python client at " + time.strftime("%Y-%m-%d %H:%M:%S")
        sock.file_edit("/test.txt", new_content)

    sock.close()


if __name__ == "__main__":
    test_basic()