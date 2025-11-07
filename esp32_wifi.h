#ifndef __LIBJJ_ESP32_WIFI_H__
#define __LIBJJ_ESP32_WIFI_H__

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif_net_stack.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#if IP_NAPT
#include <lwip/lwip_napt.h>
#endif
#include <lwip/err.h>
#include <lwip/sys.h>

#include "list.h"
#include "utils.h"
#include "ping.h"

#ifndef WIFI_DEFAULT_MAC
#define WIFI_DEFAULT_MAC                ESP_MAC_EFUSE_FACTORY
#endif

#define WIFI_CHANNEL_AUTO               (0)

enum {
        ESP_WIFI_MODE_OFF,
        ESP_WIFI_MODE_STA,
        ESP_WIFI_MODE_AP,
        ESP_WIFI_MODE_STA_AP,
        NUM_ESP_WIFI_MODES,
};

static int cfg_wifi_mode_convert[NUM_ESP_WIFI_MODES] = {
        [ESP_WIFI_MODE_OFF]     = WIFI_MODE_NULL,
        [ESP_WIFI_MODE_STA]     = WIFI_MODE_STA,
        [ESP_WIFI_MODE_AP]      = WIFI_MODE_AP,
        [ESP_WIFI_MODE_STA_AP]  = WIFI_MODE_APSTA,
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

#if ESP_IDF_VERSION_MAJOR >= 5 && ESP_IDF_VERSION_MINOR > 5
static const char *str_wifi_bands[] = {
        [WIFI_BAND_2G]          = "2.4G",
        [WIFI_BAND_5G]          = "5G",
};

static const char *str_wifi_band_modes[] = {
        [WIFI_BAND_MODE_2G_ONLY]        = "2.4G",
        [WIFI_BAND_MODE_5G_ONLY]        = "5G",
        [WIFI_BAND_MODE_AUTO]           = "AUTO",
};
#endif // ESP_IDF_VERSION_MAJOR

struct wifi_assoc_cfg {
        char ssid[24];
        char passwd[64];
        uint8_t auth;
        uint8_t cipher;
        uint8_t channel;
        char local[32];
        char gw[32];
        char subnet[32];
        char dns[32];
};

struct wifi_ap_cfg {
        struct wifi_assoc_cfg assoc;
        uint8_t dhcps;
        uint8_t max_sta;
        uint8_t csa_count;
        uint8_t dtim_period;
        uint8_t ssid_with_sn;
        uint16_t beacon_intv;
};

// TODO: HE VHT configs
// TODO: connection timed out
struct wifi_sta_cfg {
        struct wifi_assoc_cfg assoc;
        uint8_t dhcpc;
        uint8_t scan_mode;
        uint8_t rm_enabled;
        uint8_t btm_enabled;
        uint8_t mbo_enabled;
        uint8_t retry_count;
        uint16_t inactive_sec;
};

struct wifi_adv_cfg {
        uint8_t tx_power;
        uint8_t ps_mode;
        uint8_t dynamic_cs;
        uint8_t csi;
        uint8_t ampdu_rx;
        uint8_t ampdu_tx;
        uint8_t amsdu_tx;
        uint8_t sta_disconnected_pm;
};

struct wifi_buf_cfg {
        uint8_t enabled;
        uint8_t static_rx_buf_num;
        uint8_t dynamic_rx_buf_num;
        uint8_t static_tx_buf_num;
        uint8_t dynamic_tx_buf_num;
        uint8_t rx_mgmt_buf_num;
        uint8_t cache_tx_buf_num;
        uint8_t mgmt_sbuf_num;
        uint8_t tx_hetb_queue_num;
        uint16_t rx_ba_win;
};

// TODO: tx rate config
struct wifi_cfg {
        uint8_t mode;
        uint8_t band;
        struct wifi_adv_cfg adv;
        struct wifi_buf_cfg buf;
        struct wifi_ap_cfg ap;
        struct wifi_sta_cfg sta;
};

struct wifi_ctx {
        struct wifi_cfg *cfg;
        esp_netif_t *netif_ap;
        esp_netif_t *netif_sta;
};

struct wifi_event_cb_ctx {
        struct list_head node;
        void (*cb)(esp_event_base_t event_base, int32_t event_id, void *event_data, void *userdata);
        void *userdata;
};

static struct list_head wifi_event_cbs = LIST_HEAD_INIT(wifi_event_cbs);
static portMUX_TYPE lck_wifi_event_cbs = portMUX_INITIALIZER_UNLOCKED;

static struct wifi_ctx g_wifi_ctx;

static uint8_t wifi_sta_connected;
static uint8_t wifi_sta_ip_got;

static __unused int wifi_mode_get(void)
{
        return g_wifi_ctx.cfg ? g_wifi_ctx.cfg->mode : (int)ESP_WIFI_MODE_OFF;
}

void wifi_ipinfo_print(esp_netif_t *netif)
{
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                pr_info("address: " IPSTR "\n", IP2STR(&ip_info.ip));
                pr_info("netmask: " IPSTR "\n", IP2STR(&ip_info.netmask));
                pr_info("gateway: " IPSTR "\n", IP2STR(&ip_info.gw));
        } else {
                pr_err("failed to get IP info\n");
        }

        for (int i = 0; i < ESP_NETIF_DNS_MAX; i++)
        {
                esp_netif_dns_info_t dns_info = { };

                if (ESP_OK != esp_netif_get_dns_info(netif, (esp_netif_dns_type_t)i, &dns_info))
                        continue;

                if (dns_info.ip.u_addr.ip4.addr != 0) {
                        pr_info("DNS%d: " IPSTR "\n", i, IP2STR(&dns_info.ip.u_addr.ip4));
                }
        }
}

int wifi_netif_ip4_get(esp_netif_t *netif, esp_ip4_addr_t *out)
{
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                if (out)
                        *out = ip_info.ip;
                
                return 0;
        }

        return -EIO;
}

int wifi_netif_gw4_get(esp_netif_t *netif, esp_ip4_addr_t *out)
{
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                if (out)
                        *out = ip_info.gw;
                
                return 0;
        }

        return -EIO;
}

static TaskHandle_t task_handle_sta_ping;

static int task_wifi_conn_ping_cb(struct ping_ctx *ctx, struct pbuf *p, const ip_addr_t *addr)
{
        struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)((uint8_t *)(p->payload) + icmp_ip_hlen);

        if ((iecho->type == ICMP_ER) && (iecho->id == lwip_htons(PING_ID))) {
                int *ping_success = (int *)ctx->userdata;

                if (lwip_ntohs(iecho->seqno) != (ctx->seq)) {
                        pr_dbg("mismatched seq\n");
                } else {
                        *ping_success = 1;
                }

                // {
                //         uint32_t rtt = esp32_millis() - ctx->ts_send;
                //         pr_dbg("Reply from %s: icmp_seq=%d time=%lu ms\n",
                //                 ipaddr_ntoa(addr), lwip_ntohs(iecho->seqno), rtt);
                // }

                xSemaphoreGive(ctx->sem);

                return 0;
        }

        return -EINVAL;
}

static void task_wifi_sta_ping(void *arg)
{
        uint32_t ulNotifiedValue;
        const unsigned ping_failure_thres = 5;
        unsigned ping_failure = 0;
        int ping_success = 0;
        int need_reconnect = 0;
        struct ping_ctx *pctx = NULL;
        ip_addr_t last_tgt = { };

        while (1) {
                ip_addr_t dst = { };

                // wait for wifi events
                xTaskNotifyWait(0, 0, &ulNotifiedValue, portMAX_DELAY);

                if (!wifi_sta_connected || !wifi_sta_ip_got) {
                        continue;
                }

                pr_info("start ping daemon\n");

                ping_failure = 0;
                need_reconnect = 0;

                if (wifi_netif_gw4_get(g_wifi_ctx.netif_sta, (esp_ip4_addr_t *)&dst)) {
                        pr_err("failed to get gateway ip, abort\n");
                        continue;
                }

                // if different gateway free and realloc
                if (memcmp(&last_tgt, &dst, sizeof(ip_addr_t))) {
                        if (pctx) {
                                ping4_deinit(pctx);
                                pctx = NULL;
                        }
                        
                        pctx = ping4_init_ip4(dst, task_wifi_conn_ping_cb, (void *)&ping_success);
                        if (!pctx) {
                                pr_err("failed to init ping4 context, abort\n");
                                continue;
                        }
                }

                while (1) {
                        ping_success = 0;
                        ping4_send(pctx);

                        if (xSemaphoreTake(pctx->sem, pdMS_TO_TICKS(1000)) == pdFALSE) {
                                pr_info("ping timed out\n");
                                ping_failure++;
                        } else {
                                if (ping_success) {
                                        if (ping_failure)
                                                pr_info("ping failure cleared\n");

                                        ping_failure = 0;
                                } else {
                                        ping_failure++;
                                }
                        }

                        if (ping_failure >= ping_failure_thres) {
                                pr_err("ping continuously failed for %u times, wifi may lost, reconnect now\n", ping_failure_thres);
                                need_reconnect = 1;
                                break;
                        }

                        vTaskDelay(pdMS_TO_TICKS(1000));
                }

                if (need_reconnect && wifi_sta_connected) {
                        esp_wifi_disconnect();
                }
        }
}

static __unused int wifi_event_cb_register(void (*cb)(esp_event_base_t event_base, int32_t event_id, void *event_data, void *userdata), void *userdata)
{
        struct wifi_event_cb_ctx *cb_ctx = (struct wifi_event_cb_ctx *)calloc(1, sizeof(struct wifi_event_cb_ctx));

        if (!cb_ctx) {
                pr_err("no memory\n");
                return -ENOMEM;
        }

        INIT_LIST_HEAD(&cb_ctx->node);
        cb_ctx->cb = cb;
        cb_ctx->userdata = userdata;

        taskENTER_CRITICAL(&lck_wifi_event_cbs);
        list_add(&cb_ctx->node, &wifi_event_cbs);
        taskEXIT_CRITICAL(&lck_wifi_event_cbs);

        return 0;
}

static void wifi_event_cb_call(esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
        struct list_head *n;
        list_for_each(n, &wifi_event_cbs) {
                struct wifi_event_cb_ctx *cb_ctx = list_container_of(n, struct wifi_event_cb_ctx, node);
                if (cb_ctx->cb) {
                        cb_ctx->cb(event_base, event_id, event_data, cb_ctx->userdata);
                }
        }
}

static void wifi_sta_reconnect_timer(void *arg)
{
        pr_info("try reconnect to %s\n", g_wifi_ctx.cfg->sta.assoc.ssid);
        esp_wifi_connect();
}

static esp_timer_handle_t timer_wifi_sta_reconn;
static const esp_timer_create_args_t timer_args_wifi_sta_reconn = {
        .callback = wifi_sta_reconnect_timer,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sta_reconn",
};

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html#wi-fi-reason-code
static void __wifi_event_handle_internal(esp_event_base_t event_base,
                                         int32_t event_id,
                                         void *event_data)
{
        if (event_base == WIFI_EVENT) {
                switch (event_id) {
                case WIFI_EVENT_AP_STACONNECTED: {
                        wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)event_data;
                        pr_info("sta " MACSTR " joined, AID: %d\n", MAC2STR(e->mac), e->aid);
                        break;
                }
                
                case WIFI_EVENT_AP_STADISCONNECTED: {
                        wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)event_data;
                        pr_info("sta " MACSTR " left, AID: %d reason: %d\n", MAC2STR(e->mac), e->aid, e->reason);
                        break;
                }
                
                case WIFI_EVENT_STA_START:
                        esp_wifi_connect();
                        break;
                
                case WIFI_EVENT_STA_CONNECTED: {
                        wifi_event_sta_connected_t *e = (wifi_event_sta_connected_t *)event_data;
                        pr_info("sta connected to %s (" MACSTR "), ch: %u\n", e->ssid, MAC2STR(e->bssid), e->channel);
                        wifi_sta_connected = 1;
                        break;
                }

                case WIFI_EVENT_STA_DISCONNECTED: {
                        wifi_event_sta_disconnected_t *e = (wifi_event_sta_disconnected_t *)event_data;
                        pr_info("sta disconnect %s (" MACSTR "), reason: %d rssi: %d\n", e->ssid, MAC2STR(e->bssid), e->reason, e->rssi);
                        wifi_sta_connected = 0;

                        if (!esp_timer_is_active(timer_wifi_sta_reconn)) {
                                pr_info("schedule to reconnect to AP\n");
                                esp_timer_start_once(timer_wifi_sta_reconn, 1ULL * 1000 * 1000);
                        }

                        break;
                }

                case WIFI_EVENT_AP_START:
                case WIFI_EVENT_AP_STOP:
                case WIFI_EVENT_STA_STOP:
                case WIFI_EVENT_STA_BEACON_TIMEOUT:
                case WIFI_EVENT_WIFI_READY:
                case WIFI_EVENT_SCAN_DONE:
                default:
                        break;
                }

        } else if (event_base == IP_EVENT) {
                switch (event_id) {
                case IP_EVENT_STA_GOT_IP: {
                        // ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                        // pr_info("got ip:" IPSTR "\n", IP2STR(&event->ip_info.ip));
                        wifi_ipinfo_print(esp_netif_get_default_netif());
                        wifi_sta_ip_got = 1;

                        xTaskNotifyGive(task_handle_sta_ping);

                        break;
                }

                case IP_EVENT_GOT_IP6:
                case IP_EVENT_STA_LOST_IP:
                        wifi_sta_ip_got = 0;
                        break;

                default:
                        break;
                }
        }
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
        __wifi_event_handle_internal(event_base, event_id, event_data);
        wifi_event_cb_call(event_base, event_id, event_data);
}

static int is_assoc_static_ip_valid(struct wifi_assoc_cfg *cfg)
{
        if (!is_valid_ipaddr(cfg->local, AF_INET))
                return 0;
        
        if (!is_valid_ipaddr(cfg->gw, AF_INET))
                return 0;           
        
        if (!is_valid_ipaddr(cfg->subnet, AF_INET))
                return 0;

        return 1;
}

esp_netif_t *wifi_netif_softap_init(struct wifi_ap_cfg *cfg)
{
        wifi_config_t wifi_cfg = { };
        wifi_ap_config_t *ap_cfg = &wifi_cfg.ap;
        esp_netif_t *netif;
        esp_err_t ret;

        if (cfg->ssid_with_sn) {
                uint8_t mac[6] = { };
                esp32_mac_get(WIFI_DEFAULT_MAC, mac);
                snprintf((char *)ap_cfg->ssid, sizeof(ap_cfg->ssid), "%s %02x:%02x:%02x", cfg->assoc.ssid, mac[3], mac[4], mac[5]);
        } else {
                strncpy((char *)ap_cfg->ssid, cfg->assoc.ssid, sizeof(ap_cfg->ssid));
        }

        strncpy((char *)ap_cfg->password, cfg->assoc.passwd, sizeof(ap_cfg->password));
        ap_cfg->max_connection = cfg->max_sta;
        ap_cfg->authmode = (wifi_auth_mode_t)cfg->assoc.auth; // TODO
        ap_cfg->pairwise_cipher = (wifi_cipher_type_t )cfg->assoc.cipher; // TODO
        ap_cfg->beacon_interval = cfg->beacon_intv;
        ap_cfg->csa_count = cfg->csa_count;
        ap_cfg->dtim_period = cfg->dtim_period;
        ap_cfg->pmf_cfg.required = false;

        if (cfg->assoc.channel > 0) {
                ap_cfg->channel = cfg->assoc.channel;
        }

        netif = esp_netif_create_default_wifi_ap();
        if (!netif) {
                pr_err("esp_netif_create_default_wifi_ap()\n");
                return NULL;
        }

        if (!cfg->dhcps) {
                ret = esp_netif_dhcps_stop(netif);
                if (ret != ESP_OK) {
                        pr_err("esp_netif_dhcpc_stop(): %d\n", ret);
                        goto err;
                }
        }

        if (is_assoc_static_ip_valid(&cfg->assoc)) {
                if (cfg->dhcps) {
                        ret = esp_netif_dhcps_stop(netif);
                        if (ret != ESP_OK) {
                                pr_err("esp_netif_dhcpc_stop(): %d\n", ret);
                                goto err;
                        }
                }

                esp_netif_ip_info_t ip_info = { };
                ip4addr_aton(cfg->assoc.local,  (ip4_addr_t *)&ip_info.ip);
                ip4addr_aton(cfg->assoc.gw,     (ip4_addr_t *)&ip_info.gw);
                ip4addr_aton(cfg->assoc.subnet, (ip4_addr_t *)&ip_info.netmask);

                ret = esp_netif_set_ip_info(netif, &ip_info);
                if (ret != ESP_OK) {
                        pr_err("esp_netif_set_ip_info(): %d\n", ret);
                        goto err;
                }

                if (is_valid_ipaddr(cfg->assoc.dns, AF_INET)) {
                        esp_netif_dns_info_t dns = { };
                        ip4addr_aton(cfg->assoc.dns, (ip4_addr_t *)&dns.ip.u_addr.ip4);
                        dns.ip.type = IPADDR_TYPE_V4;

                        ret = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
                        if (ret != ESP_OK) {
                                pr_err("esp_netif_set_dns_info(): %d\n", ret);
                                goto err;
                        }

                        pr_info("set dns to %s\n", cfg->assoc.dns);
                }

                if (cfg->dhcps) {
                        // esp_netif_ip_info_t ip_start, ip_end;
                        // ip_start.ip = ((uint32_t)(ip_info.ip) & 0xffffff00) | 0x02;
                        // ip_end.ip = ((uint32_t)(ip_info.ip) & 0xffffff00) | 0x10;

                        ret = esp_netif_dhcps_start(netif);
                        if (ret != ESP_OK) {
                                pr_err("esp_netif_dhcpc_stop(): %d\n", ret);
                                goto err;
                        }
                }
        }

        ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
        if (ret != ESP_OK) {
                pr_err("esp_wifi_set_config(): %d\n", ret);
                goto err;
        }

        return netif;

err:
        if (netif)
                esp_netif_destroy_default_wifi(netif);

        return NULL;
}

esp_netif_t *wifi_netif_sta_init(struct wifi_sta_cfg *cfg)
{
        wifi_config_t wifi_cfg = { };
        wifi_sta_config_t *sta_cfg = &wifi_cfg.sta;
        esp_netif_t *netif;
        esp_err_t ret;

        strncpy((char *)sta_cfg->ssid, cfg->assoc.ssid, sizeof(sta_cfg->ssid));
        strncpy((char *)sta_cfg->password, cfg->assoc.passwd, sizeof(sta_cfg->password));
        sta_cfg->failure_retry_cnt = cfg->retry_count;
        sta_cfg->scan_method = (wifi_scan_method_t)cfg->scan_mode; // TODO
        sta_cfg->threshold.authmode = (wifi_auth_mode_t)cfg->assoc.auth; // TODO
        sta_cfg->sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

        if (cfg->assoc.channel > 0) {
                sta_cfg->channel = cfg->assoc.channel;
        }

        netif = esp_netif_create_default_wifi_sta();
        if (!netif) {
                pr_err("esp_netif_create_default_wifi_sta()\n");
                return NULL;
        }

        if (!cfg->dhcpc && is_assoc_static_ip_valid(&cfg->assoc)) {
                ret = esp_netif_dhcpc_stop(netif);
                if (ret != ESP_OK) {
                        pr_err("esp_netif_dhcpc_stop(): %d\n", ret);
                        goto err;
                }

                pr_info("use static ip config\n");

                esp_netif_ip_info_t ip_info = { };
                ip4addr_aton(cfg->assoc.local,  (ip4_addr_t *)&ip_info.ip);
                ip4addr_aton(cfg->assoc.gw,     (ip4_addr_t *)&ip_info.gw);
                ip4addr_aton(cfg->assoc.subnet, (ip4_addr_t *)&ip_info.netmask);

                ret = esp_netif_set_ip_info(netif, &ip_info);
                if (ret != ESP_OK) {
                        pr_err("esp_netif_set_ip_info(): %d\n", ret);
                        goto err;
                }

                if (is_valid_ipaddr(cfg->assoc.dns, AF_INET)) {
                        esp_netif_dns_info_t dns = { };
                        ip4addr_aton(cfg->assoc.dns, (ip4_addr_t *)&dns.ip.u_addr.ip4);
                        dns.ip.type = IPADDR_TYPE_V4;

                        ret = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
                        if (ret != ESP_OK) {
                                pr_err("esp_netif_set_dns_info(): %d\n", ret);
                                goto err;
                        }

                        pr_info("set dns to %s\n", cfg->assoc.dns);
                }
        }

        ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
        if (ret != ESP_OK) {
                pr_err("esp_wifi_set_config(): %d\n", ret);
                goto err;
        }

        return netif;

err:
        if (netif)
                esp_netif_destroy_default_wifi(netif);

        return NULL;
}

// void softap_set_dns_addr(esp_netif_t *esp_netif_ap, esp_netif_t *esp_netif_sta)
// {
//         const uint8_t DHCPS_OFFER_DNS = 0x02;
//         esp_netif_dns_info_t dns;
//         esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns);
//         uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;
//         ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(esp_netif_ap));
//         ESP_ERROR_CHECK(esp_netif_dhcps_option(esp_netif_ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_option, sizeof(dhcps_offer_option)));
//         ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns));
//         ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));
// }

void wifi_early_init(void)
{
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
}

int wifi_start(struct wifi_ctx *ctx, struct wifi_cfg *cfg)
{
        esp_err_t ret;

        if (cfg->mode == ESP_WIFI_MODE_OFF) {
                pr_info("WiFi OFF\n");
                return 0;
        }

        wifi_early_init();

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                        ESP_EVENT_ANY_ID,
                        &wifi_event_handler,
                        NULL,
                        NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                        ESP_EVENT_ANY_ID,
                        &wifi_event_handler,
                        NULL,
                        NULL));

        wifi_init_config_t defcfg = WIFI_INIT_CONFIG_DEFAULT();

        if (cfg->buf.enabled) {
                defcfg.static_rx_buf_num = cfg->buf.static_rx_buf_num;
                defcfg.dynamic_rx_buf_num = cfg->buf.dynamic_rx_buf_num;
                defcfg.static_tx_buf_num = cfg->buf.static_tx_buf_num;
                defcfg.dynamic_tx_buf_num = cfg->buf.dynamic_tx_buf_num;
                defcfg.rx_mgmt_buf_num = cfg->buf.rx_mgmt_buf_num;
                defcfg.cache_tx_buf_num = cfg->buf.cache_tx_buf_num;
                defcfg.rx_ba_win = cfg->buf.rx_ba_win;
                defcfg.mgmt_sbuf_num = cfg->buf.mgmt_sbuf_num;
                defcfg.tx_hetb_queue_num = cfg->buf.tx_hetb_queue_num;
        }

        defcfg.csi_enable = cfg->adv.csi;
        defcfg.ampdu_rx_enable = cfg->adv.ampdu_rx;
        defcfg.ampdu_tx_enable = cfg->adv.ampdu_tx;
        defcfg.amsdu_tx_enable = cfg->adv.amsdu_tx;
        defcfg.sta_disconnected_pm = cfg->adv.sta_disconnected_pm;

        ret = esp_wifi_init(&defcfg);
        if (ret != ESP_OK) {
                pr_err("esp_wifi_init(): %d\n", ret);
                return -ENOMEM;
        }

        pr_info("basic config:\n");
        pr_info("\tstatic_rx_buf_num: %d\n", defcfg.static_rx_buf_num);
        pr_info("\tdynamic_rx_buf_num: %d\n", defcfg.dynamic_rx_buf_num);
        pr_info("\tstatic_tx_buf_num: %d\n", defcfg.static_tx_buf_num);
        pr_info("\tdynamic_tx_buf_num: %d\n", defcfg.dynamic_tx_buf_num);
        pr_info("\trx_mgmt_buf_num: %d\n", defcfg.rx_mgmt_buf_num);
        pr_info("\tcache_tx_buf_num: %d\n", defcfg.cache_tx_buf_num);
        pr_info("\tcsi_enable: %d\n", defcfg.csi_enable);
        pr_info("\tampdu_rx_enable: %d\n", defcfg.ampdu_rx_enable);
        pr_info("\tampdu_tx_enable: %d\n", defcfg.ampdu_tx_enable);
        pr_info("\tamsdu_tx_enable: %d\n", defcfg.amsdu_tx_enable);
        pr_info("\tnvs_enable: %d\n", defcfg.nvs_enable);
        pr_info("\trx_ba_win: %d\n", defcfg.rx_ba_win);
        pr_info("\tmgmt_sbuf_num: %d\n", defcfg.mgmt_sbuf_num);
        pr_info("\tbeacon_max_len: %d\n", defcfg.beacon_max_len);
        pr_info("\tsta_disconnected_pm: %d\n", defcfg.sta_disconnected_pm);
        pr_info("\ttx_hetb_queue_num: %d\n", defcfg.tx_hetb_queue_num);

        ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (ret != ESP_OK) {
                pr_err("esp_wifi_set_mode(): %d\n", ret);
                return -EIO;
        }

        if (cfg->mode == ESP_WIFI_MODE_STA_AP || cfg->mode == ESP_WIFI_MODE_STA_AP) {
                ctx->netif_ap = wifi_netif_softap_init(&cfg->ap);
        }

        if (cfg->mode == ESP_WIFI_MODE_STA || cfg->mode == ESP_WIFI_MODE_STA_AP) {
                ctx->netif_sta = wifi_netif_sta_init(&cfg->sta);
        }

        ret = esp_wifi_start();
        if (ret != ESP_OK) {
                pr_err("esp_wifi_start(): %d\n", ret);
                return -EIO;
        }

        ret = esp_wifi_set_ps((wifi_ps_type_t)cfg->adv.ps_mode);
        if (ESP_OK != ret) {
                pr_err("esp_wifi_set_ps(): %d\n", ret);
        }

        {
                uint8_t pwr = cfg->adv.tx_power;
                
                if (pwr > 21)
                        pwr = 21;
                if (pwr < 2)
                        pwr = 2;
                
                ret = esp_wifi_set_max_tx_power(pwr * 4);
                if (ret != ESP_OK) {
                        pr_err("esp_wifi_set_max_tx_power(): %d\n", ret);
                }
        }

        esp_wifi_set_dynamic_cs(cfg->adv.dynamic_cs);

        if (cfg->mode == ESP_WIFI_MODE_STA_AP || cfg->mode == ESP_WIFI_MODE_STA) {
                esp_netif_set_default_netif(ctx->netif_sta);

                // esp_wifi_set_inactive_time(WIFI_IF_STA, cfg->sta.inactive_sec);

                xTaskCreatePinnedToCore(task_wifi_sta_ping, "ping_daemon", 4096, NULL, 1, &task_handle_sta_ping, CPU0);

                if (esp_timer_create(&timer_args_wifi_sta_reconn, &timer_wifi_sta_reconn) != ESP_OK) {
                        pr_err("failed to create timer\n");
                        return -ENOMEM;
                }
        } else {
                esp_netif_set_default_netif(ctx->netif_ap);
        }

        if (cfg->mode == ESP_WIFI_MODE_STA_AP || cfg->mode == ESP_WIFI_MODE_AP) {
                if (esp_netif_napt_enable(ctx->netif_ap) != ESP_OK) {
                        pr_err("failed to enable NAPT on AP interface\n");
                }
        }

        ctx->cfg = cfg;

        return 0;
}

#endif // __LIBJJ_ESP32_WIFI_H__