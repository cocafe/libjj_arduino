#ifndef __LIBJJ_RPC_CAN_RLIMIT_H__
#define __LIBJJ_RPC_CAN_RLIMIT_H__

void rpc_can_rlimit_add(void)
{
#ifdef CONFIG_HAVE_CANTCP_RLIMIT
        http_rpc.on("/can_rlimit_cfg", HTTP_GET, [](){
                unsigned enabled, update_hz;
                struct http_cfg_param params[] = {
                        HTTP_CFG_PARAM_INT(enabled, enabled),
                        HTTP_CFG_PARAM_INT(default_update_hz, update_hz),
                };
                int modified = 0;

                enabled = can_rlimit_enabled;
                update_hz = can_rlimit_update_hz_default;

                if (http_param_help_print(http_rpc, params, ARRAY_SIZE(params)))
                        return;

                modified = http_param_parse(http_rpc, params, ARRAY_SIZE(params));
                if (modified < 0) {
                        http_rpc.send(500, "text/plain", "Invalid value or internal error\n");
                        return;
                }

                if (modified == 1) {
                        can_rlimit_enabled = !!enabled;
                        can_rlimit_update_hz_default = (update_hz >= 255 )? 0 : update_hz;
                        g_cfg.rlimit_cfg.enabled = can_rlimit_enabled;
                        g_cfg.rlimit_cfg.default_hz = can_rlimit_update_hz_default;

                        http_rpc.send(200, "text/plain", "OK\n");
                } else {
                        char buf[128] = { };
                        size_t c = 0;

                        c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"enabled\": %d,\n", can_rlimit_enabled);
                        c += snprintf(&buf[c], sizeof(buf) - c, "  \"default_update_hz\": %d\n", can_rlimit_update_hz_default);
                        c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

                        http_rpc.send(200, "text/plain", buf);
                }
        });

        http_rpc.on("/can_rlimit", HTTP_GET, [](){
                uint8_t enabled_saved = can_rlimit_enabled;

                if (http_rpc.hasArg("add") || http_rpc.hasArg("mod")) {
                        if (!http_rpc.hasArg("id") || !http_rpc.hasArg("hz")) {
                                http_rpc.send(500, "text/plain", "<id> <hz 0:unlimited>\n");
                                return;
                        }
                        int err = 0;

                        String _id = http_rpc.arg("id");
                        unsigned id = strtoull_wrap(_id.c_str(), 16, &err);

                        if (err) {
                                http_rpc.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        String _hz = http_rpc.arg("hz");
                        unsigned hz = strtoull_wrap(_hz.c_str(), 10, &err);

                        if (err) {
                                http_rpc.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        if (http_rpc.hasArg("add")) {
                                if (enabled_saved) {
                                        can_rlimit_enabled = 0;
                                        while (can_rlimit_lck) {
                                                vTaskDelay(pdMS_TO_TICKS(1));
                                        }
                                }

                                struct can_ratelimit *n = can_ratelimit_get(id);

                                if (n) {
                                        http_rpc.send(500, "text/plain", "Already in hash table\n");
                                } else {
                                        n = can_ratelimit_add(id, hz);
                                        if (!n)
                                                http_rpc.send(500, "text/plain", "No memory\n");
                                        else
                                                http_rpc.send(500, "text/plain", "OK\n");
                                }

                                can_rlimit_enabled = enabled_saved;
                        } else {
                                if ((err = can_ratelimit_set(id, hz))) {
                                        switch (err) {
                                        case -ENOENT:
                                                http_rpc.send(500, "text/plain", "No such item\n");
                                                break;
                                        
                                        default:
                                                http_rpc.send(500, "text/plain", "Error!\n");
                                                break;
                                        }
                                } else {
                                        http_rpc.send(500, "text/plain", "OK\n");
                                }
                        }
                } else if (http_rpc.hasArg("get") || http_rpc.hasArg("del")) {
                        if (!http_rpc.hasArg("id")) {
                                http_rpc.send(500, "text/plain", "<id>\n");
                                return;
                        }

                        int err = 0;

                        String _id = http_rpc.arg("id");
                        unsigned id = strtoull_wrap(_id.c_str(), 16, &err);

                        if (err) {
                                http_rpc.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        struct can_ratelimit *n = can_ratelimit_get(id);
                        if (!n) {
                                http_rpc.send(500, "text/plain", "No such item\n");
                        } else {
                                if (http_rpc.hasArg("get")) {
                                        char buf[16] = { };
                                        snprintf(buf, sizeof(buf), "0x%03x %u\n", n->can_id, 1000 / n->sampling_ms);
                                        http_rpc.send(200, "text/plain", buf);
                                } else {
                                        if (enabled_saved) {
                                                can_rlimit_enabled = 0;
                                                while (can_rlimit_lck) {
                                                        vTaskDelay(pdMS_TO_TICKS(1));
                                                }
                                        }

                                        can_ratelimit_del(n);

                                        can_rlimit_enabled = enabled_saved;

                                        http_rpc.send(200, "text/plain", "OK\n");
                                }
                        }
                } else {
                        char buf[256] = { };
                        size_t c = 0;

                        struct can_ratelimit *n;
                        unsigned bkt;

                        hash_for_each(htbl_can_rlimit, bkt, n, hnode) {
                                c += snprintf(&buf[c], sizeof(buf) - c, "0x%03x %u\n", n->can_id, 1000 / n->sampling_ms);
                        }

                        http_rpc.send(200, "text/plain", buf);
                }
        });
#endif // CONFIG_HAVE_CANTCP_RLIMIT
}

#endif // __LIBJJ_RPC_CAN_RLIMIT_H__