#ifndef __LIBJJ_TASK_WIFI_CONN_H__
#define __LIBJJ_TASK_WIFI_CONN_H__

#include "ping.h"

static void task_wifi_conn(void *arg)
{
        const unsigned ping_failure_thres = 5;
        unsigned ping_failure = 0;

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

                wifi_sta_cfg_apply_once(&g_cfg.wifi_cfg);
                esp_wifi_force_wakeup_acquire();
                pr_info("wifi connected, RSSI %d dBm, BSSID: %s\n", WiFi.RSSI(), WiFi.BSSIDstr().c_str());
                pr_info("ip local: %s gw: %s subnet: %s dns: %s\n",
                        WiFi.localIP().toString().c_str(),
                        WiFi.gatewayIP().toString().c_str(),
                        WiFi.subnetMask().toString().c_str(),
                        WiFi.dnsIP().toString().c_str());

#ifdef WIFI_CONN_LED_BLINK
                led_on(wifi_led, 0, 0, 255);
#endif

                while (WiFi.status() == WL_CONNECTED) {
                        if (WiFi.gatewayIP().toString() != "0.0.0.0") {
                                if (ping4(WiFi.gatewayIP(), 1, 500, 500) != 1) {
                                        ping_failure++;

                                        if (ping_failure >= ping_failure_thres) {
                                                pr_err("ping continuously failed for %u times, wifi may lost, reconnect now\n", ping_failure_thres);
                                                break;
                                        }
                                } else {
                                        ping_failure = 0;
                                }
                        }

                        xSemaphoreTake(sem_wifi_wait, pdMS_TO_TICKS(5000));
                }

                pr_info("wifi connection lost, reconnect now\n");
                esp_wifi_force_wakeup_release();
                wifi_reconnect(wifi_cfg);
        }
}

static __unused void task_wifi_conn_start(unsigned cpu)
{
        xTaskCreatePinnedToCore(task_wifi_conn, "wifi_conn", 4096, NULL, 1, NULL, cpu);
}

static void task_wifi_ap_blink(void *arg)
{
        uint8_t last_sta_num = 0;

        while (1) {
                uint8_t sta_num = WiFi.softAPgetStationNum();

                if (!wifi_led_blink) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        continue;
                }

                if (sta_num == 0) {
                        static uint8_t pattern[] = { 1, 0, 0, 0, 0, 0 };
                        static uint8_t i = 0;

                        if (pattern[i]) {
                                led_on(wifi_led, 0, 255, 0);
                        } else {
                                led_off(wifi_led);
                        }

                        i++;
                        if (i >= ARRAY_SIZE(pattern))
                                i = 0;
                } else {
                        if (last_sta_num == 0)
                                led_on(wifi_led, 0, 255, 0);
                }

                last_sta_num = sta_num;
                vTaskDelay(pdMS_TO_TICKS(1000));
        }
}

static __unused void task_wifi_ap_blink_start(unsigned cpu)
{
        xTaskCreatePinnedToCore(task_wifi_ap_blink, "wifi_blink", 2048, NULL, 1, NULL, cpu);
}

#endif // __LIBJJ_TASK_WIFI_CONN_H__