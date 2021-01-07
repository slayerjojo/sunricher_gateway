#include "sr_layer_room.h"
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
}ContextDiscoverRooms;

static int mission_discover_rooms(SigmaMission *mission, uint8_t cleanup)
{
    ContextDiscoverRooms *ctx = sigma_mission_extends(mission);

    if (cleanup)
    {
        kv_list_iterator_release(ctx->it);
        return 1;
    }

    if (os_ticks_from(ctx->timer) < os_ticks_ms(200))
        return 0;
    ctx->timer = os_ticks();

    const char *id = kv_list_iterator("rooms", &(ctx->it));
    if (!id)
        return 1;

    char *room = 0;
    do
    {
        room = kv_acquire(id, 0);
        if (!room)
        {
            SigmaLogError(0, 0, "room %s not found.", id);
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
        cJSON_AddItemToObject(packet, "room", cJSON_Parse(room));

        char *str = cJSON_PrintUnformatted(packet);
        sll_send(ctx->client, seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
        os_free(str);
        cJSON_Delete(packet);
    }
    while (0);

    if (room)
        os_free(room);
    return 0;
}

static void handle_room(void *ctx, uint8_t event, void *msg, int size)
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
        cJSON *room = cJSON_GetObjectItem(packet, "room");
        if (!room)
        {
            SigmaLogError(0, 0, "room not found.");
            return;
        }

        cJSON *id = cJSON_GetObjectItem(room, "roomId");
        if (!id)
        {
            SigmaLogError(0, 0, "roomId not found.");
            return;
        }
    
        kv_list_add("rooms", id->valuestring);

        char *v = kv_acquire(id->valuestring, 0);
        cJSON *old = cJSON_Parse(v);
        if (old)
        {
            cJSON *item = old->child;
            while (item)
            {
                if (!cJSON_GetObjectItem(room, item->string))
                    cJSON_AddItemToObject(room, item->string, cJSON_Duplicate(item, 1));
                item = item->next;
            }
            cJSON_Delete(old);
        }

        char *str = cJSON_PrintUnformatted(room);
        if (kv_set(id->valuestring, str, os_strlen(str)) < 0)
            SigmaLogError(0, 0, "save room %s failed.", id->valuestring);
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
        cJSON_AddItemToObject(packet, "room", cJSON_Duplicate(room, 1));

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

        ContextDiscoverRooms *ctx = 0;
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
        mission = sigma_mission_create(0, MISSION_TYPE_DISCOVER_ROOMS, mission_discover_rooms, sizeof(ContextDiscoverRooms) + os_strlen(user->valuestring) + 1);
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

        cJSON *room = cJSON_GetObjectItem(packet, "room");
        if (!room)
        {
            SigmaLogError(0, 0, "room not found.");
            return;
        }

        cJSON *roomId = cJSON_GetObjectItem(room, "roomId");
        if (!roomId)
        {
            SigmaLogError(0, 0, "roomId not found.");
            return;
        }

        const char *id = roomId->valuestring;

        kv_delete(id);
        kv_list_remove("rooms", id);

        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Room"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_DELETE_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(packet, "header", header);
        room = cJSON_CreateObject();
        cJSON_AddItemToObject(room, "roomId", cJSON_CreateString(id));
        cJSON_AddItemToObject(packet, "room", room);

        char *str = cJSON_PrintUnformatted(packet);
        sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
        os_free(str);
        cJSON_Delete(packet);
    }
}

void slr_init(void)
{
    sigma_event_listen(EVENT_TYPE_PACKET, handle_room, 0);
}

void slr_update(void)
{
}
