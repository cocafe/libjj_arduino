#ifndef __LIBJJ_CAN_H__
#define __LIBJJ_CAN_H__

#include <stdint.h>

#include "utils.h"
#include "leds.h"
#include "logging.h"
#include "can_frame.h"
#include "can_rlimit.h"

enum {
        CAN_BAUDRATE_4K096BPS,
        CAN_BAUDRATE_5KBPS,
        CAN_BAUDRATE_10KBPS,
        CAN_BAUDRATE_20KBPS,
        CAN_BAUDRATE_25KBPS,
        CAN_BAUDRATE_31K25BPS,
        CAN_BAUDRATE_33K3BPS,
        CAN_BAUDRATE_40KBPS,
        CAN_BAUDRATE_50KBPS,
        CAN_BAUDRATE_80KBPS,
        CAN_BAUDRATE_100KBPS,
        CAN_BAUDRATE_125KBPS,
        CAN_BAUDRATE_200KBPS,
        CAN_BAUDRATE_250KBPS,
        CAN_BAUDRATE_500KBPS,
        CAN_BAUDRATE_800KBPS,
        CAN_BAUDRATE_1000KBPS,
        NUM_CAN_BAUDRATES,
};

static __unused const char *str_can_baudrates[] = {
        [CAN_BAUDRATE_4K096BPS] = "CAN_4K096BPS",
        [CAN_BAUDRATE_5KBPS]    = "CAN_5KBPS",
        [CAN_BAUDRATE_10KBPS]   = "CAN_10KBPS",
        [CAN_BAUDRATE_20KBPS]   = "CAN_20KBPS",
        [CAN_BAUDRATE_25KBPS]   = "CAN_25KBPS",
        [CAN_BAUDRATE_31K25BPS] = "CAN_31K25BPS",
        [CAN_BAUDRATE_33K3BPS]  = "CAN_33K3BPS",
        [CAN_BAUDRATE_40KBPS]   = "CAN_40KBPS",
        [CAN_BAUDRATE_50KBPS]   = "CAN_50KBPS",
        [CAN_BAUDRATE_80KBPS]   = "CAN_80KBPS",
        [CAN_BAUDRATE_100KBPS]  = "CAN_100KBPS",
        [CAN_BAUDRATE_125KBPS]  = "CAN_125KBPS",
        [CAN_BAUDRATE_200KBPS]  = "CAN_200KBPS",
        [CAN_BAUDRATE_250KBPS]  = "CAN_250KBPS",
        [CAN_BAUDRATE_500KBPS]  = "CAN_500KBPS",
        [CAN_BAUDRATE_800KBPS]  = "CAN_800KBPS",
        [CAN_BAUDRATE_1000KBPS] = "CAN_1000KBPS",
};

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
        void (*cb)(can_frame_t *, struct can_rlimit_node *rlimit);
};

static struct can_recv_cb_ctx can_recv_cbs[4] = { };
static uint8_t can_recv_cb_cnt;

static SemaphoreHandle_t lck_can_cb;

static __unused int can_recv_cb_register(void (*cb)(can_frame_t *, struct can_rlimit_node *))
{
        int err = 0;

        xSemaphoreTake(lck_can_cb, portMAX_DELAY);

        if (can_recv_cb_cnt >= ARRAY_SIZE(can_recv_cbs)) {
                err = -ENOSPC;
                goto unlock;
        }

        can_recv_cbs[can_recv_cb_cnt++].cb = cb;

unlock:
        xSemaphoreGive(lck_can_cb);

        return err;
}

static void task_can_recv(void *arg)
{
        static union {
                uint8_t _[sizeof(can_frame_t) + 16];
                can_frame_t f;
        } _ = { };
        static can_frame_t *f = &_.f;

        pr_info("started\n");

        while (1) {
#ifdef CONFIG_HAVE_CAN_RLIMIT
                struct can_rlimit_node *rlimit = NULL;
#endif
                uint8_t aggr = 0;

                while (can_dev->recv(f) == 0) {
#ifdef CONFIG_HAVE_CAN_RLIMIT
                        if (can_rlimit.cfg->enabled) {
                                xSemaphoreTake(can_rlimit.lck, portMAX_DELAY);

                                if (!rlimit || rlimit->can_id != f->id) {
                                        rlimit = can_ratelimit_get(f->id);

                                        if (!rlimit) {
                                                rlimit = __can_ratelimit_add(can_id);
                                        }
                                }
                        } else {
                                rlimit = NULL;
                        }
#endif

#if 0
                        pr_raw("canbus recv: id: 0x%04lx len: %hhu ", f->id, f->dlc);
                        for (uint8_t i = 0; i < f->dlc; i++) {
                                pr_raw("0x%02x ", f->data[i]);
                        }
                        pr_raw("\n");
#endif

                        for (int i = 0; i < ARRAY_SIZE(can_recv_cbs); i++) {
                                if (can_recv_cbs[i].cb) {
                                        can_recv_cbs[i].cb(f,
#ifdef CONFIG_HAVE_CAN_RLIMIT
                                                rlimit
#else
                                                NULL
#endif
                                        );
                                        aggr++;
                                }
                        }

#ifdef CONFIG_HAVE_CAN_RLIMIT
                        if (can_rlimit.cfg->enabled) {
                                xSemaphoreGive(can_rlimit.lck);
                        }
#endif

                        // XXX: to avoid starvation of other tasks
                        // taskYIELD();

                        if (aggr >= 10) {
                                vTaskDelay(pdMS_TO_TICKS(1));
                                aggr = 0;
                        }
                }

                vTaskDelay(pdMS_TO_TICKS(1));
        }
}

#ifdef CAN_LED_BLINK
static uint8_t can_txrx = 0;
static uint8_t can_led = CAN_LED_BLINK;
static uint8_t can_led_blink = 1;

static void task_can_led_blink(void *arg)
{
        while (1) {
                can_txrx = 0;

                vTaskDelay(pdMS_TO_TICKS(500));

                if (!can_led_blink)
                        continue;

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
#endif // CAN_LED_BLINK

static __unused int task_can_start(unsigned task_cpu)
{
        if (can_dev) {
#ifdef CAN_LED_BLINK
                xTaskCreatePinnedToCore(task_can_led_blink, "led_blink_can", 1024, NULL, 1, NULL, task_cpu);
                vTaskDelay(pdMS_TO_TICKS(50));
#endif
                xTaskCreatePinnedToCore(task_can_recv, "can_recv", 4096, NULL, 1, NULL, task_cpu);
                vTaskDelay(pdMS_TO_TICKS(50));
        } else {
                pr_err("no device inited\n");
                return -ENODEV;
        }

        return 0;
}

static __unused void can_init(void)
{
        lck_can_cb = xSemaphoreCreateMutex();
}

#endif // __LIBJJ_CAN_H__