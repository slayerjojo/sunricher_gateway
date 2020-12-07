#include "sr_sublayer_link_lanwork.h"
#include "sr_layer_link.h"
#include "sr_opcode.h"
#include "interface_network.h"
#include "interface_os.h"
#include "sigma_log.h"
#include "sigma_event.h"
#include "cJSON.h"
#include "hex.h"

#define LANWORK_UDP_PORT_LISTEN 8887
#define LANWORK_UDP_PORT_BCAST 8888
#define LANWORK_TCP_PORT 8889

#define LANWORK_BUFFER (1024 * 4)
#define LANWORK_IDLE_INTERVAL 60000

typedef struct _link_lanwork_packet {
    struct _link_lanwork_packet *_next;

    uint8_t retry:4;
    uint8_t max:4;
    uint32_t pos;
    uint32_t size;
    uint8_t *buffer;
}LinkLanworkPacket;

typedef struct _link_session_lanwork{
    struct _link_session_lanwork *_next;

    uint8_t auth:1;
    char id[33];
    uint8_t key[32];
    int fp;
    uint32_t timer;
    LinkLanworkPacket *packets;
    uint32_t size;
    uint32_t pos;
    uint8_t *buffer;
}LinkSessionLanwork;

enum {
    STATE_CLIENT_UNAUTH,
};

static uint32_t _timer_bind = 0;
static int _listener = -1;
static LinkSessionLanwork *_sessions = 0;
static LinkSessionLanwork _bcast = {0};

static void handle_client_auth(void *ctx, uint8_t event, void *msg, int size)
{
    cJSON *packet = (cJSON *)msg;

    cJSON *header = cJSON_GetObjectItem(packet, "header");
    if (!header)
        return;

    cJSON *method = cJSON_GetObjectItem(header, "method");
    if (!method || os_strcmp(method->valuestring, "Directive"))
        return;

    cJSON *ns = cJSON_GetObjectItem(header, "namespace");
    if (!ns || os_strcmp(ns->valuestring, "Discovery"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!os_strcmp(name->valuestring, OPCODE_BIND_GATEWAY))
    {
        cJSON *payload = cJSON_GetObjectItem(packet, "payload");
        if (!payload)
        {
            SigmaLogError(0, 0, "payload not found(fp:%d).", cJSON_GetObjectItem(packet, "fp")->valueint);
            return;
        }

        cJSON *user = cJSON_GetObjectItem(payload, "userId");
        if (!user)
        {
            SigmaLogError(0, 0, "userId not found(fp:%d).", cJSON_GetObjectItem(packet, "fp")->valueint);
            return;
        }
        char owner[33] = {0};
        int retOwner = sll_client_owner(owner);

        uint8_t key[32] = {0};
        int ret = sll_client_key(user->valuestring, key);
        if (ret > 0)
        {
            ssll_auth(cJSON_GetObjectItem(packet, "fp")->valueint, user->valuestring, key);
        }
        else if (_timer_bind && os_ticks_from(_timer_bind) < os_ticks_ms(30000))
        {
            cJSON *userKey = cJSON_GetObjectItem(payload, "userKey");
            if (userKey)
            {
                hex2bin(key, userKey->valuestring, 32);
                sll_client_add(user->valuestring, key);
                ssll_auth(cJSON_GetObjectItem(packet, "fp")->valueint, user->valuestring, key);
                ret = 16;

                sigma_event_dispatch(EVENT_TYPE_GATEWAY_BOUNDED, user->valuestring, os_strlen(user->valuestring));
            }
        }

        uint8_t seq = sll_seq();
        cJSON *resp = cJSON_CreateObject();
        header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Discovery"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_BIND_GATEWAY_RESP));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(resp, "header", header);
        payload = cJSON_CreateObject();
        cJSON_AddItemToObject(payload, "bindResult", cJSON_CreateString(ret < 0 ? "UNBOUND" : "OK"));
        cJSON_AddItemToObject(payload, "isOwner", cJSON_CreateBool(!os_strcmp(owner, user->valuestring)));
        cJSON_AddItemToObject(payload, "userId", cJSON_CreateString(user->valuestring));
        cJSON_AddItemToObject(resp, "payload", payload);
        char *rsp = cJSON_PrintUnformatted(resp);
        if (ret < 0)
        {
            if (cJSON_GetObjectItem(packet, "fp"))
                ssll_raw(cJSON_GetObjectItem(packet, "fp")->valueint, key, seq, rsp, os_strlen(rsp));
            else
                SigmaLogError(0, 0, "fp is null.%s", rsp);
        }
        else
        {
            ssll_send(user->valuestring, seq, rsp, os_strlen(rsp));
        }
        os_free(rsp);
        cJSON_Delete(resp);
    }
    else if (!os_strcmp(name->valuestring, OPCODE_BOUND_USERS_QUERY))
    {
        cJSON *user = cJSON_GetObjectItem(packet, "user");
        if (!user)
        {
            SigmaLogError(0, 0, "userId not found(fp:%d).", cJSON_GetObjectItem(packet, "fp")->valueint);
            return;
        }
        char owner[33] = {0};
        int retOwner = sll_client_owner(owner);

        uint8_t key[32] = {0};
        int ret = sll_client_key(user->valuestring, key);
        if (ret < 0)
        {
            SigmaLogError(0, 0, "user not bind(user:%d).", user->valuestring);
            return;
        }
        
        uint8_t seq = sll_seq();
        cJSON *resp = cJSON_CreateObject();
        header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Discovery"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_BOUND_USERS_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(resp, "header", header);
        cJSON *users = cJSON_CreateArray();

        uint32_t size = 0;
        char *client = sll_client_list(&size);
        uint32_t pos = 0;
        while (pos < size)
        {
            cJSON *c = cJSON_CreateObject();
            cJSON_AddItemToObject(c, "isOwner", cJSON_CreateBool(!os_strcmp(client + pos, owner)));
            cJSON_AddItemToObject(c, "userId", cJSON_CreateString(client + pos));
            cJSON_AddItemToArray(users, c);
            pos += os_strlen(client + pos) + 1;
        }

        cJSON_AddItemToObject(resp, "users", users);
        char *rsp = cJSON_PrintUnformatted(resp);
        ssll_send(user->valuestring, seq, rsp, os_strlen(rsp));
        os_free(rsp);
        cJSON_Delete(resp);
    }
}

static void handle_gateway_bind(void *ctx, uint8_t event, void *msg, int size)
{
    ssll_bind();
}

void ssll_init(void)
{
    _bcast.fp = -1;

    sigma_event_listen(EVENT_TYPE_PACKET, handle_client_auth, 0);
    sigma_event_listen(EVENT_TYPE_GATEWAY_BIND, handle_gateway_bind, 0);
}

void ssll_update(void)
{
    int ret = 0;
    uint8_t ip[4] = {192, 168, 123, 255};
    uint16_t port;

    if (_bcast.fp < 0)
        _bcast.fp = network_udp_create(LANWORK_UDP_PORT_LISTEN);
    if (_bcast.fp >= 0)
    {
        LinkLanworkPacket *packet = _bcast.packets, *min = 0, *prev = 0, *prev_min = 0;
        while (packet)
        {
            if (!min)
            {
                prev_min = prev;
                min = packet;
            }
            if (min->retry > packet->retry)
            {
                prev_min = prev;
                min = packet;
            }
            prev = packet;
            packet = packet->_next;
        }
        if (min)
        {
            if (!_bcast.timer || os_ticks_from(_bcast.timer) > os_ticks_ms(50 * (min->retry + 1)))
            {
                ret = network_udp_send(_bcast.fp, min->buffer + min->pos, min->size - min->pos, ip, LANWORK_UDP_PORT_BCAST);
                if (ret < 0)
                {
                    network_udp_close(_bcast.fp);
                    _bcast.fp = -1;
                }
                else if (ret > 0)
                {
                    _bcast.timer = os_ticks();

                    min->retry++;
                    if (min->retry >= min->max)
                    {
                        if (prev_min)
                            prev_min = min->_next;
                        else
                            _bcast.packets = min->_next;
                        os_free(min->buffer);
                        os_free(min);
                    }
                }
            }
        }
    }
    if (_bcast.fp >= 0)
    {
        uint8_t *buffer = os_malloc(LANWORK_BUFFER);
        if (!buffer)
            SigmaLogError(0, 0, "out of memory");
        if (buffer)
        {
            ret = network_udp_recv(_bcast.fp, buffer, LANWORK_BUFFER, ip, &port);
            if (ret < 0)
            {
                network_udp_close(_bcast.fp);
                _bcast.fp = -1;
            }
            else if (ret > 0)
            {
                cJSON *msg = 0;
                do {
                    int size = sll_parser(buffer, ret);
                    if (size <= 0)
                    {
                        SigmaLogError(buffer, ret, "packet will be discarded because it is incomplete or wrong format.raw:");
                        break;
                    }
                    SRLinkHeader *packet = sll_unpack(buffer, ret, _bcast.key);
                    buffer[sizeof(SRLinkHeader) + packet->length] = 0;

                    msg = cJSON_Parse((const char *)(packet + 1));
                    if (!msg)
                    {
                        SigmaLogError(0, 0, "json parse error.raw:%s", (const char *)(packet + 1));
                        break;
                    }
                    cJSON *header = cJSON_GetObjectItem(msg, "header");
                    if (!header)
                    {
                        SigmaLogError(0, 0, "packet header error.raw:%s", (const char *)(packet + 1));
                        break;
                    }
                    cJSON *idx = cJSON_GetObjectItem(header, "messageIndex");
                    if (!idx || (((unsigned int)idx->valueint) & 0xff) != packet->seq)
                    {
                        SigmaLogError(buffer, ret, "packet messageIndex not match.raw:");
                        break;
                    }
                    cJSON *name = cJSON_GetObjectItem(header, "name");
                    if (!name)
                    {
                        SigmaLogError(0, 0, "packet name error.raw:%s", (const char *)(packet + 1));
                        break;
                    }
                    if (os_strcmp(name->valuestring, OPCODE_DISCOVER_GATEWAY))
                    {
                        SigmaLogError(0, 0, "packet name is not allow.raw:%s", (const char *)(packet + 1));
                        break;
                    }
                    char ips[12 + 4] = {0};
                    sprintf(ips, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
                    cJSON_AddItemToObject(msg, "ip", cJSON_CreateString(ips));
                    cJSON_AddItemToObject(msg, "port", cJSON_CreateNumber(port));

                    SigmaLogAction(0, 0, "bcast recv:%s", (const char *)(packet + 1));

                    sigma_event_dispatch(EVENT_TYPE_PACKET, msg, 0);
                } while (0);
                if (msg)
                    cJSON_Delete(msg);
            }
            os_free(buffer);
        }
    }
    if (_bcast.fp < 0)
    {
        if (_bcast.buffer)
        {
            os_free(_bcast.buffer);
            _bcast.buffer = 0;
        }
        while (_bcast.packets)
        {
            LinkLanworkPacket *packet = _bcast.packets;
            _bcast.packets = _bcast.packets->_next;

            os_free(packet->buffer);
            os_free(packet);
        }
    }
    if (_listener < 0)
        _listener = network_tcp_server(LANWORK_TCP_PORT);
    if (_listener >= 0)
    {
        int fp = network_tcp_accept(_listener, ip, &port);
        if (fp < -1)
        {
            network_tcp_close(_listener);
            _listener = -1;
        }
        else if (fp >= 0)
        {
            LinkSessionLanwork *session = os_malloc(sizeof(LinkSessionLanwork));
            if (!session)
                SigmaLogError(0, 0, "out of memory");
            if (session)
            {
                os_memset(session, 0, sizeof(LinkSessionLanwork));

                session->fp = fp;
                session->timer = os_ticks();
                session->auth = 0;

                session->_next = _sessions;
                _sessions = session;
            }
        }
    }
    LinkSessionLanwork *session = _sessions, *prev = 0;
    while (session)
    {
        if (os_ticks_from(session->timer) > os_ticks_ms(LANWORK_IDLE_INTERVAL))
        {
            SigmaLogError(session->id, 16, "fp:%d heartbeat stoped, id:");
            break;
        }
        if (session->packets)
        {
            LinkLanworkPacket *min = session->packets;
            ret = network_tcp_send(session->fp, min->buffer + min->pos, min->size - min->pos);
            if (ret < 0)
            {
                SigmaLogError(session->id, 16, "fp:%d send failed.", session->fp);
                break;
            }
            else if (ret > 0)
            {
                min->pos += ret;
                if (min->pos >= min->size)
                {
                    session->packets = min->_next;
                    os_free(min->buffer);
                    os_free(min);
                }
            }
        }
        if (!session->buffer)
        {
            session->buffer = os_malloc(sizeof(SRLinkHeader));
            if (!session->buffer)
            {
                SigmaLogError(0, 0, "out of memory");
                break;
            }
            session->pos = 0;
            session->size = sizeof(SRLinkHeader);
        }
        if (session->buffer)
        {
            int ret = network_tcp_recv(session->fp, session->buffer + session->pos, session->size - session->pos);
            if (ret < 0)
            {
                SigmaLogError(session->id, 16, "fp:%d recv failed. session:", session->fp);
                break;
            }
            else if (ret > 0)
            {
                session->pos += ret;
                if (session->pos >= session->size)
                {
                    ret = sll_parser(session->buffer, session->size);
                    if (ret < 0)
                    {
                        SigmaLogError(session->buffer, session->size, "packet will be discarded because format is wrong.raw:");
                        break;
                    }
                    else if (!ret)
                    {
                        if (session->size != sizeof(SRLinkHeader) + network_ntohl(((SRLinkHeader *)(session->buffer))->length))
                        {
                            session->size = sizeof(SRLinkHeader) + network_ntohl(((SRLinkHeader *)(session->buffer))->length);

                            uint8_t *buffer = os_malloc(session->size + 1);
                            if (!buffer)
                            {
                                SigmaLogError(0, 0, "out of memory");
                                break;
                            }
                            os_memcpy(buffer, session->buffer, sizeof(SRLinkHeader));
                            os_free(session->buffer);
                            session->buffer = buffer;
                        }
                    }
                    else
                    {
                        cJSON *msg = 0;
                        do {
                            SRLinkHeader *packet = sll_unpack(session->buffer, session->size, session->key);
                            session->buffer[sizeof(SRLinkHeader) + packet->length] = 0;

                            msg = cJSON_Parse((const char *)(packet + 1));
                            if (!msg)
                            {
                                SigmaLogError(0, 0, "json parse error.raw:%s", (const char *)(packet + 1));
                                break;
                            }
                            cJSON *header = cJSON_GetObjectItem(msg, "header");
                            if (!header)
                            {
                                SigmaLogError(0, 0, "packet header error.raw:%s", (const char *)(packet + 1));
                                break;
                            }
                            cJSON *idx = cJSON_GetObjectItem(header, "messageIndex");
                            if (!idx || (((unsigned int)idx->valueint) & 0xff) != packet->seq)
                            {
                                SigmaLogError(session->buffer, session->size, "packet messageIndex not match.raw:");
                                break;
                            }
                            if (!session->auth)
                            {
                                cJSON *name = cJSON_GetObjectItem(header, "name");
                                if (!name)
                                {
                                    SigmaLogError(0, 0, "packet name error.raw:%s", (const char *)(packet + 1));
                                    break;
                                }
                                if (os_strcmp(name->valuestring, OPCODE_BIND_GATEWAY))
                                {
                                    SigmaLogError(0, 0, "packet name is not allow.raw:%s", (const char *)(packet + 1));
                                    break;
                                }
                                cJSON_AddItemToObject(msg, "fp", cJSON_CreateNumber(session->fp));
                            }
                            else
                            {
                                cJSON_AddItemToObject(msg, "user", cJSON_CreateString(session->id));
                                cJSON_AddItemToObject(msg, "fp", cJSON_CreateNumber(session->fp));
                                session->timer = os_ticks();
                            }

                            SigmaLogAction(0, 0, "session %s:%d recv:%s", session->id, session->fp, (const char *)(packet + 1));

                            sigma_event_dispatch(EVENT_TYPE_PACKET, msg, 0);
                        } while (0);
                        if (msg)
                            cJSON_Delete(msg);
                        os_free(session->buffer);
                        session->buffer = 0;
                    }
                }
            }
        }
        prev = session;
        session = session->_next;
    }
    if (session)
    {
        if (session->fp >= 0)
            network_tcp_close(session->fp);
        if (session->buffer)
        {
            os_free(session->buffer);
            session->buffer = 0;
        }
        while (session->packets)
        {
            LinkLanworkPacket *packet = session->packets;
            session->packets = session->packets->_next;

            os_free(packet->buffer);
            os_free(packet);
        }
        if (prev)
            prev->_next = session->_next;
        else
            _sessions = session->_next;
        os_free(session);
    }
}

void ssll_bind(void)
{
    _timer_bind = os_ticks();

    SigmaLogAction(0, 0, "timer for binding gateway is started");
}

void ssll_auth(int fp, const char *id, uint8_t *key)
{
    LinkSessionLanwork *session = _sessions;
    while (session)
    {
        if (session->fp == fp)
            break;
        session = session->_next;
    }
    if (!session)
    {
        SigmaLogError(0, 0, "session not found.(fp:%d)", fp);
        return;
    }
    os_strcpy(session->id, id);
    os_memcpy(session->key, key, 16);
    session->auth = 1;

    SigmaLogAction(0, 0, "session is authed(id:%s)", session->id);

    sigma_event_dispatch(EVENT_TYPE_CLIENT_AUTH, (char *)id, os_strlen(id));
}

void ssll_bcast(uint8_t seq, const void *buffer, uint32_t size)
{
    LinkSessionLanwork *session = &_bcast;

    SRLinkHeader *header = sll_pack(seq, buffer, size, session->key);
    if (header)
    {
        LinkLanworkPacket *packet = os_malloc(sizeof(LinkLanworkPacket));
        if (!packet)
            SigmaLogError(0, 0, "out of memory");
        if (packet)
        {
            os_memset(packet, 0, sizeof(LinkLanworkPacket));

            packet->pos = 0;
            packet->size = sizeof(SRLinkHeader) + network_ntohl(header->length);
            packet->buffer = (uint8_t *)header;
            packet->_next = 0;

            LinkLanworkPacket *last = session->packets;
            while (last && last->_next)
                last = last->_next;
            if (last)
                last->_next = packet;
            else
                session->packets = packet;
        }
    }
}

void ssll_report(uint8_t seq, const void *buffer, uint32_t size)
{
    LinkSessionLanwork *session = _sessions;
    while (session)
    {
        if (session->auth)
        {
            SRLinkHeader *header = sll_pack(seq, buffer, size, session->key);
            if (header)
            {
                LinkLanworkPacket *packet = os_malloc(sizeof(LinkLanworkPacket));
                if (!packet)
                    SigmaLogError(0, 0, "out of memory");
                if (packet)
                {
                    os_memset(packet, 0, sizeof(LinkLanworkPacket));

                    packet->pos = 0;
                    packet->size = sizeof(SRLinkHeader) + network_ntohl(header->length);
                    packet->buffer = (uint8_t *)header;
                    packet->_next = 0;

                    LinkLanworkPacket *last = session->packets;
                    while (last && last->_next)
                        last = last->_next;
                    if (last)
                        last->_next = packet;
                    else
                        session->packets = packet;
                }
            }
        }
        session = session->_next;
    }
}

void ssll_send(const char *id, uint8_t seq, const void *buffer, uint32_t size)
{
    uint8_t key[32] = {0};
    if (sll_client_key(id, key) < 0)
    {
        SigmaLogError(0, 0, "client %s not found", id);
        return;
    }

    LinkSessionLanwork *session = _sessions;
    while (session)
    {
        if (!os_strcmp(session->id, id))
        {
            SRLinkHeader *header = sll_pack(seq, buffer, size, key);
            if (header)
            {
                LinkLanworkPacket *packet = os_malloc(sizeof(LinkLanworkPacket));
                if (!packet)
                    SigmaLogError(0, 0, "out of memory");
                if (packet)
                {
                    os_memset(packet, 0, sizeof(LinkLanworkPacket));

                    packet->pos = 0;
                    packet->size = sizeof(SRLinkHeader) + network_ntohl(header->length);
                    packet->buffer = (uint8_t *)header;
                    packet->_next = 0;

                    LinkLanworkPacket *last = session->packets;
                    while (last && last->_next)
                        last = last->_next;
                    if (last)
                        last->_next = packet;
                    else
                        session->packets = packet;
                }
            }
            break;
        }
        session = session->_next;
    }
}

void ssll_raw(int fp, uint8_t *key, uint8_t seq, const void *buffer, uint32_t size)
{
    LinkSessionLanwork *session = _sessions;
    while (session)
    {
        if (session->fp == fp)
        {
            SRLinkHeader *header = sll_pack(seq, buffer, size, key);
            if (header)
            {
                LinkLanworkPacket *packet = os_malloc(sizeof(LinkLanworkPacket));
                if (!packet)
                    SigmaLogError(0, 0, "out of memory");
                if (packet)
                {
                    os_memset(packet, 0, sizeof(LinkLanworkPacket));

                    packet->pos = 0;
                    packet->size = sizeof(SRLinkHeader) + network_ntohl(header->length);
                    packet->buffer = (uint8_t *)header;
                    packet->_next = 0;

                    LinkLanworkPacket *last = session->packets;
                    while (last && last->_next)
                        last = last->_next;
                    if (last)
                        last->_next = packet;
                    else
                        session->packets = packet;
                }
            }
            break;
        }
        session = session->_next;
    }
}
