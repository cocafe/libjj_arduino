#ifndef __LIBJJ_CAN_TWAI_H__
#define __LIBJJ_CAN_TWAI_H__

#include <stdint.h>
#include <errno.h>
#include "utils.h"

#include "can.h"

#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>

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

static const char *str_twai_mode[] = {
        [TWAI_MODE_NORMAL]      = "TWAI_NORMAL",
        [TWAI_MODE_NO_ACK]      = "TWAI_NO_ACK",
        [TWAI_MODE_LISTEN_ONLY] = "TWAI_LISTEN_ONLY",
};

static uint16_t tx_timedout_ms = 200;

#ifdef CAN_TWAI_USE_RINGBUF
static RingbufHandle_t twai_rx_ringbuf;
#endif // CAN_TWAI_USE_RINGBUF

static int TWAI_send(uint32_t can_id, uint8_t len, uint8_t *data)
{
        twai_message_t msg = { };

        if (unlikely(len > TWAI_FRAME_MAX_DLC)) {
                return -EINVAL;
        }

        msg.identifier = can_id;
        msg.extd = 0;
        msg.rtr = 0;
        msg.data_length_code = len;
        memcpy(msg.data, data, len);

        if (twai_transmit(&msg, pdMS_TO_TICKS(tx_timedout_ms)) != ESP_OK) {
                cnt_can_send_error++;
                return -EIO;
        }

        cnt_can_send++;

#ifdef CAN_LED_BLINK
        can_txrx = 1;
#endif

        return 0;
}

static int __attribute__((unused)) TWAI_recv_once(can_frame_t *f, int timedout_ms)
{
        twai_message_t msg = { };

        if (timedout_ms < 0)
                timedout_ms = portMAX_DELAY;
        else
                timedout_ms = pdMS_TO_TICKS(timedout_ms);

        // this function blocks until timedout
        if (twai_receive(&msg, timedout_ms) != ESP_OK) {
                cnt_can_recv_error++;
                return -EIO;
        }

        if (unlikely(msg.data_length_code == 0)) {
                cnt_can_recv_error++;
                return -EINVAL;
        }

        if (unlikely(msg.rtr)) {
                cnt_can_recv_rtr++;
                return -ENOTSUP;
        }

        f->id = msg.identifier;
        f->dlc = msg.data_length_code;
        for (unsigned i = 0; i < TWAI_FRAME_MAX_DLC; i++)
                f->data[i] = msg.data[i];

        cnt_can_recv++;

#ifdef CAN_LED_BLINK
        can_txrx = 1;
#endif

        return 0;
}

static int __attribute__((unused)) TWAI_recv(can_frame_t *f)
{
        return TWAI_recv_once(f, -1);
}

#ifdef CAN_TWAI_USE_RINGBUF
static void task_TWAI_rx(void *arg)
{
        static twai_message_t msg = { };

        while (1) {
                if (twai_receive(&msg, portMAX_DELAY) != ESP_OK)
                        continue;

                if (unlikely(msg.data_length_code == 0)) {
                        cnt_can_recv_error++;
                        continue;
                }

                if (unlikely(msg.rtr)) {
                        cnt_can_recv_rtr++;
                        continue;
                }

                // FIXME: drain on new connection??
                if (!(xRingbufferSend(twai_rx_ringbuf, &msg, sizeof(msg), 0))) {
                        cnt_can_recv_drop++;
                        continue;
                }

                cnt_can_recv++;

#ifdef CAN_LED_BLINK
                can_txrx = 1;
#endif
        }
}

// get frame from rx ringbuf
static int TWAI_recv_ring(can_frame_t *f)
{
        size_t msgsz = 0;
        twai_message_t *msg = (twai_message_t *)xRingbufferReceive(twai_rx_ringbuf, &msgsz, pdMS_TO_TICKS(10));

        if (!msg)
                return -ENODATA;

        f->id = msg->identifier;
        f->dlc = msg->data_length_code;
        for (unsigned i = 0; i < TWAI_FRAME_MAX_DLC; i++)
                f->data[i] = msg->data[i];

        vRingbufferReturnItem(twai_rx_ringbuf, (void *)msg);

        return 0;
}
#endif

static can_device_t can_twai = {
        .send = TWAI_send,
#ifdef CAN_TWAI_USE_RINGBUF
        .recv = TWAI_recv_ring,
#else
        .recv = TWAI_recv,
#endif // CAN_TWAI_USE_RINGBUF
};

int TWAI_init(struct twai_cfg *cfg)
{
        if (cfg->pin_rx < 0 || cfg->pin_tx < 0)
                return -EINVAL;

        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)cfg->pin_tx, 
                                                                     (gpio_num_t)cfg->pin_rx,
                                                                     (twai_mode_t)cfg->mode);
        g_config.rx_queue_len = cfg->rx_qlen;
        twai_timing_config_t t_config = { };
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        switch (cfg->baudrate) {
        case CAN_BAUDRATE_25KBPS:
                t_config = TWAI_TIMING_CONFIG_25KBITS();
                break;

        case CAN_BAUDRATE_50KBPS:
                t_config = TWAI_TIMING_CONFIG_50KBITS();
                break;

        case CAN_BAUDRATE_100KBPS:
                t_config = TWAI_TIMING_CONFIG_100KBITS();
                break;

        case CAN_BAUDRATE_125KBPS:
                t_config = TWAI_TIMING_CONFIG_125KBITS();
                break;

        case CAN_BAUDRATE_250KBPS:
                t_config = TWAI_TIMING_CONFIG_250KBITS();
                break;

        case CAN_BAUDRATE_500KBPS:
                t_config = TWAI_TIMING_CONFIG_500KBITS();
                break;

        case CAN_BAUDRATE_800KBPS:
                t_config = TWAI_TIMING_CONFIG_800KBITS();
                break;

        case CAN_BAUDRATE_1000KBPS:
                t_config = TWAI_TIMING_CONFIG_1MBITS();
                break;

        default:
                return -EINVAL;
        }

        twai_driver_install(&g_config, &t_config, &f_config);

        if (twai_start() != ESP_OK) {
                return -EIO;
        }

#ifdef CAN_TWAI_USE_RINGBUF
        twai_rx_ringbuf = xRingbufferCreate(cfg->rx_rbuf_size, RINGBUF_TYPE_NOSPLIT);
        if (twai_rx_ringbuf == NULL) {
                return -ENOMEM;
        }

        xTaskCreatePinnedToCore(task_TWAI_rx, "twai_rx", 4096, NULL, 5, NULL, cfg->rx_cpuid);
#endif // CAN_TWAI_USE_RINGBUF

        tx_timedout_ms = cfg->tx_timedout_ms;

        return 0;
}

int TWAI_exit(void)
{
        if (twai_stop() != ESP_OK)
                return -EIO;

        if (twai_driver_uninstall() != ESP_OK)
                return -EFAULT;

#ifdef CAN_TWAI_USE_RINGBUF
        vRingbufferDelete(twai_rx_ringbuf);
#endif // CAN_TWAI_USE_RINGBUF

        return 0;
}

#endif // __LIBJJ_CAN_TWAI_H__
