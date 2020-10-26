#ifndef __SR_LAYER_LINK_H__
#define __SR_LAYER_LINK_H__

#include "env.h"

#define FLAG_LINK_SEND_LANWORK (1 << 0)
#define FLAG_LINK_SEND_BROADCAST (1 << 1)
#define FLAG_LINK_SEND_MQTT (1 << 2)
#define FLAG_LINK_PACKET_EVENT (1 << 3)
#define FLAG_LINK_PACKET_DIRECTIVE (1 << 4)

typedef struct
{
    uint8_t head;
    uint32_t length;
    uint8_t seq;
    uint8_t type;
    uint8_t cs;
}__attribute__((packed)) SRLinkHeader;

void sll_init(void);
void sll_update(void);

uint8_t sll_seq(void);
SRLinkHeader *sll_pack(uint8_t seq, const void *buffer, uint32_t size, const uint8_t *key);
SRLinkHeader *sll_unpack(uint8_t *buffer, uint32_t size, const uint8_t *key);
uint8_t *sll_headless_pack(uint8_t seq, const void *buffer, uint32_t *size, const uint8_t *key);
uint8_t *sll_headless_unpack(uint8_t *buffer, uint32_t *size, const uint8_t *key);
int sll_parser(const uint8_t *buffer, uint32_t size);

void sll_report(uint8_t seq, const uint8_t *buffer, uint32_t size, uint32_t flags);
void sll_send(const char *id, uint8_t seq, const uint8_t *buffer, uint32_t size, uint32_t flags);

int sll_client_owner(char *owner);
char *sll_client_list(uint32_t *size);
int sll_client_key(const char *id, uint8_t *key);
int sll_client_add(const char *id, uint8_t *key);
int sll_client_add_direct(const char *id, uint8_t *key);

#endif
