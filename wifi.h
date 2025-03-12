#ifndef __LIBJJ_WIFI_H__
#define __LIBJJ_WIFI_H__

#include <stdio.h>

#include <WiFi.h>

#include "utils.h"
#include "leds.h"

#define WIFI_CONNECT_TIMEOUT_SEC        (60)

static wifi_power_t __wifi_tx_power = WIFI_POWER_19_5dBm;

struct wifi_nw_cfg {
        char ssid[64];
        char passwd[64];
        uint16_t timeout_sec;
        uint8_t use_dhcp;
        char local[32];
        char gw[32];
        char subnet[32];
};

static struct wifi_nw_cfg wifi_failsafe = {
        "0xC0CAFE",
        "jijijiji",
        10,
        1,
        "\0",
        "\0",
        "\0",
};

static void __wifi_init(void)
{
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(500);
        WiFi.useStaticBuffers(true);
        WiFi.setSleep(false);
        WiFi.setTxPower(__wifi_tx_power);
        WiFi.mode(WIFI_STA);
}

static void wifi_init(wifi_power_t tx_pwr)
{
        __wifi_tx_power = tx_pwr;
        __wifi_init();
}

static void wifi_connect(struct wifi_nw_cfg *cfg)
{
        // reset once
        __wifi_init();

        if (!cfg->use_dhcp) {
                IPAddress local, gw, subnet;

                local.fromString(cfg->local);
                gw.fromString(cfg->gw);
                subnet.fromString(cfg->subnet);

                WiFi.config(local, gw, subnet);
        }

        WiFi.begin(cfg->ssid, cfg->passwd);
}

static int wifi_first_connect(struct wifi_nw_cfg **cfgs, size_t cfg_cnt)
{
        int idx = 0;
        int ok = 0;

        while (!ok) {
                struct wifi_nw_cfg *cfg = cfgs[idx];
                int timeout = cfg->timeout_sec ? cfg->timeout_sec : WIFI_CONNECT_TIMEOUT_SEC;

                wifi_connect(cfg);

                printf("connecting to SSID \"%s\" ", cfg->ssid);
                for (int i = 0; i < timeout; i++) {
                        if (WiFi.status() == WL_CONNECTED) {
                                ok = 1;
                                break;
                        }

                        led_on();
                        delay(500);
                        led_off();
                        delay(500);
                        printf(".");
                }
                printf("\n");

                if (!ok) {
                        size_t next = idx + 1 >= cfg_cnt ? 0 : idx + 1;
                        printf("failed to connect to \"%s\", try \"%s\"\n", cfg->ssid, cfgs[next]->ssid);
                        idx = next;
                }
        }

        return idx;
}

#endif // __LIBJJ_WIFI_H__