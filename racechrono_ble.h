#ifndef __LIBJJ_RACECHRONO_BLE_H__
#define __LIBJJ_RACECHRONO_BLE_H__

#include <stdint.h>

#include <RaceChrono.h>

#include "logging.h"
#include "esp32_utils.h"
#include "can.h"

#ifndef BLE_DEFAULT_MAC
#define BLE_DEFAULT_MAC                 ESP_MAC_EFUSE_FACTORY
#endif

#ifndef BLE_DEFAULT_UPDATE_RATE_HZ
#define BLE_DEFAULT_UPDATE_RATE_HZ      10
#endif

#define BLE_UPDATE_HZ_TO_MS(hz)         (((hz) == 0) ? 0 : (1000 / (hz)))

struct ble_cfg {
        char devname[16];
        uint8_t enabled;
        uint8_t update_hz;
};

static int ble_is_connected = 0;
static char ble_device_prefix[16] = "RaceChrono";
static uint8_t racechrono_ble_update_rate_hz = BLE_DEFAULT_UPDATE_RATE_HZ;

static uint64_t cnt_can_ble_send; // send to remote

#ifdef CAN_BLE_LED_BLINK
static uint8_t can_ble_txrx = 0;
static uint8_t can_ble_led = CAN_BLE_LED_BLINK;
static uint8_t can_ble_led_blink = 1;
#endif // CAN_BLE_LED_BLINK

static SemaphoreHandle_t lck_ble_send;

using PidExtra = struct
{
        uint32_t update_intv_ms = BLE_UPDATE_HZ_TO_MS(racechrono_ble_update_rate_hz);
        uint32_t ts_last_send = 0;
};
RaceChronoPidMap<PidExtra> pidMap;

class PrintRaceChronoCommands : public RaceChronoBleCanHandler
{
public:
        void allowAllPids(uint16_t updateIntervalMs)
        {
                pr_info("allow all PIDs by remote\n");
                pidMap.allowAllPids(updateIntervalMs);
        }

        void denyAllPids()
        {
                pr_info("deny all PIDs by remote\n");
                pidMap.reset();
        }

        void allowPid(uint32_t pid, uint16_t updateIntervalMs)
        {
                if (pidMap.allowOnePid(pid, updateIntervalMs)) {
                        void *entry = pidMap.getEntryId(pid);
                        PidExtra *pidExtra = pidMap.getExtra(entry);
                        pidExtra->ts_last_send = 0;
                        pidExtra->update_intv_ms = BLE_UPDATE_HZ_TO_MS(racechrono_ble_update_rate_hz);
                        if (racechrono_ble_update_rate_hz != 0)
                                pr_info("pid: 0x%03lx update interval %u Hz\n", pid, racechrono_ble_update_rate_hz);
                        else
                                pr_info("pid: 0x%03lx unlimited update interval\n", pid);
                } else {
                        pr_err("unable to handle this request\n");
                }
        }

        void handleDisconnect()
        {
                if (!pidMap.isEmpty() || pidMap.areAllPidsAllowed()) {
                        pr_info("reset pid maps\n");
                        this->denyAllPids();
                }
        }
} raceChronoHandler;

static void blc_rc_pidmap_for_each(void (*cb)(PidExtra *))
{
        if (!cb)
                return;

        void *entry = pidMap.getFirstEntryId();

        while (entry != nullptr)
        {
                PidExtra *extra = pidMap.getExtra(entry);

                if (extra)
                        cb(extra);

                entry = pidMap.getNextEntryId(entry);
        }
}

static char *ble_device_name_generate(void)
{
        uint8_t mac[6];
        static char dev_name[32];

        if (esp32_mac_get(BLE_DEFAULT_MAC, mac)) {
                pr_err("failed to get BT mac address\n");
                sprintf(dev_name, "%s ??:??:??", ble_device_prefix);
        } else {
                sprintf(dev_name, "%s %x:%x:%x", ble_device_prefix, mac[3], mac[4], mac[5]);
        }

        return dev_name;
}

static void task_ble_conn_update(void *arg)
{
        pr_info("started\n");

        while (1) {
                if (RaceChronoBle.isConnected()) {
                        if (!ble_is_connected) {
                                pr_info("BLE connected\n");
                                ble_is_connected = 1;
                        }

#ifdef __LIBJJ_EVENT_UDP_MC_H__
                        // keep sending to avoid miss
                        event_udp_mc_send(EVENT_RC_BLE_CONNECTED, NULL, 0);
#endif
                } else {
                        if (ble_is_connected) {
                                pr_info("BLE disconnected\n");
                                ble_is_connected = 0;
                                raceChronoHandler.handleDisconnect();
                        }

#ifdef __LIBJJ_EVENT_UDP_MC_H__
                        event_udp_mc_send(EVENT_RC_BLE_DISCONNECTED, NULL, 0);
#endif
                }

                vTaskDelay(pdMS_TO_TICKS(5 * 1000UL));
        }
}

#ifdef CAN_BLE_LED_BLINK
static void task_can_ble_led_blink(void *arg)
{
        while (1) {
                if (!can_ble_led_blink) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        continue;
                }

                if (ble_is_connected) {
                        can_ble_txrx = 0;

                        vTaskDelay(pdMS_TO_TICKS(500));

                        if (can_ble_txrx) {
                                static uint8_t last_on = 0;

                                // blink
                                if (!last_on) {
                                        led_on(can_ble_led, 0, 255, 0);
                                        last_on = 1;
                                } else {
                                        led_off(can_ble_led);
                                        last_on = 0;
                                }
                        } else {
                                // keep on if connected but no activity
                                led_on(can_ble_led, 0, 0, 255);
                        }
                } else {
                        // beacon blink to notice BLE on and we are powered on
                        static uint8_t pattern[] = { 1, 0, 0, 0, 0, 0 };
                        static uint8_t i = 0;

                        if (pattern[i]) {
                                led_on(can_ble_led, 0, 255, 0);
                        } else {
                                led_off(can_ble_led);
                        }

                        i++;
                        if (i >= ARRAY_SIZE(pattern))
                                i = 0;

                        vTaskDelay(pdMS_TO_TICKS(1000));
                }
        }
}
#endif // CAN_BLE_LED_BLINK

static __unused void task_ble_conn_start(unsigned cpu)
{
        xTaskCreatePinnedToCore(task_ble_conn_update, "ble_conn", 4096, NULL, 1, NULL, cpu);
#ifdef CAN_BLE_LED_BLINK
        xTaskCreatePinnedToCore(task_can_ble_led_blink, "led_blink_ble", 1024, NULL, 1, NULL, cpu);
#endif
}

static void can_ble_frame_send(can_frame_t *f)
{
        if (!RaceChronoBle.isConnected())
                return;

        void *entry = pidMap.getEntryId(f->id);

        if (entry == NULL)
                return;

        PidExtra *extra = pidMap.getExtra(entry);
        uint32_t now = esp32_millis();
        if ((now - extra->ts_last_send) >= extra->update_intv_ms) {
                xSemaphoreTake(lck_ble_send, portMAX_DELAY);
                RaceChronoBle.sendCanData(f->id, f->data, f->dlc);
                xSemaphoreGive(lck_ble_send);

                extra->ts_last_send = now;
                cnt_can_ble_send++;

#ifdef CAN_BLE_LED_BLINK
                can_ble_txrx = 1;
#endif
        }
}

static void __unused ble_init(struct ble_cfg *cfg)
{
        lck_ble_send = xSemaphoreCreateMutex();

        if (cfg) {
                strncpy(ble_device_prefix, cfg->devname, sizeof(ble_device_prefix));
                racechrono_ble_update_rate_hz = cfg->update_hz;
        }

        RaceChronoBle.setUp(ble_device_name_generate(), &raceChronoHandler);
        RaceChronoBle.startAdvertising();

        if (can_dev) {
                can_recv_cb_register(can_ble_frame_send);
        }
}

#endif // __LIBJJ_RACECHRONO_BLE_H__