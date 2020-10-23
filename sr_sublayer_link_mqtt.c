#include "sr_sublayer_link_mqtt.h"
#include "sr_layer_link.h"
#include "sigma_event.h"
#include "sigma_mission.h"
#include "sigma_log.h"
#include "interface_os.h"
#include "MQTTAsync.h"
#include "cJSON.h"

#define MQTT_SERVER "tcp://52.82.41.140:1883"

enum {
    STATE_SSLM_IDLE = 0,
    STATE_SSLM_INIT,
    STATE_SSLM_RECONNECT,
    STATE_SSLM_CONNECTED,
};

static const char *_server = "server";
static uint8_t _key[16] = {0};

static uint8_t _state = STATE_SSLM_INIT;
static uint32_t _timer = 0;

static char *_client_id = 0;
static MQTTAsync _client;

static void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;

    SigmaLogError("disconnected.%s", cause ? cause : "");

    MQTTAsync_destroy(&_client);

    _state = STATE_SSLM_RECONNECT;
    _timer = os_ticks();
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    
    cJSON *msg = 0;
    do {
        uint8_t *packet = sll_headless_unpack(message->payload, &(message->payloadlen), _key);
    
        SigmaLogAction("topic:%s message:%s", topicName, (char *)packet);

        msg = cJSON_Parse((const char *)packet);
        if (!msg)
        {
            SigmaLogError("json parse error.raw:%s", (const char *)(packet + 1));
            break;
        }
        cJSON *header = cJSON_GetObjectItem(msg, "header");
        if (!header)
        {
            SigmaLogError("packet header error.raw:%s", (const char *)(packet + 1));
            break;
        }
        cJSON_AddItemToObject(msg, "user", cJSON_CreateString(_server));
        sigma_event_dispatch(EVENT_TYPE_PACKET, msg, 0);
    } while (0);
    if (msg)
        cJSON_Delete(msg);

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

typedef struct
{
    uint8_t state;
    uint32_t timer;
    uint32_t pos;
    uint32_t size;
    uint8_t *list;
}SigmaMissionMqttSubscribe;

static void onSubscribe(void* context, MQTTAsync_successData* response)
{
    SigmaMissionMqttSubscribe *ctx = (SigmaMissionMqttSubscribe *)context;

    SigmaLogAction("MQTTAsync_subscribe %s successed.", ctx->list + ctx->pos);
    
    ctx->pos += os_strlen(ctx->list + ctx->pos) + 1;
    ctx->state = 0;
}

static void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
    SigmaMissionMqttSubscribe *ctx = (SigmaMissionMqttSubscribe *)context;

    SigmaLogAction("MQTTAsync_subscribe %s failed.(code:%d)", ctx->list + ctx->pos, response->code);
    
    ctx->pos += os_strlen(ctx->list + ctx->pos) + 1;
    ctx->state = 0;
}

static int mission_mqtt_subscribe(SigmaMission *mission)
{
    SigmaMissionMqttSubscribe *ctx = (SigmaMissionMqttSubscribe *)sigma_mission_extends(mission);

    if (ctx->pos >= ctx->size)
    {
        sigma_event_dispatch(EVENT_TYPE_CLIENT_AUTH, (void *)_server, os_strlen(_server));
        os_free(ctx->list);
        return 1;
    }
    if (0 == ctx->state)
    {
        MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
        opts.onSuccess = onSubscribe;
        opts.onFailure = onSubscribeFailure;
        opts.context = ctx;

        ctx->state = 1;
        ctx->timer = os_ticks();

        int ret = 0;
        if ((ret = MQTTAsync_subscribe(_client, ctx->list + ctx->pos, 0, &opts)) != MQTTASYNC_SUCCESS)
        {
            SigmaLogError("MQTTAsync_subscribe %s failed.(ret:%d)", ctx->list + ctx->pos, ret);
            ctx->state = 0;
            ctx->pos += os_strlen(ctx->list + ctx->pos) + 1;
        }
    }
    if (1 == ctx->state)
    {
        if (os_ticks_from(ctx->timer) > os_ticks_ms(5000))
        {
            ctx->state = 0;
            ctx->pos += os_strlen(ctx->list + ctx->pos) + 1;
        }
    }
    return 0;
}

static void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;

    SigmaLogAction("connected.");
    _state = STATE_SSLM_CONNECTED;

    uint32_t size = 0;
    char *list = sll_client_list(&size);
    if (!list)
    {
        SigmaLogError("empty client list.");
        return;
    }

    SigmaMission *mission = 0;
    SigmaMissionIterator it = {0};
    while ((mission = sigma_mission_iterator(&it)))
    {
        if (MISSION_TYPE_MQTT_SUBSCRIBE == mission->type)
            break;
    }
    if (!mission)
    {
        mission = sigma_mission_create(mission, MISSION_TYPE_MQTT_SUBSCRIBE, mission_mqtt_subscribe, sizeof(SigmaMissionMqttSubscribe));
        if (!mission)
        {
            SigmaLogError("out of memory");
            os_free(list);
            return;
        }
    }
    SigmaMissionMqttSubscribe *ctx = (SigmaMissionMqttSubscribe *)sigma_mission_extends(mission);
    if (ctx->list)
        os_free(ctx->list);
    ctx->state = 0;
    ctx->pos = 0;
    ctx->size = size;
    ctx->list = list;
}

static void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	MQTTAsync client = (MQTTAsync)context;

	SigmaLogAction("connect failed.(ret:%d)", response->code);

    _state = STATE_SSLM_RECONNECT;
    _timer = os_ticks();
}

void sslm_init(void)
{
    _state = STATE_SSLM_IDLE;

    uint8_t key[16] = {0};
    if (sll_client_key(_server, key) < 0)
    {
        sll_client_add_direct(_server, _key);
    }

    sslm_start("chenjing");
}

void sslm_update(void)
{
    if (STATE_SSLM_INIT == _state)
    {
        int ret = MQTTAsync_create(&_client, MQTT_SERVER, _client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
        if (ret != MQTTASYNC_SUCCESS)
        {
            SigmaLogError("MQTTAsync_create failed.(ret:%d)", ret);
            _state = STATE_SSLM_RECONNECT;
            _timer = os_ticks();
            return;
        }
        ret = MQTTAsync_setCallbacks(_client, _client, connlost, msgarrvd, NULL);
        if (ret != MQTTASYNC_SUCCESS)
        {
            SigmaLogError("MQTTAsync_setCallbacks failed.(ret:%d)", ret);
            MQTTAsync_destroy(&_client);
            _state = STATE_SSLM_RECONNECT;
            _timer = os_ticks();
            return;
        }
        MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;
        opts.keepAliveInterval = 20;
        opts.cleansession = 1;
        opts.onSuccess = onConnect;
        opts.onFailure = onConnectFailure;
        opts.context = _client;
        
        ret = MQTTAsync_connect(_client, &opts);
        if (ret != MQTTASYNC_SUCCESS)
        {
            SigmaLogError("MQTTAsync_connect failed.(ret:%d)", ret);
            MQTTAsync_destroy(&_client);
            _state = STATE_SSLM_RECONNECT;
            _timer = os_ticks();
            return;
        }
        _state = STATE_SSLM_RECONNECT;
        _timer = os_ticks();
    }
    if (STATE_SSLM_RECONNECT == _state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(5000))
            _state = STATE_SSLM_INIT;
    }
}

void sslm_start(const char *id)
{
    if (_client_id)
        os_free(_client_id);
    _client_id = os_malloc(os_strlen(id) + 1);
    if (!_client_id)
    {
        SigmaLogError("out of memory");
        return;
    }
    os_strcpy(_client_id, id);
    _state = STATE_SSLM_INIT;
}

void sslm_report(uint8_t seq, const void *buffer, uint32_t size)
{
    if (STATE_SSLM_CONNECTED != _state)
        return;

    uint32_t length = 0;
    char *clients = sll_client_list(&length);

    uint32_t pos = 0;
    while (pos < length)
    {
        sslm_send(clients + pos, seq, buffer, size);

        pos += os_strlen(clients + pos) + 1;
    }
}

void onSendFailure(void* context, MQTTAsync_failureData* response)
{
	MQTTAsync client = (MQTTAsync)context;

    SigmaLogError("send failed.(token:%d code:%d)", response->token, response->code);

    MQTTAsync_destroy(_client);
    
    _state = STATE_SSLM_RECONNECT;
    _timer = os_ticks();
}

void onSend(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;

    SigmaLogAction("send successed.(token:%d)", response->token);
}

void sslm_send(const char *id, uint8_t seq, const void *buffer, uint32_t size)
{
    SigmaLogAction("send to %s(seq:%u size:%d)", id, seq, size);

    uint8_t key[32] = {0};
    if (sll_client_key(id, key) < 0)
    {
        SigmaLogError("client %s not found", id);
        return;
    }
	
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	opts.onSuccess = onSend;
	opts.onFailure = onSendFailure;

    uint8_t *packet = sll_headless_pack(seq, buffer, &size, key);

	MQTTAsync_message msg = MQTTAsync_message_initializer;
	msg.payload = packet;
	msg.payloadlen = size;
	msg.qos = 0;
	msg.retained = 0;

    int ret = 0;
	if ((ret = MQTTAsync_sendMessage(_client, id, &msg, &opts)) != MQTTASYNC_SUCCESS)
		SigmaLogError("MQTTAsync_sendMessage failed.(ret:%d)", ret);
    os_free(packet);
}