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

struct can_ratelimit_cfg {
        uint8_t enabled;
        uint8_t default_hz;
};

#ifdef CONFIG_HAVE_CANTCP_RLIMIT
struct can_ratelimit {
        struct hlist_node       hnode;
        uint16_t                can_id;
        uint16_t                sampling_ms;
        uint32_t                last_ts;
};

// XXX: this only protect add/del with rpc server
static uint8_t __attribute__((aligned(16))) can_rlimit_lck = 0; 
static struct hlist_head htbl_can_rlimit[1 << 8] = { };
static uint8_t can_rlimit_enabled = 1;
static uint8_t can_rlimit_update_hz_default = 20;
#endif // CONFIG_HAVE_CANTCP_RLIMIT

static NetworkServer can_tcp_server(CONFIG_CANTCP_SERVER_PORT);
static int can_tcp_client_fd = -1;

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

#ifdef CONFIG_HAVE_CANTCP_RLIMIT
static inline unsigned update_hz_to_ms(unsigned hz)
{
        return 1 * 1000 / hz;
}

static inline struct can_ratelimit *can_ratelimit_add(unsigned can_id, unsigned update_hz)
{
        struct can_ratelimit *n = (struct can_ratelimit *)calloc(1, sizeof(struct can_ratelimit));
        if (!n) {
                pr_err("%s(): no memory\n", __func__);
                return NULL;
        }

        INIT_HLIST_NODE(&n->hnode);

        if (update_hz == 0) {
                n->sampling_ms = 0;
        } else {
                n->sampling_ms = update_hz_to_ms(update_hz);
        }

        n->can_id = can_id;

        hash_add(htbl_can_rlimit, &n->hnode, n->can_id);

        pr_dbg("add can_id: 0x%03x sampling_ms: %ums hz: %u\n", can_id, n->sampling_ms, update_hz);

        return n;
}

static inline void can_ratelimit_del(struct can_ratelimit *n)
{
        hash_del(&n->hnode);
        free(n);
}

static inline struct can_ratelimit *can_ratelimit_get(unsigned can_id)
{
        uint32_t bkt = hash_bkt(htbl_can_rlimit, can_id);
        struct can_ratelimit *n;
        int8_t found = 0;

        hash_for_bkt_each(htbl_can_rlimit, bkt, n, hnode) {
                if (n->can_id != can_id)
                        continue;

                found = 1;
                break;
        }

        if (found)
                return n;

        return NULL;
}

static int can_ratelimit_set(unsigned can_id, unsigned update_hz)
{
        struct can_ratelimit *n = can_ratelimit_get(can_id);

        if (n) {
                if (update_hz == 0) {
                        pr_dbg("can_id 0x%03x: no ratelimit\n", can_id);
                        n->sampling_ms = 0;
                } else {
                        n->sampling_ms = update_hz_to_ms(update_hz);
                }

                return 0;
        }

        return -ENOENT;
}

static int __is_can_id_ratelimited(unsigned can_id, uint32_t now)
{
        struct can_ratelimit *n = can_ratelimit_get(can_id);

        if (!n) {
                if (can_rlimit_update_hz_default) {
                        can_ratelimit_add(can_id, can_rlimit_update_hz_default);
                }
        } else {
                if (n->sampling_ms == 0)
                        return 0;

                if (now - n->last_ts >= n->sampling_ms) {
                        n->last_ts = now;
                        return 0;
                }

                return 1;
        }

        return 0;
}

static int is_can_id_ratelimited(unsigned can_id, uint32_t now)
{
        int ret;

        if (!can_rlimit_enabled)
                return 0;

        // whitelist: 0x7DF and 0x7E0...0x7EF
        if ((can_id == 0x7DF) || (can_id && 0x7E0))
                return 0;

        WRITE_ONCE(can_rlimit_lck, 1);

        ret = __is_can_id_ratelimited(can_id, now);

        WRITE_ONCE(can_rlimit_lck, 0);

        return ret;
}

static void can_ratelimit_init(struct can_ratelimit_cfg *cfg)
{
        if (cfg) {
                can_rlimit_enabled = cfg->enabled;
                can_rlimit_update_hz_default = cfg->default_hz;
        }

        for (int i = 0; i < ARRAY_SIZE(htbl_can_rlimit); i++) {
                htbl_can_rlimit[i].first = NULL;
        }

        hash_init(htbl_can_rlimit);
}
#endif // CONFIG_HAVE_CANTCP_RLIMIT

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

static uint8_t *pattern_find(uint8_t *haystack, size_t haystack_len,
                             uint8_t *needle, size_t needle_len)
{
        if (needle_len == 0 || haystack_len < needle_len) {
                return NULL;
        }

        for (size_t i = 0; i <= haystack_len - needle_len; i++) {
                if (memcmp(&haystack[i], needle, needle_len) == 0) {
                        return &haystack[i];
                }
        }

        return NULL;
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
                        // pr_info("%s(): invalid packet\n", __func__);
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

static void can_tcp_recv_cb(can_frame_t *f)
{
        int sockfd = READ_ONCE(can_tcp_client_fd);
        int n;

        if (READ_ONCE(sockfd) < 0) {
                return;
        }

#ifdef CONFIG_HAVE_CANTCP_RLIMIT
        if (is_can_id_ratelimited(f->id, esp32_millis())) {
                return;
        }
#endif

        f->magic = htole32(CAN_DATA_MAGIC);
        f->id = htole32(f->id);

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

#ifdef CONFIG_CANTCP_NO_DELAY
                // disable TCP Nagle
                // send small pkt asap
                // but will increase memory pressure
                client.setNoDelay(true);
#endif

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

static void can_tcp_server_init(unsigned cpu)
{
        if (can_dev) {
                can_recv_cb_register(can_tcp_recv_cb);
                can_tcp_server.begin();
#ifdef CONFIG_CANTCP_NO_DELAY
                can_tcp_server.setNoDelay(true);
#endif
                xTaskCreatePinnedToCore(task_can_tcp, "can_tcp", 4096, NULL, 1, NULL, cpu);
#ifdef CAN_TCP_LED_BLINK
                xTaskCreatePinnedToCore(task_can_tcp_led_blink, "led_blink_tcp", 1024, NULL, 1, NULL, cpu);
#endif
        }
}

#endif // __LIBJJ_CAN_TCP_H__