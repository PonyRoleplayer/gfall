#include "game/dagger/play/canvas.h"
#include "game/dagger/formats/img.h"
#include "engine/assets/vfs.h"

/* See canvas.h. */

/* Scratch pictures are let go when they pass this, and read again when next wanted. */
#define SCRATCH_MAX (512u * 1024u)
#define WORD_MAX 40
#define LINE_MAX 96
#define PIECE 22
#define BUTTON_BYTES (CANVAS_BUTTON_W * CANVAS_BUTTON_H)

static Composer *g_on;
static int g_ink = CANVAS_INK;
static int g_shadow = CANVAS_INK_SHADOW;

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
    if (ok) {
        df_u32 size;

        c->scroll = vfs_read("SPOP.RCI", &size);
        c->buttons = vfs_read("BUTTONS.RCI", &size);
    }
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

const df_u8 *composer_table(Composer *c, const char *name, df_u32 *size)
{
    MemHeap *was;
    const df_u8 *bytes;

    was = mem_heap_use(c->heap);
    bytes = vfs_read(name, size);
    mem_heap_use(was);
    return bytes;
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
    g_ink = CANVAS_INK;
    g_shadow = CANVAS_INK_SHADOW;
}

void canvas_set_ink(int ink, int shadow)
{
    g_ink = ink;
    g_shadow = shadow;
}

void canvas_scroll(const Rect *r)
{
    int across;
    int down;
    int row;
    int col;

    if (!g_on->scroll) {
        canvas_box(r, CANVAS_DARK);
        return;
    }
    across = (r->w + PIECE - 1) / PIECE;
    down = (r->h + PIECE - 1) / PIECE;
    for (row = 0; row < down; row++) {
        int piece_row;

        piece_row = row == 0 ? 0 : (row == down - 1 ? 2 : 1);
        for (col = 0; col < across; col++) {
            int piece_col;

            piece_col = col == 0 ? 0 : (col == across - 1 ? 2 : 1);
            canvas_cutout(g_on->scroll + (piece_row * 3 + piece_col) * PIECE * PIECE, PIECE, PIECE, r->x + col * PIECE, r->y + row * PIECE);
        }
    }
}

void canvas_button(int which, int x, int y)
{
    if (g_on->buttons) {
        canvas_copy_part(g_on->buttons + which * BUTTON_BYTES, CANVAS_BUTTON_W, 0, 0, CANVAS_BUTTON_W, CANVAS_BUTTON_H, x, y);
    }
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
    smallfont_draw(&g_on->font, g_on->screen, CANVAS_W, CANVAS_H, x, y, g_ink, g_shadow, text);
}

void canvas_label_centred(const Rect *r, int down, const char *text)
{
    canvas_label(r->x + (r->w - canvas_width(text)) / 2, r->y + down, text);
}

int canvas_width(const char *text)
{
    return smallfont_width(&g_on->font, text);
}

/*
 * Cuts text into lines of a width at spaces and its own line breaks: each
 * line to one_line, if given, with its number. As many as max_lines. How many.
 */
static int cut_lines(int width, const char *text, int max_lines, const Rect *draw_in)
{
    char line[LINE_MAX];
    int lines;
    int at;

    lines = 0;
    at = 0;
    line[0] = 0;
    while (lines < max_lines) {
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
        for (i = 0; i < at; i++) {
            joined[i] = line[i];
        }
        if (at && n) {
            joined[i++] = ' ';
        }
        for (n = 0; word[n] && i < LINE_MAX - 1; n++) {
            joined[i++] = word[n];
        }
        joined[i] = 0;
        if (at && canvas_width(joined) > width) {
            if (draw_in) {
                canvas_label(draw_in->x, draw_in->y + lines * CANVAS_LINE, line);
            }
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
            if (lines < max_lines && at) {
                if (draw_in) {
                    canvas_label(draw_in->x, draw_in->y + lines * CANVAS_LINE, line);
                }
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

int canvas_text(const Rect *r, const char *text)
{
    return cut_lines(r->w, text, r->h / CANVAS_LINE, r);
}

int canvas_text_lines(int width, const char *text)
{
    return cut_lines(width, text, CANVAS_H, 0);
}
