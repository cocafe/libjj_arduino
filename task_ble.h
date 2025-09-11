#ifndef __TASK_BLE_H__
#define __TASK_BLE_H__

static unsigned ble_gw_connected = 0;
static unsigned ble_event_received = 0;
static TaskHandle_t handle_task_ble_conn_timer = NULL;

static void task_ble_conn_timer(void *arg)
{
        pr_info("started\n");

        while (1) {
                ble_event_received = 0;
                vTaskDelay(15 * 1000);

                if (!ble_event_received) {
                        if (ble_gw_connected) {
                                pr_info("did not receive ble event over time, peer may down, disconnect now\n");
                                ble_gw_connected = 0;
                        }

                        break;
                }
        }

        handle_task_ble_conn_timer = NULL;
        pr_info("stopped\n");

        vTaskDelete(NULL);
}

static void event_udp_mc_cb(uint8_t event, void *data, unsigned dlen)
{
        ble_event_received = 1;

        switch (event) {
        case EVENT_RC_BLE_CONNECTED:
                if (!ble_gw_connected) {
                        pr_info("ble connected\n");
                        ble_gw_connected = 1;

                        if (handle_task_ble_conn_timer == NULL) {
                                xTaskCreatePinnedToCore(task_ble_conn_timer, "ble_timer", 4096, NULL, 1, &handle_task_ble_conn_timer, CPU1);
                        }
                }

                break;

        case EVENT_RC_BLE_DISCONNECTED:
                if (ble_gw_connected) {
                        pr_info("ble disconnected\n");

                        if (handle_task_ble_conn_timer != NULL) {
                                vTaskDelete(handle_task_ble_conn_timer);
                                handle_task_ble_conn_timer = NULL;
                        }

                        ble_gw_connected = 0;
                }

                break;

        default:
                break;
        }
}

#endif // __TASK_BLE_H__