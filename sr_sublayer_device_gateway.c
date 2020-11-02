#include "sr_sublayer_device_gateway.h"
#include "sr_layer_link.h"
#include "sr_layer_device.h"
#include "sr_sublayer_link_mqtt.h"
#include "sr_opcode.h"
#include "interface_os.h"
#include "interface_kv.h"
#include "sigma_event.h"
#include "sigma_log.h"

#define GATEWAY_FIREWARE_VERSION "1.0"

static void handle_gateway_discover(void *ctx, uint8_t event, void *msg, int size)
{
    cJSON *packet = (cJSON *)msg;

    cJSON *header = cJSON_GetObjectItem(packet, "header");
    if (!header)
    {
        SigmaLogError("header not found.");
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
        SigmaLogError("name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_GATEWAY_ADD_DEVICE_START))
    {
        cJSON *payload = cJSON_GetObjectItem(packet, "payload");
        if (!payload)
        {
            SigmaLogError("payload not found.");
            return;
        }

        cJSON *list = cJSON_GetObjectItem(payload, "macList");
        if (!list)
        {
            SigmaLogError("macList not found.");
            return;
        }

        SigmaLogAction("ble start scan");
    }
    else if (!os_strcmp(name->valuestring, OPCODE_GATEWAY_ADD_DEVICE_STOP))
    {
        SigmaLogAction("ble stop scan");
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
                SigmaLogError("gateway not found.");
                break;
            }

            ep = sld_load(gateway);
            if (!ep)
            {
                SigmaLogError("device %d not found.", gateway);
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
            sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_BROADCAST);
            os_free(str);
            cJSON_Delete(packet);
        }
        while (0);
        if (gateway)
            kv_free(gateway);
    }
}

void ssdg_init(void)
{
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
        cJSON_AddItemToObject(attrs, "firmwareVersion", cJSON_CreateString(GATEWAY_FIREWARE_VERSION));

        cJSON *capabilities = cJSON_Parse("[{\"type\":\"EndpointHealth\",\"version\":\"1\",\"properties\":[\"connectivity\"],\"reportable\":true}]");

        sld_create(id, "SR BLE Gateway", "SR_GATEWAY", connections, attrs, capabilities);
        kv_set("gateway", id, os_strlen(id));
        cJSON_Delete(attrs);
        cJSON_Delete(capabilities);
        
        gateway = kv_acquire("gateway", 0);
    }
    sslm_start(gateway);
    os_free(gateway);

    sigma_event_listen(EVENT_TYPE_PACKET, handle_gateway_discover, 0);
}

void ssdg_update(void)
{
}

