/*
 * MD5 Hash Implementation
 * Based on RFC 1321
 */

#ifndef MD5_H
#define MD5_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 计算MD5哈希值
 * @param data 输入数据
 * @param len 数据长度
 * @param hash 输出的16字节哈希值
 */
void md5_hash(const uint8_t* data, size_t len, uint8_t* hash);

/**
 * 计算MD5并转换为十六进制字符串
 * @param data 输入数据
 * @param len 数据长度
 * @param hex_str 输出的33字节字符串（32字符+null）
 */
void md5_hex_string(const uint8_t* data, size_t len, char* hex_str);

#ifdef __cplusplus
}
#endif

#endif /* MD5_H */