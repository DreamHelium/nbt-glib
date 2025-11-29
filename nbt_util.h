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

#ifndef DHLRC_NBT_UTIL_H
#define DHLRC_NBT_UTIL_H

#include "nbt.h"

G_BEGIN_DECLS

NbtNode *nbt_node_new_byte (const char *key, gint8 value);
NbtNode *nbt_node_new_short (const char *key, gint16 value);
NbtNode *nbt_node_new_int (const char *key, gint32 value);
NbtNode *nbt_node_new_long (const char *key, gint64 value);
NbtNode *nbt_node_new_float (const char *key, float value);
NbtNode *nbt_node_new_double (const char *key, double value);
NbtNode *nbt_node_new_string (const char *key, const char *value);
NbtNode *nbt_node_new_byte_array (const char *key, gint8 *value, int len);
NbtNode *nbt_node_new_int_array (const char *key, gint32 *value, int len);
NbtNode *nbt_node_new_long_array (const char *key, gint64 *value, int len);
NbtNode *nbt_node_new_compound (const char *key);
NbtNode *nbt_node_new_list (const char *key);
gint8 nbt_node_get_byte (const NbtNode *node, gboolean *failed);
gint16 nbt_node_get_short (const NbtNode *node, gboolean *failed);
gint32 nbt_node_get_int (const NbtNode *node, gboolean *failed);
gint64 nbt_node_get_long (const NbtNode *node, gboolean *failed);
float nbt_node_get_float (const NbtNode *node, gboolean *failed);
double nbt_node_get_double (const NbtNode *node, gboolean *failed);
const char *nbt_node_get_string (const NbtNode *node, gboolean *failed);
const gint8 *nbt_node_get_byte_array (const NbtNode *node, int *len,
                                      gboolean *failed);
const gint32 *nbt_node_get_int_array (const NbtNode *node, int *len,
                                      gboolean *failed);
const gint64 *nbt_node_get_long_array (const NbtNode *node, int *len,
                                       gboolean *failed);
const char *nbt_node_get_key (const NbtNode *node);
void nbt_node_reset_key (const NbtNode *node, const char *key);
gboolean nbt_node_prepend (NbtNode *node, NbtNode *child);
gboolean nbt_node_append (NbtNode *node, NbtNode *child);
gboolean nbt_node_insert_before (NbtNode *parent, NbtNode *sibling,
                                 NbtNode *node);
gboolean nbt_node_insert_after (NbtNode *parent, NbtNode *sibling,
                                NbtNode *node);
NbtNode *nbt_node_child_to_index (NbtNode *root, int index);
NbtNode *nbt_node_child_to_key (NbtNode *root, const char *key);
gboolean nbt_node_remove_node_index (NbtNode *root, int index);
gboolean nbt_node_remove_node_key (NbtNode *root, const char *key);
NbtNode *nbt_node_dup (NbtNode *root);

G_END_DECLS

#endif // DHLRC_NBT_UTIL_H
