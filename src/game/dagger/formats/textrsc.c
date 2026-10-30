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

int textrsc_read(int id, char *out, int cap)
{
    /* The longest record, the class questions', is 29 KB. */
    static df_u8 record[32768];
    VfsFile *file;
    df_u32 from;
    df_u32 to;
    df_u32 i;
    int n;

    file = vfs_open(FILE_NAME);
    if (!file) {
        return -1;
    }
    if (!find_record(file, id, &from, &to) || to - from > sizeof(record)
        || !vfs_read_at(file, from, record, to - from)) {
        vfs_close(file);
        return -1;
    }
    vfs_close(file);
    n = 0;
    for (i = 0; i < to - from && record[i] != RECORD_END && n < cap - 1; i++) {
        if (record[i] == LINE_END || record[i] == LINE_END_TOO) {
            out[n++] = '\n';
        } else if (record[i] >= 32 && record[i] < 127) {
            out[n++] = (char)record[i];
        }
    }
    out[n] = 0;
    return n;
}
