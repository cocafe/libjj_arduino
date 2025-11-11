#ifndef __LIBJJ_TASK_RC_STATS_H__
#define __LIBJJ_TASK_RC_STATS_H__

struct can_frame_rc_stats {
        uint16_t ble_send_hz;
        uint8_t ble_per_pid;
        uint8_t esp32_tempC;
        uint16_t uptime_sec;
        uint8_t reserved[2];
} __attribute__((packed));

static void rc_stats_frame_construct(can_frame_t *f, unsigned can_id)
{
        struct can_frame_rc_stats *d = (struct can_frame_rc_stats *)f->data;
        static uint32_t ts = 0;
        uint32_t now = esp32_millis();

        f->magic = htole32(CAN_DATA_MAGIC);
        f->id = htole32(can_id);
        f->dlc = 8;

        d->ble_send_hz = htole16(rc_hz_overall);
        d->ble_per_pid = g_cfg.ble_cfg.update_hz;
        d->uptime_sec = htole16(now / 1000);

        if (now - ts >= 10000) {
                float tempC = 0.0;

                if (esp32_tsens_get(&tempC)) {
                        d->esp32_tempC = 0xff;
                } else {
                        d->esp32_tempC = (unsigned)tempC + 40;
                }

                ts = now;
        }
}

static void task_rc_stats_data(void *arg)
{
        static can_frame_raw8_t raw = { };
        can_frame_t *f = &raw.f;
        intptr_t can_id = (intptr_t)arg;

        if (can_id == 0)
                vTaskDelete(NULL);

        while (1) {
                if (!ble_is_connected)
                        goto sleep;

                rc_stats_frame_construct(f, can_id);
                rc_ble_can_frame_send_ratelimited(f);

sleep:
                vTaskDelay(pdMS_TO_TICKS(1000));
        }
}

#endif // __LIBJJ_TASK_RC_STATS_H__