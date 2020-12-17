#include "sr_sublayer_device_gateway.h"
#include "sr_layer_link.h"
#include "sr_layer_device.h"
#include "sr_sublayer_link_mqtt.h"
#include "sr_opcode.h"
#include "interface_os.h"
#include "interface_kv.h"
#include "driver_telink_mesh.h"
#include "driver_telink_extends_sunricher.h"
#include "sigma_mission.h"
#include "sigma_event.h"
#include "sigma_log.h"
#include "hex.h"

#define GATEWAY_FIREWARE_VERSION "1.0"

#define SR_MESH_NAME "Srm@7478@a"
#define SR_MESH_PASSWORD "475869"
#define SR_MESH_LTK {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f}

const char CHAR_SPACE[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

typedef struct
{
    char name[17];
    char password[17];
    uint8_t ltk[16];
}TelinkMeshKV;

enum {
    STATE_GATEWAY_TELINK_MESH_FAMILY,
    STATE_GATEWAY_TELINK_MESH_ADD_START,
    STATE_GATEWAY_TELINK_MESH_ADD_STOP,
    STATE_GATEWAY_TELINK_MESH_RECRUIT,
    STATE_GATEWAY_TELINK_MESH_DISCOVER,
    STATE_GATEWAY_TELINK_MESH_LOOKUP,
    STATE_GATEWAY_TELINK_MESH_FILTER,
    STATE_GATEWAY_TELINK_MESH_PROFILE,
    STATE_GATEWAY_TELINK_MESH_KICKOUT,
    STATE_GATEWAY_TELINK_MESH_CONFLICT,
};

const char *name_device(SRCategory category)
{
    return "SR BLE Light";
}

const char *name_category(SRCategory category)
{
    return "LIGHT";
}

typedef struct _depot_device
{
    struct _depot_device *_next;

    uint16_t addr;
    uint16_t conflict;
    uint8_t inlist;
    uint8_t mac[6];
}DepotDevice;

typedef struct
{
    uint8_t stop:1;
    uint8_t recuit:1;
    uint8_t state;
    uint32_t timer;
    uint8_t map[32];
    cJSON *whitelist;
    DepotDevice *devices;
}ContextTelinkMeshAdd;

static int mission_telink_mesh_add(SigmaMission *mission)
{
    ContextTelinkMeshAdd *ctx = sigma_mission_extends(mission);

    if (STATE_GATEWAY_TELINK_MESH_FAMILY == ctx->state)
    {
        do
        {
            TelinkMeshKV mesh = {0};
            if (kv_get("telink_mesh", &mesh, sizeof(TelinkMeshKV)) < 0)
            {
                SigmaLogError(0, 0, "kv_acquire failed. key: telink_mesh");
                break;
            }

            int ret = telink_mesh_set(mesh.name, mesh.password, mesh.ltk, 0);
            if (ret < 0)
            {
                SigmaLogError(0, 0, "telink_mesh_set failed.(ret:%d) name:%s", ret, mesh.name);
                break;
            }

            if (ret > 0)
            {
                SigmaLogDebug(0, 0, "telink_mesh_set successed.");
                break;
            }
            return 0;
        }
        while (0);

        if (ctx->whitelist)
            cJSON_Delete(ctx->whitelist);
        ctx->whitelist = 0;

        return 1;
    }
    if (STATE_GATEWAY_TELINK_MESH_ADD_START == ctx->state)
    {
        uint8_t ltk[] = SR_MESH_LTK;
        int ret = telink_mesh_set(SR_MESH_NAME, SR_MESH_PASSWORD, ltk, 0);
        if (ret < 0)
        {
            SigmaLogError(0, 0, "telink_mesh_set failed.(ret:%d) name:%s", ret, SR_MESH_NAME);
            ctx->state = STATE_GATEWAY_TELINK_MESH_FAMILY;
            return 0;
        }
        if (ret > 0)
        {
            SigmaLogDebug(0, 0, "mesh add device started.");
            ctx->state = STATE_GATEWAY_TELINK_MESH_DISCOVER;
        }
    }
    if (STATE_GATEWAY_TELINK_MESH_ADD_STOP == ctx->state)
    {
        if (ctx->whitelist)
            cJSON_Delete(ctx->whitelist);
        ctx->whitelist = 0;

        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Discovery"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_ADD_OR_UPDATE_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(packet, "header", header);

        char *str = cJSON_PrintUnformatted(packet);
        sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT);
        os_free(str);
        cJSON_Delete(packet);

        SigmaLogDebug(0, 0, "mesh add device stoped.");
        return 1;
    }
    if (STATE_GATEWAY_TELINK_MESH_RECRUIT == ctx->state)
    {
        TelinkMeshKV mesh = {0};
        if (kv_get("telink_mesh", &mesh, sizeof(TelinkMeshKV)) < 0)
        {
            SigmaLogError(0, 0, "kv_acquire failed. key: telink_mesh");
            ctx->state = STATE_GATEWAY_TELINK_MESH_FAMILY;
            return 0;
        }

        int ret = telink_mesh_set(mesh.name, mesh.password, mesh.ltk, 2);
        if (ret < 0)
        {
            SigmaLogError(0, 0, "telink_mesh_set failed.(ret:%d)", ret);
            ctx->state = STATE_GATEWAY_TELINK_MESH_FAMILY;
            return 0;
        }
        if (ret > 0)
            ctx->state = STATE_GATEWAY_TELINK_MESH_FILTER;
    }
    if (STATE_GATEWAY_TELINK_MESH_DISCOVER == ctx->state)
    {
        if (ctx->stop)
        {
            ctx->state = STATE_GATEWAY_TELINK_MESH_FAMILY;
            return 0;
        }

        int ret = telink_mesh_device_discover();
        if (ret < 0)
        {
            SigmaLogError(0, 0, "telink_mesh_device_discover failed.(ret:%d)", ret);
            ctx->state = STATE_GATEWAY_TELINK_MESH_FAMILY;
            return 0;
        }
        if (ret > 0)
        {
            ctx->state = STATE_GATEWAY_TELINK_MESH_LOOKUP;
            ctx->timer = os_ticks();
            ctx->recuit = 0;
        }
    }
    if (STATE_GATEWAY_TELINK_MESH_LOOKUP == ctx->state)
    {
        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;

        uint16_t device = 0;
        uint8_t mac[6] = {0};
        int ret = telink_mesh_device_lookup(&device, mac);
        if (ret < 0)
        {
            SigmaLogError(0, 0, "telink_mesh_device_lookup failed.(ret:%d)", ret);
            ctx->state = STATE_GATEWAY_TELINK_MESH_FAMILY;
            return 0;
        }
        if (!ret)
        {
            if (ctx->stop || os_ticks_from(ctx->timer) > os_ticks_ms(3000))
            {
                os_memset(ctx->map, 0, 32);
                const char *device = 0;
                void *it = 0;
                while ((device = kv_list_iterator("devices", &it)))
                {
                    cJSON *ep = sld_load(device);
                    if (!ep)
                    {
                        SigmaLogError(0, 0, "device %s not found.", device);
                        continue;
                    }
                    cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
                    if (!attrs)
                    {
                        cJSON_Delete(ep);
                        continue;
                    }
                    cJSON *mac = cJSON_GetObjectItem(attrs, "mac");
                    if (!mac)
                    {
                        cJSON_Delete(ep);
                        continue;
                    }
                    cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
                    if (!addr)
                    {
                        cJSON_Delete(ep);
                        continue;
                    }
                    
                    ctx->map[addr->valueint / 8] |= 1 << (addr->valueint % 8);
                    cJSON_Delete(ep);
                }
                kv_list_iterator_release(it);

                ctx->state = STATE_GATEWAY_TELINK_MESH_DISCOVER;
                if (ctx->recuit)
                    ctx->state = STATE_GATEWAY_TELINK_MESH_RECRUIT;
                return 0;
            }
            return 0;
        }
        int inlist = 0;
        uint16_t conflict = 0;
        char id[7 + 12 + 1] = {0};
        sprintf(id, "telink_%02x%02x%02x%02x%02x%02x", 
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]);
        cJSON *ep = sld_load(id);
        if (ep)
        {
            inlist = 1;
            cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
            if (attrs && device != cJSON_GetObjectItem(attrs, "addr")->valueint)
                conflict = cJSON_GetObjectItem(attrs, "addr")->valueint;
            cJSON_Delete(ep);
        }
        SigmaLogDebug(mac, 6, "device %04x found. mac:", device);

        ctx->timer = os_ticks();

        cJSON *wl = 0;
        inlist = 1;
        if (ctx->whitelist)
        {
            wl = ctx->whitelist->child;
            while (wl)
            {
                uint8_t m[6] = {0};
                hex2bin(m, wl->valuestring, 12);
                if (!os_memcmp(m, "\xff\xff\xff\xff\xff\xff", 6))
                    break;
                if (!os_memcmp(m, mac, 6))
                    break;
                wl = wl->next;
            }
            if (!wl)
                inlist = 0;
        }

        DepotDevice *dd = ctx->devices;
        while (dd)
        {
            if (!os_memcmp(dd->mac, mac, 6))
                break;
            dd = dd->_next;
        }
        if (dd)
        {
            dd->addr = device;
            dd->inlist = inlist;
            return 0;
        }
        dd = os_malloc(sizeof(DepotDevice));
        if (!dd)
        {
            SigmaLogError(0, 0, "out of memory");
            ctx->state = STATE_GATEWAY_TELINK_MESH_FAMILY;
            return 0;
        }
        os_memcpy(dd->mac, mac, 6);
        dd->addr = device;
        dd->conflict = conflict;
        dd->inlist = inlist;
        dd->_next = ctx->devices;
        ctx->devices = dd;
        if (inlist)
            ctx->recuit = 1;
    }
    if (STATE_GATEWAY_TELINK_MESH_FILTER == ctx->state)
    {
        if (!ctx->devices)
        {
            ctx->state = STATE_GATEWAY_TELINK_MESH_ADD_START;
            return 0;
        }

        if (!ctx->devices->conflict)
        {
            uint8_t map[32] = {0};
            os_memcpy(map, ctx->map, 32);

            DepotDevice *conflict = ctx->devices->_next;
            while (conflict)
            {
                map[conflict->addr / 8] |= 1 << (conflict->addr % 8);
                conflict = conflict->_next;
            }
            int pos = 0;
            while (pos <= 255)
            {
                if (((ctx->devices->addr + pos) & 0xff) && !(map[(ctx->devices->addr + pos) / 8] & (1 << ((ctx->devices->addr + pos) % 8))))
                    break;
                pos++;
            }
            if (pos)
                ctx->devices->conflict = (ctx->devices->addr + pos) & 0xff;
        }
        if (ctx->devices->inlist)
            ctx->state = STATE_GATEWAY_TELINK_MESH_PROFILE;
        else
            ctx->state = STATE_GATEWAY_TELINK_MESH_KICKOUT;
        if (ctx->devices->conflict)
            ctx->state = STATE_GATEWAY_TELINK_MESH_CONFLICT;
    }
    if (STATE_GATEWAY_TELINK_MESH_CONFLICT == ctx->state)
    {
        int ret = telink_mesh_device_addr(ctx->devices->addr, ctx->devices->mac, ctx->devices->conflict);
        if (!ret)
            return 0;
        if (ret < 0)
        {
            SigmaLogError(0, 0, "telink_mesh_device_addr failed.(ret:%d old:%d new:%d)", ret, ctx->devices->addr, ctx->devices->conflict);
            ctx->state = STATE_GATEWAY_TELINK_MESH_KICKOUT;
            return 0;
        }
        ctx->devices->addr = ctx->devices->conflict;
        if (ctx->devices->inlist)
            ctx->state = STATE_GATEWAY_TELINK_MESH_PROFILE;
        else
            ctx->state = STATE_GATEWAY_TELINK_MESH_KICKOUT;
        ctx->timer = os_ticks();
    }
    if (STATE_GATEWAY_TELINK_MESH_KICKOUT == ctx->state)
    {
        int ret = telink_mesh_device_kickout(ctx->devices->addr);
        if (!ret)
            return 0;
        if (ret < 0)
            SigmaLogError(0, 0, "telink_mesh_device_kickout failed.(ret:%d device:%04x)", ret, ctx->devices->addr);
        DepotDevice *dd = ctx->devices;
        ctx->devices = ctx->devices->_next;
        os_free(dd);
        ctx->state = STATE_GATEWAY_TELINK_MESH_FILTER;
    }
    if (STATE_GATEWAY_TELINK_MESH_PROFILE == ctx->state)
    {
        SRCategory category;
        int ret = tmes_device_type(ctx->devices->addr, &category, 0, 0);
        if (!ret)
            return 0;
        if (ret < 0)
        {
            SigmaLogError(0, 0, "tmes_device_type failed.(ret:%d device:%d)", ret, ctx->devices->addr);
            ctx->state = STATE_GATEWAY_TELINK_MESH_KICKOUT;
            return 0;
        }
        char id[7 + 12 + 1] = {0};
        sprintf(id, "telink_%02x%02x%02x%02x%02x%02x", 
            ctx->devices->mac[0],
            ctx->devices->mac[1],
            ctx->devices->mac[2],
            ctx->devices->mac[3],
            ctx->devices->mac[4],
            ctx->devices->mac[5]);
        const char *connections[] = {
            "TELINK_BLE",
            0
        };
        cJSON *attributes = cJSON_CreateObject();
        {
            char gateway[33] = {0};
            if (kv_get("gateway", gateway, 32) > 0)
                cJSON_AddItemToObject(attributes, "gatewayId", cJSON_CreateString(gateway));
        }
        cJSON_AddItemToObject(attributes, "mac", cJSON_CreateString(id + 7));
        cJSON_AddItemToObject(attributes, "addr", cJSON_CreateNumber(ctx->devices->addr));
        cJSON_AddItemToObject(attributes, "category", cJSON_CreateNumber(category));

        const char *caps = "[{\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true}]";
        if (0x0111 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true}\
            ]";
        else if (0x0112 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true}\
            ]";
        else if (0x0113 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true}\
            ]";
        else if (0x0114 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true}\
            ]";
        else if (0x0130 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true}\
            ]";
        else if (0x0131 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true}\
            ]";
        else if (0x0132 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true},\
                {\"type\":\"ColorTemperatureController\",\"version\":\"1\",\"properties\":[\"percentage\"],\"reportable\":true}\
            ]";
        else if (0x0133 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true},\
                {\"type\":\"ColorController\",\"version\":\"1\",\"properties\":[\"rgb\"],\"reportable\":true}\
            ]";
        else if (0x0134 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true},\
                {\"type\":\"ColorController\",\"version\":\"1\",\"properties\":[\"rgb\"],\"reportable\":true},\
                {\"type\":\"WhiteController\",\"version\":\"1\",\"properties\":[\"white\"],\"reportable\":true}\
            ]";
        else if (0x0135 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true},\
                {\"type\":\"ColorTemperatureController\",\"version\":\"1\",\"properties\":[\"percentage\"],\"reportable\":true},\
                {\"type\":\"ColorController\",\"version\":\"1\",\"properties\":[\"rgb\"],\"reportable\":true}\
            ]";
        else if (0x0136 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true}\
            ]";
        else if (0x0137 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true},\
                {\"type\":\"ColorTemperatureController\",\"version\":\"1\",\"properties\":[\"percentage\"],\"reportable\":true},\
                {\"type\":\"ColorController\",\"version\":\"1\",\"properties\":[\"rgb\"],\"reportable\":true}\
            ]";
        else if (0x0138 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true}\
            ]";
        else if (0x0139 == category)
            caps = "[\
                {\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true},\
                {\"type\":\"PowerController\",\"version\":\"1\",\"properties\":[\"powerState\"],\"reportable\":true},\
                {\"type\":\"BrightnessController\",\"version\":\"1\",\"properties\":[\"brightness\"],\"reportable\":true},\
                {\"type\":\"ColorTemperatureController\",\"version\":\"1\",\"properties\":[\"percentage\"],\"reportable\":true}\
            ]";
        cJSON *capabilities = cJSON_Parse(caps);
        sld_create(id, name_device(category), name_category(category), connections, attributes, capabilities);
        sld_profile_report(id, 0, OPCODE_ADD_OR_UPDATE_REPORT, 0);

        char sht[7 + 4 + 1] = {0};
        sprintf(sht, "tladdr_%04x", ctx->devices->addr);
        kv_set(sht, id, 7 + 12);

        cJSON_Delete(attributes);
        cJSON_Delete(capabilities);
        
        ctx->map[ctx->devices->addr / 8] |= 1 << (ctx->devices->addr % 8);

        DepotDevice *dd = ctx->devices;
        ctx->devices = ctx->devices->_next;
        os_free(dd);
        ctx->state = STATE_GATEWAY_TELINK_MESH_FILTER;
    }

    return 0;
}

typedef struct 
{
    uint32_t timer;
    void *it;
    char client[];
}ContextDiscoverEndpoints;

static int mission_discover_endpoints(SigmaMission *mission)
{
    ContextDiscoverEndpoints *ctx = sigma_mission_extends(mission);

    if (os_ticks_from(ctx->timer) < 200)
        return 0;
    ctx->timer = os_ticks();

    const char *device = kv_list_iterator("devices", &(ctx->it));
    if (!device)
    {
        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Discovery"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_DISCOVER_ENDPOINTS_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(packet, "header", header);

        cJSON_AddItemToObject(packet, "endpoint", cJSON_CreateNull());

        cJSON *pl = cJSON_CreateObject();
        cJSON_AddItemToObject(pl, "theEnd", cJSON_CreateTrue());
        cJSON_AddItemToObject(packet, "payload", pl);
        char *str = cJSON_PrintUnformatted(packet);
        sll_send(ctx->client, seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
        os_free(str);
        cJSON_Delete(packet);
        return 1;
    }
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddItemToObject(payload, "theEnd", cJSON_CreateFalse());
    sld_profile_report(device, ctx->client, OPCODE_DISCOVER_ENDPOINTS_REPORT, payload);
    cJSON_Delete(payload);

    return 0;
}

static void handle_gateway_discover(void *ctx, uint8_t event, void *msg, int size)
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

    if (!os_strcmp(name->valuestring, OPCODE_GATEWAY_ADD_DEVICE_START))
    {
        cJSON *list = 0;
        cJSON *payload = cJSON_GetObjectItem(packet, "payload");
        if (payload)
            list = cJSON_GetObjectItem(payload, "macList");

        SigmaLogDebug(0, 0, "ble start scan");

        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_TELINK_MESH_ADD == mission->type)
                break;
        }
        ContextTelinkMeshAdd *ctx = 0;
        if (!mission)
        {
            mission = sigma_mission_create(0, MISSION_TYPE_TELINK_MESH_ADD, mission_telink_mesh_add, sizeof(ContextTelinkMeshAdd));
            if (!mission)
            {
                SigmaLogError(0, 0, "out of memory");
                return;
            }
            ctx = sigma_mission_extends(mission);
            ctx->state = STATE_GATEWAY_TELINK_MESH_ADD_START;
            ctx->whitelist = 0;
        }
        ctx = sigma_mission_extends(mission);
        ctx->stop = 0;
        ctx->timer = os_ticks();
        if (ctx->whitelist)
            cJSON_Delete(ctx->whitelist);
        ctx->whitelist = 0;
        if (list)
            ctx->whitelist = cJSON_Duplicate(list, 1);
    }
    else if (!os_strcmp(name->valuestring, OPCODE_GATEWAY_ADD_DEVICE_STOP))
    {
        SigmaLogDebug(0, 0, "ble stop scan");
        
        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_TELINK_MESH_ADD == mission->type)
                break;
        }
        if (!mission)
            return;
        ContextTelinkMeshAdd *ctx = sigma_mission_extends(mission);
        ctx->stop = 1;
    }
    else if (!os_strcmp(name->valuestring, OPCODE_DISCOVER_GATEWAY))
    {
        char *gateway = 0;
        cJSON *ep = 0;
        do
        {
            gateway = kv_acquire("gateway", 0);
            if (!gateway)
            {
                SigmaLogError(0, 0, "gateway not found.");
                break;
            }

            ep = sld_load(gateway);
            if (!ep)
            {
                SigmaLogError(0, 0, "device %d not found.", gateway);
                break;
            }

            uint8_t seq = sll_seq();
            cJSON *packet = cJSON_CreateObject();
            cJSON *header = cJSON_CreateObject();
            cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
            cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Discovery"));
            cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_DISCOVER_GATEWAY_RESP));
            cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
            cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
            cJSON_AddItemToObject(packet, "header", header);

            cJSON_AddItemToObject(packet, "endpoint", ep);

            char *str = cJSON_PrintUnformatted(packet);

            SigmaLogAction(0, 0, "response discover gateway: %s", str);

            sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_BROADCAST);
            os_free(str);
            cJSON_Delete(packet);
        }
        while (0);
        if (gateway)
            kv_free(gateway);
    }
    else if (!os_strcmp(name->valuestring, OPCODE_DISCOVER_ENDPOINTS))
    {
        cJSON *user = cJSON_GetObjectItem(packet, "user");
        if (!user)
        {
            SigmaLogError(0, 0, "unknown sender");
            return;
        }

        ContextDiscoverEndpoints *ctx = 0;
        SigmaMission *mission = 0;
        SigmaMissionIterator it = {0};
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_DISCOVER_ENDPOINTS != mission->type)
                continue;
            ctx = sigma_mission_extends(mission);
            if (os_strcmp(ctx->client, user->valuestring))
                continue;
            break;
        }
        if (mission)
        {
            kv_list_iterator_release(ctx->it);
            sigma_mission_release(mission);
        }
        mission = sigma_mission_create(0, MISSION_TYPE_DISCOVER_ENDPOINTS, mission_discover_endpoints, sizeof(ContextDiscoverEndpoints) + os_strlen(user->valuestring) + 1);
        if (!mission)
        {
            SigmaLogError(0, 0, "out of memory.");
            return;
        }
        ctx = sigma_mission_extends(mission);
        ctx->timer = os_ticks();
        os_strcpy(ctx->client, user->valuestring);
    }
}

typedef struct 
{
    union
    {
        uint8_t onoff;
        uint8_t brightness;
        uint8_t white;
        uint8_t percentage;
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        }rgb;
    };
    char device[];
}ContextPCOperate;

static int mission_powercontroller_operate(SigmaMission *mission)
{
    ContextPCOperate *ctx = sigma_mission_extends(mission);

    cJSON *ep = sld_load(ctx->device);
    if (!ep)
    {
        SigmaLogError(0, 0, "device %s not found.", ctx->device);
        return 1;
    }

    do
    {
        cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
        if (!attrs)
        {
            SigmaLogError(0, 0, "device %s not found", ctx->device);
            break;
        }

        cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
        if (!addr)
        {
            SigmaLogError(0, 0, "device %s addr not found", ctx->device);
            break;
        }

        int ret = telink_mesh_light_onoff(addr->valueint, ctx->onoff, 0);
        if (!ret)
            break;

        if (ret > 0)
        {
            sld_property_set(ctx->device, "PowerController", "powerState", cJSON_CreateString(ctx->onoff ? "ON" : "OFF"));
            sld_property_report(ctx->device, "ChangeReport");
        }

        cJSON_Delete(ep);

        return 1;
    }
    while(0);

    cJSON_Delete(ep);

    return 0;
}

static int mission_brightnesscontroller_operate(SigmaMission *mission)
{
    ContextPCOperate *ctx = sigma_mission_extends(mission);

    cJSON *ep = sld_load(ctx->device);
    if (!ep)
    {
        SigmaLogError(0, 0, "device %s not found.", ctx->device);
        return 1;
    }

    do
    {
        cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
        if (!attrs)
        {
            SigmaLogError(0, 0, "device %s not found", ctx->device);
            break;
        }

        cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
        if (!addr)
        {
            SigmaLogError(0, 0, "device %s addr not found", ctx->device);
            break;
        }

        int ret = telink_mesh_light_luminance(addr->valueint, ctx->brightness);
        if (!ret)
            break;

        if (ret > 0)
        {
            sld_property_set(ctx->device, "BrightnessController", "brightness", cJSON_CreateNumber(ctx->brightness));
            sld_property_report(ctx->device, "ChangeReport");
        }

        cJSON_Delete(ep);

        return 1;
    }
    while(0);

    cJSON_Delete(ep);

    return 0;
}

static int mission_colorcontroller_operate(SigmaMission *mission)
{
    ContextPCOperate *ctx = sigma_mission_extends(mission);

    cJSON *ep = sld_load(ctx->device);
    if (!ep)
    {
        SigmaLogError(0, 0, "device %s not found.", ctx->device);
        return 1;
    }

    do
    {
        cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
        if (!attrs)
        {
            SigmaLogError(0, 0, "device %s not found", ctx->device);
            break;
        }

        cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
        if (!addr)
        {
            SigmaLogError(0, 0, "device %s addr not found", ctx->device);
            break;
        }

        int ret = telink_mesh_light_color(addr->valueint, ctx->rgb.r, ctx->rgb.g, ctx->rgb.b);
        if (!ret)
            break;

        if (ret > 0)
        {
            cJSON *color = cJSON_CreateObject();
            cJSON_AddItemToObject(color, "red", cJSON_CreateNumber(ctx->rgb.r));
            cJSON_AddItemToObject(color, "green", cJSON_CreateNumber(ctx->rgb.g));
            cJSON_AddItemToObject(color, "blue", cJSON_CreateNumber(ctx->rgb.b));
            sld_property_set(ctx->device, "ColorController", "rgb", color);
            sld_property_report(ctx->device, "ChangeReport");
        }

        cJSON_Delete(ep);

        return 1;
    }
    while(0);

    cJSON_Delete(ep);

    return 0;
}

static int mission_whitecontroller_operate(SigmaMission *mission)
{
    ContextPCOperate *ctx = sigma_mission_extends(mission);

    cJSON *ep = sld_load(ctx->device);
    if (!ep)
    {
        SigmaLogError(0, 0, "device %s not found.", ctx->device);
        return 1;
    }

    do
    {
        cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
        if (!attrs)
        {
            SigmaLogError(0, 0, "device %s not found", ctx->device);
            break;
        }

        cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
        if (!addr)
        {
            SigmaLogError(0, 0, "device %s addr not found", ctx->device);
            break;
        }

        int ret = telink_mesh_light_ctcolor(addr->valueint, ctx->white);
        if (!ret)
            break;

        if (ret > 0)
        {
            sld_property_set(ctx->device, "WhiteController", "white", cJSON_CreateNumber(ctx->white));
            sld_property_report(ctx->device, "ChangeReport");
        }

        cJSON_Delete(ep);

        return 1;
    }
    while(0);

    cJSON_Delete(ep);

    return 0;
}

static int mission_color_temperature_controller_operate(SigmaMission *mission)
{
    ContextPCOperate *ctx = sigma_mission_extends(mission);

    cJSON *ep = sld_load(ctx->device);
    if (!ep)
    {
        SigmaLogError(0, 0, "device %s not found.", ctx->device);
        return 1;
    }

    do
    {
        cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
        if (!attrs)
        {
            SigmaLogError(0, 0, "device %s not found", ctx->device);
            break;
        }

        cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
        if (!addr)
        {
            SigmaLogError(0, 0, "device %s addr not found", ctx->device);
            break;
        }

        int ret = telink_mesh_light_ctcolor(addr->valueint, ctx->percentage);
        if (!ret)
            break;

        if (ret > 0)
        {
            sld_property_set(ctx->device, "ColorTemperatureController", "percentage", cJSON_CreateNumber(ctx->percentage));
            sld_property_report(ctx->device, "ChangeReport");
        }

        cJSON_Delete(ep);

        return 1;
    }
    while(0);

    cJSON_Delete(ep);

    return 0;
}

static void handle_device_powercontroller(void *ctx, uint8_t event, void *msg, int size)
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
    if (!ns || os_strcmp(ns->valuestring, "PowerController"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!name)
    {
        SigmaLogError(0, 0, "name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_POWERCONTROLLER_TURNON))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(ep, "endpointId");
        if (!id)
        {
            SigmaLogError(0, 0, "endpoint.endpointId not found.");
            return;
        }

        ContextPCOperate *ctx = 0;
        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_DEVICE_POWERCONTROLLER_OPERATE == mission->type)
                continue;
            ctx = sigma_mission_extends(mission);
            if (os_strcmp(ctx->device, id->valuestring))
                continue;
            break;
        }
        if (mission)
            sigma_mission_release(mission);
        mission = sigma_mission_create(
            0, 
            MISSION_TYPE_DEVICE_POWERCONTROLLER_OPERATE, 
            mission_powercontroller_operate, 
            sizeof(ContextPCOperate) + os_strlen(id->valuestring) + 1);
        if (!mission)
        {
            SigmaLogError(0, 0, "out of memory");
            return;
        }
        ctx = sigma_mission_extends(mission);
        os_strcpy(ctx->device, id->valuestring);
        ctx->onoff = 1;
    }
    else if (!os_strcmp(name->valuestring, OPCODE_POWERCONTROLLER_TURNOFF))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(ep, "endpointId");
        if (!id)
        {
            SigmaLogError(0, 0, "endpoint.endpointId not found.");
            return;
        }

        ContextPCOperate *ctx = 0;
        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_DEVICE_POWERCONTROLLER_OPERATE == mission->type)
                continue;
            ctx = sigma_mission_extends(mission);
            if (os_strcmp(ctx->device, id->valuestring))
                continue;
            break;
        }
        if (mission)
            sigma_mission_release(mission);
        mission = sigma_mission_create(
            0, 
            MISSION_TYPE_DEVICE_POWERCONTROLLER_OPERATE, 
            mission_powercontroller_operate, 
            sizeof(ContextPCOperate) + os_strlen(id->valuestring) + 1);
        if (!mission)
        {
            SigmaLogError(0, 0, "out of memory");
            return;
        }
        ctx = sigma_mission_extends(mission);
        os_strcpy(ctx->device, id->valuestring);
        ctx->onoff = 0;
    }
}

static void handle_device_brightnesscontroller(void *ctx, uint8_t event, void *msg, int size)
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
    if (!ns || os_strcmp(ns->valuestring, "BrightnessController"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!name)
    {
        SigmaLogError(0, 0, "name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_BRIGHTNESSCONTROLLER_SETBRIGHTNESS))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(ep, "endpointId");
        if (!id)
        {
            SigmaLogError(0, 0, "endpoint.endpointId not found.");
            return;
        }

        cJSON *payload = cJSON_GetObjectItem(packet, "payload");
        if (!payload)
        {
            SigmaLogError(0, 0, "payload not found.");
            return;
        }

        cJSON *brightness = cJSON_GetObjectItem(payload, "brightness");
        if (!brightness)
        {
            SigmaLogError(0, 0, "payload.brightness not found.");
            return;
        }

        ContextPCOperate *ctx = 0;
        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_DEVICE_BRIGHTNESSCONTROLLER_OPERATE == mission->type)
                continue;
            ctx = sigma_mission_extends(mission);
            if (os_strcmp(ctx->device, id->valuestring))
                continue;
            break;
        }
        if (mission)
            sigma_mission_release(mission);
        mission = sigma_mission_create(
            0, 
            MISSION_TYPE_DEVICE_BRIGHTNESSCONTROLLER_OPERATE, 
            mission_brightnesscontroller_operate, 
            sizeof(ContextPCOperate) + os_strlen(id->valuestring) + 1);
        if (!mission)
        {
            SigmaLogError(0, 0, "out of memory");
            return;
        }
        ctx = sigma_mission_extends(mission);
        os_strcpy(ctx->device, id->valuestring);
        ctx->brightness = brightness->valueint;
    }
}

static void handle_device_colorcontroller(void *ctx, uint8_t event, void *msg, int size)
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
    if (!ns || os_strcmp(ns->valuestring, "ColorController"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!name)
    {
        SigmaLogError(0, 0, "name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_COLORCONTROLLER_SETCOLOR))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(ep, "endpointId");
        if (!id)
        {
            SigmaLogError(0, 0, "endpoint.endpointId not found.");
            return;
        }

        cJSON *payload = cJSON_GetObjectItem(packet, "payload");
        if (!payload)
        {
            SigmaLogError(0, 0, "payload not found.");
            return;
        }

        cJSON *color = cJSON_GetObjectItem(payload, "rgb");
        if (color )
        {
            ContextPCOperate *ctx = 0;
            SigmaMissionIterator it = {0};
            SigmaMission *mission = 0;
            while ((mission = sigma_mission_iterator(&it)))
            {
                if (MISSION_TYPE_DEVICE_COLORCONTROLLER_OPERATE == mission->type)
                    continue;
                ctx = sigma_mission_extends(mission);
                if (os_strcmp(ctx->device, id->valuestring))
                    continue;
                break;
            }
            if (mission)
                sigma_mission_release(mission);
            mission = sigma_mission_create(
                0, 
                MISSION_TYPE_DEVICE_COLORCONTROLLER_OPERATE, 
                mission_colorcontroller_operate, 
                sizeof(ContextPCOperate) + os_strlen(id->valuestring) + 1);
            if (!mission)
            {
                SigmaLogError(0, 0, "out of memory");
                return;
            }
            ctx = sigma_mission_extends(mission);
            os_strcpy(ctx->device, id->valuestring);
            if (cJSON_GetObjectItem(color, "red"))
                ctx->rgb.r = cJSON_GetObjectItem(color, "red")->valueint;
            if (cJSON_GetObjectItem(color, "green"))
                ctx->rgb.g = cJSON_GetObjectItem(color, "green")->valueint;
            if (cJSON_GetObjectItem(color, "blue"))
                ctx->rgb.b = cJSON_GetObjectItem(color, "blue")->valueint;
        }
    }
    else if (!os_strcmp(name->valuestring, OPCODE_COLORCONTROLLER_SETRGB))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(ep, "endpointId");
        if (!id)
        {
            SigmaLogError(0, 0, "endpoint.endpointId not found.");
            return;
        }

        cJSON *payload = cJSON_GetObjectItem(packet, "payload");
        if (!payload)
        {
            SigmaLogError(0, 0, "payload not found.");
            return;
        }

        cJSON *color = cJSON_GetObjectItem(payload, "rgb");
        if (color )
        {
            ContextPCOperate *ctx = 0;
            SigmaMissionIterator it = {0};
            SigmaMission *mission = 0;
            while ((mission = sigma_mission_iterator(&it)))
            {
                if (MISSION_TYPE_DEVICE_COLORCONTROLLER_OPERATE == mission->type)
                    continue;
                ctx = sigma_mission_extends(mission);
                if (os_strcmp(ctx->device, id->valuestring))
                    continue;
                break;
            }
            if (mission)
                sigma_mission_release(mission);
            mission = sigma_mission_create(
                0, 
                MISSION_TYPE_DEVICE_COLORCONTROLLER_OPERATE, 
                mission_colorcontroller_operate, 
                sizeof(ContextPCOperate) + os_strlen(id->valuestring) + 1);
            if (!mission)
            {
                SigmaLogError(0, 0, "out of memory");
                return;
            }
            ctx = sigma_mission_extends(mission);
            os_strcpy(ctx->device, id->valuestring);
            if (cJSON_GetObjectItem(color, "red"))
                ctx->rgb.r = cJSON_GetObjectItem(color, "red")->valueint;
            if (cJSON_GetObjectItem(color, "green"))
                ctx->rgb.g = cJSON_GetObjectItem(color, "green")->valueint;
            if (cJSON_GetObjectItem(color, "blue"))
                ctx->rgb.b = cJSON_GetObjectItem(color, "blue")->valueint;
        }
    }
}

static void handle_device_whitecontroller(void *ctx, uint8_t event, void *msg, int size)
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
    if (!ns || os_strcmp(ns->valuestring, "WhiteController"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!name)
    {
        SigmaLogError(0, 0, "name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_WHITECONTROLLER_SETWHITE))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(ep, "endpointId");
        if (!id)
        {
            SigmaLogError(0, 0, "endpoint.endpointId not found.");
            return;
        }

        cJSON *payload = cJSON_GetObjectItem(packet, "payload");
        if (!payload)
        {
            SigmaLogError(0, 0, "payload not found.");
            return;
        }

        cJSON *color = cJSON_GetObjectItem(payload, "value");
        if (color)
        {
            ContextPCOperate *ctx = 0;
            SigmaMissionIterator it = {0};
            SigmaMission *mission = 0;
            while ((mission = sigma_mission_iterator(&it)))
            {
                if (MISSION_TYPE_DEVICE_WHITECONTROLLER_OPERATE == mission->type)
                    continue;
                ctx = sigma_mission_extends(mission);
                if (os_strcmp(ctx->device, id->valuestring))
                    continue;
                break;
            }
            if (mission)
                sigma_mission_release(mission);
            mission = sigma_mission_create(
                0, 
                MISSION_TYPE_DEVICE_WHITECONTROLLER_OPERATE, 
                mission_whitecontroller_operate, 
                sizeof(ContextPCOperate) + os_strlen(id->valuestring) + 1);
            if (!mission)
            {
                SigmaLogError(0, 0, "out of memory");
                return;
            }
            ctx = sigma_mission_extends(mission);
            os_strcpy(ctx->device, id->valuestring);
            ctx->white = color->valueint;
        }
    }
}

static void handle_device_color_temperature_controller(void *ctx, uint8_t event, void *msg, int size)
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
    if (!ns || os_strcmp(ns->valuestring, "ColorTemperatureController"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!name)
    {
        SigmaLogError(0, 0, "name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_COLORTEMPERATURECONTROLLER_SETPERCENTAGE))
    {
        cJSON *ep = cJSON_GetObjectItem(packet, "endpoint");
        if (!ep)
        {
            SigmaLogError(0, 0, "endpoint not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(ep, "endpointId");
        if (!id)
        {
            SigmaLogError(0, 0, "endpoint.endpointId not found.");
            return;
        }

        cJSON *payload = cJSON_GetObjectItem(packet, "payload");
        if (!payload)
        {
            SigmaLogError(0, 0, "payload not found.");
            return;
        }

        cJSON *percentage = cJSON_GetObjectItem(payload, "percentage");
        if (percentage)
        {
            ContextPCOperate *ctx = 0;
            SigmaMissionIterator it = {0};
            SigmaMission *mission = 0;
            while ((mission = sigma_mission_iterator(&it)))
            {
                if (MISSION_TYPE_DEVICE_COLORTEMPERATURECONTROLLER_OPERATE == mission->type)
                    continue;
                ctx = sigma_mission_extends(mission);
                if (os_strcmp(ctx->device, id->valuestring))
                    continue;
                break;
            }
            if (mission)
                sigma_mission_release(mission);
            mission = sigma_mission_create(
                0, 
                MISSION_TYPE_DEVICE_COLORTEMPERATURECONTROLLER_OPERATE, 
                mission_color_temperature_controller_operate, 
                sizeof(ContextPCOperate) + os_strlen(id->valuestring) + 1);
            if (!mission)
            {
                SigmaLogError(0, 0, "out of memory");
                return;
            }
            ctx = sigma_mission_extends(mission);
            os_strcpy(ctx->device, id->valuestring);
            ctx->percentage = percentage->valueint;
        }
    }
}

static int mission_device_kickout(SigmaMission *mission)
{
    uint16_t *addr = sigma_mission_extends(mission);

    int ret = telink_mesh_device_kickout(*addr);
    if (!ret)
        return ret;
    if (ret < 0)
        SigmaLogError(0, 0, "telink_mesh_device_kickout(addr:%04x) failed", *addr);
    return 1;
}

static void handle_device_delete(void *ctx, uint8_t event, void *msg, int size)
{
    const char *device = (const char *)msg;

    cJSON *ep = sld_load(device);
    if (ep)
    {
        cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
        if (attrs)
        {
            cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
            if (addr)
            {
                SigmaMission *mission = 0;
                SigmaMissionIterator it = {0};
                while ((mission = sigma_mission_iterator(&it)))
                {
                    if (MISSION_TYPE_TELINK_MESH_DEVICE_KICKOUT != mission->type)
                        continue;
                    uint16_t *ctx = sigma_mission_extends(mission);
                    if (*ctx != addr->valueint)
                        continue;
                    break;
                }
                if (!mission)
                {
                    mission = sigma_mission_create(0, MISSION_TYPE_TELINK_MESH_DEVICE_KICKOUT, mission_device_kickout, sizeof(uint16_t));
                    if (!mission)
                        SigmaLogError(0, 0, "out of memory.");
                    if (mission)
                        *(uint16_t *)sigma_mission_extends(mission) = addr->valueint;
                }
            }
        }
    }
                    
    cJSON_Delete(ep);
}

static void handle_gateway_bounded(void *ctx, uint8_t event, void *msg, int size)
{
    const char *client = msg;
    
    sld_profile_report(0, client, OPCODE_BIND_GATEWAY_REPORT, 0);
}

void ssdg_init(void)
{
    telink_mesh_init();

    TelinkMeshKV mesh = {0};
    if (kv_get("telink_mesh", &mesh, sizeof(TelinkMeshKV)) < 0)
    {
        int i;
        for (i = 0; i < 16; i++)
        {
            mesh.name[i] = CHAR_SPACE[os_rand() % (sizeof(CHAR_SPACE) - 1)];
            mesh.password[i] = CHAR_SPACE[os_rand() % (sizeof(CHAR_SPACE) - 1)];
            mesh.ltk[i] = os_rand();
        }
        kv_set("telink_mesh", &mesh, sizeof(TelinkMeshKV));
    }
       
    /*
    SigmaMission *mission = sigma_mission_create(0, MISSION_TYPE_TELINK_MESH_ADD, mission_telink_mesh_add, sizeof(ContextTelinkMeshAdd));
    if (!mission)
        SigmaLogError(0, 0, "out of memory");
    if (mission)
    {
        ContextTelinkMeshAdd *ctx = sigma_mission_extends(mission);
        ctx->state = STATE_GATEWAY_TELINK_MESH_FAMILY;
        ctx->whitelist = 0;
        ctx->stop = 1;
    }
    */

    char *gateway = kv_acquire("gateway", 0);
    if (!gateway)
    {
        char id[33] = {0};
        sld_id_create(id);

        const char *connections[] = {
            "TELINK_BLE",
            0
        };
        cJSON *attrs = cJSON_CreateObject();
        char mac[12 + 1] = {0};
        bin2hex(mac, network_net_mac(), 6);
        cJSON_AddItemToObject(attrs, "wifiMac", cJSON_CreateString(mac));
        cJSON_AddItemToObject(attrs, "firmwareVersion", cJSON_CreateString(GATEWAY_FIREWARE_VERSION));

        cJSON *capabilities = cJSON_Parse("[{\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true}]");

        sld_create(id, "SR BLE Gateway", "SR_GATEWAY", connections, attrs, capabilities);
        kv_set("gateway", id, os_strlen(id));
        
        sld_profile_report(id, 0, OPCODE_BIND_GATEWAY_REPORT, 0);

        cJSON_Delete(attrs);
        cJSON_Delete(capabilities);
        
        gateway = kv_acquire("gateway", 0);
    }
    sslm_start(gateway);
    os_free(gateway);

    sigma_event_listen(EVENT_TYPE_PACKET, handle_gateway_discover, 0);
    sigma_event_listen(EVENT_TYPE_PACKET, handle_device_powercontroller, 0);
    sigma_event_listen(EVENT_TYPE_PACKET, handle_device_brightnesscontroller, 0);
    sigma_event_listen(EVENT_TYPE_PACKET, handle_device_colorcontroller, 0);
    sigma_event_listen(EVENT_TYPE_PACKET, handle_device_whitecontroller, 0);
    sigma_event_listen(EVENT_TYPE_PACKET, handle_device_color_temperature_controller, 0);
    sigma_event_listen(EVENT_TYPE_DEVICE_DELETE, handle_device_delete, 0);
    sigma_event_listen(EVENT_TYPE_GATEWAY_BOUNDED, handle_gateway_bounded, 0);
}

void ssdg_update(void)
{
    telink_mesh_update();

    {
        uint16_t src;
        uint8_t online;
        uint8_t luminance;
        if (telink_mesh_light_status(&src, &online, &luminance))
        {
            char key[7 + 4 + 1] = {0};
            sprintf(key, "tladdr_%04x", src);

            char id[7 + 12 + 1] = {0};
            int ret = kv_get(key, id, 7 + 12 + 1);
            if (ret < 0)
                SigmaLogError(0, 0, "device addr:%04x not found.", src);
            if (ret > 0)
            {
                sld_property_set(id, "EndpointHealth", "connectivity", cJSON_CreateString(online ? "OK" : "UNREACHABLE"));
                sld_property_set(id, "BrightnessController", "brightness", cJSON_CreateNumber(luminance));
                sld_property_set(id, "PowerController", "powerState", cJSON_CreateNumber(!!luminance));
                sld_property_report(id, "ChangeReport");
            }
        }
    }
}

