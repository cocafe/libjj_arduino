#ifndef __LIBJJ_RACECHRONO_FWD_H__
#define __LIBJJ_RACECHRONO_FWD_H__

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>

#include "libjj/utils.h"
#include "libjj/logging.h"

#ifndef __LIBJJ_CAN_TCP_H__
#error include can_tcp.h first
#endif

#ifndef __LIBJJ_RACECHRONO_BLE_H__
#error include racechrono_ble.h first
#endif

struct rc_fwd_cfg {
        uint8_t enabled;
        uint16_t port;
        char mcaddr[24];
};

static char rc_udp_mc_addr[24] = "239.0.0.1";
static int rc_udp_mc_port = 4090;
static int rc_udp_mc_sock = -1;
static sockaddr_in rc_udp_mc_skaddr;

static uint64_t cnt_can_udp_recv_error;
static uint64_t cnt_can_udp_recv; // from remote multicast
static uint64_t cnt_can_udp_send_error;
static uint64_t cnt_can_udp_send; // send to remote multicase

static int __attribute__((unused)) racechrono_udp_mc_send(can_frame_t *f)
{
        int sock = READ_ONCE(rc_udp_mc_sock);
        int sendsz = sizeof(can_frame_t) + f->dlc;
        int rc = sendto(sock, f, sendsz, 0,
                        (struct sockaddr *)&rc_udp_mc_skaddr,
                        sizeof(rc_udp_mc_skaddr));
        if (rc != sendsz) {
                cnt_can_udp_send_error++;

                if (rc <= 0) {
                        return -errno;
                }

                return -EFAULT;
        }

        cnt_can_udp_send++;

        return 0;
}

static int racechrono_udp_mc_recv(void)
{
        static uint8_t buf[(sizeof(can_frame_t) + 8) + 1] = { };
        struct sockaddr_in src_addr;
        socklen_t addrlen = sizeof(src_addr);
        int sock = READ_ONCE(rc_udp_mc_sock);

        if (unlikely(sock < 0))
                return -ENOENT;

        // blocked waiting
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr *)&src_addr, &addrlen);
        if (len < 0) {
                pr_err("recvfrom(): %d %s\n", errno, strerror(abs(errno)));
                return -errno;
        }

        can_frame_t *f = (can_frame_t *)buf;
        if (!is_valid_can_frame(f)) {
                cnt_can_udp_recv_error++;
                return -EINVAL;
        }

        can_ble_frame_send(f);
        cnt_can_udp_recv++;

        return 0;
}

static int racechrono_udp_mc_create(char *mc_addr, int port)
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
        mreq.imr_interface.s_addr = inet_addr(WiFi.localIP().toString().c_str());
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                pr_err("setsockopt() failed\n");
                return -EFAULT;
        }

        memset(&rc_udp_mc_skaddr, 0x00, sizeof(rc_udp_mc_skaddr));
        rc_udp_mc_skaddr.sin_family = AF_INET;
        rc_udp_mc_skaddr.sin_addr.s_addr = inet_addr(mc_addr);
        rc_udp_mc_skaddr.sin_port = htons(port);

        WRITE_ONCE(rc_udp_mc_sock, sock);
        pr_info("%s:%u\n", mc_addr, port);

        return 0;
}

static void racechrono_udp_mc_close(void)
{
        int sock = READ_ONCE(rc_udp_mc_sock);

        if (sock < -1)
                return;

        rc_udp_mc_sock = -1;
        shutdown(sock, SHUT_RDWR);
        close(sock);

        return;
}

static void task_racechrono_fwd_recv(void *arg)
{
        pr_info("started\n");

        while (1) {
                if (rc_udp_mc_sock < -1) {
                        vTaskDelay(pdMS_TO_TICKS(5000));
                        continue;
                }

                racechrono_udp_mc_recv();
        }
}

static void racechrono_fwd_wifi_event_cb(int event)
{
        switch (event) {
        case WIFI_EVENT_CONNECTED:
                racechrono_udp_mc_create(rc_udp_mc_addr, rc_udp_mc_port);
                break;
        
        case WIFI_EVENT_DISCONNECTED:
                racechrono_udp_mc_close();
                break;

        default:
                break;
        }
}

static void __attribute__((unused)) racechrono_fwd_receiver_init(struct rc_fwd_cfg *cfg)
{
        if (!cfg)
                return;

        if (!cfg->enabled)
                return;

        strncpy(rc_udp_mc_addr, cfg->mcaddr, sizeof(rc_udp_mc_addr));
        rc_udp_mc_port = cfg->port;
        wifi_event_cb_add(racechrono_fwd_wifi_event_cb);
        xTaskCreatePinnedToCore(task_racechrono_fwd_recv, "rc_fwd", 4096, NULL, 1, NULL, CPU1);
}

#endif // __LIBJJ_RACECHRONO_FWD_H__