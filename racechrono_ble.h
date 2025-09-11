#ifndef __LIBJJ_RACECHRONO_BLE_H__
#define __LIBJJ_RACECHRONO_BLE_H__

#include <stdint.h>

#include <esp_bt.h>
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

enum {
        BLE_TXPWR_MINUS_24DBM,
        BLE_TXPWR_MINUS_21DBM,
        BLE_TXPWR_MINUS_18DBM,
        BLE_TXPWR_MINUS_15DBM,
        BLE_TXPWR_MINUS_12DBM,
        BLE_TXPWR_MINUS_9DBM,
        BLE_TXPWR_MINUS_6DBM,
        BLE_TXPWR_MINUS_3DBM,
        BLE_TXPWR_0DBM,
        BLE_TXPWR_3DBM,
        BLE_TXPWR_6DBM,
        BLE_TXPWR_9DBM,         // ESP DEFAULT
        BLE_TXPWR_12DBM,
        BLE_TXPWR_15DBM,
        BLE_TXPWR_18DBM,
        BLE_TXPWR_20DBM,
        NUM_BLE_TXPWR_LEVELS,
};

static const char *str_ble_txpwr[] = {
        [BLE_TXPWR_MINUS_24DBM] = "-24dBm",
        [BLE_TXPWR_MINUS_21DBM] = "-21dBm",
        [BLE_TXPWR_MINUS_18DBM] = "-18dBm",
        [BLE_TXPWR_MINUS_15DBM] = "-15dBm",
        [BLE_TXPWR_MINUS_12DBM] = "-12dBm",
        [BLE_TXPWR_MINUS_9DBM] = "-9dBm",
        [BLE_TXPWR_MINUS_6DBM] = "-6dBm",
        [BLE_TXPWR_MINUS_3DBM] = "-3dBm",
        [BLE_TXPWR_0DBM] = "0dBm",
        [BLE_TXPWR_3DBM] = "3dBm",
        [BLE_TXPWR_6DBM] = "6dBm",
        [BLE_TXPWR_9DBM] = "9dBm",
        [BLE_TXPWR_12DBM] = "12dBm",
        [BLE_TXPWR_15DBM] = "15dBm",
        [BLE_TXPWR_18DBM] = "18dBm",
        [BLE_TXPWR_20DBM] = "20dBm",
};

static const esp_power_level_t ble_txpwr_to_esp_value[NUM_BLE_TXPWR_LEVELS] = {
#ifdef CONFIG_IDF_TARGET_ESP32C6
        [BLE_TXPWR_MINUS_24DBM] = ESP_PWR_LVL_N15,
        [BLE_TXPWR_MINUS_21DBM] = ESP_PWR_LVL_N15,
        [BLE_TXPWR_MINUS_18DBM] = ESP_PWR_LVL_N15,
#else
        [BLE_TXPWR_MINUS_24DBM] = ESP_PWR_LVL_N24,
        [BLE_TXPWR_MINUS_21DBM] = ESP_PWR_LVL_N21,
        [BLE_TXPWR_MINUS_18DBM] = ESP_PWR_LVL_N18,
#endif
        [BLE_TXPWR_MINUS_15DBM] = ESP_PWR_LVL_N15,
        [BLE_TXPWR_MINUS_12DBM] = ESP_PWR_LVL_N12,
        [BLE_TXPWR_MINUS_9DBM] = ESP_PWR_LVL_N9,
        [BLE_TXPWR_MINUS_6DBM] = ESP_PWR_LVL_N6,
        [BLE_TXPWR_MINUS_3DBM] = ESP_PWR_LVL_N3,
        [BLE_TXPWR_0DBM] = ESP_PWR_LVL_N0,
        [BLE_TXPWR_3DBM] = ESP_PWR_LVL_P3,
        [BLE_TXPWR_6DBM] = ESP_PWR_LVL_P6,
        [BLE_TXPWR_9DBM] = ESP_PWR_LVL_P9,
        [BLE_TXPWR_12DBM] = ESP_PWR_LVL_P12,
        [BLE_TXPWR_15DBM] = ESP_PWR_LVL_P15,
        [BLE_TXPWR_18DBM] = ESP_PWR_LVL_P18,
        [BLE_TXPWR_20DBM] = ESP_PWR_LVL_P20,
};

static const int ble_txpwr_from_esp_value[NUM_BLE_TXPWR_LEVELS] = {
#ifdef CONFIG_IDF_TARGET_ESP32C6
        [0] = BLE_TXPWR_MINUS_24DBM,
        [1] = BLE_TXPWR_MINUS_21DBM,
        [2] = BLE_TXPWR_MINUS_18DBM,
#else
        [ESP_PWR_LVL_N24] = BLE_TXPWR_MINUS_24DBM,
        [ESP_PWR_LVL_N21] = BLE_TXPWR_MINUS_21DBM,
        [ESP_PWR_LVL_N18] = BLE_TXPWR_MINUS_18DBM,
#endif
        [ESP_PWR_LVL_N15] = BLE_TXPWR_MINUS_15DBM,
        [ESP_PWR_LVL_N12] = BLE_TXPWR_MINUS_12DBM,
        [ESP_PWR_LVL_N9] = BLE_TXPWR_MINUS_9DBM,
        [ESP_PWR_LVL_N6] = BLE_TXPWR_MINUS_6DBM,
        [ESP_PWR_LVL_N3] = BLE_TXPWR_MINUS_3DBM,
        [ESP_PWR_LVL_N0] = BLE_TXPWR_0DBM,
        [ESP_PWR_LVL_P3] = BLE_TXPWR_3DBM,
        [ESP_PWR_LVL_P6] = BLE_TXPWR_6DBM,
        [ESP_PWR_LVL_P9] = BLE_TXPWR_9DBM,
        [ESP_PWR_LVL_P12] = BLE_TXPWR_12DBM,
        [ESP_PWR_LVL_P15] = BLE_TXPWR_15DBM,
        [ESP_PWR_LVL_P18] = BLE_TXPWR_18DBM,
        [ESP_PWR_LVL_P20] = BLE_TXPWR_20DBM,
};

static const char *str_ble_pwr_types[ESP_BLE_PWR_TYPE_NUM] = {
        [ESP_BLE_PWR_TYPE_CONN_HDL0] = "conn0",
        [ESP_BLE_PWR_TYPE_CONN_HDL1] = "conn1",
        [ESP_BLE_PWR_TYPE_CONN_HDL2] = "conn2",
        [ESP_BLE_PWR_TYPE_CONN_HDL3] = "conn3",
        [ESP_BLE_PWR_TYPE_CONN_HDL4] = "conn4",
        [ESP_BLE_PWR_TYPE_CONN_HDL5] = "conn5",
        [ESP_BLE_PWR_TYPE_CONN_HDL6] = "conn6",
        [ESP_BLE_PWR_TYPE_CONN_HDL7] = "conn7",
        [ESP_BLE_PWR_TYPE_CONN_HDL8] = "conn8",
        [ESP_BLE_PWR_TYPE_ADV] = "adv",
        [ESP_BLE_PWR_TYPE_SCAN] = "scan",
        [ESP_BLE_PWR_TYPE_DEFAULT] = "default",
};

struct ble_cfg {
        char devname[16];
        uint8_t enabled;
        uint8_t update_hz;
        uint8_t tx_power;
};

static int ble_is_connected = 0;
static char ble_device_prefix[32] = "RaceChrono";
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
        static char dev_name[64];

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

        char *name = ble_device_name_generate();

        pr_info("devname: %s\n", name);

        RaceChronoBle.setUp(name, &raceChronoHandler);
        RaceChronoBle.startAdvertising();

        if (cfg->tx_power < ARRAY_SIZE(ble_txpwr_to_esp_value)) {
                if (ESP_OK != esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ble_txpwr_to_esp_value[cfg->tx_power]))
                        pr_err("esp_ble_tx_power_set() failed\n");
        }

        if (can_dev) {
                can_recv_cb_register(can_ble_frame_send);
        }
}

#endif // __LIBJJ_RACECHRONO_BLE_H__