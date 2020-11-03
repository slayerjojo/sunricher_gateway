#ifndef __DRIVER_TELINK_MESH_H__
#define __DRIVER_TELINK_MESH_H__

#include "env.h"

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

typedef void (*TelinkMeshListener)(uint8_t event, const void *msg, uint32_t size);

void telink_mesh_init(TelinkMeshListener listener);
void telink_mesh_update(void);

int telink_mesh_add_device(uint8_t period, uint8_t after, void (*rsp)(int ret));
int telink_mesh_set(const char *name, const char *password, const uint8_t *ltk, uint8_t effect, void (*rsp)(int ret));
int telink_mesh_get(void (*rsp)(int ret, const char *name, const char *password, const uint8_t *ltk));

#endif
