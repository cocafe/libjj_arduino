#ifndef __LIBJJ_I2C_H__
#define __LIBJJ_I2C_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <Wire.h>

static void __i2cdetect(char *buf, size_t len, uint8_t first, uint8_t last)
{
        uint8_t i, address, error;
        size_t c = 0;

        // table header
        c += snprintf(&buf[c], len - c, "   ");
        for (i = 0; i < 16; i++) {
                c += snprintf(&buf[c], len - c, "%3x", i);
        }

        c += snprintf(&buf[c], len - c, "\n");

        // table body
        // addresses 0x00 through 0x77
        for (address = 0; address <= 119; address++) {
                if (address % 16 == 0) {
                        //printff("\n%#02x:", address & 0xF0);
                        c += snprintf(&buf[c], len - c, "\n");
                        c += snprintf(&buf[c], len - c, "%02x:", address & 0xF0);
                }
                if (address >= first && address <= last) {
                        Wire.beginTransmission(address);
                        error = Wire.endTransmission();
                        if (error == 0) {
                                // device found
                                //printff(" %02x", address);
                                c += snprintf(&buf[c], len - c, " %02x", address);
                        } else if (error == 4) {
                                // other error
                                c += snprintf(&buf[c], len - c, " XX");
                        } else {
                                // error = 2: received NACK on transmit of address
                                // error = 3: received NACK on transmit of data
                                c += snprintf(&buf[c], len - c, " --");
                        }
                } else {
                        // address not scanned
                        c += snprintf(&buf[c], len - c, "   ");
                }
        }

        c += snprintf(&buf[c], len - c, "\n");
}

static void __attribute__((unused)) i2cdetect(char *buf, size_t len)
{
        __i2cdetect(buf, len, 0x03, 0x77);  // default range
}

#endif // __LIBJJ_I2C_H__