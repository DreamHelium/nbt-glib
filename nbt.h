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
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

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

// There's always 1024 (32*32) chunks in a region file
#define CHUNKS_IN_REGION 1024

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

    typedef struct MCA
    {
        // raw nbt data
        uint8_t *rawdata[CHUNKS_IN_REGION];
        // raw nbt data length
        uint32_t size[CHUNKS_IN_REGION];
        // mca chunk modify time
        uint32_t epoch[CHUNKS_IN_REGION];
        // parsed nbt data
        NbtNode *data[CHUNKS_IN_REGION];
        // if region position is defined
        uint8_t hasPosition;
        int x;
        int z;
    } MCA;

    typedef struct NBT_Error
    {
        // Error ID, see above
        int errid;
        // Error position
        int position;
    } NBT_Error;

    /**
     * @brief The full progress setting function
     * @param klass The class of the progress
     * @param value The value of the progress
     * @param message The message of the current progress
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
    NbtNode *nbt_node_new_with_progress (uint8_t *data, size_t length,
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
    NbtNode *nbt_node_new (uint8_t *data, size_t length);
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
    NbtNode *nbt_node_new_opt (uint8_t *data, size_t length, NBT_Error *err,
                               DhProgressFullSet set_func, void *klass,
                               GCancellable *cancellable, int min, int max);
    /**
     * @brief Free the node.
     * @param node The root node needed to be freed.
     */
    void nbt_node_free (NbtNode *node);
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
    /* Reserved for further change. */
    MCA *MCA_Init (const char *filename);
    MCA *MCA_Init_WithPos (int x, int z);
    int MCA_ReadRaw (uint8_t *data, size_t length, MCA *mca,
                     int skip_chunk_error);
    int MCA_ReadRaw_File (FILE *fp, MCA *mca, int skip_chunk_error);
    int MCA_WriteRaw_File (FILE *fp, MCA *mca);
    int MCA_ParseAll (MCA *mca);
    void MCA_Free (MCA *mca);
    /**
     * @brief Reset the text when parsing NBT to NbtNode.
     * @param text The changed text
     */
    void nbt_glib_set_parsing_nbt_to_node_txt (const char *text);
    /**
     * @brief Reset the text when the task is over.
     * @param text The changed text
     */
    void nbt_glib_set_finish_txt (const char *text);

#ifdef __cplusplus
}
#endif

#endif