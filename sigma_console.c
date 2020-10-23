#include "sigma_console.h"
#include "sigma_log.h"
#include "interface_network.h"
#include "interface_os.h"
#include <stdarg.h>

#define PORT_CONSOLE 8000
#define MAX_CONSOLE_BUFFER 1024

enum
{
    SC_STATE_CONSOLE_INIT = 0,
    SC_STATE_CONSOLE_ACCEPT,
    SC_STATE_CONSOLE_SERVICE,
};

static uint8_t _state = SC_STATE_CONSOLE_INIT;
static uint16_t _port = 0;
static int _fp_server = -1;
static int _fp_client = -1;
static uint8_t _buffer[MAX_CONSOLE_BUFFER] = {0};
static uint32_t _size = 0;
static ConsoleHandler _handler = 0;

void sigma_console_init(uint16_t port, ConsoleHandler handler)
{
    _state = SC_STATE_CONSOLE_INIT;
    _fp_server = -1;
    _port = port;
    _handler = handler;
}

void sigma_console_update(void)
{
    if (SC_STATE_CONSOLE_INIT == _state)
    {
        if (_fp_server >= 0)
            network_tcp_close(_fp_server);
        _fp_server = network_tcp_server(_port);
        _state = SC_STATE_CONSOLE_ACCEPT;
        if (_fp_server < 0)
        {
            SigmaLogError("console create failed");
            _state = SC_STATE_CONSOLE_INIT;
        }
    }
    if (SC_STATE_CONSOLE_ACCEPT == _state)
    {
        uint8_t ip[4] = {0};
        uint16_t port = 0;
        _fp_client = network_tcp_accept(_fp_server, ip, &port);
        if (_fp_client < -1)
        {
            SigmaLogError("console accept failed");
            _state = SC_STATE_CONSOLE_INIT;
        }
        else if (_fp_client >= 0)
        {
            SigmaLogAction("console accepted");
            _size = 0;
            _state = SC_STATE_CONSOLE_SERVICE;
        }
    }
    if (SC_STATE_CONSOLE_SERVICE == _state)
    {
        if (_size >= MAX_CONSOLE_BUFFER - 1)
            _size = 0;
        int ret = network_tcp_recv(_fp_client, _buffer + _size, MAX_CONSOLE_BUFFER - _size - 1);
        if (!ret)
            return;
        if (ret < 0)
        {
            SigmaLogError("console connection broken");
            network_tcp_close(_fp_client);
            _state = SC_STATE_CONSOLE_ACCEPT;
            return;
        }
        _size += ret;
        _buffer[_size] = 0;
        char *start = (char *)_buffer;
        while (start)
        {
            char *end = os_strstr(start, "\n");
            if (!end)
                break;
            *end = 0;
            if (*(end - 1) == '\r')
                *(end - 1) = 0;
            const char *parameters[32] = {0};
            int count = 0;
            char *parameter = os_strstr(start, "|");
            while (parameter)
            {
                *parameter = 0;
                parameters[count++] = parameter + 1;
                if (count >= 32)
                    break;
                parameter = os_strstr(parameter + 1, "|");
            }
            if (!os_strcmp(start, "exit") || !os_strcmp(start, "quit"))
            {
                SigmaLogAction("console connection broken");
                network_tcp_close(_fp_client);
                _state = SC_STATE_CONSOLE_ACCEPT;
            }
            else if (_handler)
            {
                SigmaLogAction("console:%s", start);
                _handler(start, parameters, count);
            }
            start = end + 1;
        }
        if (((uint64_t)start > (uint64_t)_buffer) && ((uint64_t)start - (uint64_t)_buffer) < _size)
            os_memmove(_buffer, start, _size - (uint64_t)start + (uint64_t)_buffer);
        _size -= (uint64_t)start - (uint64_t)_buffer;
    }
}

void sigma_console_write(const char *fmt, ...)
{
    if (SC_STATE_CONSOLE_SERVICE != _state)
    {
        SigmaLogError("console state error");
        return;
    }
    char output[1024] = {0};

    va_list args;
    va_start(args, fmt);
    vsprintf(output + strlen(output), fmt, args);
    va_end(args);

    strcat(output, "\n");

    network_tcp_send(_fp_client, output, strlen(output));
}

void sigma_console_close(void)
{
    if (SC_STATE_CONSOLE_SERVICE != _state)
    {
        SigmaLogError("console state error");
        return;
    }
    SigmaLogError("console connection close");
    network_tcp_close(_fp_client);
    _state = SC_STATE_CONSOLE_ACCEPT;
    return;
}
