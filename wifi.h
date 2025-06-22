#ifndef __LIBJJ_WIFI_H__
#define __LIBJJ_WIFI_H__

#include <stdio.h>

#include <esp_wifi.h>

#include <WiFi.h>

#include <freertos/semphr.h>

#include "utils.h"
#include "leds.h"
#include "logging.h"

#define WIFI_CONNECT_TIMEOUT_SEC        (60)

static SemaphoreHandle_t sem_wifi_wait;

enum {
        WIFI_EVENT_CONNECTED,
        WIFI_EVENT_CONNECTING,
        WIFI_EVENT_DISCONNECTED,
        WIFI_EVENT_IP_GOT,
        NUM_WIFI_EVENTS,
};

enum {
        ESP_WIFI_MODE_OFF,
        ESP_WIFI_MODE_STA,
        ESP_WIFI_MODE_AP,
        ESP_WIFI_MODE_STA_AP,
        NUM_ESP_WIFI_MODES,
};

static __unused const char *str_wifi_modes[] = {
        [ESP_WIFI_MODE_OFF]     = "OFF",
        [ESP_WIFI_MODE_STA]     = "STA",
        [ESP_WIFI_MODE_AP]      = "AP",
        [ESP_WIFI_MODE_STA_AP]  = "STA_AP",
};

static const char *str_wifi_ps_modes[] = {
        [WIFI_PS_NONE]          = "NONE",
        [WIFI_PS_MIN_MODEM]     = "PS_MIN",     // enabled PS
        [WIFI_PS_MAX_MODEM]     = "PS_MAX",     // MAX PS
};

struct wifi_event_cb_ctx {
        void (*cb)(int event);
};

#ifdef WIFI_CONN_LED_BLINK
uint8_t wifi_led = WIFI_CONN_LED_BLINK;
uint8_t wifi_led_blink = 1;
#endif

static struct wifi_event_cb_ctx wifi_event_cbs[4] = { };
static uint8_t wifi_event_cb_cnt;

static SemaphoreHandle_t lck_wifi_cb;

static __unused int wifi_event_cb_register(void (*cb)(int event))
{
        int err = 0;

        xSemaphoreTake(lck_wifi_cb, portMAX_DELAY);

        if (wifi_event_cb_cnt >= ARRAY_SIZE(wifi_event_cbs)) {
                err = -ENOSPC;
                goto unlock;
        }

        wifi_event_cbs[wifi_event_cb_cnt++].cb = cb;

unlock:
        xSemaphoreGive(lck_wifi_cb);

        return err;
}

static wifi_power_t __wifi_tx_power = WIFI_POWER_19_5dBm;
static wifi_mode_t __wifi_mode = WIFI_OFF;
static __unused size_t wifi_idx = 0;

enum {
        ESP_WIFI_POWER_21dBm,
        ESP_WIFI_POWER_20_5dBm,
        ESP_WIFI_POWER_20dBm,
        ESP_WIFI_POWER_19_5dBm,
        ESP_WIFI_POWER_19dBm,
        ESP_WIFI_POWER_18_5dBm,
        ESP_WIFI_POWER_17dBm,
        ESP_WIFI_POWER_15dBm,
        ESP_WIFI_POWER_13dBm,
        ESP_WIFI_POWER_11dBm,
        ESP_WIFI_POWER_8_5dBm,
        ESP_WIFI_POWER_7dBm,
        ESP_WIFI_POWER_5dBm,
        ESP_WIFI_POWER_2dBm,
        ESP_WIFI_POWER_MINUS_1dBm,
        NUM_ESP_WIFI_TXPWR,
};

static const char *str_wifi_txpwr[NUM_ESP_WIFI_TXPWR] = {
        [ESP_WIFI_POWER_21dBm]          = "21dBm",
        [ESP_WIFI_POWER_20_5dBm]        = "20.5dBm",
        [ESP_WIFI_POWER_20dBm]          = "20dBm",
        [ESP_WIFI_POWER_19_5dBm]        = "19.5dBm",
        [ESP_WIFI_POWER_19dBm]          = "19dBm",
        [ESP_WIFI_POWER_18_5dBm]        = "18.5dBm",
        [ESP_WIFI_POWER_17dBm]          = "17dBm",
        [ESP_WIFI_POWER_15dBm]          = "15dBm",
        [ESP_WIFI_POWER_13dBm]          = "13dBm",
        [ESP_WIFI_POWER_11dBm]          = "11dBm",
        [ESP_WIFI_POWER_8_5dBm]         = "8.5dBm",
        [ESP_WIFI_POWER_7dBm]           = "7dBm",
        [ESP_WIFI_POWER_5dBm]           = "5dBm",
        [ESP_WIFI_POWER_2dBm]           = "2dBm",
        [ESP_WIFI_POWER_MINUS_1dBm]     = "-1dBm",
};

static int cfg_wifi_txpwr_convert[NUM_ESP_WIFI_TXPWR] = {
        [ESP_WIFI_POWER_21dBm]          = WIFI_POWER_21dBm,
        [ESP_WIFI_POWER_20_5dBm]        = WIFI_POWER_20_5dBm,
        [ESP_WIFI_POWER_20dBm]          = WIFI_POWER_20dBm,
        [ESP_WIFI_POWER_19_5dBm]        = WIFI_POWER_19_5dBm,
        [ESP_WIFI_POWER_19dBm]          = WIFI_POWER_19dBm,
        [ESP_WIFI_POWER_18_5dBm]        = WIFI_POWER_18_5dBm,
        [ESP_WIFI_POWER_17dBm]          = WIFI_POWER_17dBm,
        [ESP_WIFI_POWER_15dBm]          = WIFI_POWER_15dBm,
        [ESP_WIFI_POWER_13dBm]          = WIFI_POWER_13dBm,
        [ESP_WIFI_POWER_11dBm]          = WIFI_POWER_11dBm,
        [ESP_WIFI_POWER_8_5dBm]         = WIFI_POWER_8_5dBm,
        [ESP_WIFI_POWER_7dBm]           = WIFI_POWER_7dBm,
        [ESP_WIFI_POWER_5dBm]           = WIFI_POWER_5dBm,
        [ESP_WIFI_POWER_2dBm]           = WIFI_POWER_2dBm,
        [ESP_WIFI_POWER_MINUS_1dBm]     = WIFI_POWER_MINUS_1dBm,
};

struct wifi_nw_cfg {
        char ssid[64];
        char passwd[64];
        uint16_t timeout_sec;
        uint8_t use_dhcp;
        char local[32];
        char gw[32];
        char subnet[32];
};

static struct wifi_nw_cfg __unused wifi_sta_failsafe = {
        "0xC0CAFE",
        "jijijiji",
        10,
        1,
        "\0",
        "\0",
        "\0",
};

static __unused int wifi_mode_get(void)
{
        return __wifi_mode;
}

static __unused void wifi_event_call(int event)
{
        pr_dbg("event: %d\n", event);

        for (int i = 0; i < ARRAY_SIZE(wifi_event_cbs); i++) {
                if (wifi_event_cbs[i].cb)
                        wifi_event_cbs[i].cb(event);
        }
}

static int wifi_early_init(void)
{
        lck_wifi_cb = xSemaphoreCreateMutex();
        sem_wifi_wait = xSemaphoreCreateBinary();

        return 0;
}

static void __wifi_init(void)
{
        WiFi.useStaticBuffers(CONFIG_WIFI_USE_STATIC_BUF);
        WiFi.enableLongRange(CONFIG_WIFI_USE_LONG_RANGE);
        WiFi.setSleep(false);
        WiFi.setTxPower(__wifi_tx_power);
        WiFi.mode(__wifi_mode);
        esp_wifi_set_ps(WIFI_PS_NONE);
        esp_wifi_set_dynamic_cs(true);
}

static inline void wifi_tx_power_print(int val)
{
        if (val >= ARRAY_SIZE(str_wifi_txpwr)) {
                pr_info("unknown tx power\n");
                return;
        }

        pr_info("tx power: %s\n", str_wifi_txpwr[val]);
}

static inline void wifi_tx_power_set(int txpwr)
{
        if (txpwr < ARRAY_SIZE(str_wifi_txpwr)) {
                wifi_tx_power_print(txpwr);
                __wifi_tx_power = (wifi_power_t)cfg_wifi_txpwr_convert[txpwr];
        }
}

static __unused int wifi_sta_init(struct wifi_nw_cfg *nw, int txpwr)
{
        IPAddress local, gw, subnet;

        if (!local.fromString(nw->local) || !gw.fromString(nw->gw) || !subnet.fromString(nw->subnet))
                return -EINVAL;

        wifi_tx_power_set(txpwr);

        __wifi_mode = WIFI_STA;
        __wifi_init();

        WiFi.setAutoReconnect(false);

        return 0;
}

static __unused int wifi_ap_init(struct wifi_nw_cfg *nw, int txpwr)
{
        IPAddress local, gw, subnet;

        if (!local.fromString(nw->local) || !gw.fromString(nw->gw) || !subnet.fromString(nw->subnet))
                return -EINVAL;

        wifi_tx_power_set(txpwr);

        __wifi_mode = WIFI_AP;
        __wifi_init();
        WiFi.softAPConfig(local, gw, subnet);
        WiFi.softAP(nw->ssid, nw->passwd);

        return 0;
}

static __unused int wifi_sta_ap_init(struct wifi_nw_cfg *sta, struct wifi_nw_cfg *ap, int txpwr)
{
        IPAddress local, gw, subnet;

        if (!local.fromString(sta->local) || !gw.fromString(sta->gw) || !subnet.fromString(sta->subnet))
                return -EINVAL;

        // validate and use for AP
        if (!local.fromString(ap->local) || !gw.fromString(ap->gw) || !subnet.fromString(ap->subnet))
                return -EINVAL;

        wifi_tx_power_set(txpwr);

        __wifi_mode = WIFI_AP_STA;
        __wifi_init();

        WiFi.setAutoReconnect(false);
        WiFi.softAPConfig(local, gw, subnet);
        WiFi.softAP(ap->ssid, ap->passwd);

        return 0;
}

static __unused void wifi_connect(struct wifi_nw_cfg *cfg)
{
        if (!cfg->use_dhcp) {
                IPAddress local, gw, subnet;

                local.fromString(cfg->local);
                gw.fromString(cfg->gw);
                subnet.fromString(cfg->subnet);

                WiFi.config(local, gw, subnet);
        }

        WiFi.begin(cfg->ssid, cfg->passwd);
}

static __unused void wifi_reconnect(struct wifi_nw_cfg *cfg)
{
        WiFi.disconnect(true);
        wifi_connect(cfg);
}

static __unused int wifi_first_connect(struct wifi_nw_cfg **cfgs, size_t cfg_cnt)
{
        int idx = 0;
        int ok = 0;

        while (!ok) {
                struct wifi_nw_cfg *cfg = cfgs[idx];
                int timeout = cfg->timeout_sec ? cfg->timeout_sec : WIFI_CONNECT_TIMEOUT_SEC;

                wifi_reconnect(cfg);

                pr_info("connecting to SSID \"%s\" ...\n", cfg->ssid);
                for (int i = 0; i < timeout * 2; i++) {
                        if (WiFi.status() == WL_CONNECTED) {
                                ok = 1;
#ifdef WIFI_CONN_LED_BLINK
                                led_on(wifi_led, 0, 0, 255);
#endif
                                break;
                        }

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
                }

                if (!ok) {
                        size_t next = idx + 1 >= cfg_cnt ? 0 : idx + 1;
                        pr_info("failed to connect to \"%s\" after %d secs, try \"%s\"\n", cfg->ssid, timeout, cfgs[next]->ssid);
                        idx = next;
                }
        }

        pr_info("connected and bind to SSID \"%s\"\n", cfgs[idx]->ssid);

        return idx;
}

static __unused void wifi_connection_check_post(void)
{
        xSemaphoreGive(sem_wifi_wait);
}

void wifi_sys_event_cb(arduino_event_id_t event)
{
        switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                wifi_event_call(WIFI_EVENT_CONNECTED);
                break;

        // XXX: if already disconnected,
        //      this event still will be called again
        //      if WiFi.disconnect(true) is called
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                wifi_event_call(WIFI_EVENT_DISCONNECTED);
                break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                wifi_event_call(WIFI_EVENT_IP_GOT);
                break;

        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        case ARDUINO_EVENT_WIFI_READY:
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
        case ARDUINO_EVENT_WIFI_STA_START:
        case ARDUINO_EVENT_WIFI_STA_STOP:
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
        default:
                break;
        }
}

#endif // __LIBJJ_WIFI_H__