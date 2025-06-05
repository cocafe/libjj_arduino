#ifndef __LIBJJ_LEDS_H__
#define __LIBJJ_LEDS_H__

#include <Arduino.h>

#include "utils.h"

#ifndef GPIO_LED_MAIN
#warning GPIO_LED_MAIN is not defined
#define GPIO_LED_MAIN                   (-1)
#endif

#ifndef GPIO_LED_AUX
#warning GPIO_LED_AUX is not defined
#define GPIO_LED_AUX                    (-1)
#endif

enum {
        LED_TYPE_INVALID,
        LED_GPIO_SIMPLE,
        LED_WS2812,
        NUM_LED_TYPES,
};

enum {
        LED_SIMPLE_MAIN,
        LED_SIMPLE_AUX,
#ifdef HAVE_WS2812_LED
        LED_RGB_WS2812,
#endif
        NUM_LEDS,
};

struct led_cfg {
        int gpio;
        uint8_t type;
};

struct led_cfg led_cfgs[NUM_LEDS] = {
        [LED_SIMPLE_MAIN]       = { GPIO_LED_MAIN,      LED_GPIO_SIMPLE },
        [LED_SIMPLE_AUX]        = { GPIO_LED_AUX,       LED_GPIO_SIMPLE },
#ifdef HAVE_WS2812_LED
        [LED_RGB_WS2812]        = { GPIO_LED_WS2812,    LED_WS2812      },
#endif
};

static inline void gpio_led_on(int gpio)
{
        digitalWrite(gpio, HIGH);
}

static inline void gpio_led_off(int gpio)
{
        digitalWrite(gpio, LOW);
}

static inline void gpio_led_switch(int gpio)
{
        if (digitalRead(gpio) == HIGH) {
                digitalWrite(gpio, LOW);
        } else {
                digitalWrite(gpio, HIGH);
        }
}

#ifdef HAVE_WS2812_LED

#include <FastLED.h>

#ifndef NUM_LED_WS2812
#warning NUM_LED_WS2812 not defined, use default value 1
#endif

CRGB led_ws2812[NUM_LED_WS2812];
unsigned led_ws2812_brightness = 10;

static void __ws2812_led_on(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
        if (unlikely(idx >= NUM_LED_WS2812))
                return;

        led_ws2812[idx] = CRGB(g, r, b);
        FastLED.show();
}

static void __ws2812_led_off(uint8_t idx)
{
        if (unlikely(idx >= NUM_LED_WS2812))
                return;

        led_ws2812[idx] = CRGB(0, 0, 0);
        FastLED.show();
}

static void ws2812_led_on(uint8_t r, uint8_t g, uint8_t b)
{
        __ws2812_led_on(0, r, g, b);
}

static void ws2812_led_off(void)
{
        __ws2812_led_off(0);
}

static inline void __ws2812_led_flash(uint8_t idx, uint8_t r, uint8_t g, uint8_t b, int intv_ms)
{
        static long last_timestamp = 0;
        static int last_state = LOW;
        long ts = millis();

        if (unlikely(idx >= NUM_LED_WS2812))
                return;

        if (ts - last_timestamp >= intv_ms) {
                if (last_state == LOW) {
                        __ws2812_led_on(idx, r, g, b);
                        last_state = HIGH;
                } else {
                        __ws2812_led_off(idx);
                        last_state = LOW;
                }

                last_timestamp = ts;
        }
}

static __attribute__((unused)) inline void led_ws2812_brightness_set(unsigned level)
{
        FastLED.setBrightness(level > 255 ? 255 : level);
}

#else
static inline void ws2812_led_on(uint8_t r, uint8_t g, uint8_t b) { }
static inline void ws2812_led_off(void) { }
#endif // HAVE_WS2812_LED

static void led_on(unsigned which, uint8_t r, uint8_t g, uint8_t b)
{
        struct led_cfg *led = &led_cfgs[which];

        if (unlikely(which >= ARRAY_SIZE(led_cfgs)))
                return;

        if (unlikely(led->type == LED_TYPE_INVALID))
                return;

        switch (led->type) {
        case LED_GPIO_SIMPLE:
                if (led->gpio >= 0)
                        gpio_led_on(led->gpio);

                break;

        case LED_WS2812:
                ws2812_led_on(r, g, b);
                break;

        default:
                return;
        }
}

static void led_off(unsigned which)
{
        struct led_cfg *led = &led_cfgs[which];

        if (unlikely(which >= ARRAY_SIZE(led_cfgs)))
                return;

        if (unlikely(led->type == LED_TYPE_INVALID))
                return;

        switch (led->type) {
        case LED_GPIO_SIMPLE:
                if (led->gpio >= 0)
                        gpio_led_off(led->gpio);

                break;

        case LED_WS2812:
                ws2812_led_off();
                break;

        default:
                return;
        }
}

static __attribute__((unused)) void led_init(void)
{
        for (int i = 0; i < ARRAY_SIZE(led_cfgs); i++) {
                struct led_cfg *led = &led_cfgs[i];
                if (led->type != LED_GPIO_SIMPLE)
                        continue;

                if (led->gpio < 0)
                        continue;
                
                pinMode(led->gpio, OUTPUT);
                gpio_led_off(led->gpio);
        }

#if defined (HAVE_WS2812_LED) && defined (GPIO_LED_WS2812)
        FastLED.addLeds<WS2812, GPIO_LED_WS2812, GRB>(led_ws2812, NUM_LED_WS2812);
        led_ws2812_brightness_set(led_ws2812_brightness);
#endif
}

#endif // __LIBJJ_LEDS_H__