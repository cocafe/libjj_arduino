#ifndef __LIBJJ_CAN_TCP_H__
#define __LIBJJ_CAN_TCP_H__

#include <stdint.h>
#include <endian.h>

#include <lwip/sockets.h>
#include <WebServer.h>

#include "utils.h"
#include "logging.h"

#include "list.h"
#include "hashtable.h"

#define CAN_DATA_MAGIC                  (0xC0CAFEEE)

#include "can.h"

struct cantcp_cfg {
        uint8_t enabled;
        uint8_t nodelay;
        uint16_t port;
        uint16_t sock_bufsz;
};

static cantcp_cfg *g_cantcp_cfg;

static int can_tcp_client_fd = -1;

static uint64_t cnt_can_tcp_send;       // send to remote
static uint64_t cnt_can_tcp_send_bytes;
static uint64_t cnt_can_tcp_send_error;
static unsigned cnt_can_tcp_send_hz;
static uint64_t cnt_can_tcp_recv;       // from remote
static uint64_t cnt_can_tcp_recv_bytes;
static uint64_t cnt_can_tcp_recv_error;
static uint64_t cnt_can_tcp_recv_invalid;

static int is_valid_can_frame(can_frame_t *f)
{
        if (unlikely(le32toh(f->magic) != CAN_DATA_MAGIC)) {
                return 0;
        }

        if (unlikely(f->dlc != CAN_FRAME_DLC)) {
                return 0;
        }

        return 1;
}

static int can_frames_input(uint8_t *buf, int len)
{
        int pos;

        if (likely(le32toh(*((uint32_t *)&buf[0])) == CAN_DATA_MAGIC)) {
                pos = 0;
        } else {
                const static uint32_t magic = htole32(CAN_DATA_MAGIC);
                uint8_t *pattern = (uint8_t *)&magic;
                uint8_t *ret = pattern_find(buf, len, pattern, sizeof(magic));

                if (!ret) {
                        // pr_info("invalid packet\n", __func__);
                        cnt_can_tcp_recv_invalid++;
                        return -EINVAL;
                }

                pos = (intptr_t)ret - (intptr_t)buf;
        }

        while (pos < len) {
                can_frame_t *f = (can_frame_t *)&buf[pos];
                uint32_t dlen = f->dlc;

                if (pos + sizeof(can_frame_t) > len ||
                    pos + dlen + sizeof(can_frame_t) > len) {
                        break;
                }

                if (f->heartbeat)
                        goto next_frame;

                if (!is_valid_can_frame(f)) {
                        cnt_can_tcp_recv_invalid++;
                        return -EINVAL;
                }

                // XXX: when use TWAI
                //      send may block for a long while
                //      then TCP window will be full
                can_dev->send(le32toh(f->id), f->dlc, f->data);

#if 0
                pr_raw("tcp recv: id: 0x%04lx len: %u ", le32toh(f->id), f->dlc);
                for (uint8_t i = 0; i < f->dlc; i++) {
                        pr_raw("0x%02x ", f->data[i]);
                }
                pr_raw("\n");
#endif

                cnt_can_tcp_recv++;

                wifi_activity = 1;

next_frame:
                pos += dlen + sizeof(can_frame_t);
        }

        return pos; // consumed
}

static void can_tcp_recv_cb(can_frame_t *f, struct can_rlimit_node *rlimit)
{
        static unsigned ts_hz_counter = 0;
        static unsigned hz_counter;
        unsigned now = esp32_millis();
        int sockfd = READ_ONCE(can_tcp_client_fd);
        int n;

        if (READ_ONCE(sockfd) < 0) {
                return;
        }

#ifdef CONFIG_HAVE_CAN_RLIMIT
        if (is_can_id_ratelimited(rlimit, CAN_RLIMIT_TYPE_TCP, now)) {
                return;
        }
#endif

        if ((n = send(sockfd, (uint8_t *)f, (size_t)(sizeof(can_frame_t) + f->dlc), 0)) < 0) {
                if (errno == EINTR) {
                        return;
                }

                pr_dbg("send(): n: %d err: %d %s\n", n, errno, strerror(abs(errno)));
                cnt_can_tcp_send_error++;

                pr_info("close connection\n");
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                WRITE_ONCE(can_tcp_client_fd, -1);

                return;
        }

        hz_counter++;
        if (now - ts_hz_counter >= 1000) {
                cnt_can_tcp_send_hz = hz_counter;
                ts_hz_counter = now;
                hz_counter = 0;
        }

        cnt_can_tcp_send++;
        cnt_can_tcp_send_bytes += n;

#if 0
        pr_raw("tcp send: id: 0x%04lx len: %u ", le32toh(f->id), f->dlc);
        for (uint8_t i = 0; i < f->dlc; i++) {
                pr_raw("0x%02x ", f->data[i]);
        }
        pr_raw("\n");
#endif

        wifi_activity = 1;
}

static void can_tcp_server_recv(int sockfd)
{
        static uint8_t buf[(sizeof(can_frame_t) + 8) * 10] = { };
        static int pos = 0;

        pos = 0;

        while (true) {
                int n, c;

                // this posix dude blocks
                n = recv(sockfd, &buf[pos], sizeof(buf) - pos, 0);

                if (n > 0) {
                        pos += n;

                        c = can_frames_input(buf, pos);
                        if (c > 0) {
                                if (c < pos) {
                                        memmove(&buf[0], &buf[c], pos - c);
                                        pos -= c;
                                } else { // c == pos
                                        pos = 0;
                                }
                        } else { // c <= 0: got error, drop all
                                pos = 0;
                        }

                        cnt_can_tcp_recv_bytes += n;
                } else {
                        if (errno == EINTR)
                                continue;

                        pr_dbg("recv(): n: %d err: %d %s\n", n, errno, strerror(abs(errno)));
                        cnt_can_tcp_recv_error++;

                        if (errno == EAGAIN) {
                                pr_dbg("recv() timedout, n = %d\n", n);
                                mdelay((100));

                                continue;
                        } else {
                                // err code 113 here, which should be 103 in posix
                                // XXX: wifi stack got error?!
                                // if (errno == ECONNABORTED) {
                                //         if (wifi_sta_connected) {
                                //                 esp_wifi_disconnect();
                                //         }
                                // }

                                // close connection now
                                break;
                        }
                }
        }
}

static void task_can_tcp(void *arg)
{
        struct cantcp_cfg *cfg = (struct cantcp_cfg *)arg;
        struct sockaddr_in server_addr;
        unsigned server_port = cfg->port;
        char addr_str[64];
        int listen_sock;

        listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock < 0) {
                pr_err("socket(): %d %s\n", errno, strerror(abs(errno)));
                vTaskDelete(NULL);
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(server_port);

        if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                pr_err("bind(): %d %s\n", errno, strerror(abs(errno)));
                close(listen_sock);
                vTaskDelete(NULL);
                return;
        }

        inet_ntop(AF_INET, &server_addr.sin_addr, addr_str, sizeof(addr_str));
        pr_info("start server at %s:%d\n", addr_str, server_port);

        listen(listen_sock, 1);

        while (1) {
                struct sockaddr_in client_addr = { };
                socklen_t client_addr_len = sizeof(client_addr);
                int client_sock;

                cnt_can_tcp_send_hz = 0;

                client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
                if (client_sock < 0) {
                        pr_err("accept(): %d %s\n", errno, strerror(abs(errno)));
                        continue;
                }

                {
                        int enable = 1;
                        setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));

                        // Interval before starting keepalive probes (seconds)
                        int idle = 10;     // start sending probes after 10s idle
                        int interval = 5;  // send probes every 5s
                        int count = 3;     // after 3 failed probes, consider peer dead
                        setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
                        setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
                        setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
                }

                if (cfg->sock_bufsz) {
                        int bufsize = cfg->sock_bufsz;
                        setsockopt(client_sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
                        setsockopt(client_sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
                }

                if (cfg->nodelay) {
                        int flag = 1;
                        setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                }

                inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
                pr_err("client: %s:%d connected\n", addr_str, ntohs(client_addr.sin_port));

                WRITE_ONCE(can_tcp_client_fd, client_sock);

                can_tcp_server_recv(client_sock);

                if (READ_ONCE(can_tcp_client_fd) >= 0) {
                        pr_info("close connection\n");
                        shutdown(client_sock, SHUT_RDWR);
                        close(client_sock);
                }

                WRITE_ONCE(can_tcp_client_fd, -1);

                mdelay((200));
        }
}

static __unused void can_tcp_server_init(struct cantcp_cfg *cfg, unsigned cpu)
{
        if (!cfg->enabled || !can_dev)
                return;

        g_cantcp_cfg = cfg;

        can_recv_cb_register(can_tcp_recv_cb);

        xTaskCreatePinnedToCore(task_can_tcp, "cantcp_srv", 4096, g_cantcp_cfg, 1, NULL, cpu);
}

#endif // __LIBJJ_CAN_TCP_H__