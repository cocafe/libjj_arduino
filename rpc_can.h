#ifndef __LIBJJ_RPC_CAN_H__
#define __LIBJJ_RPC_CAN_H__

void rpc_can_add(void)
{
        http_rpc.on("/can_stats", HTTP_GET, [](){
                char buf[1024] = { };
                size_t c = 0;

                c += snprintf(&buf[c], sizeof(buf) - c, "# HELP can_stats\n");
                c += snprintf(&buf[c], sizeof(buf) - c, "# TYPE can_stats gauge\n");
//              c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"inited\"} %u\n", can_dev ? 1 : 0);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"send_err\"} %llu\n", cnt_can_send_error);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"recv_err\"} %llu\n", cnt_can_recv_error);
#ifdef CAN_TWAI_USE_RINGBUF
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"can_recv_drop\"} %llu\n", cnt_can_recv_drop);
#endif
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"recv_rtr\"} %llu\n", cnt_can_recv_rtr);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"recv\"} %llu\n", cnt_can_recv);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"send\"} %llu\n", cnt_can_send);
#ifdef __LIBJJ_CAN_TCP_H__
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"tcp_recv_err\"} %llu\n", cnt_can_tcp_recv_error);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"tcp_recv\"} %llu\n", cnt_can_tcp_recv);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"tcp_send\"} %llu\n", cnt_can_tcp_send);
#endif
#ifdef __LIBJJ_RACECHRONO_FWD_H__
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"udp_recv_err\"} %llu\n", cnt_can_udp_recv_error);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"udp_recv\"} %llu\n", cnt_can_udp_recv);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"udp_send_err\"} %llu\n", cnt_can_udp_send_error);
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"udp_send\"} %llu\n", cnt_can_udp_send);
#endif
#ifdef __LIBJJ_RACECHRONO_BLE_H__
                c += snprintf(&buf[c], sizeof(buf) - c, "can_stats{t=\"ble_send\"} %llu\n", cnt_can_ble_send);
#endif

                http_rpc.send(200, "text/plain", buf);
        });
}

#endif // __LIBJJ_RPC_CAN_H__
