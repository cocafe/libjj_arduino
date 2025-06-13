#ifndef __LIBJJ_UTLIS_H__
#define __LIBJJ_UTLIS_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <endian.h>
#include <memory.h>

typedef uint32_t __u32;
typedef uint64_t __u64;

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;
typedef uint32_t __be32;
typedef uint64_t __le64;
typedef uint64_t __be64;

#ifndef __min
#define __min(a, b)                             \
        ({                                      \
                __typeof__((a)) _a = (a);       \
                __typeof__((b)) _b = (b);       \
                _a < _b ? _a : _b;              \
        })
#endif /* __min */

#ifndef __max
#define __max(a, b)                             \
        ({                                      \
                __typeof__((a)) _a = (a);       \
                __typeof__((b)) _b = (b);       \
                _a > _b ? _a : _b;              \
        })
#endif /* __max */

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef sizeof_member
#define sizeof_member(type, member) sizeof(((type *)(0))->member)
#endif

/* Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */
#define __stringify_1(x...)     #x
#define __stringify(x...)       __stringify_1(x)

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(x)           (int)(sizeof(x) / sizeof(x[0]))
#endif

#ifndef likely
# define likely(x)              __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
# define unlikely(x)            __builtin_expect(!!(x), 0)
#endif

#ifndef BIT
# define BIT(nr)                (1UL << (nr))
#endif

#define ROUND_UP_POWER2(x) ((x) <= 1 ? 1 : (1U << (32 - __builtin_clz((x) - 1))))
#define ROUND_UP_POWER2_ULL(x) ((x) <= 1 ? 1 : (1ULL << (64 - __builtin_clzll((x) - 1))))

/* Optimization barrier */
/* The "volatile" is due to gcc bugs */
#define barrier() __asm__ __volatile__("": : :"memory")

typedef uint8_t __attribute__((__may_alias__))  __u8_alias_t;
typedef uint16_t __attribute__((__may_alias__)) __u16_alias_t;
typedef uint32_t __attribute__((__may_alias__)) __u32_alias_t;
typedef uint64_t __attribute__((__may_alias__)) __u64_alias_t;

static __always_inline void __read_once_size(const volatile void *p, void *res, int size)
{
        switch (size) {
        case 1: *(__u8_alias_t  *) res = *(volatile __u8_alias_t  *) p; break;
        case 2: *(__u16_alias_t *) res = *(volatile __u16_alias_t *) p; break;
        case 4: *(__u32_alias_t *) res = *(volatile __u32_alias_t *) p; break;
        case 8: *(__u64_alias_t *) res = *(volatile __u64_alias_t *) p; break;
        default:
                barrier();
                __builtin_memcpy((void *)res, (const void *)p, size);
                barrier();
        }
}

static __always_inline void __write_once_size(volatile void *p, void *res, int size)
{
        switch (size) {
        case 1: *(volatile  __u8_alias_t *) p = *(__u8_alias_t  *) res; break;
        case 2: *(volatile __u16_alias_t *) p = *(__u16_alias_t *) res; break;
        case 4: *(volatile __u32_alias_t *) p = *(__u32_alias_t *) res; break;
        case 8: *(volatile __u64_alias_t *) p = *(__u64_alias_t *) res; break;
        default:
                barrier();
                __builtin_memcpy((void *)p, (const void *)res, size);
                barrier();
        }
}

/*
 * Prevent the compiler from merging or refetching reads or writes. The
 * compiler is also forbidden from reordering successive instances of
 * READ_ONCE and WRITE_ONCE, but only when the compiler is aware of some
 * particular ordering. One way to make the compiler aware of ordering is to
 * put the two invocations of READ_ONCE or WRITE_ONCE in different C
 * statements.
 *
 * These two macros will also work on aggregate data types like structs or
 * unions. If the size of the accessed data type exceeds the word size of
 * the machine (e.g., 32 bits or 64 bits) READ_ONCE() and WRITE_ONCE() will
 * fall back to memcpy and print a compile-time warning.
 *
 * Their two major use cases are: (1) Mediating communication between
 * process-level code and irq/NMI handlers, all running on the same CPU,
 * and (2) Ensuring that the compiler does not fold, spindle, or otherwise
 * mutilate accesses that either do not require ordering or that interact
 * with an explicit memory barrier or atomic instruction that provides the
 * required ordering.
 */

#define READ_ONCE(x)                                    \
({                                                      \
        union { typeof(x) __val; char __c[1]; } __u =   \
                { .__c = { 0 } };                       \
        __read_once_size(&(x), __u.__c, sizeof(x));     \
        __u.__val;                                      \
})

#define WRITE_ONCE(x, val)                              \
({                                                      \
        union { typeof(x) __val; char __c[1]; } __u =   \
                { .__val = (val) };                     \
        __write_once_size(&(x), __u.__c, sizeof(x));    \
        __u.__val;                                      \
})

struct strval {
        const char *str;
        int32_t val;
};

enum {
        MATCH_CASE = 0,
        CASELESS = 1,
};

static inline int ___snprintf_to_vprintf(char *buffer, size_t bufsz, const char *format, ...)
{
        int ret;

        va_list vlist2;
        va_start(vlist2, format);

        ret = vprintf(format, vlist2);

        va_end(vlist2);

        return ret;
}

static int __attribute__((unused)) is_str_equal(char *a, char *b, int caseless)
{
        enum {
                NOT_EQUAL = 0,
                EQUAL = 1,
        };

        size_t len_a;
        size_t len_b;

        if (!a || !b)
                return NOT_EQUAL;

        if (unlikely(a == b))
                return EQUAL;

        len_a = strlen(a);
        len_b = strlen(b);

        if (len_a != len_b)
                return NOT_EQUAL;

        if (caseless) {
                if (!strncasecmp(a, b, len_a))
                        return EQUAL;

                return NOT_EQUAL;
        }

        if (!strncmp(a, b, len_a))
                return EQUAL;

        return NOT_EQUAL;
}

static void __attribute__((unused)) hexdump(const void *data, size_t size) {
        char ascii[17];
        size_t i, j;
        ascii[16] = '\0';

        for (i = 0; i < size; ++i) {
                printf("%02X ", ((uint8_t *)data)[i]);
                if (((uint8_t *)data)[i] >= ' ' && ((uint8_t *)data)[i] <= '~') {
                        ascii[i % 16] = ((uint8_t *)data)[i];
                } else {
                        ascii[i % 16] = '.';
                }
                if ((i + 1) % 8 == 0 || i + 1 == size) {
                        printf(" ");
                        if ((i + 1) % 16 == 0) {
                                printf("|  %s \n", ascii);
                        } else if (i + 1 == size) {
                                ascii[(i + 1) % 16] = '\0';
                                if ((i + 1) % 16 <= 8) {
                                        printf(" ");
                                }
                                for (j = (i + 1) % 16; j < 16; ++j) {
                                        printf("   ");
                                }
                                printf("|  %s \n", ascii);
                        }
                }
        }
}

static void __attribute__((unused)) hexdump_addr(const void *data, size_t size, uint32_t prefix_addr)
{
        char ascii[17];
        size_t i, j;
        ascii[16] = '\0';

        for (i = 0; i < size; ++i) {
                if (i % 16 == 0)
                        printf("%08lx | ", prefix_addr + i);

                printf("%02X ", ((uint8_t *)data)[i]);
                if (((uint8_t *)data)[i] >= ' ' && ((uint8_t *)data)[i] <= '~') {
                        ascii[i % 16] = ((uint8_t *)data)[i];
                } else {
                        ascii[i % 16] = '.';
                }
                if ((i + 1) % 8 == 0 || i + 1 == size) {
                        printf(" ");
                        if ((i + 1) % 16 == 0) {
                                printf("|  %s \n", ascii);
                        } else if (i + 1 == size) {
                                ascii[(i + 1) % 16] = '\0';
                                if ((i + 1) % 16 <= 8) {
                                        printf(" ");
                                }
                                for (j = (i + 1) % 16; j < 16; ++j) {
                                        printf("   ");
                                }
                                printf("|  %s \n", ascii);
                        }
                }
        }
}

static unsigned long long __attribute__((unused)) strtoull_wrap(const char *str, int base, int *err)
{
        char *endptr;
        unsigned long long ret = strtoull(str, &endptr, base);
        
        if (endptr == str || errno == ERANGE || *endptr != '\0') {
                *err = -EINVAL;
                return 0;
        }

        *err = 0;
        return ret;
}

static long long __attribute__((unused)) strtoll_wrap(const char *str, int base, int *err)
{
        char *endptr;
        long long ret = strtoull(str, &endptr, base);
        
        if (endptr == str || errno == ERANGE || *endptr != '\0') {
                *err = -EINVAL;
                return 0;
        }

        *err = 0;
        return ret;
}

void *realloc_safe(void *old_ptr, size_t old_sz, size_t new_sz)
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

static inline int ptr_word_write(void *data, size_t word_sz, int64_t val)
{
        switch (word_sz) {
        case 1:
                *(int8_t *)data = (int8_t)val;
                break;

        case 2:
                *(int16_t *)data = (int16_t)val;
                break;

        case 4:
                *(int32_t *)data = (int32_t)val;
                break;

        case 8:
                *(int64_t *)data = val;
                break;

        default:
                return -EINVAL;
        }

        return 0;
}

static inline int ptr_unsigned_word_write(void *data, size_t word_sz, uint64_t val)
{
        switch (word_sz) {
        case 1:
                *(uint8_t *)data = (uint8_t)val;
                break;

        case 2:
                *(uint16_t *)data = (uint16_t)val;
                break;

        case 4:
                *(uint32_t *)data = (uint32_t)val;
                break;

        case 8:
                *(uint64_t *)data = val;
                break;

        default:
                return -EINVAL;
        }

        return 0;
}

static inline int ptr_unsigned_word_read(void *data, size_t word_sz, uint64_t *val)
{
        switch (word_sz) {
        case 1:
                *val = *(uint8_t *)data;
                break;

        case 2:
                *val = *(uint16_t *)data;
                break;

        case 4:
                *val = *(uint32_t *)data;
                break;

        case 8:
                *val = *(uint64_t *)data;
                break;

        default:
                return -EINVAL;
        }

        return 0;
}

// perform arithmetic extend
static inline int ptr_signed_word_read(void *data, size_t word_sz, int64_t *val)
{
        switch (word_sz) {
        case 1:
                *val = *(int8_t *)data;
                break;

        case 2:
                *val = *(int16_t *)data;
                break;

        case 4:
                *val = *(int32_t *)data;
                break;

        case 8:
                *val = *(int64_t *)data;
                break;

        default:
                return -EINVAL;
        }

        return 0;
}

int vprintf_resize(char **buf, size_t *pos, size_t *len, const char *fmt, va_list arg)
{
        va_list arg2;
        char cbuf;
        char *sbuf = *buf;
        int _len, ret;

        va_copy(arg2, arg);
        _len = vsnprintf(&cbuf, sizeof(cbuf), fmt, arg2);
        va_end(arg2);

        if (_len < 0)
                return _len;

        if (!sbuf) {
                size_t append_len = _len + 2;

                sbuf = (char *)calloc(append_len, sizeof(char));
                if (!sbuf)
                        return -ENOMEM;

                *buf = sbuf;
                *len = append_len;
                *pos = 0;
        } else {
                size_t append_len = _len + 2;
                size_t new_len = *len + append_len;

                // do realloc
                if (*pos + append_len > *len) {
                        sbuf = (char *)realloc_safe(*buf, *len, new_len);
                        if (!sbuf)
                                return -ENOMEM;

                        *buf = sbuf;
                        *len = new_len;
                }
        }

        sbuf = &((*buf)[*pos]);

        ret = vsnprintf(sbuf, *len - *pos, fmt, arg);
        if (ret < 0) {
                return ret;
        }

        *pos += ret;

        return ret;
}

int snprintf_resize(char **buf, size_t *pos, size_t *len, const char *fmt, ...)
{
        va_list ap;
        int ret;

        va_start(ap, fmt);
        ret = vprintf_resize(buf, pos, len, fmt, ap);
        va_end(ap);

        return ret;
}

#endif // __LIBJJ_UTLIS_H__