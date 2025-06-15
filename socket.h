#ifndef __LIBJJ_SOCKET_H__
#define __LIBJJ_SOCKET_H__

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>

#include "libjj/logging.h"

struct udp_mc_cfg {
        uint8_t enabled;
        uint16_t port;
        char mcaddr[24];
};

static int __unused udp_mc_sock_create(const char *if_addr, const char *mc_addr, unsigned port, sockaddr_in *skaddr)
{
        int sock;

        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
                pr_err("socket(): %d %s\n", errno, strerror(abs(errno)));
                return -EIO;
        }

        int yes = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                pr_err("socket(): %d %s\n", errno, strerror(abs(errno)));
                return -EFAULT;
        }

        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(mc_addr);
        mreq.imr_interface.s_addr = inet_addr(if_addr);
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                pr_err("setsockopt() failed\n");
                return -EFAULT;
        }

        memset(skaddr, 0x00, sizeof(sockaddr_in));
        skaddr->sin_family = AF_INET;
        skaddr->sin_addr.s_addr = inet_addr(mc_addr);
        skaddr->sin_port = htons(port);

        return sock;
}

static int udp_mc_sock_close(int sock)
{
        if (sock < -1)
                return -ENODATA;

        shutdown(sock, SHUT_RDWR);
        close(sock);

        return 0;
}

#endif // __LIBJJ_SOCKET_H__