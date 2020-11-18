#ifndef __DRIVER_TELINK_EXTENDS_SUNRICHER_H__
#define __DRIVER_TELINK_EXTENDS_SUNRICHER_H__

#include "env.h"

typedef enum {
    SR_CATEGORY_LIGHT_DIMMER_L = 0x0111,
    SR_CATEGORY_LIGHT_RELAY_L = 0x0112,
    SR_CATEGORY_LIGHT_DIMMER_CODE_L = 0x0113,
    SR_CATEGORY_LIGHT_RELAY_N = 0x0114,
    SR_CATEGORY_LIGHT_ONOFF = 0x0130,
    SR_CATEGORY_LIGHT_MONO = 0x0131,
    SR_CATEGORY_LIGHT_CTT = 0x0132,
    SR_CATEGORY_LIGHT_RGB = 0x0133,
    SR_CATEGORY_LIGHT_RGBW = 0x0134,
    SR_CATEGORY_LIGHT_RGB_CCT = 0x0135,
    SR_CATEGORY_LIGHT_RGB_DTW = 0x0136,
    
    SR_CATEGORY_REMOTER_1_BLRC01 = 0x0200,
    SR_CATEGORY_REMOTER_1_BLRC02 = 0x0201,
    SR_CATEGORY_REMOTER_1_BLRC03 = 0x0202,
    SR_CATEGORY_REMOTER_1_BLRC04 = 0x0203,
    SR_CATEGORY_REMOTER_1_BLRC05 = 0x0204,
    SR_CATEGORY_REMOTER_1_BLRC06 = 0x0205,
    SR_CATEGORY_REMOTER_1_BLRC07 = 0x0206,
    SR_CATEGORY_REMOTER_1_BLRC08 = 0x0207,
    SR_CATEGORY_REMOTER_1_BLRC09 = 0x0208,
    SR_CATEGORY_REMOTER_1_BLRC10 = 0x0209,
    SR_CATEGORY_REMOTER_1_BLRC11 = 0x020a,
    SR_CATEGORY_REMOTER_1_BLRC12 = 0x020b,
    SR_CATEGORY_REMOTER_1_BLRC13 = 0x020c,
    SR_CATEGORY_REMOTER_1_BLRC14 = 0x020d,
    SR_CATEGORY_REMOTER_1_BLRC15 = 0x020e,
    SR_CATEGORY_REMOTER_1_BLRC16 = 0x020f,
    SR_CATEGORY_REMOTER_1_BLRC17 = 0x0300,
    SR_CATEGORY_REMOTER_1_BLRC18 = 0x0301,
    SR_CATEGORY_REMOTER_1_BLRC19 = 0x0302,
    SR_CATEGORY_REMOTER_1_BLRC20 = 0x0303,
    
    SR_CATEGORY_REMOTER_2_BLRC01 = 0x0a00,
    SR_CATEGORY_REMOTER_2_BLRC02 = 0x0a01,
    SR_CATEGORY_REMOTER_2_BLRC03 = 0x0a02,
    SR_CATEGORY_REMOTER_2_BLRC04 = 0x0a03,
    SR_CATEGORY_REMOTER_2_BLRC05 = 0x0a04,
    SR_CATEGORY_REMOTER_2_BLRC06 = 0x0a05,
    SR_CATEGORY_REMOTER_2_BLRC07 = 0x0a06,
    SR_CATEGORY_REMOTER_2_BLRC08 = 0x0a07,
    SR_CATEGORY_REMOTER_2_BLRC09 = 0x0a08,
    SR_CATEGORY_REMOTER_2_BLRC10 = 0x0a09,
    SR_CATEGORY_REMOTER_2_BLRC11 = 0x0a0a,
    SR_CATEGORY_REMOTER_2_BLRC12 = 0x0a0b,
    SR_CATEGORY_REMOTER_2_BLRC13 = 0x0a0c,
    SR_CATEGORY_REMOTER_2_BLRC14 = 0x0a0d,
    SR_CATEGORY_REMOTER_2_BLRC15 = 0x0a0e,
    SR_CATEGORY_REMOTER_2_BLRC16 = 0x0a0f,
    SR_CATEGORY_REMOTER_2_BLRC17 = 0x0b00,
    SR_CATEGORY_REMOTER_2_BLRC18 = 0x0b01,
    SR_CATEGORY_REMOTER_2_BLRC19 = 0x0b02,
    SR_CATEGORY_REMOTER_2_BLRC20 = 0x0b03,
    
    SR_CATEGORY_REMOTER_3_BLRC01 = 0x0c00,
    SR_CATEGORY_REMOTER_3_BLRC02 = 0x0c01,
    SR_CATEGORY_REMOTER_3_BLRC03 = 0x0c02,
    SR_CATEGORY_REMOTER_3_BLRC04 = 0x0c03,
    SR_CATEGORY_REMOTER_3_BLRC05 = 0x0c04,
    SR_CATEGORY_REMOTER_3_BLRC06 = 0x0c05,
    SR_CATEGORY_REMOTER_3_BLRC07 = 0x0c06,
    SR_CATEGORY_REMOTER_3_BLRC08 = 0x0c07,
    SR_CATEGORY_REMOTER_3_BLRC09 = 0x0c08,
    SR_CATEGORY_REMOTER_3_BLRC10 = 0x0c09,
    SR_CATEGORY_REMOTER_3_BLRC11 = 0x0c0a,
    SR_CATEGORY_REMOTER_3_BLRC12 = 0x0c0b,
    SR_CATEGORY_REMOTER_3_BLRC13 = 0x0c0c,
    SR_CATEGORY_REMOTER_3_BLRC14 = 0x0c0d,
    SR_CATEGORY_REMOTER_3_BLRC15 = 0x0c0e,
    SR_CATEGORY_REMOTER_3_BLRC16 = 0x0c0f,
    SR_CATEGORY_REMOTER_3_BLRC17 = 0x0d00,
    SR_CATEGORY_REMOTER_3_BLRC18 = 0x0d01,
    SR_CATEGORY_REMOTER_3_BLRC19 = 0x0d02,
    SR_CATEGORY_REMOTER_3_BLRC20 = 0x0d03,

    SR_CATEGORY_REMOTER_SWITCH_TOUCH = 0x0e00,
    SR_CATEGORY_REMOTER_SWITCH_CODE = 0x0e01,
    SR_CATEGORY_REMOTER_CURTAIN = 0x0e02,
    SR_CATEGORY_REMOTER_BUTTON = 0x0e03,

    SR_CATEGORY_SENSOR = 0x0400,
    SR_CATEGORY_SENSOR_DOOR = 0x0401,
    SR_CATEGORY_SENSOR_TEMP = 0x0402,
    
    SR_CATEGORY_TRANSPORT = 0x0531,

    SR_CATEGORY_PERIPHERY = 0x0600,

    SR_CATEGORY_CURTAIN = 0x0701,
    SR_CATEGORY_CURTAIN_BLINDS = 0x0702,

    SR_CATEGORY_SOCKET = 0x0801,
}SRCategory;

typedef enum {
    SR_TRANSITION_RESERVE = 0,
    SR_TRANSITION_FADE_UP,
    SR_TRANSITION_FADE_DOWN,
    SR_TRANSITION_FADE_UP_DOWN,
    SR_TRANSITION_MIX,
    SR_TRANSITION_JUMP,
    SR_TRANSITION_FLASH,
}SRTransition;

/*
 * 0x00
 * 保留
 * 0x01
 * 七彩混变
 * 0x02
 * 红色渐变
 * 0x03
 * 绿色渐变
 * 0x04
 * 蓝色渐变
 * 0x05
 * 黄色渐变
 * 0x06
 * 青色渐变
 * 0x07
 * 紫色渐变
 * 0x08
 * 白色渐变
 * 0x09
 * 红绿渐变
 * 0x0A
 * 红蓝渐变
 * 0x0B
 * 绿蓝渐变
 * 0x0C
 * 七彩频闪
 * 0x0D
 * 红色频闪
 * 0x0E
 * 绿色频闪
 * 0x0F
 * 蓝色频闪
 * 0x10
 * 黄色频闪
 * 0x11
 * 青色频闪
 * 0x12
 * 紫色频闪
 * 0x13
 * 白色频闪
 * 0x14
 * 七彩跳变
 * 0x15-0xFF
 * 保留
 */
typedef enum {
    SR_PREINSTALL_LIGHT_MODE_RESERVE = 0x00, //保留
}SRPreinstallLightMode;

typedef enum {
    SR_SWITCH_TYPE_ONOFF = 1,
    SR_SWITCH_TYPE_PUSH,
    SR_SWITCH_TYPE_3WAY,
}SRSwitchType;

typedef struct {
    uint8_t type;
    uint8_t enable;
    union {
        uint8_t scene;
        struct {
            uint8_t bright;
            uint8_t rgb[3];
            uint8_t ct;
            uint16_t duration;
        }color;
        struct {
            uint8_t value;
            uint16_t duration;
        }onoff;
    };
}SRSunSetting;

int tmes_device_type_write(uint16_t dst, SRCategory category);
int tmes_device_type(uint16_t dst, SRCategory *category, uint8_t *group_count, uint32_t *start);
int tmes_device_type_clear(uint16_t dst);
int tmes_device_mac(uint16_t dst, uint16_t *category, uint8_t *mac);
int tmes_light_mode(uint16_t dst, uint8_t *speed, uint8_t *temperature, uint8_t *global, uint8_t *mode, uint8_t *id, uint8_t *internal);
int tmes_light_mode_custom_list(uint16_t dst, uint8_t *ids, uint8_t *size);
int tmes_light_mode_custom(uint16_t dst, uint8_t id, uint8_t *color, uint8_t *count);
int tmes_light_mode_custom_add(uint16_t dst, uint8_t id, uint8_t idx, uint8_t *color);
int tmes_light_mode_custom_delete(uint16_t dst, uint8_t id);
int tmes_light_mode_global_write(uint16_t dst, SRTransition transition, uint8_t enable);
int tmes_light_mode_speed_write(uint16_t dst, uint8_t speed);
int tmes_light_mode_temperature_write(uint16_t dst, uint8_t temperature);
int tmes_light_mode_preinstall_write(uint16_t dst, SRPreinstallLightMode preinstall);
int tmes_light_mode_custom_run(uint16_t dst, SRTransition transition);
int tmes_light_scene_list(uint16_t dst, uint8_t *scenes, uint8_t *size);
int tmes_light_scene(uint16_t dst, uint8_t idx, uint8_t *scene);
int tmes_light_scene_save(uint16_t dst, uint8_t scene, uint16_t delay);
int tmes_light_scene_delete(uint16_t dst, uint8_t scene);
int tmes_light_scene_run(uint16_t dst, uint8_t scene);
int tmes_light_channel_enable(uint16_t dst, uint8_t channels, uint8_t mask);
int tmes_light_channel_action(uint16_t dst, uint8_t channel, uint8_t action, uint8_t flag);
int tmes_light_channel_speed_action(uint16_t dst, uint8_t action);
int tmes_light_preinstall_stop(uint16_t dst);
int tmes_light_custom_stop(uint16_t dst);
int tmes_light_preinstall_run(uint16_t dst, SRPreinstallLightMode mode, uint8_t global, uint8_t speed, uint8_t temperature);
int tmes_light_custom_run(uint16_t dst, uint8_t mode, SRTransition transition, uint8_t global, uint8_t speed, uint8_t temperature);
int tmes_light_move_to_level(uint16_t dst, uint8_t bright, uint8_t r, uint8_t g, uint8_t b, uint8_t ct, uint16_t time);
int tmes_light_pwm_write(uint16_t dst, uint16_t freq);
int tmes_light_pwm(uint16_t dst, uint16_t *freq);
int tmes_light_pwm_deadtime_write(uint16_t dst, uint8_t interval);
int tmes_light_pwm_deadtime(uint16_t dst, uint8_t *interval);
int tmes_light_bright_min_write(uint16_t dst, uint8_t percent);
int tmes_light_bright_min(uint16_t dst, uint8_t *percent);
int tmes_light_bright_gamma_write(uint16_t dst, uint8_t gamma);
int tmes_light_bright_gamma(uint16_t dst, uint8_t *gamma);
int tmes_light_onoff_duration_write(uint16_t dst, uint16_t duration);
int tmes_light_onoff_duration(uint16_t dst, uint16_t *duration);
int tmes_device_status_error(uint16_t dst, uint8_t *error);
int tmes_device_status_running(uint16_t dst, uint8_t *bright, uint8_t *rgb, uint8_t *temperature, uint8_t *tail);
int tmes_light_status_power(uint16_t dst, SRCategory *category, uint16_t *voltage);
int tmes_switch_type_write(uint16_t dst, SRSwitchType type);
int tmes_switch_type(uint16_t dst, SRSwitchType *type);
int tmes_uart_tx_type_write(uint16_t dst, uint8_t type);
int tmes_uart_tx_type(uint16_t dst, uint8_t *type);
int tmes_light_schedule_write(uint16_t dst, uint8_t hour, uint8_t minute, uint8_t luminance, uint8_t *rgb, uint8_t ct, uint8_t cten);
int tmes_light_schedule_start(uint16_t dst);
int tmes_light_schedule_stop(uint16_t dst);
int tmes_light_schedule(uint16_t dst, uint8_t idx, uint8_t *hour, uint8_t *minute, uint8_t *bright, uint8_t *rgb, uint8_t *ct, uint8_t *cten);
int tmes_light_preinstall_run_32(uint32_t group, SRPreinstallLightMode mode, uint8_t global, uint8_t speed, uint8_t temperature);
int tmes_light_custom_run_32(uint32_t group, uint8_t mode, SRTransition transition, uint8_t global, uint8_t speed, uint8_t temperature);
int tmes_light_group_get_32(uint16_t dst, uint8_t *enable, uint32_t *group);
int tmes_device_pair_enable(uint16_t dst);
int tmes_device_return_to_factory(uint16_t dst);
int tmes_remoter_group_write(uint16_t dst, uint8_t map, uint8_t *mac);
int tmes_remoter_group_unset(uint16_t dst, uint8_t map, uint8_t *mac);
int tmes_button_group_write(uint16_t dst, uint8_t map, uint8_t *mac);
int tmes_button_group_unset(uint16_t dst, uint8_t map, uint8_t *mac);
int tmes_sensor_group_write(uint16_t dst, uint8_t sensor, uint8_t *group);
int tmes_sensor_group_unset(uint16_t dst, uint8_t sensor);
int tmes_sensor_group(uint16_t dst, uint8_t sensor, uint32_t *group);
int tmes_light_button_function_write(uint16_t dst, uint8_t type, uint8_t key, uint8_t press, uint8_t func, uint8_t switch_model);
int tmes_linkage_condition_check_write(uint8_t dst, uint8_t id, uint8_t enable, uint8_t condition, uint8_t *parameters, uint8_t size);
int tmes_gps_write(uint16_t dst, uint32_t longitude, uint32_t latitude);
int tmes_gps(uint16_t dst, uint32_t *longitude, uint32_t *latitude);
int tmes_sun_onoff_write(uint16_t dst, uint8_t sunrise, uint8_t enable, uint8_t onoff, uint16_t duration);
int tmes_sun_scene_standard_write(uint16_t dst, uint8_t sunrise, uint8_t enable, uint8_t scene);
int tmes_sun_scene_custom_write(uint16_t dst, uint8_t sunrise, uint8_t enable, uint8_t scene);
int tmes_sun_color_write(uint16_t dst, uint8_t sunrise, uint8_t enable, uint8_t bright, uint8_t *rgb, uint8_t ct, uint16_t duration);
int tmes_sun(uint16_t dst, uint8_t sunrise, SRSunSetting *setting);
int tmes_sun_clear(uint16_t dst, uint8_t sunrise);
int tmes_sun_enable(uint16_t dst, uint8_t sunrise);
int tmes_sun_disable(uint16_t dst, uint8_t sunrise);
int tmes_local_time_zone_write(uint16_t dst, uint8_t east, uint8_t hour, uint8_t minute);
int tmes_local_time_zone(uint16_t dst, uint8_t *east, uint8_t *now_hour, uint8_t *now_minute, uint8_t *sunrise_hour, uint8_t *sunrise_minute, uint8_t *sunset_hour, uint8_t *sunset_minute);
int tmes_button_control(uint16_t dst, uint8_t type, uint8_t key, uint32_t id);

#endif
