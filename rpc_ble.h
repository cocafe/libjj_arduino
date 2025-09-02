#ifndef __LIBJJ_RPC_BLE_H__
#define __LIBJJ_RPC_BLE_H__

void rpc_ble_add(void)
{
        http_rpc.on("/ble_txpower", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        String data = http_rpc.arg("set");
                        int match = 0;
                        int i = 0;

                        for (i = 0; i < ARRAY_SIZE(str_ble_txpwr); i++) {
                                if (is_str_equal((char *)str_ble_txpwr[i], (char *)data.c_str(), CASELESS)) {
                                        match = 1;
                                        break;
                                }
                        }

                        if (!match) {
                                http_rpc.send(404, "text/plain", "Invalid value\n");
                                return;
                        }

                        if (esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ble_txpwr_to_esp_value[i]) != ESP_OK)
                                http_rpc.send(200, "text/plain", "error\n");
                        else
                                http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[1024] = { };
                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "\n");

                        for (unsigned i = 0; i < ESP_BLE_PWR_TYPE_NUM; i++) {
                                int esp_val = esp_ble_tx_power_get((esp_ble_power_type_t)i);

                                c += snprintf(&buf[c], sizeof(buf) - c, "%s ", str_ble_pwr_types[i]);

                                if (esp_val < 0 || esp_val >= ARRAY_SIZE(ble_txpwr_from_esp_value)) {
                                        c += snprintf(&buf[c], sizeof(buf) - c, "(unknown: %d)\n", esp_val);
                                } else {
                                        c += snprintf(&buf[c], sizeof(buf) - c, "%s\n", str_ble_txpwr[ble_txpwr_from_esp_value[esp_val]]);
                                }
                        }

                        http_rpc.send(200, "text/plain", buf);
                }
        });
}

#endif // __LIBJJ_RPC_BLE_H__