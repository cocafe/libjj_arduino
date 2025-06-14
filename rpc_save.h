#ifndef __LIBJJ_RPC_SAVE_H__
#define __LIBJJ_RPC_SAVE_H__

void rpc_save_add(void)
{
        http_rpc.on("/cfg_reset", HTTP_GET, [](){
                save_update(&g_save, &g_cfg_default);
                save_write(&g_save);

                http_rpc.send(200, "text/plain", "OK\n");
        });

        http_rpc.on("/cfg_save", HTTP_GET, [](){
                save_update(&g_save, &g_cfg);
                save_write(&g_save);

                http_rpc.send(200, "text/plain", "OK\n");
        });

        http_rpc.on("/cfg_json", HTTP_GET, [](){
                jbuf_t jkey_cfg = { };

                if (!jbuf_cfg_make) {
                        http_rpc.send(500, "text/plain", "unsupported\n");
                        return;
                }

                jbuf_cfg_make(&jkey_cfg, &g_cfg);

                char *buf = jbuf_json_text_save(&jkey_cfg, NULL);

                if (!buf)
                        http_rpc.send(500, "text/plain", "ERROR\n");
                else
                        http_rpc.send(200, "text/plain", buf);

                if (buf)
                        free(buf);

                jbuf_deinit(&jkey_cfg);
        });

        http_rpc.on("/factory_json", HTTP_GET, [](){
                if (http_rpc.hasArg("del")) {
                        if (config_json_delete())
                                http_rpc.send(200, "text/plain", "delete error\n");
                        else
                                http_rpc.send(200, "text/plain", "OK\n");
                } else if (http_rpc.hasArg("save")) {
                        if (config_json_save(&g_cfg))
                                http_rpc.send(200, "text/plain", "save error\n");
                        else
                                http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        int err;
                        size_t len;

                        char *buf = (char *)spiffs_file_read(CONFIG_SAVE_JSON_PATH, &err, &len);
                        if (err) {
                                http_rpc.send(500, "text/plain", "read error\n");
                                return;
                        }

                        http_rpc.send_P(200, "application/json", buf, len);

                        free(buf);
                }
        });

        http_rpc.on("/factory_json", HTTP_POST, [](){
                if (!jbuf_cfg_make) {
                        http_rpc.send(500, "text/plain", "unsupported\n");
                        return;
                }

                if (!http_rpc.hasArg("plain")) {
                        http_rpc.send(500, "text/plain", "no json received\n");
                        return;
                }

                String _json = http_rpc.arg("plain");
                jbuf_t jkey_cfg = { };

                printf("%s\n", _json.c_str());

                jbuf_cfg_make(&jkey_cfg, &g_cfg);

                if (jbuf_json_text_load(&jkey_cfg, _json.c_str(), _json.length())) {
                        http_rpc.send(500, "text/plain", "invalid json\n");
                        jbuf_deinit(&jkey_cfg);
                        return;
                }

                if (config_json_save(&g_cfg)) {
                        http_rpc.send(500, "text/plain", "failed to write json\n");
                } else {
                        http_rpc.send(200, "text/plain", "OK\n");
                }

                jbuf_deinit(&jkey_cfg);
        });

        http_rpc.on("/verify_fwhash", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        String data = http_rpc.arg("set");
                        int i = atoi(data.c_str());

                        if (i == 1) {
                                g_save.ignore_fwhash = 0;
                        } else {
                                g_save.ignore_fwhash = 1;
                        }

                        save_update(&g_save, &g_cfg);
                        save_write(&g_save);

                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[8] = { };

                        snprintf(buf, sizeof(buf), "%hhu\n", !g_save.ignore_fwhash);

                        http_rpc.send(200, "text/plain", buf);
                }
        });
}

#endif // __LIBJJ_RPC_SAVE_H__