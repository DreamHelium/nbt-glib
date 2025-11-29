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

#include "nbt_util.h"

static NbtNode *
create_node (NBT_Tags tag, const char *key)
{
  NbtData *data = g_new0 (NbtData, 1);
  data->type = tag;
  data->key = g_strdup (key);
  NbtNode *node = g_node_new (data);
  return node;
}

NbtNode *
nbt_node_new_byte (const char *key, gint8 value)
{
  NbtNode *node = create_node (TAG_Byte, key);
  NbtData *data = node->data;
  data->value_i = value;
  return node;
}

NbtNode *
nbt_node_new_short (const char *key, gint16 value)
{
  NbtNode *node = create_node (TAG_Short, key);
  NbtData *data = node->data;
  data->value_i = value;
  return node;
}

NbtNode *
nbt_node_new_int (const char *key, gint32 value)
{
  NbtNode *node = create_node (TAG_Int, key);
  NbtData *data = node->data;
  data->value_i = value;
  return node;
}

NbtNode *
nbt_node_new_long (const char *key, gint64 value)
{
  NbtNode *node = create_node (TAG_Long, key);
  NbtData *data = node->data;
  data->value_i = value;
  return node;
}

NbtNode *
nbt_node_new_float (const char *key, float value)
{
  NbtNode *node = create_node (TAG_Float, key);
  NbtData *data = node->data;
  data->value_d = value;
  return node;
}

NbtNode *
nbt_node_new_double (const char *key, double value)
{
  NbtNode *node = create_node (TAG_Double, key);
  NbtData *data = node->data;
  data->value_d = value;
  return node;
}

NbtNode *
nbt_node_new_string (const char *key, const char *value)
{
  NbtNode *node = create_node (TAG_String, key);
  NbtData *data = node->data;
  data->value_a.value = g_strdup (value);
  data->value_a.len = 1; /* Since there's no need to fill it, we fill 1. */
  return node;
}

NbtNode *
nbt_node_new_byte_array (const char *key, gint8 *value, int len)
{
  NbtNode *node = create_node (TAG_Byte_Array, key);
  NbtData *data = node->data;
  data->value_a.len = len;
  data->value_a.value = g_new0 (gint8, len);
  memcpy (data->value_a.value, value, len * sizeof (gint8));
  return node;
}

NbtNode *
nbt_node_new_int_array (const char *key, gint32 *value, int len)
{
  NbtNode *node = create_node (TAG_Int_Array, key);
  NbtData *data = node->data;
  data->value_a.len = len;
  data->value_a.value = g_new0 (gint32, len);
  memcpy (data->value_a.value, value, len * sizeof (gint32));
  return node;
}

NbtNode *
nbt_node_new_long_array (const char *key, gint64 *value, int len)
{
  NbtNode *node = create_node (TAG_Long_Array, key);
  NbtData *data = node->data;
  data->value_a.len = len;
  data->value_a.value = g_new0 (gint64, len);
  memcpy (data->value_a.value, value, len * sizeof (gint64));
  return node;
}

NbtNode *
nbt_node_new_compound (const char *key)
{
  NbtNode *node = create_node (TAG_Compound, key);
  return node;
}

NbtNode *
nbt_node_new_list (const char *key)
{
  NbtNode *node = create_node (TAG_List, key);
  return node;
}

static void
fill_failed (gboolean *failed, gboolean value)
{
  if (failed)
    *failed = value;
}

gint8
nbt_node_get_byte (const NbtNode *node, gboolean *failed)
{
  if (!node)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Byte)
    {
      fill_failed (failed, FALSE);
      return data->value_i;
    }
fail:
  fill_failed (failed, TRUE);
  return -1;
}

gint16
nbt_node_get_short (const NbtNode *node, gboolean *failed)
{
  if (!node)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Short)
    {
      fill_failed (failed, FALSE);
      return data->value_i;
    }
fail:
  fill_failed (failed, TRUE);
  return -1;
}

gint32
nbt_node_get_int (const NbtNode *node, gboolean *failed)
{
  if (!node)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Int)
    {
      fill_failed (failed, FALSE);
      return data->value_i;
    }
fail:
  fill_failed (failed, TRUE);
  return -1;
}

gint64
nbt_node_get_long (const NbtNode *node, gboolean *failed)
{
  if (!node)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Long)
    {
      fill_failed (failed, FALSE);
      return data->value_i;
    }
fail:
  fill_failed (failed, TRUE);
  return -1;
}

float
nbt_node_get_float (const NbtNode *node, gboolean *failed)
{
  if (!node)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Float)
    {
      fill_failed (failed, FALSE);
      return data->value_d;
    }
fail:
  fill_failed (failed, TRUE);
  return 0.0f;
}

double
nbt_node_get_double (const NbtNode *node, gboolean *failed)
{
  if (!node)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Double)
    {
      fill_failed (failed, FALSE);
      return data->value_d;
    }
fail:
  fill_failed (failed, TRUE);
  return 0.0f;
}

const char *
nbt_node_get_string (const NbtNode *node, gboolean *failed)
{
  if (!node)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_String)
    {
      fill_failed (failed, FALSE);
      return data->value_a.value;
    }
fail:
  fill_failed (failed, TRUE);
  return NULL;
}

const gint8 *
nbt_node_get_byte_array (const NbtNode *node, int *len, gboolean *failed)
{
  if (!node || !len)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Byte_Array)
    {
      fill_failed (failed, FALSE);
      *len = data->value_a.len;
      return data->value_a.value;
    }
fail:
  fill_failed (failed, TRUE);
  return NULL;
}

const gint32 *
nbt_node_get_int_array (const NbtNode *node, int *len, gboolean *failed)
{
  if (!node || !len)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Int_Array)
    {
      fill_failed (failed, FALSE);
      *len = data->value_a.len;
      return data->value_a.value;
    }
fail:
  fill_failed (failed, TRUE);
  return NULL;
}

const gint64 *
nbt_node_get_long_array (const NbtNode *node, int *len, gboolean *failed)
{
  if (!node || !len)
    goto fail;
  NbtData *data = node->data;
  if (data->type == TAG_Long_Array)
    {
      fill_failed (failed, FALSE);
      *len = data->value_a.len;
      return data->value_a.value;
    }
fail:
  fill_failed (failed, TRUE);
  return NULL;
}

const char *
nbt_node_get_key (const NbtNode *node)
{
  g_return_val_if_fail (node, NULL);
  NbtData *data = node->data;
  return data->key;
}

void
nbt_node_reset_key (const NbtNode *node, const char *key)
{
  g_return_if_fail (node);
  if (node->parent)
    {
      NbtNode *parent = node->parent;
      NbtData *data = parent->data;
      g_return_if_fail (data->type != TAG_List);
    }

  NbtData *data = node->data;
  g_free (data->key);
  data->key = g_strdup (key);
}

gboolean
nbt_node_prepend (NbtNode *node, NbtNode *child)
{
  g_return_val_if_fail (node && child, FALSE);
  NbtData *data = node->data;
  g_return_val_if_fail (data->type == TAG_Compound || data->type == TAG_List,
                        FALSE);
  if (data->type == TAG_List && node->children)
    {
      NbtData *first_child_data = node->children->data;
      NbtData *child_data = child->data;
      g_return_val_if_fail (first_child_data->type == child_data->type, FALSE);
    }
  g_node_prepend (node, child);
  return TRUE;
}

gboolean
nbt_node_append (NbtNode *node, NbtNode *child)
{
  g_return_val_if_fail (node && child, FALSE);
  NbtData *data = node->data;
  g_return_val_if_fail (data->type == TAG_Compound || data->type == TAG_List,
                        FALSE);
  if (data->type == TAG_List && node->children)
    {
      NbtData *first_child_data = node->children->data;
      NbtData *child_data = child->data;
      g_return_val_if_fail (first_child_data->type == child_data->type, FALSE);
    }
  g_node_append (node, child);
  return TRUE;
}

gboolean
nbt_node_insert_before (NbtNode *parent, NbtNode *sibling, NbtNode *node)
{
  g_return_val_if_fail (node && parent, FALSE);
  g_return_val_if_fail (G_NODE_IS_ROOT (node), FALSE);
  NbtData *parent_data = parent->data;
  g_return_val_if_fail (parent_data->type == TAG_Compound
                            || parent_data->type == TAG_List,
                        FALSE);
  NbtData *node_data = node->data;
  if (parent_data->type == TAG_List)
    {
      if (sibling)
        {
          g_return_val_if_fail (sibling->parent == parent, FALSE);
          NbtData *sibling_data = sibling->data;
          g_return_val_if_fail (node_data->type == sibling_data->type, FALSE);
        }
      else
        {
          NbtNode *first_child = parent->children;
          if (first_child)
            {
              NbtData *first_child_data = first_child->data;
              g_return_val_if_fail (first_child_data->type == node_data->type,
                                    FALSE);
            }
        }
    }
  g_node_insert_before (parent, sibling, node);
  return TRUE;
}

gboolean
nbt_node_insert_after (NbtNode *parent, NbtNode *sibling, NbtNode *node)
{
  g_return_val_if_fail (node && parent, FALSE);
  g_return_val_if_fail (G_NODE_IS_ROOT (node), FALSE);
  NbtData *parent_data = parent->data;
  g_return_val_if_fail (parent_data->type == TAG_Compound
                            || parent_data->type == TAG_List,
                        FALSE);
  NbtData *node_data = node->data;
  if (parent_data->type == TAG_List)
    {
      if (sibling)
        {
          g_return_val_if_fail (sibling->parent == parent, FALSE);
          NbtData *sibling_data = sibling->data;
          g_return_val_if_fail (node_data->type == sibling_data->type, FALSE);
        }
      else
        {
          NbtNode *first_child = parent->children;
          if (first_child)
            {
              NbtData *first_child_data = first_child->data;
              g_return_val_if_fail (first_child_data->type == node_data->type,
                                    FALSE);
            }
        }
    }
  g_node_insert_after (parent, sibling, node);
  return TRUE;
}

NbtNode *
nbt_node_child_to_index (NbtNode *root, int index)
{
  g_return_val_if_fail (root && root->children, NULL);
  NbtNode *child = root->children;
  for (int i = 0; i < index; i++)
    {
      child = child->next;
      g_return_val_if_fail (child, NULL);
    }
  return child;
}

NbtNode *
nbt_node_child_to_key (NbtNode *root, const char *key)
{
  g_return_val_if_fail (root && root->children, NULL);
  NbtNode *child = root->children;
  while (child)
    {
      NbtData *data = child->data;
      if (g_str_equal (data->key, key))
        return child;
      child = child->next;
    }
  return NULL;
}

gboolean
nbt_node_remove_node_index (NbtNode *root, int index)
{
  g_return_val_if_fail (root, FALSE);
  NbtNode *node = nbt_node_child_to_index (root, index);
  g_return_val_if_fail (node, FALSE);
  g_node_unlink (node);
  nbt_node_free (node);
  return TRUE;
}

gboolean
nbt_node_remove_node_key (NbtNode *root, const char *key)
{
  g_return_val_if_fail (root, FALSE);
  NbtNode *node = nbt_node_child_to_key (root, key);
  g_return_val_if_fail (node, FALSE);
  g_node_unlink (node);
  nbt_node_free (node);
  return TRUE;
}

static gpointer
copy_func (gconstpointer src, gpointer data)
{
  const NbtData *src_data = src;
  NbtData *new_data = g_new0 (NbtData, 1);
  new_data->key = g_strdup (src_data->key);
  new_data->type = src_data->type;
  int type_len = 0;
  switch (src_data->type)
    {
    case TAG_Byte:
    case TAG_Short:
    case TAG_Int:
    case TAG_Long:
      new_data->value_i = src_data->value_i;
      return new_data;
    case TAG_Float:
    case TAG_Double:
      new_data->value_d = src_data->value_d;
      return new_data;
    case TAG_Compound:
    case TAG_List:
      return new_data;
    case TAG_String:
      new_data->value_a.value = g_strdup (src_data->value_a.value);
      new_data->value_a.len = src_data->value_a.len;
      return new_data;
    case TAG_Byte_Array:
      type_len = sizeof (gint8);
      goto copy;
    case TAG_Int_Array:
      type_len = sizeof (gint32);
      goto copy;
    case TAG_Long_Array:
      type_len = sizeof (gint64);
    copy:
      new_data->value_a.len = src_data->value_a.len;
      new_data->value_a.value = g_malloc0 (type_len * src_data->value_a.len);
      memcpy (new_data->value_a.value, src_data->value_a.value,
              type_len * src_data->value_a.len);
      return new_data;
    default:
      g_free (new_data->key);
      g_free (new_data);
      return NULL;
    }
}

NbtNode *
nbt_node_dup (NbtNode *root)
{
  g_return_val_if_fail (root, NULL);
  return g_node_copy_deep (root, copy_func, NULL);
}