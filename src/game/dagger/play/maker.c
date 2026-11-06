#include "game/dagger/play/maker.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/effects.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See maker.h. */

/* DFU's rectangles on classic's 320 x 200. */
#define SLOT_X 3
#define SLOT_Y0 30
#define SLOT_STEP 32
#define SLOT_W 230
#define ADD_X 244
#define ADD_Y 114
#define ADD_W 28
#define ADD_H 28
#define BUY_X 244
#define BUY_Y 147
#define NEW_Y 163
#define EXIT_Y 179
#define BTN_W 24
#define BTN_H 16
#define TARGET_X 275
#define ELEMENT_X 299
#define TARGET_Y0 114
#define NEXT_X 275
#define NEXT_Y 80
#define PREV_Y 96
#define ARROW_W 9
#define ARROW_H 16
#define ICON_X 288
#define ICON_Y 94
#define NAME_X 59
#define NAME_Y 184
#define NAME_W 142
#define NAME_H 7
#define ICON_SIZE 16
#define ICONS_PER_ROW 20
#define ICON_FILE_W 320
#define MASK_FILE_W 40
#define TARGET_W 24
#define ICONS 69
/* The second picture's exit and spinners. */
#define EDIT_EXIT_X 281
#define EDIT_EXIT_Y 94
#define SPIN_W 24
#define SPIN_H 16
#define SPIN_BAND 5
/* The pickers' scroll and its rows. */
#define PICK_X 40
#define PICK_Y 14
#define PICK_W 240
#define PICK_H 172
#define PICK_ROWS 14
#define PICK_ROW_H 10
#define LIST_MAX 96

enum {
    SP_DUR_BASE, SP_DUR_PLUS, SP_DUR_PER, SP_CH_BASE, SP_CH_PLUS, SP_CH_PER,
    SP_MAG_BASE_LOW, SP_MAG_BASE_HIGH, SP_MAG_PLUS_LOW, SP_MAG_PLUS_HIGH, SP_MAG_PER,
    SPINNERS
};

typedef struct Spinner {
    short x;
    short y;
    short high;
    df_u8 supports;
} Spinner;

static const Spinner g_spin[SPINNERS] = {
    { 64, 94, MAKER_DURATION_MAX, MAGIC_DURATION }, { 104, 94, MAKER_DURATION_MAX, MAGIC_DURATION },
    { 160, 94, MAKER_PER_MAX, MAGIC_DURATION },
    { 64, 114, MAKER_CHANCE_MAX, MAGIC_CHANCE }, { 104, 114, MAKER_CHANCE_MAX, MAGIC_CHANCE },
    { 160, 114, MAKER_PER_MAX, MAGIC_CHANCE },
    { 64, 134, MAKER_MAGNITUDE_MAX, MAGIC_MAGNITUDE }, { 104, 134, MAKER_MAGNITUDE_MAX, MAGIC_MAGNITUDE },
    { 144, 134, MAKER_MAGNITUDE_MAX, MAGIC_MAGNITUDE }, { 184, 134, MAKER_MAGNITUDE_MAX, MAGIC_MAGNITUDE },
    { 235, 134, MAKER_PER_MAX, MAGIC_MAGNITUDE }
};

static struct {
    int open;
    Composer composer;
    Spellbook *book;
    Player *player;
    const df_u8 *main_pic;
    const df_u8 *edit_pic;
    const df_u8 *icons;
    const df_u8 *masks_lit;
    const df_u8 *masks_dim;
    Spell spell;
    int mode;
    /* The slot being picked for or edited, and the settings in the editor. */
    int slot;
    EffectSettings edit;
    /* The pickers' lists: effect ids, how many, the scroll, and the group chosen. */
    int list[LIST_MAX];
    int listed;
    int scroll;
    int group;
    int allowed_targets;
    int allowed_elements;
    char name[SPELL_NAME];
    int name_len;
    const char *note;
    int changed;
} g;

/* ---- The spell. ---- */

static int used(int k)
{
    return g.spell.effect[k].effect != SPELL_NO_EFFECT;
}

static void clear_spell(void)
{
    int k;

    mem_zero(&g.spell, sizeof(g.spell));
    for (k = 0; k < MAGIC_SLOTS; k++) {
        g.spell.effect[k].effect = SPELL_NO_EFFECT;
    }
    g.spell.target = TARGET_CASTER;
    g.spell.element = ELEMENT_MAGIC;
    g.spell.icon = 0;
    g.name[0] = 0;
    g.name_len = 0;
}

/* What the effects allow together: targets all of them allow, elements any does (magic always). Then a choice that is still allowed. */
static void recompute_allowed(void)
{
    int k;
    int bit;

    g.allowed_targets = MAGIC_TARGET_ALL;
    g.allowed_elements = MAGIC_ELEMENT_MAGIC_ONLY;
    for (k = 0; k < MAGIC_SLOTS; k++) {
        const MagicEffect *fx;

        fx = used(k) ? magic_effect(g.spell.effect[k].effect) : 0;
        if (fx) {
            g.allowed_targets &= fx->targets;
            g.allowed_elements |= fx->elements;
        }
    }
    if (!(g.allowed_targets & (1 << g.spell.target))) {
        for (bit = 0; bit < TARGET_COUNT; bit++) {
            if (g.allowed_targets & (1 << bit)) {
                g.spell.target = (df_u8)bit;
                break;
            }
        }
    }
    if (!(g.allowed_elements & (1 << g.spell.element))) {
        for (bit = 0; bit < ELEMENT_COUNT; bit++) {
            if (g.allowed_elements & (1 << bit)) {
                g.spell.element = (df_u8)bit;
                break;
            }
        }
    }
}

const Spell *maker_spell(void)
{
    return &g.spell;
}

int maker_mode(void)
{
    return g.mode;
}

int maker_cost(int *gold)
{
    return spell_cost(&g.spell, &g.player->sheet, gold);
}

int maker_list_count(void)
{
    return g.listed;
}

const char *maker_list_name(int i)
{
    const MagicEffect *fx;

    fx = i >= 0 && i < g.listed ? magic_effect(g.list[i]) : 0;
    if (!fx) {
        return "";
    }
    return g.mode == MAKER_MODE_SUB && fx->sub[0] ? fx->sub : fx->group;
}

const char *maker_note(void)
{
    return g.note;
}

/* DFU's defaults for an effect just added: a round, a percent and a point, each growing one a level. */
static void defaults(EffectSettings *e, int effect)
{
    mem_zero(e, sizeof(*e));
    e->effect = (df_u16)effect;
    e->duration_base = e->duration_plus = e->duration_per = 1;
    e->chance_base = e->chance_plus = e->chance_per = 1;
    e->magnitude_base_low = e->magnitude_base_high = e->magnitude_plus_low = e->magnitude_plus_high = e->magnitude_per = 1;
}

/* ---- The lists. ---- */

static int effect_order(int a, int b)
{
    return df_eq(magic_effect(a)->group, magic_effect(b)->group) ? 0 : 1;
}

/* The groups there are an effect of, built or recorded, by name; each listed by its first effect's id. */
static void build_groups(void)
{
    RecTable *t;
    int i;
    int j;

    t = effect_table();
    g.listed = 0;
    for (i = 0; i < t->count && g.listed < LIST_MAX; i++) {
        const MagicEffect *fx;
        int seen;

        fx = (const MagicEffect *)(t->rows + (df_u32)i * sizeof(MagicEffect));
        if (effects_built(fx->id) == EFFECT_NOT_BUILT) {
            continue;
        }
        seen = 0;
        for (j = 0; j < g.listed; j++) {
            if (!effect_order(g.list[j], fx->id)) {
                seen = 1;
            }
        }
        if (!seen) {
            g.list[g.listed++] = fx->id;
        }
    }
    /* By name: an insertion sort over the group names (names differ in their first letters well enough for case folding to be plain). */
    for (i = 1; i < g.listed; i++) {
        int key;

        key = g.list[i];
        for (j = i - 1; j >= 0; j--) {
            const char *a;
            const char *b;
            int k;

            a = magic_effect(g.list[j])->group;
            b = magic_effect(key)->group;
            for (k = 0; a[k] && a[k] == b[k]; k++) {
            }
            if (a[k] <= b[k]) {
                break;
            }
            g.list[j + 1] = g.list[j];
        }
        g.list[j + 1] = key;
    }
    g.scroll = 0;
}

/* The built effects of one group, in subgroup order. */
static void build_subs(int group)
{
    RecTable *t;
    int i;

    t = effect_table();
    g.listed = 0;
    for (i = 0; i < t->count && g.listed < LIST_MAX; i++) {
        const MagicEffect *fx;

        fx = (const MagicEffect *)(t->rows + (df_u32)i * sizeof(MagicEffect));
        if ((fx->id >> 8) == group && effects_built(fx->id) != EFFECT_NOT_BUILT) {
            g.list[g.listed++] = fx->id;
        }
    }
    g.scroll = 0;
}

/* ---- Doing. ---- */

static void say(const char *line)
{
    g.note = line;
    g.changed = 1;
}

/* An effect chosen for the slot: the editor opens on it, unless it would leave no target in common. */
static void effect_chosen(int effect)
{
    const MagicEffect *fx;
    int k;
    int targets;

    fx = magic_effect(effect);
    targets = fx ? fx->targets : 0;
    for (k = 0; k < MAGIC_SLOTS; k++) {
        const MagicEffect *other;

        other = k != g.slot && used(k) ? magic_effect(g.spell.effect[k].effect) : 0;
        if (other) {
            targets &= other->targets;
        }
    }
    if (!fx || !targets) {
        g.mode = MAKER_MODE_MAIN;
        say("That cannot be joined with the other effects.");
        return;
    }
    defaults(&g.edit, effect);
    g.spell.effect[g.slot] = g.edit;
    recompute_allowed();
    g.mode = MAKER_MODE_EDIT;
    g.note = 0;
    g.changed = 1;
}

static int first_free_slot(void)
{
    int k;

    for (k = 0; k < MAGIC_SLOTS; k++) {
        if (!used(k)) {
            return k;
        }
    }
    return -1;
}

static void group_chosen(int effect)
{
    int n;

    build_subs(effect >> 8);
    n = g.listed;
    if (n == 1) {
        effect_chosen(g.list[0]);
        return;
    }
    g.group = effect >> 8;
    g.mode = MAKER_MODE_SUB;
    g.changed = 1;
}

static void buy(void)
{
    int gold;
    int cost;

    if (spell_effects(&g.spell) == 0) {
        say("The spell has no effect.");
        return;
    }
    cost = spell_cost(&g.spell, &g.player->sheet, &gold);
    (void)cost;
    if ((int)g.player->pack.gold < gold) {
        say("You do not have enough gold.");
        return;
    }
    if (!g.name[0]) {
        say("You must choose a name.");
        return;
    }
    df_copy(g.spell.name, g.name, SPELL_NAME);
    if (!spellbook_add(g.book, &g.spell)) {
        say("There is a spell of that name already, or no room for another.");
        return;
    }
    g.player->pack.gold -= (df_u32)gold;
    clear_spell();
    recompute_allowed();
    say("The spell is inscribed in your spellbook.");
}

int maker_open(Spellbook *book, Player *player)
{
    df_u32 size;

    maker_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.book = book;
    g.player = player;
    g.main_pic = composer_table(&g.composer, "INFO01I0.IMG", &size);
    if (!g.main_pic || size < (df_u32)CANVAS_BYTES) {
        g.main_pic = 0;
    }
    g.edit_pic = composer_table(&g.composer, "MASK05I0.IMG", &size);
    if (!g.edit_pic || size < (df_u32)CANVAS_BYTES) {
        g.edit_pic = 0;
    }
    g.icons = composer_table(&g.composer, "ICON00I0.IMG", &size);
    g.masks_lit = composer_table(&g.composer, "MASK04I0.IMG", &size);
    g.masks_dim = composer_table(&g.composer, "MASK01I0.IMG", &size);
    if (!g.main_pic || !g.edit_pic) {
        maker_close();
        return 0;
    }
    clear_spell();
    recompute_allowed();
    g.changed = 1;
    return 1;
}

void maker_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

const unsigned char *maker_pixels(void)
{
    return g.composer.screen;
}

int maker_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

/* ---- Drawing. ---- */

static void number(int x, int y, int value)
{
    char text[16];

    log_format(text, (int)sizeof(text), "%d", value);
    canvas_label(x, y, text);
}

static void compose_effect_slot(int k)
{
    Rect r;
    char name[48];
    char line[64];
    const MagicEffect *fx;
    const EffectSettings *e;
    int y;

    if (!used(k)) {
        return;
    }
    e = &g.spell.effect[k];
    r.x = SLOT_X;
    r.y = (df_u16)(SLOT_Y0 + k * SLOT_STEP);
    r.w = SLOT_W;
    r.h = 9;
    spell_effect_name(e->effect, name, (int)sizeof(name));
    canvas_label_centred(&r, 0, name);
    fx = magic_effect(e->effect);
    y = 10;
    if (fx && (fx->supports & MAGIC_DURATION)) {
        log_format(line, (int)sizeof(line), "Duration %d + %d per %d level(s)", e->duration_base, e->duration_plus, e->duration_per);
        canvas_label_centred(&r, y, line);
        y += CANVAS_LINE;
    }
    if (fx && (fx->supports & MAGIC_CHANCE)) {
        log_format(line, (int)sizeof(line), "Chance %d + %d per %d level(s)", e->chance_base, e->chance_plus, e->chance_per);
        canvas_label_centred(&r, y, line);
        y += CANVAS_LINE;
    }
    if (fx && (fx->supports & MAGIC_MAGNITUDE)) {
        log_format(line, (int)sizeof(line), "%d-%d + %d-%d per %d level(s)", e->magnitude_base_low, e->magnitude_base_high,
                   e->magnitude_plus_low, e->magnitude_plus_high, e->magnitude_per);
        canvas_label_centred(&r, y, line);
    }
}

/* A button of the target or element column: its colour icon when chosen, its grey one when only allowed, none when not. */
static void compose_choice(const df_u8 *lit, const df_u8 *dim, int sx, int sw, int row, int x, int y, int allowed, int chosen)
{
    const df_u8 *from;

    if (!allowed) {
        return;
    }
    from = chosen ? lit : dim;
    if (from) {
        canvas_copy_part(from, MASK_FILE_W, sx, row * ICON_SIZE, sw, ICON_SIZE, x, y);
    }
}

static void compose_main(void)
{
    char text[40];
    int k;
    int gold;
    int points;

    canvas_copy_all(g.main_pic);
    for (k = 0; k < MAGIC_SLOTS; k++) {
        compose_effect_slot(k);
    }
    if (g.icons && g.spell.icon < ICONS) {
        canvas_copy_part(g.icons, ICON_FILE_W, (g.spell.icon % ICONS_PER_ROW) * ICON_SIZE, (g.spell.icon / ICONS_PER_ROW) * ICON_SIZE,
                         ICON_SIZE, ICON_SIZE, ICON_X, ICON_Y);
    }
    for (k = 0; k < TARGET_COUNT; k++) {
        compose_choice(g.masks_lit, g.masks_dim, 0, TARGET_W, k, TARGET_X, TARGET_Y0 + k * ICON_SIZE,
                       (g.allowed_targets >> k) & 1, g.spell.target == k);
    }
    for (k = 0; k < ELEMENT_COUNT; k++) {
        compose_choice(g.masks_lit, g.masks_dim, TARGET_W, ICON_SIZE, k, ELEMENT_X, TARGET_Y0 + k * ICON_SIZE,
                       (g.allowed_elements >> k) & 1, g.spell.element == k);
    }
    points = spell_cost(&g.spell, &g.player->sheet, &gold);
    number(46, 151, g.player->sheet.magicka_max);
    number(40, 159, (int)g.player->pack.gold);
    number(59, 167, gold);
    number(70, 176, points);
    log_format(text, (int)sizeof(text), "%s%s", g.mode == MAKER_MODE_NAME ? g.name : (g.name[0] ? g.name : ""),
               g.mode == MAKER_MODE_NAME ? "_" : "");
    canvas_label(NAME_X + 1, NAME_Y, text);
}

static void compose_picker(void)
{
    Rect scroll;
    int i;

    scroll.x = PICK_X;
    scroll.y = PICK_Y;
    scroll.w = PICK_W;
    scroll.h = PICK_H;
    canvas_scroll(&scroll);
    canvas_label(PICK_X + 14, PICK_Y + 12, g.mode == MAKER_MODE_GROUP ? "Choose an effect" : "Choose which");
    for (i = 0; i < PICK_ROWS; i++) {
        const MagicEffect *fx;
        char name[48];
        int at;

        at = g.scroll + i;
        if (at >= g.listed) {
            break;
        }
        fx = magic_effect(g.list[at]);
        if (g.mode == MAKER_MODE_GROUP || !fx) {
            df_copy(name, fx ? fx->group : "?", (int)sizeof(name));
        } else {
            spell_effect_name(g.list[at], name, (int)sizeof(name));
        }
        canvas_label(PICK_X + 14, PICK_Y + 26 + i * PICK_ROW_H, name);
    }
}

/* One spinner's value, or nothing for a parameter the effect has not. */
static int spin_value(const EffectSettings *e, int i)
{
    const df_u8 *v[SPINNERS];

    v[SP_DUR_BASE] = &e->duration_base;
    v[SP_DUR_PLUS] = &e->duration_plus;
    v[SP_DUR_PER] = &e->duration_per;
    v[SP_CH_BASE] = &e->chance_base;
    v[SP_CH_PLUS] = &e->chance_plus;
    v[SP_CH_PER] = &e->chance_per;
    v[SP_MAG_BASE_LOW] = &e->magnitude_base_low;
    v[SP_MAG_BASE_HIGH] = &e->magnitude_base_high;
    v[SP_MAG_PLUS_LOW] = &e->magnitude_plus_low;
    v[SP_MAG_PLUS_HIGH] = &e->magnitude_plus_high;
    v[SP_MAG_PER] = &e->magnitude_per;
    return *v[i];
}

static void spin_set(EffectSettings *e, int i, int value)
{
    df_u8 *v[SPINNERS];

    v[SP_DUR_BASE] = &e->duration_base;
    v[SP_DUR_PLUS] = &e->duration_plus;
    v[SP_DUR_PER] = &e->duration_per;
    v[SP_CH_BASE] = &e->chance_base;
    v[SP_CH_PLUS] = &e->chance_plus;
    v[SP_CH_PER] = &e->chance_per;
    v[SP_MAG_BASE_LOW] = &e->magnitude_base_low;
    v[SP_MAG_BASE_HIGH] = &e->magnitude_base_high;
    v[SP_MAG_PLUS_LOW] = &e->magnitude_plus_low;
    v[SP_MAG_PLUS_HIGH] = &e->magnitude_plus_high;
    v[SP_MAG_PER] = &e->magnitude_per;
    *v[i] = (df_u8)value;
    /* A range keeps its low under its high: the one moved drags the other. */
    if (i == SP_MAG_BASE_LOW && e->magnitude_base_low > e->magnitude_base_high) {
        e->magnitude_base_high = e->magnitude_base_low;
    } else if (i == SP_MAG_BASE_HIGH && e->magnitude_base_high < e->magnitude_base_low) {
        e->magnitude_base_low = e->magnitude_base_high;
    } else if (i == SP_MAG_PLUS_LOW && e->magnitude_plus_low > e->magnitude_plus_high) {
        e->magnitude_plus_high = e->magnitude_plus_low;
    } else if (i == SP_MAG_PLUS_HIGH && e->magnitude_plus_high < e->magnitude_plus_low) {
        e->magnitude_plus_low = e->magnitude_plus_high;
    }
}

static void compose_edit(void)
{
    const MagicEffect *fx;
    char text[64];
    Rect r;
    int i;
    int gold;

    canvas_copy_all(g.edit_pic);
    fx = magic_effect(g.edit.effect);
    r.x = 5;
    r.y = 19;
    r.w = 312;
    r.h = 69;
    spell_effect_name(g.edit.effect, text, (int)sizeof(text));
    canvas_label_centred(&r, 0, text);
    if (fx) {
        canvas_label_centred(&r, 14, (fx->supports & MAGIC_DURATION) ? "Duration: how many minutes it lasts." : "It acts at once.");
        if (fx->supports & MAGIC_CHANCE) {
            canvas_label_centred(&r, 24, "Chance: the percent that it takes hold.");
        }
        if (fx->supports & MAGIC_MAGNITUDE) {
            canvas_label_centred(&r, 34, "Magnitude: the points it does, a range and more for each level.");
        }
    }
    for (i = 0; i < SPINNERS; i++) {
        char v[8];
        int w;

        if (!fx || !(fx->supports & g_spin[i].supports)) {
            continue;
        }
        log_format(v, (int)sizeof(v), "%d", spin_value(&g.edit, i));
        w = canvas_width(v);
        canvas_label(g_spin[i].x + (SPIN_W - w) / 2, g_spin[i].y + 4, v);
    }
    gold = spell_effect_gold(&g.edit, 0, 0);
    number(272, 124, gold);
}

void maker_compose(void)
{
    canvas_use(&g.composer);
    if (g.mode == MAKER_MODE_EDIT) {
        compose_edit();
    } else {
        compose_main();
        if (g.mode == MAKER_MODE_GROUP || g.mode == MAKER_MODE_SUB) {
            compose_picker();
        }
    }
}

/* ---- Clicks and keys. ---- */

static int in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static int click_edit(int x, int y)
{
    int i;
    const MagicEffect *fx;

    if (in_rect(x, y, EDIT_EXIT_X, EDIT_EXIT_Y, BTN_W, BTN_H)) {
        g.spell.effect[g.slot] = g.edit;
        g.mode = MAKER_MODE_MAIN;
        g.changed = 1;
        return MAKER_STAY;
    }
    fx = magic_effect(g.edit.effect);
    for (i = 0; fx && i < SPINNERS; i++) {
        if ((fx->supports & g_spin[i].supports) && in_rect(x, y, g_spin[i].x, g_spin[i].y, SPIN_W, SPIN_H)) {
            int v;

            v = spin_value(&g.edit, i);
            if (y < g_spin[i].y + SPIN_BAND) {
                v++;
            } else if (y >= g_spin[i].y + SPIN_H - SPIN_BAND) {
                v--;
            }
            v = v < 1 ? 1 : (v > g_spin[i].high ? g_spin[i].high : v);
            spin_set(&g.edit, i, v);
            g.spell.effect[g.slot] = g.edit;
            g.changed = 1;
        }
    }
    return MAKER_STAY;
}

static int click_picker(int x, int y)
{
    int at;

    if (!in_rect(x, y, PICK_X, PICK_Y, PICK_W, PICK_H)) {
        g.mode = MAKER_MODE_MAIN;
        g.changed = 1;
        return MAKER_STAY;
    }
    at = g.scroll + (y - PICK_Y - 26) / PICK_ROW_H;
    if (y >= PICK_Y + 26 && at >= 0 && at < g.listed) {
        if (g.mode == MAKER_MODE_GROUP) {
            group_chosen(g.list[at]);
        } else {
            effect_chosen(g.list[at]);
        }
    }
    return MAKER_STAY;
}

int maker_click(int x, int y)
{
    int k;

    if (g.mode == MAKER_MODE_EDIT) {
        return click_edit(x, y);
    }
    if (g.mode == MAKER_MODE_GROUP || g.mode == MAKER_MODE_SUB) {
        return click_picker(x, y);
    }
    g.mode = MAKER_MODE_MAIN;
    g.note = 0;
    if (in_rect(x, y, BUY_X, EXIT_Y, BTN_W, BTN_H)) {
        return MAKER_LEAVE;
    }
    if (in_rect(x, y, BUY_X, BUY_Y, BTN_W, BTN_H)) {
        buy();
    } else if (in_rect(x, y, BUY_X, NEW_Y, BTN_W, BTN_H)) {
        clear_spell();
        recompute_allowed();
        g.changed = 1;
    } else if (in_rect(x, y, ADD_X, ADD_Y, ADD_W, ADD_H)) {
        g.slot = first_free_slot();
        if (g.slot < 0) {
            say("A spell holds three effects.");
        } else {
            build_groups();
            g.mode = MAKER_MODE_GROUP;
            g.changed = 1;
        }
    } else if (in_rect(x, y, NAME_X, NAME_Y, NAME_W, NAME_H + 2)) {
        g.mode = MAKER_MODE_NAME;
        g.changed = 1;
    } else if (in_rect(x, y, NEXT_X, NEXT_Y, ARROW_W, ARROW_H)) {
        g.spell.icon = (df_u8)((g.spell.icon + 1) % ICONS);
        g.changed = 1;
    } else if (in_rect(x, y, NEXT_X, PREV_Y, ARROW_W, ARROW_H)) {
        g.spell.icon = (df_u8)((g.spell.icon + ICONS - 1) % ICONS);
        g.changed = 1;
    } else {
        for (k = 0; k < MAGIC_SLOTS; k++) {
            if (used(k) && in_rect(x, y, SLOT_X, SLOT_Y0 + k * SLOT_STEP, SLOT_W, 9 + 3 * CANVAS_LINE)) {
                g.slot = k;
                g.edit = g.spell.effect[k];
                g.mode = MAKER_MODE_EDIT;
                g.changed = 1;
                return MAKER_STAY;
            }
        }
        for (k = 0; k < TARGET_COUNT; k++) {
            if (in_rect(x, y, TARGET_X, TARGET_Y0 + k * ICON_SIZE, TARGET_W, ICON_SIZE) && ((g.allowed_targets >> k) & 1)) {
                g.spell.target = (df_u8)k;
                g.changed = 1;
            }
        }
        for (k = 0; k < ELEMENT_COUNT; k++) {
            if (in_rect(x, y, ELEMENT_X, TARGET_Y0 + k * ICON_SIZE, ICON_SIZE, ICON_SIZE) && ((g.allowed_elements >> k) & 1)) {
                g.spell.element = (df_u8)k;
                g.changed = 1;
            }
        }
    }
    return MAKER_STAY;
}

int maker_key(int vk)
{
    if (g.mode == MAKER_MODE_NAME) {
        if (vk == 0x0D || vk == 0x1B) {
            g.mode = MAKER_MODE_MAIN;
        } else if (vk == 0x08) {
            if (g.name_len > 0) {
                g.name[--g.name_len] = 0;
            }
        } else if (g.name_len < SPELL_NAME - 1 && (vk == 0x20 || (vk >= 0x30 && vk <= 0x39) || (vk >= 0x41 && vk <= 0x5A))) {
            int c;

            c = vk;
            /* Capital at the start of a word, small after. */
            if (vk >= 0x41 && g.name_len > 0 && g.name[g.name_len - 1] != ' ') {
                c = vk + 32;
            }
            g.name[g.name_len++] = (char)c;
            g.name[g.name_len] = 0;
        }
        g.changed = 1;
        return MAKER_STAY;
    }
    if (vk == 0x1B) {
        if (g.mode != MAKER_MODE_MAIN) {
            if (g.mode == MAKER_MODE_EDIT) {
                g.spell.effect[g.slot] = g.edit;
            }
            g.mode = MAKER_MODE_MAIN;
            g.changed = 1;
            return MAKER_STAY;
        }
        return MAKER_LEAVE;
    }
    if (g.mode == MAKER_MODE_MAIN && (vk == 0x2E || vk == 0x08) && g.slot >= 0 && g.slot < MAGIC_SLOTS && used(g.slot)) {
        g.spell.effect[g.slot].effect = SPELL_NO_EFFECT;
        recompute_allowed();
        g.changed = 1;
    } else if ((g.mode == MAKER_MODE_GROUP || g.mode == MAKER_MODE_SUB) && (vk == 0x28 || vk == 0x26)) {
        g.scroll += vk == 0x28 ? 1 : -1;
        g.scroll = g.scroll > g.listed - PICK_ROWS ? g.listed - PICK_ROWS : g.scroll;
        g.scroll = g.scroll < 0 ? 0 : g.scroll;
        g.changed = 1;
    }
    return MAKER_STAY;
}
