#include "game/dagger/play/canvas.h"
#include "game/dagger/formats/img.h"

/* See canvas.h. */

/* Scratch pictures are let go when they pass this, and read again when next wanted. */
#define SCRATCH_MAX (512u * 1024u)
#define WORD_MAX 40
#define LINE_MAX 96

static Composer *g_on;

int composer_open(Composer *c)
{
    MemHeap *was;
    int ok;

    mem_zero(c, sizeof(*c));
    c->heap = mem_heap_new();
    c->scratch = mem_heap_new();
    if (!c->heap || !c->scratch) {
        composer_close(c);
        return 0;
    }
    was = mem_heap_use(c->heap);
    c->screen = (df_u8 *)mem_alloc(CANVAS_BYTES);
    ok = c->screen && smallfont_load(&c->font, "FONT0003.FNT");
    mem_heap_use(was);
    if (!ok) {
        composer_close(c);
        return 0;
    }
    return 1;
}

void composer_close(Composer *c)
{
    if (c->scratch) {
        mem_heap_delete(c->scratch);
    }
    if (c->heap) {
        mem_heap_delete(c->heap);
    }
    if (g_on == c) {
        g_on = 0;
    }
    mem_zero(c, sizeof(*c));
}

const df_u8 *composer_read(Composer *c, const char *name)
{
    MemHeap *was;
    ImgPicture pic;
    int ok;

    was = mem_heap_use(c->heap);
    ok = img_load(name, &pic);
    mem_heap_use(was);
    return ok ? pic.pixels : 0;
}

MemHeap *composer_scratch_begin(Composer *c)
{
    if (mem_heap_bytes(c->scratch) > SCRATCH_MAX) {
        /* Whatever texture.c cached in it has to go with it: the caller's doll_forget_files first. */
        mem_heap_delete(c->scratch);
        c->scratch = mem_heap_new();
    }
    return mem_heap_use(c->scratch);
}

void composer_scratch_end(MemHeap *was)
{
    mem_heap_use(was);
}

void canvas_use(Composer *c)
{
    g_on = c;
}

int rect_has(const Rect *r, int x, int y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

void canvas_copy_all(const df_u8 *from)
{
    mem_copy(g_on->screen, from, CANVAS_BYTES);
}

void canvas_copy_rect(const df_u8 *from, const Rect *r)
{
    int row;

    for (row = 0; row < r->h; row++) {
        df_u32 at;

        at = (df_u32)(r->y + row) * CANVAS_W + r->x;
        mem_copy(g_on->screen + at, from + at, r->w);
    }
}

void canvas_copy_part(const df_u8 *from, int w, int sx, int sy, int sw, int sh, int x, int y)
{
    int row;

    for (row = 0; row < sh; row++) {
        mem_copy(g_on->screen + (y + row) * CANVAS_W + x, from + (sy + row) * w + sx, (df_u32)sw);
    }
}

void canvas_cutout(const df_u8 *from, int w, int h, int x, int y)
{
    int row;
    int col;

    for (row = 0; row < h; row++) {
        if (y + row < 0 || y + row >= CANVAS_H) {
            continue;
        }
        for (col = 0; col < w; col++) {
            int index;

            index = from[row * w + col];
            if (index && x + col >= 0 && x + col < CANVAS_W) {
                g_on->screen[(y + row) * CANVAS_W + x + col] = (df_u8)index;
            }
        }
    }
}

void canvas_box(const Rect *r, int index)
{
    int row;
    int col;

    for (row = 0; row < r->h; row++) {
        for (col = 0; col < r->w; col++) {
            g_on->screen[(r->y + row) * CANVAS_W + r->x + col] = (df_u8)index;
        }
    }
}

void canvas_label(int x, int y, const char *text)
{
    smallfont_draw(&g_on->font, g_on->screen, CANVAS_W, CANVAS_H, x, y, CANVAS_INK, CANVAS_INK_SHADOW, text);
}

void canvas_label_centred(const Rect *r, int down, const char *text)
{
    canvas_label(r->x + (r->w - canvas_width(text)) / 2, r->y + down, text);
}

int canvas_width(const char *text)
{
    return smallfont_width(&g_on->font, text);
}

int canvas_text(const Rect *r, const char *text)
{
    char line[LINE_MAX];
    int lines;
    int at;

    lines = 0;
    at = 0;
    line[0] = 0;
    while ((lines + 1) * CANVAS_LINE <= r->h) {
        char word[WORD_MAX];
        char joined[LINE_MAX + WORD_MAX];
        int n;
        int i;

        n = 0;
        while (*text && *text != ' ' && *text != '\n' && n < WORD_MAX - 1) {
            word[n++] = *text++;
        }
        word[n] = 0;
        /* The word on this line, if there is room for it. */
        i = 0;
        while (i < at) {
            joined[i] = line[i];
            i++;
        }
        if (at && n) {
            joined[i++] = ' ';
        }
        for (n = 0; word[n] && i < LINE_MAX - 1; n++) {
            joined[i++] = word[n];
        }
        joined[i] = 0;
        if (at && canvas_width(joined) > r->w) {
            canvas_label(r->x, r->y + lines * CANVAS_LINE, line);
            lines++;
            for (at = 0; word[at]; at++) {
                line[at] = word[at];
            }
            line[at] = 0;
        } else {
            for (at = 0; joined[at] && at < LINE_MAX - 1; at++) {
                line[at] = joined[at];
            }
            line[at] = 0;
        }
        if (*text == '\n' || !*text) {
            if ((lines + 1) * CANVAS_LINE <= r->h && at) {
                canvas_label(r->x, r->y + lines * CANVAS_LINE, line);
                lines++;
            }
            at = 0;
            line[0] = 0;
            if (!*text) {
                break;
            }
        }
        text++;
    }
    return lines;
}
