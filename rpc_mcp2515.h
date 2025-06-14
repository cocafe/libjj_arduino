#ifndef __LIBJJ_RPC_MCP2515_H__
#define __LIBJJ_RPC_MCP2515_H__

void rpc_mcp2515_add(void)
{
#ifdef CONFIG_HAVE_CAN_MCP2515
        http_rpc.on("/mcp2515_cfg", HTTP_GET, [](){
                struct mcp2515_cfg tmp = { };
                struct http_cfg_param params[] = {
                        HTTP_CFG_PARAM_STRVAL(mode, tmp.mode, str_mcp_mode),
                        HTTP_CFG_PARAM_STRVAL(quatrz, tmp.quartz, str_mcp_quartz),
                        HTTP_CFG_PARAM_STRVAL(baudrate, tmp.baudrate, str_can_baudrates),
                        HTTP_CFG_PARAM_INT(pin_int, tmp.pin_int),
                };
                int modified = 0;

                memcpy(&tmp, &g_cfg.mcp2515_cfg, sizeof(struct mcp2515_cfg));

                if (http_param_help_print(http_rpc, params, ARRAY_SIZE(params)))
                        return;

                modified = http_param_parse(http_rpc, params, ARRAY_SIZE(params));
                if (modified < 0) {
                        http_rpc.send(500, "text/plain", "Invalid value or internal error\n");
                        return;
                }

                if (modified == 1) {
                        memcpy(&g_cfg.mcp2515_cfg, &tmp, sizeof(struct mcp2515_cfg));
                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[256] = { };

                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"baudrate\": \"%s\",\n", str_can_baudrates[g_cfg.mcp2515_cfg.baudrate]);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"clkrate\": \"%s\",\n", str_mcp_quartz[g_cfg.mcp2515_cfg.quartz]);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"mode\": \"%s\",\n", str_mcp_mode[g_cfg.mcp2515_cfg.mode]);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"pin_int\": %hhu\n", g_cfg.mcp2515_cfg.pin_int);
                        c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                        http_rpc.send(200, "text/plain", buf);
                }
        });
#endif // CONFIG_HAVE_CAN_MCP2515
}

#endif // __LIBJJ_RPC_MCP2515_H__