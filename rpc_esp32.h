#ifndef __LIBJJ_RPC_ESP_H__
#define __LIBJJ_RPC_ESP_H__

#include <WebServer.h>

void rpc_esp32_add(void)
{
        http_rpc.on("/esp32_reset", HTTP_GET, [](){
                http_rpc.send(200, "text/plain", "OK\n");
                delay(500);
                ESP.restart();
        });

        http_rpc.on("/esp32_uptime", HTTP_GET, [](){
                char buf[20] = { };

                snprintf(buf, sizeof(buf), "%lu\n", millis());

                http_rpc.send(200, "text/plain", buf);
        });

        http_rpc.on("/esp32_top", HTTP_GET, [](){
                char *buf;
                unsigned sampling_ms = 500;
                unsigned bufsz = 1200;

                if (http_rpc.hasArg("sampling_ms")) {
                        String _val = http_rpc.arg("sampling_ms");
                        int err;

                        sampling_ms = strtoull_wrap(_val.c_str(), 10, &err);

                        if (err) {
                                http_rpc.send(200, "text/plain", "Invalid value\n");
                                return;
                        }
                }

                if (http_rpc.hasArg("bufsz")) {
                        String _val = http_rpc.arg("bufsz");
                        int err;

                        bufsz = strtoull_wrap(_val.c_str(), 10, &err);

                        if (err) {
                                http_rpc.send(200, "text/plain", "Invalid value\n");
                                return;
                        }
                }

                buf = (char *)malloc(bufsz);
                if (!buf) {
                        http_rpc.send(200, "text/plain", "No memory for output buffer\n");
                        return;
                }

                esp32_top_stats_print(sampling_ms, snprintf, buf, bufsz);
                http_rpc.send(200, "text/plain", buf);

                free(buf);
        });

        http_rpc.on("/esp32_tsens", HTTP_GET, [](){
                char buf[8] = { };
                float tempC = 0.0;

                if (esp32_tsens_get(&tempC)) {
                        http_rpc.send(200, "text/plain", "ERROR\n");
                        return;
                }

                snprintf(buf, sizeof(buf), "%.2fC\n", tempC);
                http_rpc.send(200, "text/plain", buf);
        });

        http_rpc.on("/esp32_stats", HTTP_GET, [](){
                char buf[512] = { };
                float tempC = 0.0;
                int cpu = 0;
                size_t c = 0;

                if (esp32_tsens_get(&tempC)) {
                        tempC = NAN;
                }

                if ((cpu = esp32_cpu_usage_get(100)) < 0) {
                        cpu = -1;
                }

                c += snprintf(&buf[c], sizeof(buf) - c, "# HELP esp_stats\n");
                c += snprintf(&buf[c], sizeof(buf) - c, "# TYPE esp_stats gauge\n");
                c += snprintf(&buf[c], sizeof(buf) - c, "esp_stats{t=\"uptime\"} %lu\n", millis() / 1000);
                c += snprintf(&buf[c], sizeof(buf) - c, "esp_stats{t=\"tempC\"} %.2f\n", tempC);
                c += snprintf(&buf[c], sizeof(buf) - c, "esp_stats{t=\"cpu_usage\"} %d\n", cpu);

                http_rpc.send(200, "text/plain", buf);
        });
}

#endif // __LIBJJ_RPC_ESP_H__