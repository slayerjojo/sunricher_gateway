#ifndef __SR_SUBLAYER_LINK_MQTT_H__
#define __SR_SUBLAYER_LINK_MQTT_H__

#include "env.h"

void sslm_init(void);
void sslm_update(void);

void sslm_start(const char *id);

void sslm_report(uint8_t seq, const void *buffer, uint32_t size, uint32_t flags);
void sslm_send(const char *id, uint8_t seq, const void *buffer, uint32_t size, uint32_t flags);

#endif
