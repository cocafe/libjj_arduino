#ifndef __LIBJJ_LOGGING_H__
#define __LIBJJ_LOGGING_H__

#include <stdio.h>

#include "esp32_utils.h"

#if ARDUINO_USB_CDC_ON_BOOT
        #if ARDUINO_USB_MODE
                #define logging_printf HWCDCSerial.printf
        #else
                #define logging_printf USBSerial.printf
        #endif // ARDUINO_USB_MODE
#else
#define logging_printf printf
#endif // ARDUINO_USB_CDC_ON_BOOT

#define ___pr_timestamp()                                          \
        do {                                                       \
                logging_printf("[%8ju] ", esp32_millis());      \
        } while (0)

#define ___pr_wrapped(msg, fmt...)                                 \
        do {                                                       \
                logging_printf(msg, ##fmt);                     \
        } while (0)

#define pr_info(msg, fmt...)                            \
        do {                                            \
                ___pr_timestamp();                      \
                ___pr_wrapped("%s(): ", __func__);      \
                ___pr_wrapped(msg, ##fmt);              \
        } while(0)

#define pr_info_once(msg, fmt...)                       \
        do {                                            \
                static uint8_t __t = 0;                 \
                if (__t)                                \
                        break;                          \
                                                        \
                ___pr_wrapped("%s(): ", __func__);      \
                ___pr_wrapped(msg, ##fmt);              \
                __t = 1;                                \
        } while(0)

#define pr_raw_ts(msg, fmt...)                          \
        do {                                            \
                ___pr_timestamp();                      \
                ___pr_wrapped(msg, ##fmt);              \
        } while(0)

#define pr_raw(msg, fmt...)                             \
        do {                                            \
                ___pr_wrapped(msg, ##fmt);              \
        } while(0)

#define pr_none(msg, fmt...) do { } while (0)

#define pr_err          pr_info
#define pr_dbg          pr_info
#define pr_verbose      pr_none

#endif // __LIBJJ_LOGGING_H__