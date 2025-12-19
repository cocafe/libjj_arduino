#ifndef __LIBJJ_RPC_PM_H__
#define __LIBJJ_RPC_PM_H__

#include <esp_pm.h>

void rpc_pm_add(void)
{
#ifdef CONFIG_PM_ENABLE
        http_rpc.on("/esp32_pm", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        int max_freq = -1, min_freq = -1, light_slp = -1, err = 0;

                        if (http_rpc.hasArg("max_freq")) {
                                String _max_freq = http_rpc.arg("max_freq");

                                if (is_str_equal((char *)_max_freq.c_str(), "max", CASELESS)) {
                                        max_freq = ESP_PM_CPU_FREQ_MAX;
                                } else if (is_str_equal((char *)_max_freq.c_str(), "min", CASELESS)) {
                                        max_freq = 80;
                                } else {
                                        int i = strtoll_wrap(_max_freq.c_str(), 10, &err);

                                        if (err || i < 0) {
                                                http_rpc.send(500, "text/plain", "invalid value\n");
                                                return;
                                        }

                                        max_freq = i;
                                }
                        }

                        if (http_rpc.hasArg("min_freq")) {
                                String _min_freq = http_rpc.arg("min_freq");

                                if (is_str_equal((char *)_min_freq.c_str(), "max", CASELESS)) {
                                        min_freq = ESP_PM_CPU_FREQ_MAX;
                                } else if (is_str_equal((char *)_min_freq.c_str(), "min", CASELESS)) {
                                        min_freq = 80;
                                } else {
                                        int i = strtoll_wrap(_min_freq.c_str(), 10, &err);

                                        if (err || i < 0) {
                                                http_rpc.send(500, "text/plain", "invalid value\n");
                                                return;
                                        }

                                        min_freq = i;
                                }
                        }

                        if (http_rpc.hasArg("light_sleep")) {
                                String _light_slp = http_rpc.arg("light_sleep");
                                int i = strtoll_wrap(_light_slp.c_str(), 10, &err);

                                if (err || i < 0) {
                                        http_rpc.send(500, "text/plain", "invalid value\n");
                                        return;
                                }

                                light_slp = !!i;
                        }

                        if (max_freq < 0 || min_freq < 0 || light_slp < 0) {
                                http_rpc.send(500, "text/plain", "<max_freq:max/min/num> <min_freq:max/min:num> <light_sleep:0/1>\n");
                                return;
                        }

                        pr_info("freq max/min: %d/%d light_sleep: %d\n", max_freq, min_freq, light_slp);

                        esp_pm_config_t cfg = {
                                .max_freq_mhz = max_freq,
                                .min_freq_mhz = min_freq,
                                .light_sleep_enable = (bool)light_slp,
                        };

                        err = esp_pm_configure(&cfg);
                        if (err != ESP_OK) {
                                if (err == ESP_ERR_INVALID_ARG)
                                        http_rpc.send(500, "text/plain", "api: invalid value\n");
                                else if (err == ESP_ERR_NOT_SUPPORTED)
                                        http_rpc.send(500, "text/plain", "api: not supported\n");
                                else
                                        http_rpc.send(500, "text/plain", "api failure\n");

                                return;
                        }

                        http_rpc.send(200, "text/plain", "OK\n");
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
#endif

#ifdef Arduino_h
        //function takes the following frequencies as valid values:
        //  240, 160, 80    <<< For all XTAL types
        //  40, 20, 10      <<< For 40MHz XTAL
        //  26, 13          <<< For 26MHz XTAL
        //  24, 12          <<< For 24MHz XTAL
        http_rpc.on("/esp32_cpufreq", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        static const unsigned valid_freq[] = { 240, 160, 80, 40, 20, 10, 26, 13, 24, 12 };
                        String _freq = http_rpc.arg("set");
                        unsigned long freq;
                        int err, valid = 0;

                        freq = strtoull_wrap(_freq.c_str(), 10, &err);
                        if (err) {
                                http_rpc.send(500, "text/plain", "invalid number\n");
                                return;
                        }

                        for (unsigned i = 0; i < ARRAY_SIZE(valid_freq); i++) {
                                if (freq == valid_freq[i]) {
                                        valid = 1;
                                        break;
                                }
                        }

                        if (!valid) {
                                char buf[128];
                                int c = 0;
                                c += snprintf(&buf[c], sizeof(buf) - c, "valid freq MHz: ");
                                for (unsigned i = 0; i < ARRAY_SIZE(valid_freq); i++) {
                                        c += snprintf(&buf[c], sizeof(buf) - c, "%u ", valid_freq[i]);
                                }
                                c += snprintf(&buf[c], sizeof(buf) - c, "\n");
                                http_rpc.send(500, "text/plain", buf);
                                return;
                        }

                        setCpuFrequencyMhz(freq);
                        http_rpc.send(500, "text/plain", "OK\n");
                } else {
                        char buf[32] = { };

                        unsigned freq = getCpuFrequencyMhz();

                        snprintf(buf, sizeof(buf), "%u MHz\n", freq);

                        http_rpc.send(200, "text/plain", buf);
                }
        });

        http_rpc.on("/esp32_xtalfreq", HTTP_GET, [](){
                char buf[32] = { };

                unsigned freq = getXtalFrequencyMhz();

                snprintf(buf, sizeof(buf), "%u MHz\n", freq);

                http_rpc.send(200, "text/plain", buf);
        });

        http_rpc.on("/esp32_abpfreq", HTTP_GET, [](){
                char buf[32] = { };

                unsigned long freq = getApbFrequency();

                snprintf(buf, sizeof(buf), "%lu Hz\n", freq);

                http_rpc.send(200, "text/plain", buf);
        });
#endif
}

#endif // __LIBJJ_RPC_PM_H__