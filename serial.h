#ifndef __LIBJJ_SERIAL_H_
#define __LIBJJ_SERIAL_H_

#include <stdio.h>

#include <Arduino.h>

static void serial_init(unsigned baud_rate)
{
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);

        Serial.begin(baud_rate);

#if ARDUINO_USB_CDC_ON_BOOT == 0
        while(!Serial)
                mdelay(10);
#endif
}

#endif // __LIBJJ_SERIAL_H_