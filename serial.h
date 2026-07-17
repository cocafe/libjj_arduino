#ifndef __LIBJJ_SERIAL_H_
#define __LIBJJ_SERIAL_H_

#include <stdio.h>

#include <Arduino.h>

#include "logging.h"
#include <esp_log.h>

static inline int esp_log_redirect(const char *fmt, va_list args)
{
        return Serial.vprintf(fmt, args);
}

static void serial_init(unsigned baud_rate)
{
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);

        Serial.begin(baud_rate);

#if ARDUINO_USB_CDC_ON_BOOT == 0
        while(!Serial)
                mdelay(10);
#endif

        esp_log_set_vprintf(esp_log_redirect);
}

#endif // __LIBJJ_SERIAL_H_