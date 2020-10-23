#ifndef __ENV_H__
#define __ENV_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define PLATFORM_WSL

#ifdef PLATFORM_WSL
#define PLATFORM_LINUX
#endif

#ifdef PLATFORM_OPENWRT
#define PLATFORM_LINUX
#endif

#ifdef PLATFORM_ANDROID
#define PLATFORM_LINUX
#define DEBUG
#endif

#ifdef PLATFORM_LINUX

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#endif

#define STRUCT_REFERENCE(type, inst, struction) ((type *)(((char *)(inst)) - (uint64_t)(&((type *)0)->struction)))

#define UNUSED(x) (void)(x)

#define maximum(x, y) ((x) > (y) ? (x) : (y))
#define minimum(x, y) ((x) < (y) ? (x) : (y))

#define BUFFER_CAST(buffer, offset, type) *((type *)((uint8_t *)(buffer) + offset))

#ifdef __cplusplus
}
#endif

#endif
