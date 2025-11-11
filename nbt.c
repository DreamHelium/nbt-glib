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

#define bswap_16(x) GUINT16_SWAP_LE_BE (x)
#define bswap_32(x) GUINT32_SWAP_LE_BE (x)
#define bswap_64(x) GUINT64_SWAP_LE_BE (x)

typedef struct NBT_Buffer
{
  uint8_t *data;
  size_t len;
  size_t pos;
} NBT_Buffer;

static char *convert_string_to_mutf8 (const char *str);
static int nbt_node_write_nbt_to_gbytearray (GByteArray *arr, NbtNode *node,
                                             int writekey,
                                             DhProgressFullSet set_func,
                                             void *main_klass, int *n_node,
                                             int nodes, clock_t start_time,
                                             GCancellable *cancellable);

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
          nbt_node_write_uint8_to_gbytearray (arr, ((uint8_t *)value)[i]);
      }
      break;
    case TAG_Int_Array:
      {
        int i;
        for (i = 0; i < len; i++)
          nbt_node_write_uint32_to_gbytearray (arr, ((uint32_t *)value)[i]);
      }
      break;
    case TAG_Long_Array:
      {
        int i;
        for (i = 0; i < len; i++)
          nbt_node_write_uint64_to_gbytearray (arr, ((uint64_t *)value)[i]);
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
      nbt_node_write_number_to_gbytearray (arr, data->value_i, data->type);
      break;
    case TAG_Float:
    case TAG_Double:
      nbt_node_write_point_to_gbytearray (arr, data->value_d, data->type);
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
      ret = nbt_node_write_list_to_gbytearray (arr, node, set_func, main_klass,
                                               n_node, nodes, start_time,
                                               cancellable);
      return ret;
    case TAG_Compound:
      ret = nbt_node_write_compound_to_gbytearray (arr, node, set_func,
                                                   main_klass, n_node, nodes,
                                                   start_time, cancellable);
      return ret;
    default:
      return LIBNBT_ERROR_INTERNAL;
    }
  return 0;
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
                                              main_klass, &n, n_node, clock (),
                                              cancellable);
  if (ret || g_cancellable_is_cancelled (cancellable))
    {
      g_byte_array_free (buf, TRUE);
      GQuark error_domain = g_quark_from_string ("NBT_NODE_ERROR_CANCELLED");
      g_set_error_literal (error, error_domain, -1,
                           "The task was cancelled in packing process.");
      return NULL;
    }

  /* Compress the data */
  GBytes *original_bytes = g_byte_array_free_to_bytes (buf);
  gsize original_len = 0;
  guint8 *original_nbt = g_bytes_unref_to_data (original_bytes, &original_len);
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
          g_file_make_directory_with_parents (file, cancellable, error);
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
