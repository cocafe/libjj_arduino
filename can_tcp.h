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

#ifndef CONFIG_CANTCP_SERVER_PORT
#warning CONFIG_CANTCP_SERVER_PORT is not defined, use default port number
#define CONFIG_CANTCP_SERVER_PORT                 35000
#endif

#include "can.h"

struct cantcp_cfg {
        uint8_t enabled;
        uint8_t nodelay;
};

static NetworkServer can_tcp_server(CONFIG_CANTCP_SERVER_PORT);
static int can_tcp_client_fd = -1;
static uint8_t can_tcp_no_delay = 0;

static uint64_t cnt_can_tcp_send;       // send to remote
static uint64_t cnt_can_tcp_send_bytes;
static uint64_t cnt_can_tcp_send_error;
static uint64_t cnt_can_tcp_recv;       // from remote
static uint64_t cnt_can_tcp_recv_bytes;
static uint64_t cnt_can_tcp_recv_error;
static uint64_t cnt_can_tcp_recv_invalid;

#ifdef CAN_TCP_LED_BLINK
static uint8_t can_tcp_txrx = 0;
static uint8_t can_tcp_led = CAN_TCP_LED_BLINK;
static uint8_t can_tcp_led_blink = 1;

static void task_can_tcp_led_blink(void *arg)
{
        while (1) {
                can_tcp_txrx = 0;

                vTaskDelay(pdMS_TO_TICKS(500));

                if (!can_tcp_led_blink)
                        continue;

                if (can_tcp_client_fd < 0)
                        continue;

                if (can_tcp_txrx) {
                        static uint8_t last_on = 0;

                        // blink
                        if (!last_on) {
                                led_on(can_tcp_led, 0, 255, 0);
                                last_on = 1;
                        } else {
                                led_off(can_tcp_led);
                                last_on = 0;
                        }
                } else {
                        led_on(can_tcp_led, 0, 0, 255);
                }
        }
}
#endif // CAN_TCP_LED_BLINK

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

#ifdef CAN_TCP_LED_BLINK
                can_tcp_txrx = 1;
#endif

next_frame:
                pos += dlen + sizeof(can_frame_t);
        }

        return pos; // consumed
}

static void can_tcp_recv_cb(can_frame_t *f, struct can_rlimit_node *rlimit)
{
        int sockfd = READ_ONCE(can_tcp_client_fd);
        int n;

        if (READ_ONCE(sockfd) < 0) {
                return;
        }

#ifdef CONFIG_HAVE_CAN_RLIMIT
        if (is_can_id_ratelimited(rlimit, CAN_RLIMIT_TYPE_TCP, esp32_millis())) {
                return;
        }
#endif

        if ((n = send(sockfd, (uint8_t *)f, (size_t)(sizeof(can_frame_t) + f->dlc), 0)) < 0) {
                if (errno == EINTR) {
                        return;
                }

                pr_dbg("send(): n: %d err: %d %s\n", n, errno, strerror(abs(errno)));
                cnt_can_tcp_send_error++;

                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                WRITE_ONCE(can_tcp_client_fd, -1);

                return;
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

#ifdef CAN_TCP_LED_BLINK
        can_tcp_txrx = 1;
#endif
}

static void can_tcp_server_worker(void)
{
        NetworkClient client = can_tcp_server.accept();

        if (client) {
                static uint8_t buf[(sizeof(can_frame_t) + 8) * 10] = { };
                static int pos = 0;
                int sockfd = client.fd();

                pr_info("client %s:%u connected\n", client.remoteIP().toString().c_str(), client.remotePort());

                {
                        int bufsize = 4096;

                        setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
                        setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
                }

                // for client.readBytes()
                // client.setTimeout(1000);

                // disable TCP Nagle
                // send small pkt asap
                // but will increase memory pressure
                if (can_tcp_no_delay) {
                        client.setNoDelay(true);
                }

                WRITE_ONCE(can_tcp_client_fd, sockfd);

                pos = 0;

                while (client.connected()) {
                        int n, c;

                        // return 0 on timedout
                        // n = client.readBytes(&buf[pos], sizeof(buf) - pos);

                        // return 0 on no data, this shit does not block
                        // n = client.read(&buf[pos], sizeof(buf) - pos);

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
                                        vTaskDelay(pdMS_TO_TICKS(100));

                                        continue;
                                } else {
                                        // err code 113 here, which should be 103 in posix
                                        // XXX: wifi stack got error?!
                                        if (errno == ECONNABORTED) {
                                                WiFi.disconnect(true);
                                                wifi_connection_check_post();
                                        }

                                        // close connection now
                                        break;
                                }
                        }
                }

#ifdef CAN_TCP_LED_BLINK
                led_off(can_tcp_led);
#endif

                pr_info("client disconnected\n");
                WRITE_ONCE(can_tcp_client_fd, -1);
                client.stop();
        }
}

static void task_can_tcp(void *arg)
{
        pr_info("started\n");

        while (1) {
                can_tcp_server_worker();
                vTaskDelay(pdMS_TO_TICKS(1));
        }
}

static __unused void can_tcp_server_init(struct cantcp_cfg *cfg, unsigned cpu)
{
        can_tcp_no_delay = cfg->nodelay;

        if (can_dev) {
                can_recv_cb_register(can_tcp_recv_cb);
                can_tcp_server.begin();

                if (can_tcp_no_delay) {
                        can_tcp_server.setNoDelay(true);
                }

                xTaskCreatePinnedToCore(task_can_tcp, "can_tcp", 4096, NULL, 1, NULL, cpu);
#ifdef CAN_TCP_LED_BLINK
                xTaskCreatePinnedToCore(task_can_tcp_led_blink, "led_blink_tcp", 4096, NULL, 1, NULL, cpu);
#endif
        }
}

#endif // __LIBJJ_CAN_TCP_H__