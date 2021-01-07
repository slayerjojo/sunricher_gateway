#include "env.h"
#include "sr_layer_link.h"
#include "sr_sublayer_link_mqtt.h"
#include "sr_layer_device.h"
#include "sr_layer_scene.h"
#include "sr_layer_room.h"
#include "interface_os.h"
#include "driver_usart_linux.h"
#include "driver_telink_mesh.h"
#include "driver_telink_extends_sunricher.h"
#include "sigma_console.h"
#include "sigma_event.h"
#include "sigma_mission.h"
#include "command_option.h"
#include "sigma_log.h"
#include "hex.h"

static char _serial[64] = {0};
static SigmaMission *_missions = 0;

static int option_serial(char *option)
{
    os_strcpy(_serial, option);
    return 0;
}

typedef struct
{
    char name[17];
    char password[17];
    uint8_t ltk[16];
    uint8_t effect;
}ContextTelinkMeshSet;

static int mission_telink_mesh_set(SigmaMission *mission, uint8_t cleanup)
{
    ContextTelinkMeshSet *ctx = (ContextTelinkMeshSet *)sigma_mission_extends(mission);

    if (cleanup)
        return 1;

    int ret = telink_mesh_set(ctx->name, ctx->password, ctx->ltk, ctx->effect);
    if (!ret)
        return 0;
    if (_missions == mission)
        _missions = 0;
    if (ret > 0)
    {
        SigmaLogAction(ctx->ltk, 16, "telink_mesh_set successed.effect:%d name:%s password:%s ltk:", ctx->effect, ctx->name, ctx->password);
        return 1;
    }
    else if (ret < 0)
    {
        SigmaLogError(ctx->ltk, 16, "telink_mesh_set failed.effect:%d name:%s password:%s ltk:", ctx->effect, ctx->name, ctx->password);
        return 1;
    }
    return 0;
}

static int mission_telink_mesh_add_start(SigmaMission *mission, uint8_t cleanup)
{
    if (cleanup)
        return 1;

    int ret = telink_mesh_device_add(0xff, 0xff);
    if (!ret)
        return 0;
    if (_missions == mission)
        _missions = 0;
    if (ret > 0)
    {
        SigmaLogAction(0, 0, "telink_mesh_device_add(0xff, 0xff) successed.");
        return 1;
    }
    else if (ret < 0)
    {
        sigma_console_write("telink_mesh_device_add(0xff, 0xff) failed.");
        return 1;
    }
    return 0;
}

static int mission_telink_mesh_add_stop(SigmaMission *mission, uint8_t cleanup)
{
    if (cleanup)
        return 1;

    int ret = telink_mesh_device_add(0, 0);
    if (!ret)
        return 0;
    if (_missions == mission)
        _missions = 0;
    if (ret > 0)
    {
        SigmaLogAction(0, 0, "telink_mesh_device_add(0, 0) successed.");
        return 1;
    }
    else if (ret < 0)
    {
        sigma_console_write("telink_mesh_device_add(0, 0) failed.");
        return 1;
    }
    return 0;
}

static int mission_telink_mesh_discover(SigmaMission *mission, uint8_t cleanup)
{
    if (cleanup)
        return 1;

    int ret = telink_mesh_device_discover();
    if (!ret)
        return 0;
    if (_missions == mission)
        _missions = 0;
    if (ret > 0)
    {
        SigmaLogAction(0, 0, "telink_mesh_device_discover successed.");
        return 1;
    }
    else if (ret < 0)
    {
        sigma_console_write("telink_mesh_device_discover failed.");
        return 1;
    }
    return 0;
}

typedef struct
{
    uint8_t mac[6];
    uint16_t addr;
    uint16_t new;
}ContextTelinkMeshDeviceAddr;

static int mission_telink_mesh_device_addr(SigmaMission *mission, uint8_t cleanup)
{
    ContextTelinkMeshDeviceAddr *ctx = sigma_mission_extends(mission);

    if (cleanup)
        return 1;

    int ret = telink_mesh_device_addr(ctx->addr, ctx->mac, ctx->new);
    if (!ret)
        return 0;
    if (_missions == mission)
        _missions = 0;
    if (ret > 0)
    {
        SigmaLogAction(ctx->mac, 6, "telink_mesh_device_addr successed. addr:%04x new:%04x mac:", ctx->addr, ctx->new);
        return 1;
    }
    else if (ret < 0)
    {
        SigmaLogError(ctx->mac, 6, "telink_mesh_device_addr failed. addr:%04x new:%04x mac:", ctx->addr, ctx->new);
        return 1;
    }
    return 0;
}

static int mission_telink_mesh_device_type(SigmaMission *mission, uint8_t cleanup)
{
    uint16_t *addr = sigma_mission_extends(mission);

    if (cleanup)
        return 1;

    SRCategory category;
    int ret = tmes_device_type(*addr, &category, 0, 0);
    if (!ret)
        return 0;
    if (_missions == mission)
        _missions = 0;
    if (ret > 0)
    {
        SigmaLogAction(0, 0, "tmes_device_type successed. addr:%04x category:%04x", *addr, category);
        return 1;
    }
    else if (ret < 0)
    {
        SigmaLogAction(0, 0, "tmes_device_type failed. addr:%04x", *addr);
        return 1;
    }
    return 0;
}

static int mission_telink_mesh_light_status(SigmaMission *mission, uint8_t cleanup)
{
    if (cleanup)
        return 1;

    int ret = telink_mesh_light_status_request();
    if (!ret)
        return 0;
    if (_missions == mission)
        _missions = 0;
    if (ret > 0)
    {
        SigmaLogAction(0, 0, "telink_mesh_light_status_request successed.");
        return 1;
    }
    else if (ret < 0)
    {
        SigmaLogAction(0, 0, "telink_mesh_light_status_request failed.");
        return 1;
    }
    return 0;
}

static void handle_console(const char *command, const char *parameters[], int count)
{
    if (!os_strcmp(command, "key_press"))
    {
        sigma_console_write("OK.");
        sigma_event_dispatch(EVENT_TYPE_GATEWAY_BIND, 0, 0);
    }
    else if (!os_strcmp(command, "mqtt_pub"))
    {
        sigma_console_write("OK.");
        sslm_send(parameters[0], 0, parameters[1], os_strlen(parameters[1]), FLAG_LINK_PACKET_EVENT);
    }
    else if (!os_strcmp(command, "send_packet"))
    {
        cJSON *msg = cJSON_Parse((const char *)(parameters[0]));
        if (!msg)
        {
            sigma_console_write("JSON parse error.");
            return;
        }
        if (!cJSON_GetObjectItem(msg, "user"))
            cJSON_AddItemToObject(msg, "user", cJSON_CreateString("server"));
        if (!cJSON_GetObjectItem(msg, "fp"))
            cJSON_AddItemToObject(msg, "fp", cJSON_CreateNumber(sigma_console_fp()));

        sigma_console_write("OK.");
        sigma_event_dispatch(EVENT_TYPE_PACKET, msg, 0);
        cJSON_Delete(msg);
    }
    else if (!os_strcmp(command, "mesh_default"))
    {
        _missions = sigma_mission_create(_missions, MISSION_TYPE_CONSOLE, mission_telink_mesh_set, sizeof(ContextTelinkMeshSet));
        if (!_missions)
        {
            sigma_console_write("out of memory");
            return;
        }
        ContextTelinkMeshSet *ctx = sigma_mission_extends(_missions);
        os_strcpy(ctx->name, "Srm@7478@a");
        os_strcpy(ctx->password, "475869");
        const uint8_t ltk[] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};
        os_memcpy(ctx->ltk, ltk, 16);
        ctx->effect = 0;
        if (count >= 1)
            ctx->effect = atoi(parameters[0]);
    }
    else if (!os_strcmp(command, "mesh_family"))
    {
        _missions = sigma_mission_create(_missions, MISSION_TYPE_CONSOLE, mission_telink_mesh_set, sizeof(ContextTelinkMeshSet));
        if (!_missions)
        {
            sigma_console_write("out of memory");
            return;
        }
        ContextTelinkMeshSet *ctx = sigma_mission_extends(_missions);
        os_strcpy(ctx->name, "Srm@7478@b");
        os_strcpy(ctx->password, "475869");
        const uint8_t ltk[] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};
        os_memcpy(ctx->ltk, ltk, 16);
        ctx->effect = 0;
        if (count >= 1)
            ctx->effect = atoi(parameters[0]);
    }
    else if (!os_strcmp(command, "mesh_add_start"))
    {
        _missions = sigma_mission_create(_missions, MISSION_TYPE_CONSOLE, mission_telink_mesh_add_start, 0);
        if (!_missions)
        {
            sigma_console_write("out of memory");
            return;
        }
    }
    else if (!os_strcmp(command, "mesh_add_stop"))
    {
        _missions = sigma_mission_create(_missions, MISSION_TYPE_CONSOLE, mission_telink_mesh_add_stop, 0);
        if (!_missions)
        {
            sigma_console_write("out of memory");
            return;
        }
    }
    else if (!os_strcmp(command, "mesh_discover"))
    {
        _missions = sigma_mission_create(_missions, MISSION_TYPE_CONSOLE, mission_telink_mesh_discover, 0);
        if (!_missions)
        {
            sigma_console_write("out of memory");
            return;
        }
    }
    else if (!os_strcmp(command, "device_addr"))
    {
        _missions = sigma_mission_create(_missions, MISSION_TYPE_CONSOLE, mission_telink_mesh_device_addr, sizeof(ContextTelinkMeshDeviceAddr));
        if (!_missions)
        {
            sigma_console_write("out of memory");
            return;
        }
        ContextTelinkMeshDeviceAddr *ctx = sigma_mission_extends(_missions);
        hex2bin(ctx->mac, parameters[0], 12);
        hex2bin((uint8_t *)&(ctx->addr), parameters[1], 4);
        hex2bin((uint8_t *)&(ctx->new), parameters[2], 4);
    }
    else if (!os_strcmp(command, "device_type"))
    {
        _missions = sigma_mission_create(_missions, MISSION_TYPE_CONSOLE, mission_telink_mesh_device_type, sizeof(uint16_t));
        if (!_missions)
        {
            sigma_console_write("out of memory");
            return;
        }
        uint16_t *ctx = sigma_mission_extends(_missions);
        hex2bin((uint8_t *)ctx, parameters[0], 4);
    }
    else if (!os_strcmp(command, "light_status"))
    {
        _missions = sigma_mission_create(_missions, MISSION_TYPE_CONSOLE, mission_telink_mesh_light_status, 0);
        if (!_missions)
        {
            sigma_console_write("out of memory");
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    srand(time(0));

    register_command_option_argument("serial", option_serial, "serial port.syntax:</dev/tty.usbserial-xxxxx>");
    if (parse_command_option(argc, argv) < 0)
        return -1;

    sigma_console_init(8000, handle_console);

    sigma_mission_init();

    sll_init();
    sld_init();
    slr_init();
    sls_init();

    linux_usart_path(0, _serial);

    while (1)
    {
        sll_update();
        sld_update();
        slr_update();
        sls_update();

        sigma_console_update();
        sigma_mission_update();
    }
    return 0;
}
