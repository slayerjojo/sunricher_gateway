#ifndef __INTERFACE_KV_H__
#define __INTERFACE_KV_H__

#include "env.h"

#include "driver_kv_linux.h"

#define kv_delete linux_kv_delete
#define kv_set linux_kv_set
#define kv_size linux_kv_size
#define kv_get linux_kv_get
#define kv_exist linux_kv_exist
#define kv_acquire linux_kv_acquire
#define kv_free linux_kv_free

#endif
