#include "driver_linux.h"
#include "sigma_log.h"

#if defined(PLATFORM_LINUX)

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

static uint8_t _ip_addr[] = {127, 0, 0, 1};
static uint8_t _mask_addr[] = {255, 255, 255, 0};
static uint8_t _nat_addr[] = {0, 0, 0, 0};
static uint8_t _mac_addr[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void linux_network_init(void)
{
    struct ifreq *ifr = 0;
    int s = -1, i;
    int numif;

    do {
        struct ifconf ifc;

        memset(&ifc, 0, sizeof(ifc));
        ifc.ifc_ifcu.ifcu_req = 0;
        ifc.ifc_len = 0;

        if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
        {
            SigmaLogError("failed!");
            break;
        }

        if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
        {
            SigmaLogError("failed!");
            break;
        }

        if ((ifr = (struct ifreq *) malloc(ifc.ifc_len)) == 0)
        {
            SigmaLogError("failed!");
            break;
        }

        ifc.ifc_ifcu.ifcu_req = ifr;

        if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
        {
            SigmaLogError("failed!");
            break;
        }

        numif = ifc.ifc_len / sizeof(struct ifreq);

        struct ifreq *r = 0;
        for (i = 0; i < numif; i++)
        {
            r = &ifr[i];
            if (!strcmp(r->ifr_name, "eth0.2"))
                break;
        }

        if (i < numif)
        {
            if (ioctl(s, SIOCGIFHWADDR, r) < 0)
            {
                SigmaLogError("failed!");
                break;
            }

            memcpy(_mac_addr, r->ifr_hwaddr.sa_data, 6);
            
            if (ioctl(s, SIOCGIFADDR, r) < 0)
            {
                SigmaLogError("failed!");
                break;
            }

            struct sockaddr_in *sin = (struct sockaddr_in *)&r->ifr_addr;

            memcpy(_ip_addr, &sin->sin_addr.s_addr, 4);
            
            if (ioctl(s, SIOCGIFNETMASK, r) < 0)
            {
                SigmaLogError("failed!");
                break;
            }

            memcpy(_mask_addr, &sin->sin_addr.s_addr, 4);
            
            if (ioctl(s, SIOCGIFBRDADDR, r) < 0)
            {
                SigmaLogError("failed!");
                break;
            }

            memcpy(_nat_addr, &sin->sin_addr.s_addr, 4);
        }
    } while (0);

    if (s >= 0)
        close(s);

    if (ifr)
        free(ifr);
}

int linux_network_tcp_server(uint16_t port)
{
    int fp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fp < 0)
    {
        SigmaLogError("errno:%d error:%s", errno, strerror(errno));
        return -1;
    }

    int sock_opts = 1;
    setsockopt(fp, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_opts, sizeof(sock_opts));
    sock_opts = fcntl(fp, F_GETFL);
    sock_opts |= O_NONBLOCK;
    fcntl(fp, F_SETFL, sock_opts);

    struct sockaddr_in saLocal;
    saLocal.sin_family = AF_INET;
    saLocal.sin_port = htons(port);
    saLocal.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fp, (struct sockaddr *)&saLocal, sizeof(struct sockaddr_in)) < 0)
    {
        SigmaLogError("errno:%d error:%s", errno, strerror(errno));
        close(fp);
        return -1;
    }
    if (listen(fp, 5) < 0)
    {
        SigmaLogError("errno:%d error:%s", errno, strerror(errno));
        close(fp);
        return -1;
    }
    return fp;
}

int linux_network_tcp_accept(int fp, uint8_t *ip, uint16_t *port)
{
    struct sockaddr_in sa;
    int salen = sizeof(sa);
    int conn = accept(fp, (struct sockaddr *)&sa, (socklen_t *)&salen);
    if (conn < 0)
    {
        if (EAGAIN == errno || EINTR == errno || EWOULDBLOCK == errno)
            return -1;
        SigmaLogError("error ret:%d(%s)", conn, strerror(errno));
        return -2;
    }
    *port = sa.sin_port;
    ip[0] = (uint8_t)(sa.sin_addr.s_addr >> 24);
    ip[1] = (uint8_t)(sa.sin_addr.s_addr >> 16);
    ip[2] = (uint8_t)(sa.sin_addr.s_addr >> 8);
    ip[3] = (uint8_t)(sa.sin_addr.s_addr >> 0);
    return conn;
}

int linux_network_tcp_client(const char *host, uint16_t port)
{
    int fp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fp < 0)
    {
        SigmaLogError("create socket failed:(%d)%s", errno, strerror(errno));
        return -1;
    }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    struct hostent *ht = gethostbyname(host);
    if (!ht)
    {
        SigmaLogError("gethostbyname %s failed.(%s)", host, strerror(errno));
        return -2;
    }
    memcpy(&sa.sin_addr, ht->h_addr, ht->h_length);

    int sock_opts = 1;
    setsockopt(fp, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_opts, sizeof(sock_opts));
    sock_opts = fcntl(fp, F_GETFL);
    sock_opts |= O_NONBLOCK;
    fcntl(fp, F_SETFL, sock_opts);

    int ret = connect(fp, (struct sockaddr *)&sa, sizeof(sa));
    if (ret < 0)
    {
        if (EINPROGRESS != errno && errno != EALREADY && errno != EISCONN)
        {
            close(fp);
            SigmaLogError("%d connect failed. %s", fp, strerror(errno));
            return -1;
        }
    }
    return fp;
}

int linux_network_tcp_connected(int fp)
{
    fd_set fsw;
    fd_set fse;

    FD_ZERO(&fsw);
    FD_SET(fp, &fsw);

    FD_ZERO(&fse);
    FD_SET(fp, &fse);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int ret = select(fp + 1, NULL, &fsw, &fse, (struct timeval *)&tv);
    if (ret < 0)
    {
        SigmaLogError("fp:%d errno:%u error:%s", fp, errno, strerror(errno));
        return -1;
    }
    if (!ret)
        return 0;
    if (FD_ISSET(fp, &fse))
    {
        SigmaLogError("fp:%d errno:%u error:%s", fp, errno, strerror(errno));
        return -1;
    }
    if (!FD_ISSET(fp, &fsw))
        return 0;

    int error = 0;
    socklen_t length = sizeof(int);
    if (getsockopt(fp, SOL_SOCKET, SO_ERROR, (void *)&error, &length) < 0)
    {
        SigmaLogError("fp:%d errno:%u error:%s", fp, errno, strerror(errno));
        return -1;
    }
    if (error)
    {
        SigmaLogError("errno:%d error:%s", error, strerror(error));
        return -1;
    }
    return 1;
}

int linux_network_tcp_recv(int fp, void *buffer, uint32_t size)
{
    int ret = recv(fp, buffer, size, MSG_DONTWAIT);
    if (!ret)
    {
        SigmaLogError("%d closed by remote", fp);
        return -1;
    }
    if (ret < 0)
    {
        if (ENOTCONN == errno || EAGAIN == errno || EWOULDBLOCK == errno)
            return 0;
        SigmaLogError("%d closed by error(%u:%s)", fp, errno, strerror(errno));
        return -1;
    }
    return ret;
}

int linux_network_tcp_send(int fp, const void *buffer, uint32_t size)
{
    int ret = send(fp, buffer, size, 0);
    if (!ret)
    {
        SigmaLogError("%d closed by remote", fp);
        return -1;
    }
    if (ret < 0)
    {
        if(EAGAIN == errno || EWOULDBLOCK == errno)
            return 0;
        SigmaLogError("%d closed by error(%s)", fp, strerror(errno));
        return -1;
    }
    return ret;
}

int linux_network_udp_create(uint16_t port)
{
    int fp = socket(AF_INET, SOCK_DGRAM, 0);

    int sock_opts = 1;
    setsockopt(fp, SOL_SOCKET, SO_BROADCAST, (void*)&sock_opts, sizeof(sock_opts));
    sock_opts = 1;
    setsockopt(fp, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_opts, sizeof(sock_opts));
    sock_opts = 1;
    setsockopt(fp, SOL_SOCKET, SO_REUSEPORT, (void*)&sock_opts, sizeof(sock_opts));
    sock_opts = fcntl(fp, F_GETFL);
    sock_opts |= O_NONBLOCK;
    fcntl(fp, F_SETFL, sock_opts);

    struct sockaddr_in saLocal;
    saLocal.sin_family = AF_INET;
    saLocal.sin_port = htons(port);
    saLocal.sin_addr.s_addr = inet_addr("0.0.0.0");

    if (bind(fp, (struct sockaddr *)&saLocal, sizeof(struct sockaddr_in)) < 0)
    {
        close(fp);
        return -2;
    }
    return fp;
}

int linux_network_udp_recv(int fp, void *buffer, uint32_t size, uint8_t *ip, uint16_t *port)
{
    struct sockaddr_in sa;
    socklen_t len = sizeof(struct sockaddr_in);
    ssize_t ret = recvfrom(fp, buffer, size, MSG_DONTWAIT, (struct sockaddr *)&sa, &len);
    if (!ret)
    {
        SigmaLogError("%d closed by remote", fp);
        return -1;
    }
    if (ret < 0)
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            return 0;
        SigmaLogError("%d closed by error(%u:%s)", fp, errno, strerror(errno));
        return -1;
    }
    *port = ntohs(sa.sin_port);
    *(uint32_t *)ip = sa.sin_addr.s_addr;
    return ret;
}

int linux_network_udp_send(int fp, const void *buffer, uint32_t size, const uint8_t *ip, uint16_t port)
{
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = *(uint32_t *)ip;
    int ret = sendto(fp, buffer, size, 0, (const struct sockaddr *)&sa, (socklen_t)sizeof(struct sockaddr_in));
    if (!ret)
    {
        SigmaLogError("%d closed by remote", fp);
        return -1;
    }
    if (ret < 0)
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            return 0;
        SigmaLogError("%d closed by error(%s)", fp, strerror(errno));
        return -1;
    }
    return ret;
}

int linux_network_close(int fp)
{
    return close(fp);
}

const uint8_t *linux_network_ip(void)
{
    return _ip_addr;
}

const uint8_t *linux_network_mask(void)
{
    return _mask_addr;
}

const uint8_t *linux_network_nat(void)
{
    return _nat_addr;
}

const uint8_t *linux_network_mac(void)
{
    return _mac_addr;
}

#endif
