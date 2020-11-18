#ifndef __SIGMA_LOG_H__
#define __SIGMA_LOG_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "env.h"

enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_ACTION,
    LOG_LEVEL_DEBUG
};

#ifdef  _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#ifdef PLATFORM_ANDROID
#define __PRETTY_FUNCTION__ __FUNCTION__
#define DEBUG
#endif

#define SigmaLogError(buffer, size, ...) sigma_log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, LOG_LEVEL_ERROR, buffer, size, __VA_ARGS__)
#define SigmaLogAction(buffer, size, ...) sigma_log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, LOG_LEVEL_ACTION, buffer, size, __VA_ARGS__)

#define SigmaLogHalt(buffer, size, ...) do {                                                                \
    sigma_log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, LOG_LEVEL_ERROR, buffer, size, __VA_ARGS__); \
    exit(EXIT_FAILURE);                                                                                     \
} while(0);

#ifdef DEBUG
#define SigmaLogDebug(buffer, size, ...) sigma_log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, LOG_LEVEL_DEBUG, buffer, size, __VA_ARGS__)
#else
#define SigmaLogDebug(...)
#endif

void sigma_log_println(const char *file, uint32_t line, const char *func, unsigned char level, const void *buffer, size_t size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
