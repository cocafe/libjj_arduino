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
                                        led_on(LED_RGB_WS2812, 255, 0, 255);
                                        mdelay((250));
                                        led_off(LED_RGB_WS2812);
                                        mdelay((250));
#elif (GPIO_LED_FUNC >= 0)
                                        led_on(LED_SIMPLE_FUNC, 255, 255, 255);
                                        mdelay((250));
                                        led_off(LED_SIMPLE_FUNC);
                                        mdelay((250));
#else
                                        mdelay((500));
#endif
                                }

                                pr_info("restart now\n");

                                ESP.restart();
                        }

                        mdelay((500));
                }

                mdelay((1000));
        }
}

static __unused void task_rst_btn_start(unsigned cpu)
{
        mdelay((100));
        xTaskCreatePinnedToCore(task_rst_btn, "rst_btn", 4096, NULL, 1, NULL, cpu);
}
#endif // GPIO_BTN_RST

#endif // __LIBJJ_TASK_RST_BTN_H__