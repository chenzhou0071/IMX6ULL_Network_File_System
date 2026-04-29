#!/usr/bin/env python3
"""
IMX6ULL FTP Server - Python 测试客户端
完整功能测试
"""

import socket
import struct
import hashlib
import time
import os

# ============================================================
# 配置
# ============================================================
SERVER_IP = "192.168.183.176"
SERVER_PORT = 8888

# 协议常量
PROTOCOL_MAGIC = 0x46545053
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
CMD_FILE_RENAME_RESP = 0x0208
CMD_FILE_EDIT = 0x0209
CMD_FILE_EDIT_RESP = 0x020A
CMD_FILE_UPLOAD_START = 0x0301
CMD_FILE_UPLOAD_START_RESP = 0x0302
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
        checksum = crc32(data_bytes)

        # 帧头 (20字节) + 数据
        header = struct.pack('<IHHIII',
            PROTOCOL_MAGIC,
            PROTOCOL_VERSION,
            cmd,
            self.seq,
            length,
            checksum
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

        # 接收帧头 (20字节)
        header = self.sock.recv(20)
        if len(header) < 20:
            return None, None, None

        magic, version, cmd, seq, length, checksum = struct.unpack('<IHHIII', header)
        if magic != PROTOCOL_MAGIC:
            print(f"[!] 无效的魔数: {hex(magic)}")
            return None, None, None

        # 接收数据
        data = b''
        while len(data) < length:
            chunk = self.sock.recv(length - len(data))
            if not chunk:
                break
            data += chunk

        return cmd, seq, data

    def login(self, username, password):
        """登录"""
        password_md5 = hashlib.md5(password.encode()).hexdigest()
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
            print(f"[+] 文件列表 ({count} 个)")

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
                print(content[:500])
                return content
            else:
                print("[!] 读取失败: " + payload[69:133].decode('utf-8', errors='ignore').rstrip('\x00'))
        return None

    def file_edit(self, path, content):
        """编辑文件"""
        content_bytes = content.encode('utf-8')
        content_len = len(content_bytes)

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

    def file_delete(self, path):
        """删除文件"""
        data = path.encode('utf-8').ljust(512, b'\x00')
        self.send_frame(CMD_FILE_DELETE, data)
        cmd, seq, payload = self.recv_frame()

        if cmd == CMD_FILE_DELETE_RESP:
            status = payload[0]
            if status == ERR_SUCCESS:
                print(f"[+] 文件已删除: {path}")
                return True
            else:
                print("[!] 删除失败: " + payload[1:65].decode('utf-8', errors='ignore').rstrip('\x00'))
                return False
        return False

    def file_rename(self, old_path, new_path):
        """重命名文件"""
        # 格式: old_path\0new_path (用\0分隔)
        data = old_path.encode('utf-8') + b'\x00' + new_path.encode('utf-8')
        self.send_frame(CMD_FILE_RENAME, data)
        cmd, seq, payload = self.recv_frame()

        if cmd == CMD_FILE_RENAME_RESP:
            status = payload[0]
            if status == ERR_SUCCESS:
                print(f"[+] 文件已重命名: {old_path} -> {new_path}")
                return True
            else:
                print("[!] 重命名失败: " + payload[1:65].decode('utf-8', errors='ignore').rstrip('\x00'))
                return False
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

            # process_info_t 结构体布局 (packed):
            # 0-3:   pid (4 bytes)
            # 4-67:  name (64 bytes)
            # 68-99: user (32 bytes)
            # 100-103: cpu_usage (4 bytes)
            # 104-107: mem_usage (4 bytes)
            # 108:    state (1 byte)
            # 109-116: state_name (8 bytes)
            # 117-120: mem_size (4 bytes)
            # 总大小: 121 bytes

            for i in range(min(count, 10)):
                offset = 3 + i * 121
                if offset + 121 > len(payload):
                    break
                pid = struct.unpack('<I', payload[offset+0:offset+4])[0]
                name = payload[offset+4:offset+68].decode('utf-8', errors='ignore').rstrip('\x00')
                user = payload[offset+68:offset+100].decode('utf-8', errors='ignore').rstrip('\x00')
                cpu = struct.unpack('<f', payload[offset+100:offset+104])[0]
                mem = struct.unpack('<f', payload[offset+104:offset+108])[0]
                state = payload[offset+108:offset+109].decode('utf-8', errors='ignore')
                state_name = payload[offset+109:offset+117].decode('utf-8', errors='ignore').rstrip('\x00')
                print(f"    PID:{pid:6d} {name:20s} CPU:{cpu:5.1f}% MEM:{mem:5.1f}% {state_name}")


# ============================================================
# 完整测试流程
# ============================================================
def test_full():
    """完整测试流程"""
    import os

    print("=" * 50)
    print("IMX6ULL FTP Server - 完整功能测试")
    print("=" * 50)

    sock = FTPSocket(SERVER_IP, SERVER_PORT)

    # 1. 登录
    print("\n[步骤1] 登录...")
    if not sock.login("admin", "admin123"):
        sock.close()
        return
    print("[+] 登录成功")

    # 2. 文件列表
    print("\n[步骤2] 获取根目录文件列表...")
    sock.file_list("/")

    # 3. 系统信息
    print("\n[步骤3] 获取系统信息...")
    sock.sys_info()

    # 4. 进程列表
    print("\n[步骤4] 获取进程列表...")
    sock.proc_list()

    # 5. 上传文件 - 创建本地测试文件
    print("\n[步骤5] 上传文件...")
    local_file = "test_upload.txt"
    test_content = f"Test file content\nCreated at {time.strftime('%Y-%m-%d %H:%M:%S')}\nThis is a test file for upload.\n"
    with open(local_file, 'w', encoding='utf-8') as f:
        f.write(test_content)
    print(f"[+] 创建本地测试文件: {local_file}")

    remote_file = "/uploads/test_upload.txt"
    print(f"[+] 上传文件到: {remote_file}")

    # 使用file_edit写入到uploads目录
    if sock.file_edit(remote_file, test_content):
        print("[+] 文件上传成功")

        # 6. 进入上传文件夹
        print("\n[步骤6] 进入上传文件夹...")
        sock.file_list("/uploads")

        # 7. 重命名文件
        print("\n[步骤7] 重命名文件...")
        new_name = "/uploads/test_upload_renamed.txt"
        if sock.file_rename(remote_file, new_name):
            # 8. 编辑文件
            print("\n[步骤8] 编辑文件...")
            edited_content = test_content + f"\n# Edited at {time.strftime('%Y-%m-%d %H:%M:%S')}\n"
            if sock.file_edit(new_name, edited_content):
                print("[+] 文件编辑成功")

                # 9. 下载文件（读取文件内容）
                print("\n[步骤9] 下载并验证文件...")
                content = sock.file_read(new_name)
                if content:
                    print(f"[+] 文件下载成功，内容长度: {len(content)} bytes")

                    # 10. 删除文件
                    print("\n[步骤10] 删除文件...")
                    if sock.file_delete(new_name):
                        print("[+] 文件删除成功")
                    else:
                        print("[!] 文件删除失败")
                else:
                    print("[!] 文件下载失败")
            else:
                print("[!] 文件编辑失败")
        else:
            print("[!] 文件重命名失败")
    else:
        print("[!] 文件上传失败")

    # 清理本地文件
    print("\n[清理] 删除本地测试文件...")
    if os.path.exists(local_file):
        os.remove(local_file)
        print(f"[+] 已删除: {local_file}")

    sock.close()

    print("\n" + "=" * 50)
    print("[+] 完整测试流程结束!")
    print("=" * 50)


if __name__ == "__main__":
    test_full()
