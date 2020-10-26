#include "sr_layer_link.h"
#include "sr_sublayer_link_lanwork.h"
#include "sr_sublayer_link_mqtt.h"
#include "sigma_log.h"
#include "interface_kv.h"
#include "interface_crypto.h"

#define PACKET_HEADER 0xaa

static uint8_t _seq = 0;

void sll_init(void)
{
    ssll_init();
    sslm_init();
}

void sll_update(void)
{
    ssll_update();
    sslm_update();
}

uint8_t sll_seq(void)
{
    return _seq++;
}

SRLinkHeader *sll_pack(uint8_t seq, const void *buffer, uint32_t size, const uint8_t *key)
{
    key = 0;//chenjing
    SRLinkHeader *packet = os_malloc(sizeof(SRLinkHeader) + (key ? (size / 16 + 1) * 16 : size));
    if (!packet)
    {
        SigmaLogError("out of memory");
        return 0;
    }
    packet->head = 0xaa;
    packet->length = network_htonl(key ? (size / 16 + 1) * 16 : size);
    packet->seq = seq;
    uint8_t i, cs = 0;
    for (i = 0; i < sizeof(SRLinkHeader) - 1; i++)
    {
        cs ^= *(((const uint8_t *)packet) + i);
    }
    packet->cs = cs;
    if (key)
        crypto_encrypt(packet + 1, buffer, size, key); 
    else
        os_memcpy(packet + 1, buffer, size);
    return packet;
}

SRLinkHeader *sll_unpack(uint8_t *buffer, uint32_t size, const uint8_t *key)
{
    key = 0;//chenjing
    SRLinkHeader *packet = (SRLinkHeader *)buffer;
    packet->length = network_ntohl(packet->length);
    if (key)
        packet->length = crypto_decrypt(packet + 1, packet + 1, packet->length, key);
    return packet;
}

uint8_t *sll_headless_pack(uint8_t seq, const void *buffer, uint32_t *size, const uint8_t *key)
{
    key = 0;//chenjing
    uint8_t *packet = os_malloc((key ? (*size / 16 + 1) * 16 : *size) + 1);
    if (!packet)
    {
        SigmaLogError("out of memory");
        return 0;
    }
    if (key)
    {
        crypto_encrypt(packet, buffer, *size, key); 
        *size = ((*size / 16) + 1) * 16 + 1;
    }
    else
    {
        os_memcpy(packet, buffer, *size);
    }
    return packet;
}

uint8_t *sll_headless_unpack(uint8_t *buffer, uint32_t *size, const uint8_t *key)
{
    key = 0;//chenjing
    uint8_t *packet = buffer;
    if (key)
        *size = crypto_decrypt(packet, packet, *size, key);
    return packet;
}

int sll_parser(const uint8_t *buffer, uint32_t size)
{
    SRLinkHeader *packet = (SRLinkHeader *)buffer;
    if (packet->head != PACKET_HEADER)
    {
        SigmaLogError("head error");
        return -1;
    }
    if (size < sizeof(SRLinkHeader))
        return 0;
    uint8_t i, cs = 0;
    for (i = 0; i < sizeof(SRLinkHeader) - 1; i++)
    {
        cs ^= *(buffer + i);
    }
    if (packet->cs != cs)
    {
        SigmaLogError("checksum error");
        return -1;
    }
    packet->cs = cs;
    if (size < sizeof(SRLinkHeader) + network_ntohl(packet->length))
        return 0;
    return sizeof(SRLinkHeader) + network_ntohl(packet->length);
}

void sll_report(uint8_t seq, const uint8_t *buffer, uint32_t size, uint32_t flags)
{
    if (flags & FLAG_LINK_SEND_LANWORK)
        ssll_report(seq, buffer, size);
    if (flags & FLAG_LINK_SEND_MQTT)
        sslm_report(seq, buffer, size, flags);
}

void sll_send(const char *id, uint8_t seq, const uint8_t *buffer, uint32_t size, uint32_t flags)
{
    if (flags & FLAG_LINK_SEND_LANWORK)
        ssll_send(id, seq, buffer, size);
    if (flags & FLAG_LINK_SEND_MQTT)
        sslm_send(id, seq, buffer, size, flags);
}

int sll_client_owner(char *owner)
{
    return kv_get("owner", owner, 32);
}

int sll_client_key(const char *id, uint8_t *key)
{
    return kv_get(id, key, 16);
}

int sll_client_add(const char *id, uint8_t *key)
{
    int size = kv_size("owner");
    if (!size)
        kv_set("owner", id, os_strlen(id));

    return sll_client_add_direct(id, key);
}

int sll_client_add_direct(const char *id, uint8_t *key)
{
    int size = kv_size("clients");
    char *ids = os_malloc(size + os_strlen(id) + 1);
    if (!ids)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    if (size)
        kv_get("clients", ids, size);
    int pos = 0;
    while (pos < size)
    {
        if (!os_strcmp(ids + pos, id))
            break;
        pos += os_strlen(ids + pos) + 1;
    }
    if (pos >= size)
    {
        os_strcpy(ids + size, id);
        size += os_strlen(id) + 1;
        kv_set("clients", ids, size);
    }
    
    kv_set(id, key, 16);
    return 0;
}

char *sll_client_list(uint32_t *size)
{
    return kv_acquire("clients", size);
}
