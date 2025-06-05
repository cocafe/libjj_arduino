#ifndef __LIBJJ_RPC_TWAI_H__
#define __LIBJJ_RPC_TWAI_H__

#include <WebServer.h>

void rpc_twai_add(void)
{
#ifdef CONFIG_HAVE_CAN_TWAI

        http_server.on("/twai_cfg", HTTP_GET, [](){
                struct twai_cfg tmp = { };
                struct http_cfg_param params[] = {
                        HTTP_CFG_PARAM_INT(pin_tx, tmp.pin_tx),
                        HTTP_CFG_PARAM_INT(pin_rx, tmp.pin_rx),
                        HTTP_CFG_PARAM_STRMAP(can_mode, tmp.mode, cfg_twai_mode),
                        HTTP_CFG_PARAM_INT(can_baudrate, tmp.baudrate),
                        HTTP_CFG_PARAM_INT(rx_qlen, tmp.rx_qlen),
#ifdef CAN_TWAI_USE_RINGBUF
                        HTTP_CFG_PARAM_INT(rx_cpuid, tmp.rx_cpuid),
                        HTTP_CFG_PARAM_INT(rx_rbuf_size, tmp.rx_rbuf_size),
#endif
                        HTTP_CFG_PARAM_INT(tx_timedout_ms, tmp.tx_timedout_ms),
                };
                int modified = 0;

                memcpy(&tmp, &g_cfg.twai_cfg, sizeof(struct twai_cfg));

                if (http_param_help_print(&http_server, params, ARRAY_SIZE(params)))
                        return;

                modified = http_param_parse(&http_server, params, ARRAY_SIZE(params));
                if (modified < 0) {
                        http_server.send(500, "text/plain", "Invalid value or internal error\n");
                        return;
                }

                if (modified == 1) {
                        memcpy(&g_cfg.twai_cfg, &tmp, sizeof(g_cfg.twai_cfg));
                        http_server.send(200, "text/plain", "OK\n");
                } else {
                        char buf[256] = { };
                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"pin_tx\": %hhu,\n", g_cfg.twai_cfg.pin_tx);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"pin_rx\": %hhu,\n", g_cfg.twai_cfg.pin_rx);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"can_mode\": \"%s\",\n", cfg_twai_mode[g_cfg.twai_cfg.mode].str);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"can_baudrate\": \"%s\",\n", cfg_twai_baudrate[g_cfg.twai_cfg.baudrate].str);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"rx_qlen\": %hhu,\n", g_cfg.twai_cfg.rx_qlen);
#ifdef CAN_TWAI_USE_RINGBUF
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"rx_cpuid\": %hhu,\n", g_cfg.twai_cfg.rx_cpuid);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"rx_rbuf_size\": %hu,\n", g_cfg.twai_cfg.rx_rbuf_size);
#endif
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"tx_timedout_ms\": %hu\n", g_cfg.twai_cfg.tx_timedout_ms);
                        c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                        http_server.send(200, "text/plain", buf);
                }
        });

#endif // CONFIG_HAVE_CAN_TWAI
}

#endif // __LIBJJ_RPC_TWAI_H__