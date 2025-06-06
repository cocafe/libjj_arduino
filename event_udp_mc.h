#ifndef __LIBJJ_EVENT_UDP_MC_H__
#define __LIBJJ_EVENT_UDP_MC_H__

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <esp_mac.h>
#include <esp_system.h>

#include "libjj/utils.h"
#include "libjj/logging.h"
#include "libjj/socket.h"

#define EVENT_PKT_MAGIC                 (0x40905090UL)

enum {
        EVENT_DEV_HEARTBEAT,
        EVENT_RC_BLE_CONNECTED,
        EVENT_RC_BLE_DISCONNECTED,
        NUM_UDP_EVENTS,
};

typedef struct udp_event_pkt udp_event_t;

struct udp_event_pkt {
        __le32 magic;
        uint8_t event;
        uint8_t dlen;
        uint8_t data[];
} __attribute__((packed));

struct event_dev_hb {
        uint8_t devname[16];
        uint8_t sn[8];
};

struct udp_evnet_all {
        union {
                struct event_dev_hb dev_hb;
        };
};

struct udp_event_cb_ctx {
        void (*cb)(uint8_t event, void *data, unsigned dlen);
};

static struct udp_event_cb_ctx udp_event_cbs[4] = { };
static uint8_t udp_event_cb_cnt;

static SemaphoreHandle_t lck_udp_event_cb;

static int __attribute__((unused)) udp_event_cb_register(void (*cb)(uint8_t event, void *data, unsigned dlen))
{
        int err = 0;

        xSemaphoreTake(lck_udp_event_cb, portMAX_DELAY);

        if (udp_event_cb_cnt >= ARRAY_SIZE(udp_event_cbs)) {
                err = -ENOSPC;
                goto unlock;
        }

        udp_event_cbs[udp_event_cb_cnt++].cb = cb;

unlock:
        xSemaphoreGive(lck_udp_event_cb);

        return err;
}

static unsigned udp_event_pkt_dlen[] = {
        [EVENT_DEV_HEARTBEAT]           = sizeof(struct event_dev_hb),
        [EVENT_RC_BLE_CONNECTED]        = 0,
        [EVENT_RC_BLE_DISCONNECTED]     = 0,
};

static char evt_udp_mc_addr[24] = "239.0.0.1";
static int evt_udp_mc_port = 5090;
static int evt_udp_mc_sock = -1;
static sockaddr_in evt_udp_mc_skaddr;

static int __event_udp_mc_send(udp_event_t *pkt)
{
        int sock = READ_ONCE(evt_udp_mc_sock);

        if (unlikely(sock < 0))
                return -ENODEV;

        int sendsz = sizeof(udp_event_t) + pkt->dlen;
        int rc = sendto(sock, pkt, sendsz, 0,
                        (struct sockaddr *)&evt_udp_mc_skaddr,
                        sizeof(evt_udp_mc_skaddr));
        if (rc != sendsz) {
                if (rc <= 0) {
                        return -errno;
                }

                return -EFAULT;
        }

        return 0;
}

static int __attribute__((unused)) event_udp_mc_send(uint8_t event, uint8_t *data, unsigned dlen)
{
        udp_event_t *pkt = (udp_event_t *)malloc(sizeof(udp_event_t) + dlen);
        int rc;

        if (!pkt)
                return -ENOMEM;

        pkt->magic = htole32(EVENT_PKT_MAGIC);
        pkt->event = event;
        pkt->dlen = dlen;

        if (data && dlen)
                memcpy(&pkt->data, data, dlen);

        rc = __event_udp_mc_send(pkt);

        pr_verbose("sent event %hhu %dbytes\n", event, sizeof(udp_event_t) + dlen);

        free(pkt);

        return rc;
}

static int event_udp_input(udp_event_t *pkt, int len)
{
        if (le32toh(pkt->magic) != EVENT_PKT_MAGIC)
                return -EINVAL;

        if (pkt->event >= NUM_UDP_EVENTS)
                return -EINVAL;

        if (pkt->dlen != udp_event_pkt_dlen[pkt->event])
                return -EINVAL;

        switch (pkt->event) {
        case EVENT_RC_BLE_CONNECTED:
                break;

        case EVENT_RC_BLE_DISCONNECTED:
                break;

        case EVENT_DEV_HEARTBEAT:
        default:
                break;
        }

        return 0;
}

static int event_udp_mc_recv(void)
{
        static uint8_t buf[(sizeof(udp_event_t) + sizeof(struct udp_evnet_all)) + 1] = { };
        struct sockaddr_in src_addr;
        socklen_t addrlen = sizeof(src_addr);
        int sock = READ_ONCE(evt_udp_mc_sock);

        if (unlikely(sock < 0))
                return -ENODEV;

        // blocked waiting
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr *)&src_addr, &addrlen);
        if (len < 0) {
                pr_err("recvfrom(): %d %s\n", errno, strerror(abs(errno)));
                return -errno;
        }

        return event_udp_input((udp_event_t *)buf, len);
}


static void task_event_udp_mc_recv(void *arg)
{
        pr_info("started\n");

        while (1) {
                if (READ_ONCE(evt_udp_mc_sock) < -1) {
                        vTaskDelay(pdMS_TO_TICKS(5000));
                        continue;
                }

                event_udp_mc_recv();
        }
}

static int event_udp_mc_sock_create(char *mc_addr, unsigned port)
{
        int sock = udp_mc_sock_create(mc_addr, port, &evt_udp_mc_skaddr);

        if (sock < 0)
                return sock;

        WRITE_ONCE(evt_udp_mc_sock, sock);
        pr_info("%s:%u\n", mc_addr, port);

        return 0;
}

static void event_udp_mc_sock_close(void)
{
        if (!udp_mc_sock_close(READ_ONCE(evt_udp_mc_sock))) {
                evt_udp_mc_sock = -1;
        }
}

static void event_udp_mc_wifi_event_cb(int event)
{
        switch (event) {
        case WIFI_EVENT_CONNECTED:
                event_udp_mc_sock_create(evt_udp_mc_addr, evt_udp_mc_port);
                break;
        
        case WIFI_EVENT_DISCONNECTED:
                event_udp_mc_sock_close();
                break;

        default:
                break;
        }
}

static void __attribute__((unused)) event_udp_mc_init(struct udp_mc_cfg *cfg, unsigned cpu)
{
        if (!cfg)
                return;

        if (!cfg->enabled)
                return;

        lck_udp_event_cb = xSemaphoreCreateMutex();
        strncpy(evt_udp_mc_addr, cfg->mcaddr, sizeof(evt_udp_mc_addr));
        evt_udp_mc_port = cfg->port;
        wifi_event_cb_register(event_udp_mc_wifi_event_cb);
        xTaskCreatePinnedToCore(task_event_udp_mc_recv, "udp_event", 4096, NULL, 1, NULL, cpu);
}

static void task_event_up_dev_hb(void *arg)
{
        struct event_dev_hb hb = { };
        char *devname = (char *)arg;

        if (esp_efuse_mac_get_default(hb.sn) != ESP_OK) {
                pr_err("failed to get default mac\n");
                vTaskDelete(NULL);
                return;
        }

        strncpy((char *)hb.devname, devname, sizeof(hb.devname));

        while (1) {
                event_udp_mc_send(EVENT_DEV_HEARTBEAT, (uint8_t *)&hb, sizeof(hb));
                vTaskDelay(pdMS_TO_TICKS(10000));
        }
}

static void __attribute__((unused)) task_event_udp_dev_hb_start(char *devname, unsigned cpu)
{
        xTaskCreatePinnedToCore(task_event_up_dev_hb, "dev_hb", 4096, devname, 1, NULL, cpu);
}

#endif // __LIBJJ_EVENT_UDP_MC_H__