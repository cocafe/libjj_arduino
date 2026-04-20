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

        http_rpc.on("/ble_conn_param", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        nimble_conn_param_update(rc_ble.server, &rc_ble.cfg->conn, rc_ble.conn_hdl);
                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        if (!ble_is_connected) {
                                http_rpc.send(500, "text/plain", "not connected\n");
                                return;
                        }

                        NimBLEConnInfo conn_info = rc_ble.server->getPeerInfoByHandle(rc_ble.conn_hdl);
                        char buf[512] = { };
                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "intv: %f ms\n", conn_info.getConnInterval() * 125 / (float)100.0);
                        c += snprintf(&buf[c], sizeof(buf) - c, "latency: %u\n", conn_info.getConnLatency());
                        c += snprintf(&buf[c], sizeof(buf) - c, "timeout: %u ms\n", conn_info.getConnTimeout() * 10);
                        c += snprintf(&buf[c], sizeof(buf) - c, "mtu: %u\n", conn_info.getMTU());

                        http_rpc.send(200, "text/plain", buf);
                }
        });

        http_rpc.on("/ble_phy", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[512] = { };
                        size_t c = 0;
                        uint8_t tx_phy = 0, rx_phy = 0;

                        if (!ble_is_connected) {
                                http_rpc.send(500, "text/plain", "not connected\n");
                                return;
                        }

                        if (rc_ble.server->getPhy(rc_ble.conn_hdl, &tx_phy, &rx_phy) == false) {
                                http_rpc.send(500, "text/plain", "error\n");
                                return;
                        }

                        c += snprintf(&buf[c], sizeof(buf) - c, "tx_phy: ");
                        c += snprintf(&buf[c], sizeof(buf) - c, "%s%s%s\n",
                                (tx_phy & BLE_GAP_LE_PHY_1M_MASK) ? "1M" : "",
                                (tx_phy & BLE_GAP_LE_PHY_2M_MASK) ? "2M" : "",
                                (tx_phy & BLE_GAP_LE_PHY_CODED_MASK) ? "Coded" : "");
                        c += snprintf(&buf[c], sizeof(buf) - c, "rx_phy: ");
                        c += snprintf(&buf[c], sizeof(buf) - c, "%s%s%s\n",
                                (rx_phy & BLE_GAP_LE_PHY_1M_MASK) ? "1M" : "",
                                (rx_phy & BLE_GAP_LE_PHY_2M_MASK) ? "2M" : "",
                                (rx_phy & BLE_GAP_LE_PHY_CODED_MASK) ? "Coded" : "");

                        http_rpc.send(200, "text/plain", buf);
                }
        });
}

#endif // __LIBJJ_RPC_BLE_H__