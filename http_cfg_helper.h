#ifndef __LIBJJ_HTTP_CFG_HELPER_H__
#define __LIBJJ_HTTP_CFG_HELPER_H__

#include <stdint.h>

#include <WebServer.h>

#include "utils.h"
#include "string.h"

#define HTTP_CFG_PARAM_INT(_name, _var) { __stringify(_name), &(_var), sizeof((_var)), 0, NULL, NULL, 0 }
#define HTTP_CFG_PARAM_STR(_name, _var) { __stringify(_name),  (_var), sizeof((_var)), 1, NULL, NULL, 0 }
#define HTTP_CFG_PARAM_STRMAP(_name, _var, _map) { __stringify(_name),  &(_var), sizeof((_var)), 0, _map, NULL, ARRAY_SIZE(_map) }
#define HTTP_CFG_PARAM_STRVAL(_name, _var, _map) { __stringify(_name),  &(_var), sizeof((_var)), 0, NULL, _map, ARRAY_SIZE(_map) }

struct http_cfg_param {
        const char *arg;
        void *val;
        size_t size;
        uint8_t is_char;
        struct strval *map;
        const char **map1;
        size_t mapsz;
};

#define WRITE_INT(ptr, sz, val)                         \
do {                                                    \
        switch (sz) {                                   \
        case 1:                                         \
                *((int8_t *)ptr) = (int8_t)val;         \
                break;                                  \
        case 2:                                         \
                *((int16_t *)ptr) = (int16_t)val;       \
                break;                                  \
        case 4:                                         \
                *((int32_t *)ptr) = (int32_t)val;       \
                break;                                  \
        case 8:                                         \
                *((int64_t *)ptr) = (int64_t)val;       \
                break;                                  \
                                                        \
        default:                                        \
                break;                                  \
        }                                               \
} while (0)

static __unused int http_param_help_print(WebServer &http_server, struct http_cfg_param *params, unsigned params_cnt)
{
        if (!http_server.hasArg("help"))
                return 0;

        char buf[1024] = { };
        int c = 0;

        for (int i = 0; i < params_cnt; i++) {
                struct http_cfg_param *p = &params[i];

                if (unlikely(p->arg == NULL))
                        continue;

                c += snprintf(&buf[c], sizeof(buf) - c, "%s: ", p->arg);
                if (p->is_char) {
                        c += snprintf(&buf[c], sizeof(buf) - c, "<string>");
                } else if (p->map && p->mapsz) {
                        c += snprintf(&buf[c], sizeof(buf) - c, "[");
                        for (int j = 0; j < p->mapsz; j++) {
                                c += snprintf(&buf[c], sizeof(buf) - c, "%s", p->map[j].str);
                                if (j + 1 < p->mapsz)
                                        c += snprintf(&buf[c], sizeof(buf) - c, " | ");
                        }
                        c += snprintf(&buf[c], sizeof(buf) - c, "]");
                } else {
                        c += snprintf(&buf[c], sizeof(buf) - c, "<integer>");
                }
                c += snprintf(&buf[c], sizeof(buf) - c, "\n");
        }

        http_server.send(200, "text/plain", buf);

        return 1;
}

static __unused int http_param_parse(WebServer &http_server, struct http_cfg_param *params, unsigned params_cnt)
{
        int modified = 0;

        for (unsigned i = 0; i < params_cnt; i++) {
                struct http_cfg_param *p = &params[i];

                if (unlikely(p->arg == NULL))
                        continue;

                if (http_server.hasArg(p->arg)) {
                        String arg = http_server.arg(p->arg);

                        if (unlikely(p->val == NULL || p->size == 0))
                                continue;

                        if (p->is_char) {
                                strncpy((char *)p->val, arg.c_str(), p->size);
                                ((char *)p->val)[p->size - 1] = '\0';
                                modified = 1;
                        } else if (p->map && p->mapsz) {
                                int found = 0;

                                for (unsigned j = 0; j < p->mapsz; j++) {
                                        if (is_str_equal((char *)arg.c_str(), (char *)(p->map[j].str), CASELESS)) {
                                                WRITE_INT(p->val, p->size, p->map[j].val);
                                                found = 1;
                                                break;
                                        }
                                }

                                if (found)
                                        modified = 1;
                                else
                                        return -ENOENT;
                        } else if (p->map1 && p->mapsz) { 
                                int found = 0;

                                for (unsigned j = 0; j < p->mapsz; j++) {
                                        if (is_str_equal((char *)arg.c_str(), (char *)(p->map1[j]), CASELESS)) {
                                                WRITE_INT(p->val, p->size, j);
                                                found = 1;
                                                break;
                                        }
                                }

                                if (found)
                                        modified = 1;
                                else
                                        return -ENOENT;
                        } else {
                                char *endptr;
                                int64_t _arg = strtoll(arg.c_str(), &endptr, 10);
                                
                                if (endptr == arg.c_str() || errno == ERANGE || *endptr != '\0') {
                                        return -EINVAL;
                                }

                                WRITE_INT(p->val, p->size, _arg);
                                modified = 1;
                        }
                }
        }

        return modified;
}

// int http_param_json_print(struct http_cfg_param *params, unsigned params_cnt, char *buf, unsigned bufsz)
// {
//         int c = 0;

//         c += snprintf(&buf[c], sizeof(buf) - c, "{\n");
//         for (int i = 0; i < params_cnt; i++) {
//                 struct http_cfg_param *p = &params[i];

//                 if (unlikely(p->arg == NULL))
//                         continue;

//                 c += snprintf(&buf[c], sizeof(buf) - c, " \"%s\": ", p->arg);
//                 if (p->is_char)
//                         c += snprintf(&buf[c], sizeof(buf) - c, "\"%s\"", (char *)p->val);
//                 else
//                         c += snprintf(&buf[c], sizeof(buf) - c, "%jd", (int64_t){ p->val });
//                 if (i + 1 < params_cnt)
//                         c += snprintf(&buf[c], sizeof(buf) - c, ",");
                
//                 c += snprintf(&buf[c], sizeof(buf) - c, "\n");
//         }
//         c += snprintf(&buf[c], sizeof(buf) - c, "}\n");

//         return c;
// }

#endif // __LIBJJ_HTTP_CFG_HELPER_H__