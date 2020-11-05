#include "driver_telink_extends_sunricher.h"
#include "driver_telink_mesh.h"

typedef struct
{
    uint16_t dst;
    SRCategory category;
}SRDeviceType;

int tmes_device_type_set(uint16_t dst, SRCategory category)
{
    uint8_t data[3];
    data[0] = 0x00;
    data[1] = 0x01;
    data[2] = category & 0xff;
    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_device_type_get(uint16_t dst, SRCategory *category, uint8_t *group_count, uint32_t *start)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[2] = {0};
        data[0] = 0x00;
        data[1] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[8] = {0};
        ret = telink_mesh_extend_sr_read(dst, data, 1);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *category = *(uint16_t *)&(data[1]);
        *group_count = data[3];
        *start = *(uint32_t *)&(data[4]);
        return 1;
    }
    return 0;
}

int tmes_device_type_clear(uint16_t dst)
{
    uint8_t data[2];
    data[0] = 0x00;
    data[1] = 0xff;
    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_device_mac(uint16_t dst, uint16_t *category, uint8_t *mac)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[1] = {0};
        data[0] = 0x76;
        ret = telink_mesh_extend_write(dst, data, 1);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[8] = {0};
        data[0] = 0x76;
        ret = telink_mesh_extend_sr_read(dst, data, 1);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *category = *(uint16_t *)&(data[1]);
        os_memcpy(mac, &(data[3]), 6);
        return 1;
    }
    return 0;
}

int tmes_light_mode_get(uint16_t dst, uint8_t *speed, uint8_t *temperature, uint8_t *global, uint8_t *mode, uint8_t *id, uint8_t *internal)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[2] = {0};
        data[0] = 0x01;
        data[1] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[8] = {0};
        data[0] = 0x01;
        data[1] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 2);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *speed = data[2];
        *temperature = data[3] >> 4;
        *global = data[3] & 0x0f;
        *mode = data[4];
        *id = data[5];
        *internal = data[6];
        return 1;
    }
    return 0;
}

int tmes_light_mode_custom_list(uint16_t dst, uint8_t *ids, uint8_t *size)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[4] = {0};
        data[0] = 0x01;
        data[1] = 0x01;
        data[2] = 0x00;
        data[3] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[6] = {0};
        data[0] = 0x01;
        data[1] = 0x01;
        data[2] = 0x00;
        data[3] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 4);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        (*size) = 0;
        int i = 0;
        for (i = 0; i < 16; i++)
        {
            if (data[4 + i / 8] & (1 << (i % 8)))
                ids[(*size)++] = i;
        }
        return 1;
    }
    return 0;
}

typedef struct
{
    uint32_t map;
    uint8_t count;
    uint8_t color[];
}SRLModeCustomGet;

int tmes_light_mode_custom_get(uint16_t dst, uint8_t id, uint8_t *color, uint8_t *count)
{
    static uint16_t addr = 0;
    static SRLModeCustomGet *ctx = 0;

    int ret = 0;
    if (!addr)
    {
        uint8_t data[4] = {0};
        data[0] = 0x01;
        data[1] = 0x01;
        data[2] = 0x00;
        data[3] = id;
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[9] = {0};
        data[0] = 0x01;
        data[1] = 0x01;
        data[2] = 0x00;
        data[3] = id;
        ret = telink_mesh_extend_sr_read(dst, data, 4);
        if (ret <= 0)
        {
            addr = 0;
            if (ctx)
                os_free(ctx);
            ctx = 0;
            return ret;
        }
        if (!ctx)
        {
            ctx = os_malloc(sizeof(SRLModeCustomGet) + 3 * data[4]);
            if (!ctx)
            {
                SigmaLogError("out of memory");
                addr = 0;
                if (ctx)
                    os_free(ctx);
                ctx = 0;
                return -1;
            }
            ctx->count = data[4];
            ctx->map = 0;
        }
        if (ctx->count != data[4])
        {
            SigmaLogError("total color is not match.");
            addr = 0;
            if (ctx)
                os_free(ctx);
            ctx = 0;
            return -1;
        }
        os_memcpy(&(ctx->color[3 * data[5]]), &data[6], 3);
        ctx->map |= 1 << data[5];
        int i = 0;
        for (i = 0; i < ctx->count; i++)
        {
            if (!(ctx->map & (1 << i)))
                break;
        }
        if (i >= ctx->count)
        {
            *count = ctx->count;
            os_memcpy(color, ctx->color, ctx->count * 3);

            addr = 0;
            if (ctx)
                os_free(ctx);
            ctx = 0;
            return 1;
        }
    }
    return 0;
}

int tmes_light_mode_custom_add(uint16_t dst, uint8_t id, uint8_t idx, uint8_t *color)
{
    uint8_t data[8];
    data[0] = 0x01;
    data[1] = 0x01;
    data[2] = 0x01;
    data[3] = id;
    data[4] = idx;
    os_memcpy(&data[5], color, 3);
    return telink_mesh_extend_write(dst, data, 8);
}

int tmes_light_mode_custom_delete(uint16_t dst, uint8_t id)
{
    uint8_t data[4];
    data[0] = 0x01;
    data[1] = 0x01;
    data[2] = 0x02;
    data[3] = id;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_mode_global_set(uint16_t dst, SRMode mode, uint8_t enable)
{
    uint8_t data[4];
    data[0] = 0x01;
    data[1] = 0x02;
    data[2] = enable;
    data[3] = mode;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_mode_speed_set(uint16_t dst, uint8_t speed)
{
    uint8_t data[3];
    data[0] = 0x01;
    data[1] = 0x03;
    data[2] = speed;
    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_light_mode_temperature_set(uint16_t dst, uint8_t temperature)
{
    uint8_t data[3];
    data[0] = 0x01;
    data[1] = 0x04;
    data[2] = temperature;
    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_light_mode_preinstall_set(uint16_t dst, uint8_t preinstall)
{
    uint8_t data[4];
    data[0] = 0x01;
    data[1] = 0x05;
    data[2] = 0x01;
    data[3] = preinstall;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_mode_custom_run(uint16_t dst, SRMode mode)
{
    uint8_t data[4];
    data[0] = 0x01;
    data[1] = 0x05;
    data[2] = 0x02;
    data[3] = mode;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_scene_list(uint16_t dst, uint8_t *scenes, uint8_t *size)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[4] = {0};
        data[0] = 0x01;
        data[1] = 0x06;
        data[2] = 0x00;
        data[3] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[6] = {0};
        data[0] = 0x01;
        data[1] = 0x06;
        data[2] = 0x00;
        data[3] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 4);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        (*size) = 0;
        int i = 0;
        for (i = 0; i < 16; i++)
        {
            if (data[4 + i / 8] & (1 << (i % 8)))
                scenes[(*size)++] = i;
        }
        return 1;
    }
    return 0;
}

int tmes_light_scene_get(uint16_t dst, uint8_t idx, uint8_t *scene)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[4] = {0};
        data[0] = 0x01;
        data[1] = 0x06;
        data[2] = 0x00;
        data[3] = idx;
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[5] = {0};
        data[0] = 0x01;
        data[1] = 0x06;
        data[2] = 0x00;
        data[3] = idx;
        ret = telink_mesh_extend_sr_read(dst, data, 4);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *scene = data[4];
        return 1;
    }
    return 0;
}

int tmes_light_scene_save(uint16_t dst, uint8_t scene, uint16_t delay)
{
    uint8_t data[6] = {0};
    data[0] = 0x01;
    data[1] = 0x06;
    data[2] = 0x01;
    data[3] = scene;
    data[4] = delay >> 0;
    data[5] = delay >> 8;
    return telink_mesh_extend_write(dst, data, 6);
}

int tmes_light_scene_delete(uint16_t dst, uint8_t scene)
{
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x06;
    data[2] = 0x02;
    data[3] = scene;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_scene_run(uint16_t dst, uint8_t scene)
{
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x06;
    data[2] = 0x03;
    data[3] = scene;
    return telink_mesh_extend_write(dst, data, 4);
}
