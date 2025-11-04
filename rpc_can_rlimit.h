#ifndef __LIBJJ_RPC_CAN_RLIMIT_H__
#define __LIBJJ_RPC_CAN_RLIMIT_H__

void rpc_can_rlimit_add(void)
{
#ifdef CONFIG_HAVE_CAN_RLIMIT
        http_rpc.on("/can_rlimit_enable", HTTP_GET, [](){
                if (http_rpc.hasArg("set")) {
                        int err;
                        String _set = http_rpc.arg("set");
                        unsigned set = strtoull_wrap(_set.c_str(), 10, &err);

                        if (err) {
                                http_rpc.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        can_rlimit.cfg->enabled = !!set;
                } else {
                        char buf[16] = { };

                        snprintf(buf, sizeof(buf), "%d\n", can_rlimit.cfg->enabled);

                        http_rpc.send(200, "text/plain", buf);
                }
        });

        http_rpc.on("/can_rlimit", HTTP_GET, [](){
                if (http_rpc.hasArg("add") || http_rpc.hasArg("mod")) {
                        if (!http_rpc.hasArg("id") || !http_rpc.hasArg("hz") || http_rpc.hasArg("type")) {
                                http_rpc.send(500, "text/plain", "<id> <type> <hz 0:unlimited -1:drop>\n");
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
                        int hz = strtoll_wrap(_hz.c_str(), 10, &err);

                        if (err) {
                                http_rpc.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        String _type = http_rpc.arg("type");
                        unsigned type = strtoll_wrap(_type.c_str(), 10, &err);

                        if (err) {
                                http_rpc.send(500, "text/plain", "Invalid value\n");
                                return;
                        }

                        struct can_rlimit_node *n = can_ratelimit_get(id);
                        if (http_rpc.hasArg("add")) {
                                // if (enabled_saved) {
                                //         can_rlimit.cfg->enabled = 0;
                                //         while (can_rlimit_lck) {
                                //                 vTaskDelay(pdMS_TO_TICKS(1));
                                //         }
                                // }

                                if (n) {
                                        http_rpc.send(500, "text/plain", "Already in hash table\n");
                                } else {
                                        n = can_ratelimit_add(id);

                                        if (n)
                                                can_ratelimit_set(n, type, hz);

                                        if (!n)
                                                http_rpc.send(500, "text/plain", "No memory\n");
                                        else
                                                http_rpc.send(500, "text/plain", "OK\n");
                                }
                        } else {
                                if ((err = can_ratelimit_set(n, type, hz))) {
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

                        struct can_rlimit_node *n = can_ratelimit_get(id);
                        if (!n) {
                                http_rpc.send(500, "text/plain", "No such item\n");
                        } else {
                                if (http_rpc.hasArg("get")) {
                                        char buf[32] = { };
                                        snprintf(buf, sizeof(buf), "0x%03x %d %d\n", n->can_id, n->data[0].update_ms, n->data[1].update_ms);

                                        http_rpc.send(200, "text/plain", buf);
                                } else {
                                        can_ratelimit_del(n);

                                        http_rpc.send(200, "text/plain", "OK\n");
                                }
                        }
                } else {
                        char buf[1024] = { };
                        size_t c = 0;

                        struct can_rlimit_node *n;
                        unsigned bkt;

                        hash_for_each(can_rlimit.htbl, bkt, n, hnode) {
                                c += snprintf(&buf[c], sizeof(buf) - c, "0x%03x ", n->can_id);

                                for (unsigned i = 0; i < ARRAY_SIZE(n->data); i++) {
                                        c += snprintf(&buf[c], sizeof(buf) - c, "%d ", n->data[i].update_ms);
                                }

                                c += snprintf(&buf[c], sizeof(buf) - c, "\n");
                        }

                        http_rpc.send(200, "text/plain", buf);
                }
        });
#endif // CONFIG_HAVE_CAN_RLIMIT
}

#endif // __LIBJJ_RPC_CAN_RLIMIT_H__