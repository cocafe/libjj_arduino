#ifndef __LIBJJ_LEDS_H__
#define __LIBJJ_LEDS_H__

#include <Arduino.h>

#include "utils.h"

#ifndef GPIO_LED_MAIN
#define GPIO_LED_MAIN                   (-1)
#endif

#ifndef GPIO_LED_AUX
#define GPIO_LED_AUX                    (-1)
#endif

#ifndef GPIO_LED_FUNC
#define GPIO_LED_FUNC                   (-1)
#endif

#ifndef LED_WS2812_ORDER
#define LED_WS2812_ORDER                RGB
#endif

enum {
        LED_TYPE_INVALID,
        LED_TYPE_GPIO,
        LED_TYPE_WS2812,
        NUM_LED_TYPES,
};

enum {
        LED_SIMPLE_MAIN,
        LED_SIMPLE_AUX,
        LED_SIMPLE_FUNC,
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
        [LED_SIMPLE_MAIN]       = { GPIO_LED_MAIN,      LED_TYPE_GPIO   },
        [LED_SIMPLE_AUX]        = { GPIO_LED_AUX,       LED_TYPE_GPIO   },
        [LED_SIMPLE_FUNC]       = { GPIO_LED_FUNC,      LED_TYPE_GPIO   },
#ifdef HAVE_WS2812_LED
        [LED_RGB_WS2812]        = { GPIO_LED_WS2812,    LED_TYPE_WS2812 },
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

#ifdef LED_USE_FASTLED
#include <FastLED.h>

#ifndef NUM_LED_WS2812
#warning NUM_LED_WS2812 not defined, use default value 1
#define NUM_LED_WS2812 1
#endif

CRGB led_ws2812[NUM_LED_WS2812];
unsigned led_ws2812_brightness = 10;

static void __ws2812_led_on(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
        if (unlikely(idx >= NUM_LED_WS2812))
                return;

        led_ws2812[idx] = CRGB(r, g, b);
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
        long ts = esp32_millis();

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

static __unused inline void led_ws2812_brightness_set(unsigned level)
{
        FastLED.setBrightness(level > 255 ? 255 : level);
}
#endif // LED_USE_FASTLED

#ifdef LED_USE_NEOPIXEL
#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel neopixels(NUM_LED_WS2812, GPIO_LED_WS2812, __PASTE(NEO_, LED_WS2812_ORDER) + NEO_KHZ800);

unsigned led_ws2812_brightness = 10;

static void __ws2812_led_on(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
        if (unlikely(idx >= NUM_LED_WS2812))
                return;

        neopixels.setBrightness(led_ws2812_brightness);
        neopixels.setPixelColor(idx, neopixels.Color(r, g, b));
        neopixels.show();
}

static void __ws2812_led_off(uint8_t idx)
{
        if (unlikely(idx >= NUM_LED_WS2812))
                return;

        neopixels.setBrightness(0);
        // neopixels.setPixelColor(idx, neopixels.Color(0, 0, 0));
        neopixels.show();
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
        long ts = esp32_millis();

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

static inline __unused void led_ws2812_brightness_set(unsigned level)
{
        neopixels.setBrightness(level);
        led_ws2812_brightness = level;
}

static inline __unused uint8_t led_ws2812_brightness_get(void)
{
        led_ws2812_brightness = neopixels.getBrightness();
        return led_ws2812_brightness;
}
#endif // LED_USE_NEOPIXEL

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
        case LED_TYPE_GPIO:
                if (led->gpio >= 0)
                        gpio_led_on(led->gpio);

                break;

        case LED_TYPE_WS2812:
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
        case LED_TYPE_GPIO:
                if (led->gpio >= 0)
                        gpio_led_off(led->gpio);

                break;

        case LED_TYPE_WS2812:
                ws2812_led_off();
                break;

        default:
                return;
        }
}

static __unused void led_init(void)
{
        for (int i = 0; i < ARRAY_SIZE(led_cfgs); i++) {
                struct led_cfg *led = &led_cfgs[i];
                if (led->type != LED_TYPE_GPIO)
                        continue;

                if (led->gpio < 0)
                        continue;

                pinMode(led->gpio, OUTPUT);
                gpio_led_off(led->gpio);
        }

#if defined (HAVE_WS2812_LED) && defined (GPIO_LED_WS2812)
#if defined LED_USE_FASTLED
        FastLED.addLeds<WS2812, GPIO_LED_WS2812, LED_WS2812_ORDER>(led_ws2812, NUM_LED_WS2812);
        led_ws2812_brightness_set(led_ws2812_brightness);
        led_off(LED_RGB_WS2812);
#elif defined LED_USE_NEOPIXEL
        neopixels.begin();
        neopixels.clear();
#endif
#endif
}

struct rgb_led {
        uint8_t active;
        uint8_t rgb[3];
};

struct rgb_led_ctx {
        struct rgb_led *states;
        unsigned states_cnt;
        unsigned brightness;
        unsigned intv_ms_off;
        unsigned intv_ms_on;
        void (*update_cb)(struct rgb_led_ctx *);
};

unsigned rgb_led_suspended = 0;

void rgb_led_active_set(struct rgb_led_ctx *ctx, unsigned idx, int active)
{
        if (idx >= ctx->states_cnt)
                return;
        
        ctx->states[idx].active = !!active;
}

void task_rgb_led(void *arg)
{
        struct rgb_led_ctx *ctx = (struct rgb_led_ctx *)arg;
        struct rgb_led *state;
        unsigned on = 0;
        unsigned idx = 0;
        unsigned brightness = 0;

        if (!ctx->update_cb || !ctx->states || !ctx->states_cnt) {
                vTaskDelete(NULL);
        }

        led_ws2812_brightness_set(5);

        led_on(LED_RGB_WS2812, 255, 255, 255);
        mdelay(1000);
        led_off(LED_RGB_WS2812);

        while (1) {
                if (rgb_led_suspended) {
                        mdelay(1000);
                        continue;
                }

                ctx->update_cb(ctx);

                if (brightness != ctx->brightness) {
                        brightness = ctx->brightness;
                        led_ws2812_brightness_set(brightness);
                }

                state = &ctx->states[idx];

                if (!on) {
                        led_off(LED_RGB_WS2812);
                        mdelay(ctx->intv_ms_off);
                        on = 1;
                } else {
                        unsigned next = idx + 1;
                        unsigned have_active = 0;

                        for (unsigned i = 0; i < ctx->states_cnt; i++) {
                                if (ctx->states[i].active)
                                        have_active++;
                        }

                        if (!have_active) {
                                mdelay(500);
                                continue;
                        }

                        if (ctx->states[idx].active) {
                                led_on(LED_RGB_WS2812, state->rgb[0], state->rgb[1], state->rgb[2]);
                        }

                        mdelay(ctx->intv_ms_on);
                        on = 0;

                        while (1) {
                                if (next >= ctx->states_cnt)
                                        next = 0;

                                if (ctx->states[next].active) {
                                        idx = next;
                                        break;
                                }

                                next++;
                        }
                }
        }
}

static __unused void task_rgb_start(struct rgb_led_ctx *ctx, int cpu)
{
        xTaskCreatePinnedToCore(task_rgb_led, "rgb_led", 2048, ctx, 1, NULL, cpu);
}

#endif // __LIBJJ_LEDS_H__