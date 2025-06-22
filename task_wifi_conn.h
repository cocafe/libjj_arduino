#ifndef __LIBJJ_TASK_WIFI_CONN_H__
#define __LIBJJ_TASK_WIFI_CONN_H__

static void task_wifi_conn(void *arg)
{
        vTaskDelay(pdMS_TO_TICKS(1000));

        WiFi.onEvent(wifi_sys_event_cb, ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent(wifi_sys_event_cb, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        WiFi.onEvent(wifi_sys_event_cb, ARDUINO_EVENT_WIFI_STA_GOT_IP);

        wifi_idx = wifi_first_connect(wifi_configs, ARRAY_SIZE(wifi_configs));

        while (1) {
                struct wifi_nw_cfg *wifi_cfg = wifi_configs[wifi_idx];

                if (WiFi.status() != WL_CONNECTED) {
                        pr_info("connecting to SSID \"%s\" ...\n", wifi_cfg->ssid);
                        uint32_t ts_start_connect = esp32_millis();

                        while (WiFi.status() != WL_CONNECTED) {
#ifdef WIFI_CONN_LED_BLINK
                                if (wifi_led_blink) {
                                        led_on(wifi_led, 255, 0, 0);
                                        vTaskDelay(pdMS_TO_TICKS(250));
                                        led_off(wifi_led);
                                        vTaskDelay(pdMS_TO_TICKS(250));
                                } else {
                                        vTaskDelay(pdMS_TO_TICKS(500));
                                }
#else
                                vTaskDelay(pdMS_TO_TICKS(500));
#endif // #ifdef WIFI_CONN_LED_BLINK

                                if (esp32_millis() - ts_start_connect >= (wifi_cfg->timeout_sec * 1000)) {
                                        pr_info("connection timed out, try again\n");
                                        ts_start_connect = esp32_millis();
                                        wifi_reconnect(wifi_cfg);
                                }
                        }
                }

                pr_info("wifi connected, RSSI %d dBm, BSSID: %s\n", WiFi.RSSI(), WiFi.BSSIDstr().c_str());

#ifdef WIFI_CONN_LED_BLINK
                led_on(wifi_led, 0, 0, 255);
#endif

                while (WiFi.status() == WL_CONNECTED) {
                        xSemaphoreTake(sem_wifi_wait, pdMS_TO_TICKS(5000));
                }

                pr_info("wifi connection lost, reconnect now\n");
                wifi_reconnect(wifi_cfg);
        }
}

static __unused void task_wifi_conn_start(unsigned cpu)
{
        xTaskCreatePinnedToCore(task_wifi_conn, "wifi_conn", 4096, NULL, 1, NULL, cpu);
}

#endif // __LIBJJ_TASK_WIFI_CONN_H__