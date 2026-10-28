#include "game/dagger/formats/sndfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/*
 * See sndfile.h. The directory is read once, at the end of the file:
 * a uint32 id and a uint32 size per record, the records packed from byte
 * 4 in the same order. It is kept, as the clips are, so a place's heap
 * dying takes none of it.
 */

#define SND_NAME "DAGGER.SND"
/* Most clip bytes kept: the place's sounds are a few hundred KB; all 459 would be 7.6 MB. */
#define SND_BUDGET (3u * 1024u * 1024u)

static int g_tried;
static int g_count;
static df_u32 *g_offset;
static df_u32 *g_size;
static df_u32 *g_id;
static const SoundClip **g_clip;
static df_u32 g_bytes;

static int open_dir(void)
{
    VfsFile *f;
    df_u8 head[4];
    df_u8 *dir;
    df_u32 file_size;
    df_u32 dir_bytes;
    df_u32 cursor;
    int n;
    int i;

    if (g_tried) {
        return g_count > 0;
    }
    g_tried = 1;
    f = vfs_open(SND_NAME);
    if (!f) {
        log_text(LOG_WARN, "sndfile: no " SND_NAME);
        return 0;
    }
    file_size = vfs_size(f);
    if (file_size < 4 || !vfs_read_at(f, 0, head, 4) || df_ru16(head + 2) != 0x0200) {
        vfs_close(f);
        return 0;
    }
    n = (int)df_ru16(head);
    dir_bytes = (df_u32)n * 8u;
    if (n == 0 || dir_bytes > file_size - 4) {
        vfs_close(f);
        return 0;
    }
    dir = (df_u8 *)mem_keep(dir_bytes);
    g_offset = (df_u32 *)mem_keep((df_u32)n * 4u);
    g_size = (df_u32 *)mem_keep((df_u32)n * 4u);
    g_id = (df_u32 *)mem_keep((df_u32)n * 4u);
    g_clip = (const SoundClip **)mem_keep((df_u32)n * (df_u32)sizeof(SoundClip *));
    if (!dir || !g_offset || !g_size || !g_id || !g_clip
        || !vfs_read_at(f, file_size - dir_bytes, dir, dir_bytes)) {
        vfs_close(f);
        return 0;
    }
    vfs_close(f);
    cursor = 4;
    for (i = 0; i < n; i++) {
        g_id[i] = df_ru32(dir + i * 8);
        g_size[i] = df_ru32(dir + i * 8 + 4);
        g_offset[i] = cursor;
        g_clip[i] = 0;
        if (g_size[i] > file_size - dir_bytes - cursor) {
            log_text(LOG_WARN, "sndfile: " SND_NAME " directory runs past the file");
            return 0;
        }
        cursor += g_size[i];
    }
    g_count = n;
    return 1;
}

int sndfile_count(void)
{
    return open_dir() ? g_count : 0;
}

df_u32 sndfile_bytes(void)
{
    return g_bytes;
}

int sndfile_by_id(df_u32 id)
{
    int i;

    if (!open_dir()) {
        return -1;
    }
    for (i = 0; i < g_count; i++) {
        if (g_id[i] == id) {
            return i;
        }
    }
    return -1;
}

const SoundClip *sndfile_clip(int index)
{
    VfsFile *f;
    SoundClip *clip;
    signed char *data;
    df_u32 n;
    df_u32 i;

    if (!open_dir() || index < 0 || index >= g_count) {
        return 0;
    }
    if (g_clip[index]) {
        return g_clip[index];
    }
    n = g_size[index];
    if (n == 0) {
        return 0;
    }
    if (g_bytes + n > SND_BUDGET) {
        log_print(LOG_WARN, "sndfile: clip %d would pass the %u KB kept for sound", index,
                  (unsigned int)(SND_BUDGET / 1024u));
        return 0;
    }
    clip = (SoundClip *)mem_keep((df_u32)sizeof(SoundClip) + n);
    f = vfs_open(SND_NAME);
    if (!clip || !f) {
        vfs_close(f);
        return 0;
    }
    data = (signed char *)(clip + 1);
    if (!vfs_read_at(f, g_offset[index], data, n)) {
        vfs_close(f);
        return 0;
    }
    vfs_close(f);
    /* Unsigned, silence at 0x80: the top bit flipped makes it signed. */
    for (i = 0; i < n; i++) {
        data[i] = (signed char)((df_u8)data[i] ^ 0x80u);
    }
    clip->data = data;
    clip->frames = n;
    clip->rate = 11025;
    g_bytes += n + (df_u32)sizeof(SoundClip);
    g_clip[index] = clip;
    return clip;
}
