#include "driver_linux.h"

#if defined(PLATFORM_LINUX)

#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include "aes.h"

uint32_t linux_ticks(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

uint32_t linux_ticks_from(uint32_t start)
{
	uint32_t now;

    if (!start)
        return 0;
    
    now = os_ticks();
    if (now >= start)
        return now - start;
    return 0xffffffffL - start + now;
}

int linux_crypto_encrypt(void *out, const void *in, uint32_t size, const uint8_t *shared)
{
    return AES128CBCEncrypt(in, out, size, shared, shared);
}

int linux_crypto_decrypt(void *out, const void *in, uint32_t size, const uint8_t *shared)
{
    return AES128CBCDecrypt(in, out, size, shared, shared);
}


#endif
