#ifndef __LIBJJ_RPC_PINTEST_H__
#define __LIBJJ_RPC_PINTEST_H__

void rpc_pintest_add(void)
{
        http_rpc.on("/pintest", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        int pin = -1, mode = -1, strength = -1, state = -1;

                        if (http_rpc.hasArg("pin")) {
                                String arg = http_rpc.arg("pin");
                                int err;
                                uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);
                                
                                if (err || i > 255) {
                                        http_rpc.send(500, "text/plain", "invalid value\n");
                                        return;
                                }

                                pin = (int)i;
                        }

                        if (http_rpc.hasArg("mode")) {
                                String arg = http_rpc.arg("mode");

                                if (arg == "INPUT") {
                                        mode = INPUT;
                                } else if (arg == "OUTPUT") {
                                        mode = OUTPUT;
                                } else if (arg == "INPUT_PULLUP") {
                                        mode = INPUT_PULLUP;
                                } else if (arg == "INPUT_PULLDOWN") {
                                        mode = INPUT_PULLDOWN;
                                } else if (arg == "OUTPUT_OPEN_DRAIN") {
                                        mode = OUTPUT_OPEN_DRAIN;
                                } else {
                                        http_rpc.send(500, "text/plain", "invalid value\n");
                                        return;
                                }
                        }

                        if (http_rpc.hasArg("state")) {
                                String arg = http_rpc.arg("state");

                                if (arg == "LOW") {
                                        state = LOW;
                                } else if (arg == "HIGH") {
                                        state = HIGH;
                                }
                        }

                        if (http_rpc.hasArg("strength")) {
                                String arg = http_rpc.arg("strength");
                                int err;
                                uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);
                                
                                if (err || i >= GPIO_DRIVE_CAP_MAX) {
                                        http_rpc.send(500, "text/plain", "invalid value\n");
                                        return;
                                }

                                strength = (int)i;
                        }

                        if (pin < 0 || mode < 0) {
                                http_rpc.send(200, "text/plain", "<pin> <mode: INPUT/OUTPUT/INPUT_PULLUP/INPUT_PULLDOWN/OUTPUT_OPEN_DRAIN> [state: LOW/HIGH] [strength: 0/1/2/3]\n");
                                return;
                        }

                        pinMode(pin, mode);

                        switch (mode) {
                        case OUTPUT:
                        case OUTPUT_OPEN_DRAIN:
                                if (strength >= 0) {
                                        gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)strength);
                                }

                                if (state >= 0) {
                                        digitalWrite(pin, state);
                                }

                                break;

                        default:
                                break;
                        }
                        
                        http_rpc.send(200, "text/plain", "OK\n");
                } else if (http_rpc.hasArg("dump")) {
                        if (!http_rpc.hasArg("pin")) {
                                http_rpc.send(200, "text/plain", "<pin>\n");
                                return;
                        }

                        String arg = http_rpc.arg("pin");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);
                        
                        if (err || i > 255) {
                                http_rpc.send(500, "text/plain", "invalid value\n");
                                return;
                        }

                        int bufsz = 512;
                        char *buf = NULL;

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

                        FILE *f = fmemopen(buf, bufsz, "w");
                        if (f == NULL) {
                                http_rpc.send(200, "failed to open memory stream\n");
                                free(buf);
                                return;
                        }

                        gpio_dump_io_configuration(f, (1ULL << i));

                        fflush(f);
                        fclose(f);

                        http_rpc.send(200, "text/plain", buf);

                        free(buf);
                }  else if (http_rpc.hasArg("get")) {
                        char buf[32];

                        if (!http_rpc.hasArg("pin")) {
                                http_rpc.send(200, "text/plain", "<pin>\n");
                                return;
                        }

                        String arg = http_rpc.arg("pin");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);
                        
                        if (err || i > 255) {
                                http_rpc.send(500, "text/plain", "invalid value\n");
                                return;
                        }

                        snprintf(buf, sizeof(buf), "%s\n", digitalRead(i) == HIGH ? "HIGH" : "LOW");

                        http_rpc.send(200, "text/plain", buf);
                } else {
                        http_rpc.send(200, "text/plain", "<get>/<set>/<dump>\n");
                }
        });
}

#endif // __LIBJJ_RPC_PINTEST_H__