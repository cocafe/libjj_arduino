#ifndef __LIBJJ_ESP32_BT_H__
#define __LIBJJ_ESP32_BT_H__

#include <stdint.h>

#include <esp_bt.h>

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

static int ble_is_connected = 0;

static uint64_t cnt_can_ble_send; // send to remote

static uint8_t ble_activity = 0;
static int8_t led_ble_activity = -1;

static void timer_cb_ble_led(void *arg)
{
        if (ble_is_connected) {
                if (ble_activity) {
                        static uint8_t last_on = 0;

                        // blink
                        if (!last_on) {
                                led_on(led_ble_activity, 0, 255, 0);
                                last_on = 1;
                        } else {
                                led_off(led_ble_activity);
                                last_on = 0;
                        }
                } else {
                        // keep on if connected but no activity
                        led_on(led_ble_activity, 0, 0, 255);
                }

                ble_activity = 0;
        } else {
                // beacon blink to notice BLE on and we are powered on
                static uint8_t pattern[] = { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
                static uint8_t i = 0;

                if (pattern[i]) {
                        led_on(led_ble_activity, 0, 255, 0);
                } else {
                        led_off(led_ble_activity);
                }

                i++;
                if (i >= ARRAY_SIZE(pattern))
                        i = 0;
        }
}

static esp_timer_handle_t timer_ble_led;
static const esp_timer_create_args_t timer_args_ble_led = {
        .callback = timer_cb_ble_led,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "led_ble",
};

static __unused int ble_led_timer_start(unsigned tick_ms)
{
#ifdef BLE_LED_BLINK
        led_ble_activity = BLE_LED_BLINK;
#endif

        if (led_ble_activity < 0) {
                pr_err("led gpio not defined\n");
                return -EINVAL;
        }

        if (esp_timer_create(&timer_args_ble_led, &timer_ble_led) != ESP_OK) {
                pr_err("failed to create timer\n");
                return -ENOMEM;
        }

        if (ESP_OK != esp_timer_start_periodic(timer_ble_led, tick_ms * 1000UL)) {
                pr_err("failed to start timer\n");
                return -EFAULT;
        }

        return 0;
}

static void task_ble_conn_broadcast(void *arg)
{
        pr_info("started\n");

        while (1) {
                vTaskDelay(pdMS_TO_TICKS(5 * 1000UL));

                if (ble_is_connected) {
#ifdef __LIBJJ_EVENT_UDP_MC_H__
                        // keep sending to avoid miss
                        event_udp_mc_send(EVENT_RC_BLE_CONNECTED, NULL, 0);
#endif
                } else {
#ifdef __LIBJJ_EVENT_UDP_MC_H__
                        event_udp_mc_send(EVENT_RC_BLE_DISCONNECTED, NULL, 0);
#endif
                }
        }
}

static __unused void task_ble_conn_broadcast_start(unsigned cpu)
{
        xTaskCreatePinnedToCore(task_ble_conn_broadcast, "ble_state", 4096, NULL, 1, NULL, cpu);
}

#endif // __LIBJJ_ESP32_BT_H__