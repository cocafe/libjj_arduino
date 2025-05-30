#ifndef __LIBJJ_LOGGING_H__
#define __LIBJJ_LOGGING_H__

#include <stdio.h>

#define LOG_ERR_STREAM          stdout

#ifdef LOG_ALWAYS_FLUSH
#define LOG_FLUSH(fp) do { fflush((fp)); } while (0)
#else
#define LOG_FLUSH(fp) do { } while (0)
#endif // LOG_ALWAYS_FLUSH

#define ___pr_wrapped(fp, msg, fmt...)                                  \
        do {                                                            \
                fprintf(fp, msg, ##fmt);                                \
                LOG_FLUSH(fp);                                          \
        } while (0)

#define pr_err(msg, fmt...)                                             \
        do {                                                            \
                ___pr_wrapped(LOG_ERR_STREAM, "%s(): ", __func__);      \
                ___pr_wrapped(LOG_ERR_STREAM, msg, ##fmt);              \
        } while(0)

#define pr_info(msg, fmt...)                                            \
        do {                                                            \
                ___pr_wrapped(stdout, "%s(): ", __func__);              \
                ___pr_wrapped(stdout, msg, ##fmt);                      \
        } while(0)

#define pr_dbg          pr_info
#define pr_verbose      pr_info

#endif // __LIBJJ_LOGGING_H__