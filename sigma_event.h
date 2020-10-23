#ifndef __SIGMA_EVENT_H__
#define __SIGMA_EVENT_H__

#include "env.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum
{
    EVENT_TYPE_PACKET,
    EVENT_TYPE_CLIENT_CONNECTION,
    EVENT_TYPE_CLIENT_AUTH,
    EVENT_TYPE_GATEWAY_BIND,
};

typedef void (*SigmaEventHandler)(void *ctx, uint8_t event, void *msg, int size);

void *sigma_event_listen(uint8_t event, SigmaEventHandler handler, uint32_t size);
void sigma_event_dispatch(uint8_t event, void *msg, int size);

#ifdef __cplusplus
}
#endif

#endif
