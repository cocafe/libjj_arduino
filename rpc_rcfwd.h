#ifndef __LIBJJ_RPC_RCFWD_H__
#define __LIBJJ_RPC_RCFWD_H__

#include <WebServer.h>

void rpc_rcfwd_add(void)
{
#ifdef __LIBJJ_RACECHRONO_FWD_H__
        http_rpc.on("/rc_fwd_cfg", HTTP_GET, [](){
                struct rc_fwd_cfg rc_fwd_cfg;
                struct http_cfg_param params[] = {
                        HTTP_CFG_PARAM_INT(rc_fwd_enabled, rc_fwd_cfg.enabled),
                        HTTP_CFG_PARAM_INT(rc_fwd_port, rc_fwd_cfg.port),
                        HTTP_CFG_PARAM_STR(rc_fwd_mcaddr, rc_fwd_cfg.mcaddr),
                };
                int modified = 0;

                memcpy(&rc_fwd_cfg, &g_cfg.rc_fwd_cfg, sizeof(rc_fwd_cfg));

                if (http_param_help_print(http_rpc, params, ARRAY_SIZE(params)))
                        return;

                modified = http_param_parse(http_rpc, params, ARRAY_SIZE(params));
                if (modified < 0) {
                        http_rpc.send(500, "text/plain", "Invalid value or internal error\n");
                        return;
                }

                if (modified == 1) {
                        IPAddress ip;

                        if (ip.fromString(rc_fwd_cfg.mcaddr) != true) {
                                http_rpc.send(200, "text/plain", "Invalid IP\n");
                                return;
                        }

                        memcpy(&g_cfg.rc_fwd_cfg, &rc_fwd_cfg, sizeof(g_cfg.rc_fwd_cfg));

                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[256] = { };
                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"enabled\": %d,\n", g_cfg.rc_fwd_cfg.enabled);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"mcaddr\": \"%s\",\n", g_cfg.rc_fwd_cfg.mcaddr);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"port\": %d\n", g_cfg.rc_fwd_cfg.port);
                        c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                        http_rpc.send(200, "text/plain", buf);
                }
        });

#endif // __LIBJJ_RACECHRONO_FWD_H__
}

#endif // __LIBJJ_RPC_RCFWD_H__