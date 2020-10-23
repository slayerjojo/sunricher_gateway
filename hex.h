#ifndef __HEX_H__
#define __HEX_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

char *bin2hex(char *hex, const uint8_t *bin, uint16_t size);
uint8_t *hex2bin(uint8_t *bin, const char *hex, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif
