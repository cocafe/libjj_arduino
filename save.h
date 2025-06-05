#ifndef __LIBJJ_SAVE_H__
#define __LIBJJ_SAVE_H__

#include <stdio.h>
#include <stdint.h>

#include <esp_ota_ops.h>
#include <esp_image_format.h>

#include <EEPROM.h>
#include <CRC32.h>

#include "utils.h"
#include "logging.h"

#define SAVE_MAGIC                      (0x00C0CAFE)
#define SAVE_PAGE                       (0)

struct save {
        uint32_t        magic;
        union {
                struct {
                        uint8_t ignore_fwhash   : 1;
                        uint8_t reserved        : 7;
                };
                uint8_t flags;
        };
        uint8_t         fwhash[8];
        uint16_t        save_sz;
        struct config   cfg;
        uint32_t        crc32;
};

static struct save g_save;
static uint8_t current_fw_hash[32] = { };

static int save_fw_hash_verify(void)
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
        
        printf("firmware SHA-256: ");
        for (int i = 0; i < 32; ++i) {
                printf("%02X", data.image_digest[i]);
                current_fw_hash[i] = data.image_digest[i];
        }
        printf("\n");

        return 0;
}

static uint32_t save_crc32(struct save *save)
{
        size_t cnt = sizeof(struct save) - sizeof(typeof(g_save.crc32));
        uint8_t *data = (uint8_t *)save;
        CRC32 crc;

        crc.add((uint8_t *)save, cnt);
        crc.restart();
        for (size_t i = 0; i < cnt; i++) {
                crc.add(data[i]);
        }

        return crc.calc();
}

static int save_verify(struct save *save)
{
        uint32_t crc32;

        if (save->magic != SAVE_MAGIC)
                return -ENODATA;

        if (!save->ignore_fwhash) {
                for (int i = 0; i < ARRAY_SIZE(save->fwhash); i++) {
                        if (save->fwhash[i] != current_fw_hash[i]) {
                                pr_info("firmware hash mismatched\n");
                                return -EBADF;
                        }
                }
        }

        if (sizeof(struct save) != save->save_sz) {
                pr_info("save size mismatched\n");
                return -EBADF;
        }

        crc32 = save_crc32(save);

        pr_info("calc crc32: 0x%08lx\n", crc32);
        pr_info("save crc32: 0x%08lx\n", save->crc32);

        if (crc32 != save->crc32)
                return -EINVAL;

        return 0;
}

static int __save_read(size_t addr, struct save *save)
{
        uint8_t buf[sizeof(struct save)] = { 0 };
        struct save *s = (struct save *)&buf[0];
        int err;

        for (size_t i = 0; i < sizeof(struct save); i++) {
                buf[i] = EEPROM.read(addr + i);
        }

#ifdef SAVE_HEXDUMP
        hexdump(buf, sizeof(buf));
#endif

        if ((err = save_verify(s)))
                return err; 

        memcpy(save, buf, sizeof(struct save));

        return 0;
}

static int save_read(struct save *save)
{
        int err;

        if ((err = __save_read(SAVE_PAGE, save))) {
                pr_info("failed to read save, err = %d\n", err);
                return err;
        }

        pr_info("read save ok\n");
        return 0;
}

static void __save_write(size_t addr, struct save *save)
{
        uint8_t *buf = (uint8_t *)save;

        for (size_t i = 0; i < sizeof(struct save); i++) {
                EEPROM.write(addr + i, buf[i]);
        }

        EEPROM.commit();
}

static void save_write(struct save *save)
{
        save->magic = SAVE_MAGIC;
        
        for (int i = 0; i < ARRAY_SIZE(save->fwhash); i++) {
                save->fwhash[i] = save->ignore_fwhash ? 0x00 : current_fw_hash[i];
        }

        save->save_sz = sizeof(struct save);

        save->crc32 = save_crc32(save);

        pr_info("save crc32: 0x%08lx\n", save->crc32);

#ifdef SAVE_HEXDUMP
        hexdump(save, sizeof(struct save));
#endif

        __save_write(SAVE_PAGE, save);

        pr_info("saved to flash\n");
}

static void save_update(struct save *save, struct config *cfg)
{
        if (cfg)
                memcpy(&save->cfg, cfg, sizeof(save->cfg));
}

static void save_create(struct save *save, struct config *cfg)
{
        memset(save, 0x00, sizeof(struct save));

        save->magic = SAVE_MAGIC;

        memcpy(&save->cfg, cfg, sizeof(save->cfg));
}

static void save_init(void)
{
        uint32_t eeprom_size = ROUND_UP_POWER2(sizeof(struct save));

        EEPROM.begin(eeprom_size);

        save_fw_hash_verify();

        pr_info("sizeof save: %u, eeprom size: %lu\n", sizeof(struct save), eeprom_size);

        if (save_read(&g_save) != 0) {
                pr_info("failed to read save from flash, create new one\n");
                save_create(&g_save, &g_cfg_default);
                save_write(&g_save);
        }

        memcpy(&g_cfg, &g_save.cfg, sizeof(g_cfg));
}

static __attribute__((unused)) void save_reset_gpio_check(unsigned gpio_rst)
{
        static uint32_t ts_pressed = 0;

        while (digitalRead(gpio_rst) == LOW) {
                uint32_t now = millis();

                pr_info("config reset button pressed\n");

                if (ts_pressed == 0) {
                        ts_pressed = now;
                        continue;
                } else if (now - ts_pressed >= 3000) {
                        memcpy(&g_cfg, &g_cfg_default, sizeof(g_cfg));
                        save_update(&g_save, &g_cfg);
                        save_write(&g_save);

                        pr_info("config reset performed\n");

                        while (digitalRead(gpio_rst) == LOW) {
#ifdef HAVE_WS2812_LED
                                led_on(LED_WS2812, 0, 255, 255);
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
                        }

                        ESP.restart();
                }

                delay(500);
        }
}

#endif // __LIBJJ_SAVE_H__