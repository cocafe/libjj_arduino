#ifndef __LIBJJ_MCP2515_CAN_H__
#define __LIBJJ_MCP2515_CAN_H__

#include <stdint.h>

#include <mcp_can.h>

#include "utils.h"
#include "logging.h"
#include "can.h"

#ifndef CONFIG_HAVE_CAN_MCP2515
#define GPIO_MCP2515_INT                (-1)
#endif

#ifndef GPIO_MCP2515_SPI_SS
#error GPIO_MCP2515_SPI_SS is not defined
#endif

struct mcp2515_cfg {
        uint8_t baudrate;
        uint8_t quartz;
        uint8_t mode;
        int8_t  pin_int;
};

static uint8_t mcp2515_pin_int;

enum {
        MCP2515_QUARTZ_20MHZ,
        MCP2515_QUARTZ_16MHZ,
        MCP2515_QUARTZ_8MHZ,
        NUM_MCP2515_QUARTZ_TYPES,
};

enum {
        MCP2515_MODE_NORMAL,
        MCP2515_MODE_SLEEP,
        MCP2515_MODE_LOOPBACK,
        MCP2515_MODE_LISTENONLY,
        NUM_MCP2515_MODES,
};

static int mcp2515_cfg_mode_convert[NUM_MCP2515_MODES] = {
        [MCP2515_MODE_NORMAL]     = MCP_NORMAL,
        [MCP2515_MODE_SLEEP]      = MCP_SLEEP,
        [MCP2515_MODE_LOOPBACK]   = MCP_LOOPBACK,
        [MCP2515_MODE_LISTENONLY] = MCP_LISTENONLY,
};

static int mcp2515_cfg_quartz_convert[NUM_MCP2515_QUARTZ_TYPES] = {
        [MCP2515_QUARTZ_20MHZ] = MCP_20MHZ,
        [MCP2515_QUARTZ_16MHZ] = MCP_16MHZ,
        [MCP2515_QUARTZ_8MHZ]  = MCP_8MHZ,
};

static int mcp2515_cfg_baudrate_convert[NUM_CAN_BAUDRATES] = {
        [CAN_BAUDRATE_4K096BPS] = CAN_4K096BPS,
        [CAN_BAUDRATE_5KBPS]    = CAN_5KBPS,
        [CAN_BAUDRATE_10KBPS]   = CAN_10KBPS,
        [CAN_BAUDRATE_20KBPS]   = CAN_20KBPS,
        [CAN_BAUDRATE_25KBPS]   = -1,
        [CAN_BAUDRATE_31K25BPS] = CAN_31K25BPS,
        [CAN_BAUDRATE_33K3BPS]  = CAN_33K3BPS,
        [CAN_BAUDRATE_40KBPS]   = CAN_40KBPS,
        [CAN_BAUDRATE_50KBPS]   = CAN_50KBPS,
        [CAN_BAUDRATE_80KBPS]   = CAN_80KBPS,
        [CAN_BAUDRATE_100KBPS]  = CAN_100KBPS,
        [CAN_BAUDRATE_125KBPS]  = CAN_125KBPS,
        [CAN_BAUDRATE_200KBPS]  = CAN_200KBPS,
        [CAN_BAUDRATE_250KBPS]  = CAN_250KBPS,
        [CAN_BAUDRATE_500KBPS]  = CAN_500KBPS,
        [CAN_BAUDRATE_800KBPS]  = -1,
        [CAN_BAUDRATE_1000KBPS] = CAN_1000KBPS,
};

static const char *str_mcp_quartz[] = {
        [MCP2515_QUARTZ_20MHZ] = "MCP_QUARTZ_20MHZ",
        [MCP2515_QUARTZ_16MHZ] = "MCP_QUARTZ_16MHZ",
        [MCP2515_QUARTZ_8MHZ]  = "MCP_QUARTZ_8MHZ",
};

static const char *str_mcp_mode[] = {
        [MCP2515_MODE_NORMAL]     = "MCP_MODE_NORMAL",
        [MCP2515_MODE_SLEEP]      = "MCP_MODE_SLEEP",
        [MCP2515_MODE_LOOPBACK]   = "MCP_MODE_LOOPBACK",
        [MCP2515_MODE_LISTENONLY] = "MCP_MODE_LISTENONLY",
};

static MCP_CAN CAN0(GPIO_MCP2515_SPI_SS);

static int mcp2515_send(uint32_t can_id, uint8_t len, uint8_t *data)
{
        if (CAN0.sendMsgBuf(can_id, len, data) == CAN_OK) {
                cnt_can_send++;

#ifdef CAN_LED_BLINK
                can_txrx = 1;
#endif

                return 0;
        }

        cnt_can_send_error++;

        return -EIO;
}

static int mcp2515_recv_once(can_frame_t *f)
{
        if (digitalRead(mcp2515_pin_int) == LOW) {
                uint32_t id;
                uint8_t len;

                if (CAN0.readMsgBuf(&id, &len, f->data) == CAN_OK) {
                        f->id = id;
                        f->dlc = len;

#ifdef CAN_LED_BLINK
                        can_txrx = 1;
#endif

                        cnt_can_recv++;

                        return 0;
                } else {
                        cnt_can_recv_error++;
                        return -EIO;
                }
        } else {
                mdelay((1));
        }

        return -EAGAIN;
}

static can_device_t can_mcp2515 = {
        .send = mcp2515_send,
        .recv = mcp2515_recv_once,
};

static int mcp2515_init(struct mcp2515_cfg *cfg)
{
        int baudrate, quartz, mode;

        if (!cfg)
                return -ENODATA;

        if (cfg->pin_int < 0) {
                pr_info("invalid pin config\n");
                return -ENODEV;
        }

        if (cfg->baudrate >= ARRAY_SIZE(mcp2515_cfg_baudrate_convert) ||
                cfg->quartz >= ARRAY_SIZE(mcp2515_cfg_quartz_convert) ||
                cfg->mode >= ARRAY_SIZE(mcp2515_cfg_mode_convert)) {
                pr_info("invalid config\n");
                return -EINVAL;
        }

        pr_info("baudrate: %s\n", str_can_baudrates[cfg->baudrate]);
        pr_info("quartz: %s\n", str_mcp_quartz[cfg->quartz]);
        pr_info("mode: %s\n", str_mcp_mode[cfg->mode]);

        baudrate = mcp2515_cfg_baudrate_convert[cfg->baudrate];
        quartz = mcp2515_cfg_quartz_convert[cfg->quartz];
        mode = mcp2515_cfg_mode_convert[cfg->mode];

        if (baudrate == -1) {
                pr_info("unsupported baudrate\n");
                return -EINVAL;
        }

        if (CAN0.begin(MCP_ANY, baudrate, quartz) != CAN_OK) {
                return -EIO;
        }

        CAN0.setMode(mode);
        mcp2515_pin_int = (uint8_t)cfg->pin_int;
        pinMode(cfg->pin_int, INPUT);

        return 0;
}

#endif // __LIBJJ_MCP2515_CAN_H__