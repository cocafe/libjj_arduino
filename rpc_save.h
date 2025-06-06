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