#ifndef __LIBJJ_CAN_UDP_H__
#define __LIBJJ_CAN_UDP_H__

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "utils.h"
#include "list.h"
#include "logging.h"
#include "socket.h"
#include "can_tcp.h"

struct canudp_cfg {
        uint8_t enabled;
        uint16_t port;
};

static struct canudp_cfg *g_canudp_cfg;
static int can_udp_sock = -1;
static struct sockaddr_in canudp_client_addr;

int can_udp_send(can_frame_t *f)
{
        ssize_t n;

        if (can_udp_sock < 0)
                return -ENOENT;
        
        n = sendto(can_udp_sock,
                   (uint8_t *)f,
                   (size_t)(sizeof(can_frame_t) + f->dlc),
                   0,
                   (struct sockaddr*)&canudp_client_addr,
                   sizeof(canudp_client_addr));
        if (n < 0) {
                pr_err("sendto(): %d %s\n", errno, strerror(abs(errno)));
                return -errno;
        }

        wifi_activity = 1;
        
        return 0;
}

int __can_udp_send_ratelimited(can_frame_t *f, struct can_rlimit_node *rlimit)
{
        if (can_udp_sock < 0)
                return -ENOENT;
        
#ifdef CONFIG_HAVE_CAN_RLIMIT
        if (is_can_id_ratelimited(rlimit, CAN_RLIMIT_TYPE_TCP, esp32_millis())) {
                return -EBUSY;
        }
#endif

        return can_udp_send(f);
}

int can_udp_send_ratelimited(can_frame_t *f)
{
        return __can_udp_send_ratelimited(f, can_ratelimit_get(f->id));
}

static void task_can_udp(void *arg)
{
        struct sockaddr_in client_addr = { };
        socklen_t addrlen = sizeof(client_addr);
        char buf[64];
        ssize_t n;

        while (1) {
                // we only expected heartbeat from remote
                n = recvfrom(can_udp_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&client_addr, &addrlen);
                if (n > 0) {
                        can_frame_t *f = (can_frame_t *)buf;

                        if (le32toh(f->magic) != CAN_DATA_MAGIC) {
                                pr_dbg("recv invalid magic\n");
                                goto sleep;
                        }

                        if (f->heartbeat) {
                                if (client_addr.sin_addr.s_addr != canudp_client_addr.sin_addr.s_addr &&
                                    client_addr.sin_port != canudp_client_addr.sin_port) {
                                        pr_dbg("client: %s:%u\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                                        memcpy(&canudp_client_addr, &client_addr, addrlen);
                                }
                        }
                }

sleep:
                mdelay((100));
        }
}

static void can_udp_recv_cb(can_frame_t *f, struct can_rlimit_node *rlimit)
{
        __can_udp_send_ratelimited(f, rlimit);
}

static void __unused can_udp_init(struct canudp_cfg *cfg)
{
        struct sockaddr_in server_addr = { };
        int sock;

        if (!cfg->enabled)
                return;

        g_canudp_cfg = cfg;
        
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock < 0) {
                pr_err("socket(): %d %s\n", errno, strerror(abs(errno)));
                return;
        }

        server_addr.sin_family          = AF_INET;
        server_addr.sin_addr.s_addr     = htonl(INADDR_ANY);
        server_addr.sin_port            = htons(cfg->port);

        if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                pr_err("bind(): %d %s\n", errno, strerror(abs(errno)));
                close(sock);
                return;
        }

        pr_info("listen on port %u\n", cfg->port);

        can_udp_sock = sock;

        xTaskCreatePinnedToCore(task_can_udp, "can_udp", 4096, NULL, 1, NULL, CPU1);
        can_recv_cb_register(can_udp_recv_cb);
}

#endif // __LIBJJ_CAN_UDP_H__