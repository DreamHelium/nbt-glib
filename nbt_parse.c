/*  nbt_parse - NBT parse part of the nbt-glib
    Copyright (C) 2025 Dream_Helium

    SPDX-License-Identifier: LGPL-3.0-or-later

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

#include "nbt_parse.h"
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>

#ifndef NBT_GLIB_DISABLE_TRANSLATION
#include <libintl.h>
#define _(text) gettext (text)
#else
#define _(text) text
#endif

/* Since the macro's `-` will be formatted, we use the expanded function */
static GQuark
nbt_glib_parse_error_quark (void)
{
  static GQuark q;
  if G_UNLIKELY (q == 0)
    q = g_quark_from_static_string ("nbt-glib-parse-error-quark");
  return q;
}

#define isValidTag(tag) ((tag) > TAG_End && (tag) <= TAG_Long_Array)

#define bswap_16(x) GUINT16_SWAP_LE_BE (x)
#define bswap_32(x) GUINT32_SWAP_LE_BE (x)
#define bswap_64(x) GUINT64_SWAP_LE_BE (x)

#define N_(txt) txt

/* One can copy to its code and translate */
static const char *texts[]
    = { N_ ("Decompressing."),
        N_ ("Some leftover text detected after parsing."),
        N_ ("Some internal error happened, which is not your fault."),
        N_ ("The parsing progress has been cancelled."),
        N_ ("Couldn't get the type after the End type."),
        N_ ("The tag is invalid."),
        N_ ("Couldn't get key."),
        N_ ("The length of the array is not the corresponding length."),
        N_ ("Couldn't find the corresponding %s type."),
        N_ ("The length of the array/list couldn't be found"),
        N_ ("Couldn't get type in the list."),
        N_ ("The tag of the list is invalid."),
        N_ ("Parsing finished!"),
        N_ ("Parsing NBT file to NBT node tree."),
        N_ ("Parsing file failed."),
        N_ ("Parsing file.") };

typedef struct NBT_Buffer
{
  uint8_t *data;
  size_t len;
  size_t pos;
} NBT_Buffer;

static void
nbt_data_free (NbtNode *node)
{
  NbtData *data = node->data;
  if (data->key)
    g_free (data->key);
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
  *result = g_malloc (len + 1);
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
  NBT_Buffer *buffer = g_malloc (sizeof (NBT_Buffer));
  if (buffer == NULL)
    return NULL;
  buffer->data = data;
  buffer->len = length;
  buffer->pos = 0;
  return buffer;
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
  else
    return 0;
}

/**
 *
 * @param str The original string
 * @param len The length of the string
 * @return The converted UTF-8 string
 * @sa
 * https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/io/DataInput.html#modified-utf-8
 */
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
      else if (skip_len_tmp == 3)
        c = ((str[0] & 0xf) << 12) + ((str[1] & 0x3f) << 6) + (str[2] & 0x3f);
      else
        {
          g_free (utf16);
          return NULL; /* Unexpected situation */
        }
      utf16[i] = c;
    }
  utf16[i] = 0;
  char *utf8 = g_utf16_to_utf8 (utf16, len, NULL, NULL, NULL);
  g_free (utf16);
  return utf8;
}

static int
parse_value (NbtNode *node, NBT_Buffer *buffer, uint8_t skipkey,
             DhProgressFullSet set_func, void *main_klass,
             GCancellable *cancellable, int min, int max, clock_t start_time,
             GError **err)
{
  if (!node || !buffer || !buffer->data)
    {
      g_set_error_literal (
          err, NBT_GLIB_PARSE_ERROR, NBT_GLIB_PARSE_ERROR_INTERNAL,
          _ ("Some internal error happened, which is not your fault."));
      return 1;
    }
  if (g_cancellable_is_cancelled (cancellable))
    {
      g_set_error_literal (err, NBT_GLIB_PARSE_ERROR,
                           NBT_GLIB_PARSE_ERROR_CANCELLED,
                           _ ("The parsing progress has been cancelled."));
      return 1;
    }

  if (set_func && main_klass)
    {
      clock_t passed_ms = 1000 * (clock () - start_time) / CLOCKS_PER_SEC;
      if (passed_ms % 500 == 0)
        set_func (main_klass, min + buffer->pos * (max - min) / buffer->len,
                  _ ("Parsing NBT file to NBT node tree."));
    }

  NbtData *data = node->data;
  NBT_Tags tag = data->type;
  if (tag == TAG_End)
    {
      if (!LIBNBT_getUint8 (buffer, (uint8_t *)&tag))
        {
          g_set_error_literal (
              err, NBT_GLIB_PARSE_ERROR, NBT_GLIB_PARSE_ERROR_INTERRUPTED,
              _ ("Couldn't get the type after the End type."));
          return 1;
        }
      if (!isValidTag (tag))
        {
          g_set_error_literal (err, NBT_GLIB_PARSE_ERROR,
                               NBT_GLIB_PARSE_ERROR_INVALID_TAG,
                               _ ("The tag is invalid."));
          return 1;
        }
      data->type = tag;
    }
  const char *type = NULL;
  if (!skipkey)
    {
      char *key = NULL;
      if (!LIBNBT_getKey (buffer, &key))
        {
          g_set_error_literal (err, NBT_GLIB_PARSE_ERROR,
                               NBT_GLIB_PARSE_ERROR_INTERRUPTED,
                               _ ("Couldn't get key."));
          return 1;
        }
      char *new_key = NULL;
      if (key)
        {
          new_key = convert_string (key, strlen (key));
          g_free (key);
          if (!new_key)
            {
              type = "key";
              goto case_default;
            }
        }
      data->key = new_key;
    }

  switch (tag)
    {
    case TAG_Byte:
      {
        uint8_t value;
        if (!LIBNBT_getUint8 (buffer, &value))
          {
            type = _ ("byte");
            goto case_default;
          }
        data->value_i = value;
        break;
      }
    case TAG_Short:
      {
        uint16_t value;
        if (!LIBNBT_getUint16 (buffer, &value))
          {
            type = _ ("short");
            goto case_default;
          }
        data->value_i = value;
        break;
      }
    case TAG_Int:
      {
        uint32_t value;
        if (!LIBNBT_getUint32 (buffer, &value))
          {
            type = _ ("int");
            goto case_default;
          }
        data->value_i = value;
        break;
      }
    case TAG_Long:
      {
        uint64_t value;
        if (!LIBNBT_getUint64 (buffer, &value))
          {
            type = _ ("long");
            goto case_default;
          }
        data->value_i = value;
        break;
      }
    case TAG_Float:
      {
        float value;
        if (!LIBNBT_getFloat (buffer, &value))
          {
            type = _ ("float");
            goto case_default;
          }
        data->value_d = value;
        break;
      }
    case TAG_Double:
      {
        double value;
        if (!LIBNBT_getDouble (buffer, &value))
          {
            type = _ ("double");
            goto case_default;
          }
        data->value_d = value;
        break;
      }
    case TAG_Byte_Array:
      {
        uint32_t len;
        if (!LIBNBT_getUint32 (buffer, &len))
          goto array_length_get_error;
        data->value_a.len = len;
        if (buffer->pos + len > buffer->len)
          goto array_error;
        data->value_a.value = g_new0 (uint8_t, len);
        memcpy (data->value_a.value, buffer->data + buffer->pos, len);
        buffer->pos += len;
        break;
      }
    case TAG_String:
      {
        uint16_t len;
        if (!LIBNBT_getUint16 (buffer, &len))
          goto array_length_get_error;
        data->value_a.len = len + 1;
        if (buffer->pos + len > buffer->len)
          goto array_error;
        guint8 *value = g_new0 (uint8_t, len + 1);
        memcpy (value, buffer->data + buffer->pos, len);
        value[len] = 0;
        char *new_value = convert_string (value, strlen (value));
        /* The convertion of string might fail */
        if (new_value == NULL)
          {
            g_free (value);
            type = _ ("string");
            goto case_default;
          }
        g_free (value);
        data->value_a.value = new_value;
        buffer->pos += len;
        break;
      }
    case TAG_List:
      {
        uint8_t list_type;
        if (!LIBNBT_getUint8 (buffer, &list_type))
          {
            g_set_error_literal (err, NBT_GLIB_PARSE_ERROR,
                                 NBT_GLIB_PARSE_ERROR_INTERRUPTED,
                                 _ ("Couldn't get type in the list."));
            return 1;
          }
        uint32_t len;
        if (!LIBNBT_getUint32 (buffer, &len))
          goto array_length_get_error;
        if (list_type == TAG_End && len != 0)
          {
            g_set_error_literal (err, NBT_GLIB_PARSE_ERROR,
                                 NBT_GLIB_PARSE_ERROR_INVALID_TAG,
                                 _ ("The tag of the list is invalid."));
            return 1;
          }
        NbtNode *last = NULL;
        for (int i = 0; i < len; i++)
          {
            NbtNode *child = create_nbt (list_type);
            int ret = parse_value (child, buffer, 1, set_func, main_klass,
                                   cancellable, min, max, start_time, err);
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
              {
                type = _ ("in compound");
                goto case_default;
              }
            if (list_type == 0)
              break;
            NbtNode *child = create_nbt (list_type);
            int ret = parse_value (child, buffer, 0, set_func, main_klass,
                                   cancellable, min, max, start_time, err);
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
          goto array_length_get_error;
        data->value_a.len = len;
        if (buffer->pos + len * 4 > buffer->len)
          goto array_error;
        data->value_a.value = g_new0 (uint32_t, len);
        memcpy (data->value_a.value, buffer->data + buffer->pos, len * 4);
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
          goto array_length_get_error;
        data->value_a.len = len;
        if (buffer->pos + len * 8 > buffer->len)
          goto array_error;
        data->value_a.value = g_new0 (uint64_t, len);
        memcpy (data->value_a.value, buffer->data + buffer->pos, len * 8);
        buffer->pos += len * 8;
        int i;
        uint64_t *value = data->value_a.value;
        for (i = 0; i < len; i++)
          value[i] = bswap_64 (value[i]);
        break;
      }
    default:
    array_length_get_error:
      g_set_error_literal (
          err, NBT_GLIB_PARSE_ERROR, NBT_GLIB_PARSE_ERROR_INTERRUPTED,
          _ ("The length of the array/list couldn't be found"));
      return 1;
    array_error:
      g_set_error_literal (err, NBT_GLIB_PARSE_ERROR,
                           NBT_GLIB_PARSE_ERROR_INTERRUPTED,
                           _ ("The length of the array is not the "
                              "corresponding length."));
      return 1;
    case_default:
      g_set_error (err, NBT_GLIB_PARSE_ERROR, NBT_GLIB_PARSE_ERROR_INTERRUPTED,
                   _ ("Couldn't find the corresponding %s type."), type);
      return 1;
    }
  return 0;
}

NbtNode *
nbt_node_new_opt (uint8_t *data, size_t length, GError **err,
                  DhProgressFullSet set_func, void *klass,
                  GCancellable *cancellable, int min, int max)
{
  NBT_Buffer *buffer;
  GZlibCompressorFormat format;
  gboolean no_compression = FALSE;
  /* Unzip data */
  if (length > 1 && data[0] == 0x1f && data[1] == 0x8b)
    /* File is Gzip */
    format = G_ZLIB_COMPRESSOR_FORMAT_GZIP;
  else if (data[0] == 0x78)
    /* File is Zlib */
    format = G_ZLIB_COMPRESSOR_FORMAT_ZLIB;
  else
    no_compression = TRUE;

  if (!no_compression)
    {
      guint8 *buf_data = g_new0 (uint8_t, length);
      guint8 *buf_data_p = buf_data;
      gsize buf_p_len = length;
      GZlibDecompressor *decompressor = g_zlib_decompressor_new (format);
      gsize buf_len = length;
      GConverterResult result = G_CONVERTER_ERROR;
      clock_t start = clock ();
      gsize original_len = length;
      for (; result != G_CONVERTER_FINISHED;)
        {
          if (g_cancellable_is_cancelled (cancellable))
            break;
          GError *internal_err = NULL;
          gsize current_consumed_len = 0;
          gsize current_write_len = 0;
          /* The convertion might not be completed */
          result = g_converter_convert (
              G_CONVERTER (decompressor), data, length, buf_data, buf_len,
              G_CONVERTER_NO_FLAGS, &current_consumed_len, &current_write_len,
              &internal_err);
          if (set_func && klass)
            {
              clock_t passed_ms = 1000 * (clock () - start) / CLOCKS_PER_SEC;
              if (passed_ms % 500 == 0)
                set_func (klass, (original_len - length) * 100 / original_len,
                          _ ("Decompressing."));
            }
          if (result == G_CONVERTER_ERROR)
            {
              /* There's no space in buf */
              if (internal_err->code == G_IO_ERROR_NO_SPACE)
                {
                  buf_len += 128;
                  buf_p_len += 128;
                  buf_data_p = g_realloc (buf_data_p, buf_p_len);
                  buf_data = buf_data_p + (buf_p_len - buf_len);
                  g_error_free (internal_err);
                  continue;
                }
              if (err)
                {
                  *err = internal_err;
                  break;
                }
            }
          data += current_consumed_len;
          length -= current_consumed_len;
          buf_data += current_write_len;
          buf_len -= current_write_len;
          if (buf_len == 0)
            {
              buf_len += 128;
              buf_p_len += 128;
              buf_data_p = g_realloc (buf_data_p, buf_p_len);
              buf_data = buf_data_p + (buf_p_len - buf_len);
            }
        }

      if (err && *err || result == G_CONVERTER_ERROR)
        {
          g_free (buf_data_p);
          return NULL;
        }

      buffer = init_buffer (buf_data_p, buf_p_len - buf_len);

      /* Cancelled */
      if (g_cancellable_is_cancelled (cancellable))
        {
          g_free (buffer->data);
          g_free (buffer);
          g_set_error_literal (err, NBT_GLIB_PARSE_ERROR,
                               NBT_GLIB_PARSE_ERROR_CANCELLED,
                               _ ("The parsing progress has been cancelled."));
          return NULL;
        }
    }
  else
    {
      guint8 *data_dup = g_new0 (guint8, length);
      memcpy (data_dup, data, length);
      buffer = init_buffer (data_dup, length);
    }

  NbtNode *root = create_nbt (TAG_End);
  int ret = parse_value (root, buffer, 0, set_func, klass, cancellable, min,
                         max, clock (), err);
  g_free (buffer->data);

  if (ret != 0)
    {
      g_node_destroy (root);
      g_free (buffer);
      return NULL;
    }
  else
    {
      if (set_func && klass)
        set_func (klass, max, _ ("Parsing finished!"));
      if (buffer->pos != buffer->len)
        g_set_error_literal (err, NBT_GLIB_PARSE_ERROR,
                             NBT_GLIB_PARSE_ERROR_LEFTOVER_DATA,
                             _ ("Some leftover text detected after parsing."));
      g_free (buffer);
      return root;
    }
}

NbtNode *
nbt_node_new_from_filename (const char *filename, GError **err,
                            DhProgressFullSet set_func, void *main_klass,
                            GCancellable *cancellable, int min, int max)
{
  if (set_func && main_klass)
    set_func (main_klass, min, _ ("Parsing file."));
  GError *internal_err = NULL;
  GFile *file = g_file_new_for_path (filename);
  guint8 *data = NULL;
  gsize len = 0;
  if (!g_file_load_contents (file, cancellable, &data, &len, NULL,
                             &internal_err))
    {
      if (err)
        *err = internal_err;
      else
        g_error_free (internal_err);
      g_object_unref (file);
      if (set_func && main_klass)
        set_func (main_klass, max, _ ("Parsing file failed."));
      return NULL;
    }
  NbtNode *ret = nbt_node_new_opt (data, len, err, set_func, main_klass,
                                   cancellable, min, max);
  g_object_unref (file);
  g_free (data);
  return ret;
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