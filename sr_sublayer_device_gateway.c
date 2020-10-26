#include "sr_sublayer_device_gateway.h"
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

    cJSON *method = cJSON_GetObjectItem(packet, "method");
    if (!method || os_strcmp(method->valuestring, "Directive"))
        return;

    cJSON *ns = cJSON_GetObjectItem(packet, "namespace");
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
    }
    sslm_start(gateway);
    os_free(gateway);

    sigma_event_listen(EVENT_TYPE_PACKET, handle_gateway_discover, 0);
}

void ssdg_update(void)
{
}

