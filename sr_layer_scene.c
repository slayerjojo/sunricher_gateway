#include "sr_layer_scene.h"
#include "sr_layer_link.h"
#include "sr_opcode.h"
#include "interface_os.h"
#include "sigma_mission.h"
#include "sigma_event.h"
#include "sigma_log.h"
#include "cJSON.h"
#include "interface_kv.h"

typedef struct 
{
    uint32_t timer;
    void *it;
    char client[];
}ContextDiscoverScenes;

static int mission_discover_scenes(SigmaMission *mission)
{
    ContextDiscoverScenes *ctx = sigma_mission_extends(mission);

    if (os_ticks_from(ctx->timer) < os_ticks_ms(200))
        return 0;
    ctx->timer = os_ticks();

    const char *id = kv_list_iterator("scenes", &(ctx->it));
    if (!id)
        return 1;

    char *scene = 0;
    do
    {
        scene = kv_acquire(id, 0);
        if (!scene)
        {
            SigmaLogError(0, 0, "scene %s not found.", id);
            break;
        }

        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Room"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_DISCOVER_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(packet, "header", header);
        cJSON_AddItemToObject(packet, "scene", cJSON_Parse(scene));

        char *str = cJSON_PrintUnformatted(packet);
        sll_send(ctx->client, seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
        os_free(str);
        cJSON_Delete(packet);
    }
    while (0);

    if (scene)
        os_free(scene);
    return 0;
}

static void handle_scene(void *ctx, uint8_t event, void *msg, int size)
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
    if (!ns || os_strcmp(ns->valuestring, "Room"))
        return;

    cJSON *name = cJSON_GetObjectItem(header, "name");
    if (!name)
    {
        SigmaLogError(0, 0, "name not found.");
        return;
    }

    if (!os_strcmp(name->valuestring, OPCODE_ADD_OR_UPDATE))
    {
        cJSON *scene = cJSON_GetObjectItem(packet, "scene");
        if (!scene)
        {
            SigmaLogError(0, 0, "scene not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(scene, "sceneId");
        if (!id)
        {
            SigmaLogError(0, 0, "sceneId not found.");
            return;
        }
    
        kv_list_add("scenes", id->valuestring);

        char *v = kv_acquire(id->valuestring, 0);
        cJSON *old = cJSON_Parse(v);
        if (old)
        {
            cJSON *item = old->child;
            while (item)
            {
                if (!cJSON_GetObjectItem(scene, item->string))
                    cJSON_AddItemToObject(scene, item->string, cJSON_Duplicate(item, 1));
                item = item->next;
            }
            cJSON_Delete(old);
        }

        char *str = cJSON_PrintUnformatted(scene);
        if (kv_set(id->valuestring, str, os_strlen(str)) < 0)
            SigmaLogError(0, 0, "save scene %s failed.", id->valuestring);
        os_free(str);

        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Room"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_ADD_OR_UPDATE_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(packet, "header", header);
        cJSON_AddItemToObject(packet, "scene", cJSON_Duplicate(scene, 1));

        str = cJSON_PrintUnformatted(packet);
        sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
        os_free(str);
        cJSON_Delete(packet);
    }
    else if (!os_strcmp(name->valuestring, OPCODE_DISCOVER))
    {
        cJSON *user = cJSON_GetObjectItem(packet, "user");
        if (!user)
        {
            SigmaLogError(0, 0, "unknown sender");
            return;
        }

        ContextDiscoverScenes *ctx = 0;
        SigmaMission *mission = 0;
        SigmaMissionIterator it = {0};
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_DISCOVER_ROOMS != mission->type)
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
        mission = sigma_mission_create(0, MISSION_TYPE_DISCOVER_ROOMS, mission_discover_scenes, sizeof(ContextDiscoverScenes) + os_strlen(user->valuestring) + 1);
        if (!mission)
        {
            SigmaLogError(0, 0, "out of memory.");
            return;
        }
        ctx = sigma_mission_extends(mission);
        ctx->timer = os_ticks();
        os_strcpy(ctx->client, user->valuestring);
    }
    else if (!os_strcmp(name->valuestring, OPCODE_DELETE))
    {
        cJSON *user = cJSON_GetObjectItem(packet, "user");
        if (!user)
        {
            SigmaLogError(0, 0, "unknown sender");
            return;
        }

        cJSON *scene = cJSON_GetObjectItem(packet, "scene");
        if (!scene)
        {
            SigmaLogError(0, 0, "scene not found.");
            return;
        }

        cJSON *sceneId = cJSON_GetObjectItem(scene, "sceneId");
        if (!sceneId)
        {
            SigmaLogError(0, 0, "sceneId not found.");
            return;
        }

        const char *id = sceneId->valuestring;

        kv_delete(id);
        kv_list_remove("scenes", id);

        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Room"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_DELETE_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(packet, "header", header);
        scene = cJSON_CreateObject();
        cJSON_AddItemToObject(scene, "sceneId", cJSON_CreateString(id));
        cJSON_AddItemToObject(packet, "scene", scene);

        char *str = cJSON_PrintUnformatted(packet);
        sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
        os_free(str);
        cJSON_Delete(packet);
    }
}

void sls_init(void)
{
    sigma_event_listen(EVENT_TYPE_PACKET, handle_scene, 0);
}

void sls_update(void)
{
}
