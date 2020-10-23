#ifndef __DRIVER_KV_LINUX_H__
#define __DRIVER_KV_LINUX_H__

#include "env.h"

void linux_kv_delete(const char *key);
int linux_kv_set(const char *key, const uint8_t *value, uint32_t size);
uint32_t linux_kv_size(const char *key);
int linux_kv_get(const char *key, uint8_t *value, uint32_t size);
uint8_t *linux_kv_acquire(const char *key, uint32_t *size);
void linux_kv_free(void *v);

#endif
