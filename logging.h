#ifndef __LIBJJ_LOGGING_H__
#define __LIBJJ_LOGGING_H__

#include <stdio.h>

#ifndef LOGGING_SERIAL
#if ARDUINO_USB_CDC_ON_BOOT
        #if ARDUINO_USB_MODE
                #define LOGGING_SERIAL HWCDCSerial
        #else
                #define LOGGING_SERIAL USBSerial
        #endif // ARDUINO_USB_MODE
#else
#define LOGGING_SERIAL Serial
#endif // ARDUINO_USB_CDC_ON_BOOT
#endif

#define pr_ser LOGGING_SERIAL.printf

#define ___pr_timestamp()                                          \
        do {                                                       \
                pr_ser("[%8ju] ", esp_timer_get_time() / 1000);      \
        } while (0)

#define ___pr_wrapped(msg, fmt...)                                 \
        do {                                                       \
                pr_ser(msg, ##fmt);                     \
        } while (0)

#define pr_ts_func(msg, fmt...)                         \
        do {                                            \
                ___pr_timestamp();                      \
                ___pr_wrapped("%s(): ", __func__);      \
                ___pr_wrapped(msg, ##fmt);              \
        } while(0)

#define pr_ts_func_once(msg, fmt...)                    \
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

#define pr_info         pr_ts_func
#define pr_info_once    pr_ts_func_once
#define pr_err          pr_info
#define pr_dbg          pr_info
#define pr_verbose      pr_none

#endif // __LIBJJ_LOGGING_H__