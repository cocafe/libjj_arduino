#ifndef __LIBJJ_JKEY_H__
#define __LIBJJ_JKEY_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "list.h"
#include "logging.h"
#include "cJSON.h"
#include "spiffs.h"
#include "json.h"

#define cJSON_Boolean                   (cJSON_False | cJSON_True)
#define cJSON_Compound                  (cJSON_Object | cJSON_Array)

#define JBUF_INIT_ALLOC_KEYS            (20)
#define JBUF_GROW_ARR_REALLOC_INCR      (5)

static uint8_t json_print_on_load = 0;

typedef struct json_key jkey_t;
typedef struct json_key_buf jbuf_t;
typedef int (*jbuf_traverse_cb)(jkey_t *jkey, int has_next, int depth, int argc, va_list arg);

enum json_key_types {
        JKEY_TYPE_UNKNOWN = 0,
        JKEY_TYPE_OBJECT,
        JKEY_TYPE_RO_ARRAY,
        JKEY_TYPE_FIXED_ARRAY,
        JKEY_TYPE_GROW_ARRAY,
        JKEY_TYPE_LIST_ARRAY,
        JKEY_TYPE_STRREF,
        JKEY_TYPE_STRBUF,
        JKEY_TYPE_STRPTR,
        JKEY_TYPE_BOOL,
        JKEY_TYPE_INT,
        JKEY_TYPE_UINT,
        JKEY_TYPE_DOUBLE,
        NUM_JKEY_TYPES,
};

struct json_key {
        const char             *key;
        unsigned                type;
        unsigned                cjson_type;

        struct {
                size_t          sz;                     // (int, strptr): @data.ref points to data block sz
                                                        // (strbuf): length of static allocated string buffer
                                                        // (strref): this should be 0

                void           *ref;                    // (data, obj): refer to data
                ssize_t         ref_offs;               // offset address of parent's ref
                uint8_t         ref_ptr;                // is @ref pointing to a pointer? (double pointer)
                uint8_t         ref_parent;             // is @ref referencing parent's @data.ref?
                                                        // data_key of array actually refs parent,
                                                        // but this flag is not set for data_key.
                uint8_t         ref_malloc;             // TODO: REMOVE?
                uint8_t         int_base;
                uint8_t         is_float;
        } data;

        struct {
                void           *base_ref;               // (arrptr, objptr): base ref address
                ssize_t         base_ref_offs;
                uint8_t         base_ref_ptr;           // is @base_ref pointing to a pointer?
                uint8_t         base_ref_malloc;        // TODO: REMOVE?
                uint8_t         base_ref_parent;        // is @base_ref referencing parent's @data.ref?
                                                        //                                   ^^^^^^^^^
                size_t          sz;
                union {
                        struct {
                                size_t  ele_cnt;
                        } fixed;
                        struct {
                                ssize_t offs_head;
                                ssize_t offs_data;
                                uint8_t head_inited;
                        } list;
                        struct {
                                size_t  alloc_cnt;      // allocated element count
                                size_t *ext_ele_cnt;    // element count for external iteration
                                ssize_t ext_ele_cnt_offs;       // extern counter offset of parent's @data.ref
                        } grow;
                } arr;
        } obj;

        struct {
                const char    **map;
                size_t          cnt;
        } strval;

        unsigned                child_cnt;
        uint8_t                 child[] __attribute__((aligned(8)));
};

struct json_key_buf {
        size_t          alloc_sz;
        void           *base;
        void           *head;
        void           *end;
};

int _jbuf_traverse_recursive(jkey_t *jkey,
                             jbuf_traverse_cb pre,
                             jbuf_traverse_cb post,
                             int has_next,
                             int depth,
                             int argc,
                             va_list arg);

static inline unsigned is_cjson_type(uint32_t a, uint32_t b)
{
        return a & b;
}

#define jbuf_offset_add(buf, type, key, offset)                         \
        do {                                                            \
                void *cookie = jbuf_##type##_add(buf, key, NULL);       \
                if (!cookie)                                            \
                        break;                                          \
                                                                        \
                jkey_ref_parent_set(buf, cookie, offset);               \
        } while (0)

#define jbuf_offset_obj_open(buf, key, offset)                         \
        ({                                                              \
                void *t = jbuf_obj_open(buf, key);                      \
                void *cookie = NULL;                                    \
                if (t) {                                                \
                        jkey_ref_parent_set(buf, t, offset);           \
                        cookie = t;                                     \
                }                                                       \
                                                                        \
                cookie;                                                 \
        })

#define jbuf_offset_objptr_open(buf, key, sz, offset)                  \
        ({                                                              \
                void *t = jbuf_objptr_open(buf, key, NULL, sz);        \
                void *cookie = NULL;                                    \
                if (t) {                                                \
                        jkey_base_ref_parent_set(buf, t, offset);      \
                        cookie = t;                                     \
                }                                                       \
                                                                        \
                cookie;                                                 \
        })

/*
#define jbuf_offset_objptr_open(buf, cookie, key, sz, offset)          \
        do {                                                            \
                void *t = jbuf_objptr_open(buf, key, NULL, sz);        \
                if (!t) {                                               \
                        cookie = NULL;                                  \
                        break;                                          \
                }                                                       \
                                                                        \
                jkey_base_ref_parent_set(buf, t, offset);              \
                cookie = t;                                             \
        } while (0)
*/

#define jbuf_array_data_key(buf, type) jbuf_offset_add(buf, type, NULL, 0)
#define jbuf_array_obj_data_key(buf, cookie) jbuf_offset_obj_open(buf, cookie, NULL, 0)

static uint32_t jkey_to_cjson_type[] = {
        [JKEY_TYPE_UNKNOWN]      = cJSON_Invalid,
        [JKEY_TYPE_OBJECT]       = cJSON_Object,
        [JKEY_TYPE_RO_ARRAY]     = cJSON_Array,
        [JKEY_TYPE_FIXED_ARRAY]  = cJSON_Array,
        [JKEY_TYPE_GROW_ARRAY]   = cJSON_Array,
        [JKEY_TYPE_LIST_ARRAY]   = cJSON_Array,
        [JKEY_TYPE_STRREF]       = cJSON_String,
        [JKEY_TYPE_STRBUF]       = cJSON_String,
        [JKEY_TYPE_STRPTR]       = cJSON_String,
        [JKEY_TYPE_BOOL]         = cJSON_Boolean,
        [JKEY_TYPE_INT]          = cJSON_Number,
        [JKEY_TYPE_UINT]         = cJSON_Number,
        [JKEY_TYPE_DOUBLE]       = cJSON_Number,
        [NUM_JKEY_TYPES]         = cJSON_Invalid,
};

char *jkey_type_strs[] = {
        [JKEY_TYPE_UNKNOWN]      = (char *)"unknown",
        [JKEY_TYPE_OBJECT]       = (char *)"object",
        [JKEY_TYPE_RO_ARRAY]     = (char *)"readonly_array",
        [JKEY_TYPE_FIXED_ARRAY]  = (char *)"fixed_array",
        [JKEY_TYPE_GROW_ARRAY]   = (char *)"grow_array",
        [JKEY_TYPE_LIST_ARRAY]   = (char *)"list_array",
        [JKEY_TYPE_STRREF]       = (char *)"string_ref",
        [JKEY_TYPE_STRBUF]       = (char *)"string_buf",
        [JKEY_TYPE_STRPTR]       = (char *)"string_ptr",
        [JKEY_TYPE_BOOL]         = (char *)"bool",
        [JKEY_TYPE_INT]          = (char *)"int",
        [JKEY_TYPE_UINT]         = (char *)"uint",
        [JKEY_TYPE_DOUBLE]       = (char *)"double",
};

static int is_jkey_writable_array(jkey_t *jkey)
{
        return is_cjson_type(jkey->cjson_type, cJSON_Array) &&
               jkey->type != JKEY_TYPE_RO_ARRAY;
}

static int is_jkey_compound(jkey_t *jkey)
{
        return is_cjson_type(jkey->cjson_type, cJSON_Compound);
}

static int is_jkey_ref_ptr(jkey_t *jkey)
{
        return jkey->data.ref_ptr || jkey->obj.base_ref_ptr;
}

static int is_jkey_ref_null(jkey_t *jkey)
{
        if (jkey->data.ref_ptr) {
                if (jkey->data.ref)
                        return (*(uint8_t **)jkey->data.ref == NULL) ? 1 : 0;
                else
                        return 1;
        }

        if (jkey->obj.base_ref_ptr) {
                if (jkey->obj.base_ref)
                        return (*(uint8_t **)jkey->obj.base_ref == NULL) ? 1 : 0;
                else
                        return 1;
        }

        return 0;
}

static int jbuf_grow(jbuf_t *b, size_t jk_cnt)
{
        size_t offset, new_sz;

        if (!b)
                return -EINVAL;

        if (!b->base)
                return -ENODATA;

        offset = (uint8_t *)b->head - (uint8_t *)b->base;
        new_sz = b->alloc_sz + (jk_cnt * sizeof(jkey_t));

        b->base = realloc_safe(b->base, b->alloc_sz, new_sz);
        if (!b->base)
                return -ENOMEM;

        b->alloc_sz = new_sz;
        b->head = (uint8_t *)b->base + offset;
        b->end  = (uint8_t *)b->base + b->alloc_sz;

        return 0;
}

static int jbuf_head_next(jbuf_t *b)
{
        int err = 0;

        if (!b)
                return -EINVAL;

        if (!b->base || !b->head)
                return -ENODATA;

        if ((uint8_t *)b->head + sizeof(jkey_t) > (uint8_t *)b->end)
                if ((err = jbuf_grow(b, 10)))
                        return err;

        b->head = (uint8_t *)b->head + sizeof(jkey_t);

        return 0;
}

int __jbuf_key_add(jbuf_t *b, uint32_t type, jkey_t **curr)
{
        jkey_t *k;
        size_t offset;
        int err;

        if (!b)
                return -EINVAL;

        if (!b->base || !b->head)
                return -ENODATA;

        if (type >= NUM_JKEY_TYPES)
                return -EINVAL;

        offset = (size_t)b->head - (size_t)b->base;

        if ((err = jbuf_head_next(b)))
                return err;

        k = (jkey_t *)((uint8_t *)b->base + offset);
        k->type = type;
        k->cjson_type = jkey_to_cjson_type[type];

        if (curr)
                *curr = k;

        return 0;
}

void *jbuf_key_add(jbuf_t *b, int type, const char *key, void *ref, size_t sz)
{
        jkey_t *k;
        int err;

        if ((err = __jbuf_key_add(b, type, &k))) {
                pr_err("err = %d\n", err);
                return NULL;
        }

        if (key && key[0] != '\0')
                k->key = key;

        k->data.ref = ref;
        k->data.sz = sz;

        return (void *)((uint8_t *)k - (uint8_t *)b->base);
}

jkey_t *jbuf_key_get(jbuf_t *b, void *cookie)
{
        return (jkey_t *)((uint8_t *)b->base + (size_t)cookie);
}

jkey_t *jbuf_root_key_get(jbuf_t *b)
{
        return (jkey_t *)b->base;
}

static jkey_t *jkey_array_data_key_get(jkey_t *arr)
{
        if (!is_cjson_type(arr->cjson_type, cJSON_Array))
                return NULL;

        if (arr->child_cnt == 0)
                return NULL;

        return &(((jkey_t *)arr->child)[0]);
}

static void jkey_strptr_set(jbuf_t *b, void *cookie)
{
        jkey_t *k = jbuf_key_get(b, cookie);
        k->data.sz = 0;
        k->data.ref_ptr = 1;
        k->data.ref_malloc = 1;
}

void jkey_int_base_set(jbuf_t *b, void *cookie, uint8_t int_base)
{
        jkey_t *k;

        k = jbuf_key_get(b, cookie);
        k->data.int_base = int_base;
        k->cjson_type = cJSON_String;
}

void jkey_ref_parent_set(jbuf_t *b, void *cookie, ssize_t offset)
{
        jkey_t *k;

        k = jbuf_key_get(b, cookie);
        k->data.ref_offs = offset;
        k->data.ref_parent = 1;
}

void jkey_base_ref_parent_set(jbuf_t *b, void *cookie, ssize_t offset)
{
        jkey_t *k;

        k = jbuf_key_get(b, cookie);
        k->obj.base_ref_offs = offset;
        k->obj.base_ref_parent = 1;
}

void *jbuf_obj_open(jbuf_t *b, const char *key)
{
        return jbuf_key_add(b, JKEY_TYPE_OBJECT, key, NULL, 0);
}

void *jbuf_objptr_open(jbuf_t *b, const char *key, void *ref, size_t sz)
{
        jkey_t *k;
        void *cookie = jbuf_key_add(b, JKEY_TYPE_OBJECT, key, NULL, sz);
        if (!cookie)
                return NULL;

        k = jbuf_key_get(b, cookie);
        k->obj.base_ref = ref;
        k->obj.base_ref_ptr = 1;
        k->obj.base_ref_malloc = 1;

        return cookie;
}

void jbuf_obj_close(jbuf_t *b, void *cookie)
{
        jkey_t *k = (jkey_t *)((size_t)b->base + (size_t)cookie);
        size_t len = ((size_t)b->head - (size_t)k - sizeof(jkey_t));

        k->child_cnt = len / sizeof(jkey_t);
}

void *jbuf_arr_open(jbuf_t *b, const char *key)
{
        return jbuf_key_add(b, JKEY_TYPE_RO_ARRAY, key, NULL, 0);
}

void *jbuf_fixed_arr_open(jbuf_t *b, const char *key)
{
        return jbuf_key_add(b, JKEY_TYPE_FIXED_ARRAY, key, NULL, 0);
}

void *jbuf_grow_arr_open(jbuf_t *b, const char *key)
{
        return jbuf_key_add(b, JKEY_TYPE_GROW_ARRAY, key, NULL, 0);
}

void *jbuf_list_arr_open(jbuf_t *b, const char *key)
{
        return jbuf_key_add(b, JKEY_TYPE_LIST_ARRAY, key, NULL, 0);
}

void jbuf_arr_close(jbuf_t *b, void *cookie)
{
        return jbuf_obj_close(b, cookie);
}

void __jbuf_fixed_arr_setup(jbuf_t *b, void *cookie, void *ref, size_t ele_cnt, size_t ele_sz, int base_ref_ptr)
{
        jkey_t *k = jbuf_key_get(b, cookie);

        k->obj.base_ref                 = ref;
        k->obj.sz                       = ele_sz;
        k->obj.arr.fixed.ele_cnt        = ele_cnt;

        if (base_ref_ptr) {
                k->obj.base_ref_ptr     = 1;
                k->obj.base_ref_malloc  = 1;
        }
}

#define jbuf_fixed_arr_setup(_b, _cookie, _ref) \
        _jbuf_fixed_arr_setup(_b, _cookie, _ref, ARRAY_SIZE(_ref), sizeof((_ref)[0]))

void _jbuf_fixed_arr_setup(jbuf_t *b, void *cookie, void *ref, size_t ele_cnt, size_t ele_sz)
{
        __jbuf_fixed_arr_setup(b, cookie, ref, ele_cnt, ele_sz, 0);
}

void jbuf_fixed_arrptr_setup(jbuf_t *b, void *cookie, void **ref, size_t ele_cnt, size_t ele_sz)
{
        __jbuf_fixed_arr_setup(b, cookie, ref, ele_cnt, ele_sz, 1);
}

void jbuf_offset_fixed_arr_setup(jbuf_t *b, void *cookie, ssize_t offset, size_t ele_cnt, size_t ele_sz)
{
        __jbuf_fixed_arr_setup(b, cookie, NULL, ele_cnt, ele_sz, 0);
        jkey_base_ref_parent_set(b, cookie, offset);
}

void jbuf_grow_arr_setup(jbuf_t *b, void *cookie, void **ref, size_t *ext_ele_cnt, size_t ele_sz)
{
        jkey_t *k = jbuf_key_get(b, cookie);

        k->data.ref                     = NULL;
        k->obj.base_ref                 = ref;
        k->obj.base_ref_ptr             = 1;
        k->obj.base_ref_malloc          = 1;
        k->obj.sz                       = ele_sz;
        k->obj.arr.grow.alloc_cnt       = 0;
        k->obj.arr.grow.ext_ele_cnt     = ext_ele_cnt;
}

void jbuf_offset_grow_arr_setup(jbuf_t *b, void *cookie, ssize_t offset, ssize_t ext_ele_cnt_offs, size_t ele_sz)
{
        jkey_t *k = jbuf_key_get(b, cookie);

        jbuf_grow_arr_setup(b, cookie, NULL, NULL, ele_sz);
        jkey_base_ref_parent_set(b, cookie, offset);

        k->obj.arr.grow.ext_ele_cnt_offs = ext_ele_cnt_offs;
}

/**
 * jbuf_list_arr_setup() - describe list array, open array first
 *
 * @param b: jbuf
 * @param cookie: list array cookie
 * @param head: pointer to external list_head, the entry point of list
 * @param ctnr_sz: size of container which holds sub list_head to be allocated
 * @param offsof_ctnr_head: offset of list_head in container
 * @param ctnr_data_sz: size of data to write in container, for data key
 * @param offsof_ctnr_data: offset of data in container to write, for data key
 */
void jbuf_list_arr_setup(jbuf_t *b,
                         void *cookie,
                         struct list_head *head,
                         size_t ctnr_sz,
                         ssize_t offsof_ctnr_head,
                         size_t ctnr_data_sz,
                         ssize_t offsof_ctnr_data)
{
        jkey_t *k = jbuf_key_get(b, cookie);

        k->data.ref                     = NULL;
        k->data.sz                      = ctnr_data_sz;
        k->obj.base_ref                 = head;
        k->obj.base_ref_ptr             = 0;
        k->obj.base_ref_malloc          = 0;
        k->obj.sz                       = ctnr_sz;
        k->obj.arr.list.offs_head       = offsof_ctnr_head;
        k->obj.arr.list.offs_data       = offsof_ctnr_data;
        k->obj.arr.list.head_inited     = 0;

        if (head) {
                INIT_LIST_HEAD(head);
                k->obj.arr.list.head_inited = 1;
        }
}

void jbuf_offset_list_arr_setup(jbuf_t *b,
                                void *cookie,
                                ssize_t offsof_head_in_parent,
                                size_t ctnr_sz,
                                ssize_t offsof_ctnr_head,
                                size_t ctnr_data_sz,
                                ssize_t offsof_ctnr_data)
{
        jbuf_list_arr_setup(b, cookie, NULL, ctnr_sz, offsof_ctnr_head, ctnr_data_sz, offsof_ctnr_data);
        jkey_base_ref_parent_set(b, cookie, offsof_head_in_parent);
}

// external ref (read) only char*
void *jbuf_strref_add(jbuf_t *b, const char *key, char *ref)
{
        return jbuf_key_add(b, JKEY_TYPE_STRREF, key, ref, 0);
}

#define jbuf_strbuf_add(_b, _key, _ref) __jbuf_strbuf_add(_b, _key, _ref, sizeof(_ref))

// external static size char[] buf
void *__jbuf_strbuf_add(jbuf_t *b, const char *key, char *ref, size_t bytes)
{
        return jbuf_key_add(b, JKEY_TYPE_STRBUF, key, ref, bytes);
}

// alloc internally char* for input
void *jbuf_strptr_add(jbuf_t *b, const char *key, char **ref)
{
        void *cookie;

        cookie = jbuf_key_add(b, JKEY_TYPE_STRPTR, key, ref, 0);
        if (!cookie)
                return NULL;

        jkey_strptr_set(b, cookie);

        return cookie;
}

#define jbuf_strval_add(_b, _key, _ref, _strmap) \
        __jbuf_strval_add(_b, _key, &(_ref), sizeof(_ref), _strmap, ARRAY_SIZE(_strmap))

void *__jbuf_strval_add(jbuf_t *b, const char *key, void *ref, size_t refsz, const char *map[], size_t map_cnt)
{
        jkey_t *k;
        void *cookie = jbuf_key_add(b, JKEY_TYPE_UINT, key, ref, refsz);
        if (!cookie)
                return NULL;

        k = jbuf_key_get(b, cookie);
        k->strval.map = map;
        k->strval.cnt = map_cnt;
        k->cjson_type = cJSON_String;

        return cookie;
}

#define jbuf_u8_add(_b, _key, _ref)     __jbuf_uint_add(_b, _key, &(_ref), sizeof(uint8_t))
#define jbuf_u16_add(_b, _key, _ref)    __jbuf_uint_add(_b, _key, &(_ref), sizeof(uint16_t))
#define jbuf_u32_add(_b, _key, _ref)    __jbuf_uint_add(_b, _key, &(_ref), sizeof(uint32_t))
#define jbuf_u64_add(_b, _key, _ref)    __jbuf_uint_add(_b, _key, &(_ref), sizeof(uint64_t))

#define jbuf_uint_add(_b, _key, _ref) \
        __jbuf_uint_add(_b, _key, &(_ref), sizeof(_ref))

void *__jbuf_uint_add(jbuf_t *b, const char *key, void *ref, size_t refsz)
{
        return jbuf_key_add(b, JKEY_TYPE_UINT, key, ref, refsz);
}

#define jbuf_s8_add(_b, _key, _ref)     __jbuf_sint_add(_b, _key, &(_ref), sizeof(int8_t))
#define jbuf_s16_add(_b, _key, _ref)    __jbuf_sint_add(_b, _key, &(_ref), sizeof(int16_t))
#define jbuf_s32_add(_b, _key, _ref)    __jbuf_sint_add(_b, _key, &(_ref), sizeof(int32_t))
#define jbuf_s64_add(_b, _key, _ref)    __jbuf_sint_add(_b, _key, &(_ref), sizeof(int64_t))

#define jbuf_sint_add(_b, _key, _ref) \
        __jbuf_sint_add(_b, _key, &(_ref), sizeof(_ref))

void *__jbuf_sint_add(jbuf_t *b, const char *key, void *ref, size_t refsz)
{
        return jbuf_key_add(b, JKEY_TYPE_INT, key, ref, refsz);
}

#define jbuf_float_add(_b, _key, _ref) \
        __jbuf_float_add(_b, _key, &_ref)

void *__jbuf_float_add(jbuf_t *b, const char *key, float *ref)
{
        void *cookie = jbuf_key_add(b, JKEY_TYPE_DOUBLE, key, ref, sizeof(double));
        jkey_t *k = jbuf_key_get(b, cookie);

        k->data.is_float = 1;

        return cookie;
}

#define jbuf_double_add(_b, _key, _ref) \
        __jbuf_double_add(_b, _key, &_ref)

void *__jbuf_double_add(jbuf_t *b, const char *key, double *ref)
{
        return jbuf_key_add(b, JKEY_TYPE_DOUBLE, key, ref, sizeof(double));
}

#define jbuf_bool_add(_b, _key, _ref) \
        __jbuf_bool_add(_b, _key, &(_ref), sizeof(_ref))

void *__jbuf_bool_add(jbuf_t *b, const char *key, void *ref, size_t refsz)
{
        return jbuf_key_add(b, JKEY_TYPE_BOOL, key, ref, refsz);
}

#define jbuf_hex_uint_add(_b, _key, _ref)                       \
        do {                                                    \
                void *cookie = jbuf_uint_add(_b, _key, _ref);   \
                if (!cookie)                                    \
                        break;                                  \
                                                                \
                jkey_int_base_set(_b, cookie, 16);              \
        } while (0)

#define jbuf_hex_sint_add(_b, _key, _ref)                       \
        do {                                                    \
                void *cookie = jbuf_sint_add(_b, _key, _ref);   \
                if (!cookie)                                    \
                        break;                                  \
                                                                \
                jkey_int_base_set(_b, cookie, 16);              \
        } while (0)

#define jbuf_offset_strbuf_add(_b, _key, _container, _member) \
        __jbuf_offset_strbuf_add(_b, _key, offsetof(_container, _member), sizeof_member(_container, _member))

void *__jbuf_offset_strbuf_add(jbuf_t *b, const char *key, ssize_t offset, size_t bytes)
{
        void *cookie = __jbuf_strbuf_add(b, key, NULL, bytes);
        if (!cookie)
                return NULL;

        jkey_ref_parent_set(b, cookie, offset);

        return cookie;
}

#define jbuf_offset_strval_add(_b, _key, _container, _member, _strmap) \
        __jbuf_offset_strval_add(_b, _key, offsetof(_container, _member), sizeof_member(_container, _member), _strmap, ARRAY_SIZE(_strmap))

void *__jbuf_offset_strval_add(jbuf_t *b, const char *key, ssize_t offset, size_t refsz, const char *map[], size_t map_cnt)
{
        void *cookie = __jbuf_strval_add(b, key, NULL, refsz, map, map_cnt);
        if (!cookie)
                return NULL;

        jkey_ref_parent_set(b, cookie, offset);

        return cookie;
}

static inline void jkey_data_ptr_deref(jkey_t *jkey, void **out, size_t new_sz)
{
        *out = NULL;

        if (jkey->data.ref_ptr) {
                void *data_ref = *(uint8_t **)jkey->data.ref;

                if (NULL == jkey->data.ref)
                        return;

                if (NULL == data_ref && jkey->data.ref_malloc) {
                        size_t data_sz = jkey->data.sz;

                        if (new_sz)
                                data_sz = new_sz;

                        if (data_sz == 0)
                                return;

                        data_ref = calloc(1, data_sz);
                        if (NULL == data_ref)
                                return;

                        if (new_sz)
                                jkey->data.sz = new_sz;

                        *(uint8_t **)jkey->data.ref = (uint8_t *)data_ref;
                }

                *out = *(uint8_t **)jkey->data.ref;

                return;
        }

        // @data.ref points to actual data
        *out = jkey->data.ref;
}

static int jkey_bool_write(jkey_t *jkey, cJSON *node)
{
        uint32_t val = (node->type == cJSON_True) ? 1 : 0;
        void *dst = NULL;

        jkey_data_ptr_deref(jkey, &dst, 0);
        if (!dst) {
                pr_err("data pointer is NULL\n");
                return -ENODATA;
        }

        return ptr_word_write(dst, jkey->data.sz, val);
}

static int jkey_int_write(jkey_t *jkey, cJSON *node)
{
        double val = cJSON_GetNumberValue(node);
        void *dst = NULL;

        if (isnan(val)) {
                pr_err("key [%s] failed to get number from cJSON\n", nullstr_guard(jkey->key));
                return -EFAULT;
        }

        if (jkey->data.sz == 0) {
                pr_err("data size %zu of key [%s] failed sanity check\n", jkey->data.sz, nullstr_guard(jkey->key));
                return -EFAULT;
        }

        jkey_data_ptr_deref(jkey, &dst, 0);
        if (!dst) {
                pr_err("data pointer is NULL\n");
                return -ENODATA;
        }

        return ptr_word_write(dst, jkey->data.sz, (int64_t)val);
}

static int jkey_float_write(jkey_t *jkey, cJSON *node)
{
        double val = cJSON_GetNumberValue(node);
        void *dst = NULL;

        if (isnan(val)) {
                pr_err("key [%s] failed to get number from cJSON\n", nullstr_guard(jkey->key));
                return -EFAULT;
        }

        if (jkey->data.sz != sizeof(double)) {
                pr_err("data size %zu of key [%s] failed sanity check\n", jkey->data.sz, nullstr_guard(jkey->key));
                return -EFAULT;
        }

        jkey_data_ptr_deref(jkey, &dst, 0);
        if (!dst) {
                pr_err("data pointer is NULL\n");
                return -ENODATA;
        }

        if (jkey->data.is_float) {
                *(float *)dst = (float)val;
        } else {
#ifdef __ARM_ARCH_7A__
                if (likely((size_t)dst % sizeof(double))) {
                        *(double *)dst = val;
                } else {
                        pr_info("float point unaligned access detected, fix your struct\n");
                        memcpy(dst, &(double){ val }, sizeof(double));
                }
#else
                *(double *)dst = val;
#endif
        }

        return 0;
}

static int strval_map_to_int(void *dst, size_t dst_sz, char *src, char **map, size_t map_sz)
{
        size_t src_len;

        src_len = strlen(src);

        for (uint64_t i = 0; i < map_sz; i++) {
                char *item = map[i];
                size_t item_len = strlen(item);

                if (src_len != item_len)
                        continue;

                if (!strncasecmp(item, src, __min(item_len, src_len))) {
                        return ptr_word_write(dst, dst_sz, (int64_t)i);
                }
        }

        pr_err("cannot convert string \'%s\' to value\n", src);

        return -ENOENT;
}

static inline int jkey_is_strptr(jkey_t *jkey)
{
        return jkey->type == JKEY_TYPE_STRPTR;
}

static int jkey_string_write(jkey_t *jkey, cJSON *node)
{
        char *json_str = cJSON_GetStringValue(node);
        size_t json_len, alloc_sz = 0;
        void *dst = NULL;
        int err = 0;

        if (!json_str) {
                pr_err("key [%s] failed to get string from cJSON\n", nullstr_guard(jkey->key));
                return -EFAULT;
        }

        json_len = strlen(json_str);

        if (json_str[0] == '\0') {
                pr_verbose("key [%s] got empty string\n", nullstr_guard(jkey->key));
                return 0;
        }

        // + 2 = not always copy precisely
        // had better keep extra spaces for wchar_t
        if (jkey_is_strptr(jkey)) {
                alloc_sz = json_len + 2;
        }

        jkey_data_ptr_deref(jkey, &dst, alloc_sz);
        if (!dst) {
                pr_err("data pointer is NULL\n");
                return -ENODATA;
        }

        switch (jkey->type) {
        case JKEY_TYPE_INT:
        case JKEY_TYPE_UINT:
                if (jkey->data.int_base) {
                        uint64_t t = strtoull(json_str, NULL, jkey->data.int_base);
                        return ptr_word_write(dst, jkey->data.sz, t);
                } else if (jkey->strval.map) {
                        return strval_map_to_int(dst, jkey->data.sz, json_str,
                                                 (char **)jkey->strval.map,
                                                 jkey->strval.cnt);
                } else {
                        pr_err("cannot convert string to number for key [%s]\n", nullstr_guard(jkey->key));
                        return -EINVAL;
                }

                break;

        // FIXME: JKEY_TYPE_STRREF: ??
        case JKEY_TYPE_STRBUF:
        case JKEY_TYPE_STRPTR:
                if (jkey->data.sz == 0) {
                        pr_err("key [%s] data size is not inited\n", nullstr_guard(jkey->key));
                        return -EINVAL;
                }

                {
                        size_t len = json_len;

                        if (jkey->type == JKEY_TYPE_STRBUF) {
                                if (jkey->data.sz < json_len) {
                                        pr_info("jkey [%s] cannot hold json string\n", nullstr_guard(jkey->key));
                                        len = jkey->data.sz - 1;
                                }
                        }

                        // jkey->data.sz is allocated above, always > json_len
                        strncpy((char *)dst, json_str, len);
                        ((char *)dst)[len] = '\0';
                }

                break;

        default:
                pr_err("invalid jkey type: %d, key [%s]\n", jkey->type, nullstr_guard(jkey->key));
                return -EINVAL;
        }

        return err;
}

int jkey_value_write(jkey_t *jkey, cJSON *node)
{
        int err;

        switch (node->type) {
        case cJSON_True:
        case cJSON_False:
                err = jkey_bool_write(jkey, node);
                break;

        case cJSON_String:
                err = jkey_string_write(jkey, node);
                break;

        case cJSON_Number:
                switch (jkey->type) {
                case JKEY_TYPE_DOUBLE:
                        err = jkey_float_write(jkey, node);
                        break;

                case JKEY_TYPE_INT:
                case JKEY_TYPE_UINT:
                        err = jkey_int_write(jkey, node);
                        break;

                default:
                        err = -EFAULT;
                        break;
                }
                break;

        default:
                pr_err("unsupported cjson type %u for key [%s]\n", node->type, nullstr_guard(jkey->key));
                err = -EINVAL;
                break;
        }

        return err;
}

int jkey_cjson_input(jkey_t *jkey, cJSON *node)
{
        if (!jkey || !node)
                return 0;

        if (is_jkey_compound(jkey))
                return 0;

        if (!jkey->data.ref) {
                pr_dbg("key [%s] data ref is NULL\n", nullstr_guard(jkey->key));
                return 0;
        }

        if (0 == is_cjson_type(jkey->cjson_type, node->type)) {
                pr_dbg("key [%s] node type mismatched\n", nullstr_guard(jkey->key));
                return 0;
        }

        return jkey_value_write(jkey, node);
}

static int is_jkey_cjson_node_match(jkey_t *jkey, cJSON *node)
{
        int jkey_named = jkey->key ? 1 : 0;
        int node_named = node->string ? 1 : 0;

        if (0 == is_cjson_type(jkey->cjson_type, node->type))
                return 0;

        if (0 != (jkey_named ^ node_named))
                return 0;

        if (jkey_named && node_named) {
                size_t strlen_jkey = strlen(jkey->key);
                size_t strlen_node = strlen(node->string);

                if (strlen_jkey != strlen_node)
                        return 0;

                if (0 != strncmp(jkey->key,
                                 node->string,
                                 strlen_jkey))
                        return 0;
        }

        return 1; // matched
}

static int jkey_array_key_check(jkey_t *arr)
{
        if (arr->type != JKEY_TYPE_FIXED_ARRAY &&
            arr->type != JKEY_TYPE_GROW_ARRAY &&
            arr->type != JKEY_TYPE_LIST_ARRAY)
                return -ECANCELED;

        if (arr->child_cnt == 0) {
                pr_err("array key [%s] does not have any child keys to parse itself\n", nullstr_guard(arr->key));
                return -EINVAL;
        }

        if (arr->obj.base_ref == NULL) {
                pr_info("array key [%s] did not define data reference\n", nullstr_guard(arr->key));
                return -EINVAL;
        }

        if (arr->obj.sz == 0) {
                pr_err("array key [%s] element size is 0\n", nullstr_guard(arr->key));
                return -EINVAL;
        }

        return 0;
}

static int jkey_array_data_key_check(jkey_t *arr_key, jkey_t *data_key)
{
        if (!data_key->data.ref_parent && !data_key->obj.base_ref_parent) {
                pr_err("array [%s] data key should ref its parent\n", nullstr_guard(arr_key->key));
                return -EINVAL;
        }

        return 0;
}

static int jkey_base_ref_alloc(jkey_t *jkey, size_t base_sz)
{
        void *base_ref;

        if (!jkey->obj.base_ref ||
            !jkey->obj.base_ref_ptr ||
            !jkey->obj.base_ref_malloc)
                return 0;

        if (!base_sz) {
                pr_err("key [%s] allocate size is 0\n", nullstr_guard(jkey->key));
                return -EINVAL;
        }

        base_ref = *((uint8_t **)jkey->obj.base_ref);

        if (base_ref != NULL) {
                return 0;
        }

        base_ref = calloc(1, base_sz);
        if (!base_ref)
                return -ENOMEM;

        *((uint8_t **)jkey->obj.base_ref) = (uint8_t *)base_ref;

        return 0;
}

static int jkey_fixed_array_alloc(jkey_t *arr) {
        int err;
        size_t base_sz = arr->obj.arr.fixed.ele_cnt * arr->obj.sz;

        if (!arr->obj.arr.fixed.ele_cnt) {
                pr_err("array [%s] did not define max element cnt\n", nullstr_guard(arr->key));
                return -EINVAL;
        }

        err = jkey_base_ref_alloc(arr, base_sz);
        if (err == -ENOMEM) {
                pr_err("array [%s] failed to allocate %zu bytes\n",
                       arr->key, base_sz);
                return err;
        }

        return err;
}

static int jkey_fixed_grow_array_ref_update(jkey_t *arr_key, jkey_t *data_key, size_t idx)
{
        size_t idx_offset = arr_key->obj.sz * idx;
        void *base_ref = arr_key->obj.base_ref;

        if (0 == arr_key->obj.sz) {
                pr_err("array [%s] invalid element size\n", nullstr_guard(arr_key->key));
                return -EINVAL;
        }

        if (arr_key->obj.base_ref_ptr)
                base_ref = *((uint8_t **)arr_key->obj.base_ref);

        if (base_ref == NULL) {
                pr_dbg("array [%s] points to NULL\n", nullstr_guard(arr_key->key));
                return -ENODATA;
        }

        if (data_key->data.ref_parent)
                data_key->data.ref = (uint8_t *)base_ref + idx_offset;

        if (data_key->obj.base_ref_parent)
                data_key->obj.base_ref = (uint8_t *)base_ref + idx_offset;

        return 0;
}

static int jkey_grow_array_realloc(jkey_t *arr, size_t idx)
{
        size_t need_alloc = arr->obj.arr.grow.alloc_cnt + JBUF_GROW_ARR_REALLOC_INCR;
        size_t new_sz = need_alloc * arr->obj.sz;
        void *base_ref;

        if (idx < arr->obj.arr.grow.alloc_cnt) {
                return 0;
        }

        if (!arr->obj.base_ref_ptr || !arr->obj.base_ref || !arr->obj.sz) {
                pr_err("invalid grow array [%s]\n", nullstr_guard(arr->key));
                return -EINVAL;
        }

        base_ref = *(uint8_t **)arr->obj.base_ref;
        if (base_ref == NULL) {
                if (!arr->obj.base_ref_malloc) {
                        pr_err("array [%s] refer NULL pointer and not do_malloc\n", nullstr_guard(arr->key));
                        return -EINVAL;
                }

                base_ref = calloc(1, new_sz);
                if (!base_ref) {
                        pr_err("array [%s] failed to allocate %zu bytes\n", nullstr_guard(arr->key), new_sz);
                        return -ENOMEM;
                }

        } else { // if in management routine, do realloc()
                size_t *extern_ele_cnt = arr->obj.arr.grow.ext_ele_cnt;
                size_t old_sz = (*extern_ele_cnt) * arr->obj.sz;

                base_ref = realloc_safe(base_ref, old_sz, new_sz);
                if (!base_ref) {
                        pr_err("failed to realloc() array [%s] data\n", nullstr_guard(arr->key));
                        return -ENOMEM;
                }
        }

        arr->obj.arr.grow.alloc_cnt = need_alloc;
        *(uint8_t **)arr->obj.base_ref = (uint8_t *)base_ref;

        return 0;
}

static int jkey_grow_array_cnt_incr(jkey_t *arr)
{
        size_t *extern_ele_cnt = arr->obj.arr.grow.ext_ele_cnt;

        if (!extern_ele_cnt) {
                pr_err("grow array key [%s] did not define extern element counter\n", nullstr_guard(arr->key));
                return -EINVAL;
        }

        (*extern_ele_cnt)++;

        return 0;
}

static int jkey_list_array_alloc(jkey_t *arr_key, void **container)
{
        struct list_head *head = (struct list_head *)arr_key->obj.base_ref;
        struct list_head *node = NULL;
        void *new_ctnr;

        if (!head) {
                pr_err("list array [%s] has NULL list_head\n", nullstr_guard(arr_key->key));
                return -EINVAL;
        }

        if (arr_key->obj.sz == 0) {
                pr_err("list array [%s] container size is 0\n", nullstr_guard(arr_key->key));
                return -EINVAL;
        }

        new_ctnr = calloc(1, arr_key->obj.sz);
        if (!new_ctnr) {
                pr_err("list array [%s] failed to allocate %zu bytes\n", nullstr_guard(arr_key->key), arr_key->obj.sz);
                return -ENOMEM;
        }

        node = (struct list_head *)((uint8_t *)new_ctnr + arr_key->obj.arr.list.offs_head);
        INIT_LIST_HEAD(node);

        list_add_tail(node, head);

        if (container)
                *container = new_ctnr;

        return 0;
}

static int jkey_list_array_ref_update(jkey_t *arr_key, jkey_t *data_key, void *container)
{
        if (data_key->data.ref_parent)
                data_key->data.ref = (uint8_t *)container + arr_key->obj.arr.list.offs_data;

        if (data_key->obj.base_ref_parent)
                data_key->obj.base_ref = (uint8_t *)container + arr_key->obj.arr.list.offs_data;

        return 0;
}

static int jkey_obj_key_ref_update(jkey_t *jkey)
{
        if (jkey->obj.base_ref && jkey->obj.base_ref_ptr)
                jkey->data.ref = *(uint8_t **)jkey->obj.base_ref;

        return 0;
}

static int jkey_child_key_ref_update(jkey_t *parent)
{
        jkey_t *child;

        if (!parent->data.ref)
                return 0;

        for (size_t i = 0; i < parent->child_cnt; i++) {
                child = &((jkey_t *)parent->child)[i];

                if (is_jkey_compound(child)) {
                        if (child->child_cnt)
                                i += child->child_cnt;
                }

                if (child->data.ref_parent)
                        child->data.ref = (uint8_t *)parent->data.ref + child->data.ref_offs;

                if (child->obj.base_ref_parent)
                        child->obj.base_ref = (uint8_t *)parent->data.ref + child->obj.base_ref_offs;
        }

        return 0;
}

static int jkey_child_arr_key_update(jkey_t *parent)
{
        jkey_t *child;

        for (size_t i = 0; i < parent->child_cnt; i++) {
                child = &((jkey_t *)parent->child)[i];

                if (is_jkey_compound(child)) {
                        if (child->child_cnt)
                                i += child->child_cnt;
                }

                if (!is_jkey_writable_array(child))
                        continue;

                if (!child->obj.base_ref_parent)
                        continue;

                switch (child->type) {
                case JKEY_TYPE_GROW_ARRAY:
                {
                        ssize_t ext_ele_cnt_offs = child->obj.arr.grow.ext_ele_cnt_offs;
                        child->obj.arr.grow.ext_ele_cnt = (size_t *)((intptr_t)parent->data.ref + ext_ele_cnt_offs);
                        child->obj.arr.grow.alloc_cnt = 0;

                        break;
                }

                case JKEY_TYPE_LIST_ARRAY:
//                        child->obj.base_ref = NULL;
                        child->obj.arr.list.head_inited = 0;

                        break;

                }
        }

        return 0;
}

static jkey_t *jkey_child_key_find(jkey_t *parent, cJSON *child_node)
{
        for (size_t i = 0; i < parent->child_cnt; i++) {
                jkey_t *k = &((jkey_t *)parent->child)[i];

                if (is_jkey_cjson_node_match(k, child_node))
                        return k;

                if (is_jkey_compound(k)) {
                        if (k->child_cnt)
                                i += k->child_cnt;
                }
        }

        return NULL;
}

static void cjson_node_print(cJSON *node, int depth, const size_t *arr_idx)
{
        if (!json_print_on_load)
                return;

        // \0 is padded with [] declaration
        if (depth >= (sizeof(json_indent) - 1))
                depth = sizeof(json_indent);

        printf("%.*s", depth, json_indent);

        if (arr_idx)
                printf("[%zu] ", *arr_idx);
        else if (node->string)
                printf("\"%s\" ", nullstr_guard(node->string));

        switch (node->type) {
        case cJSON_False:
                printf(": false");
                break;
        case cJSON_True:
                printf(": true");
                break;
        case cJSON_NULL:
                printf("[null]");
                break;
        case cJSON_Number: {
                printf("[number]");

                double num = cJSON_GetNumberValue(node);

                // does number have fraction part
                if (rint(num) != num)
                        printf(" : %.2f", num);
                else
                        printf(" : %.0f", num);

                break;
        }
        case cJSON_String:
                printf("[string]");
                printf(" : \"%s\"", cJSON_GetStringValue(node));
                break;
        case cJSON_Array:
                printf("[array]");
                break;
        case cJSON_Object:
                printf("[object]");
                break;
        case cJSON_Raw:
                printf("[raws]");
                break;
        }

        printf("\n");
}

int jkey_cjson_load_recursive(jkey_t *jkey, cJSON *node, int depth)
{
        cJSON *child_node = NULL;
        int err = 0;


        if (!jkey || !node)
                return 0;

        if (!is_jkey_cjson_node_match(jkey, node))
                return 0;

        if (is_cjson_type(node->type, cJSON_Array)) {
                jkey_t *arr_key = jkey;
                jkey_t *data_key = NULL;
                size_t i = 0;

                if ((err = jkey_array_key_check(arr_key)))
                        return err == -ECANCELED ? 0 : err;

                // always take the first child,
                // since mono-type array is only supported
                data_key = &((jkey_t *)arr_key->child)[0];

                if ((err = jkey_array_data_key_check(arr_key, data_key)))
                        return err;

                // always init list array head
                // since list_empty() needs inited head
                if (arr_key->type == JKEY_TYPE_LIST_ARRAY) {
                        struct list_head *head = (struct list_head *)arr_key->obj.base_ref;

                        if (!arr_key->obj.arr.list.head_inited) {
                                INIT_LIST_HEAD(head);
                                arr_key->obj.arr.list.head_inited = 1;
                        }
                }

                cJSON_ArrayForEach(child_node, node) {
                        cjson_node_print(child_node, depth + 1, &i);

                        if (!is_cjson_type(data_key->cjson_type, child_node->type)) {
                                pr_dbg("array [%s] child node #%zu type mismatched, ignored\n", nullstr_guard(node->string), i);
                                continue;
                        }

                        if ((err = jkey_child_arr_key_update(arr_key)))
                                return err;

                        if (arr_key->type == JKEY_TYPE_FIXED_ARRAY) {
                                if (i >= arr_key->obj.arr.fixed.ele_cnt) {
                                        pr_info("array [%s] input exceeds max element count allocated\n", nullstr_guard(node->string));
                                        break;
                                }

                                if ((err = jkey_fixed_array_alloc(arr_key)))
                                        return err;

                                if ((err = jkey_fixed_grow_array_ref_update(arr_key, data_key, i)))
                                        return err;
                        } else if (arr_key->type == JKEY_TYPE_GROW_ARRAY) {
                                if ((err = jkey_grow_array_realloc(arr_key, i)))
                                        return err;

                                if ((err = jkey_grow_array_cnt_incr(arr_key)))
                                        return err;

                                if ((err = jkey_fixed_grow_array_ref_update(arr_key, data_key, i)))
                                        return err;
                        } else if (arr_key->type == JKEY_TYPE_LIST_ARRAY) {
                                void *container;

                                if ((err = jkey_list_array_alloc(arr_key, &container)))
                                        return err;

                                jkey_list_array_ref_update(arr_key, data_key, container);
                        }

                        if (is_jkey_compound(data_key)) {
                                if ((err = jkey_cjson_load_recursive(data_key, child_node, depth + 1)))
                                        return err;
                        } else {
                                err = jkey_cjson_input(data_key, child_node);
                                if (err) {
                                        pr_err("failed to parse #%zu item of array [%s]\n", i, nullstr_guard(node->string));
                                        return err;
                                }
                        }

                        i++;
                }

                return 0;
        }

        if (is_cjson_type(node->type, cJSON_Object)) {
                if ((err = jkey_base_ref_alloc(jkey, jkey->data.sz)))
                        return err;

                if ((err = jkey_obj_key_ref_update(jkey)))
                        return err;

                if ((err = jkey_child_key_ref_update(jkey)))
                        return err;

                if ((err = jkey_child_arr_key_update(jkey)))
                        return err;

                cJSON_ArrayForEach(child_node, node) {
                        jkey_t *child_key = NULL;

                        cjson_node_print(child_node, depth + 1, NULL);

                        //
                        // XXX: O(n^) WARNING!
                        //
                        // is_jkey_cjson_node_match() inside
                        //
                        child_key = jkey_child_key_find(jkey, child_node);
                        if (!child_key) {
                                pr_dbg("child key [%s] is not found in object [%s]\n",
                                       nullstr_guard(child_node->string), nullstr_guard(node->string));
                                continue;
                        }

                        if (is_jkey_compound(child_key)) {
                                err = jkey_cjson_load_recursive(child_key, child_node, depth + 1);
                        } else {
                                err = jkey_cjson_input(child_key, child_node);
                        }

                        if (err) {
                                pr_info_once("stack of key on error:\n");
                                cjson_node_print(child_node, depth + 1, NULL);
                                cjson_node_print(node, depth, NULL);

                                return err;
                        }
                }
        }

        return err;
}

int jkey_cjson_load(jkey_t *root_key, cJSON *root_node)
{
        cjson_node_print(root_node, 0, NULL);
        return jkey_cjson_load_recursive(root_key, root_node, 0);
}

static int printf_wrapper(char **buf, size_t *pos, size_t *len, const char *fmt, ...)
{
        va_list ap;
        int ret;

        va_start(ap, fmt);
        ret = vprintf(fmt, ap);
        va_end(ap);

        return ret;
}

int jbuf_traverse_print_pre(jkey_t *jkey, int has_next, int depth, int argc, va_list arg)
{
        char **buf = NULL; size_t *buf_len = 0, *buf_pos = 0;
        int (*printf_wrap)(char **buf, size_t *pos, size_t *len, const char *fmt, ...) = printf_wrapper;

        if (argc == 3) {
               buf = va_arg(arg, char **);
               buf_pos = va_arg(arg, size_t *);
               buf_len = va_arg(arg, size_t *);
               printf_wrap = snprintf_resize;
        }

        printf_wrap(buf, buf_pos, buf_len, "%.*s", depth, json_indent);

        if (jkey->key)
                printf_wrap(buf, buf_pos, buf_len, "\"%s\" : ", nullstr_guard(jkey->key));

        switch (jkey->type) {
        case JKEY_TYPE_OBJECT:
                printf_wrap(buf, buf_pos, buf_len, "{\n");
                break;

        case JKEY_TYPE_RO_ARRAY:
        case JKEY_TYPE_FIXED_ARRAY:
        case JKEY_TYPE_GROW_ARRAY:
        case JKEY_TYPE_LIST_ARRAY:
                printf_wrap(buf, buf_pos, buf_len, "[\n");
                break;

        default:
                break;
        }

        return 0;
}

static char *backslash_convert(char *str)
{
        char *buf = NULL;
        size_t orig_len = strlen(str);
        size_t cnt = 0;

        if (NULL == strchr(str, '\\'))
                return NULL;

        for (size_t i = 0; i < orig_len; i++) {
                if (str[i] == '\\')
                        cnt++;
        }

        buf = (char *)calloc(orig_len + cnt + 2, sizeof(char));
        if (!buf)
                return NULL;

        for (size_t i = 0, j = 0; i < orig_len; i++, j++) {
                buf[j] = str[i];

                if (str[i] == '\\')
                        buf[++j] = '\\';
        }

        return buf;
}

int jbuf_traverse_print_post(jkey_t *jkey, int has_next, int depth, int argc, va_list arg)
{
        void *ref = NULL;
        char *str_utf8 = NULL, *str_converted = NULL;
        char **buf = NULL; size_t *buf_len = 0, *buf_pos = 0;
        int (*printf_wrap)(char **buf, size_t *pos, size_t *len, const char *fmt, ...) = printf_wrapper;

        if (argc == 3) {
                buf = va_arg(arg, char **);
                buf_pos = va_arg(arg, size_t *);
                buf_len = va_arg(arg, size_t *);
                printf_wrap = snprintf_resize;
        }

        if (is_jkey_compound(jkey)) {
                printf_wrap(buf, buf_pos, buf_len, "%.*s", depth, json_indent);

                switch (jkey->type) {
                case JKEY_TYPE_OBJECT:
                        printf_wrap(buf, buf_pos, buf_len, "}");
                        break;

                case JKEY_TYPE_RO_ARRAY:
                case JKEY_TYPE_FIXED_ARRAY:
                case JKEY_TYPE_GROW_ARRAY:
                case JKEY_TYPE_LIST_ARRAY:
                        printf_wrap(buf, buf_pos, buf_len, "]");
                        break;
                }

                goto line_ending;
        }

        if (jkey->data.ref) {
                ref = jkey->data.ref;

                if (jkey->data.ref_ptr)
                        ref = *(uint8_t **)jkey->data.ref;
        }

        if (!ref) {
                printf_wrap(buf, buf_pos, buf_len, "null");
                goto line_ending;
        }

        if (jkey->type == JKEY_TYPE_STRREF ||
            jkey->type == JKEY_TYPE_STRPTR ||
            jkey->type == JKEY_TYPE_STRBUF) {
                str_converted = backslash_convert((char *)ref);
                if (str_converted)
                        ref = str_converted;
        }

        switch (jkey->type) {
        case JKEY_TYPE_UINT:
        {
                char *hex_fmt = NULL;
                uint64_t d = 0;

                if (unlikely(ptr_unsigned_word_read(ref, jkey->data.sz, &d)))
                        break;

                switch (jkey->data.sz) {
                case sizeof(uint8_t):
                        hex_fmt = (char *)"\"0x%02jx\"";
                        break;
                case sizeof(uint16_t):
                        hex_fmt = (char *)"\"0x%04jx\"";
                        break;
                case sizeof(uint32_t):
                        hex_fmt = (char *)"\"0x%08jx\"";
                        break;
                case sizeof(uint64_t):
                        hex_fmt = (char *)"\"0x%016jx\"";
                        break;
                }

                if (jkey->strval.map) {
                        if (d < jkey->strval.cnt)
                                printf_wrap(buf, buf_pos, buf_len, "\"%s\"", nullstr_guard(jkey->strval.map[d]));
                        else
                                printf_wrap(buf, buf_pos, buf_len, "null");
                } else if (jkey->data.int_base == 16) {
                        printf_wrap(buf, buf_pos, buf_len, hex_fmt, d);
                } else {
                        printf_wrap(buf, buf_pos, buf_len, "%ju", d);
                }

                break;
        }

        case JKEY_TYPE_INT:
        {
                int64_t d = 0;
                char *hex_fmt = NULL;

                if (unlikely(ptr_signed_word_read(ref, jkey->data.sz, &d)))
                        break;

                switch (jkey->data.sz) {
                case sizeof(int8_t):
                        hex_fmt = (char *)"\"0x%02jx\"";
                        break;
                case sizeof(int16_t):
                        hex_fmt = (char *)"\"0x%04jx\"";
                        break;
                case sizeof(int32_t):
                        hex_fmt = (char *)"\"0x%08jx\"";
                        break;
                case sizeof(int64_t):
                        hex_fmt = (char *)"\"0x%016jx\"";
                        break;
                }

                if (jkey->data.int_base == 16)
                        printf_wrap(buf, buf_pos, buf_len, hex_fmt, d);
                else
                        printf_wrap(buf, buf_pos, buf_len, "%jd", d);

                break;
        }

        case JKEY_TYPE_STRREF:
        case JKEY_TYPE_STRPTR:
                printf_wrap(buf, buf_pos, buf_len, "\"%s\"", (char *)ref);

                break;

        case JKEY_TYPE_STRBUF:
                printf_wrap(buf, buf_pos, buf_len, "\"%.*s\"", (int)jkey->data.sz, (char *)ref);

                break;

        case JKEY_TYPE_BOOL:
        {
                uint64_t d = 0;

                if (unlikely(ptr_unsigned_word_read(ref, jkey->data.sz, &d)))
                        break;

                printf_wrap(buf, buf_pos, buf_len, "%s", d ? "true" : "false");

                break;
        }

        case JKEY_TYPE_DOUBLE:
                if (jkey->data.is_float)
                        printf_wrap(buf, buf_pos, buf_len, "%.4f", *(float *)jkey->data.ref);
                else
                        printf_wrap(buf, buf_pos, buf_len, "%.6lf", *(double *)jkey->data.ref);

                break;

        default:
                pr_err("unknown data type: %u\n", jkey->type);
                break;
        }

        if (str_converted)
                free(str_converted);

        if (str_utf8)
                free(str_utf8);

line_ending:
        if (has_next)
                printf_wrap(buf, buf_pos, buf_len, ",");

        printf_wrap(buf, buf_pos, buf_len, "\n");

        return 0;
}

int jbuf_list_array_traverse(jkey_t *arr,
                             jbuf_traverse_cb pre,
                             jbuf_traverse_cb post,
                             int depth,
                             int argc,
                             va_list arg)
{
        jkey_t *data_key = jkey_array_data_key_get(arr);
        struct list_head *head = (struct list_head *)arr->obj.base_ref;
        struct list_head *pos, *n;
        int last_one = 0;
        int err = 0;

        if (!head)
                return -ENODATA;

        if (list_empty(head))
                return 0;

        list_for_each_safe(pos, n, head) {
                void *container;

                if (pos->next == head)
                        last_one = 1;

                container = (uint8_t *)pos - arr->obj.arr.list.offs_head;

                jkey_list_array_ref_update(arr, data_key, container);

                if ((err = _jbuf_traverse_recursive(data_key, pre, post, !last_one, depth + 1, argc, arg)))
                        return err;
        }

        return err;
}

int jbuf_fixed_grow_array_traverse(jkey_t *arr,
                                   jbuf_traverse_cb pre,
                                   jbuf_traverse_cb post,
                                   int depth,
                                   int argc,
                                   va_list arg)
{
        jkey_t *data_key = jkey_array_data_key_get(arr);
        size_t ele_cnt = 0;
        int last_one = 0;
        int err = 0;

        if (arr->type == JKEY_TYPE_FIXED_ARRAY) {
                ele_cnt = arr->obj.arr.fixed.ele_cnt;
        } else if (arr->type == JKEY_TYPE_GROW_ARRAY) {
                size_t *extern_ele_cnt = arr->obj.arr.grow.ext_ele_cnt;

                if (extern_ele_cnt == NULL)
                        return 0;

                ele_cnt = *extern_ele_cnt;
        }

        for (size_t j = 0; j < ele_cnt; j++) {
                if (j + 1 >= ele_cnt)
                        last_one = 1;

                if (jkey_fixed_grow_array_ref_update(arr, data_key, j))
                        break;

                if ((err = _jbuf_traverse_recursive(data_key, pre, post, !last_one, depth + 1, argc, arg)))
                        return err;
        }

        return err;
}

int jbuf_array_traverse(jkey_t *arr,
                        jbuf_traverse_cb pre,
                        jbuf_traverse_cb post,
                        int depth,
                        int argc,
                        va_list arg)
{
        int err = 0;

        if (arr->type == JKEY_TYPE_LIST_ARRAY) {
                err = jbuf_list_array_traverse(arr, pre, post, depth, argc, arg);
        } else {
                err = jbuf_fixed_grow_array_traverse(arr, pre, post, depth, argc, arg);
        }

        return err;
}

static int jkey_null_child_conut(jkey_t *parent)
{
        int sum = 0;
        jkey_t *child;

        for (size_t i = 0; i < parent->child_cnt; i++) {
                child = &((jkey_t *)parent->child)[i];

                if (is_jkey_compound(child)) {
                        if (child->child_cnt)
                                i += child->child_cnt;
                }

                if (is_jkey_ref_ptr(child) && is_jkey_ref_null(child)) {
                        if (is_jkey_compound(child))
                                sum += 1 + child->child_cnt; // 1: child self
                        else
                                sum++;
                }
        }

        return sum;
}

int _jbuf_traverse_recursive(jkey_t *jkey,
                             jbuf_traverse_cb pre,
                             jbuf_traverse_cb post,
                             int has_next,
                             int depth,
                             int argc,
                             va_list arg)
{
        int err = 0;
        int null_child_cnt;

        if (is_jkey_ref_ptr(jkey) && is_jkey_ref_null(jkey))
                return 0;

        if ((err = jkey_child_key_ref_update(jkey)))
                return err;

        if ((err = jkey_child_arr_key_update(jkey)))
                return err;

        null_child_cnt = jkey_null_child_conut(jkey);

        if (pre) {
                if ((err = pre(jkey, has_next, depth, argc, arg)))
                        return err == -ECANCELED ? 0 : err;
        }

        for (size_t i = 0; i < jkey->child_cnt; i++) {
                jkey_t *child = &(((jkey_t *)jkey->child)[i]);

                if (is_jkey_compound(child)) {
                        if (child->child_cnt)
                                i += child->child_cnt;
                }

                // fixed/grow/list array
                if (is_cjson_type(jkey->cjson_type, cJSON_Array) && jkey->type != JKEY_TYPE_RO_ARRAY) {
                        if ((err = jbuf_array_traverse(jkey, pre, post, depth, argc, arg)))
                                return err;
                } else { // ro array/object/data
                        int last_one = 0;

                        if (i + 1 + null_child_cnt >= jkey->child_cnt)
                                last_one = 1;

                        if ((err = _jbuf_traverse_recursive(child, pre, post, !last_one, depth + 1, argc, arg)))
                                return err;
                }
        }

        if (post) {
                if ((err = post(jkey, has_next, depth, argc, arg)))
                        return err == -ECANCELED ? 0 : err;
        }

        return err;
}

static int _jbuf_traverse_print(jbuf_t *b, int argc, ...)
{
        va_list ap;
        int ret;

        va_start(ap, argc);
        ret = _jbuf_traverse_recursive((jkey_t *) b->base,
                                        jbuf_traverse_print_pre, jbuf_traverse_print_post,
                                        0, 0, argc, ap);
        va_end(ap);

        return ret;
}

int jbuf_traverse_print(jbuf_t *b)
{
        if (!b)
                return -EINVAL;

        return _jbuf_traverse_print(b, 0);
}

int jbuf_traverse_snprintf(jbuf_t *b, char **buf, size_t *pos, size_t *len)
{
        if (!b)
                return -EINVAL;

        return _jbuf_traverse_print(b, 3, buf, pos, len);
}

char *jbuf_json_text_save(jbuf_t *jbuf, size_t *bufsz)
{
        size_t len = 1024, pos = 0;
        char *buf = (char *)calloc(len, sizeof(char));
        int err;

        if ((err = jbuf_traverse_snprintf(jbuf, &buf, &pos, &len)))
                goto free;

        if (bufsz)
                *bufsz = pos;

        return buf;

free:
        free(buf);

        return NULL;
}

int jbuf_json_text_load(jbuf_t *jbuf, const char *json_text, size_t json_len)
{
        jkey_t *root_key = jbuf_root_key_get(jbuf);
        cJSON *root_node;
        int err;

        root_node = cJSON_ParseWithLength(json_text, json_len);

        if (!root_node) {
                pr_err("cJSON failed to parse\n");
                return -EINVAL;
        }

        err = jkey_cjson_load(root_key, root_node);

        cJSON_Delete(root_node);

        return err;
}

int jbuf_json_file_load(jbuf_t *jbuf, const char *path)
{
        jkey_t *root_key = jbuf_root_key_get(jbuf);
        cJSON *root_node;
        char *text;
        size_t len;
        int err;

        if (unlikely(!path || path[0] == '\0')) {
                pr_err("file path is empty\n");
                return -ENODATA;
        }

        text = (char *)spiffs_file_read(path, &err, &len);
        if (!text) {
                pr_err("failed to read file %s, err = %d %s\n", path, err, strerror(abs(err)));
                return -EIO;
        }

        root_node = cJSON_ParseWithLength(text, len);
        if (!root_node) {
                pr_err("cJSON failed to parse file: %s\n", path);
                err = -EINVAL;

                goto text_free;
        }

        err = jkey_cjson_load(root_key, root_node);

        cJSON_Delete(root_node);

text_free:
        free(text);

        return err;
}

int jbuf_json_file_save(jbuf_t *jbuf, const char *path)
{
        size_t len = 1024, pos = 0;
        char *buf = (char *)calloc(len, sizeof(char));
        int err;

        if ((err = jbuf_traverse_snprintf(jbuf, &buf, &pos, &len)))
                goto free;

        if ((err = spiffs_file_write(path, (uint8_t *)buf, pos))) {
                pr_err("failed to write %s, err = %d %s\n", path, err, strerror(abs(err)));
                goto free;
        }

free:
        free(buf);

        return err;
}

int jbuf_init(jbuf_t *b, size_t jk_cnt)
{
        if (!b || !jk_cnt)
                return -EINVAL;

        if (b->base || b->head)
                return -EINVAL;

        b->alloc_sz = jk_cnt * sizeof(jkey_t);

        b->base = calloc(1, b->alloc_sz);
        if (!b->base)
                return -ENOMEM;

        b->head = b->base;
        b->end  = (uint8_t *)b->base + b->alloc_sz;

        return 0;
}

int jkey_ref_free(jkey_t *jkey)
{
        void *ref;

        if (!jkey->data.ref)
                return 0;

        if (!jkey->data.ref_ptr || !jkey->data.ref_malloc)
                return 0;

        ref = *(uint8_t **)jkey->data.ref;

        if (!ref) {
//                pr_dbg("key [%s] did not allocated data\n", jkey->key);
                return 0;
        }

        free(ref);
        *(uint8_t **)jkey->data.ref = NULL;

        return 0;
}

int jkey_base_ref_free(jkey_t *jkey)
{
        void *base_ref;

        if (!jkey->obj.base_ref)
                return 0;

        if (!jkey->obj.base_ref_ptr || !jkey->obj.base_ref_malloc)
                return 0;

        base_ref = *(uint8_t **)jkey->obj.base_ref;
        if (!base_ref) {
                pr_dbg("key [%s] did not allocated data\n", nullstr_guard(jkey->key));
                return 0;
        }

        free(base_ref);
        *(uint8_t **)jkey->obj.base_ref = NULL;

        return 0;
}

int jbuf_traverse_free_pre(jkey_t *jkey, int has_next, int depth, int argc, va_list arg)
{
        // algorithm is deep-first, free compound object at the last (post cb)
        if (is_jkey_compound(jkey))
                return 0;

        jkey_ref_free(jkey);

        return 0;
}

int jkey_list_array_free(jkey_t *arr)
{
        struct list_head *head = (struct list_head *)arr->obj.base_ref;
        struct list_head *pos, *n;
        void *container;

        if (!head)
                return -ENODATA;

        if (list_empty(head))
                return 0;

        list_for_each_safe(pos, n, head) {
                list_del(pos);
                container = (uint8_t *)pos - arr->obj.arr.list.offs_head;
                free(container);
        }

        INIT_LIST_HEAD(head);

        return 0;
}

int jbuf_traverse_free_post(jkey_t *jkey, int has_next, int depth, int argc, va_list arg)
{
        if (!is_jkey_compound(jkey))
                return 0;

        if (is_cjson_type(jkey->cjson_type, cJSON_Object)) {
                jkey_ref_free(jkey);
                jkey_base_ref_free(jkey);
        } else if (is_cjson_type(jkey->cjson_type, cJSON_Array)) {
                switch (jkey->type) {
                case JKEY_TYPE_GROW_ARRAY:
                case JKEY_TYPE_FIXED_ARRAY:
                        jkey_base_ref_free(jkey);
                        break;

                case JKEY_TYPE_LIST_ARRAY:
                        jkey_list_array_free(jkey);
                        break;
                }
        }

        return 0;
}

static int _jbuf_traverse_free(jbuf_t *b, int argc, ...)
{
        va_list ap;
        int ret;

        va_start(ap, argc);
        ret = _jbuf_traverse_recursive((jkey_t *) b->base,
                                        jbuf_traverse_free_pre,
                                        jbuf_traverse_free_post,
                                        0, 0, 0, ap);
        va_end(ap);

        return ret;
}

int jbuf_traverse_free(jbuf_t *b)
{
        if (!b)
                return -EINVAL;

        return _jbuf_traverse_free(b, 0);
}

int jbuf_deinit(jbuf_t *b)
{
        if (!b)
                return -EINVAL;

        if (!b->base)
                return -ENODATA;

        jbuf_traverse_free(b);

        free(b->base);
        memset(b, 0x00, sizeof(jbuf_t));

        return 0;
}

#endif /* __LIBJJ_JKEY_H__ */