#include "sigma_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#if defined(PLATFORM_ANDROID)
#include <android/log.h>
#include "hex.h"
#endif

static uint8_t color_hash(const char *s)
{
    uint32_t hash = 0;
    int i = strlen(s);
    while (i--)
    {
        hash <<= 1;
        hash += s[i];
    }
    return ((hash >> 1) % 9) + 1;
}

void sigma_log_println(const char *file, uint32_t line, const char *func, unsigned char level, const void *buffer, size_t size, const char *fmt, ...)
{
    uint32_t i = 0;

    file += strlen(__FILE__) - strlen("sigma_log.c");

#ifdef PLATFORM_ANDROID
    int prio = ANDROID_LOG_DEBUG;
    if (LOG_LEVEL_ERROR == level)
        prio = ANDROID_LOG_ERROR;
    else if (LOG_LEVEL_ACTION == level)
        prio = ANDROID_LOG_INFO;

    char format[1024] = {0};
    sprintf(format, "%d/%s/%s", line, func, fmt);
    if (size)
    {
        i = strlen(format);
        if (size > (1024 - i - 1) / 2)
            size = (1024 - i - 1) / 2;
        bin2hex(format + strlen(format), buffer, size);
    }
    
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(prio, file, format, args);
    va_end(args);
#else
    time_t now = time(0);
    struct tm *t = localtime(&now);

    static uint32_t seq = 0;

    printf("\033[0m%02d:%02d:%02d|%d|%s:%d|\033[3%dm%s\033[0m|\033[%dmlv:%d|",
        t->tm_hour, t->tm_min, t->tm_sec, 
        seq++, 
        file, line, color_hash(func), func,
        (!level ? 31 : 0),
        level);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    for (i = 0; i < size; i++)
        printf("%02x", *(((unsigned char *)buffer) + i));

    printf("\033[0m\n");

    fflush(stdout);
#endif
}
