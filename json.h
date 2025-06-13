#ifndef __JJ_JSON_H__
#define __JJ_JSON_H__

#include "cJSON.h"
#include "logging.h"
#include "spiffs.h"

static const char json_indent[32] = {
                '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
                '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
                '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
                '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\0',
};

unsigned json_traverse_do_print(cJSON *node, unsigned c,
                        int (*__snprintf)(char *buffer, size_t bufsz, const char *format, ...), char *buf, size_t bufsz)
{
        unsigned n = 0;

        switch (node->type) {
        case cJSON_NULL:
                n += snprintf(&buf[c + n], bufsz - c - n, "[null]   ");
                break;
        case cJSON_Number:
                n += snprintf(&buf[c + n], bufsz - c - n, "[number] ");
                break;
        case cJSON_String:
                n += snprintf(&buf[c + n], bufsz - c - n, "[string] ");
                break;
        case cJSON_Array:
                n += snprintf(&buf[c + n], bufsz - c - n, "[array]  ");
                break;
        case cJSON_Object:
                n += snprintf(&buf[c + n], bufsz - c - n, "[object] ");
                break;
        case cJSON_Raw:
                n += snprintf(&buf[c + n], bufsz - c - n, "[raws]   ");
                break;
        case cJSON_True:
        case cJSON_False:
                n += snprintf(&buf[c + n], bufsz - c - n, "[bool]   ");
                break;
        }

        if (node->string)
                n += snprintf(&buf[c + n], bufsz - c - n, "\"%s\" ", node->string);

        switch (node->type) {
        case cJSON_False:
                n += snprintf(&buf[c + n], bufsz - c - n, ": false");
                break;
        case cJSON_True:
                n += snprintf(&buf[c + n], bufsz - c - n, ": true");
                break;
        case cJSON_Number:
                n += snprintf(&buf[c + n], bufsz - c - n, ": %.f", cJSON_GetNumberValue(node));
                break;
        case cJSON_String:
                n += snprintf(&buf[c + n], bufsz - c - n, ": \"%s\"", cJSON_GetStringValue(node));
                break;
        }

        n += snprintf(&buf[c + n], bufsz - c - n, "\n");

        return n;
}

unsigned json_traverse_print(cJSON *node, int depth, unsigned c,
                        int (*__snprintf)(char *buffer, size_t bufsz, const char *format, ...), char *buf, size_t bufsz)
{
        cJSON *child = NULL;
        unsigned n = 0;

        if (!node)
                return 0;

        n += snprintf(&buf[c + n], bufsz - c - n, "%.*s", depth, json_indent);
        n += json_traverse_do_print(node, c + n, __snprintf, buf, bufsz);

        // child = root->child
        cJSON_ArrayForEach(child, node) {
                n += json_traverse_print(child, depth + 1, c + n, __snprintf, buf, bufsz);
        }

        return n;
}

int json_print(const char *filepath, int (*__snprintf)(char *buffer, size_t bufsz, const char *format, ...), char *buf, size_t bufsz)
{
        cJSON *root_node;
        char *text;
        int err = 0;
        size_t len;

        if (unlikely(!filepath))
                return -EINVAL;

        if (unlikely(filepath[0] == '\0')) {
                pr_err("@path is empty\n");
                return -ENODATA;
        }

        if (!__snprintf) {
                __snprintf = ___snprintf_to_vprintf;
        }

        text = (char *)spiffs_file_read(filepath, &err, &len);
        if (!text) {
                pr_err("failed to read file %s, err = %d %s\n", filepath, err, strerror(abs(err)));
                return err;
        }

        root_node = cJSON_ParseWithLength(text, len);
        if (!root_node) {
                pr_err("cJSON failed to parse text\n");
                err = -EINVAL;

                goto free_text;
        }

        json_traverse_print(root_node, 0, 0, __snprintf, buf, bufsz);

        cJSON_Delete(root_node);

free_text:
        free(text);

        return err;
}

#endif // __JJ_JSON_H__