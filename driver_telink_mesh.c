#include "driver_telink_mesh.h"
#include "interface_usart.h"
#include "sigma_mission.h"

#define MASTER_CMD_ADD_DEVICE 0x24
#define MASTER_CMD_SET_GW_MESH_NAME 0x26
#define MASTER_CMD_SET_GW_MESH_PASSWORD 0x26
#define MASTER_CMD_SET_GW_MESH_LTK 0x28
#define MASTER_CMD_TAKE_EFFECT 0x29
#define MASTER_CMD_GET_NETWORK_INFO 0x2b
#define SLAVE_CMD_ACK 0x25
#define SLAVE_NETWORK_INFO_ACK 0x2C
#define GATEWAY_EVENT_MESH 0x81
#define GATEWAY_EVENT_NEW_DEVICE_FOUND 0x82
#define GATEWAY_EVENT_PROVISION_COMPLETE 0x83
#define GATEWAY_EVENT_PROVISION_BY_OTHERS 0x84

enum {
    STATE_TLM_USART_OPEN = 0,
    STATE_TLM_USART_READ,
};

enum
{
    STATE_TLMAD_REQUEST,
    STATE_TLMAD_WAIT,
    STATE_TLMAD_OVER,
    
    STATE_TLMSM_GET_NAME,
    STATE_TLMSM_GET_PASSWORD,
    STATE_TLMSM_GET_LTK,
    STATE_TLMSM_TAKE_EFFECT,
    STATE_TLMSM_WAIT,
    STATE_TLMSM_OVER,

    STATE_TLMGM_GET_NAME,
    STATE_TLMGM_GET_PASSWORD,
    STATE_TLMGM_GET_LTK,
    STATE_TLMGM_WAIT,
    STATE_TLMGM_OVER,
};

typedef struct
{
    uint32_t timer;
    uint8_t state;
    uint8_t period;
    uint8_t after;
    void (*rsp)(int ret);
}SigmaMissionTLMAddDevice;

typedef struct
{
    uint32_t timer;
    uint8_t state;
    uint8_t effect;
    char name[17];
    char password[17];
    char ltk[16];
    void (*rsp)(int ret);
}SigmaMissionTLMSetMesh;

typedef struct
{
    uint32_t timer;
    uint8_t state;
    char name[17];
    char password[17];
    char ltk[16];
    void (*rsp)(int ret, const char *name, const char *password, const uint8_t *ltk);
}SigmaMissionTLMGetMesh;

static uint8_t _buffer[1024] = {0};
static uint16_t _size = 0;
static uint8_t _state = 0;
static TelinkMeshListener _listener = 0;

static void usart_packet(uint8_t cmd, uint8_t *parameters, uint16_t size)
{
    if (SLAVE_CMD_ACK == cmd)
    {
        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_TELINK_ADD_DEVICE == mission->type && MASTER_CMD_ADD_DEVICE == parameters[0])
                break;
            if (MISSION_TYPE_TELINK_SET_MESH == mission->type && MASTER_CMD_SET_GW_MESH_NAME == parameters[0])
                break;
            if (MISSION_TYPE_TELINK_SET_MESH == mission->type && MASTER_CMD_SET_GW_MESH_PASSWORD == parameters[0])
                break;
            if (MISSION_TYPE_TELINK_SET_MESH == mission->type && MASTER_CMD_SET_GW_MESH_LTK == parameters[0])
                break;
        }
        if (!mission)
        {
            SigmaLogError("mission not found.(cmd:%u)", cmd);
            return;
        }
        if (MASTER_CMD_ADD_DEVICE == parameters[0])
        {
            SigmaMissionTLMAddDevice *ctx = sigma_mission_extends(mission);
            if (!parameters[1])
            {
                ctx->state = STATE_TLMAD_WAIT;
            }
            else
            {
                ctx->state = STATE_TLMAD_OVER;
                if (ctx->rsp)
                    ctx->rsp(-parameters[1]);
            }
        }
        else if (MASTER_CMD_SET_GW_MESH_NAME == parameters[0])
        {
            SigmaMissionTLMSetMesh *ctx = sigma_mission_extends(mission);
            ctx->state = STATE_TLMSM_GET_PASSWORD;
            if (parameters[1])
            {
                ctx->state = STATE_TLMSM_OVER;
                if (ctx->rsp)
                    ctx->rsp(-1);
                SigmaLogError("MISSION_TYPE_TELINK_GET_MESH_NAME failed.(ret:%d)", -parameters[1]);
            }
        }
        else if (MASTER_CMD_SET_GW_MESH_PASSWORD == parameters[0])
        {
            SigmaMissionTLMSetMesh *ctx = sigma_mission_extends(mission);
            ctx->state = STATE_TLMSM_GET_LTK;
            if (parameters[1])
            {
                ctx->state = STATE_TLMSM_OVER;
                if (ctx->rsp)
                    ctx->rsp(-1);
                SigmaLogError("MISSION_TYPE_TELINK_GET_MESH_PASSWORD failed.(ret:%d)", -parameters[1]);
            }
        }
        else if (MASTER_CMD_SET_GW_MESH_LTK == parameters[0])
        {
            SigmaMissionTLMSetMesh *ctx = sigma_mission_extends(mission);
            ctx->state = STATE_TLMSM_TAKE_EFFECT;
            if (parameters[1])
            {
                ctx->state = STATE_TLMSM_OVER;
                if (ctx->rsp)
                    ctx->rsp(-1);
                SigmaLogError("MISSION_TYPE_TELINK_GET_MESH_LTK failed.(ret:%d)", -parameters[1]);
            }
        }
        else if (MASTER_CMD_TAKE_EFFECT == parameters[0])
        {
            SigmaMissionTLMSetMesh *ctx = sigma_mission_extends(mission);
            ctx->state = STATE_TLMSM_TAKE_EFFECT;
            ctx->state = STATE_TLMSM_OVER;
            if (ctx->rsp)
                ctx->rsp(-parameters[1]);
            if (parameters[1])
                SigmaLogError("MISSION_TYPE_TELINK_GET_MESH_LTK failed.(ret:%d)", -parameters[1]);
        }
    }
    else if (SLAVE_NETWORK_INFO_ACK == cmd)
    {
        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_TELINK_GET_MESH == mission->type)
                break;
        }
        if (!mission)
        {
            SigmaLogError("mission not found.(cmd:%u)", cmd);
            return;
        }
        SigmaMissionTLMGetMesh *ctx = sigma_mission_extends(mission);
        if (MASTER_CMD_SET_GW_MESH_NAME == parameters[0])
        {
            os_memcpy(ctx->name, &(parameters[1]), size - 3);
            ctx->state = STATE_TLMGM_GET_PASSWORD;
        }
        if (MASTER_CMD_SET_GW_MESH_PASSWORD == parameters[0])
        {
            os_memcpy(ctx->password, &(parameters[1]), size - 3);
            ctx->state = STATE_TLMGM_GET_LTK;
        }
        if (MASTER_CMD_SET_GW_MESH_LTK == parameters[0])
        {
            os_memcpy(ctx->ltk, &(parameters[1]), 16);
            ctx->state = STATE_TLMGM_OVER;
        }
    }
    else if (GATEWAY_EVENT_NEW_DEVICE_FOUND == cmd)
    {
        if (_listener)
            _listener(TELINK_EVENT_NEW_DEVICE_FOUND, parameters[0]);
    }
    else if (GATEWAY_EVENT_MESH == cmd)
    {
        TelinkBLEMeshHeader *header = (TelinkBLEMeshHeader *)parameters;
    }
    else if (GATEWAY_EVENT_PROVISION_COMPLETE == cmd)
    {
        SigmaMissionIterator it = {0};
        SigmaMission *mission = 0;
        while ((mission = sigma_mission_iterator(&it)))
        {
            if (MISSION_TYPE_TELINK_ADD_DEVICE == mission->type)
                break;
        }
        if (!mission)
        {
            SigmaLogError("MISSION_TYPE_TELINK_ADD_DEVICE not found");
            return;
        }
        SigmaMissionTLMAddDevice *ctx = sigma_mission_extends(mission);
        ctx->state = STATE_TLMAD_OVER;
        if (ctx->rsp)
            ctx->rsp(1);
    }
}

void telink_mesh_init(TelinkMeshListener listener)
{
    _listener = listener;

    _size = STATE_TLM_USART_OPEN;

    usart_init();
}

void telink_mesh_update(void)
{
    usart_update();

    if (STATE_TLM_USART_OPEN == _state)
    {
        if (usart_open(0, 115200, 8, 'N', 1) < 0)
        {
            SigmaLogError("usart_open failed.");
            return;
        }
        _state = STATE_TLM_USART_READ;
    }
    if (STATE_TLM_USART_READ == _state)
    {
        int ret = usart_read(0, _buffer + _size, 1024 - _size);
        if (!ret)
            return;
        if (ret < 0)
        {
            SigmaLogError("usart_read failed");
            _state = STATE_TLM_USART_OPEN;
            return;
        }
        _size += ret;

        uint16_t pos = 0;
        while (pos < _size)
        {
            if (_buffer[pos] > 35)
            {
                SigmaLogError("length error.(>35)");
                pos++;
                continue;
            }
            if (_size >= _buffer[pos])
                break;
            if (MASTER_CMD_ADD_DEVICE == _buffer[pos + 1])
            {
                if (4 != _buffer[pos])
                {
                    SigmaLogError("MASTER_CMD_ADD_DEVICE error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_SET_GW_MESH_NAME == _buffer[pos + 1])
            {
                if (_buffer[pos] > 16 + 2)
                {
                    SigmaLogError("MASTER_CMD_SET_GW_MESH_NAME error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_SET_GW_MESH_PASSWORD == _buffer[pos + 1])
            {
                if (_buffer[pos] > 16 + 2)
                {
                    SigmaLogError("MASTER_CMD_SET_GW_MESH_PASSWORD error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_SET_GW_MESH_LTK == _buffer[pos + 1])
            {
                if (_buffer[pos] != 16 + 2)
                {
                    SigmaLogError("MASTER_CMD_SET_GW_MESH_LTK error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_TAKE_EFFECT == _buffer[pos + 1])
            {
                if (_buffer[pos] != 16 + 2)
                {
                    SigmaLogError("MASTER_CMD_TAKE_EFFECT error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_GET_NETWORK_INFO == _buffer[pos + 1])
            {
                if (_buffer[pos] != 1 + 2)
                {
                    SigmaLogError("MASTER_CMD_GET_NETWORK_INFO error");
                    pos++;
                    continue;
                }
            }
            else if (SLAVE_CMD_ACK == _buffer[pos + 1])
            {
                if (_buffer[pos] != 2 + 2)
                {
                    SigmaLogError("SLAVE_CMD_ACK error");
                    pos++;
                    continue;
                }
            }
            else if (SLAVE_NETWORK_INFO_ACK == _buffer[pos + 1])
            {
                if (_buffer[pos] > 1 + 16 + 2)
                {
                    SigmaLogError("SLAVE_NETWORK_INFO_ACK error");
                    pos++;
                    continue;
                }
            }
            else if (GATEWAY_EVENT_MESH == _buffer[pos + 1])
            {
                if (_buffer[pos] > 1 + 16 + 2)
                {
                    SigmaLogError("GATEWAY_EVENT_MESH error");
                    pos++;
                    continue;
                }
            }
            else if (GATEWAY_EVENT_NEW_DEVICE_FOUND == _buffer[pos + 1])
            {
                if (_buffer[pos] > 1 + 2)
                {
                    SigmaLogError("GATEWAY_EVENT_NEW_DEVICE_FOUND error");
                    pos++;
                    continue;
                }
            }
            else if (GATEWAY_EVENT_PROVISION_COMPLETE == _buffer[pos + 1])
            {
                if (_buffer[pos] > 2)
                {
                    SigmaLogError("GATEWAY_EVENT_PROVISION_COMPLETE error");
                    pos++;
                    continue;
                }
            }
            else if (GATEWAY_EVENT_PROVISION_BY_OTHERS == _buffer[pos + 1])
            {
                if (_buffer[pos] > 2)
                {
                    SigmaLogError("GATEWAY_EVENT_PROVISION_BY_OTHERS error");
                    pos++;
                    continue;
                }
            }
            usart_packet(_buffer[pos + 1], &(_buffer[pos + 2]), _buffer[pos] - 2);
            pos += _buffer[pos + 2];
        }
        if (0 < pos && pos < _size)
            os_memcpy(_buffer, &(_buffer[pos], _size - pos));
        _size -= pos;
    }
}

int mission_add_device(SigmaMission *mission)
{
    SigmaMissionTLMAddDevice *ctx = (SigmaMissionTLMAddDevice *)sigma_mission_extends(mission);

    if (STATE_TLMAD_REQUEST == ctx->state)
    {
        uint8_t cmd[3] = {0};
        cmd[0] = MASTER_CMD_ADD_DEVICE;
        cmd[1] = ctx->period;
        cmd[2] = ctx->after;
        usart_write(0, cmd, 3);
        ctx->state = STATE_TLMAD_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLMAD_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            if (ctx->rsp)
                ctx->rsp(-1);
            return 1;
        }
    }
    else if (STATE_TLMAD_OVER == ctx->state)
    {
        return 1;
    }
    return 0;
}

int telink_mesh_add_device(uint8_t period, uint8_t after, void (*rsp)(int ret))
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_TELINK_ADD_DEVICE == mission->type)
            break;
    }
    if (!mission)
        mission = sigma_mission_create(0, MISSION_TYPE_TELINK_ADD_DEVICE, mission_add_device, sizeof(SigmaMissionTLMAddDevice));
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMAddDevice *ctx = (SigmaMissionTLMAddDevice *)sigma_mission_extends(mission);
    ctx->state = STATE_TLMAD_REQUEST;
    ctx->period = period;
    ctx->after = after;
    ctx->retry = 3;
    ctx->rsp = rsp;
    return 0;
}

int mission_set_mesh(SigmaMission *mission)
{
    SigmaMissionTLMSetMesh *ctx = (SigmaMissionTLMSetMesh *)sigma_mission_extends(mission);

    if (STATE_TLMSM_GET_NAME == ctx->state)
    {
        uint8_t cmd[1 + 16] = {0};
        cmd[0] = MASTER_CMD_SET_GW_MESH_NAME;
        os_strcpy(cmd + 1, ctx->name);
        usart_write(0, cmd, 1 + os_strlen(ctx->name));
        ctx->state = STATE_TLMSM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLMSM_GET_PASSWORD == ctx->state)
    {
        uint8_t cmd[1 + 16] = {0};
        cmd[0] = MASTER_CMD_SET_GW_MESH_PASSWORD;
        os_strcpy(cmd + 1, ctx->password);
        usart_write(0, cmd, 1 + os_strlen(ctx->password));
        ctx->state = STATE_TLMSM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLMSM_GET_LTK == ctx->state)
    {
        uint8_t cmd[1 + 16] = {0};
        cmd[0] = MASTER_CMD_SET_GW_MESH_LTK;
        os_memcpy(cmd + 1, ctx->ltk, 16);
        usart_write(0, cmd, 1 + 16);
        ctx->state = STATE_TLMSM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLMSM_TAKE_EFFECT == ctx->state)
    {
        uint8_t cmd[2] = {0};
        cmd[0] = MASTER_CMD_TAKE_EFFECT;
        cmd[1] = ctx->effect;
        usart_write(0, cmd, 2);
        ctx->state = STATE_TLMSM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLMSM_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            if (ctx->rsp)
                ctx->rsp(-1);
            return 1;
        }
    }
    else if (STATE_TLMSM_OVER == ctx->state)
    {
        return 1;
    }
    return 0;
}

int telink_mesh_set(const char *name, const char *password, const uint8_t *ltk, uint8_t effect, void (*rsp)(int ret))
{
    if (os_strlen(name) > 16 || os_strlen(password) > 16)
    {
        SigmaLogError("parameter error(name:%s password:%s)(>16)", name, password);
        if (rsp)
            rsp(-1);
        return -1;
    }
    
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_TELINK_GET_MESH == mission->type)
            break;
    }
    if (!mission)
        mission = sigma_mission_create(0, MISSION_TYPE_TELINK_GET_MESH, mission_set_mesh, sizeof(SigmaMissionTLMSetMesh));
    if (!mission)
    {
        SigmaLogError("out of memory");
        if (rsp)
            rsp(-1);
        return -1;
    }
    SigmaMissionTLMSetMesh *ctx = sigma_mission_extends(mission);
    ctx->state = STATE_TLMSM_GET_NAME;
    ctx->effect = effect;
    os_strcpy(ctx->name, name);
    os_strcpy(ctx->password, password);
    os_memcpy(ctx->ltk, ltk);
    ctx->rsp = rsp;
    return 0;
}

int mission_get_mesh(SigmaMission *mission)
{
    SigmaMissionTLMGetMesh *ctx = (SigmaMissionTLMGetMesh *)sigma_mission_extends(mission);

    if (STATE_TLMGM_GET_NAME == ctx->state)
    {
        uint8_t cmd[2] = {0};
        cmd[0] = MASTER_CMD_GET_NETWORK_INFO;
        cmd[1] = MASTER_CMD_SET_GW_MESH_NAME;
        usart_write(0, cmd, 2);
        ctx->state = STATE_TLMGM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLMGM_GET_PASSWORD == ctx->state)
    {
        uint8_t cmd[2] = {0};
        cmd[0] = MASTER_CMD_GET_NETWORK_INFO;
        cmd[1] = MASTER_CMD_SET_GW_MESH_PASSWORD;
        usart_write(0, cmd, 2);
        ctx->state = STATE_TLMGM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLMGM_GET_LTK == ctx->state)
    {
        uint8_t cmd[2] = {0};
        cmd[0] = MASTER_CMD_GET_NETWORK_INFO;
        cmd[1] = MASTER_CMD_SET_GW_MESH_LTK;
        usart_write(0, cmd, 2);
        ctx->state = STATE_TLMGM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLMGM_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            if (ctx->rsp)
                ctx->rsp(-1, ctx->name, ctx->password, ctx->ltk);
            return 1;
        }
    }
    else if (STATE_TLMGM_OVER == ctx->state)
    {
        if (ctx->rsp)
            ctx->rsp(0, ctx->name, ctx->password, ctx->ltk);
        return 1;
    }
    return 0;
}

int telink_mesh_get(void (*rsp)(int ret, const char *name, const char *password, const uint8_t *ltk))
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_TELINK_GET_MESH == mission->type)
            break;
    }
    if (!mission)
        mission = sigma_mission_create(0, MISSION_TYPE_TELINK_GET_MESH, mission_get_mesh, sizeof(SigmaMissionTLMGetMesh));
    if (!mission)
    {
        SigmaLogError("out of memory");
        if (rsp)
            rsp(-1);
        return -1;
    }
    SigmaMissionTLMGetMesh *ctx = sigma_mission_extends(mission);
    ctx->state = STATE_TLMGM_GET_NAME;
    ctx->rsp = rsp;
    return 0;
}

int telink_mesh_light_onoff(uint16_t dst, uint8_t onoff, uint16_t delay)
{
}

int telink_mesh_light_luminance(uint16_t dst, uint8_t luminance)
{
}

int telink_mesh_music_start(uint16_t dst)
{
    return telink_mesh_light_luminance(dst, 0xfe);
}

int telink_mesh_music_stop(uint16_t dst)
{
    return telink_mesh_light_luminance(dst, 0xff);
}

int telink_mesh_light_monocolor(uint16_t dst, uint8_t color, uint8_t value)
{
}

int telink_mesh_light_color(uint16_t dst, uint8_t r, uint8_t g, uint8_t b)
{
}

int telink_mesh_light_ct(uint16_t dst, uint8_t percentage)
{
}
