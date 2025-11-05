/*  libnbt - Minecraft NBT/MCA/SNBT file parser in C
    Copyright (C) 2020 djytw

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#include "nbt.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zlib.h>
int LIBNBT_decompress_gzip (uint8_t **dest, size_t *destsize, uint8_t *src,
                            size_t srcsize);
int LIBNBT_compress_gzip (uint8_t *dest, size_t *destsize, uint8_t *src,
                          size_t srcsize);
int LIBNBT_decompress_zlib (uint8_t **dest, size_t *destsize, uint8_t *src,
                            size_t srcsize);
#define LIBNBT_compress_zlib(a, b, c, d) compress ((a), (b), (c), (d))

typedef struct NBT_Buffer
{
    uint8_t *data;
    size_t len;
    size_t pos;
} NBT_Buffer;

#define isValidTag(tag) ((tag) > TAG_End && (tag) <= TAG_Long_Array)

#ifdef _MSC_VER
#include <stdlib.h>
#define bswap_16(x) _byteswap_ushort (x)
#define bswap_32(x) _byteswap_ulong (x)
#define bswap_64(x) _byteswap_uint64 (x)
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16 (x)
#define bswap_32(x) OSSwapInt32 (x)
#define bswap_64(x) OSSwapInt64 (x)
#elif defined(__MINGW32__)
#define bswap_16(x) __builtin_bswap16 (x)
#define bswap_32(x) __builtin_bswap32 (x)
#define bswap_64(x) __builtin_bswap64 (x)
#else
#include <byteswap.h>
#endif

static NBT_Buffer *init_buffer (uint8_t *data, int length);
static uint8_t *dup_data (uint8_t *src, size_t len);
static char *convert_string (const char *str, size_t len);
static char *convert_string_to_mutf8 (const char *str);
static int nbt_node_write_nbt_to_gbytearray (GByteArray *arr, NbtNode *node,
                                             int writekey,
                                             DhProgressFullSet set_func,
                                             void *main_klass, int *n_node,
                                             int nodes, clock_t start_time,
                                             GCancellable *cancellable);
static const char *parsing_nbt_to_node_txt
    = "Parsing NBT file to NBT node tree.";
static const char *finish_txt = "Parsing finished!";

static int
LIBNBT_getUint8 (NBT_Buffer *buffer, uint8_t *result)
{
    if (buffer->pos + 1 > buffer->len)
        {
            return 0;
        }
    *result = buffer->data[buffer->pos];
    ++buffer->pos;
    return 1;
}

static int
LIBNBT_getUint16 (NBT_Buffer *buffer, uint16_t *result)
{
    if (buffer->pos + 2 > buffer->len)
        {
            return 0;
        }
    *result = *(uint16_t *)(buffer->data + buffer->pos);
    buffer->pos += 2;
    *result = bswap_16 (*result);
    return 2;
}

static int
LIBNBT_getUint32 (NBT_Buffer *buffer, uint32_t *result)
{
    if (buffer->pos + 4 > buffer->len)
        {
            return 0;
        }
    *result = *(uint32_t *)(buffer->data + buffer->pos);
    buffer->pos += 4;
    *result = bswap_32 (*result);
    return 4;
}

static int
LIBNBT_getUint64 (NBT_Buffer *buffer, uint64_t *result)
{
    if (buffer->pos + 8 > buffer->len)
        {
            return 0;
        }
    *result = *(uint64_t *)(buffer->data + buffer->pos);
    buffer->pos += 8;
    *result = bswap_64 (*result);
    return 8;
}

static int
LIBNBT_getFloat (NBT_Buffer *buffer, float *result)
{
    if (buffer->pos + 4 > buffer->len)
        {
            return 0;
        }
    uint32_t ret = *(uint32_t *)(buffer->data + buffer->pos);
    buffer->pos += 4;
    ret = bswap_32 (ret);
    *result = *(float *)&ret;
    return 4;
}

static int
LIBNBT_getDouble (NBT_Buffer *buffer, double *result)
{
    if (buffer->pos + 8 > buffer->len)
        {
            return 0;
        }
    uint64_t ret = *(uint64_t *)(buffer->data + buffer->pos);
    buffer->pos += 8;
    ret = bswap_64 (ret);
    *result = *(double *)&ret;
    return 8;
}

static int
LIBNBT_getKey (NBT_Buffer *buffer, char **result)
{
    uint16_t len;
    if (!LIBNBT_getUint16 (buffer, &len))
        {
            return 0;
        }
    if (len == 0)
        {
            *result = 0;
            return 2;
        }
    if (buffer->pos + len > buffer->len)
        {
            return 0;
        }
    *result = malloc (len + 1);
    memcpy (*result, buffer->data + buffer->pos, len);
    (*result)[len] = 0;
    buffer->pos += len;
    return 2 + len;
}

static NbtNode *
create_nbt (NBT_Tags tag)
{
    NbtData *data = g_new0 (NbtData, 1);
    data->type = tag;
    NbtNode *root = g_node_new (data);
    return root;
}

static NBT_Buffer *
init_buffer (uint8_t *data, int length)
{
    NBT_Buffer *buffer = malloc (sizeof (NBT_Buffer));
    if (buffer == NULL)
        return NULL;
    buffer->data = data;
    buffer->len = length;
    buffer->pos = 0;
    return buffer;
}

static uint8_t *
dup_data (uint8_t *src, size_t len)
{
    uint8_t *dst = malloc (len);
    if (dst == NULL)
        return NULL;
    memcpy (dst, src, len);
    return dst;
}

static void
LIBNBT_fill_err (NBT_Error *err, int errid, int position)
{
    if (err == NULL)
        {
            return;
        }
    err->errid = errid;
    err->position = position;
}

static int
parse_value (NbtNode *node, NBT_Buffer *buffer, uint8_t skipkey,
             DhProgressFullSet set_func, void *main_klass,
             GCancellable *cancellable, int min, int max, clock_t start_time)
{
    if (!node || !buffer || !buffer->data)
        return LIBNBT_ERROR_INTERNAL;
    if (g_cancellable_is_cancelled (cancellable))
        return LIBNBT_ERROR_EARLY_EOF;

    if (set_func && main_klass)
        {
            int passed_ms = 1000.0f * (clock () - start_time) / CLOCKS_PER_SEC;
            if (passed_ms % 500 == 0)
                set_func (main_klass,
                          min + buffer->pos * (max - min) / buffer->len,
                          parsing_nbt_to_node_txt);
        }

    NbtData *data = node->data;
    NBT_Tags tag = data->type;
    if (tag == TAG_End)
        {
            if (!LIBNBT_getUint8 (buffer, (uint8_t *)&tag))
                return LIBNBT_ERROR_EARLY_EOF;
            if (!isValidTag (tag))
                return LIBNBT_ERROR_INVALID_DATA;
            data->type = tag;
        }

    if (!skipkey)
        {
            char *key = NULL;
            if (!LIBNBT_getKey (buffer, &key))
                return LIBNBT_ERROR_EARLY_EOF;
            data->key = key;
        }

    switch (tag)
        {
        case TAG_Byte:
            {
                uint8_t value;
                if (!LIBNBT_getUint8 (buffer, &value))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_i = value;
                break;
            }
        case TAG_Short:
            {
                uint16_t value;
                if (!LIBNBT_getUint16 (buffer, &value))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_i = value;
                break;
            }
        case TAG_Int:
            {
                uint32_t value;
                if (!LIBNBT_getUint32 (buffer, &value))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_i = value;
                break;
            }
        case TAG_Long:
            {
                uint64_t value;
                if (!LIBNBT_getUint64 (buffer, &value))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_i = value;
                break;
            }
        case TAG_Float:
            {
                float value;
                if (!LIBNBT_getFloat (buffer, &value))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_d = value;
                break;
            }
        case TAG_Double:
            {
                double value;
                if (!LIBNBT_getDouble (buffer, &value))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_d = value;
                break;
            }
        case TAG_Byte_Array:
            {
                uint32_t len;
                if (!LIBNBT_getUint32 (buffer, &len))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_a.len = len;
                if (buffer->pos + len > buffer->len)
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_a.value = g_new0 (uint8_t, len);
                memcpy (data->value_a.value, buffer->data + buffer->pos, len);
                buffer->pos += len;
                break;
            }
        case TAG_String:
            {
                uint16_t len;
                if (!LIBNBT_getUint16 (buffer, &len))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_a.len = len + 1;
                if (buffer->pos + len > buffer->len)
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_a.value = g_new0 (uint8_t, len + 1);
                memcpy (data->value_a.value, buffer->data + buffer->pos, len);
                char *value = data->value_a.value;
                value[len] = 0;
                char *new_value = convert_string (value, strlen (value));
                free (data->value_a.value);
                data->value_a.value = new_value;
                buffer->pos += len;
                break;
            }
        case TAG_List:
            {
                uint8_t list_type;
                if (!LIBNBT_getUint8 (buffer, &list_type))
                    return LIBNBT_ERROR_EARLY_EOF;
                uint32_t len;
                if (!LIBNBT_getUint32 (buffer, &len))
                    return LIBNBT_ERROR_EARLY_EOF;
                if (list_type == TAG_End && len != 0)
                    return LIBNBT_ERROR_INVALID_DATA;
                NbtNode *last = NULL;
                for (int i = 0; i < len; i++)
                    {
                        NbtNode *child = create_nbt (list_type);
                        int ret = parse_value (child, buffer, 1, set_func,
                                               main_klass, cancellable, min,
                                               max, start_time);
                        if (ret)
                            {
                                nbt_node_free (child);
                                return ret;
                            }

                        last = g_node_insert_after (node, last, child);
                    }
                break;
            }
        case TAG_Compound:
            {
                NbtNode *last = NULL;
                while (1)
                    {
                        uint8_t list_type;
                        if (!LIBNBT_getUint8 (buffer, &list_type))
                            return LIBNBT_ERROR_EARLY_EOF;
                        if (list_type == 0)
                            break;
                        NbtNode *child = create_nbt (list_type);
                        int ret = parse_value (child, buffer, 0, set_func,
                                               main_klass, cancellable, min,
                                               max, start_time);
                        if (ret)
                            {
                                nbt_node_free (child);
                                return ret;
                            }
                        last = g_node_insert_after (node, last, child);
                    }
                break;
            }
        case TAG_Int_Array:
            {
                uint32_t len;
                if (!LIBNBT_getUint32 (buffer, &len))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_a.len = len;
                if (buffer->pos + len * 4 > buffer->len)
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_a.value = g_new0 (uint32_t, len);
                memcpy (data->value_a.value, buffer->data + buffer->pos,
                        len * 4);
                buffer->pos += len * 4;
                int i;
                uint32_t *value = data->value_a.value;
                for (i = 0; i < len; i++)
                    {
                        value[i] = bswap_32 (value[i]);
                    }
                break;
            }
        case TAG_Long_Array:
            {
                uint32_t len;
                if (!LIBNBT_getUint32 (buffer, &len))
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_a.len = len;
                if (buffer->pos + len * 8 > buffer->len)
                    return LIBNBT_ERROR_EARLY_EOF;
                data->value_a.value = g_new0 (uint64_t, len);
                memcpy (data->value_a.value, buffer->data + buffer->pos,
                        len * 8);
                buffer->pos += len * 8;
                int i;
                uint64_t *value = data->value_a.value;
                for (i = 0; i < len; i++)
                    {
                        value[i] = bswap_64 (value[i]);
                    }
                break;
            }
        default:
            return LIBNBT_ERROR_INTERNAL;
        }
    return 0;
}

int
LIBNBT_decompress_gzip (uint8_t **dest, size_t *destsize, uint8_t *src,
                        size_t srcsize)
{

    size_t sizestep = 1 << 16;
    size_t sizecur = sizestep;
    uint8_t *buffer = malloc (sizecur);

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = src;
    strm.avail_in = srcsize;
    strm.next_out = buffer;
    strm.avail_out = sizestep;

    if (inflateInit2 (&strm, 15 | 32) < 0)
        {
            return -1;
        }
    int ret;
    while ((ret = inflate (&strm, Z_NO_FLUSH)) != Z_STREAM_END)
        {
            if (ret == Z_OK)
                {
                    sizecur += sizestep;
                    strm.avail_out += sizestep;
                    uint8_t *newbuf = realloc (buffer, sizecur);
                    if (newbuf == NULL)
                        {
                            free (buffer);
                            return -1;
                        }
                    strm.next_out = strm.next_out - buffer + newbuf;
                    buffer = newbuf;
                }
            else
                {
                    return -1;
                }
        }
    inflateEnd (&strm);
    *destsize = sizecur - strm.avail_out;
    *dest = buffer;
    return 0;
}

int
LIBNBT_decompress_zlib (uint8_t **dest, size_t *destsize, uint8_t *src,
                        size_t srcsize)
{

    size_t sizestep = 1 << 16;
    size_t sizecur = sizestep;
    uint8_t *buffer = malloc (sizecur);

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = src;
    strm.avail_in = srcsize;
    strm.next_out = buffer;
    strm.avail_out = sizestep;

    if (inflateInit (&strm) < 0)
        {
            return -1;
        }
    int ret;
    while ((ret = inflate (&strm, Z_NO_FLUSH)) != Z_STREAM_END)
        {
            if (ret == Z_OK)
                {
                    sizecur += sizestep;
                    strm.avail_out += sizestep;
                    uint8_t *newbuf = realloc (buffer, sizecur);
                    if (newbuf == NULL)
                        {
                            free (buffer);
                            return -1;
                        }
                    strm.next_out = strm.next_out - buffer + newbuf;
                    buffer = newbuf;
                }
            else
                {
                    return -1;
                }
        }
    inflateEnd (&strm);
    *destsize = sizecur - strm.avail_out;
    *dest = buffer;
    return 0;
}

int
LIBNBT_compress_gzip (uint8_t *dest, size_t *destsize, uint8_t *src,
                      size_t srcsize)
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = src;
    strm.avail_in = srcsize;
    strm.next_out = dest;
    strm.avail_out = *destsize;

    if (deflateInit2 (&strm, Z_BEST_COMPRESSION, Z_DEFLATED, 15 | 16, 8,
                      Z_DEFAULT_STRATEGY)
        < 0)
        {
            return -1;
        }
    // If return Z_OK (0), it would also need more space
    // Refer to zlib.h, if not enough space, deflate() should be called until
    // error encounter or when it's Z_STREAM_END (1)
    if (deflate (&strm, Z_FINISH) <= 0)
        {
            deflateEnd (&strm);
            return -1;
        }
    deflateEnd (&strm);
    *destsize = strm.total_out;
    return 0;
}

NbtNode *
nbt_node_new_opt (uint8_t *data, size_t length, NBT_Error *errid,
                  DhProgressFullSet set_func, void *klass,
                  GCancellable *cancellable, int min, int max)
{
    NBT_Buffer *buffer;

    /* Unzip data */
    if (length > 1 && data[0] == 0x1f && data[1] == 0x8b)
        {
            // file is gzip
            size_t size;
            uint8_t *undata;
            int ret = LIBNBT_decompress_gzip (&undata, &size, data, length);
            if (ret != 0)
                {
                    LIBNBT_fill_err (errid, LIBNBT_ERROR_UNZIP_ERROR, 0);
                    return NULL;
                }
            buffer = init_buffer (undata, size);
        }
    else if (data[0] == 0x78)
        {
            // file is zlib
            size_t size;
            uint8_t *undata;
            int ret = LIBNBT_decompress_zlib (&undata, &size, data, length);
            if (ret != 0)
                {
                    LIBNBT_fill_err (errid, LIBNBT_ERROR_UNZIP_ERROR, 0);
                    return NULL;
                }
            buffer = init_buffer (undata, size);
        }
    else
        {
            uint8_t *dup_data_p = dup_data (data, length);
            buffer = init_buffer (dup_data_p, length);
        }

    /* Cancelled */
    if (g_cancellable_is_cancelled (cancellable))
        {
            free (buffer->data);
            free (buffer);
            return NULL;
        }

    NbtNode *root = create_nbt (TAG_End);
    int ret = parse_value (root, buffer, 0, set_func, klass, cancellable, min,
                           max, clock ());
    free (buffer->data);

    if (ret != 0)
        {
            LIBNBT_fill_err (errid, ret, buffer->pos);
            g_node_destroy (root);
            free (buffer);
            return NULL;
        }
    else
        {
            if (set_func && klass)
                set_func (klass, max, finish_txt);
            if (buffer->pos != buffer->len)
                {
                    LIBNBT_fill_err (errid, LIBNBT_ERROR_LEFTOVER_DATA,
                                     buffer->pos);
                }
            else
                {
                    LIBNBT_fill_err (errid, 0, buffer->pos);
                }
            free (buffer);
            return root;
        }
}

NbtNode *
nbt_node_new_with_progress (uint8_t *data, size_t length,
                            DhProgressFullSet set_func, void *main_klass,
                            GCancellable *cancellable, int min, int max)
{
    return nbt_node_new_opt (data, length, NULL, set_func, main_klass,
                             cancellable, min, max);
}

NbtNode *
nbt_node_new (uint8_t *data, size_t length)
{
    return nbt_node_new_opt (data, length, NULL, NULL, NULL, NULL, 0, 0);
}

static void
nbt_data_free (NbtNode *node)
{
    NbtData *data = node->data;
    if (data->key)
        free (data->key);
    switch (data->type)
        {
        case TAG_Byte_Array:
        case TAG_Long_Array:
        case TAG_Int_Array:
        case TAG_String:
            if (data->value_a.value != NULL)
                g_free (data->value_a.value);
        default:
            break;
        }
    g_free (data);
}

void
nbt_node_free (NbtNode *node)
{
    while (node)
        {
            GNode *next = node->next;
            if (node->children)
                nbt_node_free (node->children);
            nbt_data_free (node);
            g_slice_free (GNode, node);
            node = next;
        }
}

static void
nbt_node_write_uint8_to_gbytearray (GByteArray *buf, uint8_t value)
{
    g_byte_array_append (buf, &value, 1);
}

static void
nbt_node_write_uint16_to_gbytearray (GByteArray *buf, uint16_t value)
{
    guint16 real_value = bswap_16 (value);
    g_byte_array_append (buf, &real_value, 2);
}

static void
nbt_node_write_uint32_to_gbytearray (GByteArray *buf, uint32_t value)
{
    guint32 real_value = bswap_32 (value);
    g_byte_array_append (buf, &real_value, 4);
}

static void
nbt_node_write_uint64_to_gbytearray (GByteArray *buf, uint64_t value)
{
    guint64 real_value = bswap_64 (value);
    g_byte_array_append (buf, &real_value, 8);
}

static void
nbt_node_write_key_to_gbytearray (GByteArray *buf, char *key, int type)
{
    nbt_node_write_uint8_to_gbytearray (buf, type);
    if (key && key[0])
        {
            char *new_key = convert_string_to_mutf8 (key);
            gsize len = strlen (new_key);
            nbt_node_write_uint16_to_gbytearray (buf, len);
            int i;
            for (i = 0; i < len; i++)
                nbt_node_write_uint8_to_gbytearray (buf, new_key[i]);
            g_free (new_key);
        }
    else
        nbt_node_write_uint16_to_gbytearray (buf, 0);
}

static void
nbt_node_write_number_to_gbytearray (GByteArray *buf, uint64_t value, int type)
{
    switch (type)
        {
        case TAG_Byte:
            nbt_node_write_uint8_to_gbytearray (buf, value);
            break;
        case TAG_Short:
            nbt_node_write_uint16_to_gbytearray (buf, value);
            break;
        case TAG_Int:
            nbt_node_write_uint32_to_gbytearray (buf, value);
            break;
        case TAG_Long:
            nbt_node_write_uint64_to_gbytearray (buf, value);
            break;
        default:
            break;
        }
}

static void
nbt_node_write_float_to_gbytearray (GByteArray *buf, float value)
{
    guint32 val = bswap_32 (*(uint32_t *)&value);
    nbt_node_write_uint32_to_gbytearray (buf, val);
}

static void
nbt_node_write_double_to_gbytearray (GByteArray *buf, double value)
{
    guint64 val = bswap_64 (*(uint64_t *)&value);
    nbt_node_write_uint64_to_gbytearray (buf, val);
}

static void
nbt_node_write_point_to_gbytearray (GByteArray *buf, double value, int type)
{
    switch (type)
        {
        case TAG_Float:
            nbt_node_write_float_to_gbytearray (buf, value);
            break;
        case TAG_Double:
            nbt_node_write_double_to_gbytearray (buf, value);
            break;
        default:
            break;
        }
}

static int
nbt_node_write_list_to_gbytearray (GByteArray *arr, NbtNode *node,
                                   DhProgressFullSet set_func,
                                   void *main_klass, int *n_node, int nodes,
                                   clock_t start_time,
                                   GCancellable *cancellable)
{
    int ret = 0;
    NbtNode *child = node->children;
    int count = 0;
    while (child)
        {
            count++;
            child = child->next;
        }
    child = node->children;
    if (!child)
        nbt_node_write_uint8_to_gbytearray (arr, 0);
    else
        {
            NbtData *child_data = child->data;
            nbt_node_write_uint8_to_gbytearray (arr, child_data->type);
        }
    nbt_node_write_uint32_to_gbytearray (arr, count);
    while (child)
        {
            ret = nbt_node_write_nbt_to_gbytearray (arr, child, 0, set_func,
                                                    main_klass, n_node, nodes,
                                                    start_time, cancellable);
            if (ret)
                return ret;
            child = child->next;
        }
    return 0;
}

static int
nbt_node_write_compound_to_gbytearray (GByteArray *arr, NbtNode *node,
                                       DhProgressFullSet set_func,
                                       void *main_klass, int *n_node,
                                       int nodes, clock_t start_time,
                                       GCancellable *cancellable)
{
    int ret = 0;
    NbtNode *child = node->children;
    while (child)
        {
            ret = nbt_node_write_nbt_to_gbytearray (arr, child, 1, set_func,
                                                    main_klass, n_node, nodes,
                                                    start_time, cancellable);
            if (ret)
                return ret;
            child = child->next;
        }
    nbt_node_write_uint8_to_gbytearray (arr, 0);
    return 0;
}

static void
nbt_node_write_array_to_gbytearray (GByteArray *arr, void *value, int32_t len,
                                    int type)
{
    nbt_node_write_uint32_to_gbytearray (arr, len);
    switch (type)
        {
        case TAG_Byte_Array:
            {
                int i;
                for (i = 0; i < len; i++)
                    nbt_node_write_uint8_to_gbytearray (arr,
                                                        ((uint8_t *)value)[i]);
            }
            break;
        case TAG_Int_Array:
            {
                int i;
                for (i = 0; i < len; i++)
                    nbt_node_write_uint32_to_gbytearray (
                        arr, ((uint32_t *)value)[i]);
            }
            break;
        case TAG_Long_Array:
            {
                int i;
                for (i = 0; i < len; i++)
                    nbt_node_write_uint64_to_gbytearray (
                        arr, ((uint64_t *)value)[i]);
            }
            break;
        default:
            break;
        }
}

static void
nbt_node_write_string_to_gbytearray (GByteArray *arr, void *value)
{
    char *str = value;
    char *output_value = convert_string_to_mutf8 (str);
    gsize real_len = strlen (output_value);
    nbt_node_write_uint16_to_gbytearray (arr, real_len);
    int i;
    for (i = 0; i < real_len; i++)
        nbt_node_write_uint8_to_gbytearray (arr, output_value[i]);
    g_free (output_value);
}

static int
nbt_node_write_nbt_to_gbytearray (GByteArray *arr, NbtNode *node, int writekey,
                                  DhProgressFullSet set_func, void *main_klass,
                                  int *n_node, int nodes, clock_t start_time,
                                  GCancellable *cancellable)
{
    int ret = 0;
    if (!node)
        return LIBNBT_ERROR_INTERNAL;
    if (n_node)
        (*n_node)++;

    if (set_func && main_klass)
        {
            int passed_ms = 1000.0f * (clock () - start_time) / CLOCKS_PER_SEC;
            if (passed_ms % 500 == 0 || (*n_node + 1) == nodes)
                set_func (main_klass, (*n_node) * 100 / nodes, "Parsing NBT");
        }
    if (g_cancellable_is_cancelled (cancellable))
        return LIBNBT_ERROR_INTERNAL;
    NbtData *data = node->data;
    if (writekey)
        nbt_node_write_key_to_gbytearray (arr, data->key, data->type);
    switch (data->type)
        {
        case TAG_Byte:
        case TAG_Short:
        case TAG_Int:
        case TAG_Long:
            nbt_node_write_number_to_gbytearray (arr, data->value_i,
                                                 data->type);
            break;
        case TAG_Float:
        case TAG_Double:
            nbt_node_write_point_to_gbytearray (arr, data->value_d,
                                                data->type);
            break;
        case TAG_Byte_Array:
        case TAG_Int_Array:
        case TAG_Long_Array:
            nbt_node_write_array_to_gbytearray (arr, data->value_a.value,
                                                data->value_a.len, data->type);
            break;
        case TAG_String:
            nbt_node_write_string_to_gbytearray (arr, data->value_a.value);
            break;
        case TAG_List:
            ret = nbt_node_write_list_to_gbytearray (arr, node, set_func,
                                                     main_klass, n_node, nodes,
                                                     start_time, cancellable);
            return ret;
        case TAG_Compound:
            ret = nbt_node_write_compound_to_gbytearray (
                arr, node, set_func, main_klass, n_node, nodes, start_time,
                cancellable);
            return ret;
        default:
            return LIBNBT_ERROR_INTERNAL;
        }
    return 0;
}


static int
skip_len (const char *str)
{
    if ((gint8)*str > 0)
        return 1;
    else if ((*str & 0xe0) == 0xe0)
        return 3;
    else if ((*str & 0xc0) == 0xc0)
        return 2;
}

static char *
convert_string (const char *str, size_t len)
{
    if (!str)
        return NULL;
    guint16 *utf16 = g_new0 (guint16, len + 1);
    int i = 0;
    for (; *str; str = str + skip_len (str), i++)
        {
            int skip_len_tmp = skip_len (str);
            guint16 c = 0;
            if (skip_len_tmp == 1)
                c = (guint8)*str;
            else if (skip_len_tmp == 2)
                c = ((str[0] & 0x1f) << 6) + (str[1] & 0x3f);
            else
                c = ((str[0] & 0xf) << 12) + ((str[1] & 0x3f) << 6)
                    + (str[2] & 0x3f);
            utf16[i] = c;
        }
    utf16[i] = 0;
    char *utf8 = g_utf16_to_utf8 (utf16, len, NULL, NULL, NULL);
    g_free (utf16);
    return utf8;
}

static char *
convert_string_to_mutf8 (const char *str)
{
    if (!str)
        return NULL;
    GString *string = g_string_new (NULL);
    for (; *str; str = g_utf8_next_char (str))
        {
            gunichar c = g_utf8_get_char (str);
            if (c < 65536)
                g_string_append_unichar (string, c);
            else
                {
                    int more_val = 0x10000;
                    c = c - more_val;
                    uint8_t temp[7];
                    int hi = (1 << 20) - 1 - ((1 << 10) - 1);
                    guint hipos = (c & hi) >> 10;
                    int mid3 = 0x3c0;
                    int mid4 = 0x3f;
                    temp[0] = 0b11101101;
                    temp[1] = 0b10100000 + (hipos & mid3);
                    temp[2] = 0b10000000 + (hipos & mid4);
                    temp[3] = temp[0];
                    temp[4] = 0b10110000 + (c & mid3);
                    temp[5] = 0b10000000 + (c & mid4);
                    temp[6] = 0;
                    g_string_append (string, temp);
                }
        }
    return g_string_free_and_steal (string);
}

uint8_t *
nbt_node_pack_full (NbtNode *node, size_t *length, NBT_Compression compression,
                    GError **error, DhProgressFullSet set_func,
                    void *main_klass, GCancellable *cancellable, GFile *file)
{
    /* Write NBT buffer to ByteArray */
    GByteArray *buf = g_byte_array_new ();
    gsize n_node = g_node_n_nodes (node, G_TRAVERSE_ALL);
    int n = 0;
    int ret = nbt_node_write_nbt_to_gbytearray (buf, node, TRUE, set_func,
                                                main_klass, &n, n_node,
                                                clock (), cancellable);
    if (ret || g_cancellable_is_cancelled (cancellable))
        {
            g_byte_array_free (buf, TRUE);
            GQuark error_domain
                = g_quark_from_string ("NBT_NODE_ERROR_CANCELLED");
            g_set_error_literal (error, error_domain, -1,
                                 "The task was cancelled in packing process.");
            return NULL;
        }

    /* Compress the data */
    GBytes *original_bytes = g_byte_array_free_to_bytes (buf);
    gsize original_len = 0;
    guint8 *original_nbt
        = g_bytes_unref_to_data (original_bytes, &original_len);
    void *ret_data = NULL;

    GZlibCompressorFormat format;
    if (compression == NBT_Compression_GZIP)
        format = G_ZLIB_COMPRESSOR_FORMAT_GZIP;
    else if (compression == NBT_Compression_ZLIB)
        format = G_ZLIB_COMPRESSOR_FORMAT_ZLIB;
    else
        format = G_ZLIB_COMPRESSOR_FORMAT_RAW;

    GZlibCompressor *compressor = g_zlib_compressor_new (format, -1);
    GInputStream *is = g_memory_input_stream_new_from_data (
        original_nbt, original_len, g_free);
    GOutputStream *os = NULL;
    if (file)
        {
            if (!g_file_query_exists (file, NULL))
                {
                    g_file_make_directory_with_parents (file, cancellable,
                                                        error);
                    if (error && *error)
                        goto error_handle_early;
                    g_file_delete (file, cancellable, error);
                    if (error && *error)
                        goto error_handle_early;
                }
            GFileOutputStream *fos = g_file_replace (
                file, NULL, FALSE, G_FILE_CREATE_NONE, cancellable, error);
            /* Replace file error */
            if (error && *error)
                {
                error_handle_early:
                    g_object_unref (compressor);
                    g_object_unref (is);
                    return NULL;
                }
            os = G_OUTPUT_STREAM (fos);
        }
    else
        os = g_memory_output_stream_new_resizable ();
    GOutputStream *cos
        = g_converter_output_stream_new (os, G_CONVERTER (compressor));
    g_output_stream_splice (cos, is, G_OUTPUT_STREAM_SPLICE_NONE, cancellable,
                            error);
    if (error && *error)
        goto error_handle;
    g_output_stream_close (cos, cancellable, error);
    if (error && *error)
        goto error_handle;

    if (!file)
        {
            GBytes *bytes = g_memory_output_stream_steal_as_bytes (
                G_MEMORY_OUTPUT_STREAM (os));
            ret_data = g_bytes_unref_to_data (bytes, length);
        }

error_handle:
    g_object_unref (cos);
    g_object_unref (is);
    g_object_unref (os);
    g_object_unref (compressor);

    return ret_data;
}

static void
snbt_write_nbt (GString *string, NbtNode *node, GError **error, int max_level,
                gboolean pretty_output, gboolean space,
                DhProgressFullSet set_func, void *main_klass,
                GCancellable *cancellable)
{
}

uint8_t *
nbt_node_to_snbt_full (NbtNode *node, size_t *length, GError **error,
                       int max_level, gboolean pretty_output, gboolean space,
                       DhProgressFullSet set_func, void *main_klass,
                       GCancellable *cancellable, GFile *file)
{
    GString *string = g_string_new ("");

    guint8 *ret;
    ret = g_string_free_and_steal (string);
    if (length)
        *length = strlen (ret);
    return ret;
}

MCA *
MCA_Init (const char *filename)
{
    MCA *ret = malloc (sizeof (MCA));
    memset (ret, 0, sizeof (MCA));
    if (filename && filename[0])
        {
            char *str = strrchr (filename, '/');
            if (!str)
                str = (char *)filename;
            else
                str++;
            int count = sscanf (str, "r.%d.%d.mca", &ret->x, &ret->z);
            if (count == 2)
                {
                    ret->hasPosition = 1;
                }
        }
    return ret;
}

MCA *
MCA_Init_WithPos (int x, int z)
{
    MCA *ret = malloc (sizeof (MCA));
    memset (ret, 0, sizeof (MCA));
    ret->hasPosition = 1;
    ret->x = x;
    ret->z = z;
    return ret;
}

void
MCA_Free (MCA *mca)
{
    int i;
    for (i = 0; i < CHUNKS_IN_REGION; i++)
        {
            if (mca->data[i])
                {
                    nbt_node_free (mca->data[i]);
                }
            if (mca->rawdata[i])
                {
                    free (mca->rawdata[i]);
                }
        }
    free (mca);
}

void
nbt_glib_set_parsing_nbt_to_node_txt (const char *text)
{
    parsing_nbt_to_node_txt = text;
}

void
nbt_glib_set_finish_txt (const char *text)
{
    finish_txt = text;
}

int
MCA_ParseAll (MCA *mca)
{
    int i;
    int errcount = 0;
    NBT_Error error;
    for (i = 0; i < CHUNKS_IN_REGION; i++)
        {
            if (mca->rawdata[i])
                {
                    mca->data[i]
                        = nbt_node_new_opt (mca->rawdata[i], mca->size[i],
                                            &error, NULL, NULL, NULL, 0, 100);
                    if (mca->data[i] == NULL)
                        {
                            errcount++;
                        }
                }
        }
    return errcount;
}

int
MCA_ReadRaw_File (FILE *fp, MCA *mca, int skip_chunk_error)
{

    memset (mca->rawdata, 0, sizeof (uint8_t *) * CHUNKS_IN_REGION);
    memset (mca->size, 0, sizeof (uint32_t) * CHUNKS_IN_REGION);

    if (fp == NULL)
        {
            return LIBNBT_ERROR_INVALID_DATA;
        }
    fseek (fp, 0, SEEK_END);
    int size = ftell (fp);
    if (size <= 8192)
        {
            return LIBNBT_ERROR_INVALID_DATA;
        }
    fseek (fp, 0, SEEK_SET);

    uint64_t offsets[CHUNKS_IN_REGION];

    int j;
    for (j = 0; j < CHUNKS_IN_REGION; j++)
        {
            uint64_t t = fgetc (fp) << 16;
            t |= fgetc (fp) << 8;
            t |= fgetc (fp);
            offsets[j] = t << 12;
            uint64_t tsize = (fgetc (fp) << 12) + offsets[j];
            if (tsize > size)
                {
                    if (skip_chunk_error)
                        {
                            offsets[j] = 0;
                        }
                    else
                        {
                            return LIBNBT_ERROR_INVALID_DATA;
                        }
                }
        }

    for (j = 0; j < CHUNKS_IN_REGION; j++)
        {
            uint64_t t = fgetc (fp) << 24;
            t |= fgetc (fp) << 16;
            t |= fgetc (fp) << 8;
            t |= fgetc (fp);
            mca->epoch[j] = t;
        }

    for (j = 0; j < CHUNKS_IN_REGION; j++)
        {

            if (offsets[j] == 0)
                {
                    continue;
                }

            fseek (fp, offsets[j], SEEK_SET);
            uint32_t tsize = fgetc (fp) << 24;
            tsize |= fgetc (fp) << 16;
            tsize |= fgetc (fp) << 8;
            tsize |= fgetc (fp);

            uint8_t type = fgetc (fp);
            if (type != 2 && !skip_chunk_error)
                {
                    goto chunk_error;
                }

            mca->rawdata[j] = malloc (tsize - 1);
            mca->size[j] = tsize - 1;
            int readSize = fread (mca->rawdata[j], 1, mca->size[j], fp);

            if (readSize != tsize - 1 && !skip_chunk_error)
                {
                    goto chunk_error;
                }
        }
    return 0;
chunk_error:
    {
        int i;
        for (i = 0; i <= j; i++)
            {
                if (mca->rawdata[i])
                    {
                        free (mca->rawdata[i]);
                    }
            }
        return LIBNBT_ERROR_INVALID_DATA;
    }
}

int
MCA_ReadRaw (uint8_t *data, size_t length, MCA *mca, int skip_chunk_error)
{

    memset (mca->rawdata, 0, sizeof (uint8_t *) * CHUNKS_IN_REGION);
    memset (mca->size, 0, sizeof (uint32_t) * CHUNKS_IN_REGION);

    if (length <= 8192 || data == NULL)
        {
            return LIBNBT_ERROR_INVALID_DATA;
        }

    uint64_t offsets[CHUNKS_IN_REGION];

    int j;
    NBT_Buffer *buffer = init_buffer (data, length);
    for (j = 0; j < CHUNKS_IN_REGION; j++)
        {
            uint32_t temp = 0;
            int ret = LIBNBT_getUint32 (buffer, &temp);
            offsets[j] = temp >> 8 << 12;
            uint64_t tsize = ((temp & 0xff) << 12) + offsets[j];
            if (ret == 0 || tsize > length)
                {
                    if (skip_chunk_error)
                        {
                            offsets[j] = 0;
                        }
                    else
                        {
                            return LIBNBT_ERROR_INVALID_DATA;
                        }
                }
        }

    for (j = 0; j < CHUNKS_IN_REGION; j++)
        {
            uint32_t temp = 0;
            int ret = LIBNBT_getUint32 (buffer, &temp);
            if (ret == 0)
                {
                    if (skip_chunk_error)
                        {
                            mca->epoch[j] = 0;
                        }
                    else
                        {
                            return LIBNBT_ERROR_INVALID_DATA;
                        }
                }
            else
                {
                    mca->epoch[j] = temp;
                }
        }

    for (j = 0; j < CHUNKS_IN_REGION; j++)
        {

            if (offsets[j] == 0)
                {
                    continue;
                }

            buffer->pos = offsets[j];

            uint32_t tsize;
            uint8_t type;

            int ret = LIBNBT_getUint32 (buffer, &tsize);
            if (ret == 0)
                {
                    if (skip_chunk_error)
                        continue;
                    else
                        goto chunk_error;
                }

            ret = LIBNBT_getUint8 (buffer, &type);
            if ((ret == 0 || type != 2) && !skip_chunk_error)
                {
                    goto chunk_error;
                }

            mca->rawdata[j] = malloc (tsize - 1);
            mca->size[j] = tsize - 1;

            memcpy (mca->rawdata[j], data + offsets[j] + 5, mca->size[j]);
        }
    return 0;
chunk_error:
    {
        int i;
        for (i = 0; i <= j; i++)
            {
                if (mca->rawdata[i])
                    {
                        free (mca->rawdata[i]);
                    }
            }
        return LIBNBT_ERROR_INVALID_DATA;
    }
}

int
MCA_WriteRaw_File (FILE *fp, MCA *mca)
{
    if (mca == NULL || fp == NULL)
        {
            return LIBNBT_ERROR_INVALID_DATA;
        }
    int current = 2;
    uint32_t offsets[1024];
    int i;
    for (i = 0; i < CHUNKS_IN_REGION; i++)
        {
            if (mca->rawdata[i] == NULL)
                {
                    offsets[i] = 0;
                    continue;
                }
            fseek (fp, current << 12, SEEK_SET);
            offsets[i] = current << 8;
            uint32_t size = mca->size[i] + 1;
            fputc ((size >> 24) & 0xff, fp);
            fputc ((size >> 16) & 0xff, fp);
            fputc ((size >> 8) & 0xff, fp);
            fputc (size & 0xff, fp);
            fputc (2, fp);
            fwrite (mca->rawdata[i], 1, size - 1, fp);
            int newpos = (ftell (fp) >> 12) + 1;
            offsets[i] |= (newpos - current) & 0xff;
            current = newpos;
        }
    fseek (fp, 0, SEEK_SET);
    for (i = 0; i < CHUNKS_IN_REGION; i++)
        {
            fputc ((offsets[i] >> 24) & 0xff, fp);
            fputc ((offsets[i] >> 16) & 0xff, fp);
            fputc ((offsets[i] >> 8) & 0xff, fp);
            fputc (offsets[i] & 0xff, fp);
        }
    uint32_t t = bswap_32 (time (NULL));
    uint8_t *tbuf = (uint8_t *)&t;
    for (i = 0; i < CHUNKS_IN_REGION; i++)
        {
            fwrite (tbuf, 4, 1, fp);
        }
    fseek (fp, 0, SEEK_END);
    int64_t length;
    if ((length = ftell (fp)) % 4096)
        { // Adds padding for file if it doesn't allign with a multiple of
          // 4096 bytes by itself
            fseek (fp, (((length >> 12) + 1) << 12) - 1, SEEK_SET);
            fputc (0, fp);
        }
    fflush (fp);
    return 0;
}
