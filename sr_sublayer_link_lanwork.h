#ifndef __SR_SUBLAYER_LINK_LANWORK_H__
#define __SR_SUBLAYER_LINK_LANWORK_H__

#include "env.h"

typedef struct
{
    uint8_t *ip;
    uint16_t port;
    int fp;
}EventClientConnection;

void ssll_init(void);
void ssll_update(void);

void ssll_bcast_addr(uint8_t *ip);

void ssll_bind(void);

void ssll_auth(int fp, const char *id, uint8_t *key);

void ssll_bcast(uint8_t seq, const void *buffer, uint32_t size);
void ssll_report(uint8_t seq, const void *buffer, uint32_t size);
void ssll_send(const char *id, uint8_t seq, const void *buffer, uint32_t size);
void ssll_raw(int fp, uint8_t *key, uint8_t seq, const void *buffer, uint32_t size);

#endif
