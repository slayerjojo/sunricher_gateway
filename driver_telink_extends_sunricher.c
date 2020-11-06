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
        ret = telink_mesh_extend_sr_read(dst, data, 1, 8);
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
        ret = telink_mesh_extend_sr_read(dst, data, 1, 8);
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
        ret = telink_mesh_extend_sr_read(dst, data, 2, 8);
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
        ret = telink_mesh_extend_sr_read(dst, data, 4, 6);
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
        ret = telink_mesh_extend_sr_read(dst, data, 4, 9);
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
        ret = telink_mesh_extend_sr_read(dst, data, 4, 6);
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
        ret = telink_mesh_extend_sr_read(dst, data, 4, 5);
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

int tmes_light_channel_enable(uint16_t dst, uint8_t channels, uint8_t mask)
{
    uint8_t data[5] = {0};
    data[0] = 0x01;
    data[1] = 0x07;
    data[2] = 0x01;
    data[3] = channels;
    data[4] = mask;
    return telink_mesh_extend_write(dst, data, 5);
}

int tmes_light_channel_action(uint16_t dst, uint8_t channels, uint8_t action, uint8_t flag)
{
    uint8_t data[6] = {0};
    data[0] = 0x01;
    data[1] = 0x07;
    data[2] = 0x02;
    data[3] = channels;
    data[4] = action;
    data[5] = flag;
    return telink_mesh_extend_write(dst, data, 6);
}

int tmes_light_channel_speed_action(uint16_t dst, uint8_t action)
{
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x07;
    data[2] = 0x03;
    data[3] = action;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_preinstall_stop(void)
{
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x08;
    data[2] = 0x01;
    data[3] = 0x00;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_custom_stop(void)
{
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x08;
    data[2] = 0x02;
    data[3] = 0x00;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_preinstall_run(uint16_t group, SRPreinstallLightMode mode, uint8_t global, uint8_t speed, uint8_t temperature)
{
    uint8_t data[8] = {0};
    data[0] = 0x01;
    data[1] = 0x08;
    data[2] = 0x01;
    data[3] = mode;
    data[4] = 0x00;
    data[5] = global;
    data[6] = speed;
    data[7] = temperature;
    return telink_mesh_extend_write(group, data, 8);
}

int tmes_light_custom_run(uint16_t group, uint8_t mode, SRTransition transition, uint8_t global, uint8_t speed, uint8_t temperature)
{
    uint8_t data[8] = {0};
    data[0] = 0x01;
    data[1] = 0x08;
    data[2] = 0x02;
    data[3] = mode;
    data[4] = transition;
    data[5] = global;
    data[6] = speed;
    data[7] = temperature;
    return telink_mesh_extend_write(group, data, 8);
}

int tmes_light_move_to_level(uint16_t dst, uint8_t bright, uint8_t r, uint8_t g, uint8_t b, uint8_t ct, uint16_t time)
{
    uint8_t data[9] = {0};

    data[0] = 0x01;
    data[1] = 0x09;
    data[2] = bright;
    data[3] = r;
    data[4] = g;
    data[5] = b;
    data[6] = ct;
    data[7] = time >> 0;
    data[8] = time >> 8;

    return telink_mesh_extend_write(dst, data, 9);
}

int tmes_light_pwm_set(uint16_t dst, uint16_t freq)
{
    uint8_t data[5] = {0};

    data[0] = 0x01;
    data[1] = 0x0a;
    data[2] = 0x01;
    data[3] = freq >> 0;
    data[4] = freq >> 8;

    return telink_mesh_extend_write(dst, data, 5);
}

int tmes_light_pwm_get(uint16_t dst, uint16_t *freq)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[3] = {0};
        data[0] = 0x01;
        data[1] = 0x0a;
        data[2] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[5] = {0};
        data[0] = 0x01;
        data[1] = 0x0a;
        data[2] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 3, 5);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *freq = data[3] + (((uint16_t)data[4]) << 8);
        return 1;
    }
    return 0;
}

int tmes_light_pwm_deadtime_set(uint16_t dst, uint8_t interval)
{
    uint8_t data[4] = {0};

    data[0] = 0x01;
    data[1] = 0x0b;
    data[2] = 0x01;
    data[3] = internal;

    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_pwm_deadtime_get(uint16_t dst, uint8_t interval)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[3] = {0};
        data[0] = 0x01;
        data[1] = 0x0b;
        data[2] = 0x01;
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[4] = {0};
        data[0] = 0x01;
        data[1] = 0x0b;
        data[2] = 0x01;
        ret = telink_mesh_extend_sr_read(dst, data, 3, 4);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *internal = data[3];
        return 1;
    }
    return 0;
}

int tmes_light_bright_min_set(uint16_t dst, uint8_t percent)
{
    uint8_t data[4] = {0};

    data[0] = 0x01;
    data[1] = 0x0d;
    data[2] = 0x01;
    data[3] = percent;

    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_bright_min_get(uint16_t dst, uint8_t *percent)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[3] = {0};
        data[0] = 0x01;
        data[1] = 0x0d;
        data[2] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[4] = {0};
        data[0] = 0x01;
        data[1] = 0x0d;
        data[2] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 3, 4);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *percent = data[3];
        return 1;
    }
    return 0;
}

int tmes_light_bright_gamma_set(uint16_t dst, uint8_t gamma)
{
    uint8_t data[4] = {0};

    data[0] = 0x01;
    data[1] = 0x0e;
    data[2] = 0x01;
    data[3] = gamma;

    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_bright_gamma_get(uint16_t dst, uint8_t *gamma)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[3] = {0};
        data[0] = 0x01;
        data[1] = 0x0e;
        data[2] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[4] = {0};
        data[0] = 0x01;
        data[1] = 0x0e;
        data[2] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 3, 4);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *percent = data[3];
        return 1;
    }
    return 0;
}

int tmes_light_onoff_duration_set(uint16_t dst, uint16_t duration)
{
    uint8_t data[5] = {0};

    data[0] = 0x01;
    data[1] = 0x0f;
    data[2] = 0x01;
    data[3] = duration >> 0;
    data[4] = duration >> 8;

    return telink_mesh_extend_write(dst, data, 5);
}

int tmes_light_onoff_duration_get(uint16_t dst, uint16_t *duration)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[3] = {0};
        data[0] = 0x01;
        data[1] = 0x0f;
        data[2] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[5] = {0};
        data[0] = 0x01;
        data[1] = 0x0f;
        data[2] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 3, 5);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *duration = data[3] + (((uint16_t)data[4]) << 8);
        return 1;
    }
    return 0;
}

int tmes_light_status_error(uint16_t dst, uint8_t *error)
{
    uint8_t data[3] = {0};
    data[0] = 0x02;
    data[1] = 0x01;
    int ret = telink_mesh_extend_sr_read(dst, data, 2, 3);
    if (ret <= 0)
    {
        addr = 0;
        return ret;
    }
    *error = data[2];
    return 1;
}

int tmes_light_status_running(uint16_t dst, uint8_t *bright, uint8_t *rgb, uint8_t *temperature, uint8_t *tail)
{
    uint8_t data[8] = {0};
    data[0] = 0x02;
    data[1] = 0x06;
    int ret = telink_mesh_extend_sr_read(dst, data, 2, 8);
    if (ret <= 0)
    {
        addr = 0;
        return ret;
    }
    *bright = data[2];
    os_memcpy(rgb, &data[3], 3);
    *temperature = data[6];
    *tail = data[7];
    return 1;
}

int tmes_light_status_power(uint16_t dst, SRCategory *category, uint16_t *voltage)
{
    uint8_t data[5] = {0};
    data[0] = 0x03;
    int ret = telink_mesh_extend_sr_read(dst, data, 1, 5);
    if (ret <= 0)
    {
        addr = 0;
        return ret;
    }
    *category = (((uint16_t)data[1]) << 8) + data[2];
    *voltage = (((uint16_t)data[3]) << 8) + data[4];
    return 1;
}

int tmes_switch_type_set(uint16_t dst, SRSwitchType type)
{
    uint8_t data[3] = {0};

    data[0] = 0x07;
    data[1] = 0x01;
    data[2] = type;

    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_switch_type_get(uint16_t dst, SRSwitchType *type)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[2] = {0};
        data[0] = 0x07;
        data[1] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[3] = {0};
        data[0] = 0x07;
        data[1] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 2, 3);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *type = data[2];
        return 1;
    }
    return 0;
}

int tmes_uart_tx_type_set(uint16_t dst, uint8_t type)
{
    uint8_t data[3] = {0};

    data[0] = 0x08;
    data[1] = 0x01;
    data[2] = type;

    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_uart_tx_type_get(uint16_t dst, uint8_t *type)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[2] = {0};
        data[0] = 0x08;
        data[1] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[3] = {0};
        data[0] = 0x08;
        data[1] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 2, 3);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *type = data[2];
        return 1;
    }
    return 0;
}

int tmes_light_schedule_set(uint16_t dst, uint8_t hour, uint8_t minute, uint8_t luminance, uint8_t *rgb, uint8_t ct, uint8_t cten)
{
    uint8_t data[9] = {0};

    data[0] = 0x10;
    data[1] = hour;
    data[2] = minute;
    data[3] = luminance;
    os_memcpy(&data[4], rgb, 3);
    data[7] = ct;
    data[8] = cten;

    return telink_mesh_extend_write(dst, data, 9);
}

int tmes_light_schedule_start(void)
{
    uint8_t data[2] = {0};

    data[0] = 0x10;
    data[1] = 0xaa;

    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_light_schedule_stop(void)
{
    uint8_t data[2] = {0};

    data[0] = 0x10;
    data[1] = 0xbb;

    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_light_schedule_get(uint16_t dst, uint8_t idx, uint8_t *hour, uint8_t *minute, uint8_t *bright, uint8_t *rgb, uint8_t *ct, uint8_t *cten)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[2] = {0};
        data[0] = 0x10;
        data[1] = 0x80 + idx;
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[9] = {0};
        data[0] = 0x10;
        ret = telink_mesh_extend_sr_read(dst, data, 1, 9);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *hour = data[1];
        *minute = data[2];
        *bright = data[3];
        os_memcpy(rgb, &data[4], 3);
        *ct = data[7];
        *cten = data[8];
        return 1;
    }
    return 0;
}

int tmes_light_preinstall_run_32(uint32_t group, SRPreinstallLightMode mode, uint8_t global, uint8_t speed, uint8_t temperature)
{
    uint8_t data[8] = {0};
    data[0] = 0x11;
    data[1] = 0x01;
    data[2] = mode;
    data[3] = 0x00;
    data[4] = global;
    data[5] = speed;
    data[6] = temperature;
    data[7] = group >> 0;
    data[8] = group >> 8;

    uint16_t vendor = ((group >> 16) & 0xff) + ((group >> 24) << 8);
    return telink_mesh_extend_sr_write(vendor, 0xffff, data, 8);
}

int tmes_light_custom_run_32(uint32_t group, uint8_t mode, SRTransition transition, uint8_t global, uint8_t speed, uint8_t temperature)
{
    uint8_t data[8] = {0};
    data[0] = 0x11;
    data[1] = 0x02;
    data[2] = mode;
    data[3] = transition;
    data[4] = global;
    data[5] = speed;
    data[6] = temperature;
    data[7] = group >> 0;
    data[8] = group >> 8;

    uint16_t vendor = ((group >> 16) & 0xff) + ((group >> 24) << 8);
    return telink_mesh_extend_sr_write(vendor, 0xffff, data, 8);
}

int tmes_light_group_get_32(uint16_t dst, uint8_t *enable, uint32_t *group)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[2] = {0};
        data[0] = 0x11;
        data[1] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[7] = {0};
        data[0] = 0x11;
        data[1] = 0x00;
        ret = telink_mesh_extend_sr_read(dst, data, 2, 7);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *enable = data[2];
        *group = data[3] << 0;
        *group |= ((uint32_t)data[4]) << 8;
        *group |= ((uint32_t)data[5]) << 16;
        *group |= ((uint32_t)data[6]) << 24;
        return 1;
    }
    return 0;
}

int tmes_device_pair_enable(uint16_t dst)
{
    uint8_t data[2] = {0};
    data[0] = 0x12;
    data[1] = 0x01;
    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_device_return_to_factory(uint16_t dst)
{
    uint8_t data[2] = {0};
    data[0] = 0x12;
    data[1] = 0x07;
    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_remoter_group_set(uint16_t dst, uint8_t map, uint8_t *mac)
{
    uint8_t data[8] = {0};
    data[0] = 0x12;
    data[1] = 0x02;
    data[2] = 0x01;
    data[3] = map;
    os_memcpy(&data[4], mac, 4);
    return telink_mesh_extend_write(dst, data, 8);
}

int tmes_remoter_group_unset(uint16_t dst, uint8_t map, uint8_t *mac)
{
    uint8_t data[8] = {0};
    data[0] = 0x12;
    data[1] = 0x02;
    data[2] = 0x02;
    data[3] = map;
    os_memcpy(&data[4], mac, 4);
    return telink_mesh_extend_write(dst, data, 8);
}

int tmes_button_group_set(uint16_t dst, uint8_t map, uint8_t *mac)
{
    uint8_t data[8] = {0};
    data[0] = 0x12;
    data[1] = 0x03;
    data[2] = 0x01;
    data[3] = map;
    os_memcpy(&data[4], mac, 4);
    return telink_mesh_extend_write(dst, data, 8);
}

int tmes_button_group_unset(uint16_t dst, uint8_t map, uint8_t *mac)
{
    uint8_t data[8] = {0};
    data[0] = 0x12;
    data[1] = 0x03;
    data[2] = 0x02;
    data[3] = map;
    os_memcpy(&data[4], mac, 4);
    return telink_mesh_extend_write(dst, data, 8);
}

int tmes_sensor_group_set(uint16_t dst, uint8_t sensor, uint8_t *group)
{
    uint8_t data[8] = {0};
    data[0] = 0x12;
    data[1] = 0x04;
    data[2] = 0x01;
    data[3] = sensor;
    os_memcpy(&data[4], group, 4);
    return telink_mesh_extend_write(dst, data, 8);
}

int tmes_sensor_group_unset(uint16_t dst, uint8_t sensor)
{
    uint8_t data[4] = {0};
    data[0] = 0x12;
    data[1] = 0x04;
    data[2] = 0x02;
    data[3] = sensor;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_sensor_group_read(uint16_t dst, uint8_t sensor, uint32_t *group)
{
    static uint16_t addr = 0;
    int ret = 0;
    if (!addr)
    {
        uint8_t data[4] = {0};
        data[0] = 0x12;
        data[1] = 0x04;
        data[2] = 0x00;
        data[3] = sensor;
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;
        addr = dst;
    }
    else if (dst == addr)
    {
        uint8_t data[8] = {0};
        data[0] = 0x12;
        data[1] = 0x04;
        data[2] = 0x00;
        data[3] = sensor;
        ret = telink_mesh_extend_sr_read(dst, data, 2, 8);
        if (ret <= 0)
        {
            addr = 0;
            return ret;
        }
        *group = data[4] << 0;
        *group |= ((uint32_t)data[5]) << 8;
        *group |= ((uint32_t)data[6]) << 16;
        *group |= ((uint32_t)data[7]) << 24;
        return 1;
    }
    return 0;
}

int tmes_light_button_function_set(uint8_t type, uint8_t key, uint8_t press, uint8_t func, uint8_t switch_model)
{
    uint8_t data[6] = {0};
    data[0] = 0x13;
    data[1] = type;
    data[2] = key;
    data[3] = press;
    data[4] = func;
    data[5] = switch_model;
    return telink_mesh_extend_write(dst, data, 6);
}
