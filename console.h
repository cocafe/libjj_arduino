#ifndef __LIBJJ_CONSOLE_H__
#define __LIBJJ_CONSOLE_H__

#include <SimpleCLI.h>

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
        }
}

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

static void console_syscmd_add(void)
{
        Command cmd_reboot = console_cli.addCommand("reboot", console_cmd_reboot_cb);

        Command cmd_top = console_cli.addCommand("top", console_cmd_top_cb);
        cmd_top.addFlagArgument("snapshot");
        cmd_top.addArgument("bufsz", "1600");

        Command cmd_wifi = console_cli.addCommand("wifi", console_cmd_wifi_cb);
        cmd_wifi.addFlagArgument("up");
        cmd_wifi.addFlagArgument("down");
        cmd_wifi.addFlagArgument("nvs_reset");

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