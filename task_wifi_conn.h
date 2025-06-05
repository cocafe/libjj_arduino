#ifndef __LIBJJ_TASK_WIFI_CONN_H__
#define __LIBJJ_TASK_WIFI_CONN_H__

static void task_wifi_conn(void *arg)
{
#ifdef WIFI_CONN_LED_BLINK
        uint8_t wifi_led = WIFI_CONN_LED_BLINK;
#endif

        pr_info("started\n");

        while (1) {
                struct wifi_nw_cfg *wifi_cfg = wifi_configs[wifi_idx];

                if (WiFi.status() != WL_CONNECTED) {
                        pr_info("connecting to SSID \"%s\" ...\n", wifi_cfg->ssid);

                        while (WiFi.status() != WL_CONNECTED) {
#ifdef WIFI_CONN_LED_BLINK
                                led_on(wifi_led, 255, 0, 0);
                                vTaskDelay(pdMS_TO_TICKS(500));
                                led_off(wifi_led);
                                vTaskDelay(pdMS_TO_TICKS(500));
#else
                                vTaskDelay(pdMS_TO_TICKS(1000));
#endif // #ifdef WIFI_CONN_LED_BLINK
                        }
                }

                pr_info("wifi connected, RSSI %d dBm\n", WiFi.RSSI());
                wifi_event_call(WIFI_EVENT_CONNECTED);

#ifdef WIFI_CONN_LED_BLINK
                led_on(wifi_led, 0, 0, 255);
#endif

                while (WiFi.status() == WL_CONNECTED) {
                        vTaskDelay(pdMS_TO_TICKS(5000));
                }

                pr_info("wifi connection lost\n");
                wifi_reconnect(wifi_cfg);
                wifi_event_call(WIFI_EVENT_DISCONNECTED);
        }
}

static __attribute__((unused)) void task_wifi_conn_start(unsigned cpu)
{
        xTaskCreatePinnedToCore(task_wifi_conn, "wifi_conn", 4096, NULL, 1, NULL, cpu);
}

#endif // __LIBJJ_TASK_WIFI_CONN_H__