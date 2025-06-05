#ifndef __LIBJJ_ERRTRAP_H__
#define __LIBJJ_ERRTRAP_H__

#include "leds.h"

static void err_trap()
{
        while (1) {
                vTaskDelay(portMAX_DELAY);
        }
}

static void __attribute__((unused)) errtrap_print(const char *buf)
{
        printf(buf);
        fflush(stdout);
        err_trap();
}

#define ERRTRAP_PRINTF(fmt, ...)                                \
        do {                                                    \
                char buf[64] = { 0 };                           \
                snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
                errtrap_print(buf);                             \
        } while (0);

#endif // __LIBJJ_ERRTRAP_H__