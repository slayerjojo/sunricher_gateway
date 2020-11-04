#ifndef __DRIVER_TELINK_MESH_EXTENDS_SUNRICHER_H__
#define __DRIVER_TELINK_MESH_EXTENDS_SUNRICHER_H__

#include "env.h"

void tmes_device_type_set(uint16_t dst, uint8_t category, uint8_t type, void (*rsp)(int ret, uint16_t dst, uint8_t category, uint8_t type, uint8_t group_count, uint32_t start));
void tmes_device_type_get(uint16_t dst, void (*rsp)(int ret, uint16_t dst, uint8_t category, uint8_t type, uint8_t group_count, uint32_t start));
void tmes_device_type_clear(uint16_t dst);
void tmes_device_mac(uint16_t dst, void (*rsp)(int ret, uint16_t dst, uint8_t *mac));
void tmes_light_mode_get(uint16_t dst, void (*rsp)(int ret, uint16_t dst, uint8_t speed, uint8_t temperature, uint8_t mode, uint8_t id, uint8_t internal));
void tmes_light_mode_custom_list(uint16_t dst, void (*rsp)(int ret, uint16_t dst, uint8_t *ids, uint8_t size));
void tmes_light_mode_custom_get(uint16_t dst, uint8_t id, void (*rsp)(int ret, uint8_t id, uint8_t *color, uint8_t count));
void tmes_light_mode_custom_add(uint16_t dst, uint8_t id, uint8_t idx, uint8_t *color);
void tmes_light_mode_custom_delete(uint16_t dst, uint8_t id);
void tmes_light_mode_global_set(uint16_t dst, uint8_t mode, uint8_t enable);
void tmes_light_mode_speed_set(uint16_t dst, uint8_t speed);
void tmes_light_mode_temperature_set(uint16_t dst, uint8_t temperature);
void tmes_light_mode_preinstall_set(uint16_t dst, uint8_t preinstall);
void tmes_light_mode_run(uint16_t dst);
void tmes_light_scene_list(uint16_t dst, void (*rsp)(int ret, uint16_t dst, uint8_t *scenes, uint8_t size));
void tmes_light_scene_get(uint16_t dst, uint8_t scene, void (*rsp)(int ret, uint16_t dst, uint8_t scene, uint8_t block));
void tmes_light_scene_save(uint16_t dst, uint8_t scene, uint16_t delay);
void tmes_light_scene_delete(uint16_t dst, uint8_t scene);
void tmes_light_scene_run(uint16_t dst, uint8_t scene);
void tmes_light_channel_enable(uint16_t dst, uint8_t channel);
void tmes_light_channel_action(uint16_t dst, uint8_t channel, uint8_t action, uint8_t flag);
void tmes_light_channel_speed_action(uint16_t dst, uint8_t action);
void tmes_light_group_preinstall_run(uint16_t group, uint8_t preinstall, uint8_t global, uint8_t speed, uint8_t temperature);
void tmes_light_group_custom_run(uint16_t group, uint8_t preinstall, uint8_t global, uint8_t speed, uint8_t temperature);
void tmes_light_move_to_level(uint16_t dst, uint8_t bright, uint8_t r, uint8_t g, uint8_t b, uint8_t ct, uint16_t time);
void tmes_light_pwm_set(uint16_t dst, uint16_t freq);
void tmes_light_pwm_get(uint16_t dst, void (*rsp)(uint16_t dst, uint16_t dst));
void tmes_light_pwm_deadtime_set(uint16_t dst, uint8_t interval);
void tmes_light_pwm_deadtime_get(uint16_t dst, void (*rsp)(int ret, uint8_t interval));
void tmes_light_bright_min_set(uint16_t dst, uint8_t percent);
int tmes_light_bright_min_get(uint16_t dst, uint8_t *percent);

#endif
