#ifndef __LIBJJ_ESP32_WIFI_CFG_H__
#define __LIBJJ_ESP32_WIFI_CFG_H__

#define CONFIG_WIFI_BAND_MODE           ESP_WIFI_BAND_MODE_AUTO
#define CONFIG_WIFI_USE_DYNAMIC_CS      1
#define CONFIG_WIFI_USE_CSI             1
#define CONFIG_WIFI_USE_RX_AMPDU        1
#define CONFIG_WIFI_USE_TX_AMPDU        1
#define CONFIG_WIFI_USE_TX_AMSDU        1
#define CONFIG_WIFI_STA_IDLE_PM         0
#define CONFIG_WIFI_USE_NVS             0
#define CONFIG_WIFI_NO_11B_RATE         1
#define CONFIG_WIFI_USE_CUSTOM_BUF_NUM  0
#define CONFIG_WIFI_STATIC_RX_BUF_NUM   32
#define CONFIG_WIFI_DYNAMIC_RX_BUF_NUM  0
#define CONFIG_WIFI_STATIC_TX_BUF_NUM   32
#define CONFIG_WIFI_DYNAMIC_TX_BUF_NUM  0
#define CONFIG_WIFI_RX_MGMT_BUF_NUM     5
#define CONFIG_WIFI_CACHE_TX_BUF_NUM    0
#define CONFIG_WIFI_MGMT_SBUF_NUM       32
#define CONFIG_WIFI_TX_HETB_QUEUE_NUM   1
#define CONFIG_WIFI_RX_BA_WIN           752

#define CONFIG_WIFI_AP_USE_DHCPS        1
#define CONFIG_WIFI_AP_MAX_STAS         4
#define CONFIG_WIFI_AP_CSA_CNT          3
#define CONFIG_WIFI_AP_DTIM_PERIOD      1
#define CONFIG_WIFI_AP_SSID_WITH_SN     1
#define CONFIG_WIFI_AP_BEACON_INTV      100

#define CONFIG_WIFI_STA_USE_DHCPC       1
#define CONFIG_WIFI_STA_RM_ENABLED      1
#define CONFIG_WIFI_STA_BTM_ENABLED     0
#define CONFIG_WIFI_STA_MBO_ENABLED     0
#define CONFIG_WIFI_STA_CONN_RETRY_CNT  3
#define CONFIG_WIFI_STA_INACTIVE_SEC    60

#define CONFIG_WIFI_MY_AP_CHANNEL       WIFI_CHANNEL_AUTO
#define CONFIG_WIFI_MY_STA_CHANNEL      WIFI_CHANNEL_AUTO

#define CONFIG_WIFI_INACTIVE_TIME       60
#define CONFIG_WIFI_TX_POWER            20

#define WIFI_DEF_CONFIG(_mode)                                  \
        _mode,                                                  \
        {                                                       \
                CONFIG_WIFI_TX_POWER,                           \
                WIFI_PS_NONE,                                   \
                CONFIG_WIFI_USE_DYNAMIC_CS,                     \
                CONFIG_WIFI_USE_CSI,                            \
                CONFIG_WIFI_USE_RX_AMPDU,                       \
                CONFIG_WIFI_USE_TX_AMPDU,                       \
                CONFIG_WIFI_USE_TX_AMSDU,                       \
                CONFIG_WIFI_STA_IDLE_PM,                        \
                CONFIG_WIFI_USE_NVS,                            \
                CONFIG_WIFI_BAND_MODE,                          \
                CONFIG_WIFI_NO_11B_RATE,                        \
                ESP_WIFI_PHY_RATE_NOT_USE,                      \
                WIFI_BW40,                                      \
                WIFI_BW160,                                     \
        },                                                      \
        {                                                       \
                CONFIG_WIFI_USE_CUSTOM_BUF_NUM,                 \
                CONFIG_WIFI_STATIC_RX_BUF_NUM,                  \
                CONFIG_WIFI_DYNAMIC_RX_BUF_NUM,                 \
                CONFIG_WIFI_STATIC_TX_BUF_NUM,                  \
                CONFIG_WIFI_DYNAMIC_TX_BUF_NUM,                 \
                CONFIG_WIFI_RX_MGMT_BUF_NUM,                    \
                CONFIG_WIFI_CACHE_TX_BUF_NUM,                   \
                CONFIG_WIFI_MGMT_SBUF_NUM,                      \
                CONFIG_WIFI_TX_HETB_QUEUE_NUM,                  \
                CONFIG_WIFI_RX_BA_WIN,                          \
        }

#define WIFI_AP_CONFIG(_ssid, _auth, _passwd, _ip, _subnet)     \
        {                                                       \
                {                                               \
                        _ssid,                                  \
                        _passwd,                                \
                        _auth,                                  \
                        WIFI_CIPHER_TYPE_TKIP_CCMP,             \
                        CONFIG_WIFI_MY_AP_CHANNEL,              \
                        _ip,                                    \
                        _ip,                                    \
                        _subnet,                                \
                        _ip,                                    \
                },                                              \
                CONFIG_WIFI_AP_USE_DHCPS,                       \
                CONFIG_WIFI_AP_SSID_WITH_SN,                    \
                CONFIG_WIFI_AP_MAX_STAS,                        \
                CONFIG_WIFI_AP_CSA_CNT,                         \
                CONFIG_WIFI_AP_DTIM_PERIOD,                     \
                CONFIG_WIFI_AP_BEACON_INTV,                     \
        }

#define WIFI_STA_CONFIG(_ssid, _auth, _passwd)                  \
        {                                                       \
                {                                               \
                        _ssid,                                  \
                        _passwd,                                \
                        _auth,                                  \
                        WIFI_CIPHER_TYPE_TKIP_CCMP,             \
                        CONFIG_WIFI_MY_STA_CHANNEL,             \
                        "",                                     \
                        "",                                     \
                        "",                                     \
                        "",                                     \
                },                                              \
                1,                                              \
                WIFI_FAST_SCAN,                                 \
                CONFIG_WIFI_STA_RM_ENABLED,                     \
                CONFIG_WIFI_STA_BTM_ENABLED,                    \
                CONFIG_WIFI_STA_MBO_ENABLED,                    \
                CONFIG_WIFI_STA_CONN_RETRY_CNT,                 \
                CONFIG_WIFI_STA_INACTIVE_SEC,                   \
        }

#define WIFI_STA_CONFIG_NO_DHCPC(_ssid, _auth, _passwd, _ip, _gw, _subnet)         \
        {                                                       \
                {                                               \
                        _ssid,                                  \
                        _passwd,                                \
                        _auth,                                  \
                        WIFI_CIPHER_TYPE_TKIP_CCMP,             \
                        CONFIG_WIFI_MY_STA_CHANNEL,             \
                        _ip,                                    \
                        _gw,                                    \
                        _subnet,                                \
                        "",                                     \
                },                                              \
                0,                                              \
                WIFI_FAST_SCAN,                                 \
                CONFIG_WIFI_STA_RM_ENABLED,                     \
                CONFIG_WIFI_STA_BTM_ENABLED,                    \
                CONFIG_WIFI_STA_MBO_ENABLED,                    \
                CONFIG_WIFI_STA_CONN_RETRY_CNT,                 \
                CONFIG_WIFI_STA_INACTIVE_SEC,                   \
        }

#endif // __LIBJJ_ESP32_WIFI_CFG_H__