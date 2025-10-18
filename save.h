#ifndef __LIBJJ_SAVE_H__
#define __LIBJJ_SAVE_H__

#include <stdio.h>
#include <stdint.h>

#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <nvs_flash.h>

#include <CRC32.h>

#include "utils.h"
#include "logging.h"
#include "jkey.h"
#include "json.h"

// MUST start with '/'
#ifndef CONFIG_FACTORY_JSON_PATH
#define CONFIG_FACTORY_JSON_PATH                ("/factory.json")
#endif

#ifndef CONFIG_CFG_JSON_PATH
#define CONFIG_CFG_JSON_PATH                    ("/config.json")
#endif

static int (*jbuf_cfg_make)(jbuf_t *, struct config *);

static int save_fwhash_read(char *hash, size_t cnt)
{
        const esp_partition_t *running_partition = esp_ota_get_running_partition();
        const esp_partition_pos_t partition_position = {
                running_partition->address,
                running_partition->size
        };
        esp_image_metadata_t data;

        if (esp_image_verify(ESP_IMAGE_VERIFY, &partition_position, &data) != ESP_OK) {
                pr_info("failed to verify firmware image.\n");
                return -EIO;
        }
        
        pr_raw("firmware SHA-256: ");
        for (int i = 0, c = 0; i < 32; ++i) {
                pr_raw("%02x", data.image_digest[i]);

                if (hash)
                        c += snprintf(&hash[c], cnt, "%02x", data.image_digest[i]);
        }
        pr_raw("\n");

        return 0;
}

static int config_json_load(struct config *cfg, const char *json_path)
{
        jbuf_t jb = { };
        int err;

        if (unlikely(!jbuf_cfg_make))
                return -ENOTSUP;

        if ((err = jbuf_cfg_make(&jb, cfg)))
                return err;

        // migration for strval changes
        jkey_invalid_strval_ignore_set(1);
        err = jbuf_json_file_load(&jb, json_path);
        jkey_invalid_strval_ignore_set(0);

        jbuf_deinit(&jb);

        return err;
}

static int config_json_save(struct config *cfg, const char *json_path)
{
        jbuf_t jb = { };
        int err;

        if (unlikely(!jbuf_cfg_make))
                return -ENOTSUP;

        if ((err = jbuf_cfg_make(&jb, cfg)))
                return err;

        err = jbuf_json_file_save(&jb, json_path);

        jbuf_deinit(&jb);

        return err;
}

static int config_json_delete(const char *json_path)
{
        return spiffs_file_delete(json_path);
}

static int save_read(struct config *cfg)
{
        return config_json_load(cfg, CONFIG_CFG_JSON_PATH);
}

static int save_write(struct config *cfg)
{
        return config_json_save(cfg, CONFIG_CFG_JSON_PATH);
}

static void save_init(int (*jbuf_maker)(jbuf_t *, struct config *))
{
        uint8_t f = json_print_on_load;

        if (!jbuf_maker) {
                pr_err("to use save, must define constructor for json\n");
                return;
        }

        jbuf_cfg_make = jbuf_maker;

        save_fwhash_read(NULL, 0);

        json_print_on_load = 1;
        if (save_read(&g_cfg) != 0) {
                pr_info("failed to read config json from flash, create new one\n");

                memcpy(&g_cfg, &g_cfg_default, sizeof(g_cfg));

                if (!config_json_load(&g_cfg, CONFIG_FACTORY_JSON_PATH)) {
                        pr_info("factory json config found, config merged with it\n");

                        if (!config_json_save(&g_cfg, CONFIG_FACTORY_JSON_PATH))
                                pr_info("factory json updated\n");
                }

                save_write(&g_cfg);
        }
        json_print_on_load = f;
}

static int nvs_erase(void)
{
        esp_err_t err = nvs_flash_erase();

        if (err != ESP_OK) {
                pr_err("NVS erase failed: %d\n", err);
                return -EIO;
        }

        pr_info("NVS erased successfully\n");
        nvs_flash_init();

        return 0;
}

static void save_cfg_reset(void)
{
        memcpy(&g_cfg, &g_cfg_default, sizeof(g_cfg));

        // use factory config if found
        if (!config_json_load(&g_cfg, CONFIG_FACTORY_JSON_PATH)) {
                pr_info("factory json config found, config merged with it\n");
        }

        save_write(&g_cfg);
}

static void save_cfg_default_reset(void)
{
        memcpy(&g_cfg, &g_cfg_default, sizeof(g_cfg));
        save_write(&g_cfg);
}

static __unused void save_reset_gpio_check(unsigned gpio_rst)
{
        static uint32_t ts_pressed = 0;

        pinMode(gpio_rst, INPUT_PULLUP);

        while (digitalRead(gpio_rst) == LOW) {
                uint32_t now = esp32_millis();

                pr_info("config reset button pressed\n");

                if (ts_pressed == 0) {
                        ts_pressed = now;
                        continue;
                } else if (now - ts_pressed >= 3000) {
                        save_cfg_reset();

                        pr_info("config reset performed\n");

                        while (digitalRead(gpio_rst) == LOW) {
                                now = esp32_millis();

#ifdef HAVE_WS2812_LED
                                led_on(LED_WS2812, 255, 255, 0);
                                delay(250);
                                led_off(LED_WS2812);
                                delay(250);
#else
                                led_on(LED_SIMPLE_MAIN, 0, 0, 0);
                                led_on(LED_SIMPLE_AUX, 0, 0, 0);
                                delay(250);
                                led_off(LED_SIMPLE_MAIN);
                                led_off(LED_SIMPLE_AUX);
                                delay(250);
#endif // HAVE_WS2812_LED

                                if (now - ts_pressed >= 30 * 1000) {
                                        // delete factory.json
                                        config_json_delete(CONFIG_FACTORY_JSON_PATH);

                                        // use program default config
                                        save_cfg_default_reset();

                                        nvs_erase();

                                        while (digitalRead(gpio_rst) == LOW) {
#ifdef HAVE_WS2812_LED
                                                led_on(LED_WS2812, 255, 255, 255);
                                                delay(250);
                                                led_off(LED_WS2812);
                                                delay(250);
#else
                                                led_on(LED_SIMPLE_MAIN, 0, 0, 0);
                                                led_on(LED_SIMPLE_AUX, 0, 0, 0);
                                                delay(125);
                                                led_off(LED_SIMPLE_MAIN);
                                                led_off(LED_SIMPLE_AUX);
                                                delay(125);
#endif // HAVE_WS2812_LED
                                        }
                                }
                        }

                        ESP.restart();
                }

                delay(500);
        }
}

#endif // __LIBJJ_SAVE_H__