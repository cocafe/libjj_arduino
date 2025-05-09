#ifndef __LIBJJ_MCP2515_TCP_H__
#define __LIBJJ_MCP2515_TCP_H__

#include <stdint.h>
#include <endian.h>
#include "utils.h"

#include <mcp_can.h>
#include <WebServer.h>

#define CAN_DATA_MAGIC                  (0xC0CAFEEE)

#ifndef CAN_INT_PIN
#warning CAN_INT_PIN is not defined, use default PIN
#define CAN_INT_PIN                     6
#endif

#ifndef CAN_SS_PIN
#warning CAN_SS_PIN is not defined, use default PIN
#define CAN_SS_PIN                      7
#endif

#ifndef CAN_SERVER_PORT
#warning CAN_SERVER_PORT is not defined, use default port number
#define CAN_SERVER_PORT                 35000
#endif

static WiFiServer can_server(CAN_SERVER_PORT);

struct strval cfg_mcp_baudrate[] = {
        { "CAN_4K096BPS",       CAN_4K096BPS    },
        { "CAN_5KBPS",          CAN_5KBPS       },
        { "CAN_10KBPS",         CAN_10KBPS      },
        { "CAN_20KBPS",         CAN_20KBPS      },
        { "CAN_31K25BPS",       CAN_31K25BPS    },
        { "CAN_33K3BPS",        CAN_33K3BPS     },
        { "CAN_40KBPS",         CAN_40KBPS      },
        { "CAN_50KBPS",         CAN_50KBPS      },
        { "CAN_80KBPS",         CAN_80KBPS      },
        { "CAN_100KBPS",        CAN_100KBPS     },
        { "CAN_125KBPS",        CAN_125KBPS     },
        { "CAN_200KBPS",        CAN_200KBPS     },
        { "CAN_250KBPS",        CAN_250KBPS     },
        { "CAN_500KBPS",        CAN_500KBPS     },
        { "CAN_1000KBPS",       CAN_1000KBPS    },
};

struct strval cfg_mcp_clkrate[] = { 
        { "MCP_20MHZ",          MCP_20MHZ       },
        { "MCP_16MHZ",          MCP_16MHZ       },
        { "MCP_8MHZ",           MCP_8MHZ        },
};

struct strval cfg_mcp_mode[] = {
        { "MCP_NORMAL",         MCP_NORMAL      },
        { "MCP_SLEEP",          MCP_SLEEP       },
        { "MCP_LOOPBACK",       MCP_LOOPBACK    },
        { "MCP_LISTENONLY",     MCP_LISTENONLY  },
};

struct can_frame {
        __le32 magic;
        __le32 id;
        uint8_t len;
        uint8_t heartbeat;
        uint8_t padding[2];
        uint8_t data[];
} __attribute__((packed));

static MCP_CAN CAN0(CAN_SS_PIN);

static uint8_t can_inited = 0;

static uint64_t cnt_can_send;
static uint64_t cnt_can_send_error;
static uint64_t cnt_can_recv;
static uint64_t cnt_can_recv_error;
static uint64_t cnt_can_tcp_recv_error;
#ifdef CAN_AUX_LED
static uint8_t can_txrx = 0;
#endif

static int can_frame_input(struct can_frame *f)
{
        uint32_t id = le32toh(f->id);
        uint32_t dlen = f->len;

        if (CAN0.sendMsgBuf(id, dlen, f->data) == CAN_OK) {
                cnt_can_send++;
                #if 0
                printf("canbus send: id: 0x%04lx len: %lu ", id, dlen);
                for (uint8_t i = 0; i < dlen; i++) {
                        printf("0x%02x ", f->data[i]);
                }
                printf("\n");
                #endif

#ifdef CAN_AUX_LED
                can_txrx = 1;
#endif
        } else {
                cnt_can_send_error++;
        }

        return 0;
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
                        // printf("%s(): invalid packet\n", __func__);
                        cnt_can_tcp_recv_error++;
                        return -EINVAL;
                }

                pos = (intptr_t)ret - (intptr_t)buf;
        }

        while (pos < len) {
                struct can_frame *f = (struct can_frame *)&buf[pos];
                uint32_t dlen = f->len;
                uint32_t remain = len - pos;

                if (le32toh(f->magic) != CAN_DATA_MAGIC) {
                        // printf("%s(): invalid magic\n", __func__);
                        cnt_can_tcp_recv_error++;
                        return -EINVAL;
                }

                if (sizeof(struct can_frame) > remain ||
                    dlen + sizeof(struct can_frame) > remain) {
                        break;
                }

                if (f->heartbeat)
                        goto next_frame;

                can_frame_input(f);

next_frame:
                pos += dlen + sizeof(struct can_frame);
        }

        return pos; // consumed
}


static void can_loop(void)
{
        WiFiClient client = can_server.accept();

        if (client) {
                static unsigned long ts = millis();

                printf("new client connected\n");
                led_aux_on();

                while (client.connected()) {
#ifdef CAN_AUX_LED
                        can_txrx = 0;
#endif

                        do {
                                if (digitalRead(CAN_INT_PIN) == LOW) {
                                        static union {
                                                uint8_t _[sizeof(struct can_frame) + 16];
                                                struct can_frame f;
                                        } _ = { };
                                        static struct can_frame *d = &_.f;
                                        uint32_t id;
                                        uint8_t len;

                                        if (CAN0.readMsgBuf(&id, &len, d->data) == CAN_OK) {
                                                cnt_can_recv++;

                                                d->magic = htole32(CAN_DATA_MAGIC);
                                                d->id = htole32(id);
                                                d->len = len;

                                                #if 0
                                                printf("canbus recv: id: 0x%04lx len: %hhu ", id, len);
                                                for (uint8_t i = 0; i < len; i++) {
                                                        printf("0x%02x ", d->data[i]);
                                                }
                                                printf("\n");
                                                #endif

                                                client.write((uint8_t *)d, (size_t)(sizeof(struct can_frame) + len));

#ifdef CAN_AUX_LED
                                                can_txrx = 1;
#endif
                                        } else {
                                                cnt_can_recv_error++;
                                        }
                                }
                        } while (0);

                        // XXX: SEND TEST
                        // {
                        //         static uint8_t _candata[280] = { };
                        //         static struct can_frame *d = (struct can_frame *)_candata;
                        //         static uint32_t cached = 0;
                        //         d->magic = htole32(CAN_DATA_MAGIC);
                        //         d->id = htole32(0x666);
                        //         d->len = 8;
                        //         client.write((uint8_t *)d, (size_t)(sizeof(struct can_frame) + d->len));

                        //         cached += sizeof(struct can_frame) + d->len;
                        //         if (cached >= 512) {
                        //                 client.flush();
                        //                 cached = 0;
                        //         }
                        // }

                        do {
                                if (client.available()) {
                                        static uint8_t buf[80] = { };
                                        static int pos = 0;
                                        int n, c;

                                        n = client.read(&buf[pos], sizeof(buf) - pos);

                                        if (n <= 0) {
                                                printf("client.read(): failed to recv from client\n");
                                                cnt_can_tcp_recv_error++;
                                                pos = 0;

                                                break;
                                        }

                                        pos += n;

                                        c = can_frames_input(buf, pos);
                                        if (c > 0) {
                                                if (c < pos) {
                                                        memmove(&buf[0], &buf[c], pos - c);
                                                        pos -= c;
                                                } else {
                                                        pos = 0;
                                                }
                                        }
                                }
                        } while (0);

#ifdef CAN_AUX_LED
                        if (can_txrx) {
                                if ((millis() - ts) >= 500) {
                                        led_aux_switch();
                                        ts = millis();
                                }
                        } else if ((millis() - ts) >= 1000) {
                                led_aux_on();
                        }
#endif
                }

                client.stop();
                printf("client disconnected\n");
#ifdef CAN_AUX_LED
                led_aux_off();
#endif
        }
}

static void can_init(uint8_t mcp_baudrate, uint8_t mcp_clkrate, uint8_t mcp_mode)
{
        for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_baudrate); i++) {
                if (mcp_baudrate == cfg_mcp_baudrate[i].val) {
                        printf("MCP2515 baudrate: %s\n", cfg_mcp_baudrate[i].str);
                        break;
                }
        }

        for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_clkrate); i++) {
                if (mcp_clkrate == cfg_mcp_clkrate[i].val) {
                        printf("MCP2515 clockrate: %s\n", cfg_mcp_clkrate[i].str);
                        break;
                }
        }

        for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_mode); i++) {
                if (mcp_mode == cfg_mcp_mode[i].val) {
                        printf("MCP2515 mode: %s\n", cfg_mcp_mode[i].str);
                        break;
                }
        }

        if (CAN0.begin(MCP_ANY, mcp_baudrate, mcp_clkrate) == CAN_OK) {
                printf("MCP2515 init ok\n");
                can_inited = 1;

                CAN0.setMode(mcp_mode);
                pinMode(CAN_INT_PIN, INPUT);
                can_server.begin();
        } else {
                printf("MCP2515 init failed\n");
                can_inited = 0;
        }
}

#endif // __LIBJJ_MCP2515_TCP_H__