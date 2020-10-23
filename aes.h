#ifndef __AES_H__
#define __AES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int AESEncrypt(const uint8_t *in, uint8_t *out, int len, const uint8_t *key, int keylen);
int AESDecrypt(const uint8_t *in, uint8_t *out, int len, const uint8_t *key, int keylen);

long AES128CBCEncrypt(const uint8_t *in, uint8_t *out, long len, const uint8_t*key, const uint8_t *iv);
long AES128CBCDecrypt(const uint8_t *in, uint8_t *out, long len, const uint8_t *key, const uint8_t *iv);

#ifdef __cplusplus
}
#endif
#endif // AES_H
