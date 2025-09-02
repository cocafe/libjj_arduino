#ifndef __LIBJJ_I2C_H__
#define __LIBJJ_I2C_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <Wire.h>
#include <freertos/semphr.h>

#include "utils.h"
#include "logging.h"

#ifndef GPIO_I2C_SDA0
#define GPIO_I2C_SDA0                   (-1)
#endif

#ifndef GPIO_I2C_SCL0
#define GPIO_I2C_SCL0                   (-1)
#endif

#ifndef GPIO_I2C_SDA1
#define GPIO_I2C_SDA1                   (-1)
#endif

#ifndef GPIO_I2C_SCL1
#define GPIO_I2C_SCL1                   (-1)
#endif

#ifndef CONFIG_I2C_FREQ0
#define CONFIG_I2C_FREQ0                (100 * 1000UL)
#endif

#ifndef CONFIG_I2C_1_FREQ
#define CONFIG_I2C_1_FREQ                (100 * 1000UL)
#endif

struct i2c_cfg {
        int16_t pin_scl;
        int16_t pin_sda;
        uint32_t freq;
};

struct i2c_ctx {
        SemaphoreHandle_t lck;
        struct i2c_cfg *cfg;
        TwoWire *wire;
};

static struct i2c_ctx i2c_0;
static struct i2c_ctx i2c_1;

static void __i2cdetect(TwoWire *wire, int (*__snprintf)(char *buffer, size_t bufsz, const char *format, ...), char *buf, unsigned len, uint8_t first, uint8_t last)
{
        uint8_t i, address, error;
        size_t c = 0;

        if (!__snprintf) {
                __snprintf = ___snprintf_to_vprintf;
        }

        // table header
        c += __snprintf(&buf[c], len - c, "   ");
        for (i = 0; i < 16; i++) {
                c += __snprintf(&buf[c], len - c, "%3x", i);
        }

        c += __snprintf(&buf[c], len - c, "\n");

        // table body
        // addresses 0x00 through 0x77
        for (address = 0; address <= 119; address++) {
                if (address % 16 == 0) {
                        //printff("\n%#02x:", address & 0xF0);
                        c += __snprintf(&buf[c], len - c, "\n");
                        c += __snprintf(&buf[c], len - c, "%02x:", address & 0xF0);
                }
                if (address >= first && address <= last) {
                        wire->beginTransmission(address);
                        error = wire->endTransmission();
                        if (error == 0) {
                                // device found
                                //printff(" %02x", address);
                                c += __snprintf(&buf[c], len - c, " %02x", address);
                        } else if (error == 4) {
                                // other error
                                c += __snprintf(&buf[c], len - c, " XX");
                        } else {
                                // error = 2: received NACK on transmit of address
                                // error = 3: received NACK on transmit of data
                                c += __snprintf(&buf[c], len - c, " --");
                        }
                } else {
                        // address not scanned
                        c += __snprintf(&buf[c], len - c, "   ");
                }
        }

        c += __snprintf(&buf[c], len - c, "\n");
}

static int __unused i2cdetect(unsigned bus, char *buf, size_t len)
{
        TwoWire *wire;

        switch (bus) {
        case 0:
                wire = &Wire;
                break;

#if SOC_I2C_NUM > 1
        case 1:
                wire = &Wire1;
                break;
#endif

        default:
                return -ENODEV;
        }

        __i2cdetect(wire, snprintf, buf, len, 0x03, 0x77);  // default range

        return 0;
}

static int __i2c_init(struct i2c_ctx *i2c, TwoWire *wire, struct i2c_cfg *cfg)
{
        if (cfg->pin_scl < 0 || cfg->pin_sda < 0) {
                pr_info("invalid scl or sda pin\n");
                return -EINVAL;
        }

        wire->begin(cfg->pin_sda, cfg->pin_scl, cfg->freq);
        pr_info("scl %d sda %d, running at %luHz\n", cfg->pin_scl, cfg->pin_sda, wire->getClock());

        i2c->lck = xSemaphoreCreateMutex();
        i2c->cfg = cfg;
        i2c->wire = wire;

        vTaskDelay(pdMS_TO_TICKS(300));

        return 0;
}

static int i2c_0_init(struct i2c_cfg *cfg)
{
        return __i2c_init(&i2c_0, &Wire, cfg);
}

#if SOC_I2C_NUM > 1
static int i2c_1_init(struct i2c_cfg *cfg)
{
        return __i2c_init(&i2c_1, &Wire1, cfg);
}
#endif

static void i2c_init(struct i2c_cfg *cfg0, struct i2c_cfg *cfg1)
{
        if (i2c_0_init(cfg0))
                pr_err("failed to init i2c 0\n");

#if SOC_I2C_NUM > 1
        if (i2c_1_init(cfg1))
                pr_err("failed to init i2c 1\n");
#endif
}

static int __unused i2c_lock(struct i2c_ctx *ctx)
{
        if (!ctx->wire)
                return -ENODEV;

        return xSemaphoreTake(ctx->lck, portMAX_DELAY) == pdTRUE ? 0 : -1;
}

static void __unused i2c_unlock(struct i2c_ctx *ctx)
{
        if (!ctx->wire)
                return;

        xSemaphoreGive(ctx->lck);
}

#endif // __LIBJJ_I2C_H__