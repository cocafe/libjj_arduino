#ifndef __LIBJJ_CAN_H__
#define __LIBJJ_CAN_H__

#include <stdint.h>

#include "leds.h"
#include "logging.h"
#include "can_frame.h"

struct can_device {
        int (*send)(uint32_t can_id, uint8_t dlc, uint8_t *data);
        int (*recv)(can_frame_t *out);
};

typedef struct can_device can_device_t;

static can_device_t *can_dev;

static uint64_t cnt_can_send;
static uint64_t cnt_can_send_error;
static uint64_t cnt_can_recv;
static uint64_t cnt_can_recv_error;
static uint64_t cnt_can_recv_rtr;
#ifdef CAN_TWAI_USE_RINGBUF
static uint64_t cnt_can_recv_drop;
#endif // CAN_TWAI_USE_RINGBUF

struct can_recv_cb_ctx {
        void (*cb)(can_frame_t *);
};

static struct can_recv_cb_ctx can_recv_cbs[4] = { };
static uint8_t can_recv_cb_cnt;

// FIXME: can_recv_cb_cnt can be racing
#define can_recv_cb_add(_cb)                                    \
do {                                                            \
        if (can_recv_cb_cnt < ARRAY_SIZE(can_recv_cbs)) {       \
                can_recv_cbs[can_recv_cb_cnt++].cb = _cb;       \
        }                                                       \
} while (0)

static void task_can_recv(void *arg)
{
        static union {
                uint8_t _[sizeof(can_frame_t) + 16];
                can_frame_t f;
        } _ = { };
        static can_frame_t *f = &_.f;

        pr_info("started\n");

        while (1) {
                if (can_dev->recv(f) == 0) {
                        #if 0
                        printf("canbus recv: id: 0x%04lx len: %hhu ", id, len);
                        for (uint8_t i = 0; i < len; i++) {
                                printf("0x%02x ", f->data[i]);
                        }
                        printf("\n");
                        #endif

                        for (int i = 0; i < ARRAY_SIZE(can_recv_cbs); i++) {
                                if (can_recv_cbs[i].cb)
                                        can_recv_cbs[i].cb(f);
                        }
                }

                // XXX: to avoid starvation of other tasks
                taskYIELD();
        }
}

static __attribute__((unused)) void task_can_recv_start(unsigned cpu)
{
        if (can_dev) {
                xTaskCreatePinnedToCore(task_can_recv, "can_recv", 4096, NULL, 1, NULL, cpu);
        }
}

#ifdef CAN_LED_BLINK
static uint8_t can_txrx = 0;
static uint8_t can_led = CAN_LED_BLINK;

static void task_can_led_blink(void *arg)
{
        while (1) {
                can_txrx = 0;

                vTaskDelay(pdMS_TO_TICKS(500));

                if (can_txrx) {
                        static uint8_t last_on = 0;

                        // blink
                        if (!last_on) {
                                led_on(can_led, 0, 255, 0);
                                last_on = 1;
                        } else {
                                led_off(can_led);
                                last_on = 0;
                        }
                } else {
                        if (likely(can_dev))
                                led_on(can_led, 255, 0, 0);
                        else
                                led_off(can_led);
                }
        }
}

static __attribute__((unused)) void task_can_led_blink_start(unsigned cpu)
{
        xTaskCreatePinnedToCore(task_can_led_blink, "led_blink_can", 1024, NULL, 1, NULL, cpu);
}
#else
static __attribute__((unused)) inline void task_can_led_blink_start(unsigned cpu) { }
#endif // CAN_LED_BLINK

#endif // __LIBJJ_CAN_H__