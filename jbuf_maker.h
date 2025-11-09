#ifndef __LIBJJ_JBUF_MAKER_H__
#define __LIBJJ_JBUF_MAKER_H__

#ifdef __LIBJJ_ESP32_WIFI_H__

void jbuf_wifi_assoc_cfg_add(jbuf_t *b, const char *key, struct wifi_assoc_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_strbuf_add(b, "ssid", cfg->ssid);
        jbuf_strbuf_add(b, "passwd", cfg->passwd);
        jbuf_strval_add(b, "auth", cfg->auth, str_wifi_auth_modes);
        jbuf_strval_add(b, "cipher", cfg->cipher, str_wifi_cipher_types);
        jbuf_uint_add(b, "chnl", cfg->channel);
        jbuf_strbuf_add(b, "local", cfg->local);
        jbuf_strbuf_add(b, "gw", cfg->gw);
        jbuf_strbuf_add(b, "subnet", cfg->subnet);

        jbuf_obj_close(b, obj);
}

void jbuf_wifi_ap_cfg_add(jbuf_t *b, const char *key, struct wifi_ap_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_wifi_assoc_cfg_add(b, "assoc", &cfg->assoc);

        jbuf_bool_add(b, "dhcps", cfg->dhcps);
        jbuf_bool_add(b, "ssid_with_sn", cfg->ssid_with_sn);
        jbuf_uint_add(b, "max_sta", cfg->max_sta);
        jbuf_uint_add(b, "csa_cnt", cfg->csa_count);
        jbuf_uint_add(b, "dtim_period", cfg->dtim_period);
        jbuf_uint_add(b, "beacon_intv", cfg->beacon_intv);

        jbuf_obj_close(b, obj);
}

void jbuf_wifi_sta_cfg_add(jbuf_t *b, const char *key, struct wifi_sta_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_wifi_assoc_cfg_add(b, "assoc", &cfg->assoc);

        jbuf_bool_add(b, "dhcpc", cfg->dhcpc);
        jbuf_bool_add(b, "scan_mode", str_wifi_sta_scan_modes);
        jbuf_bool_add(b, "rm", cfg->rm_enabled);
        jbuf_bool_add(b, "btm", cfg->btm_enabled);
        jbuf_bool_add(b, "mbo", cfg->mbo_enabled);
        jbuf_bool_add(b, "retry_cnt", cfg->retry_count);
        jbuf_bool_add(b, "inactive_sec", cfg->inactive_sec);

        jbuf_obj_close(b, obj);
}

void jbuf_wifi_cfg_add(jbuf_t *b, const char *key, struct wifi_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_strval_add(b, "mode", cfg->mode, str_wifi_modes);

        {
                void *adv = jbuf_obj_open(b, "advanced");

                jbuf_uint_add(b, "tx_power_dBm", cfg->adv.tx_power);
                jbuf_strval_add(b, "ps", cfg->adv.ps_mode, str_wifi_ps_modes);
                jbuf_bool_add(b, "dynamic_cs", cfg->adv.dynamic_cs);
                jbuf_bool_add(b, "csi", cfg->adv.csi);
                jbuf_bool_add(b, "ampdu_rx", cfg->adv.ampdu_rx);
                jbuf_bool_add(b, "ampdu_tx", cfg->adv.ampdu_tx);
                jbuf_bool_add(b, "amsdu_tx", cfg->adv.amsdu_tx);
                jbuf_bool_add(b, "sta_idle_pm", cfg->adv.sta_disconnected_pm);
                jbuf_bool_add(b, "no_11b_rate", cfg->adv.no_11b_rate);
                jbuf_strval_add(b, "phy_rate", cfg->adv.phy_rate, str_wifi_tx_rates);
                jbuf_strval_add(b, "bw_2g", cfg->adv.bw_2g, str_wifi_bw);
                jbuf_strval_add(b, "bw_5g", cfg->adv.bw_5g, str_wifi_bw);

                jbuf_obj_close(b, adv);
        }

        jbuf_obj_close(b, obj);
}

#endif

#ifdef __LIBJJ_EVENT_UDP_MC_H__
void __unused jbuf_udpmc_cfg_add(jbuf_t *b, const char *key, struct udp_mc_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_bool_add(b, "enabled", cfg->enabled);
        jbuf_strbuf_add(b, "mcaddr", cfg->mcaddr);
        jbuf_uint_add(b, "port", cfg->port);

        jbuf_obj_close(b, obj);
}
#endif

#ifdef CONFIG_HAVE_CAN_MCP2515
void __unused jbuf_mcp2515_cfg_add(jbuf_t *b, const char *key, struct mcp2515_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_strval_add(b, "baudrate", cfg->baudrate, str_can_baudrates);
        jbuf_strval_add(b, "quartz", cfg->quartz, str_mcp_quartz);
        jbuf_strval_add(b, "mode", cfg->mode, str_mcp_mode);

        jbuf_obj_close(b, obj);
}
#endif

#ifdef CONFIG_HAVE_CAN_TWAI
void __unused jbuf_twai_cfg_add(jbuf_t *b, const char *key, struct twai_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_uint_add(b, "pin_tx", cfg->pin_tx);
        jbuf_uint_add(b, "pin_rx", cfg->pin_rx);
        jbuf_strval_add(b, "mode", cfg->mode, str_twai_mode);
        jbuf_strval_add(b, "baudrate", cfg->baudrate, str_can_baudrates);
        jbuf_uint_add(b, "rx_qlen", cfg->rx_qlen);
        jbuf_uint_add(b, "tx_timedout_ms", cfg->tx_timedout_ms);

        jbuf_obj_close(b, obj);
}
#endif

#ifdef __LIBJJ_RACECHRONO_BLE_H__
void __unused jbuf_ble_cfg_add(jbuf_t *b, const char *key, struct ble_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_bool_add(b, "enabled", cfg->enabled);
        jbuf_strbuf_add(b, "devname", cfg->devname);
        jbuf_uint_add(b, "update_hz", cfg->update_hz);
        jbuf_strval_add(b, "tx_power", cfg->tx_power, str_ble_txpwr);

        jbuf_obj_close(b, obj);
}
#endif // __LIBJJ_RACECHRONO_BLE_H__

#ifdef __LIBJJ_CAN_TCP_H__
void __unused jbuf_can_rlimit_add(jbuf_t *b, const char *key, struct can_rlimit_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_bool_add(b, "enabled", cfg->enabled);

        {
                void *hz = jbuf_obj_open(b, "default_hz");

                for (unsigned i = 0; i < ARRAY_SIZE(cfg->default_hz); i++) {
                        jbuf_sint_add(b, str_rlimit_types[i], cfg->default_hz[i]);
                }

                jbuf_obj_close(b, hz);
        }

        jbuf_obj_close(b, obj);
}
#endif // __LIBJJ_CAN_TCP_H__

#ifdef __LIBJJ_I2C_H__
static __unused void jbuf_i2c_cfg_add(jbuf_t *b, const char *key, struct i2c_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_sint_add(b, "scl", cfg->pin_scl);
        jbuf_sint_add(b, "sda", cfg->pin_sda);
        jbuf_uint_add(b, "freq", cfg->freq);

        jbuf_obj_close(b, obj);
}
#endif // __LIBJJ_I2C_H__

#ifdef __LIBJJ_CAN_TCP_H__
static __unused void jbuf_cantcp_add(jbuf_t *b, const char *key, struct cantcp_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_bool_add(b, "enabled", cfg->enabled);
        jbuf_bool_add(b, "nodelay", cfg->nodelay);

        jbuf_obj_close(b, obj);
}
#endif // __LIBJJ_CAN_TCP_H__

#ifdef __LIBJJ_CAN_UDP_H__
static __unused void jbuf_canudp_add(jbuf_t *b, const char *key, struct canudp_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_udpmc_cfg_add(b, "mc", &cfg->mc);

        jbuf_bool_add(b, "can_rx_forward", cfg->can_rx_fwd);
        jbuf_bool_add(b, "udp_rx_accept", cfg->udp_rx_accept);

        jbuf_obj_close(b, obj);
}
#endif // __LIBJJ_CAN_UDP_H__

#endif // __LIBJJ_JBUF_MAKER_H__
