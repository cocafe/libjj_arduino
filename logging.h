#ifndef __LIBJJ_LOGGING_H__
#define __LIBJJ_LOGGING_H__

#include <stdio.h>

#include "esp32_utils.h"

#define ___pr_timestamp()                               \
        do {                                            \
                printf("[%8ju] ", esp32_millis());      \
        } while (0)

#define ___pr_wrapped(msg, fmt...)                      \
        do {                                            \
                printf(msg, ##fmt);                     \
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