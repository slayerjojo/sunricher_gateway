#ifndef __SIGMA_CONSOLE_H__
#define __SIGMA_CONSOLE_H__

#include "env.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*ConsoleHandler)(const char *command, const char *parameters[], int count);

void sigma_console_init(uint16_t port, ConsoleHandler handler);
void sigma_console_update(void);

int sigma_console_fp(void);

void sigma_console_write(const char *fmt, ...);
void sigma_console_close(void);

#ifdef __cplusplus
}
#endif

#endif
