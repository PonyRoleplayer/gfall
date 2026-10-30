#include "game/dagger/formats/textrsc.h"
#include "engine/assets/vfs.h"
#include "engine/core/df.h"

/* See textrsc.h. */

#define FILE_NAME "TEXT.RSC"
/* The header's end, and one entry: a u16 id and a u32 offset. */
#define ENTRY_BYTES 6
#define HEADER_MAX 16384
#define LINE_END 0xFD
#define LINE_END_TOO 0xFC
#define RECORD_END 0xFE
#define VARIANT_END 0xFF

/* The record's offset, and the next record's after it (the file's end for the last). 0 when it has no such id. */
static int find_record(VfsFile *file, int id, df_u32 *from, df_u32 *to)
{
    static df_u8 header[HEADER_MAX];
    df_u8 size_bytes[2];
    df_u32 size;
    df_u32 at;
    int found;

    if (!vfs_read_at(file, 0, size_bytes, 2)) {
        return 0;
    }
    size = df_ru16(size_bytes);
    if (size > HEADER_MAX || !vfs_read_at(file, 0, header, size)) {
        return 0;
    }
    found = 0;
    *to = vfs_size(file);
    for (at = 2; at + ENTRY_BYTES <= size; at += ENTRY_BYTES) {
        if ((int)df_ru16(header + at) == id) {
            *from = df_ru32(header + at + 2);
            found = 1;
        }
    }
    if (!found) {
        return 0;
    }
    /* The offsets are not promised in order: the next one up is where this one ends. */
    for (at = 2; at + ENTRY_BYTES <= size; at += ENTRY_BYTES) {
        df_u32 offset;

        offset = df_ru32(header + at + 2);
        if (offset > *from && offset < *to) {
            *to = offset;
        }
    }
    return 1;
}

/* The longest record, the class questions', is 29 KB. */
static df_u8 g_record[32768];

/* Record id's bytes in g_record; how many, -1 when it does not read. */
static int read_record(int id)
{
    VfsFile *file;
    df_u32 from;
    df_u32 to;

    file = vfs_open(FILE_NAME);
    if (!file) {
        return -1;
    }
    if (!find_record(file, id, &from, &to) || to - from > sizeof(g_record)
        || !vfs_read_at(file, from, g_record, to - from)) {
        vfs_close(file);
        return -1;
    }
    vfs_close(file);
    return (int)(to - from);
}

/* Bytes from g_record[i] up to the record's end, or a variant's when stop_at_variant, as plain text. Its length. */
static int plain_text(int i, int size, int stop_at_variant, char *out, int cap)
{
    int n;

    n = 0;
    for (; i < size && g_record[i] != RECORD_END && n < cap - 1; i++) {
        if (g_record[i] == VARIANT_END && stop_at_variant) {
            break;
        }
        if (g_record[i] == LINE_END || g_record[i] == LINE_END_TOO) {
            out[n++] = '\n';
        } else if (g_record[i] >= 32 && g_record[i] < 127) {
            out[n++] = (char)g_record[i];
        }
    }
    out[n] = 0;
    return n;
}

int textrsc_read(int id, char *out, int cap)
{
    int size;

    size = read_record(id);
    return size < 0 ? -1 : plain_text(0, size, 0, out, cap);
}

int textrsc_variants(int id)
{
    int size;
    int n;
    int i;

    size = read_record(id);
    if (size < 0) {
        return 0;
    }
    n = 1;
    for (i = 0; i < size && g_record[i] != RECORD_END; i++) {
        n += g_record[i] == VARIANT_END;
    }
    return n;
}

int textrsc_variant(int id, int which, char *out, int cap)
{
    int size;
    int n;
    int i;

    n = textrsc_variants(id);
    if (n < 1) {
        return -1;
    }
    which = which < 0 ? 0 : which % n;
    size = read_record(id);
    if (size < 0) {
        return -1;
    }
    i = 0;
    while (which > 0 && i < size) {
        which -= g_record[i] == VARIANT_END;
        i++;
    }
    return plain_text(i, size, 1, out, cap);
}
