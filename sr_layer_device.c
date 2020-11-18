#include "sr_layer_device.h"
#include "sr_sublayer_device_gateway.h"
#include "sr_layer_link.h"
#include "sr_sublayer_link_lanwork.h"
#include "sr_opcode.h"
#include "sigma_event.h"
#include "sigma_log.h"
#include "interface_os.h"
#include "interface_kv.h"
#include "hex.h"

static void handle_device_discover(void *ctx, uint8_t event, void *msg, int size)
{
    cJSON *packet = (cJSON *)msg;

    cJSON *header = cJSON_GetObjectItem(packet, "header");
    if (!header)
    {
        SigmaLogError(0, 0, "header not found.");
        return;
    }

    cJSON *method = cJSON_GetObjectItem(header, "method");
    if (!method || os_strcmp(method->valuestring, "Directive"))
        return;

    cJSON *ns = cJSON_GetObjectItem(header, "namespace");
    if (!ns || os_strcmp(ns->valuestring, "Discovery"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!name)
    {
        SigmaLogError(0, 0, "name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_DEVICE_ADD_OR_UPDATE))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found");
            return;
        }
        
        cJSON *epid = cJSON_GetObjectItem(ep, "endpointId");
        if (!epid)
        {
            SigmaLogError(0, 0, "endpointId not found");
            return;
        }

        cJSON *device = sld_load(epid->valuestring);
        if (!device)
        {
            SigmaLogError(0, 0, "device %s not found", epid->valuestring);
            return;
        }

        cJSON *item = ep->child;
        while (item)
        {
            if (os_strcmp(item->string, "endpointId"))
            {
                cJSON_DeleteItemFromObject(device, item->string);
                cJSON_AddItemToObject(device, item->string, cJSON_Duplicate(item, 1));
            }
            item = item->next;
        }
        sld_save(epid->valuestring, device);

        sld_profile_report(epid->valuestring, 0);
        cJSON_Delete(device);
    }
    else if (!os_strcmp(name->valuestring, OPCODE_DEVICE_DELETE))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found");
            return;
        }
        
        cJSON *epid = cJSON_GetObjectItem(ep, "endpointId");
        if (!epid)
        {
            SigmaLogError(0, 0, "endpointId not found");
            return;
        }

        sld_delete(epid->valuestring);
    }
}

static void handle_device_basic(void *ctx, uint8_t event, void *msg, int size)
{
    cJSON *packet = (cJSON *)msg;

    cJSON *header = cJSON_GetObjectItem(packet, "header");
    if (!header)
    {
        SigmaLogError(0, 0, "header not found.");
        return;
    }

    cJSON *method = cJSON_GetObjectItem(header, "method");
    if (!method || os_strcmp(method->valuestring, "Directive"))
        return;

    cJSON *ns = cJSON_GetObjectItem(header, "namespace");
    if (!ns || os_strcmp(ns->valuestring, "Basic"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!name)
    {
        SigmaLogError(0, 0, "name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_DEVICE_STATE_REPORT))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found");
            return;
        }
        
        cJSON *epid = cJSON_GetObjectItem(ep, "endpointId");
        if (!epid)
        {
            SigmaLogError(0, 0, "endpointId not found");
            return;
        }

        sld_property_report(epid->valuestring, OPCODE_DEVICE_STATE_REPORT);
    }
}

static void handle_client_auth(void *ctx, uint8_t event, void *msg, int size)
{
    char *id = (char *)msg;

    sld_profile_report(0, id);
}

void sld_init(void)
{
    ssdg_init();

    sigma_event_listen(EVENT_TYPE_CLIENT_AUTH, handle_client_auth, 0);
    sigma_event_listen(EVENT_TYPE_PACKET, handle_device_discover, 0);
    sigma_event_listen(EVENT_TYPE_PACKET, handle_device_basic, 0);
}

void sld_update(void)
{
    ssdg_update();
}

const char *sld_id_create(char *id)
{
    uint8_t idb[16] = {0};
    int i = 0;
    for (i = 0; i < 16; i++)
        idb[i] = os_rand();
    bin2hex(id, idb, 16);
    return id;
}

cJSON *sld_load(const char *device)
{
    cJSON *ret = 0;
    char *str = kv_acquire(device, 0);
    if (str)
    {
        ret = cJSON_Parse(str);
        kv_free(str);
    }
    return ret;
}

void sld_save(const char *device, cJSON *value)
{
    char *str = cJSON_PrintUnformatted(value);
    if (kv_set(device, str, os_strlen(str)) < 0)
        SigmaLogError(0, 0, "save device %s failed", device);
    os_free(str);
}

const char *sld_iterator(SLDIterator *it)
{
    if (!it)
        return 0;
    if (!it->devices)
    {
        it->devices = kv_acquire("devices", &(it->size));
        it->pos = 0;
    }
    if (!it->devices)
        return 0;
    if (it->pos >= it->size)
    {
        if (it->devices)
            kv_free(it->devices);
        it->devices = 0;
        return 0;
    }
    char *ret = it->devices + it->pos;
    it->pos += os_strlen(ret) + 1;
    return ret;
}

void sld_iterator_release(SLDIterator *it)
{
    if (!it)
        return;
    if (it->devices)
        kv_free(it->devices);
    it->devices = 0;
}

void sld_create(const char *id, const char *name, const char *category, const char *connections[], cJSON *attributes, cJSON *capabilities)
{
    cJSON *device = cJSON_CreateObject();
    cJSON_AddItemToObject(device, "endpointId", cJSON_CreateString(id));
    cJSON_AddItemToObject(device, "friendlyName", cJSON_CreateString(name));
    cJSON_AddItemToObject(device, "additionalAttributes", cJSON_Duplicate(attributes, 1));
    cJSON_AddItemToObject(device, "category", cJSON_CreateString(category));
    cJSON *conns = cJSON_CreateArray();
    int i = 0;
    while (connections[i])
    {
        cJSON_AddItemToArray(conns, cJSON_CreateString(connections[i]));
        i++;
    }
    cJSON_AddItemToObject(device, "connections", conns);
    cJSON_AddItemToObject(device, "capabilities", cJSON_Duplicate(capabilities, 1));
    sld_save(id, device);

    int size = os_strlen(id) + 1;
    char *devices = kv_acquire("devices", &size);
    int pos = 0;
    while (pos < size)
    {
        if (!os_strcmp(devices + pos, id))
            break;
        pos += os_strlen(devices + pos) + 1;
    }
    if (pos >= size)
    {
        os_strcpy(devices + size, id);
        kv_set("devices", devices, size + os_strlen(id) + 1);
    }

    sld_profile_report(id, 0);
    cJSON_Delete(device);
}

void sld_delete(const char *id)
{
    kv_delete(id);

    int size = 0;
    char *devices = kv_acquire("devices", &size);
    int pos = 0;
    while (pos < size)
    {
        if (!os_strcmp(devices + pos, id))
            break;
        pos += os_strlen(devices + pos) + 1;
    }
    if (pos >= size)
    {
        if (size > os_strlen(id) + 1)
        {
            os_memcpy(devices + pos, devices + pos + os_strlen(id) + 1, size - os_strlen(id) - 1);
            kv_set("devices", devices, size - os_strlen(id) - 1);
        }
        else
        {
            kv_delete("devices");
        }
    }

    uint8_t seq = sll_seq();
    cJSON *packet = cJSON_CreateObject();
    cJSON *header = cJSON_CreateObject();
    cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
    cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Discovery"));
    cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_DEVICE_DELETE_REPORT));
    cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
    cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
    cJSON_AddItemToObject(packet, "header", header);
    cJSON *ep = cJSON_CreateObject();
    cJSON_AddItemToObject(ep, "endpointId", cJSON_CreateString(id));
    cJSON_AddItemToObject(packet, "endpoint", ep);

    char *str = cJSON_PrintUnformatted(packet);
    sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT);
    os_free(str);
    cJSON_Delete(packet);
}

void sld_profile_report(const char *device, const char *id)
{
    char *gateway = 0;
    if (!device)
        device = gateway = kv_acquire("gateway", 0);
    cJSON *ep = sld_load(device);
    if (!ep)
    {
        SigmaLogError(0, 0, "device %d not found.", device);
        if (gateway)
            kv_free(gateway);
        return;
    }

    uint8_t seq = sll_seq();
    cJSON *packet = cJSON_CreateObject();
    cJSON *header = cJSON_CreateObject();
    cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
    cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Discovery"));
    cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_BIND_GATEWAY_REPORT));
    cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
    cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
    cJSON_AddItemToObject(packet, "header", header);

    cJSON_AddItemToObject(packet, "endpoint", ep);

    if (id)
    {
        cJSON *payload = cJSON_CreateObject();
        cJSON_AddItemToObject(payload, "bindResult", cJSON_CreateString("OK"));
        char owner[33] = {0};
        int ret = sll_client_owner(owner);
        cJSON_AddItemToObject(payload, "isOwner", cJSON_CreateBool(!os_strcmp(owner, id)));
        cJSON_AddItemToObject(payload, "userId", cJSON_CreateString(id));
        cJSON_AddItemToObject(packet, "payload", payload);
    }
    char *str = cJSON_PrintUnformatted(packet);
    if (id)
    {
        sll_send(id, seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
    }
    else
    {
        sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
    }
    os_free(str);
    cJSON_Delete(packet);
    kv_free(gateway);
}

void sld_profile_reply(void)
{
    char *device = 0;
    char *gateway = 0;
    if (!device)
        device = gateway = kv_acquire("gateway", 0);
    cJSON *ep = sld_load(device);
    if (!ep)
    {
        SigmaLogError(0, 0, "device %d not found.", device);
        if (gateway)
            kv_free(gateway);
        return;
    }

    uint8_t seq = sll_seq();
    cJSON *packet = cJSON_CreateObject();
    cJSON *header = cJSON_CreateObject();
    cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
    cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Discovery"));
    cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_BIND_GATEWAY_REPORT));
    cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
    cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
    cJSON_AddItemToObject(packet, "header", header);

    cJSON_AddItemToObject(packet, "endpoint", ep);

    char *str = cJSON_PrintUnformatted(packet);
    sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_BROADCAST);
    os_free(str);
    cJSON_Delete(packet);
    kv_free(gateway);
}

cJSON *sld_property_load(const char *device)
{
    cJSON *ret = 0;
    char property[33 + 12] = {0};
    sprintf(property, "properties_%s", device);
    char *str = kv_acquire(property, 0);
    if (str)
    {
        ret = cJSON_Parse(str);
        kv_free(str);
    }
    if (!ret)
        ret = cJSON_CreateArray();
    return ret;
}

void sld_property_save(const char *device, cJSON *value)
{
    char property[33 + 12] = {0};
    sprintf(property, "properties_%s", device);
    char *str = cJSON_PrintUnformatted(value);
    if (kv_set(property, str, os_strlen(str)) < 0)
        SigmaLogError(0, 0, "save device property %s failed", device);
    os_free(str);
}

void sld_property_set(const char *device, const char *type, const char *name, cJSON *value)
{
    cJSON *properties = sld_property_load(device);

    cJSON *property = properties->child;
    while (property)
    {
        if (!os_strcmp(cJSON_GetObjectItem(property, "type")->valuestring, type) &&
            !os_strcmp(cJSON_GetObjectItem(property, "property")->valuestring, name))
            break;
        property = property->next;
    }
    if (!property)
    {
        property = cJSON_CreateObject();
        cJSON_AddItemToObject(property, "type", cJSON_CreateString(type));
        cJSON_AddItemToObject(property, "property", cJSON_CreateString(name));
        cJSON_AddItemToArray(properties, property);
    }
    cJSON_DeleteItemFromObject(property, "value");
    cJSON_AddItemToObject(property, "value", value);
    sld_property_save(device, properties);
    cJSON_Delete(properties);

    sld_property_report(device, OPCODE_DEVICE_CHANGE_REPORT);
}

void sld_property_report(const char *device, const char *opcode)
{
    char *gateway = 0;
    if (!device)
        device = gateway = kv_acquire("gateway", 0);
    cJSON *properties = sld_property_load(device);
    if (!properties)
    {
        SigmaLogError(0, 0, "device %d property not found.", device);
        if (gateway)
            kv_free(gateway);
        return;
    }

    uint8_t seq = sll_seq();
    cJSON *packet = cJSON_CreateObject();
    cJSON *header = cJSON_CreateObject();
    cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
    cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Basic"));
    cJSON_AddItemToObject(header, "name", cJSON_CreateString(opcode));
    cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
    cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
    cJSON_AddItemToObject(packet, "header", header);

    cJSON *ep = cJSON_CreateObject();
    cJSON_AddItemToObject(ep, "endpointId", cJSON_CreateString(device));
    cJSON_AddItemToObject(packet, "endpoint", ep);
    cJSON_AddItemToObject(packet, "properties", properties);

    char *str = cJSON_PrintUnformatted(packet);
    sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT);
    os_free(str);
    cJSON_Delete(packet);
    kv_free(gateway);
}
