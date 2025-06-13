#ifndef __LIBJJ_RPC_SAVE_H__
#define __LIBJJ_RPC_SAVE_H__

#include <WebServer.h>

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

                jbuf_config_make(&jkey_cfg, &g_cfg);

                char *buf = jbuf_json_text_save(&jkey_cfg, NULL);

                if (!buf)
                        http_rpc.send(500, "text/plain", "ERROR\n");
                else
                        http_rpc.send(200, "text/plain", buf);

                if (buf)
                        free(buf);

                jbuf_deinit(&jkey_cfg);
        });

        http_rpc.on("/cfg_json_file", HTTP_GET, [](){
                int err;
                size_t len;

                char *buf = (char *)spiffs_file_read(CONFIG_SAVE_JSON_PATH, &err, &len);
                if (err) {
                        http_rpc.send(500, "text/plain", "read error\n");
                        return;
                }

                http_rpc.send_P(200, "text/plain", buf, len);

                free(buf);
        });

        http_rpc.on("/cfg_json_clear", HTTP_GET, [](){
                uint8_t buf[8] = { };

                if (spiffs_file_write(CONFIG_SAVE_JSON_PATH, buf, sizeof(buf)))
                        http_rpc.send(200, "text/plain", "write error\n");
                else
                        http_rpc.send(200, "text/plain", "OK\n");
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