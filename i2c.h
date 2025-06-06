#ifndef __LIBJJ_I2C_H__
#define __LIBJJ_I2C_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <Wire.h>
#include <freertos/semphr.h>

#include "utils.h"

static SemaphoreHandle_t lck_i2c;

static void __i2cdetect(int (*__snprintf)(char *buffer, size_t bufsz, const char *format, ...), char *buf, unsigned len, uint8_t first, uint8_t last)
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
                        Wire.beginTransmission(address);
                        error = Wire.endTransmission();
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

static void __attribute__((unused)) i2cdetect(char *buf, size_t len)
{
        __i2cdetect(snprintf, buf, len, 0x03, 0x77);  // default range
}

static void __attribute__((unused)) i2c_init(unsigned pin_sda, unsigned pin_scl)
{
        lck_i2c = xSemaphoreCreateMutex();
        Wire.begin(pin_sda, pin_scl);
}

static int __attribute__((unused)) i2c_lock(void)
{
        return xSemaphoreTake(lck_i2c, portMAX_DELAY) == pdTRUE ? 0 : -1;
}

static void __attribute__((unused)) i2c_unlock(void)
{
        xSemaphoreGive(lck_i2c);
}

#endif // __LIBJJ_I2C_H__