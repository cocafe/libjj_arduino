#ifndef __LIBJJ_RPC_MCP2515_H__
#define __LIBJJ_RPC_MCP2515_H__

#include <WebServer.h>

void rpc_mcp2515_add(void)
{
#ifdef CONFIG_HAVE_CAN_MCP2515
        http_server.on("/mcp2515_cfg", HTTP_GET, [](){
                unsigned baudrate_i = 0, clkrate_i = 0, mode_i = 0;
                struct mcp2515_cfg tmp = { };
                struct http_cfg_param params[] = {
                        HTTP_CFG_PARAM_STRMAP(mode, mode_i, cfg_mcp_mode),
                        HTTP_CFG_PARAM_STRMAP(clkrate, clkrate_i, cfg_mcp_clkrate),
                        HTTP_CFG_PARAM_STRMAP(baudrate, baudrate_i, cfg_mcp_baudrate),
                        HTTP_CFG_PARAM_INT(pin_int, tmp.pin_int),
                };
                int modified = 0;

                memcpy(&tmp, &g_cfg.mcp2515_cfg, sizeof(struct mcp2515_cfg));

                for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_baudrate); i++) {
                        if (g_cfg.mcp2515_cfg.baudrate == cfg_mcp_baudrate[i].val) {
                                baudrate_i = cfg_mcp_baudrate[i].val;
                                break;
                        }
                }

                for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_clkrate); i++) {
                        if (g_cfg.mcp2515_cfg.clkrate == cfg_mcp_clkrate[i].val) {
                                clkrate_i = cfg_mcp_clkrate[i].val;
                                break;
                        }
                }

                for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_mode); i++) {
                        if (g_cfg.mcp2515_cfg.mode == cfg_mcp_mode[i].val) {
                                mode_i = cfg_mcp_mode[i].val;
                                break;
                        }
                }

                if (http_param_help_print(&http_server, params, ARRAY_SIZE(params)))
                        return;

                modified = http_param_parse(&http_server, params, ARRAY_SIZE(params));
                if (modified < 0) {
                        http_server.send(500, "text/plain", "Invalid value or internal error\n");
                        return;
                }

                if (modified == 1) {
                        memcpy(&g_cfg.mcp2515_cfg, &tmp, sizeof(struct mcp2515_cfg));
                        g_cfg.mcp2515_cfg.baudrate = cfg_mcp_baudrate[baudrate_i].val;
                        g_cfg.mcp2515_cfg.clkrate = cfg_mcp_clkrate[clkrate_i].val;
                        g_cfg.mcp2515_cfg.mode = cfg_mcp_mode[mode_i].val;

                        http_server.send(200, "text/plain", "OK\n");
                } else {
                        char buf[256] = { };

                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"baudrate\": \"%s\",\n", cfg_mcp_baudrate[baudrate_i].str);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"clkrate\": \"%s\",\n", cfg_mcp_clkrate[clkrate_i].str);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"mode\": \"%s\",\n", cfg_mcp_mode[mode_i].str);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"pin_int\": %hhu\n", g_cfg.mcp2515_cfg.pin_int);
                        c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                        http_server.send(200, "text/plain", buf);
                }
        });
#endif // CONFIG_HAVE_CAN_MCP2515
}

#endif // __LIBJJ_RPC_MCP2515_H__