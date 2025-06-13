#ifndef __LIBJJ_RPC_SPIFFS_H__
#define __LIBJJ_RPC_SPIFFS_H__

#include <WebServer.h>

void rpc_spiffs_add(void)
{
        if (!have_spiffs)
                return;

        http_rpc.on("/spiffs_stats", HTTP_GET, [](){
                char buf[100] = { };
                size_t c = 0;

                c += snprintf(&buf[c], sizeof(buf) - c, "total_bytes: %zu\n", SPIFFS.totalBytes());
                c += snprintf(&buf[c], sizeof(buf) - c, "used_bytes: %zu\n", SPIFFS.usedBytes());

                http_rpc.send(200, "text/plain", buf);
        });

        http_rpc.on("/spiffs_format", HTTP_GET, [](){
                if (SPIFFS.format())
                        http_rpc.send(200, "text/plain", "OK\n");
                else
                        http_rpc.send(500, "text/plain", "Failed\n");
        });
}

#endif // __LIBJJ_RPC_SPIFFS_H__