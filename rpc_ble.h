#ifndef __LIBJJ_RPC_BLE_H__
#define __LIBJJ_RPC_BLE_H__

#include <WebServer.h>

void rpc_ble_add(void)
{
        http_server.on("/ble_cfg", HTTP_GET, [](){
                struct ble_cfg tmp = { };
                struct http_cfg_param params[] = {
                        HTTP_CFG_PARAM_STR(devname, tmp.devname),
                        HTTP_CFG_PARAM_INT(update_hz, tmp.update_hz),
                };
                int modified = 0;

                memcpy(&tmp, &g_cfg.ble_cfg, sizeof(tmp));

                if (http_param_help_print(&http_server, params, ARRAY_SIZE(params)))
                        return;

                modified = http_param_parse(&http_server, params, ARRAY_SIZE(params));
                if (modified < 0) {
                        http_server.send(500, "text/plain", "Invalid value or internal error\n");
                        return;
                }

                if (modified == 1) {
                        if (tmp.devname[0] == '\0') {
                                http_server.send(500, "text/plain", "Dev name can't be empty\n");
                                return;
                        }

                        if (tmp.update_hz == 0) {
                                http_server.send(500, "text/plain", "Update HZ can't be zero\n");
                                return;
                        }

                        memcpy(&g_cfg.ble_cfg, &tmp, sizeof(g_cfg.ble_cfg));

                        http_server.send(200, "text/plain", "OK\n");
                } else {
                        char buf[256] = { };
                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"devname\": \"%s\",\n", g_cfg.ble_cfg.devname);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"update_hz\": %d\n", g_cfg.ble_cfg.update_hz);
                        c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                        http_server.send(200, "text/plain", buf);
                }
        });
}

#endif // __LIBJJ_RPC_BLE_H__