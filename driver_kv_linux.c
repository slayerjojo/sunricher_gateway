#include "driver_kv_linux.h"
#include <stdio.h>
#include "interface_os.h"
#include "interface_kv.h"
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

void linux_kv_list_add(const char *key, const char *item)
{
    int size = os_strlen(item) + 1;
    char *list = kv_acquire(key, &size);
    int pos = 0;
    while (pos < size)
    {
        if (!os_strcmp(list + pos, item))
            break;
        pos += os_strlen(list + pos) + 1;
    }
    if (pos >= size)
    {
        os_strcpy(list + size, item);
        kv_set(key, list, size + os_strlen(item) + 1);
    }
    kv_free(list);
}

void linux_kv_list_remove(const char *key, const char *item)
{
    int size = 0;
    char *list = kv_acquire(key, &size);
    int pos = 0;
    while (pos < size)
    {
        if (!os_strcmp(list + pos, item))
            break;
        pos += os_strlen(list + pos) + 1;
    }
    if (pos >= size)
    {
        if (size > os_strlen(item) + 1)
        {
            os_memcpy(list + pos, list + pos + os_strlen(item) + 1, size - os_strlen(item) - 1);
            kv_set(key, list, size - os_strlen(item) - 1);
        }
        else
        {
            kv_delete(key);
        }
    }
    kv_free(list);
}

typedef struct
{
    char *list;
    uint32_t pos;
    uint32_t size;
}KVListIterator;

const char *linux_kv_list_iterator(const char *key, void **iterator)
{
    KVListIterator *it = 0;
    if (!iterator)
        return 0;
    if (!*iterator)
    {
        *iterator = os_malloc(sizeof(KVListIterator));
        if (!*iterator)
        {
            SigmaLogError(0, 0, "out of memory");
            return 0;
        }
        it = *iterator;
        it->pos = 0;
        it->size = 0;
        it->list = kv_acquire(key, &(it->size));
    }
    it = *iterator;
    if (!it->list)
        return 0;
    if (it->pos >= it->size)
    {
        if (it->list)
            kv_free(it->list);
        it->list = 0;
        os_free(it);
        it = 0;
        return 0;
    }
    char *ret = it->list + it->pos;
    it->pos += os_strlen(ret) + 1;
    return ret;
}

void linux_kv_list_iterator_release(void *iterator)
{
    if (!iterator)
        return;

    if (iterator)
        os_free(iterator);
}
