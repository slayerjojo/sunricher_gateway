#ifndef __SR_LAYER_DEVICE_H__
#define __SR_LAYER_DEVICE_H__

#include "env.h"
#include "cJSON.h"

void sld_init(void);
void sld_update(void);

const char *sld_id_create(char *id);
cJSON *sld_load(const char *device);
void sld_save(const char *device, cJSON *value);
void sld_create(const char *device, const char *name, const char *category, const char *connections[], cJSON *attributes, cJSON *capabilities);
void sld_delete(const char *device);
void sld_profile_report(const char *device, const char *id);
cJSON *sld_property_load(const char *device);
void sld_property_set(const char *device, const char *ns, const char *name, cJSON *value);
void sld_property_report(const char *device, const char *opcode);

#endif