#ifndef __INTERFACE_KV_H__
#define __INTERFACE_KV_H__

#include "env.h"

#include "driver_kv_linux.h"

#define kv_init linux_kv_init
#define kv_delete linux_kv_delete
#define kv_set linux_kv_set
#define kv_size linux_kv_size
#define kv_get linux_kv_get
#define kv_exist linux_kv_exist
#define kv_acquire linux_kv_acquire
#define kv_free linux_kv_free
#define kv_list_add linux_kv_list_add
#define kv_list_remove linux_kv_list_remove
#define kv_list_iterator(k, it) linux_kv_list_iterator(k, it)
#define kv_list_iterator_release linux_kv_list_iterator_release

#endif
