#include "game/dagger/play/ui.h"
#include "game/dagger/formats/smallfont.h"
#include "engine/core/mem.h"

/* See ui.h and docs/game.md (Widgets). */

/* A handful of swatches, not a full-screen texture: 96 KB of source texels
 * serves every screen. Frames, text and the moving thumb stay independent, so
 * an idle menu or a drag performs no file reads, allocations or PCI uploads. */
static MemHeap *g_heap;
static df_u8 *g_atlas[UI_ATLASES];
static df_u8 g_rgb[768];
static df_u8 g_width[2][256];
static int g_height[2];
static UiTranslate g_translate;
static UiQuad g_quad;
static void *g_ctx;
static const int g_side[] = {128, 256, 128};
static int g_regions;

const char *ui_tr(const char *s)
{
    const char *translated;
    translated = g_translate ? g_translate(s) : 0;
    return translated ? translated : s;
}

void ui_translate(UiTranslate translate) { g_translate = translate; }

void ui_begin(UiQuad quad, void *ctx)
{
    g_quad = quad;
    g_ctx = ctx;
    g_regions = 0;
}

int ui_open(void)
{
    MemHeap *was;
    SmallFont font;
    int i, f, x, y;
    df_u32 seed;

    if (g_heap) return 1;
    g_heap = mem_heap_new();
    if (!g_heap) return 0;
    was = mem_heap_use(g_heap);
    for (i = 0; i < UI_ATLASES; i++) {
        g_atlas[i] = (df_u8 *)mem_alloc((df_u32)g_side[i] * g_side[i]);
        if (!g_atlas[i]) goto failed;
        mem_zero(g_atlas[i], (df_u32)g_side[i] * g_side[i]);
    }
    /* Grey ramp plus muted rust and moss ramps. The original-font glyphs
     * receive a gold bevel; none of the panel textures contain text. */
    for (i = 0; i < 256; i++) {
        g_rgb[i * 3] = g_rgb[i * 3 + 1] = g_rgb[i * 3 + 2] = (df_u8)i;
    }
    for (i = 0; i < 32; i++) {
        g_rgb[(160 + i) * 3] = (df_u8)(68 + i * 3);
        g_rgb[(160 + i) * 3 + 1] = (df_u8)(18 + i);
        g_rgb[(160 + i) * 3 + 2] = 8;
        g_rgb[(192 + i) * 3] = (df_u8)(28 + i);
        g_rgb[(192 + i) * 3 + 1] = (df_u8)(45 + i * 2);
        g_rgb[(192 + i) * 3 + 2] = 10;
    }
    g_rgb[240*3] = 250; g_rgb[240*3+1] = 224; g_rgb[240*3+2] = 38;
    g_rgb[241*3] = 255; g_rgb[241*3+1] = 250; g_rgb[241*3+2] = 113;
    g_rgb[242*3] = 135; g_rgb[242*3+1] = 107; g_rgb[242*3+2] = 8;
    /* Deterministic fine grain with a coarser mottling, generated once.
     * Three swatches occupy 64-square cells; final row holds flat colours. */
    seed = 0x67574a21u;
    for (y = 0; y < 128; y++) for (x = 0; x < 128; x++) {
        int grain;
        seed = seed * 1664525u + 1013904223u;
        grain = (int)(seed >> 29) + ((x / 5 + y / 3) % 3) * 2;
        g_atlas[0][y * 128 + x] = (df_u8)(y < 64 ? 49 + grain : x < 64 ? 164 + grain : 196 + grain);
    }
    for (x = 0; x < 64; x++) g_atlas[0][127*128+x] = (df_u8)(x == 0 ? 18 : x == 1 ? 104 : x == 2 ? 143 : x == 3 ? 32 : x == 4 ? 240 : 180);
    for (f = 0; f < 2; f++) {
        int cell, side, scale;
        /* FONT0003 is classic's compact block face. Enlarge its strokes
         * for native-resolution labels, then bevel at output pixels; this
         * keeps their edges sharp and their shadows one pixel wide. */
        if (!smallfont_load(&font, "FONT0003.FNT")) goto failed;
        scale = f ? 1 : 2;
        cell = f ? 8 : 16;
        side = g_side[f + 1];
        if (font.height * scale + 1 > cell) goto failed;
        g_height[f] = font.height * scale + 1;
        for (i = 33; i < 256; i++) {
            char word[2];
            df_u8 glyph[64];
            int width, sx, sy;
            word[0] = (char)i; word[1] = 0;
            width = smallfont_width(&font, word)-1;
            if (width < 0) width = 0;
            if (width > 7) width = 7;
            g_width[f][i] = (df_u8)(width * scale + 1);
            sx = (i % 16) * cell; sy = (i / 16) * cell;
            mem_zero(glyph, sizeof(glyph));
            smallfont_draw(&font, glyph, 8, 8, 0, 0, 240, -1, word);
            for (y = 0; y < font.height*scale; y++) for (x = 0; x < width*scale; x++) {
                int at;
                if (!glyph[(y/scale)*8+x/scale]) continue;
                at = (sy+y)*side+sx+x;
                g_atlas[f+1][at+side+1] = 18;
            }
            for (y = 0; y < font.height*scale; y++) for (x = 0; x < width*scale; x++) {
                int at;
                if (!glyph[(y/scale)*8+x/scale]) continue;
                at = (sy+y)*side+sx+x;
                g_atlas[f+1][at] = (df_u8)(!y || !glyph[((y-1)/scale)*8+x/scale] ? 241 :
                    y == font.height*scale-1 || !glyph[((y+1)/scale)*8+x/scale] ? 242 : 240);
            }
        }
        g_width[f][' '] = (df_u8)(3*scale+1);
    }
    mem_heap_use(was);
    return 1;
failed:
    mem_heap_use(was);
    ui_close();
    return 0;
}

void ui_close(void)
{
    int i;
    if (g_heap) mem_heap_delete(g_heap);
    g_heap = 0;
    for (i = 0; i < UI_ATLASES; i++) g_atlas[i] = 0;
}
const df_u8 *ui_atlas(int i, int *side)
{
    if (i < 0 || i >= UI_ATLASES) return 0;
    *side = g_side[i];
    return g_atlas[i];
}
const df_u8 *ui_palette(void) { return g_rgb; }

void ui_flat(int x, int y, int w, int h, int color)
{
    if (g_quad && w > 0 && h > 0) g_quad(g_ctx, 0, color, 127, 1, 1, x, y, w, h);
}
void ui_panel(int x, int y, int w, int h, int style)
{
    /* A bounded swatch grid and six thin bevels. Small controls need one
     * patch; only long/tall panels subdivide, at most four per axis. */
    if (!g_quad) {
        return;
    }
    {
        int row, col, tile_w, tile_h;
        /* At most sixteen pieces per panel. Small grain stays roughly
         * square instead of stretching into streaks down a tall sidebar. */
        tile_w = w > 128 ? (w + 3) / 4 : w;
        tile_h = h > 128 ? (h + 3) / 4 : h;
        for (row = 0; row < h; row += tile_h) for (col = 0; col < w; col += tile_w)
            g_quad(g_ctx, 0, style == 2 ? 64 : 0, style ? 64 : 0, 64, 63, x+col, y+row,
                   col+tile_w > w ? w-col : tile_w, row+tile_h > h ? h-row : tile_h);
    }
    ui_flat(x, y, w, 1, 2); ui_flat(x, y, 1, h, 2);
    ui_flat(x+1, y+1, w-2, 1, 1); ui_flat(x+1, y+1, 1, h-2, 1);
    ui_flat(x, y+h-1, w, 1, 0); ui_flat(x+w-1, y, 1, h, 0);
}
int ui_width(int font, const char *s)
{
    int n;
    n = 0;
    while (*s) n += g_width[font][(unsigned char)*s++];
    return n;
}
void ui_text(int x, int y, int max_w, const char *s, int font)
{
    int right, cell;
    right = x + max_w;
    cell = font ? 8 : 16;
    while (*s && x < right) {
        unsigned c;
        int w;
        c = (unsigned char)*s++;
        w = g_width[font][c];
        if (g_quad && c != ' ' && w) {
            int draw_w;
            draw_w = w < right-x ? w : right-x;
            g_quad(g_ctx, font+1, (c%16)*cell, (c/16)*cell, draw_w, g_height[font], x, y, draw_w, g_height[font]);
        }
        x += w;
    }
}
void ui_centered(int x, int y, int w, const char *s)
{
    int font, n;
    s = ui_tr(s);
    font = ui_width(0, s) <= w-12 ? 0 : 1;
    n = ui_width(font, s);
    ui_text(x + (n < w ? (w-n)/2 : 6), y, w-12, s, font);
}
void ui_button(int x, int y, int w, int h, const char *s, int style, int hot)
{
    ui_panel(x, y, w, h, style);
    if (hot) {
        ui_flat(x+2, y+2, w-4, 1, 4); ui_flat(x+2, y+h-3, w-4, 1, 4);
    }
    ui_centered(x, y+(h-15)/2, w, s);
}
void ui_paragraph(int x, int y, int w, int h, const char *s)
{
    char line[128];
    int used, pixels, last_space, i;
    s = ui_tr(s);
    /* Bounded wrapping at spaces, with a hard split for an unusually long
     * translated word. There is never an off-panel text draw. */
    while (*s && h >= 15) {
        used = pixels = 0; last_space = -1;
        while (s[used] && s[used] != '\n' && used < 126) {
            int next;
            next = g_width[0][(unsigned char)s[used]];
            if (pixels+next > w) break;
            if (s[used] == ' ') last_space = used;
            pixels += next; used++;
        }
        if (!used) break;
        if (s[used] && s[used] != '\n' && last_space > 0) used = last_space;
        for (i = 0; i < used; i++) line[i] = s[i];
        line[used] = 0;
        ui_text(x, y, w, line, 0);
        s += used;
        while (*s == ' ' || *s == '\n') s++;
        y += 16; h -= 16;
    }
}


/* The regions the last drawing made: a control's id and where it answers. */
#define UI_REGIONS 256
typedef struct Region {
    short id;
    short x;
    short y;
    short w;
    short h;
} Region;
static Region g_region[UI_REGIONS];

void ui_region(int id, int x, int y, int w, int h)
{
    if (g_regions < UI_REGIONS) {
        g_region[g_regions].id = (short)id;
        g_region[g_regions].x = (short)x;
        g_region[g_regions].y = (short)y;
        g_region[g_regions].w = (short)w;
        g_region[g_regions].h = (short)h;
        g_regions++;
    }
}

int ui_hit(int x, int y)
{
    int i;

    /* The last made is on top. */
    for (i = g_regions - 1; i >= 0; i--) {
        const Region *r;

        r = &g_region[i];
        if (x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h) {
            return r->id;
        }
    }
    return UI_NOTHING;
}

/* ---- A list of rows ---- */

void ui_list_move(UiList *list, int delta)
{
    int most;

    most = list->count - list->rows;
    list->scroll += delta;
    if (list->scroll > most) {
        list->scroll = most;
    }
    if (list->scroll < 0) {
        list->scroll = 0;
    }
}

void ui_list_show(UiList *list, int row)
{
    if (row < list->scroll) {
        list->scroll = row;
    } else if (row >= list->scroll + list->rows) {
        list->scroll = row - list->rows + 1;
    }
    ui_list_move(list, 0);
}

int ui_list_row_at(const UiList *list, int x, int y)
{
    int row;

    if (x < list->x || x >= list->x + list->w || y < list->y || y >= list->y + list->rows * list->row_h) {
        return -1;
    }
    row = list->scroll + (y - list->y) / list->row_h;
    return row < list->count ? row : -1;
}

void ui_list_bar(const UiList *list, int up_id, int down_id, int up_hot, int down_hot)
{
    int x;
    int h;
    int thumb;

    x = list->x + list->w + 4;
    h = list->rows * list->row_h;
    ui_panel(x, list->y, UI_BAR_W, h, 0);
    ui_button(x, list->y, UI_BAR_W, 24, "^", 0, up_hot);
    ui_button(x, list->y + h - 24, UI_BAR_W, 24, "v", 0, down_hot);
    ui_region(up_id, x, list->y, UI_BAR_W, 24);
    ui_region(down_id, x, list->y + h - 24, UI_BAR_W, 24);
    if (list->count > list->rows && h > 28 + 24 + 4) {
        /* The thumb travels the room between the arrows. */
        int room;
        int size;

        room = h - 2 * 28;
        size = room * list->rows / list->count;
        size = size < 12 ? 12 : size;
        thumb = (room - size) * list->scroll / (list->count - list->rows);
        ui_panel(x + 3, list->y + 28 + thumb, UI_BAR_W - 6, size, 0);
    }
}
