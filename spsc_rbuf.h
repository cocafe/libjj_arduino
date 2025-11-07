#ifndef __LIBJJ_SPSC_RBUF_H__
#define __LIBJJ_SPSC_RBUF_H__

#include <stdio.h>
#include <memory.h>

#include "utils.h"
#include "esp32_utils.h"

#define RBUF_IDX_INC(x, qlen) mod_2((x) + 1, qlen)

struct spsc_rbuf {
        unsigned widx __attribute__((aligned(16))); // if not == ridx, points to last slot written
        unsigned ridx __attribute__((aligned(16))); // if not == widx, points to last slot has been read
        uint8_t *ring __attribute__((aligned(16)));
        size_t qlen;
        size_t elesz;
};

int spsc_rbuf_init(struct spsc_rbuf *rbuf, size_t qlen, size_t ele_sz)
{
        memset(rbuf, 0x00, sizeof(struct spsc_rbuf));

        rbuf->ring = (uint8_t *)calloc(1, qlen * ele_sz);
        if (!rbuf->ring) {
                pr_info("failed to allocate ring\n");
                return -ENOMEM;
        }
        rbuf->qlen = qlen;
        rbuf->elesz = ele_sz;

        pr_info("allocated %zu bytes for ring\n", qlen * ele_sz);

        return 0;
}

size_t spsc_rbuf_ring_size_compute(size_t qlen, size_t ele_sz)
{
        return qlen * ele_sz;
}

int spsc_rbuf_init_extern_alloc(struct spsc_rbuf *rbuf, uint8_t *ring, size_t qlen, size_t ele_sz)
{
        memset(rbuf, 0x00, sizeof(struct spsc_rbuf));

        rbuf->ring = ring;
        rbuf->qlen = qlen;
        rbuf->elesz = ele_sz;

        return 0;
}

void *spsc_rbuf_wptr_get(struct spsc_rbuf *rbuf)
{
        uint32_t ridx = READ_ONCE(rbuf->ridx);
        uint32_t widx = READ_ONCE(rbuf->widx);

        // (widx + 1 == ridx) , is full ?
        if (RBUF_IDX_INC(widx, rbuf->qlen) == ridx) {
                return NULL;
        }

        rbuf->widx = RBUF_IDX_INC(widx, rbuf->qlen);

        return (void *)(&rbuf->ring[widx * rbuf->elesz]);
}

void *spsc_rbuf_rptr_get(struct spsc_rbuf *rbuf)
{
        uint32_t ridx = READ_ONCE(rbuf->ridx);
        uint32_t widx = READ_ONCE(rbuf->widx);

        if (widx == ridx) {
                return NULL;
        }

        return (void *)(&rbuf->ring[ridx * rbuf->elesz]);
}

int spsc_rbuf_rptr_put(struct spsc_rbuf *rbuf)
{
        uint32_t ridx = READ_ONCE(rbuf->ridx);
        uint32_t widx = READ_ONCE(rbuf->widx);

        if (widx == ridx) {
                return 0;
        }

        rbuf->ridx = RBUF_IDX_INC(ridx, rbuf->qlen);

        return 0;
}

uint32_t spsc_rbuf_qlen_get(struct spsc_rbuf *rbuf)
{
        uint32_t ridx = READ_ONCE(rbuf->ridx);
        uint32_t widx = READ_ONCE(rbuf->widx);

        if (widx == ridx) {
                return 0;
        }

        // reader is behind
        if (ridx < widx) {
                return (widx - ridx);
        }

        // writer is behind
        return (widx + 1) + ((rbuf->qlen - 1) - ridx);
}

#endif // __LIBJJ_SPSC_RBUF_H__