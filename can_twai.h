#ifndef __LIBJJ_CAN_TWAI_H__
#define __LIBJJ_CAN_TWAI_H__

#include <stdint.h>
#include <errno.h>
#include "utils.h"

#include "can.h"

#include <driver/twai.h>

#ifndef CONFIG_HAVE_CAN_TWAI
#define GPIO_TWAI_TX                    (-1)
#define GPIO_TWAI_RX                    (-1)
#endif

struct twai_cfg {
        int8_t pin_tx; // controller tx
        int8_t pin_rx; // controller rx
        uint8_t mode;
        uint8_t baudrate;
#ifdef CAN_TWAI_USE_RINGBUF
        uint8_t rx_cpuid;
        uint16_t rx_rbuf_size;
#endif
        uint16_t rx_qlen;
        uint16_t tx_timedout_ms;
};

enum {
        TWAI_MODE_CFG_NORMAL,
        TWAI_MODE_CFG_SELF_TEST,
        TWAI_MODE_CFG_LISTEN_ONLY,
        TWAI_MODE_CFG_LOOPBACK,
        NUM_TWAI_MODE_CFGS,
};

static const char *str_twai_mode[] = {
        [TWAI_MODE_CFG_NORMAL]          = "NORMAL",
        [TWAI_MODE_CFG_SELF_TEST]       = "SELF_TEST",
        [TWAI_MODE_CFG_LISTEN_ONLY]     = "LISTEN_ONLY",
        [TWAI_MODE_CFG_LOOPBACK]        = "LOOPBACK",
};

#ifdef CONFIG_TWAI_NEW_API
#include "can_twai_node.h"
#else
#include "can_twai_legacy.h"
#endif

#endif // __LIBJJ_CAN_TWAI_H__
