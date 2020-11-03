#include "driver_telink_mesh.h"
#include "interface_usart.h"
#include "sigma_mission.h"

#define TELINK_MESH_OPCODE_ALL_ON 0xd0
#define TELINK_MESH_OPCODE_LUMINANCE 0xd2
#define TELINK_MESH_OPCODE_COLOR 0xe2
#define TELINK_MESH_OPCODE_DEVICE_ADDR 0xe0
#define TELINK_MESH_OPCODE_DEVICE_ADDR_RESPONSE 0xe1
#define TELINK_MESH_OPCODE_STATUS_ALL 0xda
#define TELINK_MESH_OPCODE_STATUS_RESPONSE 0xdb
#define TELINK_MESH_OPCODE_KICKOUT 0xe3
#define TELINK_MESH_OPCODE_TIME_SET 0xe4
#define TELINK_MESH_OPCODE_TIME_GET 0xe8
#define TELINK_MESH_OPCODE_TIME_RESPONSE 0xe9
#define TELINK_MESH_OPCODE_ALARM 0xe5
#define TELINK_MESH_OPCODE_ALARM_GET 0xe6
#define TELINK_MESH_OPCODE_ALARM_RESPONSE 0xe7
#define TELINK_MESH_OPCODE_GROUP 0xd7
#define TELINK_MESH_OPCODE_GROUP_GET 0xdd
#define TELINK_MESH_OPCODE_GROUP_RESPONSE_8 0xd4
#define TELINK_MESH_OPCODE_GROUP_RESPONSE_F4 0xd5
#define TELINK_MESH_OPCODE_GROUP_RESPONSE_B4 0xd6
#define TELINK_MESH_OPCODE_SCENE 0xee
#define TELINK_MESH_OPCODE_SCENE_LOAD 0xef
#define TELINK_MESH_OPCODE_USER_ALL 0xea
#define TELINK_MESH_OPCODE_USER_RESPONSE 0xeb

#define ATT_OP_ERROR_RSP                    0x01 //!< Error Response op code
#define ATT_OP_EXCHANGE_MTU_REQ             0x02 //!< Exchange MTU Request op code
#define ATT_OP_EXCHANGE_MTU_RSP             0x03 //!< Exchange MTU Response op code
#define ATT_OP_FIND_INFO_REQ                0x04 //!< Find Information Request op code
#define ATT_OP_FIND_INFO_RSP                0x05 //!< Find Information Response op code
#define ATT_OP_FIND_BY_TYPE_VALUE_REQ       0x06 //!< Find By Type Vaue Request op code
#define ATT_OP_FIND_BY_TYPE_VALUE_RSP       0x07 //!< Find By Type Vaue Response op code
#define ATT_OP_READ_BY_TYPE_REQ             0x08 //!< Read By Type Request op code
#define ATT_OP_READ_BY_TYPE_RSP             0x09 //!< Read By Type Response op code
#define ATT_OP_READ_REQ                     0x0a //!< Read Request op code
#define ATT_OP_READ_RSP                     0x0b //!< Read Response op code
#define ATT_OP_READ_BLOB_REQ                0x0c //!< Read Blob Request op code
#define ATT_OP_READ_BLOB_RSP                0x0d //!< Read Blob Response op code
#define ATT_OP_READ_MULTI_REQ               0x0e //!< Read Multiple Request op code
#define ATT_OP_READ_MULTI_RSP               0x0f //!< Read Multiple Response op code
#define ATT_OP_READ_BY_GROUP_TYPE_REQ       0x10 //!< Read By Group Type Request op code
#define ATT_OP_READ_BY_GROUP_TYPE_RSP       0x11 //!< Read By Group Type Response op code
#define ATT_OP_WRITE_REQ                    0x12 //!< Write Request op code
#define ATT_OP_WRITE_RSP                    0x13 //!< Write Response op code
#define ATT_OP_PREPARE_WRITE_REQ            0x16 //!< Prepare Write Request op code
#define ATT_OP_PREPARE_WRITE_RSP            0x17 //!< Prepare Write Response op code
#define ATT_OP_EXECUTE_WRITE_REQ            0x18 //!< Execute Write Request op code
#define ATT_OP_EXECUTE_WRITE_RSP            0x19 //!< Execute Write Response op code
#define ATT_OP_HANDLE_VALUE_NOTI            0x1b //!< Handle Value Notification op code
#define ATT_OP_HANDLE_VALUE_IND             0x1d //!< Handle Value Indication op code
#define ATT_OP_HANDLE_VALUE_CFM             0x1e //!< Handle Value Confirmation op code
#define ATT_OP_WRITE_CMD                    0x52 //!< ATT Write Command

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
    STATE_TLM_REQUEST,
    STATE_TLM_WAIT,
    STATE_TLM_OVER,

    STATE_TLMSM_GET_NAME,
    STATE_TLMSM_GET_PASSWORD,
    STATE_TLMSM_GET_LTK,
    STATE_TLMSM_TAKE_EFFECT,

    STATE_TLMGM_GET_NAME,
    STATE_TLMGM_GET_PASSWORD,
    STATE_TLMGM_GET_LTK,

};

typedef struct
{
    uint16_t l2capLen;
    uint16_t chanId;
    uint8_t opcode;
}__attribute_((packed)) TelinkBLEMeshHeader;

typedef struct
{
    uint16_t l2capLen;
    uint16_t chanId;
    uint8_t opcode;
    uint8_t handle[2];
}__attribute_((packed)) TelinkBLEMeshNotify;

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

typedef struct
{
    void (*rsp)(int ret);

    uint32_t timer;
    uint8_t state;
    uint16_t dst;
    uint32_t seq;
    uint8_t onoff;
    uint16_t delay;
}SigmaMissionTLMLightOnoff;

typedef struct
{
    void (*rsp)(int ret);

    uint32_t timer;
    uint8_t state;
    uint16_t dst;
    uint32_t seq;
    uint8_t luminance;
}SigmaMissionTLMLightLuminance;

typedef struct
{
    void (*rsp)(int ret);

    uint32_t timer;
    uint8_t state;
    uint16_t dst;
    uint32_t seq;
    uint8_t color;
    uint8_t value;
}SigmaMissionTLMLightMonocolor;

typedef struct
{
    void (*rsp)(int ret);

    uint32_t timer;
    uint8_t state;
    uint16_t dst;
    uint32_t seq;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
}SigmaMissionTLMLightRGBColor;

typedef struct
{
    void (*rsp)(int ret);

    uint32_t timer;
    uint8_t state;
    uint16_t dst;
    uint32_t seq;
    uint8_t ct;
}SigmaMissionTLMLightCTColor;

typedef struct
{
    uint32_t timer;
    uint8_t state;
    uint8_t size;
    uint8_t packet[];
}SigmaMissionTLMBLEPacket;

typedef struct
{
    uint8_t seq[3];
    uint8_t src[2];
    uint8_t dst[2];
    uint8_t opcode;
    uint8_t vendor[2];
    uint8_t payload[];
}__attribute__((packed)) TelinkMeshPacket;

static uint8_t _buffer[1024] = {0};
static uint16_t _size = 0;
static uint8_t _state = 0;
static uint32_t _sequence = 0;
static TelinkMeshListener _listener = 0;
static uint8_t *_write_rsp = 0;

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
        if (ATT_OP_WRITE_RSP == header->opcode)
        {
            if (_write_rsp && *_write_rsp == STATE_TLM_WAIT)
                *_write_rsp = STATE_TLM_OVER;
            _write_rsp = 0;
        }
        else if (ATT_OP_HANDLE_VALUE_NOTI == header->opcode)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)header;

            TelinkMeshPacket *packet = (TelinkMeshPacket *)(notify + 1);
            if (packet->vendor[0] != 0x11 || packet->vendor[1] != 0x01)
            {
                SigmaLogError("vendor error.(vendor:%02x%02x)", packet->vendor[0], packet->vendor[1]);
                return;
            }
            if (packet->opcode == TELINK_MESH_OPCODE_DEVICE_ADDR_RESPONSE)
            {
                _listener(TELINK_EVENT_DEVICE_ADDR, packet->payload, 2);
            }
            else if (packet->opcode == TELINK_MESH_OPCODE_GROUP_RESPONSE_8)
            {
                TelinkMeshEventGroup *e = os_malloc(sizeof(TelinkMeshEventGroup) + sizeof(uint16_t) * 8);
                if (!e)
                {
                    SigmaLogError("out of memory");
                    return;
                }
                e->addr = *(uint16_t *)packet->src;
                e->count = 0;
                int i = 0;
                while (i < 8)
                {
                    if (packet->payload[i] != 0xff)
                        e->groups[e->count++] = 0x8000 + packet->payload[i];
                    i++;
                }
                _listener(TELINK_EVENT_DEVICE_GROUP, e, sizeof(TelinkMeshEventGroup) + sizeof(uint16_t) * e->count);
                os_free(e);
            }
            else if (packet->opcode == TELINK_MESH_OPCODE_GROUP_RESPONSE_F4)
            {
                TelinkMeshEventGroup *e = os_malloc(sizeof(TelinkMeshEventGroup) + sizeof(uint16_t) * 4);
                if (!e)
                {
                    SigmaLogError("out of memory");
                    return;
                }
                e->addr = *(uint16_t *)packet->src;
                e->count = 0;
                int i = 0;
                while (i < 4)
                {
                    if (*(uint16_t *)&(packet->payload[2 * i]) != 0xffff)
                        e->groups[e->count++] = *(uint16_t *)&(packet->payload[2 * i]);
                    i++;
                }
                _listener(TELINK_EVENT_DEVICE_GROUP, e, sizeof(TelinkMeshEventGroup) + sizeof(uint16_t) * e->count);
                os_free(e);
            }
            else if (packet->opcode == TELINK_MESH_OPCODE_GROUP_RESPONSE_B4)
            {
                TelinkMeshEventGroup *e = os_malloc(sizeof(TelinkMeshEventGroup) + sizeof(uint16_t) * 4);
                if (!e)
                {
                    SigmaLogError("out of memory");
                    return;
                }
                e->addr = *(uint16_t *)packet->src;
                e->count = 0;
                int i = 0;
                while (i < 4)
                {
                    if (*(uint16_t *)&(packet->payload[2 * i]) != 0xffff)
                        e->groups[e->count++] = *(uint16_t *)&(packet->payload[2 * i]);
                    i++;
                }
                _listener(TELINK_EVENT_DEVICE_GROUP, e, sizeof(TelinkMeshEventGroup) + sizeof(uint16_t) * e->count);
                os_free(e);
            }
            else if (packet->opcode == TELINK_MESH_OPCODE_STATUS_RESPONSE)
            {
                TelinkMeshEventStatus *e = os_malloc(sizeof(TelinkMeshEventStatus) + sizeof(uint8_t) * 6);
                if (!e)
                {
                    SigmaLogError("out of memory");
                    return;
                }
                e->addr = *(uint16_t *)packet->src;
                e->ttc = packet->payload[8];
                e->hops = packet->payload[9];
                os_memcpy(e->values, packet->payload, 6);
                _listener(TELINK_EVENT_DEVICE_STATUS, e, sizeof(TelinkMeshEventStatus) + sizeof(uint16_t) * e->count);
                os_free(e);
            }
            else if (packet->opcode == TELINK_MESH_OPCODE_USER_RESPONSE)
            {
                TelinkMeshEventExtends *e = os_malloc(sizeof(TelinkMeshEventExtends) + sizeof(uint8_t) * (size - sizeof(TelinkBLEMeshNotify) - sizeof(TelinkMeshPacket) - 1));
                if (!e)
                {
                    SigmaLogError("out of memory");
                    return;
                }
                e->addr = *(uint16_t *)packet->src;
                e->size = size - sizeof(TelinkBLEMeshNotify) - sizeof(TelinkMeshPacket) - 1;
                os_memcpy(e->values, packet->payload + 1, e->size);
                _listener(TELINK_EVENT_EXTENDS, e, sizeof(TelinkMeshEventExtends) + e->size);
                os_free(e);
            }
            else if (packet->opcode == TELINK_MESH_OPCODE_TIME_RESPONSE)
            {
                TelinkMeshEventTime *e = os_malloc(sizeof(TelinkMeshEventTime));
                if (!e)
                {
                    SigmaLogError("out of memory");
                    return;
                }
                e->addr = *(uint16_t *)packet->src;
                e->year = *(uint16_t *)packet->payload;
                e->month = packet->payload[2];
                e->day = packet->payload[3];
                e->hour = packet->payload[4];
                e->minute = packet->payload[5];
                e->second = packet->payload[6];
                _listener(TELINK_EVENT_DEVICE_TIME, e, sizeof(TelinkMeshEventExtends) + e->size);
                os_free(e);
            }
            else if (packet->opcode == TELINK_MESH_OPCODE_ALARM_RESPONSE)
            {
                TelinkMeshEventTime *e = os_malloc(sizeof(TelinkMeshEventTime));
                if (!e)
                {
                    SigmaLogError("out of memory");
                    return;
                }
                e->addr = *(uint16_t *)packet->src;
                e->year = *(uint16_t *)packet->payload;
                e->month = packet->payload[2];
                e->day = packet->payload[3];
                e->hour = packet->payload[4];
                e->minute = packet->payload[5];
                e->second = packet->payload[6];
                _listener(TELINK_EVENT_DEVICE_TIME, e, sizeof(TelinkMeshEventExtends) + e->size);
                os_free(e);
            }
        }
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

int mission_light_onoff(SigmaMission *mission)
{
    SigmaMissionTLMLightOnoff *ctx = (SigmaMissionTLMLightOnoff *)sigma_mission_extends(mission);

    if (STATE_TLM_REQUEST == ctx->state)
    {
        if (_write_rsp)
            return;

        uint8_t cmd[sizeof(TelinkMeshPacket) + 3] = {0};

        TelinkMeshPacket *p = (TelinkMeshPacket *)cmd;
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = ctx->dst;
        p->dst[1] = ctx->dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_ALL_ON;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = ctx->onoff;
        p->payload[1] = ctx->delay >> 0;
        p->payload[2] = ctx->delay >> 8;

        usart_write(0, cmd, sizeof(TelinkMeshPacket) + 3);
        _write_rsp = &(ctx->state);
        ctx->state = STATE_TLM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLM_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            _write_rsp = 0;
            if (ctx->rsp)
                ctx->rsp(-1);
            return 1;
        }
    }
    else if (STATE_TLM_OVER == ctx->state)
    {
        if (ctx->rsp)
            ctx->rsp(0);
        return 1;
    }
    return 0;
}

int telink_mesh_light_onoff(uint16_t dst, uint8_t onoff, uint16_t delay, void (*rsp)(int ret))
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_TELINK_LIGHT_ONOFF != mission->type)
            continue;
        SigmaMissionTLMLightOnoff *ctx = sigma_mission_extends(mission);
        if (ctx->dst != dst)
            continue;
        break;
    }
    if (!mission)
        mission = sigma_mission_create(0, MISSION_TYPE_TELINK_LIGHT_ONOFF, mission_light_onoff, sizeof(SigmaMissionTLMLightOnoff));
    if (!mission)
    {
        SigmaLogError("out of memory");
        if (rsp)
            rsp(-1);
        return -1;
    }
    SigmaMissionTLMLightOnoff *ctx = sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->dst = dst;
    ctx->onoff = onoff;
    ctx->delay = delay;
    ctx->rsp = rsp;
    return 0;
}

int mission_light_luminance(SigmaMission *mission)
{
    SigmaMissionTLMLightLuminance *ctx = (SigmaMissionTLMLightLuminance *)sigma_mission_extends(mission);

    if (STATE_TLM_REQUEST == ctx->state)
    {
        if (_write_rsp)
            return;

        uint8_t cmd[sizeof(TelinkMeshPacket) + 1] = {0};

        TelinkMeshPacket *p = (TelinkMeshPacket *)cmd;
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = ctx->dst;
        p->dst[1] = ctx->dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_LUMINANCE;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = ctx->luminance;

        usart_write(0, cmd, sizeof(TelinkMeshPacket) + 1);
        _write_rsp = &(ctx->state);
        ctx->state = STATE_TLM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLM_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            _write_rsp = 0;
            if (ctx->rsp)
                ctx->rsp(-1);
            return 1;
        }
    }
    else if (STATE_TLM_OVER == ctx->state)
    {
        if (ctx->rsp)
            ctx->rsp(-1);
        return 1;
    }
    return 0;
}

int telink_mesh_light_luminance(uint16_t dst, uint8_t luminance)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_TELINK_LIGHT_LUMINANCE != mission->type)
            continue;
        SigmaMissionTLMLightLuminance *ctx = sigma_mission_extends(mission);
        if (ctx->dst != dst)
            continue;
        break;
    }
    if (!mission)
        mission = sigma_mission_create(0, MISSION_TYPE_TELINK_LIGHT_LUMINANCE, mission_light_luminance, sizeof(SigmaMissionTLMLightLuminance));
    if (!mission)
    {
        SigmaLogError("out of memory");
        if (rsp)
            rsp(-1);
        return -1;
    }
    SigmaMissionTLMLightLuminance *ctx = sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->dst = dst;
    ctx->luminance = luminance;
    ctx->rsp = rsp;
    return 0;
}

int telink_mesh_music_start(uint16_t dst)
{
    return telink_mesh_light_luminance(dst, 0xfe);
}

int telink_mesh_music_stop(uint16_t dst)
{
    return telink_mesh_light_luminance(dst, 0xff);
}

int mission_light_monocolor(SigmaMission *mission)
{
    SigmaMissionTLMLightMonocolor *ctx = (SigmaMissionTLMLightMonocolor *)sigma_mission_extends(mission);

    if (STATE_TLM_REQUEST == ctx->state)
    {
        if (_write_rsp)
            return;

        uint8_t cmd[sizeof(TelinkMeshPacket) + 2] = {0};

        TelinkMeshPacket *p = (TelinkMeshPacket *)cmd;
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = ctx->dst;
        p->dst[1] = ctx->dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_COLOR;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = ctx->color;
        p->payload[1] = ctx->value;

        usart_write(0, cmd, sizeof(TelinkMeshPacket) + 2);
        _write_rsp = &(ctx->state);
        ctx->state = STATE_TLM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLM_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            _write_rsp = 0;
            if (ctx->rsp)
                ctx->rsp(-1);
            return 1;
        }
    }
    else if (STATE_TLM_OVER == ctx->state)
    {
        if (ctx->rsp)
            ctx->rsp(-1);
        return 1;
    }
    return 0;
}

int telink_mesh_light_monocolor(uint16_t dst, uint8_t color, uint8_t value)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_TELINK_LIGHT_MONOCOLOR != mission->type)
            continue;
        SigmaMissionTLMLightMonocolor *ctx = sigma_mission_extends(mission);
        if (ctx->dst != dst)
            continue;
        if (ctx->color != color)
            continue;
        break;
    }
    if (!mission)
        mission = sigma_mission_create(0, MISSION_TYPE_TELINK_LIGHT_MONOCOLOR, mission_light_monocolor, sizeof(SigmaMissionTLMLightMonocolor));
    if (!mission)
    {
        SigmaLogError("out of memory");
        if (rsp)
            rsp(-1);
        return -1;
    }
    SigmaMissionTLMLightMonocolor *ctx = sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->dst = dst;
    ctx->color = color;
    ctx->value = value;
    ctx->rsp = rsp;
    return 0;
}

int mission_light_rgbcolor(SigmaMission *mission)
{
    SigmaMissionTLMLightRGBColor *ctx = (SigmaMissionTLMLightRGBColor *)sigma_mission_extends(mission);

    if (STATE_TLM_REQUEST == ctx->state)
    {
        if (_write_rsp)
            return;

        uint8_t cmd[sizeof(TelinkMeshPacket) + 4] = {0};

        TelinkMeshPacket *p = (TelinkMeshPacket *)cmd;
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = ctx->dst;
        p->dst[1] = ctx->dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_COLOR;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x04;
        p->payload[1] = ctx->red;
        p->payload[2] = ctx->green;
        p->payload[3] = ctx->blue;

        usart_write(0, cmd, sizeof(TelinkMeshPacket) + 4);
        _write_rsp = &(ctx->state);
        ctx->state = STATE_TLM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLM_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            _write_rsp = 0;
            if (ctx->rsp)
                ctx->rsp(-1);
            return 1;
        }
    }
    else if (STATE_TLM_OVER == ctx->state)
    {
        if (ctx->rsp)
            ctx->rsp(-1);
        return 1;
    }
    return 0;
}

int telink_mesh_light_color(uint16_t dst, uint8_t r, uint8_t g, uint8_t b)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_TELINK_LIGHT_RGBCOLOR != mission->type)
            continue;
        SigmaMissionTLMLightRGBColor *ctx = sigma_mission_extends(mission);
        if (ctx->dst != dst)
            continue;
        break;
    }
    if (!mission)
        mission = sigma_mission_create(0, MISSION_TYPE_TELINK_LIGHT_RGBCOLOR, mission_light_rgbcolor, sizeof(SigmaMissionTLMLightRGBColor));
    if (!mission)
    {
        SigmaLogError("out of memory");
        if (rsp)
            rsp(-1);
        return -1;
    }
    SigmaMissionTLMLightRGBColor *ctx = sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->dst = dst;
    ctx->red = r;
    ctx->green = g;
    ctx->blue = b;
    return 0;
}

int mission_light_ctcolor(SigmaMission *mission)
{
    SigmaMissionTLMLightCTColor *ctx = (SigmaMissionTLMLightCTColor *)sigma_mission_extends(mission);

    if (STATE_TLM_REQUEST == ctx->state)
    {
        if (_write_rsp)
            return;

        uint8_t cmd[sizeof(TelinkMeshPacket) + 2] = {0};

        TelinkMeshPacket *p = (TelinkMeshPacket *)cmd;
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = ctx->dst;
        p->dst[1] = ctx->dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_COLOR;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x05;
        p->payload[1] = ctx->ct;

        usart_write(0, cmd, sizeof(TelinkMeshPacket) + 2);
        _write_rsp = &(ctx->state);
        ctx->state = STATE_TLM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLM_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            _write_rsp = 0;
            if (ctx->rsp)
                ctx->rsp(-1);
            return 1;
        }
    }
    else if (STATE_TLM_OVER == ctx->state)
    {
        if (ctx->rsp)
            ctx->rsp(-1);
        return 1;
    }
    return 0;
}

int telink_mesh_light_ctcolor(uint16_t dst, uint8_t percentage)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_TELINK_LIGHT_CTCOLOR != mission->type)
            continue;
        SigmaMissionTLMLightCTColor *ctx = sigma_mission_extends(mission);
        if (ctx->dst != dst)
            continue;
        break;
    }
    if (!mission)
        mission = sigma_mission_create(0, MISSION_TYPE_TELINK_LIGHT_CTCOLOR, mission_light_ctcolor, sizeof(SigmaMissionTLMLightCTColor));
    if (!mission)
    {
        SigmaLogError("out of memory");
        if (rsp)
            rsp(-1);
        return -1;
    }
    SigmaMissionTLMLightCTColor *ctx = sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->dst = dst;
    ctx->ct = ct;
    return 0;
}

int mission_ble_packet(SigmaMission *mission)
{
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);

    if (STATE_TLM_REQUEST == ctx->state)
    {
        usart_write(0, ctx->packet, sizeof(TelinkMeshPacket) + ctx->size);
        ctx->state = STATE_TLM_WAIT;
        ctx->timer = os_ticks();
    }
    else if (STATE_TLM_WAIT == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(1000))
        {
            SigmaLogError("timeout");
            return 1;
        }
    }
    else if (STATE_TLM_OVER == ctx->state)
    {
        return 1;
    }
    return 0;
}

int telink_mesh_device_addr(uint16_t dst, uint16_t new_addr)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 2);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 2;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_DEVICE_ADDR;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = new_addr >> 0;
    p->payload[1] = new_addr >> 8;

    return 0;
}

int telink_mesh_device_discover(uint16_t dst)
{
    return telink_mesh_device_addr(dst, 0xffff);
}

int telink_mesh_device_status(uint16_t dst, uint8_t relays)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 1);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 1;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_STATUS_ALL;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x10;

    return 0;
}

int telink_mesh_device_group(uint16_t dst, uint8_t relays, uint8_t type)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 2);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 2;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_GROUP_GET;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = relays;
    p->payload[1] = type;
    return 0;
}

int telink_mesh_device_scene(uint16_t dst, uint8_t relays)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 1);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 1;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_SCENE_GET;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = relays;
    return 0;
}

int telink_mesh_device_blink(uint16_t dst, uint8_t times)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 1);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 1;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_SW_CONFIG;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = times;

    return 0;
}

int telink_mesh_device_kickout(uint16_t dst)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 1);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 1;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_KICKOUT;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x01;

    return 0;
}

int telink_mesh_time_set(uint16_t dst, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 7);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 7;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_KICKOUT;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = year >> 0;
    p->payload[1] = year >> 8;
    p->payload[2] = month;
    p->payload[3] = day;
    p->payload[4] = hour;
    p->payload[5] = minute;
    p->payload[6] = second;

    return 0;
}

int telink_mesh_time_get(uint16_t dst, uint8_t relays)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 1);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 1;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_TIME_GET;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = relays;

    return 0;
}

int telink_mesh_alarm_add_device(uint16_t dst, uint8_t idx, uint8_t onoff, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 9);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 9;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_ALARM;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x00;
    p->payload[1] = idx;
    p->payload[2] = onoff | ((!month) << 4) | 0x80;
    p->payload[3] = month;
    p->payload[4] = day;
    p->payload[5] = hour;
    p->payload[6] = minute;
    p->payload[7] = second;
    p->payload[8] = 0;
    return 0;
}

int telink_mesh_alarm_add_scene(uint16_t dst, uint8_t idx, uint8_t scene, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 9);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 9;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_ALARM;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x00;
    p->payload[1] = idx;
    p->payload[2] = 2 | ((!month) << 4) | 0x80;
    p->payload[3] = month;
    p->payload[4] = day;
    p->payload[5] = hour;
    p->payload[6] = minute;
    p->payload[7] = second;
    p->payload[8] = scene;
    return 0;
}

int telink_mesh_alarm_modify_device(uint16_t dst, uint8_t idx, uint8_t onoff, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 9);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 9;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_ALARM;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x02;
    p->payload[1] = idx;
    p->payload[2] = onoff | ((!month) << 4) | 0x80;
    p->payload[3] = month;
    p->payload[4] = day;
    p->payload[5] = hour;
    p->payload[6] = minute;
    p->payload[7] = second;
    p->payload[8] = 0;
    return 0;
}

int telink_mesh_alarm_modify_scene(uint16_t dst, uint8_t idx, uint8_t scene, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 9);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 9;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_ALARM;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x02;
    p->payload[1] = idx;
    p->payload[2] = 2 | ((!month) << 4) | 0x80;
    p->payload[3] = month;
    p->payload[4] = day;
    p->payload[5] = hour;
    p->payload[6] = minute;
    p->payload[7] = second;
    p->payload[8] = scene;
    return 0;
}

int telink_mesh_alarm_delete(uint16_t dst, uint8_t idx)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 8);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 8;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_ALARM;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x01;
    p->payload[1] = idx;
    return 0;
}

int telink_mesh_alarm_run(uint16_t dst, uint8_t idx, uint8_t enable)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 8);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 8;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_ALARM;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = enable ? 0x03 : 0x04;
    p->payload[1] = idx;
    return 0;
}

int telink_mesh_alarm_get(uint16_t dst)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 2);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 2;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_ALARM_GET;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = relays;
    p->payload[1] = 0x00;//chenjing
    return 0;
}

int telink_mesh_group_add(uint16_t dst, uint16_t group)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 3);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 3;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_GROUP;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x01;
    p->payload[1] = group >> 0;
    p->payload[2] = group >> 8;
    return 0;
}

int telink_mesh_group_delete(uint16_t group)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 3);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 3;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_GROUP;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x00;
    p->payload[1] = group >> 0;
    p->payload[2] = group >> 8;
    return 0;
}

int telink_mesh_scene_add(uint8_t dst, uint8_t scene, uint8_t luminance, uint8_t r, uint8_t g, uint8_t b)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 6);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 6;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_SCENE;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x01;
    p->payload[1] = scene
    p->payload[2] = luminance;
    p->payload[3] = r;
    p->payload[4] = g;
    p->payload[5] = b;
    return 0;
}

int telink_mesh_scene_delete(uint16_t dst, uint8_t scene)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 2);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 2;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_SCENE;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = 0x00;
    p->payload[1] = scene
    return 0;
}

int telink_mesh_scene_load(uint16_t dst, uint8_t scene)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 1);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 1;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_SCENE_LOAD;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = scene;
    return 0;
}

int telink_mesh_extend_write(uint16_t dst, uint8_t relays, uint8_t *buffer, uint8_t size)
{
    SigmaMissionIterator it = {0};
    SigmaMission *mission = 0;
    while ((mission = sigma_mission_iterator(&it)) && (MISSION_TYPE_TELINK_MESH_BLE_PACKET != mission->type));
    mission = sigma_mission_create(mission, MISSION_TYPE_TELINK_MESH_BLE_PACKET, mission_ble_packet, sizeof(SigmaMissionTLMBLEPacket) + sizeof(TelinkMeshPacket) + 1 + size);
    if (!mission)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    SigmaMissionTLMBLEPacket *ctx = (SigmaMissionTLMBLEPacket *)sigma_mission_extends(mission);
    ctx->state = STATE_TLM_REQUEST;
    ctx->size = 1 + size;
    TelinkMeshPacket *p = (TelinkMeshPacket *)ctx->packet;
    p->seq[0] = _sequence >> 0;
    p->seq[1] = _sequence >> 8;
    p->seq[2] = _sequence >> 16;
    _sequence++;
    p->src[0] = 0;
    p->src[1] = 0;
    p->dst[0] = dst >> 0;
    p->dst[1] = dst >> 8;
    p->opcode = TELINK_MESH_OPCODE_USER_ALL;
    p->vendor[0] = 0x11;
    p->vendor[1] = 0x02;
    p->payload[0] = relays;
    os_memcpy(p->payload + 1, buffer, size);
    return 0;
}
