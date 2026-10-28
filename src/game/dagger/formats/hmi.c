#include "game/dagger/formats/hmi.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/sound/music.h"

/*
 * See hmi.h. The tracks are read one after another, each event put at
 * its own tick, and the list sorted once at the end (song_sort): a
 * track's own order stays within a tick.
 */

#define HMI_MAGIC "HMI-MIDISONG061595"
#define TRACK_MAGIC "HMI-MIDITRACK"
#define DIVISION_AT 0xD4
#define TRACKS_AT 0xE4
#define TRACK_DIR_AT 0xE8
#define TRACK_DATA_AT 0x57
#define TRACK_DEVS_AT 0x99
#define DEVS 8

#define BSA_NAME "MIDI.BSA"
/* Bytes of the largest song read at once; DUNGEON.HMI is 34678. */
#define SONG_BYTES 65536u

static int starts_with(const df_u8 *p, df_u32 left, const char *magic)
{
    df_u32 i;

    for (i = 0; magic[i]; i++) {
        if (i >= left || p[i] != (df_u8)magic[i]) {
            return 0;
        }
    }
    return 1;
}

typedef struct Reader {
    const df_u8 *p;
    df_u32 at;
    df_u32 end;
    int bad;
} Reader;

static df_u32 byte(Reader *r)
{
    if (r->at >= r->end) {
        r->bad = 1;
        return 0;
    }
    return r->p[r->at++];
}

/* Standard MIDI variable length: seven bits a byte, high bit set on all but the last. */
static df_u32 varlen(Reader *r)
{
    df_u32 v;
    df_u32 c;
    int n;

    v = 0;
    for (n = 0; n < 5; n++) {
        c = byte(r);
        v = (v << 7) | (c & 0x7fu);
        if (!(c & 0x80u)) {
            break;
        }
    }
    return v;
}

/* Does a track's device list play here? ZMusic's CheckCaps, for an AWE32 or an OPL3. */
static int plays(const df_u8 *devs, int fm)
{
    int k;

    for (k = 0; k < DEVS; k++) {
        df_u16 d;

        d = df_ru16(devs + k * 2);
        if (d == 0) {
            break;
        }
        if (fm ? (d == HMI_DEV_OPL3 || d == HMI_DEV_OPL2)
               : (d == HMI_DEV_SBAWE32 || d == HMI_DEV_GM)) {
            return 1;
        }
    }
    return 0;
}

typedef struct Build {
    SongEvent *ev;
    int count;
    int full;
    df_u32 last;
} Build;

static void add(Build *b, df_u32 tick, df_u32 msg)
{
    if (b->count >= MUSIC_EVENTS) {
        b->full = 1;
        return;
    }
    b->ev[b->count].tick = tick;
    b->ev[b->count].msg = msg;
    b->count++;
    if (tick > b->last) {
        b->last = tick;
    }
}

static void read_track(Build *b, const df_u8 *bytes, df_u32 start, df_u32 end)
{
    Reader r;
    df_u32 tick;
    df_u32 status;

    r.p = bytes;
    r.at = start;
    r.end = end;
    r.bad = 0;
    tick = varlen(&r);
    status = 0;
    while (!r.bad && r.at < r.end && !b->full) {
        df_u32 ev;

        ev = byte(&r);
        if (ev == 0xffu) {
            df_u32 type;
            df_u32 len;

            type = byte(&r);
            len = varlen(&r);
            if (type == 0x2fu) {
                break;
            }
            if (type == 0x51u && len >= 3 && r.at + 3 <= r.end) {
                add(b, tick, SONG_TEMPO | ((df_u32)bytes[r.at] << 16)
                                        | ((df_u32)bytes[r.at + 1] << 8) | bytes[r.at + 2]);
            }
            r.at += len;
        } else if (ev == 0xf0u || ev == 0xf7u) {
            r.at += varlen(&r);
        } else if (ev == 0xfeu) {
            /* HMI's own: loop and branch marks. Lengths as ZMusic skips them. */
            df_u32 type;

            type = byte(&r);
            if (type == 0x13u || type == 0x15u) {
                r.at += 6;
            } else if (type == 0x12u || type == 0x14u) {
                r.at += 2;
            } else if (type == 0x10u) {
                r.at += 2;
                if (r.at < r.end) {
                    r.at += (df_u32)bytes[r.at] + 5u;
                }
            } else {
                break;
            }
        } else if (ev >= 0xf1u) {
            /* System common: no channel, nothing for the synthesizer. */
            if (ev == 0xf1u || ev == 0xf3u) {
                r.at += 1;
            } else if (ev == 0xf2u) {
                r.at += 2;
            }
        } else {
            df_u32 d1;
            df_u32 d2;
            df_u32 hi;

            if (ev & 0x80u) {
                status = ev;
                d1 = byte(&r);
            } else {
                d1 = ev;
                ev = status;
            }
            if (!(ev & 0x80u)) {
                break;
            }
            hi = ev & 0xf0u;
            d2 = (hi == 0xc0u || hi == 0xd0u) ? 0 : byte(&r);
            add(b, tick, ev | ((d1 & 0x7fu) << 8) | ((d2 & 0x7fu) << 16));
            if (hi == 0x90u) {
                /* The note's length: its note-off is made here. */
                df_u32 dur;

                dur = varlen(&r);
                add(b, tick + dur, (0x80u | (ev & 0x0fu)) | ((d1 & 0x7fu) << 8));
            }
        }
        if (r.at >= r.end) {
            break;
        }
        tick += varlen(&r);
    }
    if (tick > b->last) {
        b->last = tick;
    }
}

/*
 * A song ends with its music. SUNNYDAY's fifth track ends its last note
 * at tick 6801 and then waits a delta of 1,192,560 ticks (2.8 hours at
 * 120 a second) before a reverb setting and its end, so the song took
 * hours to come round to its loop and was silent the while. Every other
 * song's last event is within 200 ticks of its last note. So an event
 * more than TAIL_MAX after the last note moves to that note's tick, and
 * the song is as long as what is left. In the song's ticks: 8 s at the
 * 120 a second every Daggerfall song runs at.
 */
#define TAIL_MAX 960u

static void trim_tail(Song *song)
{
    df_u32 note;
    df_u32 last;
    int i;

    note = 0;
    for (i = 0; i < song->count; i++) {
        df_u32 m;

        m = song->events[i].msg;
        if ((m & 0xf0u) == 0x80u && song->events[i].tick > note) {
            note = song->events[i].tick;
        }
    }
    if (song->length <= note + TAIL_MAX) {
        return;
    }
    last = note;
    for (i = 0; i < song->count; i++) {
        if (song->events[i].tick > note + TAIL_MAX) {
            song->events[i].tick = note;
        }
        if (song->events[i].tick > last) {
            last = song->events[i].tick;
        }
    }
    log_print(LOG_INFO, "hmi: a stray tail of %u ticks after the last note, cut",
              (unsigned int)(song->length - last));
    song->length = last;
}

int hmi_parse(const df_u8 *bytes, df_u32 size, int fm, Song *out)
{
    Build b;
    df_u32 dir;
    int ntracks;
    int i;

    if (!bytes || !out || size < 0x100 || !starts_with(bytes, size, HMI_MAGIC)) {
        return 0;
    }
    ntracks = (int)df_ru16(bytes + TRACKS_AT);
    dir = df_ru32(bytes + TRACK_DIR_AT);
    if (ntracks <= 0 || dir > size || (df_u32)ntracks * 4u > size - dir) {
        return 0;
    }
    b.ev = music_scratch();
    b.count = 0;
    b.full = 0;
    b.last = 0;
    if (!b.ev) {
        return 0;
    }
    for (i = 0; i < ntracks; i++) {
        df_u32 start;
        df_u32 end;
        df_u32 data;

        start = df_ru32(bytes + dir + (df_u32)i * 4u);
        end = i + 1 < ntracks ? df_ru32(bytes + dir + (df_u32)(i + 1) * 4u) : size;
        if (end > size) {
            end = size;
        }
        if (start >= end || end - start < TRACK_DEVS_AT + DEVS * 2
            || !starts_with(bytes + start, end - start, TRACK_MAGIC)) {
            continue;
        }
        if (!plays(bytes + start + TRACK_DEVS_AT, fm)) {
            continue;
        }
        data = df_ru32(bytes + start + TRACK_DATA_AT);
        if (data >= end - start) {
            continue;
        }
        read_track(&b, bytes, start + data, end);
    }
    if (b.full) {
        log_text(LOG_WARN, "hmi: more events than a song may have");
        return 0;
    }
    if (b.count == 0) {
        return 0;
    }
    out->events = b.ev;
    out->count = b.count;
    out->division = (int)df_ru16(bytes + DIVISION_AT) << 2;
    out->tempo = 4000000u;
    out->length = b.last;
    if (out->division <= 0) {
        return 0;
    }
    trim_tail(out);
    song_sort(out);
    return 1;
}

/* MIDI.BSA's directory, kept: names, offsets and sizes in file order. */
static int g_tried;
static int g_count;
static char (*g_names)[13];
static df_u32 *g_offset;
static df_u32 *g_size;
static df_u8 *g_bytes;

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
    f = vfs_open(BSA_NAME);
    if (!f) {
        log_text(LOG_WARN, "hmi: no " BSA_NAME);
        return 0;
    }
    file_size = vfs_size(f);
    if (file_size < 4 || !vfs_read_at(f, 0, head, 4) || df_ru16(head + 2) != 0x0100) {
        vfs_close(f);
        return 0;
    }
    n = (int)df_ru16(head);
    dir_bytes = (df_u32)n * 18u;
    if (n == 0 || dir_bytes > file_size - 4) {
        vfs_close(f);
        return 0;
    }
    dir = (df_u8 *)mem_keep(dir_bytes);
    g_names = (char (*)[13])mem_keep((df_u32)n * 13u);
    g_offset = (df_u32 *)mem_keep((df_u32)n * 4u);
    g_size = (df_u32 *)mem_keep((df_u32)n * 4u);
    if (!dir || !g_names || !g_offset || !g_size
        || !vfs_read_at(f, file_size - dir_bytes, dir, dir_bytes)) {
        vfs_close(f);
        return 0;
    }
    vfs_close(f);
    cursor = 4;
    for (i = 0; i < n; i++) {
        int k;

        for (k = 0; k < 12 && dir[i * 18 + k] && dir[i * 18 + k] != ' '; k++) {
            g_names[i][k] = (char)dir[i * 18 + k];
        }
        g_names[i][k] = 0;
        g_size[i] = df_ru32(dir + i * 18 + 14);
        g_offset[i] = cursor;
        if (g_size[i] > file_size - dir_bytes - cursor) {
            return 0;
        }
        cursor += g_size[i];
    }
    g_count = n;
    return 1;
}

int hmi_count(void)
{
    return open_dir() ? g_count : 0;
}

const char *hmi_name(int index)
{
    if (!open_dir() || index < 0 || index >= g_count) {
        return "";
    }
    return g_names[index];
}

int hmi_load(const char *name, int fm, Song *out)
{
    VfsFile *f;
    int i;

    if (!name || !open_dir()) {
        return 0;
    }
    for (i = 0; i < g_count; i++) {
        if (df_eq(g_names[i], name)) {
            break;
        }
    }
    if (i == g_count || g_size[i] > SONG_BYTES) {
        return 0;
    }
    if (!g_bytes) {
        g_bytes = (df_u8 *)mem_keep(SONG_BYTES);
    }
    f = vfs_open(BSA_NAME);
    if (!g_bytes || !f) {
        vfs_close(f);
        return 0;
    }
    if (!vfs_read_at(f, g_offset[i], g_bytes, g_size[i])) {
        vfs_close(f);
        return 0;
    }
    vfs_close(f);
    return hmi_parse(g_bytes, g_size[i], fm, out);
}
