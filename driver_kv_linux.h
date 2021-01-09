#ifndef __DRIVER_KV_LINUX_H__
#define __DRIVER_KV_LINUX_H__

#include "env.h"

void linux_kv_init(const char *root);
void linux_kv_delete(const char *key);
int linux_kv_set(const char *key, const void *value, uint32_t size);
uint32_t linux_kv_size(const char *key);
int linux_kv_exist(const char *key);
int linux_kv_get(const char *key, void *value, uint32_t size);
uint8_t *linux_kv_acquire(const char *key, uint32_t *size);
void linux_kv_free(void *v);
void linux_kv_list_add(const char *key, const char *item);
void linux_kv_list_remove(const char *key, const char *item);
const char *linux_kv_list_iterator(const char *key, void **iterator);
void linux_kv_list_iterator_release(void *iterator);

#endif
