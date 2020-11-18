#include "command_option.h"

#if defined(PLATFORM_LINUX)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "sigma_log.h"

struct CommandOption
{
    CommandOptionCallback callback;
    const char *tip;
    const char *name;
};

static struct option *_options = 0;
static struct CommandOption *_infos = 0;
static char _optString[255] = {0};
static uint32_t _size = 0;

void register_command_option_argument(const char *optname, CommandOptionCallback handler, const char *tip)
{
    if (!_options)
        _options = (struct option *)calloc(1, sizeof(struct option));
    if (!_infos)
        _infos = (struct CommandOption *)calloc(1, sizeof(struct CommandOption));

    _size++;
    _options = (struct option *)realloc(_options, sizeof(struct option) * (_size + 1));
    if (!_options)
    {
        SigmaLogError(0, 0, "out of memory");
        return;
    }
    _infos = (struct CommandOption *)realloc(_infos, sizeof(struct CommandOption) * _size);
    if (!_infos)
    {
        SigmaLogError(0, 0, "out of memory");
        return;
    }

    char key = *optname;

    struct option *opt = _options + _size - 1;
    strncat(_optString, &key, 1);
    strcat(_optString, ":");
    opt->name = optname;
    opt->has_arg = required_argument;
    opt->flag = 0;
    opt->val = key;

    opt++;

    opt->name = 0;
    opt->has_arg = 0;
    opt->flag = 0;
    opt->val = 0;

    struct CommandOption *info = _infos + _size - 1;
    info->callback = handler;
    info->name = optname;
    info->tip = tip;
}

void register_command_option(const char *optname, CommandOptionCallback handler, const char *tip)
{
    if (!_options)
        _options = (struct option *)calloc(1, sizeof(struct option));
    if (!_infos)
        _infos = (struct CommandOption *)calloc(1, sizeof(struct CommandOption));

    _size++;
    _options = (struct option *)realloc(_options, sizeof(struct option) * (_size + 1));
    if (!_options)
    {
        SigmaLogError(0, 0, "out of memory");
        return;
    }
    _infos = (struct CommandOption *)realloc(_infos, sizeof(struct CommandOption) * _size);
    if (!_infos)
    {
        SigmaLogError(0, 0, "out of memory");
        return;
    }

    char key = *optname;

    struct option *opt = _options + _size - 1;
    strncat(_optString, &key, 1);
    opt->name = optname;
    opt->has_arg = no_argument;
    opt->flag = 0;
    opt->val = key;

    opt++;

    opt->name = 0;
    opt->has_arg = 0;
    opt->flag = 0;
    opt->val = 0;

    struct CommandOption *info = _infos + _size - 1;
    info->callback = handler;
    info->name = optname;
    info->tip = tip;
}

int parse_command_option(int argc, char * const argv[])
{
    int optVal = 0;
    while ((optVal = getopt_long(argc, argv, _optString, _options, 0)) != -1)
    {
        uint32_t i = 0;
        if (optVal == (int)'?')
        {
            printf("\n");
            for (i = 0; i < _size; i++)
            {
                printf("%s : %s\n", (_infos + i)->name, (_infos + i)->tip);
            }
            printf("\n");
            return -1;
        }

        for (i = 0; i < _size; i++)
        {
            if ((_options + i)->val == optVal)
                break;
        }

        struct CommandOption *info = _infos + i;
        int ret = -1;
        if (info)
            ret = info->callback(optarg);
        if (ret < 0)
            return ret;
    }

    _options = (struct option *)realloc(_options, sizeof(struct option));
    _options->name = 0;
    _options->has_arg = 0;
    _options->flag = 0;
    _options->val = 0;

    _infos = (struct CommandOption *)realloc(_infos, sizeof(struct CommandOption));

    _optString[0] = 0;
    _size = 0;
    return 0;
}

#endif
