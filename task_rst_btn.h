#ifndef __LIBJJ_TASK_RST_BTN_H__
#define __LIBJJ_TASK_RST_BTN_H__

#ifdef GPIO_BTN_RST
static void task_rst_btn(void *arg)
{
        uint32_t ts_pressed = 0;

        pr_info("started\n");

        pinMode(GPIO_BTN_RST, INPUT_PULLUP);

        while (1) {
                ts_pressed = 0;

                while (digitalRead(GPIO_BTN_RST) == LOW) {
                        uint32_t now = esp32_millis();

                        pr_info("reset button pressed\n");

                        if (ts_pressed == 0) {
                                ts_pressed = now;
                                continue;
                        } else if (now - ts_pressed >= 10 * 1000) {
                                while (digitalRead(GPIO_BTN_RST) == LOW) {
#ifdef HAVE_WS2812_LED
                                        led_on(LED_WS2812, 255, 0, 255);
                                        vTaskDelay(pdMS_TO_TICKS(250));
                                        led_off(LED_WS2812);
                                        vTaskDelay(pdMS_TO_TICKS(250));
#else
                                        // TODO: pause other led blink task

                                        led_on(LED_SIMPLE_MAIN, 0, 0, 0);
                                        led_on(LED_SIMPLE_AUX, 0, 0, 0);
                                        vTaskDelay(pdMS_TO_TICKS(250));
                                        led_off(LED_SIMPLE_MAIN);
                                        led_off(LED_SIMPLE_AUX);
                                        vTaskDelay(pdMS_TO_TICKS(250));
#endif // HAVE_WS2812_LED
                                }

                                pr_info("restart now\n");

                                ESP.restart();
                        }

                        vTaskDelay(pdMS_TO_TICKS(500));
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
        }
}

static __unused void task_rst_btn_start(unsigned cpu)
{
        vTaskDelay(pdMS_TO_TICKS(100));
        xTaskCreatePinnedToCore(task_rst_btn, "rst_btn", 4096, NULL, 1, NULL, cpu);
}
#endif // GPIO_BTN_RST

#endif // __LIBJJ_TASK_RST_BTN_H__