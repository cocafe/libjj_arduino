#ifndef __LIBJJ_CAN_TWAI_NODE_H__
#define __LIBJJ_CAN_TWAI_NODE_H__

#include <stdint.h>
#include <errno.h>
#include "utils.h"

#include "can.h"
#include "ffs.h"

#include <esp_twai.h>
#include <esp_twai_onchip.h>

#define TWAI_RX_SLOT_PAYLOAD_SZ 64

union twai_rx_slot {
        can_frame_t f;
        uint8_t raw[sizeof(can_frame_t) + TWAI_RX_SLOT_PAYLOAD_SZ];
};

static uint16_t tx_timedout_ms = 200;
static twai_node_handle_t twai_node = NULL;
static QueueHandle_t twai_rxq = NULL;

const char *str_twai_states[] = {
        "error_active",
        "error_warning",
        "error_passive",
        "bus_off"
};
static uint64_t cnt_twai_bus_error = 0;
static uint64_t cnt_twai_rxq_full = 0;
static int twai_node_state = 0;

static int TWAI_send(uint32_t can_id, uint8_t len, uint8_t *data)
{
        twai_frame_t msg = { };

        if (unlikely(len > TWAI_FRAME_MAX_DLC)) {
                return -EINVAL;
        }

        msg.header.id = can_id;
        msg.header.ide = false; // not using 29-bit id
        msg.buffer = data;
        msg.buffer_len = len;

        // -1 ms to wait forever
        if (twai_node_transmit(twai_node, &msg, tx_timedout_ms) != ESP_OK) {
                cnt_can_send_error++;
                return -EIO;
        }

        cnt_can_send++;

#ifdef CAN_LED_BLINK
        can_txrx = 1;
#endif

        return 0;
}

static TaskHandle_t task_twai_rxq;
static void task_twai_rxq_worker(void *arg)
{
        static union twai_rx_slot rx; 

        while (1) {
                BaseType_t ret = xQueueReceive(twai_rxq, &rx, portMAX_DELAY);

                if (ret != pdTRUE) {
                        continue;
                }

                can_recv_one(&rx.f);
        }
}

static bool IRAM_ATTR TWAI_recv_cb(twai_node_handle_t handle,
                                   const twai_rx_done_event_data_t *edata,
                                   void *user_ctx)
{
        static union twai_rx_slot rx = { };
        can_frame_t *f = &rx.f;
        twai_frame_t rx_frame = { 
                .buffer = f->data,
                .buffer_len = TWAI_RX_SLOT_PAYLOAD_SZ,
        };
        BaseType_t woken = pdFALSE;

        if (ESP_OK == twai_node_receive_from_isr(handle, &rx_frame)) {
                if (rx_frame.header.rtr) {
                        cnt_can_recv_rtr++;
                        return false;
                }

                f->id = rx_frame.header.id;
                f->dlc = twaifd_dlc2len(rx_frame.header.dlc);

                if (pdPASS != xQueueSendFromISR(twai_rxq, &rx, &woken))
                        cnt_twai_rxq_full++;

                cnt_can_recv++;
        } else {
                cnt_can_recv_error++;
        }

        return (woken == pdTRUE); 
}

static bool IRAM_ATTR twai_listener_on_error_callback(twai_node_handle_t handle,
                                                      const twai_error_event_data_t *edata,
                                                      void *user_ctx)
{
        pr_err("bus error: 0x%x\n", edata->err_flags.val);
        cnt_twai_bus_error++;

        return false;
}

static bool IRAM_ATTR twai_listener_on_state_change_callback(twai_node_handle_t handle,
                                                             const twai_state_change_event_data_t *edata,
                                                             void *user_ctx)
{
        twai_node_state = edata->new_sta;

        if (edata->old_sta < ARRAY_SIZE(str_twai_states) || 
                edata->new_sta < ARRAY_SIZE(str_twai_states)) {
                pr_info("state changed: %s -> %s\n",
                        str_twai_states[edata->old_sta],
                        str_twai_states[edata->new_sta]);
        }        
        
        return false;
}

static can_device_t can_twai = {
        .send = TWAI_send,
        .recv = NULL,
};

int TWAI_init(struct twai_cfg *cfg)
{
        if (cfg->pin_rx < 0 || cfg->pin_tx < 0)
                return -EINVAL;

        if (cfg->mode >= ARRAY_SIZE(str_twai_mode)) {
                pr_info("invalid mode\n");
                return -EINVAL;
        }

        twai_onchip_node_config_t twai_cfg = {};

        twai_cfg.io_cfg.tx = (gpio_num_t)cfg->pin_tx;
        twai_cfg.io_cfg.rx = (gpio_num_t)cfg->pin_rx;

        if (cfg->baudrate <= NUM_CAN_BAUDRATES)
                twai_cfg.bit_timing.bitrate = can_baudrates_vals[cfg->baudrate];

        twai_cfg.tx_queue_depth = 5;

        pr_info("baudrate: %s\n", str_can_baudrates[cfg->baudrate]);
        pr_info("mode: %s\n", str_twai_mode[cfg->mode]);
        pr_info("tx_timedout_ms: %d\n", cfg->tx_timedout_ms);

        switch (cfg->mode) {
        case TWAI_MODE_CFG_LISTEN_ONLY:
                twai_cfg.flags.enable_listen_only = 1;
                break;

        case TWAI_MODE_CFG_LOOPBACK:
                twai_cfg.flags.enable_loopback = 1;
                break;

        case TWAI_MODE_CFG_SELF_TEST:
                twai_cfg.flags.enable_self_test = 1;
                break;

        default:
        case TWAI_MODE_CFG_NORMAL:
                break;
        }

        twai_rxq = xQueueCreate(cfg->rx_qlen, sizeof(union twai_rx_slot));
        if (!twai_rxq) {
                pr_err("failed to allocate twai_rxq, content bytes: %zu\n", cfg->rx_qlen * sizeof(union twai_rx_slot));
                return -ENOMEM;
        }
        pr_info("rxq allocate %zu bytes\n", cfg->rx_qlen * sizeof(union twai_rx_slot));

        if (twai_new_node_onchip(&twai_cfg, &twai_node) != ESP_OK) {
                pr_err("twai_new_node_onchip()\n");
                return -EIO;
        }

        xTaskCreatePinnedToCore(task_twai_rxq_worker, "twai_rxq", 4096, NULL, 1, &task_twai_rxq, CPU1);

        twai_event_callbacks_t user_cbs = {
                .on_rx_done = TWAI_recv_cb,
        };
        twai_node_register_event_callbacks(twai_node, &user_cbs, NULL);

        if (twai_node_enable(twai_node) != ESP_OK) {
                pr_err("twai_node_enable()\n");
                return -EIO;
        }

        tx_timedout_ms = cfg->tx_timedout_ms;

        return 0;
}

int TWAI_exit(void)
{
        if (!twai_node)
                return -ENODEV;
        
        if (twai_node_disable(twai_node) != ESP_OK)
                return -EIO;
        
        if (twai_node_delete(twai_node) != ESP_OK)
                return -EIO;
        
        twai_node = NULL;

        return 0;
}

#endif // __LIBJJ_CAN_TWAI_NODE_H__
