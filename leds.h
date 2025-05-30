#ifndef __LIBJJ_LEDS_H__
#define __LIBJJ_LEDS_H__

#include <Arduino.h>

#ifndef GPIO_MAIN_LED
#warning GPIO_MAIN_LED is not defined, functions are stub
#endif

#ifndef GPIO_AUX_LED
#warning GPIO_AUX_LED is not defined, functions are stub
#endif

#ifdef GPIO_MAIN_LED
static inline void led_off(void)
{
        digitalWrite(GPIO_MAIN_LED, LOW);
}

static inline void led_on(void)
{
        digitalWrite(GPIO_MAIN_LED, HIGH);
}

static inline void led_switch(void)
{
        if (digitalRead(GPIO_MAIN_LED) == HIGH) {
                led_off();
        } else {
                led_on();
        }
}
#else
static inline void led_off(void) { }
static inline void led_on(void) { }
static inline void led_switch(void) { }
#endif // GPIO_MAIN_LED

#ifdef GPIO_AUX_LED
static inline void led_aux_off(void)
{
        digitalWrite(GPIO_AUX_LED, LOW);
}

static inline void led_aux_on(void)
{
        digitalWrite(GPIO_AUX_LED, HIGH);
}

static inline void led_aux_switch(void)
{
        if (digitalRead(GPIO_AUX_LED) == HIGH) {
                led_aux_off();
        } else {
                led_aux_on();
        }
}

static inline void led_aux_flash(int interval_ms)
{
        static long last_timestamp = 0;
        static int last_state = LOW;
        long ts = millis();

        if (ts - last_timestamp >= interval_ms) {
                if (last_state == LOW) {
                        digitalWrite(GPIO_AUX_LED, HIGH);
                        last_state = HIGH;
                } else {
                        digitalWrite(GPIO_AUX_LED, LOW);
                        last_state = LOW;
                }

                last_timestamp = ts;
        }
}
#else
static inline void led_aux_off(void) { }
static inline void led_aux_on(void) { }
static inline void led_aux_switch(void) { }
static inline void led_aux_flash(int interval_ms) { (void)interval_ms; }
#endif // GPIO_AUX_LED

static void led_init(void)
{
        pinMode(GPIO_MAIN_LED, OUTPUT);
        pinMode(GPIO_AUX_LED, OUTPUT);

        led_aux_off();
        led_off();
}

#endif // __LIBJJ_LEDS_H__