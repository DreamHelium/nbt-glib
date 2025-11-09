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

#ifndef DHLRC_NBT_PARSE_H
#define DHLRC_NBT_PARSE_H

#include <gio/gio.h>
#include <glib.h>
#include <stdint.h>

G_BEGIN_DECLS

/**
 * @brief The error domain of the parse error.
 * @sa NbtGlibParseError
 */
#define NBT_GLIB_PARSE_ERROR nbt_glib_parse_error_quark ()

/**
 * @brief The error code of the parse error.
 */
typedef enum
{
    /** The loading text is not a valid NBT */
    NBT_GLIB_PARSE_ERROR_INTERRUPTED,
    /** Uncompress error */
    NBT_GLIB_PARSE_ERROR_UNCOMPRESS_ERROR,
    /** After parsing, some data is leftover */
    NBT_GLIB_PARSE_ERROR_LEFTOVER_DATA,
    /** Internal Error */
    NBT_GLIB_PARSE_ERROR_INTERNAL,
    /** Parse cancelled */
    NBT_GLIB_PARSE_ERROR_CANCELLED,
    /** Invalid tag */
    NBT_GLIB_PARSE_ERROR_INVALID_TAG,
} NbtGlibParseError;

/**
 *  @brief enumerations for available NBT tags
 *  @sa https://minecraft.wiki/w/NBT_format
 */
typedef enum NBT_Tags
{
    TAG_End,
    TAG_Byte,
    TAG_Short,
    TAG_Int,
    TAG_Long,
    TAG_Float,
    TAG_Double,
    TAG_Byte_Array,
    TAG_String,
    TAG_List,
    TAG_Compound,
    TAG_Int_Array,
    TAG_Long_Array
} NBT_Tags;

/**
 * @brief The data in the `NbtNode`
 */
typedef struct NbtData
{
    /** NBT tag. see `NBT_Tags` */
    enum NBT_Tags type;

    /** NBT tag name. Nullable when no name defined. '\0' ended */
    char *key;

    /** NBT tag data. */
    union
    {

        /**
         * @brief Numerical (Integer) data
         *
         * Used when tag=[`TAG_Byte`, `TAG_Short`, `TAG_Int`, `TAG_Long`]
         */
        int64_t value_i;

        /**
         * @brief Float data
         *
         * Used when tag=[`TAG_Float`, `TAG_Double`] */
        double value_d;

        /**
         * @brief Array data
         *
         * Used when tag=[`TAG_Byte_Array`, `TAG_Int_Array`,
         * `TAG_Long_Array`, `TAG_String`]
         *
         * When using `TAG_String`, the `len` can be ignored since it might
         * not be the original len. (Converted from MUTF-8)
         */
        struct
        {
            /**
             * @brief Array data
             *
             * Note that the data is converted to UTF-8 from MUTF-8 when
             * it's `TAG_String`
             */
            void *value;
            /**
             * @brief Array length, or a useless value when it's a
             * `TAG_String`.
             */
            int32_t len;
        } value_a;
    };
} NbtData;

/**
 * @brief The base node of the NBT.
 */
typedef GNode NbtNode;

/**
 * @brief The full progress setting function
 * @param klass The class of the progress
 * @param value The value of the progress
 * @param message The message of the progress
 */
typedef void (*DhProgressFullSet) (void *klass, int value,
                                   const char *message);

/**
 * @brief Create a new NBT node from data
 * @param data The original data of NBT
 * @param length The length of the data
 * @param set_func The setting function for progress
 * @param main_klass The class of the progress
 * @param cancellable Cancellable object
 * @param min The minimum value of the progress
 * @param max The maximum value of the progress
 * @return The node of the NBT, or NULL when cancelled or failed.
 */
NbtNode *nbt_node_new_with_progress (guint8 *data, size_t length,
                                     DhProgressFullSet set_func,
                                     void *main_klass,
                                     GCancellable *cancellable, int min,
                                     int max);
/**
 * @brief Create a new NBT node from data, without progress
 * @param data The original data of NBT
 * @param length The length of the data
 * @return The node of the NBT, or NULL when failed.
 */
NbtNode *nbt_node_new (guint8 *data, size_t length);
/**
 * @brief Create a new NBT node from data, with progress and error
 * @param data The original data of NBT
 * @param length The length of the data
 * @param err Error code (in later version will be `GError`)
 * @param set_func The setting function for progress
 * @param klass The class of the progress
 * @param cancellable Cancellable object
 * @param min The minimum value of the progress
 * @param max The maximum value of the progress
 * @return The node of the NBT, or NULL when cancelled or failed.
 */
NbtNode *nbt_node_new_opt (guint8 *data, size_t length, GError **err,
                           DhProgressFullSet set_func, void *klass,
                           GCancellable *cancellable, int min, int max);
/**
 * @brief Free the node.
 * @param node The root node needed to be freed.
 */
void nbt_node_free (NbtNode *node);

G_END_DECLS

#endif // DHLRC_NBT_PARSE_H
