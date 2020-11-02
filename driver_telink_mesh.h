#ifndef __DRIVER_TELINK_MESH_H__
#define __DRIVER_TELINK_MESH_H__

#include "env.h"

#define TELINK_MESH_OPCODE_ALL_ON 0xd0
#define TELINK_MESH_OPCODE_LUMINANCE 0xd2
#define TELINK_MESH_OPCODE_COLOR 0xe2
#define TELINK_MESH_OPCODE_DEVICE_ADDR 0xe0
#define TELINK_MESH_OPCODE_STATUS_ALL 0xda
#define TELINK_MESH_OPCODE_KICKOUT 0xe3
#define TELINK_MESH_OPCODE_TIME_SET 0xe4
#define TELINK_MESH_OPCODE_TIME_GET 0xe8
#define TELINK_MESH_OPCODE_ALARM 0xe5
#define TELINK_MESH_OPCODE_ALARM_GET 0xe6
#define TELINK_MESH_OPCODE_GROUP 0xd7
#define TELINK_MESH_OPCODE_GROUP_GET 0xdd
#define TELINK_MESH_OPCODE_SCENE 0xee
#define TELINK_MESH_OPCODE_SCENE_LOAD 0xef

typedef struct
{
    uint16_t l2capLen;
    uint16_t chanId;
    uint8_t opcode;
}__attribute_((packed)) TelinkBLEMeshHeader;

typedef void (*TelinkMeshListener)(uint8_t event, const void *msg, uint32_t size);

void telink_mesh_init(TelinkMeshListener listener);
void telink_mesh_update(void);

int telink_mesh_add_device(uint8_t period, uint8_t after, void (*rsp)(int ret));
int telink_mesh_set(const char *name, const char *password, const uint8_t *ltk, uint8_t effect, void (*rsp)(int ret));
int telink_mesh_get(void (*rsp)(int ret, const char *name, const char *password, const uint8_t *ltk));

#endif
