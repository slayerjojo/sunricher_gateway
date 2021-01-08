#include "sr_layer_scene.h"
#include "sr_layer_link.h"
#include "sr_layer_device.h"
#include "sr_opcode.h"
#include "interface_os.h"
#include "driver_telink_mesh.h"
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

static int mission_discover_scenes(SigmaMission *mission, uint8_t cleanup)
{
    ContextDiscoverScenes *ctx = sigma_mission_extends(mission);

    if (cleanup)
    {
        kv_list_iterator_release(ctx->it);
        return 1;
    }

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
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Scene"));
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

enum
{
    STATE_TYPE_SCENE_UPDATE_INIT,
    STATE_TYPE_SCENE_UPDATE_DONE,
    STATE_TYPE_SCENE_UPDATE_KICKOUT,
    STATE_TYPE_SCENE_UPDATE_KICKOUT_GROUP,
    STATE_TYPE_SCENE_UPDATE_APPLY,
    STATE_TYPE_SCENE_UPDATE_APPLY_PROPERTIES,
};

typedef struct
{
    uint8_t state;
    uint8_t scene;
    cJSON *scenes;
    cJSON *apply;
    cJSON *old;
    cJSON *final;
    char id[];
}ContextSceneUpdate;

static int mission_scene_recall(SigmaMission *mission, uint8_t cleanup)
{
    uint8_t *scene = sigma_mission_extends(mission);

    if (cleanup)
        return 1;

    int ret = telink_mesh_scene_load(0x8000 + *scene, *scene);
    if (!ret)
        return 0;
    if (ret < 0)
        SigmaLogError(0, 0, "telink_mesh_scene_load(scene:%d) failed", *scene);
    return 1;
}

static int mission_scene_update(SigmaMission *mission, uint8_t cleanup)
{
    ContextSceneUpdate *ctx = sigma_mission_extends(mission);

    if (cleanup)
    {
        if (ctx->scenes)
            cJSON_Delete(ctx->scenes);
        ctx->scenes = 0;

        if (ctx->apply)
            cJSON_Delete(ctx->apply);
        ctx->apply = 0;

        if (ctx->old)
            cJSON_Delete(ctx->old);
        ctx->old = 0;

        if (ctx->final)
            cJSON_Delete(ctx->final);
        ctx->final = 0;
        return 1;
    }

    if (STATE_TYPE_SCENE_UPDATE_INIT == ctx->state)
    {
        if (!ctx->apply || !ctx->apply->child)
        {
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            if (ctx->old && ctx->old->child)
                ctx->state = STATE_TYPE_SCENE_UPDATE_KICKOUT;
            return 0;
        }
        if (!ctx->old)
        {
            ctx->state = STATE_TYPE_SCENE_UPDATE_APPLY;
            return 0;
        }
        cJSON *item = cJSON_GetArrayItem(ctx->apply, 0);
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *ep = cJSON_GetObjectItem(item, "endpoint");
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *epid = cJSON_GetObjectItem(ep, "endpointId");
        if (!epid)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        int i = 0;
        for (i = cJSON_GetArraySize(ctx->old); i > 0; i--)
        {
            cJSON *old = cJSON_GetArrayItem(ctx->old, i - 1);
            if (!os_strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(old, "endpoint"), "endpointId")->valuestring, epid->valuestring))
                break;
        }
        if (i)
            cJSON_DeleteItemFromArray(ctx->old, i - 1);
        ctx->state = STATE_TYPE_SCENE_UPDATE_APPLY;
    }
    else if (STATE_TYPE_SCENE_UPDATE_DONE == ctx->state)
    {
        cJSON_DeleteItemFromObject(ctx->scenes, "actions");
        while (ctx->old && ctx->old->child)
        {
            cJSON *old = cJSON_DetachItemFromArray(ctx->old, 0);
            cJSON_AddItemToArray(ctx->final, old);
        }
        cJSON_Delete(ctx->apply);
        ctx->apply = 0;
        cJSON_Delete(ctx->old);
        ctx->old = 0;

        if (ctx->final->child)
            cJSON_AddItemToObject(ctx->scenes, "actions", ctx->final);
        else
            cJSON_Delete(ctx->final);
        ctx->final = 0;

        kv_list_add("scenes", ctx->id);

        char key[64] = {0};
        sprintf(key, "telink_scene_%s", ctx->id);
        kv_set(key, &(ctx->scene), 1);
       
        {
            char *str = cJSON_PrintUnformatted(ctx->scenes);
            if (kv_set(ctx->id, str, os_strlen(str)) < 0)
                SigmaLogError(0, 0, "save scene %s failed.", ctx->id);
            os_free(str);
        }

        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Scene"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_ADD_OR_UPDATE_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(packet, "header", header);
        cJSON_AddItemToObject(packet, "scene", ctx->scenes);
        ctx->scenes = 0;

        {
            char *str = cJSON_PrintUnformatted(packet);
            sll_report(seq, str, os_strlen(str), FLAG_LINK_SEND_LANWORK | FLAG_LINK_SEND_MQTT | FLAG_LINK_PACKET_EVENT);
            os_free(str);
            cJSON_Delete(packet);
        }

        return 1;
    }
    else if (STATE_TYPE_SCENE_UPDATE_APPLY == ctx->state)
    {
        cJSON *item = cJSON_GetArrayItem(ctx->apply, 0);
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *ep = cJSON_GetObjectItem(item, "endpoint");
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *epid = cJSON_GetObjectItem(ep, "endpointId");
        if (!epid)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        ep = sld_load(epid->valuestring);
        if (!ep)
        {
            SigmaLogError(0, 0, "device %s not found.", epid->valuestring);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        do
        {
            cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
            if (!attrs)
            {
                SigmaLogError(0, 0, "device %s additionalAttributes not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
            if (!addr)
            {
                SigmaLogError(0, 0, "device %s additionalAttributes.addr not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            int ret = telink_mesh_group_add(addr->valueint, 0x8000 + ctx->scene);
            if (ret > 0)
                ctx->state = STATE_TYPE_SCENE_UPDATE_APPLY_PROPERTIES;
            if (ret < 0)
            {
                SigmaLogError(0, 0, "telink_mesh_group_add(dst:%04x, group:%04x).", addr->valueint, 0x8000 + ctx->scene);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            }
        }
        while (0);
        cJSON_Delete(ep);
    }
    else if (STATE_TYPE_SCENE_UPDATE_APPLY_PROPERTIES == ctx->state)
    {
        cJSON *item = cJSON_GetArrayItem(ctx->apply, 0);
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *ep = cJSON_GetObjectItem(item, "endpoint");
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *epid = cJSON_GetObjectItem(ep, "endpointId");
        if (!epid)
        {
            cJSON_DeleteItemFromArray(ctx->apply, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        ep = sld_load(epid->valuestring);
        if (!ep)
        {
            SigmaLogError(0, 0, "device %s not found.", epid->valuestring);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        do
        {
            cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
            if (!attrs)
            {
                SigmaLogError(0, 0, "device %s additionalAttributes not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
            if (!addr)
            {
                SigmaLogError(0, 0, "device %s additionalAttributes.addr not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            uint8_t onoff = 0, luminance = 0, rgb[3] = {0}, ctw = 0;
            uint16_t duration = 0;
            {
                cJSON *v = sld_property_get(epid->valuestring, "BrightnessController", "brightness");
                if (v)
                {
                    luminance = v->valueint;
                    cJSON_Delete(v);
                }
            }
            {
                cJSON *v = sld_property_get(epid->valuestring, "ColorController", "rgb");
                if (v)
                {
                    cJSON *r = cJSON_GetObjectItem(v, "red");
                    if (r)
                        rgb[0] = r->valueint;
                    cJSON *g = cJSON_GetObjectItem(v, "green");
                    if (g)
                        rgb[1] = g->valueint;
                    cJSON *b = cJSON_GetObjectItem(v, "blue");
                    if (b)
                        rgb[2] = b->valueint;
                    cJSON_Delete(v);
                }
            }
            cJSON *property = cJSON_GetObjectItem(item, "properties");
            if (!property)
            {
                SigmaLogError(0, 0, "device %s scene.actions.properties not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            property = property->child;
            while (property)
            {
                cJSON *type = cJSON_GetObjectItem(property, "type");
                cJSON *p = cJSON_GetObjectItem(property, "property");
                if (!os_strcmp(type->valuestring, "BrightnessController") && !os_strcmp(p->valuestring, "brightness"))
                {
                    cJSON *value = cJSON_GetObjectItem(property, "value");
                    luminance = value->valueint;
                }
                else if (!os_strcmp(type->valuestring, "ColorController") && !os_strcmp(p->valuestring, "rgb"))
                {
                    cJSON *value = cJSON_GetObjectItem(property, "value");
                    rgb[0] = cJSON_GetObjectItem(value, "red")->valueint;
                    rgb[1] = cJSON_GetObjectItem(value, "green")->valueint;
                    rgb[2] = cJSON_GetObjectItem(value, "blue")->valueint;
                }
                else if (!os_strcmp(type->valuestring, "WhiteController") && !os_strcmp(p->valuestring, "white"))
                {
                    cJSON *value = cJSON_GetObjectItem(property, "value");
                    ctw = value->valueint;
                }
                else if (!os_strcmp(type->valuestring, "ColorTemperatureController") && !os_strcmp(p->valuestring, "percentage"))
                {
                    cJSON *value = cJSON_GetObjectItem(property, "value");
                    ctw = value->valueint;
                }
                else if (!os_strcmp(type->valuestring, "PowerController") && !os_strcmp(p->valuestring, "powerState"))
                {
                    cJSON *value = cJSON_GetObjectItem(property, "value");
                    onoff = !os_strcmp(value->valuestring, "ON");
                }
                else if (!os_strcmp(type->valuestring, "Duration") && !os_strcmp(p->valuestring, "seconds"))
                {
                    cJSON *value = cJSON_GetObjectItem(property, "value");
                    duration = value->valueint;
                }
                property = property->next;
            }
            if (!onoff)
                luminance = 0;
            int ret = telink_mesh_scene_add(addr->valueint, ctx->scene, luminance, rgb, ctw, duration);
            if (ret > 0)
            {
                cJSON *apply = cJSON_DetachItemFromArray(ctx->apply, 0);
                cJSON_AddItemToArray(ctx->final, apply);
                
                ctx->state = STATE_TYPE_SCENE_UPDATE_INIT;
            }
            if (ret < 0)
            {
                SigmaLogError(0, 0, "telink_mesh_scene_add(dst:%04x, scene:%d, group:%04x, r:%d g:%d b:%d).", 
                    addr->valueint, 
                    ctx->scene, 
                    luminance, 
                    rgb[0], rgb[1], rgb[2]);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            }
        }
        while(0);
        cJSON_Delete(ep);
    }
    else if (STATE_TYPE_SCENE_UPDATE_KICKOUT == ctx->state)
    {
        cJSON *item = cJSON_GetArrayItem(ctx->old, 0);
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->old, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *ep = cJSON_GetObjectItem(item, "endpoint");
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->old, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *epid = cJSON_GetObjectItem(ep, "endpointId");
        if (!epid)
        {
            cJSON_DeleteItemFromArray(ctx->old, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        ep = sld_load(epid->valuestring);
        if (!ep)
        {
            SigmaLogError(0, 0, "device %s not found.", epid->valuestring);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        do
        {
            cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
            if (!attrs)
            {
                SigmaLogError(0, 0, "device %s additionalAttributes not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
            if (!addr)
            {
                SigmaLogError(0, 0, "device %s additionalAttributes.addr not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            int ret = telink_mesh_scene_delete(addr->valueint, ctx->scene);
            if (ret > 0)
            {
                ctx->state = STATE_TYPE_SCENE_UPDATE_KICKOUT_GROUP;
            }
            if (ret < 0)
            {
                SigmaLogError(0, 0, "telink_mesh_scene_delete(dst:%04x, scene:%d).", 
                    addr->valueint, 
                    ctx->scene);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            }
        }
        while(0);
        cJSON_Delete(ep);
    }
    else if (STATE_TYPE_SCENE_UPDATE_KICKOUT_GROUP == ctx->state)
    {
        cJSON *item = cJSON_GetArrayItem(ctx->old, 0);
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->old, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *ep = cJSON_GetObjectItem(item, "endpoint");
        if (!item)
        {
            cJSON_DeleteItemFromArray(ctx->old, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        cJSON *epid = cJSON_GetObjectItem(ep, "endpointId");
        if (!epid)
        {
            cJSON_DeleteItemFromArray(ctx->old, 0);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        ep = sld_load(epid->valuestring);
        if (!ep)
        {
            SigmaLogError(0, 0, "device %s not found.", epid->valuestring);
            ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            return 0;
        }
        do
        {
            cJSON *attrs = cJSON_GetObjectItem(ep, "additionalAttributes");
            if (!attrs)
            {
                SigmaLogError(0, 0, "device %s additionalAttributes not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            cJSON *addr = cJSON_GetObjectItem(attrs, "addr");
            if (!addr)
            {
                SigmaLogError(0, 0, "device %s additionalAttributes.addr not found.", epid->valuestring);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
                break;
            }
            int ret = telink_mesh_group_delete(addr->valueint, 0x8000 + ctx->scene);
            if (ret > 0)
            {
                cJSON_DeleteItemFromArray(ctx->old, 0);
                ctx->state = STATE_TYPE_SCENE_UPDATE_INIT;
            }
            if (ret < 0)
            {
                SigmaLogError(0, 0, "telink_mesh_group_delete(dst:%04x, group:%04x).", 
                    addr->valueint, 
                    0x8000 + ctx->scene);
                ctx->state = STATE_TYPE_SCENE_UPDATE_DONE;
            }
        }
        while(0);
        cJSON_Delete(ep);
    }
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
    if (!ns || os_strcmp(ns->valuestring, "Scene"))
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

        if (!cJSON_GetObjectItem(scene, "actions"))
        {
            kv_list_add("scenes", id->valuestring);

            char *v = kv_acquire(id->valuestring, 0);
            cJSON *old = cJSON_Parse(v);
            kv_free(v);
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
        }
        else do
        {
            uint8_t sid = 0;
            char key[64] = {0};
            sprintf(key, "telink_scene_%s", id->valuestring);
            if (kv_get(key, key, 1) > 0)
            {
                sid = key[0];
            }
            else
            {
                uint16_t map = 0;
                const char *id = 0;
                void *it = 0;
                while ((id = kv_list_iterator("scenes", &it)))
                {
                    sprintf(key, "telink_scene_%s", id);
                    if (kv_get(key, key, 1) > 0)
                    {
                        int scene = key[0];
                        if (scene < 16)
                            map |= 1ul << scene;
                    }
                }
                kv_list_iterator_release(it);

                for (sid = 1; sid < 16; sid++)
                {
                    if (!(map & (1 << sid)))
                        break;
                }
                if (sid >= 16)
                    break;
            }

            ContextSceneUpdate *ctx = 0;
            SigmaMission *mission = 0;
            SigmaMissionIterator it = {0};
            while ((mission = sigma_mission_iterator(&it)))
            {
                if (MISSION_TYPE_SCENE_UPDATE != mission->type)
                    continue;

                ctx = sigma_mission_extends(mission);
                if (ctx->scene != sid)
                    continue;

                break;
            }
            if (mission)
            {
                SigmaLogError(0, 0, "scene %s is busy.", id->valuestring);
                break;
            }

            mission = sigma_mission_create(0, MISSION_TYPE_SCENE_UPDATE, mission_scene_update, sizeof(ContextSceneUpdate) + os_strlen(id->valuestring) + 1);
            if (!mission)
            {
                SigmaLogError(0, 0, "out of memory");
                break;
            }
            ctx = sigma_mission_extends(mission);
            ctx->state = STATE_TYPE_SCENE_UPDATE_INIT;
            os_strcpy(ctx->id, id->valuestring);
            ctx->scene = sid;
            ctx->scenes = cJSON_Duplicate(scene, 1);
            ctx->apply = cJSON_Duplicate(cJSON_GetObjectItem(scene, "actions"), 1);
            ctx->final = cJSON_CreateArray();

            char *v = kv_acquire(id->valuestring, 0);
            if (v)
            {
                cJSON *old = cJSON_Parse(v);
                if (old)
                {
                    ctx->old = cJSON_Duplicate(cJSON_GetObjectItem(old, "actions"), 1);
                    cJSON_Delete(old);
                }
                kv_free(v);
            }
            return;
        } while (0);

        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Scene"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_ADD_OR_UPDATE_REPORT));
        cJSON_AddItemToObject(header, "version", cJSON_CreateString(PROTOCOL_VERSION));
        cJSON_AddItemToObject(header, "messageIndex", cJSON_CreateNumber(seq));
        cJSON_AddItemToObject(packet, "header", header);
        cJSON_AddItemToObject(packet, "scene", cJSON_Duplicate(scene, 1));

        char *str = cJSON_PrintUnformatted(packet);
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
            if (MISSION_TYPE_DISCOVER_SCENES != mission->type)
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
        mission = sigma_mission_create(0, MISSION_TYPE_DISCOVER_SCENES, mission_discover_scenes, sizeof(ContextDiscoverScenes) + os_strlen(user->valuestring) + 1);
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
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Scene"));
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
    else if (!os_strcmp(name->valuestring, OPCODE_RECALL))
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
        uint8_t sid;
        char key[64] = {0};
        sprintf(key, "telink_scene_%s", id);
        if (kv_get(key, &sid, 1) > 0)
        {
            SigmaMission *mission = 0;
            SigmaMissionIterator it = {0};
            while ((mission = sigma_mission_iterator(&it)))
            {
                if (MISSION_TYPE_SCENE_RECALL != mission->type)
                    continue;

                uint8_t *ctx = sigma_mission_extends(mission);
                if (*ctx != sid)
                    continue;

                break;
            }
            if (!mission)
            {
                mission = sigma_mission_create(0, MISSION_TYPE_SCENE_RECALL, mission_scene_recall, sizeof(uint8_t));
                if (!mission)
                {
                    SigmaLogError(0, 0, "out of memory");
                    return;
                }
                *(uint8_t *)sigma_mission_extends(mission) = sid;
            }
        }

        uint8_t seq = sll_seq();
        cJSON *packet = cJSON_CreateObject();
        cJSON *header = cJSON_CreateObject();
        cJSON_AddItemToObject(header, "method", cJSON_CreateString("Event"));
        cJSON_AddItemToObject(header, "namespace", cJSON_CreateString("Scene"));
        cJSON_AddItemToObject(header, "name", cJSON_CreateString(OPCODE_RECALL_REPORT));
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
