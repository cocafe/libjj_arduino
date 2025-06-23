#ifndef __LIBJJ_RPC_WIFI_H__
#define __LIBJJ_RPC_WIFI_H__

void rpc_wifi_add(void)
{
#ifndef CONFIG_WIFI_AP_ONLY
        http_rpc.on("/wifi_sta_cfg", HTTP_GET, [](){
                struct wifi_nw_cfg tmp = { };
                struct http_cfg_param params[] = {
                        HTTP_CFG_PARAM_STR(ssid, tmp.ssid),
                        HTTP_CFG_PARAM_STR(passwd, tmp.passwd),
                        HTTP_CFG_PARAM_INT(use_dhcp, tmp.use_dhcp),
                        HTTP_CFG_PARAM_STR(local, tmp.local),
                        HTTP_CFG_PARAM_STR(subnet, tmp.subnet),
                        HTTP_CFG_PARAM_STR(gw, tmp.gw),
                        HTTP_CFG_PARAM_INT(timeout_sec, tmp.timeout_sec),
                };
                int modified = 0;

                memcpy(&tmp, &g_cfg.nw_sta_cfg, sizeof(tmp));

                if (http_param_help_print(http_rpc, params, ARRAY_SIZE(params)))
                        return;

                modified = http_param_parse(http_rpc, params, ARRAY_SIZE(params));
                if (modified < 0) {
                        http_rpc.send(500, "text/plain", "Invalid value or internal error\n");
                        return;
                }

                if (modified == 1) {
                        if (!tmp.use_dhcp) {
                                IPAddress ip;

                                if (ip.fromString(tmp.local) != true) {
                                        http_rpc.send(200, "text/plain", "Invalid local ip\n");
                                        return;
                                }

                                if (ip.fromString(tmp.subnet) != true) {
                                        http_rpc.send(200, "text/plain", "Invalid subnet ip\n");
                                        return;
                                }

                                if (ip.fromString(tmp.gw) != true) {
                                        http_rpc.send(200, "text/plain", "Invalid gateway ip\n");
                                        return;
                                }
                        }

                        memcpy(&g_cfg.nw_sta_cfg, &tmp, sizeof(g_cfg.nw_sta_cfg));

                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[256] = { };
                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"ssid\": \"%s\",\n", g_cfg.nw_sta_cfg.ssid);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"passwd\": \"%s\",\n", g_cfg.nw_sta_cfg.passwd);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"dhcp\": %d,\n", g_cfg.nw_sta_cfg.use_dhcp);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"local\": \"%s\",\n", g_cfg.nw_sta_cfg.local);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"subnet\": \"%s\",\n", g_cfg.nw_sta_cfg.subnet);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"gw\": \"%s\",\n", g_cfg.nw_sta_cfg.gw);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"sta_timeout_sec\": %hu\n", g_cfg.nw_sta_cfg.timeout_sec);
                        c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                        http_rpc.send(200, "text/plain", buf);
                }
        });

        http_rpc.on("/wifi_rssi", HTTP_GET, [](){
                char buf[16] = { };
                
                snprintf(buf, sizeof(buf), "%d dBm\n", WiFi.RSSI());

                http_rpc.send(200, "text/plain", buf);
        });
#endif

        http_rpc.on("/wifi_tx_power", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        String data = http_rpc.arg("set");
                        int match = 0;
                        int i = 0;

                        for (i = 0; i < ARRAY_SIZE(str_wifi_txpwr); i++) {
                                if (is_str_equal((char *)str_wifi_txpwr[i], (char *)data.c_str(), CASELESS)) {
                                        match = 1;
                                        break;
                                }
                        }

                        if (!match) {
                                http_rpc.send(404, "text/plain", "Invalid value\n");
                                return;
                        }

                        g_cfg.wifi_cfg.tx_power = i;
                        WiFi.setTxPower((wifi_power_t)(cfg_wifi_txpwr_convert[i]));

                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[256] = { };
                        size_t c = 0;
                        int r = -1;

                        wifi_power_t curr = WiFi.getTxPower();

                        for (int i = 0; i < ARRAY_SIZE(cfg_wifi_txpwr_convert); i++) {
                                if (cfg_wifi_txpwr_convert[i] == curr) {
                                        r = i;
                                        break;
                                }
                        }

                        for (int i = 0; i < ARRAY_SIZE(str_wifi_txpwr); i++) {
                                c += snprintf(&buf[c], sizeof(buf) - c, "%s %s %s\n",
                                        str_wifi_txpwr[i],
                                        g_cfg.wifi_cfg.tx_power == i ? "[*]" : "",
                                        r == i ? "[v]" : "");
                        }

                        http_rpc.send(200, "text/plain", buf);
                }
        });

        http_rpc.on("/wifi_ps", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        String data = http_rpc.arg("set");
                        int match = 0;
                        int i = 0;

                        for (i = 0; i < ARRAY_SIZE(str_wifi_ps_modes); i++) {
                                if (is_str_equal((char *)str_wifi_ps_modes[i], (char *)data.c_str(), CASELESS)) {
                                        match = 1;
                                        break;
                                }
                        }

                        if (!match) {
                                http_rpc.send(404, "text/plain", "Invalid value\n");
                                return;
                        }

                        if (esp_wifi_set_ps((wifi_ps_type_t)i) != ESP_OK)
                                http_rpc.send(200, "text/plain", "error\n");
                        else
                                http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[32] = { };
                        wifi_ps_type_t val;

                        if (esp_wifi_get_ps(&val) != ESP_OK) {
                                http_rpc.send(200, "text/plain", "error\n");
                                return;
                        }

                        if (val < 0 || val >= ARRAY_SIZE(str_wifi_ps_modes)) {
                                snprintf(buf, sizeof(buf), "%d\n", val);
                                return;
                        } else {
                                size_t c = 0;

                                for (int i = 0; i < ARRAY_SIZE(str_wifi_ps_modes); i++) {
                                        c += snprintf(&buf[c], sizeof(buf) - c, "%s %s\n", str_wifi_ps_modes[i], val == i ? "*" : "");
                                }
                        }

                        http_rpc.send(200, "text/plain", buf);
                }
        });

        // http_rpc.on("/wifi_stats", HTTP_GET, [](){
        //         if (esp_wifi_statis_dump(0xFFFFFFFF) == ESP_OK)
        //                 http_rpc.send(200, "text/plain", "OK\n");
        //         else
        //                 http_rpc.send(200, "text/plain", "error\n");
        // });

        http_rpc.on("/wifi_sta_inactive_time", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        String data = http_rpc.arg("set");
                        int err = 0;
                        unsigned i = strtoull_wrap(data.c_str(), 16, &err);

                        if (esp_wifi_set_inactive_time(WIFI_IF_STA, i) == ESP_OK) {
                                http_rpc.send(200, "text/plain", "OK\n");
                        } else {
                                http_rpc.send(200, "text/plain", "error\n");
                        }
                } else {
                        char buf[16] = { };
                        uint16_t sec = 0;

                        if (esp_wifi_get_inactive_time(WIFI_IF_STA, &sec) != ESP_OK) {
                                http_rpc.send(200, "text/plain", "error\n");
                                return;
                        }

                        snprintf(buf, sizeof(buf), "%u\n", sec);
                        http_rpc.send(200, "text/plain", buf);
                }
        });
}

#endif // __LIBJJ_RPC_WIFI_H__