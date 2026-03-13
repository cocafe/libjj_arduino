#ifndef __LIBJJ_RPC_LED_H__
#define __LIBJJ_RPC_LED_H__

void rpc_led_add(void)
{
#ifdef HAVE_WS2812_LED
        http_rpc.on("/led_ws2812_brightness", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        String arg = http_rpc.arg("set");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);
                        
                        if (err || i > 255) {
                                http_rpc.send(500, "text/plain", "invalid value\n");
                                return;
                        }

                        FastLED.setBrightness(i);

                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[8] = { };

                        snprintf(buf, sizeof(buf), "%d\n", FastLED.getBrightness());

                        http_rpc.send(200, "text/plain", buf);
                }

                http_rpc.send(200, "text/plain", "OK\n");
        });
#endif

        http_rpc.on("/ledtest", HTTP_GET, [](){
                int which = -1, state = -1;
                unsigned r = 0, g = 0, b = 0;

                if (http_rpc.hasArg("which")) {
                        String arg = http_rpc.arg("which");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);

                        if (err || i > 255) {
                                http_rpc.send(500, "text/plain", "invalid value\n");
                                return;
                        }

                        which = (int)i;
                }

                if (http_rpc.hasArg("on")) {
                        String arg = http_rpc.arg("state");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);

                        if (err || i > 255) {
                                http_rpc.send(500, "text/plain", "invalid value\n");
                                return;
                        }

                        state = (int)i;
                }

                if (http_rpc.hasArg("r")) {
                        String arg = http_rpc.arg("r");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);

                        if (err || i > 255 || i < 0) {
                                http_rpc.send(500, "text/plain", "invalid value\n");
                                return;
                        }

                        r = (unsigned)i;
                }

                if (http_rpc.hasArg("g")) {
                        String arg = http_rpc.arg("g");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);

                        if (err || i > 255 || i < 0) {
                                http_rpc.send(500, "text/plain", "invalid value\n");
                                return;
                        }

                        g = (unsigned)i;
                }

                if (http_rpc.hasArg("b")) {
                        String arg = http_rpc.arg("b");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);

                        if (err || i > 255 || i < 0) {
                                http_rpc.send(500, "text/plain", "invalid value\n");
                                return;
                        }

                        b = (unsigned)i;
                }

                if (which < 0 || state < 0 || which >= NUM_LEDS) {
                        http_rpc.send(200, "text/plain", "<which: int> <on: 0/1> <r> <g> <b>\n");
                        return;
                }

                if (state) {
                        led_on(which, r, g, b);
                } else {
                        led_off(which);
                }

                http_rpc.send(200, "text/plain", "OK\n");
        });
}

#endif // __LIBJJ_RPC_LED_H__