#ifndef __SIGMA_MISSION_H__
#define __SIGMA_MISSION_H__

#include "env.h"

#ifdef __cplusplus
extern "C"
{
#endif

enum
{
    MISSION_TYPE_MQTT_SUBSCRIBE = 0,
    MISSION_TYPE_MQTT_PUBLISH,
    MISSION_TYPE_TELINK_ADD_DEVICE,
    MISSION_TYPE_TELINK_GET_MESH,
    MISSION_TYPE_TELINK_SET_MESH,
    MISSION_TYPE_TELINK_MESH_BLE_PACKET,
};

struct _sigma_mission;

typedef int (*SigmaMissionHandler)(struct _sigma_mission *mission);

typedef struct _sigma_mission
{
    struct _sigma_mission *next;
    struct _sigma_mission *deps;

    SigmaMissionHandler handler;

    uint8_t release:1;
    uint8_t type:7;

    uint8_t extends[];
}SigmaMission;

typedef struct 
{
    SigmaMission *root;
    SigmaMission *depends;
}SigmaMissionIterator;

void sigma_mission_init(void);
void sigma_mission_update(void);

SigmaMission *sigma_mission_create(SigmaMission *mission, uint8_t type, SigmaMissionHandler handler, size_t extends);
void sigma_mission_release(SigmaMission *mission);

void *sigma_mission_extends(SigmaMission *mission);

SigmaMission *sigma_mission_iterator(SigmaMissionIterator *it);

#ifdef __cplusplus
}
#endif

#endif
