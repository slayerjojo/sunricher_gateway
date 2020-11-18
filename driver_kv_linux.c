#include "driver_kv_linux.h"
#include <stdio.h>
#include "interface_os.h"
#include "sigma_log.h"

const char *_root = "./kv/%s";

void linux_kv_delete(const char *key)
{
    char path[128] = {0};
    sprintf(path, _root, key);
    remove(path);
}

int linux_kv_set(const char *key, const void *value, uint32_t size)
{
    char path[128] = {0};
    sprintf(path, _root, key);
    FILE *fp = fopen(path, "wb");
    if (!fp)
    {
        SigmaLogError(0, 0, "create file %s failed.", path);
        return -1;
    }
    fwrite(value, size, 1, fp);
    fclose(fp);
    return size;
}

uint32_t linux_kv_size(const char *key)
{
    char path[128] = {0};
    sprintf(path, _root, key);
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fclose(fp);
    return size;
}

int linux_kv_exist(const char *key)
{
    char path[128] = {0};
    sprintf(path, _root, key);
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;
    fclose(fp);
    return 1;
}

int linux_kv_get(const char *key, void *value, uint32_t size)
{
    char path[128] = {0};
    sprintf(path, _root, key);
    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        SigmaLogError(0, 0, "file %s not found.", path);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size > length)
        size = length;
    fread(value, size, 1, fp);
    fclose(fp);
    return size;
}

uint8_t *linux_kv_acquire(const char *key, uint32_t *size)
{
    char path[128] = {0};
    sprintf(path, _root, key);
    int extends = 0;
    if (size)
    {
        extends = *size;
        *size = 0;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        SigmaLogError(0, 0, "file %s not found.", path);
        if (extends)
            return os_malloc(extends);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    uint32_t length = ftell(fp);
    if (size)
        *size = length;
    fseek(fp, 0, SEEK_SET);
    uint8_t *value = os_malloc(length + 1 + extends);
    if (!value)
    {
        SigmaLogError(0, 0, "out of memory");
        fclose(fp);
        return 0;
    }
    fread(value, length, 1, fp);
    value[length] = 0;
    fclose(fp);
    return value;
}

void linux_kv_free(void *v)
{
    os_free(v);
}
