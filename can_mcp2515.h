#ifndef __LIBJJ_MCP2515_CAN_H__
#define __LIBJJ_MCP2515_CAN_H__

#include <stdint.h>

#include <mcp_can.h>

#include "utils.h"
#include "logging.h"
#include "can.h"

#ifndef GPIO_MCP2515_SPI_SS
#error GPIO_MCP2515_SPI_SS is not defined
#endif

struct mcp2515_cfg {
        uint8_t baudrate;
        uint8_t clkrate;
        uint8_t mode;
        int8_t  pin_int;
};

static uint8_t mcp2515_pin_int;

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
                vTaskDelay(pdMS_TO_TICKS(1));
        }

        return -EAGAIN;
}

static can_device_t can_mcp2515 = {
        .send = mcp2515_send,
        .recv = mcp2515_recv_once,
};

static int mcp2515_init(struct mcp2515_cfg *cfg)
{
        int found = 0;

        if (!cfg)
                return -ENODATA;

        if (cfg->pin_int < 0) {
                pr_info("invalid pin config\n");
                return -ENODEV;
        }

        for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_baudrate); i++) {
                if (cfg->baudrate == cfg_mcp_baudrate[i].val) {
                        pr_info("baudrate: %s\n", cfg_mcp_baudrate[i].str);
                        found++;
                        break;
                }
        }

        for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_clkrate); i++) {
                if (cfg->clkrate == cfg_mcp_clkrate[i].val) {
                        pr_info("clockrate: %s\n", cfg_mcp_clkrate[i].str);
                        found++;
                        break;
                }
        }

        for (size_t i = 0; i < ARRAY_SIZE(cfg_mcp_mode); i++) {
                if (cfg->mode == cfg_mcp_mode[i].val) {
                        pr_info("mode: %s\n", cfg_mcp_mode[i].str);
                        found++;
                        break;
                }
        }

        if (found != 3)
                return -EINVAL;

        if (CAN0.begin(MCP_ANY, cfg->baudrate, cfg->clkrate) != CAN_OK) {
                return -EIO;
        }

        CAN0.setMode(cfg->mode);
        mcp2515_pin_int = (uint8_t)cfg->pin_int;
        pinMode(cfg->pin_int, INPUT);

        return 0;
}

#endif // __LIBJJ_MCP2515_CAN_H__