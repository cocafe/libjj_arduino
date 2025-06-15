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

#ifndef CONFIG_I2C_FREQ1
#define CONFIG_I2C_FREQ1                (100 * 1000UL)
#endif

static SemaphoreHandle_t lck_i2c;

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

static void __unused i2c_init(void)
{
        int pin_sda0 = GPIO_I2C_SDA0, pin_scl0 = GPIO_I2C_SCL0;

        lck_i2c = xSemaphoreCreateMutex();

        if (pin_sda0 >= 0 && pin_scl0 >= 0) {
                Wire.begin(pin_sda0, pin_scl0, CONFIG_I2C_FREQ0);
                pr_info("bus 0 running at %luHz\n", Wire.getClock());
        }

#if SOC_I2C_NUM > 1
        int pin_sda1 = GPIO_I2C_SDA1, pin_scl1 = GPIO_I2C_SCL1;

        if (pin_sda1 >= 0 && pin_scl1 >= 0) {
                Wire1.begin(pin_sda1, pin_scl1, CONFIG_I2C_FREQ1);
                pr_info("bus 1 running at %luHz\n", Wire1.getClock());
        }
#endif

        delay(300);
}

static int __unused i2c_lock(void)
{
        return xSemaphoreTake(lck_i2c, portMAX_DELAY) == pdTRUE ? 0 : -1;
}

static void __unused i2c_unlock(void)
{
        xSemaphoreGive(lck_i2c);
}

#endif // __LIBJJ_I2C_H__