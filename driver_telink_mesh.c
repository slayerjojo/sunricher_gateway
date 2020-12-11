#include "driver_telink_mesh.h"
#include "interface_usart.h"
#include "interface_os.h"
#include "sigma_mission.h"
#include "sigma_log.h"
#include "hex.h"

#define TELINK_MESH_OPCODE_ALL_ON 0xd0
#define TELINK_MESH_OPCODE_LUMINANCE 0xd2
#define TELINK_MESH_OPCODE_DEVICE_ADDR 0xe0
#define TELINK_MESH_OPCODE_DEVICE_ADDR_RESPONSE 0xe1
#define TELINK_MESH_OPCODE_COLOR 0xe2
#define TELINK_MESH_OPCODE_KICKOUT 0xe3
#define TELINK_MESH_OPCODE_SW_CONFIG 0xd3
#define TELINK_MESH_OPCODE_STATUS_ALL 0xda
#define TELINK_MESH_OPCODE_STATUS_RESPONSE 0xdb
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
#define TELINK_MESH_OPCODE_SCENE_GET 0xc0
#define TELINK_MESH_OPCODE_SCENE_RESPONSE 0xc1
#define TELINK_MESH_OPCODE_USER_ALL 0xea
#define TELINK_MESH_OPCODE_USER_RESPONSE 0xeb
#define TELINK_MESH_OPCODE_LIGHT_STATUS 0xdc
#define TELINK_MESH_OPCODE_MESH_UPDATE 0xca

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
#define MASTER_CMD_SET_GW_MESH_PASSWORD 0x27
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
    STATE_TLM_RESPONSE,
    STATE_TLM_WAIT,
    STATE_TLM_PROVISION,
    STATE_TLM_DELAY,

    STATE_TLMSM_RESTART,
    STATE_TLMSM_WAIT_RESTART,
    STATE_TLMSM_NAME,
    STATE_TLMSM_WAIT_NAME,
    STATE_TLMSM_PASSWORD,
    STATE_TLMSM_WAIT_PASSWORD,
    STATE_TLMSM_LTK,
    STATE_TLMSM_WAIT_LTK,
    STATE_TLMSM_TAKE_EFFECT,
    STATE_TLMSM_WAIT_TAKE_EFFECT,
    STATE_TLMSM_UPDATE,

    STATE_TLMGM_NAME,
    STATE_TLMGM_WAIT_NAME,
    STATE_TLMGM_PASSWORD,
    STATE_TLMGM_WAIT_PASSWORD,
    STATE_TLMGM_LTK,
    STATE_TLMGM_WAIT_LTK,
};

enum
{
    TELINK_REQUEST_MESH_SET,
    TELINK_REQUEST_MESH_GET,
    TELINK_REQUEST_LIGHT_ONOFF,
    TELINK_REQUEST_LIGHT_LUMINANCE,
    TELINK_REQUEST_LIGHT_COLOR,
    TELINK_REQUEST_LIGHT_COLOR_CHANNEL,
    TELINK_REQUEST_LIGHT_COLOR_CT,
    TELINK_REQUEST_LIGHT_STATUS_REQUEST,
    TELINK_REQUEST_DEVICE_ADD,
    TELINK_REQUEST_DEVICE_ADDR,
    TELINK_REQUEST_DEVICE_DISCOVER,
    TELINK_REQUEST_DEVICE_STATUS,
    TELINK_REQUEST_DEVICE_GROUP,
    TELINK_REQUEST_DEVICE_SCENE,
    TELINK_REQUEST_DEVICE_BLINK,
    TELINK_REQUEST_DEVICE_KICKOUT,
    TELINK_REQUEST_DEVICE_TIME_SET,
    TELINK_REQUEST_DEVICE_TIME_GET,
    TELINK_REQUEST_ALARM_ADD_DEVICE,
    TELINK_REQUEST_ALARM_ADD_SCENE,
    TELINK_REQUEST_ALARM_MODIFY_SCENE,
    TELINK_REQUEST_ALARM_MODIFY_DEVICE,
    TELINK_REQUEST_ALARM_DELETE,
    TELINK_REQUEST_ALARM_RUN,
    TELINK_REQUEST_ALARM_GET,
    TELINK_REQUEST_GROUP_ADD,
    TELINK_REQUEST_GROUP_DELETE,
    TELINK_REQUEST_SCENE_ADD,
    TELINK_REQUEST_SCENE_DELETE,
    TELINK_REQUEST_SCENE_LOAD,
    TELINK_REQUEST_SCENE_GET,
    TELINK_REQUEST_EXTENDS_WRITE,
    TELINK_REQUEST_EXTENDS_SR_WRITE,
};

typedef struct
{
    uint16_t l2capLen;
    uint16_t chanId;
    uint8_t opcode;
}__attribute__((packed)) TelinkBLEMeshHeader;

typedef struct
{
    uint16_t l2capLen;
    uint16_t chanId;
    uint8_t opcode;
    uint8_t handle[2];
}__attribute__((packed)) TelinkBLEMeshNotify;

typedef struct
{
    uint8_t seq[3];
    uint8_t src[2];
    uint8_t dst[2];
    uint8_t opcode;
    uint8_t vendor[2];
    uint8_t payload[];
}__attribute__((packed)) TelinkMeshPacket;

typedef struct
{
    uint8_t type;
    uint8_t retry;
    uint8_t state;
}TelinkRequest;

typedef struct _telink_uart_packet
{
    struct _telink_uart_packet *_next;

    uint32_t timer;
    uint8_t cmd;
    uint8_t size;
    uint8_t parameters[];
}TelinkUartPacket;

static uint8_t _buffer[1024] = {0};
static uint16_t _size = 0;
static uint8_t _state = 0;
static uint32_t _sequence = 0;
static TelinkUartPacket *_packets = 0;

static TelinkRequest *_request = 0;
static uint32_t _timer = 0;

#define CASE_SYMBOLIC_TELINK_COMMAND(cmd)   \
    case cmd:                               \
        return "\033[32m"#cmd"\033[0m"

static const char *symbolic_telink_command(uint8_t cmd)
{
    switch (cmd)
    {
		CASE_SYMBOLIC_TELINK_COMMAND(MASTER_CMD_ADD_DEVICE);
		CASE_SYMBOLIC_TELINK_COMMAND(MASTER_CMD_SET_GW_MESH_NAME);
		CASE_SYMBOLIC_TELINK_COMMAND(MASTER_CMD_SET_GW_MESH_PASSWORD);
		CASE_SYMBOLIC_TELINK_COMMAND(MASTER_CMD_SET_GW_MESH_LTK);
		CASE_SYMBOLIC_TELINK_COMMAND(MASTER_CMD_TAKE_EFFECT);
		CASE_SYMBOLIC_TELINK_COMMAND(MASTER_CMD_GET_NETWORK_INFO);
		CASE_SYMBOLIC_TELINK_COMMAND(SLAVE_CMD_ACK);
		CASE_SYMBOLIC_TELINK_COMMAND(SLAVE_NETWORK_INFO_ACK);
		CASE_SYMBOLIC_TELINK_COMMAND(GATEWAY_EVENT_MESH);
		CASE_SYMBOLIC_TELINK_COMMAND(GATEWAY_EVENT_NEW_DEVICE_FOUND);
		CASE_SYMBOLIC_TELINK_COMMAND(GATEWAY_EVENT_PROVISION_COMPLETE);
		CASE_SYMBOLIC_TELINK_COMMAND(GATEWAY_EVENT_PROVISION_BY_OTHERS);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_ALL_ON);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_LUMINANCE);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_DEVICE_ADDR);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_DEVICE_ADDR_RESPONSE);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_COLOR);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_KICKOUT);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_SW_CONFIG);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_STATUS_ALL);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_STATUS_RESPONSE);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_TIME_SET);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_TIME_GET);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_TIME_RESPONSE);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_ALARM);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_ALARM_GET);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_ALARM_RESPONSE);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_GROUP);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_GROUP_GET);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_GROUP_RESPONSE_8);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_GROUP_RESPONSE_F4);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_GROUP_RESPONSE_B4);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_SCENE);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_SCENE_LOAD);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_SCENE_GET);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_SCENE_RESPONSE);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_USER_ALL);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_USER_RESPONSE);
		CASE_SYMBOLIC_TELINK_COMMAND(TELINK_MESH_OPCODE_LIGHT_STATUS);
    }
    return "UNKOWN";
}

static void usart_packet(uint8_t cmd, uint8_t *parameters, uint16_t size)
{
    if (GATEWAY_EVENT_MESH == cmd)
    {
        TelinkBLEMeshHeader *header = (TelinkBLEMeshHeader *)parameters;
        if (ATT_OP_HANDLE_VALUE_NOTI == header->opcode)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)header;
        
            TelinkMeshPacket *packet = (TelinkMeshPacket *)(notify + 1);
            if (packet->vendor[0] != 0x11 || packet->vendor[1] != 0x02)
            {
                SigmaLogError(parameters, size, "vendor error.(vendor:%02x%02x)", packet->vendor[0], packet->vendor[1]);
                return;
            }
            else
            {
                SigmaLogDebug(notify + 1, size - sizeof(TelinkBLEMeshNotify), 
                    "cmd:%s(%02x) channel:%d opcode:%s(%02x) seq:%02x%02x%02x src:%04x dst:%04x parameters:", 
                    symbolic_telink_command(cmd), cmd, 
                    notify->chanId, 
                    symbolic_telink_command(packet->opcode), packet->opcode,
                    packet->seq[0], packet->seq[1], packet->seq[2],
                    *(uint16_t *)packet->src,
                    *(uint16_t *)packet->dst);
            }
        }
        else if (ATT_OP_WRITE_RSP == header->opcode)
        {
            SigmaLogDebug(parameters, size, "cmd:%s(%02x) parameters:", symbolic_telink_command(cmd), cmd);
        }
        else
        {
            SigmaLogDebug(parameters, size, "not support cmd:%s(%02x) parameters:", symbolic_telink_command(cmd), cmd);
            return;
        }
    }
    else if (SLAVE_CMD_ACK == cmd)
    {
        SigmaLogDebug(parameters + 1, size - 1, "cmd:%s(%02x) opcode:%s(%02x) parameters:", symbolic_telink_command(cmd), cmd, symbolic_telink_command(parameters[0]), parameters[0]);
    }
    else
    {
        SigmaLogDebug(parameters, size, "cmd:%s(%02x) parameters:", symbolic_telink_command(cmd), cmd);
    }
    TelinkUartPacket *packet = os_malloc(sizeof(TelinkUartPacket) + size);
    if (!packet)
    {
        SigmaLogError(0, 0, "out of memory");
        return;
    }
    packet->cmd = cmd;
    packet->timer = os_ticks();
    packet->size = size;
    os_memcpy(packet->parameters, parameters, size);
    packet->_next = 0;

    TelinkUartPacket *p = _packets;
    while(p && p->_next)
        p = p->_next;
    if (p)
        p->_next = packet;
    else
        _packets = packet;
}

void telink_mesh_init(void)
{
    _sequence = os_rand();
    if (!_sequence)
        _sequence = 1;
    _state = STATE_TLM_USART_OPEN;

    usart_init();
}

void telink_mesh_update(void)
{
    usart_update();

    if (STATE_TLM_USART_OPEN == _state)
    {
        if (usart_open(0, 115200, 8, 'N', 1) < 0)
        {
            SigmaLogError(0, 0, "usart_open failed.");
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
            SigmaLogError(0, 0, "usart_read failed");
            _state = STATE_TLM_USART_OPEN;
            return;
        }
        _size += ret;

        uint16_t pos = 0;
        while (pos < _size)
        {
            if (_buffer[pos] > 35)
            {
                SigmaLogError(0, 0, "length error.(>35)");
                pos = 0;
                _size = 0;
                continue;
            }
            if (_size < _buffer[pos])
                break;
            if (MASTER_CMD_ADD_DEVICE == _buffer[pos + 1])
            {
                if (4 != _buffer[pos])
                {
                    SigmaLogError(0, 0, "MASTER_CMD_ADD_DEVICE error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_SET_GW_MESH_NAME == _buffer[pos + 1])
            {
                if (_buffer[pos] > 16 + 2)
                {
                    SigmaLogError(0, 0, "MASTER_CMD_SET_GW_MESH_NAME error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_SET_GW_MESH_PASSWORD == _buffer[pos + 1])
            {
                if (_buffer[pos] > 16 + 2)
                {
                    SigmaLogError(0, 0, "MASTER_CMD_SET_GW_MESH_PASSWORD error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_SET_GW_MESH_LTK == _buffer[pos + 1])
            {
                if (_buffer[pos] != 16 + 2)
                {
                    SigmaLogError(0, 0, "MASTER_CMD_SET_GW_MESH_LTK error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_TAKE_EFFECT == _buffer[pos + 1])
            {
                if (_buffer[pos] != 16 + 2)
                {
                    SigmaLogError(0, 0, "MASTER_CMD_TAKE_EFFECT error");
                    pos++;
                    continue;
                }
            }
            else if (MASTER_CMD_GET_NETWORK_INFO == _buffer[pos + 1])
            {
                if (_buffer[pos] != 1 + 2)
                {
                    SigmaLogError(0, 0, "MASTER_CMD_GET_NETWORK_INFO error");
                    pos++;
                    continue;
                }
            }
            else if (SLAVE_CMD_ACK == _buffer[pos + 1])
            {
                if (_buffer[pos] != 2 + 2)
                {
                    SigmaLogError(0, 0, "SLAVE_CMD_ACK error");
                    pos++;
                    continue;
                }
            }
            else if (SLAVE_NETWORK_INFO_ACK == _buffer[pos + 1])
            {
                if (_buffer[pos] > 1 + 16 + 2)
                {
                    SigmaLogError(0, 0, "SLAVE_NETWORK_INFO_ACK error");
                    pos++;
                    continue;
                }
            }
            else if (GATEWAY_EVENT_MESH == _buffer[pos + 1])
            {
                if (0)
                {
                    SigmaLogError(0, 0, "GATEWAY_EVENT_MESH error");
                    pos++;
                    continue;
                }
            }
            else if (GATEWAY_EVENT_NEW_DEVICE_FOUND == _buffer[pos + 1])
            {
                if (_buffer[pos] > 1 + 2)
                {
                    SigmaLogError(0, 0, "GATEWAY_EVENT_NEW_DEVICE_FOUND error");
                    pos++;
                    continue;
                }
            }
            else if (GATEWAY_EVENT_PROVISION_COMPLETE == _buffer[pos + 1])
            {
                if (_buffer[pos] > 2)
                {
                    SigmaLogError(0, 0, "GATEWAY_EVENT_PROVISION_COMPLETE error");
                    pos++;
                    continue;
                }
            }
            else if (GATEWAY_EVENT_PROVISION_BY_OTHERS == _buffer[pos + 1])
            {
                if (_buffer[pos] > 2)
                {
                    SigmaLogError(0, 0, "GATEWAY_EVENT_PROVISION_BY_OTHERS error");
                    pos++;
                    continue;
                }
            }
            usart_packet(_buffer[pos + 1], &(_buffer[pos + 2]), _buffer[pos] - 2);
            pos += _buffer[pos];
        }
        if (0 < pos && pos < _size)
            os_memcpy(_buffer, &(_buffer[pos]), _size - pos);
        _size -= pos;
    }

    TelinkUartPacket *packet = _packets, *prev = 0;
    while (packet)
    {
        if (os_ticks_from(packet->timer) > os_ticks_ms(1000))
            break;
        prev = packet;
        packet = packet->_next;
    }
    if (packet)
    {
        SigmaLogDebug(packet->parameters, packet->size, "usart packet discard.cmd:%s(%02x) parameters:", symbolic_telink_command(packet->cmd), packet->cmd);
        if (prev)
            prev->_next = packet->_next;
        else
            _packets = packet->_next;
        os_free(packet);
    }
}

typedef struct
{
    uint8_t period;
    uint8_t after;
}TRDeviceAdd;

int telink_mesh_device_add(uint8_t period, uint8_t after)
{
    if (_request && TELINK_REQUEST_DEVICE_ADD != _request->type)
        return 0;
    TRDeviceAdd *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceAdd));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_ADD;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceAdd *)(_request + 1);
        ctx->period = period;
        ctx->after = after;
    }
    ctx = (TRDeviceAdd *)(_request + 1);
    if (ctx->period != period || ctx->after != after)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[3] = {0};
        cmd[0] = MASTER_CMD_ADD_DEVICE;
        cmd[1] = period;
        cmd[2] = after;
        usart_write(0, cmd, 3);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK && MASTER_CMD_ADD_DEVICE == packet->parameters[0])
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            int ret = 0;
            if (1 == packet->parameters[1])
            {
                SigmaLogError(0, 0, "parameter error");
                ret = -1;
            }
            else if (2 == packet->parameters[1])
            {
                SigmaLogError(0, 0, "status error");
                ret = -1;
            }
            else if (!period && !after)
            {
                _request->state = STATE_TLM_PROVISION;
                _timer = os_ticks();
                ret = 0;
            }
            else
            {
                ret = 1;
            }

            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);

            if (ret)
            {
                os_free(_request);
                _request = 0;
                return ret;
            }
        }
    }
    if (STATE_TLM_PROVISION == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(5000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_PROVISION_COMPLETE || packet->cmd == GATEWAY_EVENT_PROVISION_BY_OTHERS)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);

            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    char name[16 + 1];
    char password[16 + 1];
    char ltk[16];
    uint8_t effect;
}TRMeshSet;

int telink_mesh_set(const char *name, const char *password, const uint8_t *ltk, uint8_t effect)
{
    if (os_strlen(name) > 16 || os_strlen(password) > 16)
    {
        SigmaLogError(0, 0, "parameter error(name:%s password:%s)(>16)", name, password);
        return -1;
    }
    
    if (_request && TELINK_REQUEST_MESH_SET != _request->type)
        return 0;
    TRMeshSet *ctx = 0;
    if (!_request)
    {
        SigmaLogDebug(ltk, 16, "name:%s password:%s ltk:", name, password);
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRMeshSet));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_MESH_SET;
        _request->retry = 0;
        _request->state = STATE_TLMSM_NAME;
        ctx = (TRMeshSet *)(_request + 1);
        os_strcpy(ctx->name, name);
        os_strcpy(ctx->password, password);
        os_memcpy(ctx->ltk, ltk, 16);
        ctx->effect = effect;
    }
    ctx = (TRMeshSet *)(_request + 1);
    if (os_strcmp(ctx->name, name) || 
        os_strcmp(ctx->password, password) || 
        os_memcmp(ctx->ltk, ltk, 16) || 
        effect != ctx->effect)
        return 0;
    if (STATE_TLMSM_RESTART == _request->state)
    {
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            return 0;
        }

        uint8_t cmd[3] = {0};
        cmd[0] = MASTER_CMD_ADD_DEVICE;
        usart_write(0, cmd, 3);
        _request->state = STATE_TLMSM_WAIT_RESTART;
        _timer = os_ticks();
    }
    if (STATE_TLMSM_WAIT_RESTART == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLMSM_RESTART;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK && MASTER_CMD_ADD_DEVICE == packet->parameters[0])
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            int ret = packet->parameters[1];
            _request->state = STATE_TLMSM_NAME;
            if (1 == ret)
                SigmaLogError(0, 0, "parameter error");
            else if (2 == ret)
                SigmaLogError(0, 0, "status error");

            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            
            if (ret)
            {
                if (2 == ret)
                {
                    _request->retry++;
                    if (_request->retry < 10)
                    {
                        SigmaLogError(0, 0, "retry %d", _request->retry);
                        _request->state = STATE_TLMSM_RESTART;
                        return 0;
                    }
                }
                os_free(_request);
                _request = 0;
                return -1;
            }
        }
    }
    if (STATE_TLMSM_NAME == _request->state)
    {
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            return 0;
        }

        uint8_t cmd[1 + 16 + 1] = {0};
        cmd[0] = MASTER_CMD_SET_GW_MESH_NAME;
        os_memcpy(cmd + 1, name, 16);
        usart_write(0, cmd, 1 + os_strlen(name));
        _request->state = STATE_TLMSM_WAIT_NAME;
        _timer = os_ticks();
    }
    if (STATE_TLMSM_WAIT_NAME == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLMSM_NAME;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK && 
                (MASTER_CMD_SET_GW_MESH_NAME == packet->parameters[0] || 
                 MASTER_CMD_SET_GW_MESH_PASSWORD == packet->parameters[0] ||
                 MASTER_CMD_SET_GW_MESH_LTK == packet->parameters[0] ||
                 MASTER_CMD_TAKE_EFFECT == packet->parameters[0]))
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            int ret = packet->parameters[1];
            _request->state = STATE_TLMSM_PASSWORD;
            if (1 == ret)
                SigmaLogError(0, 0, "parameter error");
            else if (2 == ret)
                SigmaLogError(0, 0, "status error");

            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            
            if (ret)
            {
                if (2 == ret)
                {
                    _request->retry++;
                    if (_request->retry < 10)
                    {
                        SigmaLogError(0, 0, "retry %d", _request->retry);
                        _request->state = STATE_TLMSM_RESTART;
                        return 0;
                    }
                }
                os_free(_request);
                _request = 0;
                return -1;
            }
        }
    }
    if (STATE_TLMSM_PASSWORD == _request->state)
    {
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            return 0;
        }

        uint8_t cmd[1 + 16 + 1] = {0};
        cmd[0] = MASTER_CMD_SET_GW_MESH_PASSWORD;
        os_memcpy(cmd + 1, password, 16);
        usart_write(0, cmd, 1 + os_strlen(password));
        _request->state = STATE_TLMSM_WAIT_PASSWORD;
        _timer = os_ticks();
    }
    if (STATE_TLMSM_WAIT_PASSWORD == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLMSM_PASSWORD;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK && 
                (MASTER_CMD_SET_GW_MESH_NAME == packet->parameters[0] || 
                 MASTER_CMD_SET_GW_MESH_PASSWORD == packet->parameters[0] ||
                 MASTER_CMD_SET_GW_MESH_LTK == packet->parameters[0] ||
                 MASTER_CMD_TAKE_EFFECT == packet->parameters[0]))
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            int ret = packet->parameters[1];
            _request->state = STATE_TLMSM_LTK;
            
            if (1 == ret)
                SigmaLogError(0, 0, "parameter error");
            else if (2 == ret)
                SigmaLogError(0, 0, "status error");

            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            
            if (ret)
            {
                if (2 == ret)
                {
                    _request->retry++;
                    if (_request->retry < 10)
                    {
                        SigmaLogError(0, 0, "retry %d", _request->retry);
                        _request->state = STATE_TLMSM_RESTART;
                        return 0;
                    }
                }
                os_free(_request);
                _request = 0;
                return -1;
            }
        }
    }
    if (STATE_TLMSM_LTK == _request->state)
    {
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            return 0;
        }

        uint8_t cmd[1 + 16] = {0};
        cmd[0] = MASTER_CMD_SET_GW_MESH_LTK;
        os_memcpy(cmd + 1, ltk, 16);
        usart_write(0, cmd, 1 + 16);
        _request->state = STATE_TLMSM_WAIT_LTK;
        _timer = os_ticks();
    }
    if (STATE_TLMSM_WAIT_LTK == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLMSM_PASSWORD;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK && 
                (MASTER_CMD_SET_GW_MESH_NAME == packet->parameters[0] || 
                 MASTER_CMD_SET_GW_MESH_PASSWORD == packet->parameters[0] ||
                 MASTER_CMD_SET_GW_MESH_LTK == packet->parameters[0] ||
                 MASTER_CMD_TAKE_EFFECT == packet->parameters[0]))
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            int ret = packet->parameters[1];
            _request->state = STATE_TLMSM_TAKE_EFFECT;
            if (1 == ret)
                SigmaLogError(0, 0, "parameter error");
            else if (2 == ret)
                SigmaLogError(0, 0, "status error");

            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            
            if (ret)
            {
                if (2 == ret)
                {
                    _request->retry++;
                    if (_request->retry < 10)
                    {
                        SigmaLogError(0, 0, "retry %d", _request->retry);
                        _request->state = STATE_TLMSM_RESTART;
                        return 0;
                    }
                }
                os_free(_request);
                _request = 0;
                return -1;
            }
        }
    }
    if (STATE_TLMSM_TAKE_EFFECT == _request->state)
    {
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            return 0;
        }

        uint8_t cmd[1 + 1] = {0};
        cmd[0] = MASTER_CMD_TAKE_EFFECT;
        cmd[1] = effect;
        usart_write(0, cmd, 1 + 1);
        _request->state = STATE_TLMSM_WAIT_TAKE_EFFECT;
        _timer = os_ticks();
    }
    if (STATE_TLMSM_WAIT_TAKE_EFFECT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLMSM_TAKE_EFFECT;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_CMD_ACK && MASTER_CMD_TAKE_EFFECT == packet->parameters[0])
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            int ret = 0;
            if (1 == packet->parameters[1])
            {
                SigmaLogError(0, 0, "parameter error");
                ret = -1;
            }
            else if (2 == packet->parameters[1])
            {
                SigmaLogError(0, 0, "status error");
                ret = -1;
            }
            else if (2 == effect)
            {
                _request->state = STATE_TLMSM_UPDATE;
                _timer = os_ticks();
                ret = 0;
            }
            else
            {
                _request->state = STATE_TLM_PROVISION;
                _timer = os_ticks();
                ret = 0;
            }

            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);

            if (ret)
            {
                os_free(_request);
                _request = 0;
                return ret;
            }
        }
    }
    if (STATE_TLM_PROVISION == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(10000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_PROVISION_COMPLETE || packet->cmd == GATEWAY_EVENT_PROVISION_BY_OTHERS)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);

            _request->state = STATE_TLMSM_UPDATE;
            _timer = os_ticks();
        }
    }
    if (STATE_TLMSM_UPDATE == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(30000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
            TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

            if (packet->cmd == GATEWAY_EVENT_MESH && 
                ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
                TELINK_MESH_OPCODE_MESH_UPDATE == p->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);

            if (2 == effect)
            {
                _request->state = STATE_TLM_DELAY;
                _timer = os_ticks();
            }
            else
            {
                os_free(_request);
                _request = 0;
                return 1;
            }
        }
    }
    if (STATE_TLM_DELAY == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    char name[17];
    char password[17];
    char ltk[16];
}TRMeshGet;

int telink_mesh_get(char *name, char *password, uint8_t *ltk)
{
    if (_request && TELINK_REQUEST_MESH_GET != _request->type)
        return 0;
    TRMeshGet *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRMeshGet));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_MESH_GET;
        _request->retry = 0;
        _request->state = STATE_TLMGM_NAME;
        ctx = (TRMeshGet *)(_request + 1);
    }
    ctx = (TRMeshGet *)(_request + 1);
    if (STATE_TLMGM_NAME == _request->state)
    {
        uint8_t cmd[2] = {0};
        cmd[0] = MASTER_CMD_GET_NETWORK_INFO;
        cmd[1] = MASTER_CMD_SET_GW_MESH_NAME;
        usart_write(0, cmd, 2);
        _request->state = STATE_TLMGM_WAIT_NAME;
        _timer = os_ticks();
    }
    if (STATE_TLMGM_WAIT_NAME == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_NETWORK_INFO_ACK && MASTER_CMD_SET_GW_MESH_NAME == packet->parameters[0])
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_strcpy(ctx->name, &(packet->parameters[1]));
            os_free(packet);
            _request->state = STATE_TLMGM_PASSWORD;
        }
    }
    if (STATE_TLMGM_PASSWORD == _request->state)
    {
        uint8_t cmd[2] = {0};
        cmd[0] = MASTER_CMD_GET_NETWORK_INFO;
        cmd[1] = MASTER_CMD_SET_GW_MESH_PASSWORD;
        usart_write(0, cmd, 2);

        _request->state = STATE_TLMSM_WAIT_PASSWORD;
        _timer = os_ticks();
    }
    if (STATE_TLMGM_WAIT_PASSWORD == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_NETWORK_INFO_ACK && MASTER_CMD_SET_GW_MESH_PASSWORD == packet->parameters[0])
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_strcpy(ctx->password, &(packet->parameters[1]));
            os_free(packet);
            _request->state = STATE_TLMGM_LTK;
        }
    }
    if (STATE_TLMGM_LTK == _request->state)
    {
        uint8_t cmd[2] = {0};
        cmd[0] = MASTER_CMD_GET_NETWORK_INFO;
        cmd[1] = MASTER_CMD_SET_GW_MESH_LTK;
        usart_write(0, cmd, 2);
        _request->state = STATE_TLMSM_WAIT_LTK;
        _timer = os_ticks();
    }
    if (STATE_TLMSM_WAIT_LTK == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == SLAVE_NETWORK_INFO_ACK && MASTER_CMD_SET_GW_MESH_LTK == packet->parameters[0])
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_memcpy(ctx->ltk, &(packet->parameters[1]), 16);
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint16_t delay;
    uint8_t onoff;
}TRLightOnoff;

int telink_mesh_light_onoff(uint16_t dst, uint8_t onoff, uint16_t delay)
{
    if (_request && TELINK_REQUEST_LIGHT_ONOFF != _request->type)
        return 0;
    TRLightOnoff *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRLightOnoff));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_LIGHT_ONOFF;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRLightOnoff *)(_request + 1);
        ctx->dst = dst;
        ctx->onoff = onoff;
        ctx->delay = delay;
    }
    ctx = (TRLightOnoff *)(_request + 1);
    if (ctx->dst != dst || ctx->delay != delay || ctx->onoff != onoff)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 3] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_ALL_ON;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = onoff;
        p->payload[1] = delay >> 0;
        p->payload[2] = delay >> 8;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 3);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLM_REQUEST;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t luminance;
}TRLightLuminance;

int telink_mesh_light_luminance(uint16_t dst, uint8_t luminance)
{
    if (_request && TELINK_REQUEST_LIGHT_LUMINANCE != _request->type)
        return 0;
    TRLightLuminance *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRLightLuminance));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_LIGHT_LUMINANCE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRLightLuminance *)(_request + 1);
        ctx->dst = dst;
        ctx->luminance = luminance;
    }
    ctx = (TRLightLuminance *)(_request + 1);
    if (ctx->dst != dst || ctx->luminance != luminance)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_LUMINANCE;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = luminance;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLM_REQUEST;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
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

typedef struct
{
    uint16_t dst;
    uint8_t channel;
    uint8_t color;
}TRLightColorChannel;

int telink_mesh_light_color_channel(uint16_t dst, uint8_t channel, uint8_t color)
{
    if (_request && TELINK_REQUEST_LIGHT_COLOR_CHANNEL != _request->type)
        return 0;
    TRLightColorChannel *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRLightColorChannel));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_LIGHT_COLOR_CHANNEL;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRLightColorChannel *)(_request + 1);
        ctx->dst = dst;
        ctx->channel = channel;
        ctx->color = color;
    }
    ctx = (TRLightColorChannel *)(_request + 1);
    if (ctx->dst != dst || ctx->channel != channel || ctx->color != color)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 2] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_COLOR;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = channel;
        p->payload[1] = color;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 2);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t r;
    uint8_t g;
    uint8_t b;
}TRLightColor;

int telink_mesh_light_color(uint16_t dst, uint8_t r, uint8_t g, uint8_t b)
{
    if (_request && TELINK_REQUEST_LIGHT_COLOR != _request->type)
        return 0;
    TRLightColor *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRLightColor));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_LIGHT_COLOR;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRLightColor *)(_request + 1);
        ctx->dst = dst;
        ctx->r = r;
        ctx->g = g;
        ctx->b = b;
    }
    ctx = (TRLightColor *)(_request + 1);
    if (ctx->dst != dst || ctx->r != r || ctx->g != g || ctx->b != b)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 4] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_COLOR;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x04;
        p->payload[1] = r;
        p->payload[2] = g;
        p->payload[3] = b;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 4);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLM_REQUEST;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t percentage;
}TRLightColorCT;

int telink_mesh_light_ctcolor(uint16_t dst, uint8_t percentage)
{
    if (_request && TELINK_REQUEST_LIGHT_COLOR_CT != _request->type)
        return 0;
    TRLightColorCT *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRLightColorCT));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_LIGHT_COLOR_CT;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRLightColorCT *)(_request + 1);
        ctx->dst = dst;
        ctx->percentage = percentage;
    }
    ctx = (TRLightColorCT *)(_request + 1);
    if (ctx->dst != dst || ctx->percentage != percentage)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 2] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_COLOR;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x05;
        p->payload[1] = percentage;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 2);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint16_t new_addr;
    uint32_t sequence;
}TRDeviceAddr;

int telink_mesh_device_addr(uint16_t dst, uint8_t *mac, uint16_t new_addr)
{
    if (_request && TELINK_REQUEST_DEVICE_ADDR != _request->type)
        return 0;
    TRDeviceAddr *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceAddr));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_ADDR;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceAddr *)(_request + 1);
        ctx->dst = dst;
        ctx->new_addr = new_addr;
    }
    ctx = (TRDeviceAddr *)(_request + 1);
    if (ctx->dst != dst || ctx->new_addr != new_addr)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 10] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        ctx->sequence = _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->seq[0] = ctx->sequence >> 0;
        p->seq[1] = ctx->sequence >> 8;
        p->seq[2] = ctx->sequence >> 16;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_DEVICE_ADDR;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = new_addr >> 0;
        p->payload[1] = new_addr >> 8;
        p->payload[2] = 0x01;
        p->payload[3] = 0x10;
        os_memcpy(&(p->payload[4]), mac, 6);
        binrev(&(p->payload[4]), 6);
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 10);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "ack timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            _request->state = STATE_TLM_RESPONSE;
        }
    }
    if (STATE_TLM_RESPONSE == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "resp timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
            TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

            if (packet->cmd == GATEWAY_EVENT_MESH && 
                ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
                TELINK_MESH_OPCODE_DEVICE_ADDR_RESPONSE == p->opcode &&
                ((ctx->sequence >> 0) & 0xff) == p->seq[0] &&
                ((ctx->sequence >> 8) & 0xff) == p->seq[1] &&
                ((ctx->sequence >> 16) & 0xff) == p->seq[2])
            {
                break;
            }
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

int telink_mesh_device_discover(void)
{
    if (_request && TELINK_REQUEST_DEVICE_DISCOVER != _request->type)
        return 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_DISCOVER;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
    }
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 4] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = 0xff;
        p->dst[1] = 0xff;
        p->opcode = TELINK_MESH_OPCODE_DEVICE_ADDR;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0xff;
        p->payload[1] = 0xff;
        p->payload[2] = 0x01;
        p->payload[3] = 0x10;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 4);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(5000))
        {
            SigmaLogError(0, 0, "ack timeout");
            _request->retry++;
            if (_request->retry < 10)
            {
                SigmaLogError(0, 0, "retry %d", _request->retry);
                _request->state = STATE_TLM_REQUEST;
                return 0;
            }
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);


            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

int telink_mesh_device_find(uint16_t *device)
{
    TelinkUartPacket *packet = _packets, *prev = 0;
    while (packet)
    {
        if (GATEWAY_EVENT_NEW_DEVICE_FOUND == packet->cmd)
        {
            *device = packet->parameters[0];
            break;
        }
        prev = packet;
        packet = packet->_next;
    }
    if (packet)
    {
        if (prev)
            prev->_next = packet->_next;
        else
            _packets = packet->_next;
        os_free(packet);
        return 1;
    }
    return 0;
}

int telink_mesh_device_lookup(uint16_t *device, uint8_t *mac)
{
    TelinkUartPacket *packet = _packets, *prev = 0;
    while (packet)
    {
        TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

        if (packet->cmd == GATEWAY_EVENT_MESH && 
            ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
            TELINK_MESH_OPCODE_DEVICE_ADDR_RESPONSE == p->opcode)
        {
            if (!(_request && 
                TELINK_REQUEST_DEVICE_ADDR == _request->type &&
                ((((TRDeviceAddr *)(_request + 1))->sequence >> 0) & 0xff) == p->seq[0] &&
                ((((TRDeviceAddr *)(_request + 1))->sequence >> 8) & 0xff) == p->seq[1] &&
                ((((TRDeviceAddr *)(_request + 1))->sequence >> 16) & 0xff) == p->seq[2]))
            {
                *device = p->payload[0];
                os_memcpy(mac, &(p->payload[2]), 6);
                binrev(mac, 6);
                break;
            }
        }
        prev = packet;
        packet = packet->_next;
    }
    if (packet)
    {
        if (prev)
            prev->_next = packet->_next;
        else
            _packets = packet->_next;
        os_free(packet);
        return 1;
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint32_t sequence;
}TRDeviceStatus;

int telink_mesh_device_status(uint16_t dst, uint8_t *ttc, uint8_t *hops, uint8_t *values)
{
    if (_request && TELINK_REQUEST_DEVICE_STATUS != _request->type)
        return 0;
    TRDeviceStatus *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceStatus));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_STATUS;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceStatus *)(_request + 1);
        ctx->dst = dst;
    }
    ctx = (TRDeviceStatus *)(_request + 1);
    if (ctx->dst != dst)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        ctx->sequence = _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->seq[0] = ctx->sequence >> 0;
        p->seq[1] = ctx->sequence >> 8;
        p->seq[2] = ctx->sequence >> 16;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_STATUS_ALL;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x10;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            _request->state = STATE_TLM_RESPONSE;
        }
    }
    if (STATE_TLM_RESPONSE == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "resp timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
            TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

            if (packet->cmd == GATEWAY_EVENT_MESH && 
                ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
                TELINK_MESH_OPCODE_STATUS_RESPONSE == p->opcode &&
                ((ctx->sequence >> 0) & 0xff) == p->seq[0] &&
                ((ctx->sequence >> 8) & 0xff) == p->seq[1] &&
                ((ctx->sequence >> 16) & 0xff) == p->seq[2])
            {
                *ttc = p->payload[8];
                *hops = p->payload[9];
                os_memcpy(values, p->payload, 6);
                break;
            }
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }

    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t type;
    uint32_t sequence;
}TRDeviceGroup;

int telink_mesh_device_group(uint16_t dst, uint8_t type, uint16_t *groups, uint8_t *count)
{
    if (_request && TELINK_REQUEST_DEVICE_GROUP != _request->type)
        return 0;
    TRDeviceGroup *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceGroup));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_GROUP;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceGroup *)(_request + 1);
        ctx->dst = dst;
        ctx->type = type;
    }
    ctx = (TRDeviceGroup *)(_request + 1);
    if (ctx->dst != dst || ctx->type != type)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 2] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        ctx->sequence = _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->seq[0] = ctx->sequence >> 0;
        p->seq[1] = ctx->sequence >> 8;
        p->seq[2] = ctx->sequence >> 16;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_GROUP_GET;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x10;
        p->payload[1] = type;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 2);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            _request->state = STATE_TLM_RESPONSE;
        }
    }
    if (STATE_TLM_RESPONSE == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
            TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

            if (packet->cmd == GATEWAY_EVENT_MESH && 
                ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
                dst == *(uint16_t *)p->src &&
                ((ctx->sequence >> 0) & 0xff) == p->seq[0] &&
                ((ctx->sequence >> 8) & 0xff) == p->seq[1] &&
                ((ctx->sequence >> 16) & 0xff) == p->seq[2])
            {
                if (p->opcode == TELINK_MESH_OPCODE_GROUP_RESPONSE_8)
                {
                    *count = 0;
                    int i = 0;
                    while (i < 8)
                    {
                        if (p->payload[i] != 0xff)
                            groups[(*count)++] = 0x8000 + p->payload[i];
                        i++;
                    }
                    break;
                }
                else if (p->opcode == TELINK_MESH_OPCODE_GROUP_RESPONSE_F4 || p->opcode == TELINK_MESH_OPCODE_GROUP_RESPONSE_B4)
                {
                    *count = 0;
                    int i = 0;
                    while (i < 8)
                    {
                        if (*(uint16_t *)&(p->payload[2 + i]) != 0xffff)
                            groups[(*count)++] = *(uint16_t *)&(p->payload[2 + i]);
                        i++;
                    }
                    break;
                }
            }
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }

    return 0;
}

typedef struct
{
    uint16_t dst;
}TRDeviceScene;

int telink_mesh_device_scene(uint16_t dst)
{
    if (_request && TELINK_REQUEST_DEVICE_SCENE != _request->type)
        return 0;
    TRDeviceScene *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceScene));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_GROUP;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceScene *)(_request + 1);
        ctx->dst = dst;
    }
    ctx = (TRDeviceScene *)(_request + 1);
    if (ctx->dst != dst)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_SCENE_GET;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x10;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }

    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t times;
}TRDeviceBlink;

int telink_mesh_device_blink(uint16_t dst, uint8_t times)
{
    if (_request && TELINK_REQUEST_DEVICE_BLINK != _request->type)
        return 0;
    TRDeviceBlink *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceBlink));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_BLINK;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceBlink *)(_request + 1);
        ctx->dst = dst;
        ctx->times = times;
    }
    ctx = (TRDeviceBlink *)(_request + 1);
    if (ctx->dst != dst || ctx->times != times)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_SW_CONFIG;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = times;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
}TRDeviceKickout;

int telink_mesh_device_kickout(uint16_t dst)
{
    if (_request && TELINK_REQUEST_DEVICE_KICKOUT != _request->type)
        return 0;
    TRDeviceKickout *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceKickout));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_KICKOUT;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceKickout *)(_request + 1);
        ctx->dst = dst;
    }
    ctx = (TRDeviceKickout *)(_request + 1);
    if (ctx->dst != dst)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_KICKOUT;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x01;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(10000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}TRDeviceTimeSet;

int telink_mesh_time_set(uint16_t dst, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    if (_request && TELINK_REQUEST_DEVICE_TIME_SET != _request->type)
        return 0;
    TRDeviceTimeSet *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceTimeSet));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_TIME_SET;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceTimeSet *)(_request + 1);
        ctx->dst = dst;
        ctx->year = year;
        ctx->month = month;
        ctx->day = day;
        ctx->hour = hour;
        ctx->minute = minute;
        ctx->second = second;
    }
    ctx = (TRDeviceTimeSet *)(_request + 1);
    if (ctx->dst != dst || ctx->year != year || ctx->month != month || ctx->day != day || ctx->hour != hour || ctx->minute != minute || ctx->second != second)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 7] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
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
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 7);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
}TRDeviceTimeGet;

int telink_mesh_time_get(uint16_t dst, uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    if (_request && TELINK_REQUEST_DEVICE_TIME_GET != _request->type)
        return 0;
    TRDeviceTimeGet *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRDeviceTimeGet));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_DEVICE_TIME_GET;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRDeviceTimeGet *)(_request + 1);
        ctx->dst = dst;
    }
    ctx = (TRDeviceTimeGet *)(_request + 1);
    if (ctx->dst != dst)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_TIME_GET;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x10;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            _request->state = STATE_TLM_RESPONSE;
        }
    }
    if (STATE_TLM_RESPONSE == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "resp timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
            TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

            if (packet->cmd == GATEWAY_EVENT_MESH && 
                ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
                TELINK_MESH_OPCODE_TIME_RESPONSE == p->opcode)
            {
                *year = *(uint16_t *)p->payload;
                *month = p->payload[2];
                *day = p->payload[3];
                *hour = p->payload[4];
                *minute = p->payload[5];
                *second = p->payload[6];
                break;
            }
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t idx;
    uint8_t onoff;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}TRAlarmAddDevice;

int telink_mesh_alarm_add_device(uint16_t dst, uint8_t idx, uint8_t onoff, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    if (_request && TELINK_REQUEST_ALARM_ADD_DEVICE != _request->type)
        return 0;
    TRAlarmAddDevice *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRAlarmAddDevice));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_ALARM_ADD_DEVICE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRAlarmAddDevice *)(_request + 1);
        ctx->dst = dst;
        ctx->idx = idx;
        ctx->onoff = onoff;
        ctx->month = month;
        ctx->day = day;
        ctx->hour = hour;
        ctx->minute = minute;
        ctx->second = second;
    }
    ctx = (TRAlarmAddDevice *)(_request + 1);
    if (ctx->dst != dst || ctx->idx != idx || ctx->onoff != onoff || ctx->month != month || ctx->day != day || ctx->hour != hour || ctx->minute != minute || ctx->second != second)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 9] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
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
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 9);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
}

typedef struct
{
    uint16_t dst;
    uint8_t idx;
    uint8_t scene;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}TRAlarmAddScene;

int telink_mesh_alarm_add_scene(uint16_t dst, uint8_t idx, uint8_t scene, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    if (_request && TELINK_REQUEST_ALARM_ADD_SCENE != _request->type)
        return 0;
    TRAlarmAddScene *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRAlarmAddDevice));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_ALARM_ADD_SCENE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRAlarmAddScene *)(_request + 1);
        ctx->dst = dst;
        ctx->idx = idx;
        ctx->scene = scene;
        ctx->month = month;
        ctx->day = day;
        ctx->hour = hour;
        ctx->minute = minute;
        ctx->second = second;
    }
    ctx = (TRAlarmAddScene *)(_request + 1);
    if (ctx->dst != dst || ctx->idx != idx || ctx->scene != scene || ctx->month != month || ctx->day != day || ctx->hour != hour || ctx->minute != minute || ctx->second != second)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 9] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
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
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 9);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
}

typedef struct
{
    uint16_t dst;
    uint8_t idx;
    uint8_t onoff;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}TRAlarmModifyDevice;

int telink_mesh_alarm_modify_device(uint16_t dst, uint8_t idx, uint8_t onoff, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    if (_request && TELINK_REQUEST_ALARM_MODIFY_DEVICE != _request->type)
        return 0;
    TRAlarmModifyDevice *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRAlarmModifyDevice));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_ALARM_MODIFY_DEVICE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRAlarmModifyDevice *)(_request + 1);
        ctx->dst = dst;
        ctx->idx = idx;
        ctx->onoff = onoff;
        ctx->month = month;
        ctx->day = day;
        ctx->hour = hour;
        ctx->minute = minute;
        ctx->second = second;
    }
    ctx = (TRAlarmModifyDevice *)(_request + 1);
    if (ctx->dst != dst || ctx->idx != idx || ctx->onoff != onoff || ctx->month != month || ctx->day != day || ctx->hour != hour || ctx->minute != minute || ctx->second != second)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 9] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
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
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 9);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t idx;
    uint8_t scene;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}TRAlarmModifyScene;

int telink_mesh_alarm_modify_scene(uint16_t dst, uint8_t idx, uint8_t scene, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    if (_request && TELINK_REQUEST_ALARM_MODIFY_SCENE != _request->type)
        return 0;
    TRAlarmModifyScene *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRAlarmModifyDevice));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_ALARM_MODIFY_SCENE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRAlarmModifyScene *)(_request + 1);
        ctx->dst = dst;
        ctx->idx = idx;
        ctx->scene = scene;
        ctx->month = month;
        ctx->day = day;
        ctx->hour = hour;
        ctx->minute = minute;
        ctx->second = second;
    }
    ctx = (TRAlarmModifyScene *)(_request + 1);
    if (ctx->dst != dst || ctx->idx != idx || ctx->scene != scene || ctx->month != month || ctx->day != day || ctx->hour != hour || ctx->minute != minute || ctx->second != second)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 9] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
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
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 9);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t idx;
}TRAlarmDelete;

int telink_mesh_alarm_delete(uint16_t dst, uint8_t idx)
{
    if (_request && TELINK_REQUEST_ALARM_DELETE != _request->type)
        return 0;
    TRAlarmDelete *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRAlarmDelete));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_ALARM_DELETE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRAlarmDelete *)(_request + 1);
        ctx->dst = dst;
        ctx->idx = idx;
    }
    ctx = (TRAlarmDelete *)(_request + 1);
    if (ctx->dst != dst || ctx->idx != idx)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 2] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_ALARM;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x01;
        p->payload[1] = idx;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 2);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t idx;
    uint8_t enable;
}TRAlarmRun;

int telink_mesh_alarm_run(uint16_t dst, uint8_t idx, uint8_t enable)
{
    if (_request && TELINK_REQUEST_ALARM_RUN != _request->type)
        return 0;
    TRAlarmRun *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRAlarmRun));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_ALARM_RUN;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRAlarmRun *)(_request + 1);
        ctx->dst = dst;
        ctx->idx = idx;
        ctx->enable = enable;
    }
    ctx = (TRAlarmRun *)(_request + 1);
    if (ctx->dst != dst || ctx->idx != idx || ctx->enable != enable)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 2] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_ALARM;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = enable ? 0x03 : 0x04;
        p->payload[1] = idx;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 2);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
}TRAlarmGet;

int telink_mesh_alarm_get(uint16_t dst, uint8_t *avalid, uint8_t *idx, uint8_t *cmd, uint8_t *type, uint8_t *enable, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *minute, uint8_t *second, uint8_t *scene)
{
    if (_request && TELINK_REQUEST_ALARM_GET != _request->type)
        return 0;
    TRAlarmGet *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRAlarmGet));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_ALARM_GET;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRAlarmGet *)(_request + 1);
        ctx->dst = dst;
    }
    ctx = (TRAlarmGet *)(_request + 1);
    if (ctx->dst != dst)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 2] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_ALARM_GET;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x10;
        p->payload[1] = 0x00;//chenjing
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 2);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            _request->state = STATE_TLM_RESPONSE;
        }
    }
    if (STATE_TLM_RESPONSE == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "resp timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
            TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

            if (packet->cmd == GATEWAY_EVENT_MESH && 
                ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
                TELINK_MESH_OPCODE_ALARM_RESPONSE == p->opcode)
            {
                *avalid = p->payload[0];
                *idx = p->payload[1];
                *cmd = p->payload[2] & 0x0f;
                *type = (p->payload[2] >> 4) & 0x07;
                *enable = p->payload[2] >> 7;
                *month = p->payload[3];
                *day = p->payload[4];
                *hour = p->payload[5];
                *minute = p->payload[6];
                *second = p->payload[7];
                *scene = p->payload[8];
                break;
            }
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint16_t group;
}TRGroupAdd;

int telink_mesh_group_add(uint16_t dst, uint16_t group)
{
    if (_request && TELINK_REQUEST_GROUP_ADD != _request->type)
        return 0;
    TRGroupAdd *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRGroupAdd));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_GROUP_ADD;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRGroupAdd *)(_request + 1);
        ctx->dst = dst;
        ctx->group = group;
    }
    ctx = (TRGroupAdd *)(_request + 1);
    if (ctx->dst != dst || ctx->group != group)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 3] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
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
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 3);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint16_t group;
}TRGroupDelete;

int telink_mesh_group_delete(uint16_t dst, uint16_t group)
{
    if (_request && TELINK_REQUEST_GROUP_DELETE != _request->type)
        return 0;
    TRGroupDelete *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRGroupDelete));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_GROUP_DELETE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRGroupDelete *)(_request + 1);
        ctx->dst = dst;
        ctx->group = group;
    }
    ctx = (TRGroupDelete *)(_request + 1);
    if (ctx->dst != dst || ctx->group != group)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 3] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
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
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 3);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t scene;
    uint8_t luminance;
    uint8_t rgb[3];
}TRSceneAdd;

int telink_mesh_scene_add(uint8_t dst, uint8_t scene, uint8_t luminance, uint8_t *rgb)
{
    if (_request && TELINK_REQUEST_SCENE_ADD != _request->type)
        return 0;
    TRSceneAdd *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRSceneAdd));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_SCENE_ADD;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRSceneAdd *)(_request + 1);
        ctx->dst = dst;
        ctx->scene = scene;
        ctx->luminance = luminance;
        os_memcpy(ctx->rgb, rgb, 3);
    }
    ctx = (TRSceneAdd *)(_request + 1);
    if (ctx->dst != dst || ctx->scene != scene || ctx->luminance != luminance || os_memcmp(ctx->rgb, rgb, 3))
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 6] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_SCENE;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x01;
        p->payload[1] = scene;
        p->payload[2] = luminance;
        os_memcpy(&(p->payload[3]), rgb, 3);
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 6);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t scene;
}TRSceneDelete;

int telink_mesh_scene_delete(uint16_t dst, uint8_t scene)
{
    if (_request && TELINK_REQUEST_SCENE_DELETE != _request->type)
        return 0;
    TRSceneDelete *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRSceneDelete));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_SCENE_DELETE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRSceneDelete *)(_request + 1);
        ctx->dst = dst;
        ctx->scene = scene;
    }
    ctx = (TRSceneDelete *)(_request + 1);
    if (ctx->dst != dst || ctx->scene != scene)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 2] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_SCENE;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x00;
        p->payload[1] = scene;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 2);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t scene;
}TRSceneLoad;

int telink_mesh_scene_load(uint16_t dst, uint8_t scene)
{
    if (_request && TELINK_REQUEST_SCENE_LOAD != _request->type)
        return 0;
    TRSceneLoad *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRSceneLoad));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_SCENE_LOAD;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRSceneLoad *)(_request + 1);
        ctx->dst = dst;
        ctx->scene = scene;
    }
    ctx = (TRSceneLoad *)(_request + 1);
    if (ctx->dst != dst || ctx->scene != scene)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_SCENE_LOAD;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = scene;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t scene;
}TRSceneGet;

int telink_mesh_scene_get(uint16_t dst, uint8_t scene, uint8_t *luminance, uint8_t *rgb)
{
    if (_request && TELINK_REQUEST_SCENE_GET != _request->type)
        return 0;
    TRSceneGet *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRSceneGet));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_SCENE_GET;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRSceneGet *)(_request + 1);
        ctx->dst = dst;
        ctx->scene = scene;
    }
    ctx = (TRSceneGet *)(_request + 1);
    if (ctx->dst != dst || ctx->scene != scene)
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 2] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_SCENE_GET;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x10;
        p->payload[1] = scene;
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 2);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    if (STATE_TLM_RESPONSE == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "resp timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
            TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

            if (packet->cmd == GATEWAY_EVENT_MESH && 
                ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
                TELINK_MESH_OPCODE_SCENE_RESPONSE == p->opcode &&
                scene == p->payload[0])
            {
                *luminance = p->payload[1];
                os_memcpy(rgb, &p->payload[2], 3);
                break;
            }
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

int telink_mesh_light_status_request(void)
{
    if (_request && TELINK_REQUEST_LIGHT_STATUS_REQUEST != _request->type)
        return 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest));
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_LIGHT_STATUS_REQUEST;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
    }
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x1e;
        cmd[3] = 0x1a;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x12;
        cmd[9] = 0x00;
        cmd[10] = 0x01;
        usart_write(0, cmd, 10 + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(200))
        {
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

int telink_mesh_light_status(uint16_t *src, uint8_t *online, uint8_t *luminance)
{
    *src = 0;

    TelinkUartPacket *packet = _packets, *prev = 0;
    while (packet)
    {
        TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

        if (packet->cmd == GATEWAY_EVENT_MESH && 
            ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
            TELINK_MESH_OPCODE_LIGHT_STATUS == p->opcode)
        {
            if (p->payload[0])
            {
                *src = p->payload[0];
                *online = (p->payload[1]) > 0;
                *luminance = p->payload[2];

                p->payload[0] = 0;

                return 1;
            }
            if (p->payload[4])
            {
                *src = p->payload[4];
                *online = (p->payload[5]) > 0;
                *luminance = p->payload[6];

                p->payload[4] = 0;

                return 1;
            }
            break;
        }
        prev = packet;
        packet = packet->_next;
    }
    if (packet)
    {
        if (prev)
            prev->_next = packet->_next;
        else
            _packets = packet->_next;
        os_free(packet);
    }
    return 0;
}

typedef struct
{
    uint16_t dst;
    uint8_t size;
    uint8_t buffer[];
}TRExtendsWrite;

int telink_mesh_extend_write(uint16_t dst, const uint8_t *buffer, uint8_t size)
{
    if (_request && TELINK_REQUEST_EXTENDS_WRITE != _request->type)
        return 0;
    TRExtendsWrite *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRExtendsWrite) + size);
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_EXTENDS_WRITE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRExtendsWrite *)(_request + 1);
        ctx->dst = dst;
        ctx->size = size;
        os_memcpy(ctx->buffer, buffer, size);
    }
    ctx = (TRExtendsWrite *)(_request + 1);
    if (ctx->dst != dst || ctx->size != size || os_memcmp(ctx->buffer, buffer, size))
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1 + 30] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_USER_ALL;
        p->vendor[0] = 0x11;
        p->vendor[1] = 0x02;
        p->payload[0] = 0x10;
        os_memcpy(p->payload + 1, buffer, size);
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1 + size);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

int telink_mesh_extend_read(uint16_t src, uint8_t *buffer, uint8_t size)
{
    TelinkUartPacket *packet = _packets, *prev = 0;
    while (packet)
    {
        TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

        if (packet->cmd == GATEWAY_EVENT_MESH && 
            ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
            TELINK_MESH_OPCODE_USER_RESPONSE == p->opcode &&
            src == *(uint16_t *)p->src)
        {
            if (size > packet->size - sizeof(TelinkBLEMeshNotify) - sizeof(TelinkMeshPacket) - 1)
                size = packet->size - sizeof(TelinkBLEMeshNotify) - sizeof(TelinkMeshPacket) - 1;
            os_memcpy(buffer, p->payload + 1, size);
            break;
        }
        prev = packet;
        packet = packet->_next;
    }
    if (packet)
    {
        if (prev)
            prev->_next = packet->_next;
        else
            _packets = packet->_next;
        os_free(packet);
        return size;
    }
    return 0;
}

typedef struct
{
    uint16_t vendor;
    uint16_t dst;
    uint8_t size;
    uint8_t buffer[];
}TRExtendsSRWrite;

int telink_mesh_extend_sr_write(uint16_t vendor, uint16_t dst, const uint8_t *buffer, uint8_t size)
{
    if (_request && TELINK_REQUEST_EXTENDS_SR_WRITE != _request->type)
        return 0;
    TRExtendsSRWrite *ctx = 0;
    if (!_request)
    {
        _request = os_malloc(sizeof(TelinkRequest) + sizeof(TRExtendsSRWrite) + size);
        if (!_request)
        {
            SigmaLogError(0, 0, "out of memory");
            return -1;
        }
        _request->type = TELINK_REQUEST_EXTENDS_SR_WRITE;
        _request->retry = 0;
        _request->state = STATE_TLM_REQUEST;
        ctx = (TRExtendsSRWrite *)(_request + 1);
        ctx->vendor = vendor;
        ctx->dst = dst;
        ctx->size = size;
        os_memcpy(ctx->buffer, buffer, size);
    }
    ctx = (TRExtendsSRWrite *)(_request + 1);
    if (ctx->dst != dst || ctx->size != size || os_memcmp(ctx->buffer, buffer, size))
        return 0;
    if (STATE_TLM_REQUEST == _request->state)
    {
        uint8_t cmd[10 + sizeof(TelinkMeshPacket) + 1] = {0};
        cmd[0] = 0x00;
        cmd[1] = 0x02;
        cmd[2] = 0x14;
        cmd[3] = 0x10;
        cmd[4] = 0x00;
        cmd[5] = 0x04;
        cmd[6] = 0x00;
        cmd[7] = 0x12;
        cmd[8] = 0x15;
        cmd[9] = 0x00;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(cmd + 10);
        p->seq[0] = _sequence >> 0;
        p->seq[1] = _sequence >> 8;
        p->seq[2] = _sequence >> 16;
        _sequence++;
        if (!_sequence)
            _sequence = 1;
        p->src[0] = 0;
        p->src[1] = 0;
        p->dst[0] = dst >> 0;
        p->dst[1] = dst >> 8;
        p->opcode = TELINK_MESH_OPCODE_USER_ALL;
        p->vendor[0] = vendor >> 0;
        p->vendor[1] = vendor >> 8;
        p->payload[0] = 0x10;
        os_memcpy(p->payload + 1, buffer, size);
        usart_write(0, cmd, 10 + sizeof(TelinkMeshPacket) + 1);
        _request->state = STATE_TLM_WAIT;
        _timer = os_ticks();
    }
    if (STATE_TLM_WAIT == _request->state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(1000))
        {
            SigmaLogError(0, 0, "timeout");
            os_free(_request);
            _request = 0;
            return -1;
        }
        TelinkUartPacket *packet = _packets, *prev = 0;
        while (packet)
        {
            if (packet->cmd == GATEWAY_EVENT_MESH && ATT_OP_WRITE_RSP == ((TelinkBLEMeshHeader *)packet->parameters)->opcode)
                break;
            prev = packet;
            packet = packet->_next;
        }
        if (packet)
        {
            if (prev)
                prev->_next = packet->_next;
            else
                _packets = packet->_next;
            os_free(packet);
            os_free(_request);
            _request = 0;
            return 1;
        }
    }
    return 0;
}

int telink_mesh_extend_sr_read(uint16_t src, uint8_t *buffer, uint8_t match, uint8_t size)
{
    TelinkUartPacket *packet = _packets, *prev = 0;
    while (packet)
    {
        TelinkBLEMeshNotify *notify = (TelinkBLEMeshNotify *)packet->parameters;
        TelinkMeshPacket *p = (TelinkMeshPacket *)(notify + 1);

        if (packet->cmd == GATEWAY_EVENT_MESH && 
            ATT_OP_HANDLE_VALUE_NOTI == notify->opcode && 
            TELINK_MESH_OPCODE_USER_RESPONSE == p->opcode &&
            src == *(uint16_t *)p->src &&
            !os_memcmp(buffer, &(p->payload[1]), match))
        {
            if (size > packet->size - sizeof(TelinkBLEMeshNotify) - sizeof(TelinkMeshPacket) - 1)
                size = packet->size - sizeof(TelinkBLEMeshNotify) - sizeof(TelinkMeshPacket) - 1;
            os_memcpy(buffer, p->payload + 1, size);
            break;
        }
        prev = packet;
        packet = packet->_next;
    }
    if (packet)
    {
        if (prev)
            prev->_next = packet->_next;
        else
            _packets = packet->_next;
        os_free(packet);
        return size;
    }
    return 0;
}
