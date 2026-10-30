#include "game/dagger/formats/faces.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See faces.h. */

#define FLATS_NAME "FLATS.CFG"
#define RCI_NAME "TFAC00I0.RCI"
/* A flat's lines, and how many flats there can be: the file holds about 600. */
#define BLOCK_LINES 6
#define FLATS_MOST 1024

typedef struct FlatFace {
    df_u16 id;
    df_u16 face;
} FlatFace;

static FlatFace *g_flats;
static int g_nflats;
static int g_tried;

/* DFU's GetFlatID: an archive and a record as one number. */
static int flat_id(int archive, int record)
{
    return (archive << 7) + record;
}

/* The number at the start of a line, past blanks; a line's first character may be a mark (FLATS' genders' ?). */
static int line_int(const char *s, const char *end, const char **after)
{
    int v;
    int sign;

    while (s < end && (*s == ' ' || *s == '\t')) {
        s++;
    }
    sign = 1;
    if (s < end && *s == '-') {
        sign = -1;
        s++;
    }
    v = 0;
    while (s < end && *s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    if (after) {
        *after = s;
    }
    return v * sign;
}

/* Reads the flats' table, the first time. */
static void read_flats(void)
{
    const char *s;
    const char *end;
    df_u8 *bytes;
    df_u32 size;
    int line;
    int id;

    if (g_tried) {
        return;
    }
    g_tried = 1;
    bytes = vfs_read(FLATS_NAME, &size);
    g_flats = bytes ? (FlatFace *)mem_keep((df_u32)FLATS_MOST * (df_u32)sizeof(FlatFace)) : 0;
    if (!g_flats) {
        log_text(LOG_WARN, "faces: " FLATS_NAME " did not read");
        return;
    }
    s = (const char *)bytes;
    end = s + size;
    line = 0;
    id = 0;
    while (s < end && g_nflats < FLATS_MOST) {
        const char *start;
        const char *stop;

        start = s;
        while (s < end && *s != '\n') {
            s++;
        }
        stop = s;
        s++;
        while (stop > start && stop[-1] == '\r') {
            stop--;
        }
        /* A blank line ends a block, however far it got. */
        if (stop == start) {
            line = 0;
            continue;
        }
        if (line == 0) {
            const char *after;
            int archive;

            archive = line_int(start, stop, &after);
            id = flat_id(archive, line_int(after, stop, 0));
        } else if (line == BLOCK_LINES - 1) {
            int i;
            int have;

            have = 0;
            for (i = 0; i < g_nflats; i++) {
                have |= g_flats[i].id == (df_u16)id;
            }
            if (!have) {
                g_flats[g_nflats].id = (df_u16)id;
                g_flats[g_nflats].face = (df_u16)line_int(start, stop, 0);
                g_nflats++;
            }
        }
        line++;
    }
}

int faces_of_flat(int archive, int record)
{
    int id;
    int i;

    read_flats();
    id = flat_id(archive, record);
    for (i = 0; i < g_nflats; i++) {
        if (g_flats[i].id == (df_u16)id) {
            return g_flats[i].face;
        }
    }
    return FACE_NONE;
}

int faces_read(int record, df_u8 *out)
{
    VfsFile *file;
    int ok;

    file = vfs_open(RCI_NAME);
    if (!file) {
        return 0;
    }
    ok = record >= 0 && (df_u32)(record + 1) * (FACE_W * FACE_H) <= vfs_size(file)
         && vfs_read_at(file, (df_u32)record * (FACE_W * FACE_H), out, FACE_W * FACE_H);
    vfs_close(file);
    return ok;
}
