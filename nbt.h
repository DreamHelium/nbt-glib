/*  libnbt - Minecraft NBT/MCA/SNBT file parser in C
    Copyright (C) 2020 djytw

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

/* Modified by Dream_Helium.
 *
 */

#ifndef NBT_H
#define NBT_H
#include <gio/gio.h>
#include <glib.h>
#include <stdint.h>
#include "nbt_parse.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief The Compression mode
     */
    typedef enum NBT_Compression
    {
        NBT_Compression_GZIP = 1,
        NBT_Compression_ZLIB = 2,
        NBT_Compression_NONE = 3,
    } NBT_Compression;

// Error code
#define LIBNBT_ERROR_MASK 0xf0000000
#define LIBNBT_ERROR_INTERNAL                                                 \
    (LIBNBT_ERROR_MASK | 0x1) // Internal error, maybe a bug?
#define LIBNBT_ERROR_EARLY_EOF                                                \
    (LIBNBT_ERROR_MASK                                                        \
     | 0x2) // An unexpected EOF was met, maybe the file is incomplete?
#define LIBNBT_ERROR_LEFTOVER_DATA                                            \
    (LIBNBT_ERROR_MASK | 0x3) // Extra data after it supposed to end, maybe the
                              // file is corrupted
#define LIBNBT_ERROR_INVALID_DATA                                             \
    (LIBNBT_ERROR_MASK                                                        \
     | 0x4) // Invalid data detected, maybe the file is corrupted
#define LIBNBT_ERROR_BUFFER_OVERFLOW                                          \
    (LIBNBT_ERROR_MASK | 0x5) // The buffer you allocated is not enough, please
                              // use a larger buffer
#define LIBNBT_ERROR_UNZIP_ERROR                                              \
    (LIBNBT_ERROR_MASK | 0x6) // Occurs when the NBT file is compressed, but
                              // failed to decompress, the file is corrupted.

    /**
     * @brief Pack the NBT node as the NBT text, if `file` is NULL, output mode
     * will be enabled.
     * @param node The root node needed to pack as NBT text
     * @param length The length of the returned text, which can't be NULL when
     * using as the output mode
     * @param compression Compression mode
     * @param error Error code, or NULL to ignore
     * @param set_func The setting function for progress
     * @param main_klass The main class of the progress
     * @param cancellable Cancellable object
     * @param file File object, or NULL if using as the output mode
     * @return The text when in output mode, or NULL when writing to the file
     */
    uint8_t *nbt_node_pack_full (NbtNode *node, size_t *length,
                                 NBT_Compression compression, GError **error,
                                 DhProgressFullSet set_func, void *main_klass,
                                 GCancellable *cancellable, GFile *file);
    uint8_t *nbt_node_to_snbt_full (NbtNode *node, size_t *length,
                                    GError **error, int max_level,
                                    gboolean pretty_output, gboolean space,
                                    DhProgressFullSet set_func,
                                    void *main_klass,
                                    GCancellable *cancellable, GFile *file);

#ifdef __cplusplus
}
#endif

#endif