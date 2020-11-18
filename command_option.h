#ifndef __COMMAND_OPTION_H__
#define __COMMAND_OPTION_H__

#include "env.h"

#if defined(PLATFORM_LINUX)

#define COMMAND_OPTION_PARSE(option, val, separator)    \
const char *val = option;                               \
do                                                      \
{                                                       \
    char *split = strstr(option, separator);            \
    if (!split)                                         \
        return -1;                                      \
    *split = 0;                                         \
    val = option;                                       \
    option = split + 1;                                 \
}                                                       \
while (0)

typedef int (*CommandOptionCallback)(char *option);

void register_command_option_argument(const char *option, CommandOptionCallback handler, const char *tip);
void register_command_option(const char *option, CommandOptionCallback handler, const char *tip);
int parse_command_option(int argc, char * const argv[]);

#endif

#endif
