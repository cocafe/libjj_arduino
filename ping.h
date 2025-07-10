#ifndef __LIBJJ_PING_H__
#define __LIBJJ_PING_H__

#include <esp_ping.h>
#include <ping/ping_sock.h>

struct ping_ctx {
        unsigned recv;
        unsigned done;
};

static void ping_end_cb(void *hdl, void *args)
{
        struct ping_ctx *ctx = (struct ping_ctx *)args;

        esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &ctx->recv, sizeof(ctx->recv));

        ctx->done = 1;
}

int ping4(IPAddress dst,
             unsigned count = 1,
             unsigned intv_ms = 1000,
             unsigned timeout_ms = 1000)
{
        struct ping_ctx ctx = { };

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
