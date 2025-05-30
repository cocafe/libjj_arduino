#ifndef __LIBJJ_MCP2515_CAN_H__
#define __LIBJJ_MCP2515_CAN_H__

#include <stdint.h>
#include "utils.h"

#include <mcp_can.h>
#include "canframe.h"

#ifndef CAN_INT_PIN
#warning CAN_INT_PIN is not defined, use default PIN
#define CAN_INT_PIN                     6
#endif

#ifndef CAN_SS_PIN
#warning CAN_SS_PIN is not defined, use default PIN
#define CAN_SS_PIN                      7
#endif

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

static MCP_CAN CAN0(CAN_SS_PIN);

static uint8_t can_inited = 0;
static uint64_t cnt_can_send;
static uint64_t cnt_can_send_error;
static uint64_t cnt_can_recv;
static uint64_t cnt_can_recv_error;

#ifdef CAN_AUX_LED
static uint8_t can_txrx = 0;

void can_aux_led_flash(void)
{
        static unsigned long ts = millis();

        if (can_txrx) {
                if ((millis() - ts) >= 500) {
                        led_aux_switch();
                        ts = millis();
                }
        } else if ((millis() - ts) >= 1000) {
                if (likely(can_inited))
                        led_aux_on();
                else
                        led_aux_off();
        }
}

static inline void can_aux_led_loop_begin(void)
{
        can_txrx = 0;
}
#else
static inline void can_aux_led_loop_begin(void) { }
static inline void can_aux_led_flash(void) { }
#endif

int can_send(uint32_t can_id, uint8_t len, uint8_t *data)
{
        if (CAN0.sendMsgBuf(can_id, len, data) == CAN_OK) {
                cnt_can_send++;

#ifdef CAN_AUX_LED
                can_txrx = 1;
#endif

                return 0;
        }

        cnt_can_send_error++;

        return -EIO;
}

int can_recv_once(can_frame_t *f, size_t dlen)
{
        if (digitalRead(CAN_INT_PIN) == LOW) {
                uint32_t id;
                uint8_t len;

                if (CAN0.readMsgBuf(&id, &len, f->data) == CAN_OK) {
                        f->id = id;
                        f->len = len;

#ifdef CAN_AUX_LED
                        can_txrx = 1;
#endif

                        cnt_can_recv++;

                        return 0;
                } else {
                        cnt_can_recv_error++;
                        return -EIO;
                }
        }

        return -EAGAIN;
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
        } else {
                printf("MCP2515 init failed\n");
                can_inited = 0;
        }
}

#endif // __LIBJJ_MCP2515_CAN_H__