#ifndef __LIBJJ_LEDS_H__
#define __LIBJJ_LEDS_H__

#include <Arduino.h>

static inline void led_off()
{
        digitalWrite(LED_BUILTIN, LOW);
}

static inline void led_on()
{
        digitalWrite(LED_BUILTIN, HIGH);
}

static inline void led_aux_off()
{
        digitalWrite(LED_BUILTIN_AUX, LOW);
}

static inline void led_aux_on()
{
        digitalWrite(LED_BUILTIN_AUX, HIGH);
}

static inline void led_aux_switch()
{
        if (digitalRead(LED_BUILTIN_AUX) == HIGH) {
                led_aux_off();
        } else {
                led_aux_on();
        }
}

static void led_init(void)
{
        pinMode(LED_BUILTIN, OUTPUT);
        pinMode(LED_BUILTIN_AUX, OUTPUT);

        led_aux_off();
        led_off();
}

static inline void led_aux_flash(int interval_ms)
{
        static long last_timestamp = 0;
        static int last_state = LOW;
        long ts = millis();

        if (ts - last_timestamp >= interval_ms) {
                if (last_state == LOW) {
                        digitalWrite(LED_BUILTIN_AUX, HIGH);
                        last_state = HIGH;
                } else {
                        digitalWrite(LED_BUILTIN_AUX, LOW);
                        last_state = LOW;
                }

                last_timestamp = ts;
        }
}

#endif // __LIBJJ_LEDS_H__