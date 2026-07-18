#ifndef __LIBJJ_REDIS_CLIENT_H__
#define __LIBJJ_REDIS_CLIENT_H__

#include <errno.h>
#include <string.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/priv/tcp_priv.h>
#include <lwip/priv/sockets_priv.h>
#include <lwip/api.h>
#include <lwip/tcp.h>
#include <esp_vfs.h>
#include <sys/uio.h>

#include "logging.h"
#include "jkey.h"

// redis protocol format:
// *<token_cnt>\r\n
// $<token0_len>\r\n
// <token0_str>\r\n
// $<token1_len>\r\n
// <token1_str>\r\n
// ...
//
// example: "XADD mystream * field1 value1 field2 value2"
// *7\r\n
// $4\r\n
// XADD\r\n
// $8\r\n
// mystream\r\n
// $1\r\n
// *\r\n
// $6\r\n
// field1\r\n
// $6\r\n
// value1\r\n
// $6\r\n
// field2\r\n
// $6\r\n
// value2\r\n

struct redis_client_cfg {
        uint8_t enabled;
        uint8_t nodelay;
        char server[32];
        unsigned port;
};

static int redis_sockfd = -1;
static TaskHandle_t task_handle_redis_client;
static SemaphoreHandle_t lck_redis_client_send;

static int redis_cmd_token_count(const char *s)
{
        int count = 0;
        int in_token = 0;

        while (*s) {
                if (isspace((unsigned char)*s)) {
                        in_token = 0;
                } else if (!in_token) {
                        in_token = 1;
                        count++;
                }
                s++;
        }

        return count;
}

int redis_client_cmd_send(char *cmd)
{
        char buf[2048]; // FIXME: may change to heap alloc?
        int token_cnt = redis_cmd_token_count(cmd);
        unsigned c = 0;
        int err = 0;

        if (token_cnt <= 0)
                return -EINVAL;

        c += snprintf(&buf[c], sizeof(buf) - c, "*%d\r\n", token_cnt);

        {
                char *tok = strtok(cmd, " ");
                while (tok != NULL) {
                        unsigned toklen = strlen(tok);
                        c += snprintf(&buf[c], sizeof(buf) - c, "$%u\r\n%s\r\n", toklen, tok);
                        tok = strtok(NULL, " ");
                }
        }

        if (unlikely(c == sizeof(buf))) {
                pr_warn("buffer may have run out\n");
        }

        char *pos = buf;
        int sent = 0;
        while (c > 0) {
                int n = send(redis_sockfd, pos, c, 0);

                if (n < 0) {
                        pr_err("send(): %d %s\n", errno, strerror(abs(errno)));
                        err = -errno;
                        break;
                }

                pos += n;
                c -= n;
                sent += n;
        }

        if (err) {
                xTaskNotifyGive(task_handle_redis_client);
                return err;
        }

        return sent;
}

int redis_client_reply_check(void)
{
        char buf[128];
        int retry = 0;
        int need_close = 0;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(redis_sockfd, &fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int ret = select(redis_sockfd + 1, &fds, NULL, NULL, &tv);
        if (ret == 0) {
                pr_dbg("time out for server resp\n");
                return -EAGAIN;
        } else if (ret < 0) {
                pr_err("select(): %d %s\n", ret, strerror(ret));
                return ret;
        }

        do {
                int n = recv(redis_sockfd, buf, sizeof(buf) - 1, 0);
                if (n > 0) {
                        // pr_raw("recv redis server:\n");
                        // hexdump(buf, n);

                        if (buf[0] == '-') {
                                pr_info("recv error: %.*s\n", n, buf);
                                return -EIO;
                        } else if (buf[0] == '$') {
                                // pr_dbg("recv rsp: %.*s\n", n, buf);
                                return 0;
                        }
                } else {
                        if (errno == EINTR || errno == ETOOMANYREFS)
                                continue;

                        if (errno == 0) {
                                need_close = 1;
                                break;
                        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                retry++;

                                if (retry >= 10) {
                                        pr_err("too many retry, disconnect\n");
                                        need_close = 1;
                                        break;
                                }

                                continue;
                        } else {
                                need_close = 1;
                                break;
                        }
                }
        } while(1);

        if (need_close) {
                xTaskNotifyGive(task_handle_redis_client);
                return -errno;
        }

        return 0;
}

int redis_xadd_token_print(char *buf, size_t buflen, int *pos, unsigned *tkncnt, const char *field, const char *value)
{
        int c;

        c = snprintf(&buf[*pos], buflen - *pos, "$%u\r\n%s\r\n$%u\r\n%s\r\n", strlen(field), field, strlen(value), value);

        if (c <= 0)
                return -ENOSPC;

        *tkncnt += 2;
        *pos += c;

        return 0;
}

int __redis_client_xadd_send(const char *redis_key, unsigned keylen, const char *buf_tokens, unsigned buflen, unsigned cnt_tokens)
{
        struct iovec iov[2];
        char hdr[128];
        int hdrlen;
        int n;

        hdrlen = snprintf(hdr, sizeof(hdr),
                        "*%u\r\n"
                        "$4\r\n"
                        "XADD\r\n"
                        "$%u\r\n"
                        "%s\r\n"
                        "$1\r\n"
                        "*\r\n",
                        3 + cnt_tokens,
                        keylen,
                        redis_key);

        iov[0].iov_base = (void *)hdr;
        iov[0].iov_len = hdrlen;
        iov[1].iov_base = (void *)buf_tokens;
        iov[1].iov_len = buflen;

        n = writev(redis_sockfd, iov, 2);
        if (n < 0) {
                xTaskNotifyGive(task_handle_redis_client); // close sockfd
                return n;
        }

        // pr_verbose("n = %d\n", n);

        return 0;
}

int redis_client_xadd_send(const char *redis_key, unsigned keylen, const char *buf_tokens, unsigned buflen, unsigned cnt_tokens)
{
        int err = 0;

        if (redis_sockfd < 0)
                return -ENOENT;

        xSemaphoreTake(lck_redis_client_send, portMAX_DELAY);

        if ((err = __redis_client_xadd_send(redis_key, keylen, buf_tokens, buflen, cnt_tokens)) < 0) {
                goto err;
        }

        if ((err = redis_client_reply_check()) < 0) {
                goto err;
        }

err:
        xSemaphoreGive(lck_redis_client_send);

        return err;
}

int redis_client_xadd_send_async(const char *redis_key, unsigned keylen, const char *buf_tokens, unsigned buflen, unsigned cnt_tokens)
{
        int err = 0;

        if (redis_sockfd < 0)
                return -ENOENT;

        xSemaphoreTake(lck_redis_client_send, portMAX_DELAY);
        err = __redis_client_xadd_send(redis_key, keylen, buf_tokens, buflen, cnt_tokens);
        xSemaphoreGive(lck_redis_client_send);

        return err;
}

int redis_client_cmd_async(char *cmd)
{
        int err = 0;

        if (redis_sockfd < 0)
                return -ENOENT;

        xSemaphoreTake(lck_redis_client_send, portMAX_DELAY);
        err = redis_client_cmd_send(cmd);
        xSemaphoreGive(lck_redis_client_send);

        return err;
}

int redis_client_cmd(char *cmd)
{
        int err;

        if (redis_sockfd < 0)
                return -ENOENT;

        xSemaphoreTake(lck_redis_client_send, portMAX_DELAY);

        if ((err = redis_client_cmd_send(cmd)) < 0) {
                goto err;
        }

        if ((err = redis_client_reply_check()) < 0) {
                goto err;
        }

err:
        xSemaphoreGive(lck_redis_client_send);

        return err;
}

void task_redis_client(void *arg)
{
        struct redis_client_cfg *cfg = (struct redis_client_cfg *)arg;
        int sockfd;

        while (1) {
                if (wifi_mode_get() == ESP_WIFI_MODE_STA) {
                        if (!wifi_sta_connected || !wifi_sta_ip_got) {
                                mdelay((1000));
                                continue;
                        }
                }

                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0) {
                        pr_err("socket(): %d %s\n", errno, strerror(abs(errno)));
                        mdelay((1000));
                        continue;
                }

                struct sockaddr_in server_addr;
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(cfg->port);
                inet_pton(AF_INET, cfg->server, &server_addr.sin_addr.s_addr);

                if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                        pr_info("connect() to %s:%d failure, %d %s\n",
                                cfg->server, cfg->port, errno, strerror(abs(errno)));
                        mdelay((1000));
                        goto sockfree;
                }

                if (cfg->nodelay) {
                        int flag = 1;
                        setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
                }

                {
                        int enable = 1;
                        setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));

                        // Interval before starting keepalive probes (seconds)
                        int idle = 2;      // start sending probes after Xs idle
                        int interval = 1;  // send probes every Xs
                        int count = 4;     // after X failed probes, consider peer dead
                        setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
                        setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
                        setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
                }

                pr_info("connected to redis server %s:%u\n", cfg->server, cfg->port);
                redis_sockfd = sockfd;

                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                pr_info("disconnected\n");

sockfree:
                redis_sockfd = -1;
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);

                mdelay((1000));
        }

        vTaskDelete(NULL);
}

int redis_client_init(struct redis_client_cfg *cfg, unsigned cpu)
{
        if (!cfg->enabled)
                return 0;

        lck_redis_client_send = xSemaphoreCreateMutex();

        xTaskCreatePinnedToCore(task_redis_client, "redis_cli", 4096, cfg, 1, &task_handle_redis_client, cpu);

        return 0;
}

#endif // __LIBJJ_REDIS_CLIENT_H__