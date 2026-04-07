#ifndef __LIBJJ_CONSOLE_H__
#define __LIBJJ_CONSOLE_H__

#include <SimpleCLI.h>
#include <esp_pm.h>

SimpleCLI console_cli;
static HardwareSerial *console_serial;

#define console_printf console_serial->printf

static void console_error_cb(cmd_error *e)
{
        CommandError err(e);

        console_serial->print("ERROR: ");
        console_serial->println(err.toString());

        if (err.hasCommand())
        {
                console_serial->print("Did you mean \"");
                console_serial->print(err.getCommand().toString());
                console_serial->println("\"?");
        }
}

static void console_cmd_reboot_cb(cmd *c)
{
        delay(500);
        ESP.restart();
}

#ifdef __LIBJJ_ESP32_WIFI_H__
static void console_cmd_wifi_cb(cmd *c)
{
        Command cmd(c);

        Argument arg_up = cmd.getArgument("up");
        Argument arg_down = cmd.getArgument("down");
        Argument arg_nvsrst = cmd.getArgument("nvs_reset");

        if (arg_up.isSet()) {
                if (!is_wifi_up()) {
                        wifi_start(&g_wifi_ctx, g_wifi_ctx.cfg);
                }
        } else if (arg_down.isSet()) {
                if (is_wifi_up()) {
                        wifi_stop();
                }
        } else if (arg_nvsrst.isSet()) {
                nvs_erase();
        } else {
                Argument arg_txpwr = cmd.getArgument("txpower");
                int err;
                int val = arg_txpwr.getValue().toInt();
                if (val < 0)
                        return;

                if ((err = wifi_tx_power_set(val))) {
                        console_printf("wifi_tx_power_set() failed: %d\n", err);
                }
        }
}
#endif

static void console_cmd_top_cb(cmd *c)
{
        char *buf;
        unsigned sampling_ms = 500;
        unsigned bufsz = 1500;

        Command cmd(c);
        Argument arg_bufsz = cmd.getArgument("bufsz");
        Argument arg_snap = cmd.getArgument("snapshot");

        int sz = arg_bufsz.getValue().toInt();
        if (sz > 0) {
                bufsz = sz;
        }

        buf = (char *)calloc(1, bufsz);
        if (!buf) {
                console_printf("failed to allocate memory\n");
                return;
        }

        if (arg_snap.isSet()) {
                esp32_top_snapshot_print(snprintf, buf, bufsz);
        } else {
                esp32_top_stats_print(sampling_ms, snprintf, buf, bufsz);
        }

        console_printf("%s\n", buf);

        free(buf);
}

static void console_cmd_save_cb(cmd *c)
{
        Command cmd(c);

        Argument arg_rst = cmd.getArgument("reset");

        if (arg_rst.isSet()) {
                save_cfg_reset();
        }
}

static void console_cmd_mem_cb(cmd *c)
{
        console_printf("free_heap: %u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        console_printf("min_free_heap: %u\n", heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
        if (psramFound()) {
                console_printf("free_psram: %u\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
                console_printf("min_free_psram: %u\n", heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
        }
}

#ifdef __LIBJJ_RACECHRONO_BLE_H__
static void console_cmd_ble_cb(cmd *c)
{
        Command cmd(c);

        Argument arg_txpwr = cmd.getArgument("txpower");

        if (arg_txpwr.isSet()) {
                int val = arg_txpwr.getValue().toInt();
                if (val < 0)
                        return;

                if (false == NimBLEDevice::setPower(val)) {
                        console_printf("NimBLEDevice::setPower() failed\n");
                } else {
                        console_printf("BLE tx power set to %d\n", val);
                }
        }
}
#endif

#ifdef CONFIG_PM_ENABLE
static void console_cmd_esp_pm_cb(cmd *cmd)
{
        int bufsz = 512;
        char *buf = NULL;
        size_t c = 0;
        esp_pm_config_t pm_cfg = { };

        if (ESP_OK != esp_pm_get_configuration(&pm_cfg)) {
                console_printf("esp_pm_get_configuration() failed\n");
                return;
        }

        buf = (char *)malloc(bufsz);
        if (!buf) {
                console_printf("No memory for output buffer\n");
                return;
        }

        c += snprintf(&buf[c], bufsz - c, "max_freq: %dMHz\n", pm_cfg.max_freq_mhz);
        c += snprintf(&buf[c], bufsz - c, "min_freq: %dMHz\n", pm_cfg.min_freq_mhz);
        c += snprintf(&buf[c], bufsz - c, "light_sleep_enabled: %d\n", pm_cfg.light_sleep_enable);
        c += snprintf(&buf[c], bufsz - c, "pm lock list:\n");

        FILE *f = fmemopen(&buf[c], bufsz - c, "w");
        if (f == NULL) {
                console_printf("failed to open memory stream\n");
                free(buf);
                return;
        }

        esp_pm_dump_locks(f);

        fflush(f);
        fclose(f);

        console_printf("%s\n", buf);

        free(buf);
}
#endif

static void console_syscmd_add(void)
{
        Command cmd_reboot = console_cli.addCommand("reboot", console_cmd_reboot_cb);

        Command cmd_top = console_cli.addCommand("top", console_cmd_top_cb);
        cmd_top.addFlagArgument("snapshot");
        cmd_top.addArgument("bufsz", "1600");

        Command cmd_mem = console_cli.addCommand("mem", console_cmd_mem_cb);

#ifdef CONFIG_PM_ENABLE
        Command cmd_pm = console_cli.addCommand("pm", console_cmd_esp_pm_cb);
#endif

#ifdef __LIBJJ_ESP32_WIFI_H__
        Command cmd_wifi = console_cli.addCommand("wifi", console_cmd_wifi_cb);
        cmd_wifi.addFlagArgument("up");
        cmd_wifi.addFlagArgument("down");
        cmd_wifi.addFlagArgument("nvs_reset");
        cmd_wifi.addArgument("txpower", "-1");
#endif

#ifdef __LIBJJ_RACECHRONO_BLE_H__
        Command cmd_ble = console_cli.addCommand("ble", console_cmd_ble_cb);
        cmd_ble.addArgument("txpower", "-1");
#endif

        Command cmd_save = console_cli.addCommand("save", console_cmd_save_cb);
        cmd_save.addFlagArgument("reset");
}

static void console_parse(String buf)
{
        console_serial->print("# ");
        console_serial->println(buf);

        console_cli.parse(buf);

        if (console_cli.errored()) {
                CommandError err = console_cli.getError();

                console_serial->print("ERROR: ");
                console_serial->println(err.toString());

                if (err.hasCommand()) {
                        console_serial->print("Did you mean \"");
                        console_serial->print(err.getCommand().toString());
                        console_serial->println("\"?");
                }
        }
}

static void task_console_worker(void *arg)
{
        String buf;

        while (1) {
                while (console_serial->available()) {
                        int b = console_serial->read();
                        
                        if (b < 0) {
                                pr_err("failed to rx from serial\n");
                                buf.clear();
                                break;
                        }

                        if (b == '\n') {
                                console_parse(buf);
                                buf.clear();
                        } else {
                                buf.concat((char)b);
                        }
                }

                mdelay(50);
        }
}

static int console_init(HardwareSerial *serial)
{
        console_serial = serial;
        console_serial->setTimeout(500);
        console_cli.setOnError(console_error_cb);
        xTaskCreatePinnedToCore(task_console_worker, "console", 4096, NULL, 1, NULL, CPU1);
        return 0;
}

#endif // __LIBJJ_CONSOLE_H__