#!/usr/bin/env python3
"""
IMX6ULL FTP Server - 简化测试客户端
"""

import socket
import struct
import hashlib

SERVER_IP = "xxx.xxx.xxx.xxx"
SERVER_PORT = 8888

PROTOCOL_MAGIC = 0x46545053

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

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((SERVER_IP, SERVER_PORT))
    print("[+] 已连接到服务器")

    # 构建登录请求
    username = "admin"
    password = "admin123"
    # 发送MD5加密的密码
    import hashlib
    password_md5 = hashlib.md5(password.encode()).hexdigest()

    print(f"[-] 用户名: {username}")
    print(f"[-] 密码(明文): {password}")
    print(f"[-] 密码(MD5): {password_md5}")

    username_bytes = username.encode('utf-8').ljust(64, b'\x00')[:64]
    password_bytes = password_md5.encode('utf-8').ljust(64, b'\x00')[:64]
    data = username_bytes + password_bytes

    print(f"[-] 数据长度: {len(data)}")

    # 构建帧头 (20字节) - 包含checksum
    seq = 1
    checksum = crc32(data)
    header = struct.pack('<IHHIII',
        PROTOCOL_MAGIC,  # 魔数 4字节
        1,               # 版本 2字节
        0x0101,          # CMD_LOGIN 2字节
        seq,             # 序列号 4字节
        len(data),       # 数据长度 4字节
        checksum         # 校验和 4字节
    )

    frame = header + data  # 不需要再加checksum了

    print(f"[-] 发送帧，大小: {len(frame)}")
    print(f"[-] 帧头hex: {header.hex()}")
    print(f"[-] 校验和hex: {struct.pack('<I', checksum).hex()}")
    print(f"[-] 预期魔数: 53505446")

    sock.sendall(frame)

    # 接收响应
    print("[-] 等待响应...")
    response = sock.recv(1024)
    print(f"[+] 收到响应，大小: {len(response)}")
    print(f"[+] 响应hex: {response.hex()}")

    sock.close()

if __name__ == "__main__":
    main()