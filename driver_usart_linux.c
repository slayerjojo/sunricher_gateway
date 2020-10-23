#include "driver_usart_linux.h"

#if defined(PLATFORM_LINUX)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "interface_os.h"
#include "sigma_log.h"

//https://www.ibm.com/developerworks/cn/linux/l-serials/index.html

typedef struct _usart_runtime
{
    struct _usart_runtime *_next;

    int fp;
    uint8_t usart;
}UsartRuntime;

static UsartRuntime *_runtimes = 0;

void linux_usart_init(void)
{
    _runtimes = 0;
}

void linux_usart_update(void)
{
}

int linux_usart_open(uint8_t usart, uint32_t baud, uint32_t databit, char parity, uint8_t stopbits)
{
    int fp = -1;
    if (0 == usart)
    {
        fp = open("/dev/ttyS3", O_RDWR);
    }
    if (-1 == fp)
    {
        SigmaLogError("serial open error");
        return -1;
    }

    struct termios opt;
    if (tcgetattr(fp, &opt))
    {
        SigmaLogError("tcgetattr error");
        return -1;
    }
    if (115200 == baud)
    {
        cfsetispeed(&opt, B115200);
        cfsetospeed(&opt, B115200);
    }
    else
    {
        SigmaLogError("baud %u not support", baud);
        return -1;
    }
    tcflush(fp, TCIOFLUSH);

    opt.c_cflag &= ~CSIZE; 
    switch (databit)
    {   
        case 7:
            opt.c_cflag |= CS7; 
            break;
        case 8:
            opt.c_cflag |= CS8;
            break;   
        default:    
            SigmaLogError("databits %d not support", databit);
            return -1;  
    }
    switch (parity) 
    {   
        case 'n':
        case 'N':    
            opt.c_cflag &= ~PARENB;   /* Clear parity enable */
            opt.c_iflag &= ~INPCK;     /* Enable parity checking */ 
            break;  
        case 'o':   
        case 'O':     
            opt.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/  
            opt.c_iflag |= INPCK;             /* Disnable parity checking */ 
            break;  
        case 'e':  
        case 'E':   
            opt.c_cflag |= PARENB;     /* Enable parity */    
            opt.c_cflag &= ~PARODD;   /* 转换为偶效验*/     
            opt.c_iflag |= INPCK;       /* Disnable parity checking */
            break;
        case 'S': 
        case 's':  /*as no parity*/   
            opt.c_cflag &= ~PARENB;
            opt.c_cflag &= ~CSTOPB;
            break;  
        default:   
            SigmaLogError("parity '%d' Unsupported", parity);    
            return -1;  
    }
    /* 设置停止位*/  
    switch (stopbits)
    {   
        case 1:    
            opt.c_cflag &= ~CSTOPB;  
            break;  
        case 2:    
            opt.c_cflag |= CSTOPB;  
            break;
        default:    
            SigmaLogError("stopbits %d Unsupported", stopbits);  
            return -1; 
    }
    tcflush(fp, TCIFLUSH);
    opt.c_cc[VTIME] = 0; /* 设置超时15 seconds*/   
    opt.c_cc[VMIN] = 0; /* Update the opt and do it NOW */

    opt.c_cflag |= (CLOCAL | CREAD);
    opt.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);  /*Input*/
    opt.c_oflag &= ~OPOST;   /*Output*/
    opt.c_oflag &= ~(ONLCR | OCRNL); 
    opt.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|INLCR|IGNCR|ICRNL|IXON);
    if (tcsetattr(fp, TCSANOW, &opt))
    {
        SigmaLogError("tcsetattr error");
        return -1;
    }

    UsartRuntime *runtime = (UsartRuntime *)calloc(sizeof(UsartRuntime), 1);
    if (!runtime)
    {
        SigmaLogError("out of memory");
        return -1;
    }
    runtime->fp = fp;
    runtime->usart = usart;
    runtime->_next = _runtimes;
    _runtimes = runtime;

    int flags = fcntl(fp, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fp, F_SETFL, flags);

    SigmaLogAction("open usart (baud:%u databit:%u) successed.", baud, databit);

    return 1;
}

void linux_usart_close(uint8_t usart)
{
    UsartRuntime *runtime = _runtimes, *prev = 0;
    while (runtime)
    {
        if (runtime->usart == usart)
            break;
        prev = runtime;
        runtime = runtime->_next;
    }
    if (runtime)
    {
        if (prev)
            prev->_next = runtime->_next;
        else
            _runtimes = runtime->_next;

        close(runtime->fp);
        free(runtime);
    }
}

int linux_usart_write(uint8_t usart, const uint8_t *buffer, uint16_t size)
{
    UsartRuntime *runtime = _runtimes;
    while (runtime)
    {
        if (runtime->usart == usart)
            break;
        runtime = runtime->_next;
    }
    if (!runtime)
        return 0;
    return write(runtime->fp, buffer, size);
}

int linux_usart_read(uint8_t usart, const uint8_t *buffer, uint16_t size)
{
    UsartRuntime *runtime = _runtimes;
    while (runtime)
    {
        if (runtime->usart == usart)
            break;
        runtime = runtime->_next;
    }
    if (!runtime)
        return 0;
    return read(runtime->fp, buffer, size);
}

#endif
