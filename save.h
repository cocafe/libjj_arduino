#ifndef __LIBJJ_SAVE_H__
#define __LIBJJ_SAVE_H__

#include <stdio.h>
#include <stdint.h>

#include <EEPROM.h>
#include <CRC32.h>

#include "utils.h"

#define SAVE_MAGIC                      (0x00C0CAFE)
#define SAVE_PAGE                       (0)

struct save {
        uint32_t magic;
        struct config cfg;
        uint32_t crc32;
};

static struct save g_save;

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

        crc32 = save_crc32(save);

        printf("%s(): calc crc32: 0x%08lx\n", __func__, crc32);
        printf("%s(): save crc32: 0x%08lx\n", __func__, save->crc32);

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

        hexdump(buf, sizeof(buf));

        if ((err = save_verify(s)))
                return err; 

        memcpy(save, buf, sizeof(struct save));

        return 0;
}

static int save_read(struct save *save)
{
        int err;

        if ((err = __save_read(SAVE_PAGE, save))) {
                printf("%s(): failed to read save, err = %d\n", __func__, err);
                return err;
        } 

        printf("%s(): read save successfully\n", __func__);
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
        save->crc32 = save_crc32(save);

        printf("%s(): save crc32: 0x%08lx\n", __func__, save->crc32);

        hexdump(save, sizeof(struct save));

        __save_write(SAVE_PAGE, save);

        printf("%s(): saved to flash\n", __func__);
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

        printf("%s(): sizeof save: %u, eeprom size: %lu\n", __func__, sizeof(struct save), eeprom_size);

        if (save_read(&g_save) != 0) {
                printf("%s(): failed to read save from flash, create new one\n", __func__);
                save_create(&g_save, &g_cfg_default);
                save_write(&g_save);
        }

        memcpy(&g_cfg, &g_save.cfg, sizeof(g_cfg));
}

#endif // __LIBJJ_SAVE_H__