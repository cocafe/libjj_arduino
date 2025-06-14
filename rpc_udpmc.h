#ifndef __LIBJJ_RPC_UDPMC_H__
#define __LIBJJ_RPC_UDPMC_H__

static void rpc_udp_mc_cfg(WebServer &http_rpc, struct udp_mc_cfg *cfg)
{
        struct udp_mc_cfg tmp;
        struct http_cfg_param params[] = {
                HTTP_CFG_PARAM_INT(enabled, tmp.enabled),
                HTTP_CFG_PARAM_INT(port, tmp.port),
                HTTP_CFG_PARAM_STR(mcaddr, tmp.mcaddr),
        };
        int modified = 0;

        memcpy(&tmp, cfg, sizeof(struct udp_mc_cfg));

        if (http_param_help_print(http_rpc, params, ARRAY_SIZE(params)))
                return;

        modified = http_param_parse(http_rpc, params, ARRAY_SIZE(params));
        if (modified < 0) {
                http_rpc.send(500, "text/plain", "Invalid value or internal error\n");
                return;
        }

        if (modified == 1) {
                IPAddress ip;

                if (ip.fromString(tmp.mcaddr) != true) {
                        http_rpc.send(200, "text/plain", "Invalid IP\n");
                        return;
                }

                memcpy(cfg, &tmp, sizeof(struct udp_mc_cfg));

                http_rpc.send(200, "text/plain", "OK\n");
        } else {
                char buf[256] = { };
                size_t c = 0;

                c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                c += snprintf(&buf[c], sizeof(buf) - c, "  \"enabled\": %d,\n", cfg->enabled);
                c += snprintf(&buf[c], sizeof(buf) - c, "  \"mcaddr\": \"%s\",\n", cfg->mcaddr);
                c += snprintf(&buf[c], sizeof(buf) - c, "  \"port\": %d\n", cfg->port);
                c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                http_rpc.send(200, "text/plain", buf);
        }
}

void rpc_udpmc_add(void)
{
#ifdef __LIBJJ_RACECHRONO_FWD_H__
        http_rpc.on("/rc_mc_cfg", HTTP_GET, [](){
                rpc_udp_mc_cfg(http_rpc, &g_cfg.rc_mc_cfg);
        });
#endif // __LIBJJ_RACECHRONO_FWD_H__


#ifdef __LIBJJ_EVENT_UDP_MC_H__
        http_rpc.on("/event_mc_cfg", HTTP_GET, [](){
                rpc_udp_mc_cfg(http_rpc, &g_cfg.udp_mc_cfg);
        });
#endif // __LIBJJ_EVENT_UDP_MC_H__
}

#endif // __LIBJJ_RPC_UDPMC_H__