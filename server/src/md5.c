/*
 * MD5 Hash Implementation
 * Based on RFC 1321
 */

#include "md5.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* MD5 circular left shift */
#define ROTATE(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

/* F, G, H, I functions */
#define F(x, y, z) ((x & y) | (~x & z))
#define G(x, y, z) ((x & z) | (y & ~z))
#define H(x, y, z) (x ^ y ^ z)
#define I(x, y, z) (y ^ (x | ~z))

/* Initialize MD5 tables */
static const uint32_t S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static const uint32_t K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

/* Convert uint32 to little-endian */
#define PUT_ULONG_LE(n, b, i) \
    (b[i] = (uint8_t)((n) & 0xFF), \
     b[i+1] = (uint8_t)((n >> 8) & 0xFF), \
     b[i+2] = (uint8_t)((n >> 16) & 0xFF), \
     b[i+3] = (uint8_t)((n >> 24) & 0xFF))

static void md5_encode(const uint8_t* input, size_t len, uint8_t* output) {
    uint32_t a = 0x67452301;
    uint32_t b = 0xefcdab89;
    uint32_t c = 0x98badcfe;
    uint32_t d = 0x10325476;

    size_t offset;
    uint32_t X[16];
    uint8_t buffer[64];
    uint32_t used = 0;

    for (offset = 0; offset + 64 <= len; offset += 64) {
        /* Copy to X (little-endian) */
        for (int i = 0; i < 16; i++) {
            X[i] = ((uint32_t)input[offset + i*4]) |
                   ((uint32_t)input[offset + i*4 + 1] << 8) |
                   ((uint32_t)input[offset + i*4 + 2] << 16) |
                   ((uint32_t)input[offset + i*4 + 3] << 24);
        }

        uint32_t aa = a, bb = b, cc = c, dd = d;

        /* Round 1 */
        for (int i = 0; i < 16; i++) {
            uint32_t temp = a + F(b, c, d) + X[i] + K[i];
            a = d;
            d = c;
            c = b;
            b += ROTATE(temp, S[i]);
        }

        /* Round 2 */
        for (int i = 16; i < 32; i++) {
            uint32_t temp = a + G(b, c, d) + X[(5*i + 1) % 16] + K[i];
            a = d;
            d = c;
            c = b;
            b += ROTATE(temp, S[i]);
        }

        /* Round 3 */
        for (int i = 32; i < 48; i++) {
            uint32_t temp = a + H(b, c, d) + X[(3*i + 5) % 16] + K[i];
            a = d;
            d = c;
            c = b;
            b += ROTATE(temp, S[i]);
        }

        /* Round 4 */
        for (int i = 48; i < 64; i++) {
            uint32_t temp = a + I(b, c, d) + X[(7*i) % 16] + K[i];
            a = d;
            d = c;
            c = b;
            b += ROTATE(temp, S[i]);
        }

        a += aa; b += bb; c += cc; d += dd;
    }

    /* Padding */
    used = len % 64;
    memcpy(buffer, input + offset, used);
    buffer[used++] = 0x80;

    if (used > 56) {
        memset(buffer + used, 0, 64 - used);
        used = 64;
    }

    if (used < 56) {
        memset(buffer + used, 0, 56 - used);
        used = 56;
    }

    /* Length in bits */
    uint64_t bit_len = len * 8;
    buffer[56] = (uint8_t)(bit_len & 0xFF);
    buffer[57] = (uint8_t)((bit_len >> 8) & 0xFF);
    buffer[58] = (uint8_t)((bit_len >> 16) & 0xFF);
    buffer[59] = (uint8_t)((bit_len >> 24) & 0xFF);
    buffer[60] = (uint8_t)((bit_len >> 32) & 0xFF);
    buffer[61] = (uint8_t)((bit_len >> 40) & 0xFF);
    buffer[62] = (uint8_t)((bit_len >> 48) & 0xFF);
    buffer[63] = (uint8_t)((bit_len >> 56) & 0xFF);

    /* Process final block */
    for (int i = 0; i < 16; i++) {
        X[i] = ((uint32_t)buffer[i*4]) |
               ((uint32_t)buffer[i*4 + 1] << 8) |
               ((uint32_t)buffer[i*4 + 2] << 16) |
               ((uint32_t)buffer[i*4 + 3] << 24);
    }

    uint32_t aa = a, bb = b, cc = c, dd = d;

    for (int i = 0; i < 16; i++) {
        uint32_t temp = a + F(b, c, d) + X[i] + K[i];
        a = d;
        d = c;
        c = b;
        b += ROTATE(temp, S[i]);
    }
    for (int i = 16; i < 32; i++) {
        uint32_t temp = a + G(b, c, d) + X[(5*i + 1) % 16] + K[i];
        a = d;
        d = c;
        c = b;
        b += ROTATE(temp, S[i]);
    }
    for (int i = 32; i < 48; i++) {
        uint32_t temp = a + H(b, c, d) + X[(3*i + 5) % 16] + K[i];
        a = d;
        d = c;
        c = b;
        b += ROTATE(temp, S[i]);
    }
    for (int i = 48; i < 64; i++) {
        uint32_t temp = a + I(b, c, d) + X[(7*i) % 16] + K[i];
        a = d;
        d = c;
        c = b;
        b += ROTATE(temp, S[i]);
    }

    a += aa; b += bb; c += cc; d += dd;

    /* Output */
    PUT_ULONG_LE(a, output, 0);
    PUT_ULONG_LE(b, output, 4);
    PUT_ULONG_LE(c, output, 8);
    PUT_ULONG_LE(d, output, 12);
}

void md5_hash(const uint8_t* data, size_t len, uint8_t* hash) {
    md5_encode(data, len, hash);
}

void md5_hex_string(const uint8_t* data, size_t len, char* hex_str) {
    uint8_t hash[16];
    md5_encode(data, len, hash);

    for (int i = 0; i < 16; i++) {
        sprintf(hex_str + i * 2, "%02x", hash[i]);
    }
    hex_str[32] = '\0';
}