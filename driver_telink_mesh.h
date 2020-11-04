#ifndef __DRIVER_TELINK_MESH_H__
#define __DRIVER_TELINK_MESH_H__

#include "env.h"

enum {
    TELINK_EVENT_NEW_DEVICE_FOUND = 0,
    TELINK_EVENT_DEVICE_ADDR,
    TELINK_EVENT_DEVICE_GROUP,
    TELINK_EVENT_DEVICE_STATUS,
    TELINK_EVENT_DEVICE_TIME,
    TELINK_EVENT_DEVICE_ALARM,
    TELINK_EVENT_DEVICE_SCENE,
    TELINK_EVENT_EXTENDS,
};

typedef struct
{
    uint16_t addr;
    uint8_t count;
    uint16_t groups[];
}TelinkMeshEventGroup;

typedef struct
{
    uint16_t addr;
    uint8_t ttc;
    uint8_t hops;
    uint8_t values[];
}TelinkMeshEventStatus;

typedef struct
{
    uint16_t addr;
    uint8_t size;
    uint8_t extends[];
}TelinkMeshEventExtends;

typedef struct
{
    uint16_t addr;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}TelinkMeshEventTime;

typedef struct
{
    uint16_t addr;

    uint8_t avalid;
    uint8_t idx;
    uint8_t cmd;
    uint8_t type;
    uint8_t enable;
    uint8_t month;
    union {
        uint8_t day;
        uint8_t week;
    };
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t scene;
}TelinkMeshEventAlarm;

typedef struct
{
    uint16_t addr;

    uint8_t scene;
    uint8_t luminance;
    uint8_t rgb[3];
}TelinkMeshEventScene;

void telink_mesh_init(void);
void telink_mesh_update(void);

int telink_mesh_add_device(uint8_t period, uint8_t after, void (*rsp)(int ret));
int telink_mesh_set(const char *name, const char *password, const uint8_t *ltk, uint8_t effect);
int telink_mesh_get(void (*rsp)(int ret, const char *name, const char *password, const uint8_t *ltk));
int telink_mesh_light_onoff(uint16_t dst, uint8_t onoff, uint16_t delay, void (*rsp)(int ret));
int telink_mesh_light_luminance(uint16_t dst, uint8_t luminance);
int telink_mesh_music_start(uint16_t dst);
int telink_mesh_music_stop(uint16_t dst);
int telink_mesh_light_color_channel(uint16_t dst, uint8_t channel, uint8_t color);
int telink_mesh_light_color(uint16_t dst, uint8_t r, uint8_t g, uint8_t b);
int telink_mesh_light_ctcolor(uint16_t dst, uint8_t percentage);
int telink_mesh_device_addr(uint16_t dst, uint16_t new_addr);
int telink_mesh_device_discover(uint16_t dst);
int telink_mesh_device_status(uint16_t dst, uint8_t relays);
int telink_mesh_device_group(uint16_t dst, uint8_t relays, uint8_t type);
int telink_mesh_device_scene(uint16_t dst, uint8_t relays);
int telink_mesh_device_blink(uint16_t dst, uint8_t times);
int telink_mesh_device_kickout(uint16_t dst);
int telink_mesh_time_set(uint16_t dst, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
int telink_mesh_time_get(uint16_t dst, uint8_t relays);
int telink_mesh_alarm_add_device(uint16_t dst, uint8_t idx, uint8_t onoff, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
int telink_mesh_alarm_add_scene(uint16_t dst, uint8_t idx, uint8_t scene, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
int telink_mesh_alarm_modify_device(uint16_t dst, uint8_t idx, uint8_t onoff, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
int telink_mesh_alarm_modify_scene(uint16_t dst, uint8_t idx, uint8_t scene, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
int telink_mesh_alarm_delete(uint16_t dst, uint8_t idx);
int telink_mesh_alarm_run(uint16_t dst, uint8_t idx, uint8_t enable);
int telink_mesh_alarm_get(uint16_t dst, uint8_t relays);
int telink_mesh_group_add(uint16_t dst, uint16_t group);
int telink_mesh_group_delete(uint16_t dst, uint16_t group);
int telink_mesh_scene_add(uint8_t dst, uint8_t scene, uint8_t luminance, uint8_t r, uint8_t g, uint8_t b);
int telink_mesh_scene_delete(uint16_t dst, uint8_t scene);
int telink_mesh_scene_load(uint16_t dst, uint8_t scene);
int telink_mesh_extend_write(uint16_t dst, uint8_t relays, uint8_t *buffer, uint8_t size);

#endif
