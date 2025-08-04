#ifndef __LIBJJ_PING_H__
#define __LIBJJ_PING_H__

#include <lwip/err.h>
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/ip.h>
#include <lwip/inet_chksum.h>
#include <lwip/tcpip.h>

#define PING_ID                 0xAFAF
#define PING_DATA_SIZE          8

static const int icmp_ip_hlen = 20;
typedef int (*ping_cb_t)(struct ping_ctx *ctx, struct pbuf *p, const ip_addr_t *addr);

struct ping_ctx {
        SemaphoreHandle_t sem;
        ip_addr_t tgt;
        struct raw_pcb *pcb;
        unsigned seq;
        unsigned ts_send;
        ping_cb_t cb;
        void *userdata;
};

static u8_t __ping4_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
        struct ping_ctx *ctx = (struct ping_ctx *)arg;

        if (p->len >= (icmp_ip_hlen + sizeof(struct icmp_echo_hdr))) {
                if (ctx->cb) {
                        if (ctx->cb(ctx, p, addr) == 0) {
                                pbuf_free(p);
                                return 1; // consumed
                        }
                }
        }

        return 0; // not consumed, continue other logic, free by lwip
}

static void __ping4_send(void *arg)
{
        struct ping_ctx *ctx = (struct ping_ctx *)arg;
        struct pbuf *p;
        struct icmp_echo_hdr *iecho;
        size_t ping_size = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;

        p = pbuf_alloc(PBUF_IP, (u16_t)ping_size, PBUF_RAM);
        if (!p) {
                pr_err("failed to allocate pbuf for ping\n");
                xSemaphoreGive(ctx->sem);
                return;
        }

        if ((p->len != ping_size) || (p->payload == nullptr)) {
                pr_err("bad pbuf\n");
                pbuf_free(p);
                xSemaphoreGive(ctx->sem);
                return;
        }

        iecho = (struct icmp_echo_hdr *)p->payload;
        ICMPH_TYPE_SET(iecho, ICMP_ECHO);
        ICMPH_CODE_SET(iecho, 0);
        iecho->chksum = 0;
        iecho->id = lwip_htons(PING_ID);
        iecho->seqno = lwip_htons(++ctx->seq);

        for (int i = 0; i < PING_DATA_SIZE; i++) {
                ((char *)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
        }

        iecho->chksum = inet_chksum(iecho, ping_size);
        ctx->ts_send = esp32_millis();

        raw_sendto(ctx->pcb, p, &ctx->tgt);
        pbuf_free(p);
        xSemaphoreGive(ctx->sem);
}

static void __ping4_init(void *arg)
{
        struct ping_ctx *ctx = (struct ping_ctx *)arg;

        ctx->pcb = raw_new(IP_PROTO_ICMP);
        if (!ctx->pcb) {
                ctx->pcb = NULL;
                xSemaphoreGive(ctx->sem);
                return;
        }

        raw_recv(ctx->pcb, __ping4_recv, ctx);
        raw_bind(ctx->pcb, IP_ADDR_ANY);
        xSemaphoreGive(ctx->sem);
}

static void __ping4_deinit(void *arg)
{
        struct ping_ctx *ctx = (struct ping_ctx *)arg;

        if (ctx && ctx->pcb) {
                raw_remove(ctx->pcb);
        }
}

struct ping_ctx *ping4_init(const char *tgt, ping_cb_t cb, void *userdata)
{
        ip_addr_t target_ip;
        struct ping_ctx *ctx;

        if (!ipaddr_aton(tgt, &target_ip)) {
                pr_info("invalid ip addr: %s\n", tgt);
                return NULL;
        }

        ctx = (struct ping_ctx *)calloc(1, sizeof(struct ping_ctx));
        if (!ctx)
                return NULL;

        ctx->sem = xSemaphoreCreateBinary();
        ctx->tgt = target_ip;
        ctx->cb = cb;
        ctx->userdata = userdata;

        tcpip_callback(__ping4_init, ctx);
        xSemaphoreTake(ctx->sem, portMAX_DELAY);

        if (ctx->pcb == NULL) {
                pr_info("failed to init raw socket\n");
                vSemaphoreDelete(ctx->sem);
                free(ctx);

                return NULL;
        }

        return ctx;
}

int ping4_send(struct ping_ctx *ctx)
{
        unsigned seq = ctx->seq;

        tcpip_callback(__ping4_send, ctx);
        xSemaphoreTake(ctx->sem, portMAX_DELAY);

        // if seq not increased, something goes wrong
        if (seq == ctx->seq) {
                return -EFAULT;
        }

        return 0;
}

void ping4_deinit(struct ping_ctx *ctx)
{
        if (!ctx)
                return;

        tcpip_callback(__ping4_deinit, ctx);
        vSemaphoreDelete(ctx->sem);
        free(ctx);
}

#include <esp_ping.h>
#include <ping/ping_sock.h>

struct esp_ping_ctx {
        unsigned recv;
        unsigned done;
};

static void ping_end_cb(void *hdl, void *args)
{
        struct esp_ping_ctx *ctx = (struct esp_ping_ctx *)args;

        esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &ctx->recv, sizeof(ctx->recv));

        ctx->done = 1;
}

int esp_ping4(IPAddress dst,
             unsigned count = 1,
             unsigned intv_ms = 1000,
             unsigned timeout_ms = 1000)
{
        struct esp_ping_ctx ctx = { };

        ip_addr_t ip_dst = {};
        ip_dst.u_addr.ip4.addr = static_cast<uint32_t>(dst);
        ip_dst.type = IPADDR_TYPE_V4;
        
        esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
        config.count = count;
        config.interval_ms = intv_ms;
        config.timeout_ms = timeout_ms;
        config.target_addr = ip_dst;

        esp_ping_callbacks_t cbs = { };
        cbs.on_ping_end = ping_end_cb;
        cbs.cb_args = &ctx;

        void *ping;

        if (esp_ping_new_session(&config, &cbs, &ping) != ESP_OK)
                return false;

        esp_ping_start(ping);

        // Wait for result or timeout (polling loop)
        unsigned long start = esp32_millis();
        while (!ctx.done && (esp32_millis() - start < intv_ms * count + 200)) {
                vTaskDelay(pdMS_TO_TICKS(100));
        }

        esp_ping_stop(ping);
        esp_ping_delete_session(ping);

        return (int)ctx.recv;
}

#endif // __LIBJJ_PING_H__
