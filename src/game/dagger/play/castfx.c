#include "game/dagger/play/castfx.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/tables/magic.h"
#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"

/* See castfx.h. */

#define FRAMES_MAX 64
#define FRAME_MAX (320 * 200)
/* A record's header: x, y, width, height, compression, the pixel data's length (six words). */
#define HEADER_BYTES 12

static const char *const g_names[ELEMENT_COUNT] = {
    "FIRE00C6.CIF", "FRST00C6.CIF", "POIS00C6.CIF", "SHOK00C6.CIF", "MJIC00C6.CIF"
};

static struct {
    int active;
    MemHeap *heap;
    const df_u8 *bytes;
    df_u32 size;
    /* Each frame's header offset in the file. */
    df_u32 at[FRAMES_MAX];
    int count;
    int shown;
    float time;
    int release_due;
    int released;
    int pic;
    int left;
    int top;
    int w;
    int h;
} g;

static df_u8 g_rgb[768];
static int g_have_rgb;
static df_u8 *g_buf;

static int word(const df_u8 *p)
{
    return p[0] | (p[1] << 8);
}

/* The file's records found one after the other, as far as they are whole. */
static void find_frames(void)
{
    df_u32 at;

    at = 0;
    g.count = 0;
    while (at + HEADER_BYTES <= g.size && g.count < FRAMES_MAX) {
        df_u32 len;

        len = (df_u32)word(g.bytes + at + 10);
        if (at + HEADER_BYTES + len > g.size || word(g.bytes + at + 4) < 1 || word(g.bytes + at + 6) < 1) {
            break;
        }
        g.at[g.count++] = at;
        at += HEADER_BYTES + len;
    }
}

/* Frame i decoded (RLE: a code over 127 repeats the next byte code - 127 times, else code + 1 bytes follow) and sent as a cutout. */
static void show_frame(int i)
{
    const df_u8 *h;
    const df_u8 *src;
    const df_u8 *end;
    int w;
    int hh;
    int comp;
    int pos;
    int total;

    if (g.pic) {
        pic_free(g.pic);
        g.pic = 0;
    }
    g.shown = i;
    h = g.bytes + g.at[i];
    w = word(h + 4);
    hh = word(h + 6);
    comp = word(h + 8);
    total = w * hh;
    if (total > FRAME_MAX || !g_have_rgb) {
        return;
    }
    if (!g_buf) {
        g_buf = (df_u8 *)mem_keep(FRAME_MAX);
    }
    if (!g_buf) {
        return;
    }
    src = h + HEADER_BYTES;
    end = src + word(h + 10);
    pos = 0;
    if (comp == 0) {
        for (; pos < total && src < end; pos++) {
            g_buf[pos] = *src++;
        }
    } else {
        while (pos < total && src < end) {
            int code;

            code = *src++;
            if (code > 127) {
                int n;
                df_u8 v;

                n = code - 127;
                v = src < end ? *src++ : 0;
                while (n-- > 0 && pos < total) {
                    g_buf[pos++] = v;
                }
            } else {
                int n;

                for (n = code + 1; n > 0 && pos < total && src < end; n--) {
                    g_buf[pos++] = *src++;
                }
            }
        }
    }
    for (; pos < total; pos++) {
        g_buf[pos] = 0;
    }
    g.w = w;
    g.h = hh;
    g.left = word(h + 0);
    g.top = word(h + 2);
    g.pic = pic_load(g_buf, w, hh, g_rgb, 1);
}

void castfx_free(void)
{
    if (g.pic) {
        pic_free(g.pic);
    }
    if (g.heap) {
        mem_heap_delete(g.heap);
    }
    mem_zero(&g, sizeof(g));
}

int castfx_begin(int element)
{
    MemHeap *was;

    castfx_free();
    if (element < 0 || element >= ELEMENT_COUNT) {
        return 0;
    }
    if (!g_have_rgb) {
        g_have_rgb = img_art_palette(g_rgb);
    }
    g.heap = mem_heap_new();
    if (!g.heap) {
        return 0;
    }
    was = mem_heap_use(g.heap);
    g.bytes = vfs_read(g_names[element], &g.size);
    mem_heap_use(was);
    if (!g.bytes) {
        castfx_free();
        return 0;
    }
    find_frames();
    if (g.count < 2) {
        castfx_free();
        return 0;
    }
    g.active = 1;
    g.release_due = (int)((float)g.count * CASTFX_RELEASE_SHARE);
    show_frame(0);
    return 1;
}

void castfx_step(float dt)
{
    int at;

    if (!g.active) {
        return;
    }
    g.time += dt;
    at = df_ifloor(g.time / CASTFX_SECONDS * (float)g.count);
    if (at >= g.count) {
        /* The release is never lost to a slow frame. */
        if (!g.released) {
            g.release_due = -1;
        }
        g.active = 0;
        if (g.pic) {
            pic_free(g.pic);
            g.pic = 0;
        }
        return;
    }
    if (at != g.shown) {
        show_frame(at);
    }
}

int castfx_release(void)
{
    if (!g.heap || g.released) {
        return 0;
    }
    if (g.release_due < 0 || g.shown >= g.release_due) {
        g.released = 1;
        return 1;
    }
    return 0;
}

int castfx_active(void)
{
    return g.active;
}

int castfx_shown(int *pic, int *left, int *top, int *w, int *h)
{
    if (!g.active || !g.pic) {
        return 0;
    }
    *pic = g.pic;
    *left = g.left;
    *top = g.top;
    *w = g.w;
    *h = g.h;
    return 1;
}

int castfx_frames(void)
{
    return g.count;
}

int castfx_frame_index(void)
{
    return g.shown;
}
