#ifndef __LIBJJ_CAN_RLIMIT_H__
#define __LIBJJ_CAN_RLIMIT_H__

#include <stdint.h>

#include "list.h"
#include "hashtable.h"

enum {
        CAN_RLIMIT_TYPE_TCP,
        CAN_RLIMIT_TYPE_RC,
        NUM_CAN_RLIMIT_TYPES,
};

static const char *str_rlimit_types[NUM_CAN_RLIMIT_TYPES] = {
        [CAN_RLIMIT_TYPE_TCP] = "tcp",
        [CAN_RLIMIT_TYPE_RC] = "rc",
};

struct can_rlimit_cfg {
        uint8_t enabled;
        int8_t default_hz[NUM_CAN_RLIMIT_TYPES];
};

struct can_rlimit_data {
        uint32_t                last_ts;
        int16_t                 update_ms;      // say -1 to deny can id
};

struct can_rlimit_node {
        struct hlist_node       hnode;
        uint16_t                can_id;
        struct can_rlimit_data  data[NUM_CAN_RLIMIT_TYPES];
};

struct can_rlimit_ctx {
        struct hlist_head htbl[1 << 7];
        SemaphoreHandle_t lck;
        struct can_rlimit_cfg *cfg;
};

static struct can_rlimit_cfg can_rlimit_default_cfg = { };
static struct can_rlimit_ctx can_rlimit = { };

static inline unsigned update_hz_to_ms(unsigned hz)
{
        if (hz == 0)
                return 0;

        return 1 * 1000 / hz;
}

static inline unsigned update_ms_to_hz(unsigned ms)
{
        if (ms == 0)
                return 0;

        return 1 * 1000 / ms;
}

static inline struct can_rlimit_node *can_ratelimit_add(unsigned can_id)
{
        struct can_rlimit_node *n = (struct can_rlimit_node *)calloc(1, sizeof(struct can_rlimit_node));
        if (!n) {
                pr_err("%s(): no memory\n", __func__);
                return NULL;
        }

        INIT_HLIST_NODE(&n->hnode);

        n->can_id = can_id;

        hash_add(can_rlimit.htbl, &n->hnode, n->can_id);

        for (unsigned i = 0; i < ARRAY_SIZE(n->data); i++) {
                struct can_rlimit_data *d = &n->data[i];
                d->update_ms = can_rlimit.cfg->default_hz[i];
        }

        pr_dbg("can_id 0x%03x\n", can_id);

        return n;
}

static inline void can_ratelimit_del(struct can_rlimit_node *n)
{
        hash_del(&n->hnode);
        free(n);
}

static inline struct can_rlimit_node *can_ratelimit_get(unsigned can_id)
{
        uint32_t bkt = hash_bkt(can_rlimit.htbl, can_id);
        struct can_rlimit_node *n;
        int8_t found = 0;

        hash_for_bkt_each(can_rlimit.htbl, bkt, n, hnode) {
                if (n->can_id != can_id)
                        continue;

                found = 1;
                break;
        }

        if (found)
                return n;

        return NULL;
}

static int can_ratelimit_set(struct can_rlimit_node *n, unsigned type, int update_hz)
{
        if (unlikely(!n))
                return -ENODATA;

        if (unlikely(type >= NUM_CAN_RLIMIT_TYPES))
                return -EINVAL;

        struct can_rlimit_data *d = &n->data[type];

        if (update_hz == 0) {
                pr_dbg("can_id 0x%03x type \"%s\": unlimited\n", n->can_id, str_rlimit_types[type]);
                d->update_ms = 0;
        } else if (update_hz < 0) {
                pr_dbg("can_id 0x%03x type \"%s\": drop\n", n->can_id, str_rlimit_types[type]);
                d->update_ms = -1;
        } else {
                pr_dbg("can_id 0x%03x type \"%s\": %d hz\n", n->can_id, str_rlimit_types[type], update_hz);
                d->update_ms = update_hz_to_ms(update_hz);
        }

        return 0;
}

static void can_ratelimit_set_all(unsigned type, int update_hz)
{
        uint32_t bkt;
        struct hlist_node *tmp;
        struct can_rlimit_node *n;

        hash_for_each_safe(can_rlimit.htbl, bkt, tmp, n, hnode) {
                can_ratelimit_set(n, type, update_hz);
        }
}

static int __is_can_id_ratelimited(struct can_rlimit_node *n, unsigned type, uint32_t now)
{
        struct can_rlimit_data *d = &n->data[type];

        if (unlikely(type >= NUM_CAN_RLIMIT_TYPES))
                return 0;

        if (d->update_ms == 0)
                return 0;

        if (d->update_ms < 0)
                return 1;

        if (now - d->last_ts >= d->update_ms) {
                d->last_ts = now;
                return 0;
        }

        return 1;
}

static int is_can_id_ratelimited(struct can_rlimit_node *n, unsigned type, uint32_t now)
{
        int ret;

        if (!n || !can_rlimit.cfg->enabled)
                return 0;

        // whitelist: 0x7DF and 0x7E0...0x7EF
        if ((n->can_id == 0x7DF) || (n->can_id == 0x7E0))
                return 0;

        ret = __is_can_id_ratelimited(n, type, now);

        return ret;
}

static void can_rlimit_lock(void)
{
#ifdef CONFIG_HAVE_CAN_RLIMIT
        xSemaphoreTake(can_rlimit.lck, portMAX_DELAY);
#endif
}

static void can_rlimit_unlock(void)
{
#ifdef CONFIG_HAVE_CAN_RLIMIT
        xSemaphoreGive(can_rlimit.lck);
#endif
}

static int can_ratelimit_init(struct can_rlimit_cfg *cfg)
{
        can_rlimit.cfg = cfg;

        if (!cfg) {
                can_rlimit.cfg = &can_rlimit_default_cfg; 
        }

        for (int i = 0; i < ARRAY_SIZE(can_rlimit.htbl); i++) {
                can_rlimit.htbl[i].first = NULL;
        }

        can_rlimit.lck = xSemaphoreCreateMutex();
        hash_init(can_rlimit.htbl);

        return 0;
}

#endif // __LIBJJ_CAN_RLIMIT_H__