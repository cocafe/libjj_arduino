#ifndef __LIBJJ_JBUF_MAKER_H__
#define __LIBJJ_JBUF_MAKER_H__

void jbuf_wifi_nw_cfg_add(jbuf_t *b, const char *key, struct wifi_nw_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_strbuf_add(b, "ssid", cfg->ssid);
        jbuf_strbuf_add(b, "passwd", cfg->passwd);
        jbuf_uint_add(b, "timeout_sec", cfg->timeout_sec);
        jbuf_bool_add(b, "use_dhcp", cfg->use_dhcp);
        jbuf_strbuf_add(b, "local", cfg->local);
        jbuf_strbuf_add(b, "gw", cfg->gw);
        jbuf_strbuf_add(b, "subnet", cfg->subnet);

        jbuf_obj_close(b, obj);
}

void jbuf_udpmc_cfg_add(jbuf_t *b, const char *key, struct udp_mc_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_bool_add(b, "enabled", cfg->enabled);
        jbuf_strbuf_add(b, "mcaddr", cfg->mcaddr);
        jbuf_uint_add(b, "port", cfg->port);

        jbuf_obj_close(b, obj);
}

#ifdef CONFIG_HAVE_CAN_MCP2515
void jbuf_mcp2515_cfg_add(jbuf_t *b, const char *key, struct mcp2515_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_strval_add(b, "baudrate", cfg->baudrate, str_can_baudrates);
        jbuf_strval_add(b, "quartz", cfg->quartz, str_mcp_quartz);
        jbuf_strval_add(b, "mode", cfg->mode, str_mcp_mode);

        jbuf_obj_close(b, obj);
}
#endif

#ifdef CONFIG_HAVE_CAN_TWAI
void jbuf_twai_cfg_add(jbuf_t *b, const char *key, struct twai_cfg *cfg)
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

#endif // __LIBJJ_JBUF_MAKER_H__
