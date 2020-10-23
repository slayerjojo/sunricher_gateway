#ifndef __INTERFACE_CRYPTO_H__
#define __INTERFACE_CRYPTO_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"

#if defined(PLATFORM_LINUX)

#include "driver_linux.h"

#define crypto_encrypt linux_crypto_encrypt
#define crypto_decrypt linux_crypto_decrypt

#endif

#ifdef __cplusplus
}
#endif

#endif
