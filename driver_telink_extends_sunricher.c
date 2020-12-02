#include "driver_telink_extends_sunricher.h"
#include "driver_telink_mesh.h"
#include "interface_os.h"
#include "sigma_log.h"

typedef struct
{
    uint8_t state;
    uint8_t type;
    uint32_t timer;
}ContextTMES;

enum
{
    TMES_TYPE_DEVICE_TYPE = 0,
    TMES_TYPE_DEVICE_MAC,
    TMES_TYPE_LIGHT_MODE,
    TMES_TYPE_LIGHT_MODE_CUSTOM_LIST,
    TMES_TYPE_LIGHT_MODE_CUSTOM,
    TMES_TYPE_LIGHT_SCENE_LIST,
    TMES_TYPE_LIGHT_SCENE,
    TMES_TYPE_LIGHT_PWM,
    TMES_TYPE_LIGHT_PWM_DEADTIME,
    TMES_TYPE_LIGHT_BRIGHT_MIN,
    TMES_TYPE_LIGHT_BRIGHT_GAMMA,
    TMES_TYPE_LIGHT_ONOFF_DURATION,
    TMES_TYPE_LIGHT_SCHEDULE,
    TMES_TYPE_LIGHT_GROUP_GET_32,
    TMES_TYPE_SWITCH_TYPE,
    TMES_TYPE_GPS,
    TMES_TYPE_LOCAL_TIME_ZONE,
    TMES_TYPE_UART_TX_TYPE,
    TMES_TYPE_SENSOR_GROUP,
};

static ContextTMES *_context = 0;

int tmes_device_type_write(uint16_t dst, SRCategory category)
{
    uint8_t data[3];
    data[0] = 0x00;
    data[1] = 0x01;
    data[2] = category & 0xff;
    return telink_mesh_extend_write(dst, data, 3);
}

typedef struct
{
    uint16_t dst;
}TMESDevice;

int tmes_device_type(uint16_t dst, SRCategory *category, uint8_t *group_count, uint32_t *start)
{
    int ret = 0;
    uint8_t data[8] = {0};
    data[0] = 0x00;
    data[1] = 0x00;
    if (_context && TMES_TYPE_DEVICE_TYPE != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_DEVICE_TYPE;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 1, 8);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    if (category)
        *category = ntohs(*(uint16_t *)&(data[1]));
    if (group_count)
        *group_count = data[3];
    if (start)
        *start = *(uint32_t *)&(data[4]);
    return 1;
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
    int ret = 0;
    uint8_t data[8] = {0};
    data[0] = 0x76;
    if (_context && TMES_TYPE_DEVICE_MAC != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 1);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_DEVICE_MAC;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 1, 8);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    if (category)
        *category = *(uint16_t *)&(data[1]);
    if (mac)
        os_memcpy(mac, &(data[3]), 6);
    return 1;
}

int tmes_light_mode(uint16_t dst, uint8_t *speed, uint8_t *temperature, uint8_t *global, uint8_t *mode, uint8_t *id, uint8_t *internal)
{
    int ret = 0;
    uint8_t data[8] = {0};
    data[0] = 0x01;
    data[1] = 0x00;
    if (_context && TMES_TYPE_LIGHT_MODE != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_MODE;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 2, 8);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *speed = data[2];
    *temperature = data[3] >> 4;
    *global = data[3] & 0x0f;
    *mode = data[4];
    *id = data[5];
    *internal = data[6];
    return 1;
}

int tmes_light_mode_custom_list(uint16_t dst, uint8_t *ids, uint8_t *size)
{
    int ret = 0;
    uint8_t data[6] = {0};
    data[0] = 0x01;
    data[1] = 0x01;
    data[2] = 0x00;
    data[3] = 0x00;
    if (_context && TMES_TYPE_LIGHT_MODE_CUSTOM_LIST != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_MODE_CUSTOM_LIST;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 4, 6);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    (*size) = 0;
    int i = 0;
    for (i = 0; i < 16; i++)
    {
        if (data[4 + i / 8] & (1 << (i % 8)))
            ids[(*size)++] = i;
    }
    return 1;
}

typedef struct
{
    uint32_t map;
    uint16_t dst;
    uint8_t count;
    uint8_t *color;
}TMESLightModeCustom;

int tmes_light_mode_custom(uint16_t dst, uint8_t id, uint8_t *color, uint8_t *count)
{
    int ret = 0;
    uint8_t data[9] = {0};
    data[0] = 0x01;
    data[1] = 0x01;
    data[2] = 0x00;
    data[3] = id;
    if (_context && TMES_TYPE_LIGHT_MODE_CUSTOM != _context->type)
        return 0;
    TMESLightModeCustom *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESLightModeCustom));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_MODE_CUSTOM;
        _context->timer = os_ticks();
        ctx = (TMESLightModeCustom *)(_context + 1);
        ctx->dst = dst;
        ctx->color = 0;
    }
    ctx = (TMESLightModeCustom *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        if (ctx->color)
            os_free(ctx->color);
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 4, 9);
    if (!ret)
        return 0;
    if (!ctx->color)
    {
        ctx->color = os_malloc(3 * data[4]);
        ctx->count = data[4];
        ctx->map = 0;
    }
    if (ctx->count != data[4])
    {
        SigmaLogError(0, 0, "total color is not match");
        if (ctx->color)
            os_free(ctx->color);
        os_free(_context);
        _context = 0;
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

        if (ctx->color)
            os_free(ctx->color);
        os_free(_context);
        _context = 0;
        return 1;
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

int tmes_light_mode_global_write(uint16_t dst, SRTransition transition, uint8_t enable)
{
    uint8_t data[4];
    data[0] = 0x01;
    data[1] = 0x02;
    data[2] = enable;
    data[3] = transition;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_mode_speed_write(uint16_t dst, uint8_t speed)
{
    uint8_t data[3];
    data[0] = 0x01;
    data[1] = 0x03;
    data[2] = speed;
    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_light_mode_temperature_write(uint16_t dst, uint8_t temperature)
{
    uint8_t data[3];
    data[0] = 0x01;
    data[1] = 0x04;
    data[2] = temperature;
    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_light_mode_preinstall_write(uint16_t dst, SRPreinstallLightMode preinstall)
{
    uint8_t data[4];
    data[0] = 0x01;
    data[1] = 0x05;
    data[2] = 0x01;
    data[3] = preinstall;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_mode_custom_run(uint16_t dst, SRTransition transition)
{
    uint8_t data[4];
    data[0] = 0x01;
    data[1] = 0x05;
    data[2] = 0x02;
    data[3] = transition;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_scene_list(uint16_t dst, uint8_t *scenes, uint8_t *size)
{
    int ret = 0;
    uint8_t data[6] = {0};
    data[0] = 0x01;
    data[1] = 0x06;
    data[2] = 0x00;
    data[3] = 0x00;
    if (_context && TMES_TYPE_LIGHT_SCENE_LIST != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_SCENE_LIST;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 4, 6);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    (*size) = 0;
    int i = 0;
    for (i = 0; i < 16; i++)
    {
        if (data[4 + i / 8] & (1 << (i % 8)))
            scenes[(*size)++] = i;
    }
    return 1;
}

int tmes_light_scene(uint16_t dst, uint8_t idx, uint8_t *scene)
{
    int ret = 0;
    uint8_t data[5] = {0};
    data[0] = 0x01;
    data[1] = 0x06;
    data[2] = 0x00;
    data[3] = idx;
    if (_context && TMES_TYPE_LIGHT_SCENE != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_SCENE;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 4, 5);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *scene = data[4];
    return 1;
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

int tmes_light_preinstall_stop(uint16_t dst)
{
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x08;
    data[2] = 0x01;
    data[3] = 0x00;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_custom_stop(uint16_t dst)
{
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x08;
    data[2] = 0x02;
    data[3] = 0x00;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_preinstall_run(uint16_t dst, SRPreinstallLightMode mode, uint8_t global, uint8_t speed, uint8_t temperature)
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
    return telink_mesh_extend_write(dst, data, 8);
}

int tmes_light_custom_run(uint16_t dst, uint8_t mode, SRTransition transition, uint8_t global, uint8_t speed, uint8_t temperature)
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
    return telink_mesh_extend_write(dst, data, 8);
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

int tmes_light_pwm_write(uint16_t dst, uint16_t freq)
{
    uint8_t data[5] = {0};

    data[0] = 0x01;
    data[1] = 0x0a;
    data[2] = 0x01;
    data[3] = freq >> 0;
    data[4] = freq >> 8;

    return telink_mesh_extend_write(dst, data, 5);
}

int tmes_light_pwm(uint16_t dst, uint16_t *freq)
{
    int ret = 0;
    uint8_t data[5] = {0};
    data[0] = 0x01;
    data[1] = 0x0a;
    data[2] = 0x00;
    if (_context && TMES_TYPE_LIGHT_PWM != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_PWM;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 3, 5);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *freq = data[3] + (((uint16_t)data[4]) << 8);
    return 1;
}

int tmes_light_pwm_deadtime_write(uint16_t dst, uint8_t interval)
{
    uint8_t data[4] = {0};

    data[0] = 0x01;
    data[1] = 0x0b;
    data[2] = 0x01;
    data[3] = interval;

    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_pwm_deadtime(uint16_t dst, uint8_t *interval)
{
    int ret = 0;
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x0b;
    data[2] = 0x00;
    if (_context && TMES_TYPE_LIGHT_PWM_DEADTIME != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_PWM_DEADTIME;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 3, 4);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *interval = data[3];
    return 1;
}

int tmes_light_bright_min_write(uint16_t dst, uint8_t percent)
{
    uint8_t data[4] = {0};

    data[0] = 0x01;
    data[1] = 0x0d;
    data[2] = 0x01;
    data[3] = percent;

    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_bright_min(uint16_t dst, uint8_t *percent)
{
    int ret = 0;
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x0d;
    data[2] = 0x00;
    if (_context && TMES_TYPE_LIGHT_BRIGHT_MIN != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_BRIGHT_MIN;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "out of memory");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 3, 4);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *percent = data[3];
    return 1;
}

int tmes_light_bright_gamma_write(uint16_t dst, uint8_t gamma)
{
    uint8_t data[4] = {0};

    data[0] = 0x01;
    data[1] = 0x0e;
    data[2] = 0x01;
    data[3] = gamma;

    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_light_bright_gamma(uint16_t dst, uint8_t *gamma)
{
    int ret = 0;
    uint8_t data[4] = {0};
    data[0] = 0x01;
    data[1] = 0x0e;
    data[2] = 0x00;
    if (_context && TMES_TYPE_LIGHT_BRIGHT_GAMMA != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_BRIGHT_GAMMA;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 3, 4);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *gamma = data[3];
    return 1;
}

int tmes_light_onoff_duration_write(uint16_t dst, uint16_t duration)
{
    uint8_t data[5] = {0};

    data[0] = 0x01;
    data[1] = 0x0f;
    data[2] = 0x01;
    data[3] = duration >> 0;
    data[4] = duration >> 8;

    return telink_mesh_extend_write(dst, data, 5);
}

int tmes_light_onoff_duration(uint16_t dst, uint16_t *duration)
{
    int ret = 0;
    uint8_t data[5] = {0};
    data[0] = 0x01;
    data[1] = 0x0f;
    data[2] = 0x00;
    if (_context && TMES_TYPE_LIGHT_ONOFF_DURATION != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 3);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_ONOFF_DURATION;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 3, 5);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *duration = data[3] + (((uint16_t)data[4]) << 8);
    return 1;
}

int tmes_device_status_error(uint16_t dst, uint8_t *error)
{
    uint8_t data[3] = {0};
    data[0] = 0x02;
    data[1] = 0x01;
    int ret = telink_mesh_extend_sr_read(dst, data, 2, 3);
    if (ret <= 0)
        return ret;
    *error = data[2];
    return 1;
}

int tmes_device_status_running(uint16_t dst, uint8_t *bright, uint8_t *rgb, uint8_t *temperature, uint8_t *tail)
{
    uint8_t data[8] = {0};
    data[0] = 0x02;
    data[1] = 0x06;
    int ret = telink_mesh_extend_sr_read(dst, data, 2, 8);
    if (ret <= 0)
        return ret;
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
        return ret;
    *category = (((uint16_t)data[1]) << 8) + data[2];
    *voltage = (((uint16_t)data[3]) << 8) + data[4];
    return 1;
}

int tmes_switch_type_write(uint16_t dst, SRSwitchType type)
{
    uint8_t data[3] = {0};

    data[0] = 0x07;
    data[1] = 0x01;
    data[2] = type;
    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_switch_type(uint16_t dst, SRSwitchType *type)
{
    int ret = 0;
    uint8_t data[3] = {0};
    data[0] = 0x07;
    data[1] = 0x00;
    if (_context && TMES_TYPE_SWITCH_TYPE != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_SWITCH_TYPE;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 2, 3);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *type = data[2];
    return 1;
}

int tmes_uart_tx_type_write(uint16_t dst, uint8_t type)
{
    uint8_t data[3] = {0};

    data[0] = 0x08;
    data[1] = 0x01;
    data[2] = type;

    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_uart_tx_type(uint16_t dst, uint8_t *type)
{
    int ret = 0;
    uint8_t data[3] = {0};
    data[0] = 0x08;
    data[1] = 0x00;
    if (_context && TMES_TYPE_UART_TX_TYPE != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_UART_TX_TYPE;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 2, 3);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *type = data[2];
    return 1;
}

int tmes_light_schedule_write(uint16_t dst, uint8_t hour, uint8_t minute, uint8_t luminance, uint8_t *rgb, uint8_t ct, uint8_t cten)
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

int tmes_light_schedule_start(uint16_t dst)
{
    uint8_t data[2] = {0};

    data[0] = 0x10;
    data[1] = 0xaa;

    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_light_schedule_stop(uint16_t dst)
{
    uint8_t data[2] = {0};

    data[0] = 0x10;
    data[1] = 0xbb;

    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_light_schedule(uint16_t dst, uint8_t idx, uint8_t *hour, uint8_t *minute, uint8_t *bright, uint8_t *rgb, uint8_t *ct, uint8_t *cten)
{
    int ret = 0;
    uint8_t data[9] = {0};
    data[0] = 0x10;
    data[1] = 0x80 + idx;
    if (_context && TMES_TYPE_LIGHT_SCHEDULE != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_SCHEDULE;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 1, 9);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *hour = data[1];
    *minute = data[2];
    *bright = data[3];
    os_memcpy(rgb, &data[4], 3);
    *ct = data[7];
    *cten = data[8];
    return 1;
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
    int ret = 0;
    uint8_t data[7] = {0};
    data[0] = 0x11;
    data[1] = 0x00;
    if (_context && TMES_TYPE_LIGHT_GROUP_GET_32 != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LIGHT_GROUP_GET_32;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 2, 7);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *enable = data[2];
    *group = data[3] << 0;
    *group |= ((uint32_t)data[4]) << 8;
    *group |= ((uint32_t)data[5]) << 16;
    *group |= ((uint32_t)data[6]) << 24;
    return 1;
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

int tmes_remoter_group_write(uint16_t dst, uint8_t map, uint8_t *mac)
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

int tmes_button_group_write(uint16_t dst, uint8_t map, uint8_t *mac)
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

int tmes_sensor_group_write(uint16_t dst, uint8_t sensor, uint8_t *group)
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

int tmes_sensor_group(uint16_t dst, uint8_t sensor, uint32_t *group)
{
    int ret = 0;
    uint8_t data[8] = {0};
    data[0] = 0x12;
    data[1] = 0x04;
    data[2] = 0x00;
    data[3] = sensor;
    if (_context && TMES_TYPE_SENSOR_GROUP != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 4);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_SENSOR_GROUP;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 2, 8);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *group = data[4] << 0;
    *group |= ((uint32_t)data[5]) << 8;
    *group |= ((uint32_t)data[6]) << 16;
    *group |= ((uint32_t)data[7]) << 24;
    return 1;
}

int tmes_light_button_function_write(uint16_t dst, uint8_t type, uint8_t key, uint8_t press, uint8_t func, uint8_t switch_model)
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

int tmes_linkage_condition_check_write(uint8_t dst, uint8_t id, uint8_t enable, uint8_t condition, uint8_t *parameters, uint8_t size)
{
    static uint16_t addr = 0;
    static uint8_t state = 0;

    int ret = 0;
    if (0 == state)
    {
        if (!addr)
        {
            uint8_t data[9] = {0};
            data[0] = 0x16;
            data[1] = id + (enable ? 0x00 : 0x80);
            data[2] = condition;
            os_memcpy(data + 3, parameters, size < 6 ? size : 6);
            ret = telink_mesh_extend_write(dst, data, 3 + (size < 6 ? size : 6));
            if (ret <= 0)
                return ret;
            state = 1;
            addr = dst;
        }
    }
    else if (addr == dst)
    {
        uint8_t data[9] = {0};
        data[0] = 0x17;
        data[1] = id + (enable ? 0x00 : 0x80);
        os_memcpy(data + 2, parameters, size - 6);
        ret = telink_mesh_extend_write(dst, data, 2 + size - 6);
        if (ret <= 0)
            return ret;
    }
    if (ret)
    {
        if (size < 6 || state)
        {
            addr = 0;
            state = 0;
            return 1;
        }
    }
    return 0;
}

int tmes_gps_write(uint16_t dst, uint32_t longitude, uint32_t latitude)
{
    uint8_t data[9] = {0};
    data[0] = 0x1a;
    data[1] = longitude >> 0;
    data[2] = longitude >> 8;
    data[3] = longitude >> 16;
    data[4] = longitude >> 24;
    data[5] = latitude >> 0;
    data[6] = latitude >> 8;
    data[7] = latitude >> 16;
    data[8] = latitude >> 24;
    return telink_mesh_extend_write(dst, data, 9);
}

int tmes_gps(uint16_t dst, uint32_t *longitude, uint32_t *latitude)
{
    int ret = 0;
    uint8_t data[9] = {0};
    data[0] = 0x1b;
    if (_context && TMES_TYPE_GPS != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 1);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_GPS;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 1, 9);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *longitude = ((uint32_t)data[1]) << 0;
    *longitude |= ((uint32_t)data[2]) << 8;
    *longitude |= ((uint32_t)data[3]) << 16;
    *longitude |= ((uint32_t)data[4]) << 24;

    *latitude = ((uint32_t)data[5]) << 0;
    *latitude |= ((uint32_t)data[6]) << 8;
    *latitude |= ((uint32_t)data[7]) << 16;
    *latitude |= ((uint32_t)data[8]) << 24;
    return 1;
}

int tmes_sun_onoff_write(uint16_t dst, uint8_t sunrise, uint8_t enable, uint8_t onoff, uint16_t duration)
{
    uint8_t data[9] = {0};
    data[0] = sunrise ? 0x1c : 0x1d;
    data[1] = 0x01 | (enable ? 0x00 : 0x80);
    data[2] = onoff;
    data[3] = 0;
    data[4] = 0;
    data[5] = 0;
    data[6] = duration >> 0;
    data[7] = duration >> 8;
    data[8] = 0;
    return telink_mesh_extend_write(dst, data, 9);
}

int tmes_sun_scene_standard_write(uint16_t dst, uint8_t sunrise, uint8_t enable, uint8_t scene)
{
    uint8_t data[3] = {0};
    data[0] = sunrise ? 0x1c : 0x1d;
    data[1] = 0x02 | (enable ? 0x00 : 0x80);
    data[2] = scene;
    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_sun_scene_custom_write(uint16_t dst, uint8_t sunrise, uint8_t enable, uint8_t scene)
{
    uint8_t data[3] = {0};
    data[0] = sunrise ? 0x1c : 0x1d;
    data[1] = 0x03 | (enable ? 0x00 : 0x80);
    data[2] = scene;
    return telink_mesh_extend_write(dst, data, 3);
}

int tmes_sun_color_write(uint16_t dst, uint8_t sunrise, uint8_t enable, uint8_t bright, uint8_t *rgb, uint8_t ct, uint16_t duration)
{
    uint8_t data[9] = {0};
    data[0] = sunrise ? 0x1c : 0x1d;
    data[1] = 0x04 | (enable ? 0x00 : 0x80);
    data[2] = bright;
    os_memcpy(data + 3, rgb, 3);
    data[6] = ct;
    data[7] = duration >> 0;
    data[8] = duration >> 8;
    return telink_mesh_extend_write(dst, data, 9);
}

int tmes_sun(uint16_t dst, uint8_t sunrise, SRSunSetting *setting)
{
    int ret = 0;
    uint8_t data[9] = {0};
    if (_context && TMES_TYPE_SWITCH_TYPE != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        data[0] = sunrise ? 0x1c : 0x1d;
        data[1] = 0x00;
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_SWITCH_TYPE;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    data[0] = 0x1b;
    ret = telink_mesh_extend_sr_read(dst, data, 1, 9);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    setting->type = data[1] & 0x7f;
    setting->enable = data[1] & 0x80;
    if (0x01 == setting->type)
    {
        setting->onoff.value = data[2];
        setting->onoff.duration = data[3] + (((uint16_t)data[4]) << 8);
    }
    else if (0x02 == setting->type || 0x03 == setting->type)
    {
        setting->scene = data[2];
    }
    else
    {
        setting->color.bright = data[2];
        setting->color.rgb[0] = data[3];
        setting->color.rgb[1] = data[4];
        setting->color.rgb[2] = data[5];
        setting->color.ct = data[6];
        setting->color.duration = data[7] + (((uint16_t)data[8]) << 8);
    }
    return 1;
}

int tmes_sun_clear(uint16_t dst, uint8_t sunrise)
{
    uint8_t data[2] = {0};
    data[0] = sunrise ? 0x1c : 0x1d;
    data[1] = 0xc0;
    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_sun_enable(uint16_t dst, uint8_t sunrise)
{
    uint8_t data[2] = {0};
    data[0] = sunrise ? 0x1c : 0x1d;
    data[1] = 0xe0;
    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_sun_disable(uint16_t dst, uint8_t sunrise)
{
    uint8_t data[2] = {0};
    data[0] = sunrise ? 0x1c : 0x1d;
    data[1] = 0xf0;
    return telink_mesh_extend_write(dst, data, 2);
}

int tmes_local_time_zone_write(uint16_t dst, uint8_t east, uint8_t hour, uint8_t minute)
{
    uint8_t data[4] = {0};
    data[0] = 0x1e;
    data[1] = 0x01;
    data[2] = (east ? 0x00 : 0x80) + hour;
    data[3] = minute;
    return telink_mesh_extend_write(dst, data, 4);
}

int tmes_local_time_zone(uint16_t dst, uint8_t *east, uint8_t *now_hour, uint8_t *now_minute, uint8_t *sunrise_hour, uint8_t *sunrise_minute, uint8_t *sunset_hour, uint8_t *sunset_minute)
{
    int ret = 0;
    uint8_t data[9] = {0};
    data[0] = 0x1e;
    data[1] = 0x00;
    if (_context && TMES_TYPE_LOCAL_TIME_ZONE != _context->type)
        return 0;
    TMESDevice *ctx = 0;
    if (!_context)
    {
        ret = telink_mesh_extend_write(dst, data, 2);
        if (ret <= 0)
            return ret;

        _context = os_malloc(sizeof(ContextTMES) + sizeof(TMESDevice));
        if (!_context)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _context->type = TMES_TYPE_LOCAL_TIME_ZONE;
        _context->timer = os_ticks();
        ctx = (TMESDevice *)(_context + 1);
        ctx->dst = dst;
    }
    ctx = (TMESDevice *)(_context + 1);
    if (ctx->dst != dst)
        return 0;
    if (os_ticks_from(_context->timer) > os_ticks_ms(1000))
    {
        SigmaLogError(0, 0, "timeout");
        os_free(_context);
        _context = 0;
        return -1;
    }
    ret = telink_mesh_extend_sr_read(dst, data, 2, 9);
    if (!ret)
        return 0;
    os_free(_context);
    _context = 0;
    if (ret < 0)
        return ret;
    *east = !(data[2] & 0x80);
    *now_hour = data[2] & 0x7f;
    *now_minute = data[3];
    *sunrise_hour = data[5];
    *sunrise_minute = data[6];
    *sunset_hour = data[7];
    *sunset_minute = data[8];
    return 1;
}

int tmes_button_control(uint16_t dst, uint8_t type, uint8_t key, uint32_t id)
{
    uint8_t data[7] = {0};
    data[0] = 0x30;
    data[1] = type;
    data[2] = key;
    data[3] = id >> 24;
    data[4] = id >> 16;
    data[5] = id >> 8;
    data[6] = id >> 0;
    return telink_mesh_extend_write(dst, data, 7);
}
