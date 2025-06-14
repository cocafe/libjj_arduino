#ifndef __LIBJJ_RPC_PM_H__
#define __LIBJJ_RPC_PM_H__

#include <esp_pm.h>

void rpc_pm_add(void)
{
        http_rpc.on("/pm", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        
                } else {
                        int bufsz = 512;
                        char *buf = NULL;
                        size_t c = 0;
                        esp_pm_config_t pm_cfg = { };

                        if (ESP_OK != esp_pm_get_configuration(&pm_cfg)) {
                                http_rpc.send(500, "text/plain", "api failed\n");
                                return;
                        }

                        if (http_rpc.hasArg("bufsz")) {
                                String _val = http_rpc.arg("bufsz");
                                int err;

                                bufsz = strtoull_wrap(_val.c_str(), 10, &err);

                                if (err) {
                                        http_rpc.send(500, "text/plain", "invalid value\n");
                                        return;
                                }
                        }

                        buf = (char *)malloc(bufsz);
                        if (!buf) {
                                http_rpc.send(200, "text/plain", "No memory for output buffer\n");
                                return;
                        }

                        c += snprintf(&buf[c], bufsz - c, "max_freq: %dMHz\n", pm_cfg.max_freq_mhz);
                        c += snprintf(&buf[c], bufsz - c, "min_freq: %dMHz\n", pm_cfg.min_freq_mhz);
                        c += snprintf(&buf[c], bufsz - c, "light_sleep_enabled: %d\n", pm_cfg.light_sleep_enable);
                        c += snprintf(&buf[c], bufsz - c, "pm lock list:\n");

                        FILE *f = fmemopen(&buf[c], bufsz - c, "w");
                        if (f == NULL) {
                                http_rpc.send(200, "failed to open memory stream\n");
                                free(buf);
                                return;
                        }

                        esp_pm_dump_locks(f);

                        fflush(f);
                        fclose(f);

                        http_rpc.send(200, "text/plain", buf);

                        free(buf);
                }
        });
}

#endif // __LIBJJ_RPC_PM_H__