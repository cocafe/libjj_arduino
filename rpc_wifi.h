#ifndef __LIBJJ_RPC_WIFI_H__
#define __LIBJJ_RPC_WIFI_H__

void rpc_wifi_add(void)
{
#if ESP_IDF_VERSION_MAJOR >= 5 && ESP_IDF_VERSION_MINOR > 5
        http_rpc.on("/wifi_band", HTTP_GET, [](){
                char buf[32] = { };
                int band, band_mode;

                if (ESP_OK != esp_wifi_get_band(&band)) {
                        http_rpc.send(500, "text/plain", "error\n");

                if (ESP_OK != esp_wifi_get_band_mode(&band_mode)) {
                        http_rpc.send(500, "text/plain", "error\n");

                if (band >= ARRAY_SIZE(str_wifi_bands) || band_mode >= ARRAY_SIZE(str_wifi_band_modes))
                        http_rpc.send(500, "text/plain", "error\n");

                snprintf(buf, sizeof(buf), "%s %s\n", str_wifi_bands[band], str_wifi_band_modes[band_mode]);

                http_rpc.send(200, "text/plain", buf);
        });
#endif

        http_rpc.on("/wifi_proto", HTTP_GET, [](){
                char buf[64] = { };
                size_t c = 0;
                uint8_t bitmap = 0;
                struct {
                        const char *s;
                        uint8_t mask;
                } protos[] = {
                        { "B",  WIFI_PROTOCOL_11B },
                        { "G",  WIFI_PROTOCOL_11G },
                        { "N",  WIFI_PROTOCOL_11N },
                        { "LR", WIFI_PROTOCOL_LR },
#if ESP_IDF_VERSION_MAJOR >= 5 && ESP_IDF_VERSION_MINOR >= 3
                        { "A",  WIFI_PROTOCOL_11A },
                        { "AC", WIFI_PROTOCOL_11AC },
                        { "AX", WIFI_PROTOCOL_11AX },
#endif
                };

                if (wifi_mode_get() == ESP_WIFI_MODE_STA_AP || wifi_mode_get() == ESP_WIFI_MODE_AP) {
                        if (ESP_OK != esp_wifi_get_protocol(WIFI_IF_AP, &bitmap)) {
                                http_rpc.send(500, "text/plain", "error\n");
                        }

                        c += snprintf(&buf[c], sizeof(buf) - c, "AP: ");
                        for (int i = 0; i < ARRAY_SIZE(protos); i++) {
                                if (bitmap & protos[i].mask) {
                                        c += snprintf(&buf[c], sizeof(buf) - c, "%s ", protos[i].s);
                                }
                        }

                        c += snprintf(&buf[c], sizeof(buf) - c, "\n");
                }

                if (wifi_mode_get() == ESP_WIFI_MODE_STA_AP || wifi_mode_get() == ESP_WIFI_MODE_STA) {
                        if (ESP_OK != esp_wifi_get_protocol(WIFI_IF_STA, &bitmap)) {
                                http_rpc.send(500, "text/plain", "error\n");
                        }

                        c += snprintf(&buf[c], sizeof(buf) - c, "STA: ");
                        for (int i = 0; i < ARRAY_SIZE(protos); i++) {
                                if (bitmap & protos[i].mask) {
                                        c += snprintf(&buf[c], sizeof(buf) - c, "%s ", protos[i].s);
                                }
                        }

                        c += snprintf(&buf[c], sizeof(buf) - c, "\n");
                }

                http_rpc.send(200, "text/plain", buf);
        });

#if ESP_IDF_VERSION_MAJOR >= 5 && ESP_IDF_VERSION_MINOR >= 3
        http_rpc.on("/wifi_bw", HTTP_GET, [](){
                char buf[64] = { };
                size_t c = 0;
                wifi_bandwidth_t bw;

                if (wifi_mode_get() == ESP_WIFI_MODE_STA_AP || wifi_mode_get() == ESP_WIFI_MODE_AP) {
                        if (ESP_OK != esp_wifi_get_bandwidth(WIFI_IF_AP, &bw)) {
                                http_rpc.send(500, "text/plain", "error\n");
                        }

                        if (bw >= 0 && bw < ARRAY_SIZE(str_wifi_bw)) {
                                c += snprintf(&buf[c], sizeof(buf) - c, "AP: %s\n", str_wifi_bw[bw]);
                        } else {
                                c += snprintf(&buf[c], sizeof(buf) - c, "AP: unknown: %d\n", bw);
                        }
                }

                if (wifi_mode_get() == ESP_WIFI_MODE_STA_AP || wifi_mode_get() == ESP_WIFI_MODE_STA) {
                        if (ESP_OK != esp_wifi_get_bandwidth(WIFI_IF_STA, &bw)) {
                                http_rpc.send(500, "text/plain", "error\n");
                        }

                        if (bw >= 0 && bw < ARRAY_SIZE(str_wifi_bw)) {
                                c += snprintf(&buf[c], sizeof(buf) - c, "STA: %s\n", str_wifi_bw[bw]);
                        } else {
                                c += snprintf(&buf[c], sizeof(buf) - c, "STA: unknown: %d\n", bw);
                        }
                }

                http_rpc.send(200, "text/plain", buf);
        });
#endif

        http_rpc.on("/wifi_sta_rssi", HTTP_GET, [](){
                char buf[16] = { };
                int rssi = 0;

                if (ESP_OK != esp_wifi_sta_get_rssi(&rssi)) {
                        http_rpc.send(500, "text/plain", "error\n");
                }

                snprintf(buf, sizeof(buf), "%u\n", rssi);

                http_rpc.send(200, "text/plain", buf);
        });

        http_rpc.on("/wifi_tx_power", HTTP_GET, [](){
                char buf[16] = { };
                int8_t pwr = 0;

                if (ESP_OK != esp_wifi_get_max_tx_power(&pwr)) {
                        http_rpc.send(500, "text/plain", "error\n");
                }

                snprintf(buf, sizeof(buf), "%u dBm\n", pwr / 4);

                http_rpc.send(200, "text/plain", buf);
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