#ifndef __LIBJJ_PROMETHEUS_H__
#define __LIBJJ_PROMETHEUS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "list.h"

static const char PROM_METRIC_TYPE_COUNTER[]   = "counter";
static const char PROM_METRIC_TYPE_GAUGE[]     = "gauge";
static const char PROM_METRIC_TYPE_HISTOGRAM[] = "histogram";
static const char PROM_METRIC_TYPE_SUMMARY[]   = "summary";

#ifndef PROM_MAX_LABELS
#define PROM_MAX_LABELS 50
#endif

#ifndef PROM_MAX_METRICS
#define PROM_MAX_METRICS 256
#endif

#ifndef PROM_BUF_SIZE
#define PROM_BUF_SIZE 128
#endif

#define PROM_CONN_BACKLOG 10

// Generic definition for a metric including name, help and type
typedef struct prom_metric_def {
        char *name;
        char *help;
        const char *type;
} prom_metric_def;

// Key-value pair representing a label name with an assigned value
typedef struct prom_label {
        char *key;
        char *value;
} prom_label;

// Represents an instance of a metric with a given value and set of labels
typedef struct prom_metric {
        struct list_head node;
        int num_labels;
        struct prom_label labels[PROM_MAX_LABELS];
        double value;
} prom_metric;

// A container for metrics that share a common definition
typedef struct prom_metric_def_set {
        prom_metric_def *def;
        struct list_head metrics;
} prom_metric_def_set;

// Container for a set of references to prom_metrics
typedef struct prom_metric_set {
        int n_defs;
        prom_metric_def_set *defs[PROM_MAX_METRICS];
} prom_metric_set;

typedef struct prom_server {
        char *buf;
        size_t buf_pos;
        size_t buf_len;
} prom_server;

// posix realloc() does not clear new area
static void *realloc_safe(void *old_ptr, size_t old_sz, size_t new_sz)
{
        void *new_ptr;

        if (!old_ptr || !new_sz)
                return old_ptr;

        new_ptr = calloc(1, new_sz);
        if (!new_ptr)
                return NULL;

        if (new_sz >= old_sz)
                memcpy(new_ptr, old_ptr, old_sz);
        else
                memcpy(new_ptr, old_ptr, new_sz);

        free(old_ptr);

        return new_ptr;
}


static int vprintf_resize(char **buf, size_t *pos, size_t *alloc_len, const char *fmt, va_list arg)
{
        va_list arg2;
        char cbuf;
        char *sbuf = *buf;
        int append_len, ret;

        va_copy(arg2, arg);
        append_len = vsnprintf(&cbuf, sizeof(cbuf), fmt, arg2);
        va_end(arg2);

        if (append_len < 0)
                return append_len;

        if (!sbuf) {
                size_t new_len = append_len + 2;

                sbuf = (char *)calloc(new_len, sizeof(char));
                if (!sbuf)
                        return -ENOMEM;

                *alloc_len = new_len;
                *buf = sbuf;
                *pos = 0;
        } else {
                size_t new_total_len = *alloc_len + append_len + 2;

                // do realloc
                if (*pos + append_len + 2 > *alloc_len) {
                        sbuf = (char *)realloc_safe(*buf, *alloc_len, new_total_len);
                        if (!sbuf)
                                return -ENOMEM;

                        *alloc_len = new_total_len;
                        *buf = sbuf;
                }
        }

        sbuf = &((*buf)[*pos]);

        ret = vsnprintf(sbuf, *alloc_len - *pos, fmt, arg);
        if (ret < 0) {
                return ret;
        }

        *pos += ret;

        return ret;
}

static int snprintf_resize(char **buf, size_t *pos, size_t *alloc_len, const char *fmt, ...)
{
        va_list ap;
        int ret;

        va_start(ap, fmt);
        ret = vprintf_resize(buf, pos, alloc_len, fmt, ap);
        va_end(ap);

        return ret;
}

void prom_buf_free(prom_server *srv)
{
        if (srv->buf) {
                free(srv->buf);
                srv->buf = NULL;
        }
}

void prom_buf_alloc(prom_server *srv)
{
        if (srv->buf)
                return;

        if (!srv->buf_len) {
                srv->buf_len = PROM_BUF_SIZE;
        }

        // may use recorded max buffer length
        srv->buf = (char *)calloc(1, srv->buf_len);
}

int prom_init(prom_server *srv, prom_metric_set *set)
{
        memset(set, 0, sizeof(*set));
        memset(srv, 0, sizeof(*srv));

        prom_buf_alloc(srv);

        return 0;
}

void prom_register(prom_metric_set *s, prom_metric_def *d)
{
        // Check if we already have this definition
        int existing = -1;

        for (int i = 0; i < s->n_defs; i++) {
                if (s->defs[i]->def == d) {
                        existing = i;
                }
        }

        if (existing == -1) {
                // It doesn't exist, create it
                existing = s->n_defs;
                s->n_defs++;
                s->defs[existing] = (prom_metric_def_set *)malloc(sizeof(prom_metric_def_set));
                memset(s->defs[existing], 0, sizeof(prom_metric_def_set));
                s->defs[existing]->def = d;
                INIT_LIST_HEAD(&s->defs[existing]->metrics);
        }
}

// Initializes a prom_metric with a zero value and empty label set
void prom_metric_init(prom_metric *m)
{
        m->num_labels = 0;
        memset(&m->labels, 0, sizeof(prom_label) * PROM_MAX_LABELS);
        m->value = 0;
        INIT_LIST_HEAD(&m->node);
}

// Sets a label key and value on the given metric value
void prom_metric_set_label(prom_metric *m, char *key, char *value)
{
        m->labels[m->num_labels].key = key;
        m->labels[m->num_labels].value = value;
        m->num_labels++;
}

prom_metric *prom_get(prom_metric_set *s, prom_metric_def *d, int n, ...)
{
        va_list args;
        va_start(args, n);
        prom_label ulabels[PROM_MAX_LABELS];
        for (int i = 0; i < n; i++) {
                ulabels[i] = va_arg(args, prom_label);
        }
        va_end(args);

        int found = 0;
        prom_metric *m_found;
        prom_metric_def_set *ds = NULL;

        for (int i = 0; i < s->n_defs; i++) {
                if (s->defs[i]->def == d) {
                        prom_metric *m;

                        ds = s->defs[i];

                        list_for_each_entry(m, &ds->metrics, node) {
                                // Compare labels
                                int labels_match = 1;

                                if (m->num_labels != n)
                                        continue;

                                for (int l = 0; l < m->num_labels; l++) {
                                        prom_label mlab = m->labels[l];
                                        prom_label ulab = ulabels[l];
                                        if (strcmp(mlab.key, ulab.key) || strcmp(mlab.value, ulab.value))
                                                labels_match = 0;
                                }

                                if (labels_match == 1) {
                                        m_found = m;
                                        found = 1;
                                        break;
                                }
                        }

                        break;
                }
        }

        // Create if not found
        if (found == 0) {
                if (!ds) {
                        return NULL;
                } else {
                        prom_metric *m = (prom_metric *)calloc(1, sizeof(prom_metric));
                        prom_metric_init(m);

                        for (int i = 0; i < n; i++) {
                                prom_metric_set_label(m, ulabels[i].key, ulabels[i].value);
                        }

                        list_add_tail(&m->node, &ds->metrics);

                        return m;
                }
        } else {
                return m_found;
        }
}

void prom_del(prom_metric *m)
{
        list_del(&m->node);
        free(m);
}

void _prom_escape(char *buf, char *str)
{
        int pos = 0;
        int len = strlen(str);

        for (int i = 0; i < len; i++) {
                switch (str[i]) {
                case '\n':
                        buf[pos] = '\\';
                        pos++;
                        buf[pos] = 'n';
                        pos++;
                        break;
                case '\\':
                        buf[pos] = '\\';
                        pos++;
                        buf[pos] = '\\';
                        pos++;
                        break;
                case '"':
                        buf[pos] = '\\';
                        pos++;
                        buf[pos] = '"';
                        pos++;
                        break;
                default:
                        buf[pos] = str[i];
                        pos++;
                }
        }
        buf[pos] = '\0';
}

// Prints the metric value to the given IO
void prom_metric_write(prom_server *srv, prom_metric_def_set *s)
{
        char buf[PROM_BUF_SIZE];
        prom_metric *m;

        // Write the header comments
        snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "# TYPE %s %s\n", s->def->name, s->def->type);
        snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "# HELP %s %s\n", s->def->name, s->def->help);

        // Write the metric values
        list_for_each_entry(m, &s->metrics, node) {
                snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "%s", s->def->name);
                if (m->num_labels > 0) {
                        snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "{");
                        for (int i = 0; i < m->num_labels; i++) {
                                _prom_escape(buf, m->labels[i].key);
                                snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "%s", buf);
                                snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "=\"");
                                _prom_escape(buf, m->labels[i].value);
                                snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "%s", buf);
                                snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "\"");
                                if (i < (m->num_labels - 1)) {
                                        snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, ",");
                                }
                        }
                        snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, "}");
                }
                snprintf_resize(&srv->buf, &srv->buf_pos, &srv->buf_len, " %f\n", m->value);
        }
}

// Writes metrics out to the temp file
void prom_flush(prom_server *srv, prom_metric_set *s)
{
        srv->buf_pos = 0;

        for (int i = 0; i < s->n_defs; i++) {
                prom_metric_write(srv, s->defs[i]);
        }
}

void prom_cleanup(prom_server *srv, prom_metric_set *s)
{
        for (int i = 0; i < s->n_defs; i++) {
                prom_metric_def_set *ds = s->defs[i];
                prom_metric *m, *n;

                // Free each metric pointer
                list_for_each_entry_safe(m, n, &ds->metrics, node) {
                        list_del(&m->node);
                        free(m);
                }

                // Free the def set
                free(ds);
        }

        if (srv->buf)
                free(srv->buf);
}

#endif // __LIBJJ_PROMETHEUS_H__
