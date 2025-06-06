#ifndef __LIBJJ_RPC_LED_H__
#define __LIBJJ_RPC_LED_H__

#include <WebServer.h>

void rpc_led_add(void)
{
#ifdef HAVE_WS2812_LED
        http_rpc.on("/led_ws2812_brightness", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        String arg = http_rpc.arg("set");
                        int err;
                        uint32_t i = strtoull_wrap(arg.c_str(), 10, &err);
                        
                        if (err || i > 255)
                                http_rpc.send(200, "text/plain", "Invalid value\n");
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
}

#endif // __LIBJJ_RPC_LED_H__