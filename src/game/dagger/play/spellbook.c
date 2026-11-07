#include "game/dagger/play/spellbook.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/play/rules.h"
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
/* DFU's DaggerfallSpellBookWindow: the name at 123, 2; the spell points at 214, 2; the price in buy mode at 76, 154. */
#define NAME_X 123
#define NAME_Y 2
#define POINTS_X 214
#define PRICE_X 76
#define PRICE_Y 154
/* An effect's box (118 x 28) holds two lines centred, its group at 5 and its kind at 17. */
#define EFFECT_H 28
#define EFFECT_LINE0 5
#define EFFECT_LINE1 17
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
    /* The spells the list shows: the player's own, or a guild's standard ones for sale (buy mode, SPBK01I0.IMG). */
    Spellbook *list;
    /* In buy mode: the player's book the spell is bought into, the player, the shop's quality, and a note. */
    Spellbook *book;
    Player *buyer;
    int quality;
    int buy;
    const char *note;
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
    char noted[128];
} g;

/* The standard spells for sale, kept for the process: a Spellbook is large. */
static Spellbook g_stock;

/* The screen opened on the picture, whichever mode (g.list, g.pc and g.buy set by the caller after the common part). */
static int open_picture(const char *picture, const Character *pc)
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
    g.pc = pc;
    was = mem_heap_use(g.composer.heap);
    ok = img_load(picture, &g.back);
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
    g.chosen = -1;
    g.changed = 1;
    return 1;
}

int spellbook_open(Spellbook *book, const Character *pc)
{
    if (!open_picture("SPBK00I0.IMG", pc)) {
        return 0;
    }
    g.list = book;
    g.selected = book->count ? 0 : -1;
    return 1;
}

int spellbook_open_buy(Spellbook *book, Player *player, int quality)
{
    RecTable *t;
    int i;

    if (!open_picture("SPBK01I0.IMG", &player->sheet)) {
        return 0;
    }
    mem_zero(&g_stock, sizeof(g_stock));
    t = spell_table();
    for (i = 0; i < t->count; i++) {
        const MagicSpell *m;
        Spell s;

        m = (const MagicSpell *)(t->rows + (df_u32)i * sizeof(MagicSpell));
        if (m->name[0] != '!') {
            spell_standard(&s, m);
            spellbook_add(&g_stock, &s);
        }
    }
    spellbook_sort(&g_stock);
    g.buy = 1;
    g.list = &g_stock;
    g.book = book;
    g.buyer = player;
    g.quality = quality;
    g.selected = g_stock.count ? 0 : -1;
    return 1;
}

const char *spellbook_note(void)
{
    return g.note;
}

/* Four times the cost in spell points for this character, bargained (DFU's presented cost and CalculateTradePrice). */
/* What spell at in the shop's list costs in gold: four times its spell points, by the trade's sums. */
static int price_of(int at)
{
    int presented;

    presented = spell_cost(&g.list->spell[at], g.pc, 0) * 4;
    return rules_trade_price(presented, g.quality, g.pc->skill[SKILL_MERCANTILE], g.pc->attribute[ATTR_PERSONALITY], 0);
}

int spellbook_price(void)
{
    if (!g.buy || g.selected < 0 || g.selected >= g.list->count) {
        return 0;
    }
    return price_of(g.selected);
}

/* The chosen spell bought: the first click says the price, the second pays. */
static void buy_selected(void)
{
    int price;
    char line[64];

    price = spellbook_price();
    if (g.selected < 0) {
        return;
    }
    if ((int)g.buyer->pack.gold < price) {
        g.note = "You do not have enough gold.";
        g.confirm = 0;
    } else if (!g.confirm) {
        log_format(line, (int)sizeof(line), "Click Buy again to pay %d gold.", price);
        df_copy(g.noted, line, (int)sizeof(g.noted));
        g.note = g.noted;
        g.confirm = 1;
    } else {
        g.confirm = 0;
        g.buyer->pack.gold -= (df_u32)price;
        /* Classic sells a spell the player has already; the book refuses the same name, so it stays one. */
        if (spellbook_add(g.book, &g.list->spell[g.selected])) {
            g.note = "The spell is inscribed in your spellbook.";
        } else {
            g.buyer->pack.gold += (df_u32)price;
            g.note = "That spell is in your book already.";
        }
    }
    g.changed = 1;
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

/* An effect's two lines, centred in its box: its group and its kind ("Damage", "Health"), as classic's book. */
static void effect_lines(const EffectSettings *e, int x, int y)
{
    const MagicEffect *fx;
    Rect box;

    box.x = (df_u16)x;
    box.y = (df_u16)y;
    box.w = EFFECT_W;
    box.h = EFFECT_H;
    fx = magic_effect(e->effect);
    if (!fx) {
        return;
    }
    canvas_label_centred(&box, EFFECT_LINE0, fx->group);
    if (fx->sub[0]) {
        canvas_label_centred(&box, EFFECT_LINE1, fx->sub);
    }
}

/* part on the end of out, cut to cap. */
static void append(char *out, const char *part, int cap)
{
    int n;

    for (n = 0; out[n]; n++) {
    }
    while (*part && n < cap - 1) {
        out[n++] = *part++;
    }
    out[n] = 0;
}

/* An effect's numbers, said when its box is clicked: duration, chance and power by the caster's level. */
static void effect_numbers(const EffectSettings *e, char *out, int cap)
{
    const MagicEffect *fx;
    char part[48];

    out[0] = 0;
    fx = magic_effect(e->effect);
    if (fx && (fx->supports & MAGIC_DURATION)) {
        log_format(part, (int)sizeof(part), "Duration %d + %d/%d lvl. ", e->duration_base, e->duration_plus, e->duration_per);
        append(out, part, cap);
    }
    if (fx && (fx->supports & MAGIC_CHANCE)) {
        log_format(part, (int)sizeof(part), "Chance %d + %d/%d lvl. ", e->chance_base, e->chance_plus, e->chance_per);
        append(out, part, cap);
    }
    if (fx && (fx->supports & MAGIC_MAGNITUDE)) {
        range_text(part, (int)sizeof(part), "Power", e->magnitude_base_low, e->magnitude_base_high, e->magnitude_plus_low,
                   e->magnitude_plus_high, e->magnitude_per);
        append(out, part, cap);
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
        if (at >= g.list->count) {
            break;
        }
        row.x = (df_u16)(g.ox + LIST_X);
        row.y = (df_u16)(g.oy + LIST_Y + i * SPELLBOOK_ROW_H);
        row.w = LIST_W;
        row.h = SPELLBOOK_ROW_H;
        if (at == g.selected) {
            canvas_box(&row, CANVAS_DARK);
        }
        /* Each spell after its cost, as DFU's list ("25 - Fireball"): spell points in the book, gold in a shop. */
        {
            char line[48];

            log_format(line, (int)sizeof(line), "%d - %s", g.buy ? price_of(at) : spell_cost(&g.list->spell[at], g.pc, 0),
                       g.list->spell[at].name);
            canvas_label(row.x + 2, row.y + 1, line);
        }
    }
    if (g.confirm && !g.buy) {
        canvas_label(g.ox + LIST_X, g.oy + BTN_Y - CANVAS_LINE - 2, "Click Delete again");
    }
    /* The chosen spell: name, icon, target, element, effects, cost. */
    if (g.selected >= 0 && g.selected < g.list->count) {
        const Spell *s;
        int k;
        int y;
        char cost[40];

        s = &g.list->spell[g.selected];
        canvas_label(g.ox + NAME_X, g.oy + NAME_Y, s->name);
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
        if (g.buy) {
            log_format(cost, (int)sizeof(cost), "%d", spellbook_price());
            canvas_label(g.ox + PRICE_X, g.oy + PRICE_Y, cost);
        }
    }
    /* The player's spell points, now and most (not in a shop). */
    if (!g.buy && g.pc) {
        char points[24];

        log_format(points, (int)sizeof(points), "%d/%d", g.pc->magicka, g.pc->magicka_max);
        canvas_label(g.ox + POINTS_X, g.oy + NAME_Y, points);
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

    most = g.list->count - SPELLBOOK_ROWS;
    g.scroll += rows;
    g.scroll = g.scroll > most ? most : g.scroll;
    g.scroll = g.scroll < 0 ? 0 : g.scroll;
    g.changed = 1;
}

static void select(int at)
{
    if (at < 0 || at >= g.list->count) {
        return;
    }
    g.selected = at;
    g.confirm = 0;
    g.note = 0;
    show_selected();
    g.changed = 1;
}

/* The chosen spell one place up or down the book. */
static void move_selected(int by)
{
    int to;

    to = g.selected + by;
    if (g.selected < 0 || to < 0 || to >= g.list->count) {
        return;
    }
    {
        Spell swap;

        swap = g.list->spell[to];
        g.list->spell[to] = g.list->spell[g.selected];
        g.list->spell[g.selected] = swap;
    }
    select(to);
}

static void delete_selected(void)
{
    if (g.selected < 0 || g.selected >= g.list->count) {
        return;
    }
    if (!g.confirm) {
        g.confirm = 1;
        g.changed = 1;
        return;
    }
    spellbook_remove(g.list, g.selected);
    g.confirm = 0;
    g.selected = g.selected >= g.list->count ? g.list->count - 1 : g.selected;
    scroll_by(0);
    show_selected();
    g.changed = 1;
}

static void sort_book(void)
{
    char keep[SPELL_NAME];

    if (g.selected >= 0) {
        df_copy(keep, g.list->spell[g.selected].name, SPELL_NAME);
    }
    spellbook_sort(g.list);
    if (g.selected >= 0) {
        select(spellbook_find(g.list, keep));
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
                if (g.buy) {
                    buy_selected();
                } else {
                    delete_selected();
                }
            } else if (i == 2) {
                sort_book();
            } else if (!g.buy) {
                move_selected(i == 1 ? -1 : 1);
            }
            return SPELLBOOK_STAY;
        }
    }
    if (in_rect(x, y, ARROW_X, UP_ARROW_Y, ARROW_W, ARROW_H)) {
        scroll_by(-1);
    } else if (in_rect(x, y, ARROW_X, DOWN_ARROW_Y, ARROW_W, ARROW_H)) {
        scroll_by(1);
    } else if (g.selected >= 0 && g.selected < g.list->count && x >= g.ox + DETAIL_X && x < g.ox + DETAIL_X + EFFECT_W
               && y >= g.oy + EFFECT_Y0 && (y - g.oy - EFFECT_Y0) % EFFECT_STEP < EFFECT_H
               && (y - g.oy - EFFECT_Y0) / EFFECT_STEP < MAGIC_SLOTS) {
        const EffectSettings *e;

        /* An effect's box: its numbers in the note. */
        e = &g.list->spell[g.selected].effect[(y - g.oy - EFFECT_Y0) / EFFECT_STEP];
        if (e->effect != SPELL_NO_EFFECT) {
            effect_numbers(e, g.noted, (int)sizeof(g.noted));
            g.note = g.noted;
            g.changed = 1;
        }
    } else if (in_rect(x, y, LIST_X, LIST_Y, LIST_W, SPELLBOOK_ROWS * SPELLBOOK_ROW_H)) {
        int at;

        at = g.scroll + (y - g.oy - LIST_Y) / SPELLBOOK_ROW_H;
        if (at >= 0 && at < g.list->count) {
            /* A second click on the chosen spell readies it. */
            if (at == g.selected && !g.buy) {
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
        if (g.buy) {
            buy_selected();
            return SPELLBOOK_STAY;
        }
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
    } else if (vk == 0x2E && !g.buy) {
        delete_selected();
    }
    return SPELLBOOK_STAY;
}
