#include "game/dagger/formats/smallfont.h"
#include "engine/assets/vfs.h"

/* See smallfont.h. */

#define FILE_HEADER 4
#define RECORD_BYTES 4
#define GLYPHS 240
#define FIRST_CHAR 33
/* A space has no glyph: this many pixels, and one between any two letters. */
#define SPACE_WIDTH 3
#define LETTER_GAP 1

int smallfont_load(SmallFont *font, const char *name)
{
    df_u32 size;
    const df_u8 *bytes;

    font->bytes = 0;
    bytes = vfs_read(name, &size);
    if (!bytes || size < FILE_HEADER + GLYPHS * RECORD_BYTES) {
        return 0;
    }
    font->bytes = bytes;
    font->size = size;
    font->height = df_ru16(bytes + 2);
    return 1;
}

/* A character's glyph: its bytes (0 when it has none) and its width. */
static const df_u8 *glyph_of(const SmallFont *font, unsigned char c, int *width)
{
    const df_u8 *record;
    df_u32 offset;
    int index;

    *width = 0;
    index = (int)c - FIRST_CHAR;
    if (index < 0 || index >= GLYPHS) {
        return 0;
    }
    record = font->bytes + FILE_HEADER + (df_u32)index * RECORD_BYTES;
    offset = df_ru16(record);
    *width = df_ru16(record + 2);
    if (*width > 16 || offset + (df_u32)font->height * 2u > font->size) {
        *width = 0;
        return 0;
    }
    return font->bytes + offset;
}

int smallfont_width(const SmallFont *font, const char *text)
{
    int total;

    total = 0;
    if (!font->bytes) {
        return 0;
    }
    for (; *text; text++) {
        int width;

        if (*text == ' ') {
            total += SPACE_WIDTH + LETTER_GAP;
            continue;
        }
        glyph_of(font, (unsigned char)*text, &width);
        total += width + LETTER_GAP;
    }
    return total;
}

/* One glyph's set pixels in one colour. */
static void draw_glyph(const df_u8 *glyph, int width, int height, df_u8 *pixels, int w, int h, int x, int y, int colour)
{
    int row;
    int col;

    for (row = 0; row < height; row++) {
        unsigned bits;

        if (y + row < 0 || y + row >= h) {
            continue;
        }
        bits = df_ru16(glyph + row * 2);
        for (col = 0; col < width; col++) {
            if ((bits & (0x8000u >> col)) && x + col >= 0 && x + col < w) {
                pixels[(y + row) * w + x + col] = (df_u8)colour;
            }
        }
    }
}

void smallfont_draw(const SmallFont *font, df_u8 *pixels, int w, int h, int x, int y, int ink, int shadow,
                    const char *text)
{
    if (!font->bytes) {
        return;
    }
    for (; *text; text++) {
        const df_u8 *glyph;
        int width;

        if (*text == ' ') {
            x += SPACE_WIDTH + LETTER_GAP;
            continue;
        }
        glyph = glyph_of(font, (unsigned char)*text, &width);
        if (glyph) {
            if (shadow >= 0) {
                draw_glyph(glyph, width, font->height, pixels, w, h, x + 1, y + 1, shadow);
            }
            draw_glyph(glyph, width, font->height, pixels, w, h, x, y, ink);
        }
        x += width + LETTER_GAP;
    }
}
