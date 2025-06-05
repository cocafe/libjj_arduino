#ifndef __LIBJJ_RPC_CAN_TCP_H__
#define __LIBJJ_RPC_CAN_TCP_H__

#include <WebServer.h>

void rpc_can_tcp_add(void)
{
#ifdef CONFIG_HAVE_CANTCP_RLIMIT
        http_server.on("/can_tcp_rlimit_cfg", HTTP_GET, [](){
                unsigned enabled, update_hz;
                struct http_cfg_param params[] = {
                        HTTP_CFG_PARAM_INT(enabled, enabled),
                        HTTP_CFG_PARAM_INT(default_update_hz, update_hz),
                };
                int modified = 0;

                enabled = can_rlimit_enabled;
                update_hz = can_rlimit_update_hz_default;

                if (http_param_help_print(&http_server, params, ARRAY_SIZE(params)))
                        return;

                modified = http_param_parse(&http_server, params, ARRAY_SIZE(params));
                if (modified < 0) {
                        http_server.send(500, "text/plain", "Invalid value or internal error\n");
                        return;
                }

                if (modified == 1) {
                        can_rlimit_enabled = !!enabled;
                        can_rlimit_update_hz_default = (update_hz >= 255 )? 0 : update_hz;
                        g_cfg.rlimit_cfg.enabled = can_rlimit_enabled;
                        g_cfg.rlimit_cfg.default_hz = can_rlimit_update_hz_default;

                        http_server.send(200, "text/plain", "OK\n");
                } else {
                        char buf[128] = { };
                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"enabled\": %d,\n", can_rlimit_enabled);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"default_update_hz\": %d\n", can_rlimit_update_hz_default);
                        c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                        http_server.send(200, "text/plain", buf);
                }
        });

        http_server.on("/can_tcp_rlimit", HTTP_GET, [](){
                if (http_server.hasArg("add") || http_server.hasArg("mod")) {
                        if (!http_server.hasArg("id") || !http_server.hasArg("hz")) {
                                http_server.send(500, "text/plain", "<id> <hz 0:unlimited>\n");
                                return;
                        }
                        int err = 0;

                        String _id = http_server.arg("id");
                        unsigned id = strtoull_wrap(_id.c_str(), 16, &err);

                        if (err) {
                                http_server.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        String _hz = http_server.arg("hz");
                        unsigned hz = strtoull_wrap(_hz.c_str(), 10, &err);

                        if (err) {
                                http_server.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        if (http_server.hasArg("add")) {
                                xSemaphoreTake(lck_can_rlimit, portMAX_DELAY);

                                struct can_ratelimit *n = can_ratelimit_get(id);

                                if (n) {
                                        http_server.send(500, "text/plain", "Already in hash table\n");
                                } else {
                                        n = can_ratelimit_add(id, hz);
                                        if (!n)
                                                http_server.send(500, "text/plain", "No memory\n");
                                        else
                                                http_server.send(500, "text/plain", "OK\n");
                                }

                                xSemaphoreGive(lck_can_rlimit);
                        } else {
                                if ((err = can_ratelimit_set(id, hz))) {
                                        switch (err) {
                                        case -ENOENT:
                                                http_server.send(500, "text/plain", "No such item\n");
                                                break;

                                        case -ENOLCK:
                                                http_server.send(500, "text/plain", "Failed to grab lock\n");
                                                break;
                                        
                                        default:
                                                http_server.send(500, "text/plain", "Error!\n");
                                                break;
                                        }
                                } else {
                                        http_server.send(500, "text/plain", "OK\n");
                                }
                        }
                } else if (http_server.hasArg("get") || http_server.hasArg("del")) {
                        if (!http_server.hasArg("id")) {
                                http_server.send(500, "text/plain", "<id>\n");
                                return;
                        }

                        int err = 0;

                        String _id = http_server.arg("id");
                        unsigned id = strtoull_wrap(_id.c_str(), 16, &err);

                        if (err) {
                                http_server.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        xSemaphoreTake(lck_can_rlimit, portMAX_DELAY);
                        struct can_ratelimit *n = can_ratelimit_get(id);
                        if (!n) {
                                http_server.send(500, "text/plain", "No such item\n");
                        } else {
                                if (http_server.hasArg("get")) {
                                        char buf[16] = { };
                                        snprintf(buf, sizeof(buf), "0x%03x %u\n", n->can_id, 1000 / n->sampling_ms);
                                        http_server.send(200, "text/plain", buf);
                                } else {
                                        can_ratelimit_del(n);
                                        http_server.send(200, "text/plain", "OK\n");
                                }
                        }
                        xSemaphoreGive(lck_can_rlimit);
                } else {
                        char buf[256] = { };
                        size_t c = 0;

                        xSemaphoreTake(lck_can_rlimit, portMAX_DELAY);
                        struct can_ratelimit *n;
                        unsigned bkt;

                        hash_for_each(htbl_can_rlimit, bkt, n, hnode) {
                                c += snprintf(&buf[c], sizeof(buf) - c, "0x%03x %u\n", n->can_id, 1000 / n->sampling_ms);
                        }
                        xSemaphoreGive(lck_can_rlimit);

                        http_server.send(200, "text/plain", buf);
                }
        });
#endif // CONFIG_HAVE_CANTCP_RLIMIT
}

#endif // __LIBJJ_RPC_CAN_TCP_H__