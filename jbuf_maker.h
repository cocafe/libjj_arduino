#ifndef __LIBJJ_JBUF_MAKER_H__
#define __LIBJJ_JBUF_MAKER_H__

void jbuf_wifi_nw_cfg_add(jbuf_t *b, const char *key, struct wifi_nw_cfg *cfg)
{
        void *obj = jbuf_obj_open(b, key);

        jbuf_strbuf_add(b, (char *)"ssid", cfg->ssid);
        jbuf_strbuf_add(b, (char *)"passwd", cfg->passwd);
        jbuf_uint_add(b, (char *)"timeout_sec", cfg->timeout_sec);
        jbuf_bool_add(b, (char *)"use_dhcp", cfg->use_dhcp);
        jbuf_strbuf_add(b, (char *)"local", cfg->local);
        jbuf_strbuf_add(b, (char *)"gw", cfg->gw);
        jbuf_strbuf_add(b, (char *)"subnet", cfg->subnet);
        jbuf_strval_add(b, (char *)"tx_pwr", cfg->tx_pwr, str_wifi_txpwr);

        jbuf_obj_close(b, obj);
}

void jbuf_udpmc_cfg_add(jbuf_t *b, const char *key, struct udp_mc_cfg *cfg)
{
        void *udpmc_obj = jbuf_obj_open(b, key);

        jbuf_bool_add(b, (char *)"enabled", cfg->enabled);
        jbuf_strbuf_add(b, (char *)"mcaddr", cfg->mcaddr);
        jbuf_uint_add(b, (char *)"port", cfg->port);

        jbuf_obj_close(b, udpmc_obj);
}

#ifdef CONFIG_HAVE_CAN_MCP2515
void jbuf_mcp2515_cfg_add(jbuf_t *b, const char *key, struct mcp2515_cfg *cfg)
{
        void *mcp2515_obj = jbuf_obj_open(b, key);

        jbuf_strval_add(b, (char *)"baudrate", cfg->baudrate, str_can_baudrates);
        jbuf_strval_add(b, (char *)"quartz", cfg->quartz, str_mcp_quartz);
        jbuf_strval_add(b, (char *)"mode", cfg->mode, str_mcp_mode);

        jbuf_obj_close(b, mcp2515_obj);
}
#endif

#ifdef CONFIG_HAVE_CAN_TWAI
void jbuf_twai_cfg_add(jbuf_t *b, const char *key, struct twai_cfg *cfg)
{
        jbuf_uint_add(b, "pin_tx", &cfg->pin_tx);
}
#endif

#endif // __LIBJJ_JBUF_MAKER_H__
