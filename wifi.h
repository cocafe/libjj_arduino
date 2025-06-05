#ifndef __LIBJJ_WIFI_H__
#define __LIBJJ_WIFI_H__

#include <stdio.h>

#include <WiFi.h>

#include "utils.h"
#include "leds.h"
#include "logging.h"

#define WIFI_CONNECT_TIMEOUT_SEC        (60)

enum {
        WIFI_EVENT_CONNECTED,
        WIFI_EVENT_CONNECTING,
        WIFI_EVENT_DISCONNECTED,
        NUM_WIFI_EVENTS,
};

struct wifi_event_cb_ctx {
        void (*cb)(int event);
};

static struct wifi_event_cb_ctx wifi_event_cbs[4] = { };
static uint8_t wifi_event_cb_cnt;

// FIXME: can be racing
#define wifi_event_cb_add(_cb)                                  \
do {                                                            \
        if (wifi_event_cb_cnt < ARRAY_SIZE(wifi_event_cbs)) {   \
                wifi_event_cbs[wifi_event_cb_cnt++].cb = _cb;   \
        }                                                       \
} while (0)

static wifi_power_t __wifi_tx_power = WIFI_POWER_19_5dBm;
static wifi_mode_t __wifi_mode = WIFI_OFF;
static size_t wifi_idx = 0;

struct strval cfg_wifi_txpwr[] = {
        { "19.5dBm",            WIFI_POWER_19_5dBm      },
        { "19dBm",              WIFI_POWER_19dBm        },
        { "18.5dBm",            WIFI_POWER_18_5dBm      },
        { "17dBm",              WIFI_POWER_17dBm        },
        { "15dBm",              WIFI_POWER_15dBm        },
        { "13dBm",              WIFI_POWER_13dBm        },
        { "11dBm",              WIFI_POWER_11dBm        },
        { "8.5dBm",             WIFI_POWER_8_5dBm       },
        { "7dBm",               WIFI_POWER_7dBm         },
        { "5dBm",               WIFI_POWER_5dBm         },
        { "2dBm",               WIFI_POWER_2dBm         },
        { "-1dBm",              WIFI_POWER_MINUS_1dBm   },
};

struct wifi_nw_cfg {
        char ssid[64];
        char passwd[64];
        uint16_t timeout_sec;
        uint8_t use_dhcp;
        char local[32];
        char gw[32];
        char subnet[32];
        int32_t tx_pwr;
};

static struct wifi_nw_cfg __attribute__((unused)) wifi_sta_failsafe = {
        "0xC0CAFE",
        "jijijiji",
        10,
        1,
        "\0",
        "\0",
        "\0",
        WIFI_POWER_2dBm,
};

static void wifi_event_call(int event)
{
        for (int i = 0; i < ARRAY_SIZE(wifi_event_cbs); i++) {
                if (wifi_event_cbs[i].cb)
                        wifi_event_cbs[i].cb(event);
        }
}

static void __wifi_init(void)
{
        WiFi.useStaticBuffers(true);
        WiFi.setSleep(false);
        WiFi.setTxPower(__wifi_tx_power);
        WiFi.mode(__wifi_mode);
}

static inline void wifi_tx_power_print(int val)
{
        int found = 0;

        for (int i = 0; i < ARRAY_SIZE(cfg_wifi_txpwr); i++) {
                if (cfg_wifi_txpwr[i].val == val) {
                        pr_info("tx power: %s\n", cfg_wifi_txpwr[i].str);
                        return;
                }
        }

        if (!found)
                pr_info("unknown tx power\n");
}

static __attribute__((unused)) void wifi_sta_init(struct wifi_nw_cfg *nw)
{
        wifi_tx_power_print(nw->tx_pwr);
        __wifi_tx_power = (wifi_power_t)nw->tx_pwr;
        __wifi_mode = WIFI_STA;
        __wifi_init();
}

static __attribute__((unused)) int wifi_ap_init(struct wifi_nw_cfg *nw)
{
        IPAddress local, gw, subnet;

        if (!local.fromString(nw->local) || !gw.fromString(nw->gw) || !subnet.fromString(nw->subnet))
                return 1;

        wifi_tx_power_print(nw->tx_pwr);
        __wifi_tx_power = (wifi_power_t)nw->tx_pwr;
        __wifi_mode = WIFI_AP;
        __wifi_init();
        WiFi.softAPConfig(local, gw, subnet);
        WiFi.softAP(nw->ssid, nw->passwd);

        return 0;
}

static __attribute__((unused)) void wifi_connect(struct wifi_nw_cfg *cfg)
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

static __attribute__((unused)) void wifi_reconnect(struct wifi_nw_cfg *cfg)
{
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        vTaskDelay(pdMS_TO_TICKS(500));
        __wifi_init();
        wifi_connect(cfg);
}

static __attribute__((unused)) int wifi_first_connect(struct wifi_nw_cfg **cfgs, size_t cfg_cnt)
{
        int idx = 0;
        int ok = 0;

#ifdef WIFI_CONN_LED_BLINK
        uint8_t wifi_led = WIFI_CONN_LED_BLINK;
#endif

        while (!ok) {
                struct wifi_nw_cfg *cfg = cfgs[idx];
                int timeout = cfg->timeout_sec ? cfg->timeout_sec : WIFI_CONNECT_TIMEOUT_SEC;

                wifi_reconnect(cfg);

                pr_info("connecting to SSID \"%s\" ...\n", cfg->ssid);
                for (int i = 0; i < timeout; i++) {
                        if (WiFi.status() == WL_CONNECTED) {
                                ok = 1;
#ifdef WIFI_CONN_LED_BLINK
                                led_on(wifi_led, 0, 0, 255);
#endif
                                break;
                        }

#ifdef WIFI_CONN_LED_BLINK
                        led_on(wifi_led, 255, 0, 0);
                        vTaskDelay(pdMS_TO_TICKS(500));
                        led_off(wifi_led);
                        vTaskDelay(pdMS_TO_TICKS(500));
#else
                        vTaskDelay(pdMS_TO_TICKS(1000));
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

#endif // __LIBJJ_WIFI_H__