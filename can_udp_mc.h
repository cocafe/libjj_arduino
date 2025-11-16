#ifndef __LIBJJ_CAN_UDP_MC_H__
#define __LIBJJ_CAN_UDP_MC_H__

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "utils.h"
#include "list.h"
#include "logging.h"
#include "socket.h"
#include "can_tcp.h"

struct canudp_mc_cfg {
        struct udp_mc_cfg mc;
        uint8_t can_rx_fwd;
        uint8_t udp_rx_accept;
};

static struct canudp_mc_cfg *g_canudp_mc_cfg;

static int can_udp_mc_sock = -1;
static sockaddr_in can_udp_mc_skaddr;

static uint64_t cnt_can_udp_mc_recv_error;
static uint64_t cnt_can_udp_mc_recv; // from remote multicast
static uint64_t cnt_can_udp_mc_send_error;
static uint64_t cnt_can_udp_mc_send; // send to remote multicase

struct can_udp_mc_recv_cb_ctx {
        void (*cb)(can_frame_t *f);
};

static struct can_udp_mc_recv_cb_ctx rc_fwd_recv_cbs[4] = { };
static uint8_t can_udp_mc_recv_cb_cnt;

static SemaphoreHandle_t lck_can_udp_cb;

static __unused int can_udp_mc_recv_cb_register(void (*cb)(can_frame_t *))
{
        int err = 0;

        xSemaphoreTake(lck_can_udp_cb, portMAX_DELAY);

        if (can_udp_mc_recv_cb_cnt >= ARRAY_SIZE(rc_fwd_recv_cbs)) {
                err = -ENOSPC;
                goto unlock;
        }

        rc_fwd_recv_cbs[can_udp_mc_recv_cb_cnt++].cb = cb;

unlock:
        xSemaphoreGive(lck_can_udp_cb);

        return err;
}

// NOTE. esp32 c5, 750~800ptk/s is the max throughput.
static int __unused can_udp_mc_send(can_frame_t *f)
{
        int sock = READ_ONCE(can_udp_mc_sock);

        if (unlikely(sock < 0))
                return -ENODEV;

        int sendsz = sizeof(can_frame_t) + f->dlc;
        int rc = sendto(sock, f, sendsz, 0,
                        (struct sockaddr *)&can_udp_mc_skaddr,
                        sizeof(can_udp_mc_skaddr));
        if (rc != sendsz) {
                pr_dbg("sendto(): rc: %d err: %d %s\n", rc, errno, strerror(abs(errno)));
                cnt_can_udp_mc_send_error++;

                if (rc <= 0) {
                        return -errno;
                }

                return -EFAULT;
        }

        cnt_can_udp_mc_send++;

        return 0;
}

static void __unused can_udp_mc_send_ratelimited(can_frame_t *f)
{
#ifdef CONFIG_HAVE_CAN_RLIMIT
        if (is_can_id_ratelimited(can_ratelimit_get(f->id),
                                  CAN_RLIMIT_TYPE_UDP_MC,
                                  esp32_millis())) {
                return;
        }
#endif

        can_udp_mc_send(f);
}

static int can_udp_mc_recv(void)
{
        static uint8_t buf[(sizeof(can_frame_t) + 8) + 1] = { };
        struct sockaddr_in src_addr;
        socklen_t addrlen = sizeof(src_addr);
        int sock = READ_ONCE(can_udp_mc_sock);

        if (unlikely(sock < 0))
                return -ENODEV;

        // blocked waiting
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr *)&src_addr, &addrlen);
        if (len < 0) {
                pr_err("recvfrom(): %d %s\n", errno, strerror(abs(errno)));
                return -errno;
        }

        can_frame_t *f = (can_frame_t *)buf;
        if (!is_valid_can_frame(f)) {
                cnt_can_udp_mc_recv_error++;
                return -EINVAL;
        }

        cnt_can_udp_mc_recv++;

        for (unsigned i = 0; i < ARRAY_SIZE(rc_fwd_recv_cbs); i++) {
                if (rc_fwd_recv_cbs[i].cb) {
                        rc_fwd_recv_cbs[i].cb(f);
                }
        }

        return 0;
}

static void task_can_udp_recv(void *arg)
{
        pr_info("started\n");

        while (1) {
                if (READ_ONCE(can_udp_mc_sock) < 0) {
                        vTaskDelay(pdMS_TO_TICKS(5000));
                        continue;
                }

                can_udp_mc_recv();
        }
}

static void can_udp_mc_recv_cb(can_frame_t *f, struct can_rlimit_node *rlimit)
{
#ifdef CONFIG_HAVE_CAN_RLIMIT
        if (is_can_id_ratelimited(rlimit, CAN_RLIMIT_TYPE_UDP_MC, esp32_millis())) {
                return;
        }
#endif

        if (can_udp_mc_send(f)) {
                return;
        }

#if 0
        pr_raw("udp send: id: 0x%04lx len: %u ", le32toh(f->id), f->dlc);
        for (uint8_t i = 0; i < f->dlc; i++) {
                pr_raw("0x%02x ", f->data[i]);
        }
        pr_raw("\n");
#endif
}

static int can_udp_mc_create(const char *if_addr, const char *mc_addr, unsigned port)
{
        int sock = udp_mc_sock_create(if_addr, mc_addr, port, &can_udp_mc_skaddr);

        if (sock < 0)
                return sock;

        WRITE_ONCE(can_udp_mc_sock, sock);
        pr_info("%s:%u\n", mc_addr, port);

        // {
        //         int sndbuf = 8192;
        //         setsockopt(can_udp_mc_sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
        // }

        return 0;
}

static void can_udp_mc_close(void)
{
        if (!udp_mc_sock_close(READ_ONCE(can_udp_mc_sock))) {
                pr_info("\n");
                WRITE_ONCE(can_udp_mc_sock, -1);
        }
}

static void can_udp_fwd_wifi_event_cb(esp_event_base_t event_base, int32_t event_id, void *event_data, void *userdata)
{
        if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
                if (READ_ONCE(can_udp_mc_sock) < 0 && g_canudp_mc_cfg) {
                        ip_addr_t addr;
                        wifi_netif_ip4_get(esp_netif_get_default_netif(), (esp_ip4_addr_t *)&addr);
                        can_udp_mc_create(inet_ntoa(addr), g_canudp_mc_cfg->mc.mcaddr, g_canudp_mc_cfg->mc.port);
                }
        }
        
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
                can_udp_mc_close();
        }
}

static void __unused can_udp_mc_init(struct canudp_mc_cfg *cfg)
{
        if (!cfg)
                return;

        if (!cfg->mc.enabled)
                return;

        g_canudp_mc_cfg = cfg;

        lck_can_udp_cb = xSemaphoreCreateMutex();

        if (wifi_mode_get() == ESP_WIFI_MODE_AP) {
                ip_addr_t addr;
                wifi_netif_ip4_get(esp_netif_get_default_netif(), (esp_ip4_addr_t *)&addr);
                can_udp_mc_create(inet_ntoa(addr), g_canudp_mc_cfg->mc.mcaddr, g_canudp_mc_cfg->mc.port);
        } else { // STA, STA_AP
                wifi_event_cb_register(can_udp_fwd_wifi_event_cb, NULL);
        }

        if (cfg->udp_rx_accept)
                xTaskCreatePinnedToCore(task_can_udp_recv, "can_udp_mc", 4096, NULL, 1, NULL, CPU1);

        if (cfg->can_rx_fwd && can_dev) {
                can_recv_cb_register(can_udp_mc_recv_cb);
        }
}

#endif // __LIBJJ_CAN_UDP_MC_H__