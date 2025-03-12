#ifndef __LIBJJ_I2C_H__
#define __LIBJJ_I2C_H__

static void __i2cdetect(uint8_t first, uint8_t last)
{
        uint8_t i, address, error;

        // table header
        printf("   ");
        for (i = 0; i < 16; i++) {
                printf("%3x", i);
        }

        printf("\n");

        // table body
        // addresses 0x00 through 0x77
        for (address = 0; address <= 119; address++) {
                if (address % 16 == 0) {
                        //printff("\n%#02x:", address & 0xF0);
                        printf("\n");
                        printf("%02x:", address & 0xF0);
                }
                if (address >= first && address <= last) {
                        Wire.beginTransmission(address);
                        error = Wire.endTransmission();
                        if (error == 0) {
                                // device found
                                //printff(" %02x", address);
                                printf(" %02x", address);
                        } else if (error == 4) {
                                // other error
                                printf(" XX");
                        } else {
                                // error = 2: received NACK on transmit of address
                                // error = 3: received NACK on transmit of data
                                printf(" --");
                        }
                } else {
                        // address not scanned
                        printf("   ");
                }
        }

        printf("\n");
}

static void __attribute__((unused)) i2cdetect(void)
{
        printf("i2c bus detect:\n");
        __i2cdetect(0x03, 0x77);  // default range
}

#endif // __LIBJJ_I2C_H__