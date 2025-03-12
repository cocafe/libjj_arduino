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

#endif // __LIBJJ_LEDS_H__