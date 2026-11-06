#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/formats/img.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See spellbook.h. */

/* Where things are on SPBK00I0.IMG, relative to the picture's own corner (DFU's rectangles). */
#define LIST_X 5
#define LIST_Y 13
#define LIST_W 110
#define DETAIL_X 138
#define EFFECT_Y0 40
#define EFFECT_STEP 38
#define EFFECT_W 118
#define ICON_X 149
#define ICON_Y 14
#define TARGET_X 182
#define ELEMENT_X 223
#define NAME_Y 4
#define COST_Y 31
/* Buttons: delete, up, sort, down under the list; the arrows at the list's side; exit. */
#define BTN_Y 152
#define BTN_W 38
#define BTN_H 9
#define ARROW_X 121
#define UP_ARROW_Y 11
#define DOWN_ARROW_Y 132
#define ARROW_W 9
#define ARROW_H 16
#define EXIT_X 216
#define EXIT_Y 149
#define EXIT_W 43
#define EXIT_H 15
/* ICON00I0.IMG: 69 icons of 16 x 16, 20 to a row, a headerless 320 wide. MASK04I0.IMG: targets (24 wide) then elements (16), 5 each. */
#define ICON_SIZE 16
#define ICONS_PER_ROW 20
#define ICON_FILE_W 320
#define MASK_FILE_W 40
#define TARGET_W 24

static const int g_button_x[4] = { 3, 48, 90, 132 };

static struct {
    int open;
    Composer composer;
    Spellbook *book;
    const Character *pc;
    /* The picture's corner on classic's screen, the picture, and the two sheets of icons. */
    int ox;
    int oy;
    ImgPicture back;
    const df_u8 *icons;
    const df_u8 *masks;
    int selected;
    int scroll;
    int confirm;
    int chosen;
    int changed;
} g;

int spellbook_open(Spellbook *book, const Character *pc)
{
    df_u32 size;
    MemHeap *was;
    int ok;

    spellbook_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.book = book;
    g.pc = pc;
    was = mem_heap_use(g.composer.heap);
    ok = img_load("SPBK00I0.IMG", &g.back);
    mem_heap_use(was);
    g.icons = composer_table(&g.composer, "ICON00I0.IMG", &size);
    if (!g.icons || size < (df_u32)(ICON_FILE_W * 64)) {
        g.icons = 0;
    }
    g.masks = composer_table(&g.composer, "MASK04I0.IMG", &size);
    if (!g.masks || size < (df_u32)(MASK_FILE_W * 80)) {
        g.masks = 0;
    }
    if (!ok) {
        spellbook_close();
        return 0;
    }
    g.ox = g.back.x;
    g.oy = g.back.y;
    g.selected = book->count ? 0 : -1;
    g.chosen = -1;
    g.changed = 1;
    return 1;
}

void spellbook_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

const unsigned char *spellbook_pixels(void)
{
    return g.composer.screen;
}

int spellbook_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

int spellbook_chosen(void)
{
    return g.chosen;
}

int spellbook_selected(void)
{
    return g.selected;
}

int spellbook_scroll(void)
{
    return g.scroll;
}

int spellbook_confirming(void)
{
    return g.confirm;
}

/* ---- Drawing. ---- */

/* A parameter in words: "1 - 20 + 1 - 4 each level", as a spell maker's spinners set it. */
static void range_text(char *out, int cap, const char *what, int low, int high, int plus_low, int plus_high, int per)
{
    if (plus_high > 0 || plus_low > 0) {
        log_format(out, cap, "%s %d-%d +%d-%d/%d lvl", what, low, high, plus_low, plus_high, per);
    } else {
        log_format(out, cap, "%s %d-%d", what, low, high);
    }
}

static void effect_lines(const EffectSettings *e, int x, int y)
{
    const MagicEffect *fx;
    char name[48];
    char line[64];

    spell_effect_name(e->effect, name, (int)sizeof(name));
    canvas_label(x, y, name);
    fx = magic_effect(e->effect);
    y += CANVAS_LINE;
    if (fx && (fx->supports & MAGIC_DURATION)) {
        log_format(line, (int)sizeof(line), "Duration %d + %d/%d lvl", e->duration_base, e->duration_plus, e->duration_per);
        canvas_label(x, y, line);
        y += CANVAS_LINE;
    }
    if (fx && (fx->supports & MAGIC_CHANCE)) {
        log_format(line, (int)sizeof(line), "Chance %d + %d/%d lvl", e->chance_base, e->chance_plus, e->chance_per);
        canvas_label(x, y, line);
        y += CANVAS_LINE;
    }
    if (fx && (fx->supports & MAGIC_MAGNITUDE)) {
        range_text(line, (int)sizeof(line), "Power", e->magnitude_base_low, e->magnitude_base_high, e->magnitude_plus_low,
                   e->magnitude_plus_high, e->magnitude_per);
        canvas_label(x, y, line);
    }
}

void spellbook_compose(void)
{
    static const Rect whole = { 0, 0, CANVAS_W, CANVAS_H };
    int i;

    canvas_use(&g.composer);
    canvas_box(&whole, 0);
    canvas_copy_part(g.back.pixels, g.back.width, 0, 0, g.back.width, g.back.height, g.ox, g.oy);
    /* The list: a row each, the chosen one in a dark box. */
    for (i = 0; i < SPELLBOOK_ROWS; i++) {
        int at;
        Rect row;

        at = g.scroll + i;
        if (at >= g.book->count) {
            break;
        }
        row.x = (df_u16)(g.ox + LIST_X);
        row.y = (df_u16)(g.oy + LIST_Y + i * SPELLBOOK_ROW_H);
        row.w = LIST_W;
        row.h = SPELLBOOK_ROW_H;
        if (at == g.selected) {
            canvas_box(&row, CANVAS_DARK);
        }
        canvas_label(row.x + 2, row.y + 1, g.book->spell[at].name);
    }
    if (g.confirm) {
        canvas_label(g.ox + LIST_X, g.oy + BTN_Y - CANVAS_LINE - 2, "Click Delete again");
    }
    /* The chosen spell: name, icon, target, element, effects, cost. */
    if (g.selected >= 0 && g.selected < g.book->count) {
        const Spell *s;
        int k;
        int y;
        char cost[40];

        s = &g.book->spell[g.selected];
        canvas_label(g.ox + DETAIL_X, g.oy + NAME_Y, s->name);
        if (g.icons && s->icon < 69) {
            canvas_copy_part(g.icons, ICON_FILE_W, (s->icon % ICONS_PER_ROW) * ICON_SIZE, (s->icon / ICONS_PER_ROW) * ICON_SIZE,
                             ICON_SIZE, ICON_SIZE, g.ox + ICON_X, g.oy + ICON_Y);
        }
        if (g.masks && s->target < TARGET_COUNT) {
            canvas_copy_part(g.masks, MASK_FILE_W, 0, s->target * ICON_SIZE, TARGET_W, ICON_SIZE, g.ox + TARGET_X, g.oy + ICON_Y);
        }
        if (g.masks && s->element < ELEMENT_COUNT) {
            canvas_copy_part(g.masks, MASK_FILE_W, TARGET_W, s->element * ICON_SIZE, ICON_SIZE, ICON_SIZE, g.ox + ELEMENT_X,
                             g.oy + ICON_Y);
        }
        y = g.oy + EFFECT_Y0;
        for (k = 0; k < MAGIC_SLOTS; k++) {
            if (s->effect[k].effect != SPELL_NO_EFFECT) {
                effect_lines(&s->effect[k], g.ox + DETAIL_X, y);
            }
            y += EFFECT_STEP;
        }
        log_format(cost, (int)sizeof(cost), "Cost: %d spell points", spell_cost(s, g.pc, 0));
        canvas_label(g.ox + DETAIL_X, g.oy + COST_Y, cost);
    }
}

/* ---- Doing. ---- */

static void show_selected(void)
{
    if (g.selected < g.scroll) {
        g.scroll = g.selected;
    }
    if (g.selected >= g.scroll + SPELLBOOK_ROWS) {
        g.scroll = g.selected - SPELLBOOK_ROWS + 1;
    }
}

static void scroll_by(int rows)
{
    int most;

    most = g.book->count - SPELLBOOK_ROWS;
    g.scroll += rows;
    g.scroll = g.scroll > most ? most : g.scroll;
    g.scroll = g.scroll < 0 ? 0 : g.scroll;
    g.changed = 1;
}

static void select(int at)
{
    if (at < 0 || at >= g.book->count) {
        return;
    }
    g.selected = at;
    g.confirm = 0;
    show_selected();
    g.changed = 1;
}

/* The chosen spell one place up or down the book. */
static void move_selected(int by)
{
    int to;

    to = g.selected + by;
    if (g.selected < 0 || to < 0 || to >= g.book->count) {
        return;
    }
    {
        Spell swap;

        swap = g.book->spell[to];
        g.book->spell[to] = g.book->spell[g.selected];
        g.book->spell[g.selected] = swap;
    }
    select(to);
}

static void delete_selected(void)
{
    if (g.selected < 0 || g.selected >= g.book->count) {
        return;
    }
    if (!g.confirm) {
        g.confirm = 1;
        g.changed = 1;
        return;
    }
    spellbook_remove(g.book, g.selected);
    g.confirm = 0;
    g.selected = g.selected >= g.book->count ? g.book->count - 1 : g.selected;
    scroll_by(0);
    show_selected();
    g.changed = 1;
}

static void sort_book(void)
{
    char keep[SPELL_NAME];

    if (g.selected >= 0) {
        df_copy(keep, g.book->spell[g.selected].name, SPELL_NAME);
    }
    spellbook_sort(g.book);
    if (g.selected >= 0) {
        select(spellbook_find(g.book, keep));
    }
    g.changed = 1;
}

static int in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= g.ox + rx && x < g.ox + rx + rw && y >= g.oy + ry && y < g.oy + ry + rh;
}

int spellbook_click(int x, int y)
{
    int i;

    if (in_rect(x, y, EXIT_X, EXIT_Y, EXIT_W, EXIT_H)) {
        return SPELLBOOK_LEAVE;
    }
    for (i = 0; i < 4; i++) {
        if (in_rect(x, y, g_button_x[i], BTN_Y, BTN_W, BTN_H)) {
            if (i == 0) {
                delete_selected();
            } else if (i == 1) {
                move_selected(-1);
            } else if (i == 2) {
                sort_book();
            } else {
                move_selected(1);
            }
            return SPELLBOOK_STAY;
        }
    }
    if (in_rect(x, y, ARROW_X, UP_ARROW_Y, ARROW_W, ARROW_H)) {
        scroll_by(-1);
    } else if (in_rect(x, y, ARROW_X, DOWN_ARROW_Y, ARROW_W, ARROW_H)) {
        scroll_by(1);
    } else if (in_rect(x, y, LIST_X, LIST_Y, LIST_W, SPELLBOOK_ROWS * SPELLBOOK_ROW_H)) {
        int at;

        at = g.scroll + (y - g.oy - LIST_Y) / SPELLBOOK_ROW_H;
        if (at >= 0 && at < g.book->count) {
            /* A second click on the chosen spell readies it. */
            if (at == g.selected) {
                g.chosen = at;
                return SPELLBOOK_READY;
            }
            select(at);
        }
    }
    return SPELLBOOK_STAY;
}

void spellbook_wheel(int notches)
{
    scroll_by(-notches);
}

int spellbook_key(int vk)
{
    if (vk == 0x1B) {
        return SPELLBOOK_LEAVE;
    }
    if (vk == 0x0D && g.selected >= 0) {
        g.chosen = g.selected;
        return SPELLBOOK_READY;
    }
    if (vk == 0x26) {
        select(g.selected - 1);
    } else if (vk == 0x28) {
        select(g.selected + 1);
    } else if (vk == 0x21) {
        scroll_by(-SPELLBOOK_ROWS);
    } else if (vk == 0x22) {
        scroll_by(SPELLBOOK_ROWS);
    } else if (vk == 0x2E) {
        delete_selected();
    }
    return SPELLBOOK_STAY;
}
