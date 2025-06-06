#ifndef __LIBJJ_ESP32_UTILS_H__
#define __LIBJJ_ESP32_UTILS_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/temperature_sensor.h>

#include "utils.h"

#define CPU0                            (0)
#ifdef CONFIG_IDF_TARGET_ESP32S3
#define CPU1                            (1)
#elif defined CONFIG_IDF_TARGET_ESP32C3
#define CPU1                            (0)
#endif

static TaskStatus_t *esp32_top_stats_snapshot(UBaseType_t *task_cnt, unsigned long *ulTotalRunTime)
{
        TaskStatus_t *pxTaskStatusArray;
        UBaseType_t uxArraySize;
        
        uxArraySize = uxTaskGetNumberOfTasks();
        pxTaskStatusArray = (TaskStatus_t *)malloc(uxArraySize * sizeof(TaskStatus_t));
        if (!pxTaskStatusArray)
                return NULL;
        
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, ulTotalRunTime);
        *task_cnt = uxArraySize;

        return pxTaskStatusArray;
}

static int esp32_top_stats_print(unsigned sampling_ms, int (*__snprintf)(char *buffer, size_t bufsz, const char *format, ...), char *buf, size_t bufsz)
{
        struct task_track {
                const char *name;
                uint32_t tick_delta;
                uint16_t stack_high_watermark;
        };
        struct task_track *tracks;
        uint64_t total_ticks_delta = 0, idle_ticks_delta = 0;

        TaskStatus_t *tasks1, *tasks2;
        UBaseType_t task_cnt1, task_cnt2;
        unsigned long rt1, rt2;

        size_t c = 0;

        tasks1 = esp32_top_stats_snapshot(&task_cnt1, &rt1);
        if (!tasks1) {
                return -ENOMEM;
        }

        if (unlikely(sampling_ms == 0)) {
                vTaskDelay(pdMS_TO_TICKS(500));
        } else {
                vTaskDelay(pdMS_TO_TICKS(sampling_ms));
        }

        tasks2 = esp32_top_stats_snapshot(&task_cnt2, &rt2);
        if (!tasks2) {
                free(tasks1);
                return -ENOMEM;
        }

        if (task_cnt1 != task_cnt2) {
                free(tasks1);
                free(tasks2);
                return -EAGAIN;
        }

        tracks = (struct task_track *)calloc(task_cnt2, sizeof(struct task_track));
        if (!tracks) {
                free(tasks1);
                free(tasks2);
                return -ENOMEM;
        }

        for (unsigned i = 0; i < task_cnt2; i++) {
                tracks[i].name = tasks2[i].pcTaskName;
                tracks[i].stack_high_watermark = tasks2[i].usStackHighWaterMark;

                for (unsigned j = 0; j < task_cnt1; j++) {
                        if (is_str_equal((char *)tasks2[i].pcTaskName, (char *)tasks1[j].pcTaskName, CASELESS)) {
                                tracks[i].tick_delta = tasks2[i].ulRunTimeCounter - tasks1[j].ulRunTimeCounter;

                                if (strcmp(tasks2[i].pcTaskName, "IDLE") == 0 ||
                                    strcmp(tasks2[i].pcTaskName, "IDLE0") == 0 ||
                                    strcmp(tasks2[i].pcTaskName, "IDLE1") == 0) {
                                        idle_ticks_delta += tasks2[i].ulRunTimeCounter - tasks1[j].ulRunTimeCounter;
                                }

                                total_ticks_delta += tasks2[i].ulRunTimeCounter - tasks1[j].ulRunTimeCounter;
                        }
                }
        }

        if (!__snprintf) {
                __snprintf = ___snprintf_to_vprintf;
        }

        c += __snprintf(&buf[c], bufsz - c, "%-16s | %-10s | %-4s | Stack High Watermark\n", "Task", "Tick", "CPU");
        for (unsigned i = 0; i < task_cnt2; i++) {
                // XXX: ulTotalRunTime is much more smaller than total ticks
                //      ulTotalRunTime * 2 ~= total ticks
                uint64_t percent = 100ULL * tracks[i].tick_delta / total_ticks_delta;
                c += __snprintf(&buf[c], bufsz - c, "%-16s | %10lu | %3ju%% | %lu\n",
                                tracks[i].name,
                                tracks[i].tick_delta,
                                percent,
                                tracks[i].stack_high_watermark);
        }

        uint64_t total_cpu_percent = 100ULL * (total_ticks_delta - idle_ticks_delta) / total_ticks_delta;
        c += __snprintf(&buf[c], bufsz - c, "ulTotalRunTimeDelta: %lu TicksDelta: %ju IdleDelta: %ju TotalCPU: %ju%%\n",
                        rt2 - rt1, total_ticks_delta, idle_ticks_delta, total_cpu_percent);

        free(tasks1);
        free(tasks2);
        free(tracks);

        return c;
}

static int esp32_cpu_ticks_get(uint64_t *idle_ticks, uint64_t *total_ticks)
{
        TaskStatus_t *pxTaskStatusArray;
        UBaseType_t uxArraySize;
        uint64_t idle_time = 0, total_time = 0;

        uxArraySize = uxTaskGetNumberOfTasks();
        pxTaskStatusArray = (TaskStatus_t *)malloc(uxArraySize * sizeof(TaskStatus_t));

        if (pxTaskStatusArray == NULL)
                return -ENOMEM;

        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);

        for (UBaseType_t i = 0; i < uxArraySize; i++) {
                if (strcmp(pxTaskStatusArray[i].pcTaskName, "IDLE") == 0 ||
                    strcmp(pxTaskStatusArray[i].pcTaskName, "IDLE0") == 0 ||
                    strcmp(pxTaskStatusArray[i].pcTaskName, "IDLE1") == 0) {
                        idle_time += pxTaskStatusArray[i].ulRunTimeCounter;
                }

                total_time += pxTaskStatusArray[i].ulRunTimeCounter;
        }

        if (idle_ticks)
                *idle_ticks = idle_time;

        if (total_ticks)
                *total_ticks = total_time;

        free(pxTaskStatusArray);

        return 0;
}

static int esp32_cpu_usage_get(unsigned sampling_ms)
{
        uint64_t idle_s = 0, idle_e = 0, total_s = 0, total_e = 0;
        uint64_t idle_delta, total_delta;

        esp32_cpu_ticks_get(&idle_s, &total_s);
        vTaskDelay(pdMS_TO_TICKS(sampling_ms));
        esp32_cpu_ticks_get(&idle_e, &total_e);

        idle_delta = idle_e - idle_s;
        total_delta = total_e - total_s;

        return 100ULL * (total_delta - idle_delta) / total_delta;
}

static temperature_sensor_handle_t esp32_tsens;

int esp32_tsens_get(float *tempC)
{
        if (!tempC)
                return -EINVAL;

        if (esp32_tsens == NULL)
                return -ENODEV;

        if (temperature_sensor_get_celsius(esp32_tsens, tempC) != ESP_OK)
                return -EIO;

        return 0;
}

// Predefined Range (°C) Error (°C)
// 50 ~ 125 < 3
// 20 ~ 100 < 2
// -10 ~ 80 < 1
// -30 ~ 50 < 2
// -40 ~ 20 < 3
int esp32_tsens_init(void)
{
        temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
        temperature_sensor_handle_t ret = NULL;

        if (temperature_sensor_install(&cfg, &ret) != ESP_OK)
                return -ENODEV;

        if (temperature_sensor_enable(ret) != ESP_OK)
                return -EIO;

        esp32_tsens = ret;

        return 0;
}

#endif // __LIBJJ_ESP32_UTILS_H__