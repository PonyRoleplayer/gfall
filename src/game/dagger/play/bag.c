#include "game/dagger/play/bag.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/doll.h"
#include "game/dagger/play/loot.h"
#include "game/dagger/play/pile.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/tables/items.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See bag.h. */

/* The four tabs along the top; only the first has anything in it yet. */
enum {
    TAB_GEAR,
    TAB_MAGIC,
    TAB_CLOTHING,
    TAB_INGREDIENTS,
    TABS
};
static const Rect g_tab_rect[TABS] = {
    { 0, 0, 92, 10 },
    { 93, 0, 69, 10 },
    { 163, 0, 91, 10 },
    { 255, 0, 65, 10 }
};

/* The column of buttons. Info, Equip, Remove and Use are modes: what a click on an item does. */
enum {
    BUTTON_WAGON,
    BUTTON_INFO,
    BUTTON_EQUIP,
    BUTTON_REMOVE,
    BUTTON_USE,
    BUTTON_GOLD,
    BUTTONS
};
static const Rect g_button_rect[BUTTONS] = {
    { 226, 14, 31, 14 },
    { 226, 36, 31, 14 },
    { 226, 58, 31, 14 },
    { 226, 80, 31, 14 },
    { 226, 103, 31, 14 },
    { 226, 126, 31, 14 }
};
static const Rect g_exit_rect = { 222, 178, 39, 22 };

/* The two lists: the player's and what is beside them. */
enum {
    LIST_LOCAL,
    LIST_REMOTE,
    LISTS
};
/* Each list's left edge; its arrows are the first 9 pixels, its items the 50 after. */
static const int g_list_x[LISTS] = { 163, 261 };
#define LIST_Y 48
#define LIST_H 152
#define ARROW_W 9
#define ARROW_H 16
#define CELL_W 50
#define CELL_H 38
#define ROWS 4
/* An item's picture fits inside its cell less this much a side, and is never enlarged. */
#define ICON_MARGIN 2
#define ICON_W (CELL_W - 2 * ICON_MARGIN)
#define ICON_H (CELL_H - 2 * ICON_MARGIN)

/* What is over each list: how much it weighs. */
static const Rect g_target_rect[LISTS] = {
    { 165, 12, 55, 34 },
    { 263, 12, 55, 34 }
};
/* The small panel that names what the pointer is over, and the part of ITEM00I0.IMG behind it. */
static const Rect g_panel_rect = { 223, 145, 37, 32 };
static const Rect g_panel_stone = { 196, 68, 50, 37 };
/* The doll's place, and each body part's armour number on it (BODY_*, DFU's PaperDoll). */
#define DOLL_X 49
#define DOLL_Y 13
static const df_u8 g_armor_label[BODY_PARTS][2] = {
    { 70, 12 }, { 20, 38 }, { 86, 38 }, { 12, 58 }, { 6, 90 }, { 18, 120 }, { 22, 168 }
};
/* Worn armour's number is the value a part has lost, over this. */
#define ARMOR_BARE 100
#define ARMOR_POINT 5

/* The info box over the doll: its lines. */
#define INFO_LINES 6
#define INFO_LINE_CHARS 40
#define INFO_X 54
#define INFO_Y 60
#define INFO_W 100
#define LINE_H CANVAS_LINE

/* Item pictures kept once fitted to a cell: a list scrolled back reads nothing again. */
#define ICONS 8
typedef struct Icon {
    /* Which picture, and which metal it is coloured; archive 0 is an empty place. */
    int archive;
    int record;
    int metal;
    int w;
    int h;
    df_u8 pixels[ICON_W * ICON_H];
} Icon;

/* What was under the pointer last: which list (or the doll), which row of the items. */
enum {
    OVER_NOTHING,
    OVER_LOCAL,
    OVER_REMOTE,
    OVER_DOLL
};

static struct {
    Player *player;
    /* What is being searched, or 0: the right list is then what is being dropped. */
    Entity *searched;
    Inventory *remote;
    /* The screen, the small font and the heaps. */
    Composer composer;
    const df_u8 *grey;
    const df_u8 *gold;
    const df_u8 *stone;
    const df_u8 *arrows_green;
    const df_u8 *arrows_red;
    df_u8 *doll;
    df_u8 *doll_owner;
    Icon *icons;
    int doll_stale;
    int tab;
    int mode;
    int scroll[LISTS];
    int over;
    int over_index;
    int changed;
    const char *note;
    /* The info box's lines, while it shows. */
    int info_shown;
    char info[INFO_LINES][INFO_LINE_CHARS];
    int open;
} g;

/* What the right list shows, and which of the searched thing's rolled things each is (loot.c). */
static Inventory g_remote;
static df_u8 g_origin[INVENTORY_MAX];

int bag_finds_things(Entity *searched, int level)
{
    if (loot_open(searched, level, &g_remote, g_origin) > 0) {
        return 1;
    }
    loot_closed(searched, &g_remote);
    return 0;
}

/*
 * What was dropped lands at the player's feet. Should there be nowhere to
 * put it, with no block loaded round them, it goes back in the pack, so
 * nothing is ever lost.
 */
static void land_what_was_dropped(void)
{
    int i;

    if (!g_remote.count || pile_drop(&g_remote, g.player->body.feet)) {
        return;
    }
    for (i = 0; i < g_remote.count; i++) {
        inventory_add(&g.player->pack, &g_remote.item[i]);
    }
}

void bag_close(void)
{
    if (!g.open) {
        return;
    }
    g.open = 0;
    if (g.searched) {
        loot_closed(g.searched, &g_remote);
    } else {
        land_what_was_dropped();
    }
    doll_forget_files();
    composer_close(&g.composer);
}

int bag_open(Player *p, Entity *searched)
{
    MemHeap *was;

    bag_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        log_text(LOG_WARN, "inventory: its heaps or its font did not come");
        return 0;
    }
    g.open = 1;
    g.player = p;
    g.searched = searched;
    g.mode = BUTTON_EQUIP;
    g.doll_stale = 1;
    g.remote = &g_remote;
    if (searched) {
        loot_open(searched, p->sheet.level, &g_remote, g_origin);
    } else {
        inventory_clear(&g_remote);
    }
    was = mem_heap_use(g.composer.heap);
    g.doll = (df_u8 *)mem_alloc(DOLL_W * DOLL_H);
    g.doll_owner = (df_u8 *)mem_alloc(DOLL_W * DOLL_H);
    g.icons = (Icon *)mem_alloc(sizeof(Icon) * ICONS);
    mem_heap_use(was);
    g.grey = composer_read(&g.composer, "INVE00I0.IMG");
    g.gold = composer_read(&g.composer, "INVE01I0.IMG");
    g.stone = composer_read(&g.composer, "ITEM00I0.IMG");
    g.arrows_green = composer_read(&g.composer, "INVE06I0.IMG");
    g.arrows_red = composer_read(&g.composer, "INVE07I0.IMG");
    if (!g.doll || !g.doll_owner || !g.icons || !g.grey || !g.gold || !g.stone || !g.arrows_green || !g.arrows_red) {
        log_text(LOG_WARN, "inventory: its pictures did not read");
        bag_close();
        return 0;
    }
    mem_zero(g.icons, sizeof(Icon) * ICONS);
    return 1;
}

const df_u8 *bag_pixels(void)
{
    return g.composer.screen;
}

int bag_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

const char *bag_note(void)
{
    return g.note;
}

/* ---- The lists: which items each shows, and in what row. ---- */

/* 1 when the player's list shows item i now: not worn, and under the chosen tab. */
static int local_shows(int i)
{
    const Item *it;

    it = &g.player->pack.item[i];
    if (inventory_worn_at(&g.player->pack, i) >= 0) {
        return 0;
    }
    /* All there is yet is weapons and armour, the first tab's. */
    return g.tab == TAB_GEAR && it->count > 0;
}

/* How many things a list shows. */
static int list_length(int list)
{
    int n;
    int i;

    if (list == LIST_REMOTE) {
        return g.remote->count;
    }
    n = 0;
    for (i = 0; i < g.player->pack.count; i++) {
        n += local_shows(i);
    }
    return n;
}

/* The item in a list at a place counted from its top, or -1. */
static int list_item(int list, int place)
{
    int i;

    if (place < 0) {
        return -1;
    }
    if (list == LIST_REMOTE) {
        return place < g.remote->count ? place : -1;
    }
    for (i = 0; i < g.player->pack.count; i++) {
        if (local_shows(i) && place-- == 0) {
            return i;
        }
    }
    return -1;
}

/* The inventory a list reads. */
static Inventory *list_inventory(int list)
{
    return list == LIST_REMOTE ? g.remote : &g.player->pack;
}

/* A list scrolled no further than it has things to show: one that grew shorter comes back. */
static void clamp_scroll(int list)
{
    int furthest;

    furthest = list_length(list) - ROWS;
    furthest = furthest < 0 ? 0 : furthest;
    g.scroll[list] = g.scroll[list] > furthest ? furthest : (g.scroll[list] < 0 ? 0 : g.scroll[list]);
}

/* ---- Composing: each piece copied into the screen. ---- */

/* Scratch that has grown is let go: a file read into it goes with it, and is read again when next wanted. */
static MemHeap *scratch_begin(void)
{
    if (mem_heap_bytes(g.composer.scratch) > 512u * 1024u) {
        doll_forget_files();
    }
    return composer_scratch_begin(&g.composer);
}

/*
 * The fitted copy of item it's picture: from the kept ones if it is there,
 * else read, recoloured and shrunk into the place its record hashes to.
 * A plain direct-mapped cache: eight places, one of them for each picture.
 */
static const Icon *icon_of(const Item *it)
{
    const Image *picture;
    MemHeap *was;
    Icon *icon;
    int archive;
    int record;
    int metal;
    int row;
    int col;

    if (!item_picture(it, g.player->sheet.race, g.player->sheet.female, &archive, &record)) {
        return 0;
    }
    metal = item_metal(it);
    icon = &g.icons[(df_u32)(record * 31 + archive + metal * 7 + 7) % ICONS];
    if (icon->archive == archive && icon->record == record && icon->metal == metal) {
        return icon;
    }
    was = scratch_begin();
    picture = doll_read_picture(archive, record);
    composer_scratch_end(was);
    if (!picture) {
        return 0;
    }
    icon->archive = archive;
    icon->record = record;
    icon->metal = metal;
    /* Fit the box keeping its shape, the tighter side deciding; never larger than it is. */
    icon->w = picture->width;
    icon->h = picture->height;
    if (icon->w > ICON_W) {
        icon->h = icon->h * ICON_W / icon->w;
        icon->w = ICON_W;
    }
    if (icon->h > ICON_H) {
        icon->w = icon->w * ICON_H / icon->h;
        icon->h = ICON_H;
    }
    icon->w = icon->w < 1 ? 1 : icon->w;
    icon->h = icon->h < 1 ? 1 : icon->h;
    for (row = 0; row < icon->h; row++) {
        for (col = 0; col < icon->w; col++) {
            int index;

            index = picture->index[(row * picture->height / icon->h) * picture->width + col * picture->width / icon->w];
            /* In a list a mask has nothing to cut away: it is only clear. */
            if (index == DOLL_MASK) {
                index = 0;
            } else if (index && metal >= 0) {
                index = cif_metal(index, metal);
            }
            icon->pixels[row * icon->w + col] = (df_u8)index;
        }
    }
    return icon;
}

/* One list: its arrows, green where it can move, then the items in its rows. */
static void compose_list(int list)
{
    const Inventory *inv;
    int row;
    int x;

    x = g_list_x[list];
    inv = list_inventory(list);
    clamp_scroll(list);
    {
        /* The arrow files hold one list's column: 9 x 152, the arrows at its top and bottom. */
        const df_u8 *up;
        const df_u8 *down;
        int y;

        up = g.scroll[list] > 0 ? g.arrows_green : g.arrows_red;
        down = g.scroll[list] + ROWS < list_length(list) ? g.arrows_green : g.arrows_red;
        for (y = 0; y < ARROW_H; y++) {
            canvas_copy_part(up, ARROW_W, 0, y, ARROW_W, 1, x, LIST_Y + y);
            canvas_copy_part(down, ARROW_W, 0, LIST_H - ARROW_H + y, ARROW_W, 1, x, LIST_Y + LIST_H - ARROW_H + y);
        }
    }
    for (row = 0; row < ROWS; row++) {
        const Item *it;
        const Icon *icon;
        int i;
        int cell_x;
        int cell_y;

        i = list_item(list, g.scroll[list] + row);
        if (i < 0) {
            break;
        }
        it = &inv->item[i];
        cell_x = x + ARROW_W;
        cell_y = LIST_Y + row * CELL_H;
        icon = icon_of(it);
        if (icon) {
            canvas_cutout(icon->pixels, icon->w, icon->h, cell_x + (CELL_W - icon->w) / 2, cell_y + (CELL_H - icon->h) / 2);
        }
        /* A stack says how many, in its cell's lower right (ItemListScroller). */
        if (it->count > 1) {
            char number[12];

            log_format(number, (int)sizeof(number), "%d", it->count);
            canvas_label(cell_x + CELL_W - canvas_width(number) - 1, cell_y + CELL_H - CANVAS_LINE, number);
        }
    }
}

/* The doll, read again only when what is worn changed, and each body part's armour on it. */
static void compose_doll(void)
{
    int part;

    if (g.doll_stale) {
        MemHeap *was;

        was = scratch_begin();
        doll_compose(g.doll, g.doll_owner, &g.player->sheet, &g.player->pack);
        composer_scratch_end(was);
        g.doll_stale = 0;
    }
    canvas_copy_part(g.doll, DOLL_W, 0, 0, DOLL_W, DOLL_H, DOLL_X, DOLL_Y);
    for (part = 0; part < BODY_PARTS; part++) {
        char number[8];

        log_format(number, (int)sizeof(number), "%d", (ARMOR_BARE - g.player->sheet.armor[part]) / ARMOR_POINT);
        canvas_label(DOLL_X + g_armor_label[part][0], DOLL_Y + g_armor_label[part][1], number);
    }
}

/* What a list holds, over it: the player's load against what they can carry; a body or the ground. */
static void compose_targets(void)
{
    char line[32];
    const Rect *r;

    r = &g_target_rect[LIST_LOCAL];
    canvas_label(r->x + 1, r->y + 2, "Carrying");
    log_format(line, (int)sizeof(line), "%d/%d kg", df_iround(inventory_weight(&g.player->pack)),
               df_iround(rules_max_carry(g.player->sheet.attribute[ATTR_STRENGTH])));
    canvas_label(r->x + 1, r->y + 2 + LINE_H, line);
    r = &g_target_rect[LIST_REMOTE];
    canvas_label(r->x + 1, r->y + 2, g.searched ? loot_label(g.searched) : "Dropped");
    log_format(line, (int)sizeof(line), "%d kg", df_iround(inventory_weight(g.remote)));
    canvas_label(r->x + 1, r->y + 2 + LINE_H, line);
}

/* The item the pointer rests on, or 0. */
static const Item *item_under_pointer(void)
{
    if (g.over == OVER_LOCAL || g.over == OVER_DOLL) {
        return g.over_index >= 0 ? &g.player->pack.item[g.over_index] : 0;
    }
    return g.over == OVER_REMOTE && g.over_index >= 0 ? &g.remote->item[g.over_index] : 0;
}

/* The small panel: its stone, then the name of what the pointer is over, or the player's gold. */
static void compose_panel(void)
{
    const Item *it;
    char line[48];
    int row;
    int col;

    for (row = 0; row < g_panel_rect.h; row++) {
        for (col = 0; col < g_panel_rect.w; col++) {
            /* The stone is cut larger than the panel: shrunk to it, as DFU scales it. */
            int sx;
            int sy;

            sx = g_panel_stone.x + col * g_panel_stone.w / g_panel_rect.w;
            sy = g_panel_stone.y + row * g_panel_stone.h / g_panel_rect.h;
            g.composer.screen[(g_panel_rect.y + row) * CANVAS_W + g_panel_rect.x + col] = g.stone[sy * CANVAS_W + sx];
        }
    }
    it = item_under_pointer();
    if (it) {
        item_name(it, line, (int)sizeof(line));
        {
            Rect inside;

            inside.x = (df_u16)(g_panel_rect.x + 1);
            inside.y = (df_u16)(g_panel_rect.y + 1);
            inside.w = (df_u16)(g_panel_rect.w - 2);
            inside.h = g_panel_rect.h;
            canvas_text(&inside, line);
        }
        return;
    }
    canvas_label(g_panel_rect.x + 1, g_panel_rect.y + 1, "Gold");
    log_format(line, (int)sizeof(line), "%u", (unsigned)g.player->pack.gold);
    canvas_label(g_panel_rect.x + 1, g_panel_rect.y + 1 + LINE_H, line);
}

/* The info box, while one shows: a dark box over the doll, and a click anywhere puts it away. */
static void compose_info(void)
{
    Rect box;
    int line;
    int lines;

    lines = 0;
    while (lines < INFO_LINES && g.info[lines][0]) {
        lines++;
    }
    box.x = INFO_X;
    box.y = INFO_Y;
    box.w = INFO_W;
    box.h = (df_u16)(lines * LINE_H + 4);
    canvas_box(&box, CANVAS_DARK);
    for (line = 0; line < lines; line++) {
        canvas_label(INFO_X + 3, INFO_Y + 2 + line * LINE_H, g.info[line]);
    }
}

void bag_compose(void)
{
    canvas_use(&g.composer);
    canvas_copy_all(g.grey);
    canvas_copy_rect(g.gold, &g_tab_rect[g.tab]);
    canvas_copy_rect(g.gold, &g_button_rect[g.mode]);
    compose_doll();
    compose_targets();
    compose_list(LIST_LOCAL);
    compose_list(LIST_REMOTE);
    compose_panel();
    if (g.info_shown) {
        compose_info();
    }
    g.changed = 0;
}

/* ---- Pointing and clicking. ---- */

/* The list row a point falls in: its list (or -1) and the item there (-1 for an empty row). */
static int list_at(int x, int y, int *item)
{
    int list;

    *item = -1;
    for (list = 0; list < LISTS; list++) {
        int left;

        left = g_list_x[list] + ARROW_W;
        if (x >= left && x < left + CELL_W && y >= LIST_Y && y < LIST_Y + LIST_H) {
            *item = list_item(list, g.scroll[list] + (y - LIST_Y) / CELL_H);
            return list;
        }
    }
    return -1;
}

/* The worn item a point on the doll falls on, by the doll's own record of who drew there, or -1. */
static int doll_item_at(int x, int y)
{
    int owner;

    if (x < DOLL_X || x >= DOLL_X + DOLL_W || y < DOLL_Y || y >= DOLL_Y + DOLL_H) {
        return -1;
    }
    owner = g.doll_owner[(y - DOLL_Y) * DOLL_W + (x - DOLL_X)];
    return owner == DOLL_NO_OWNER ? -1 : g.player->pack.worn[owner];
}

void bag_point(int x, int y)
{
    int over;
    int index;
    int list;

    list = list_at(x, y, &index);
    if (list >= 0) {
        over = list == LIST_LOCAL ? OVER_LOCAL : OVER_REMOTE;
    } else {
        index = doll_item_at(x, y);
        over = index >= 0 ? OVER_DOLL : OVER_NOTHING;
    }
    if (index < 0) {
        over = OVER_NOTHING;
    }
    if (over != g.over || index != g.over_index) {
        g.over = over;
        g.over_index = index;
        g.changed = 1;
    }
}

/* The info box's lines for an item: its name and what it is worth. */
static void show_info(const Item *it)
{
    const WeaponTemplate *w;
    const ArmorTemplate *a;
    const Material *m;
    int line;

    mem_zero(g.info, sizeof(g.info));
    line = 0;
    item_name(it, g.info[line++], INFO_LINE_CHARS);
    log_format(g.info[line++], INFO_LINE_CHARS, "Weight: %.2f kg", it->weight * (float)it->count);
    log_format(g.info[line++], INFO_LINE_CHARS, "Worth: %u gold", (unsigned)it->value * it->count);
    w = weapon_template(it->template_id);
    a = armor_template(it->template_id);
    m = material(it->material);
    if (w) {
        log_format(g.info[line++], INFO_LINE_CHARS, "Damage: %d-%d", w->damage_min + (m ? m->modifier : 0),
                   w->damage_max + (m ? m->modifier : 0));
    } else if (a) {
        log_format(g.info[line++], INFO_LINE_CHARS, "Armor: %d", a->shield ? a->shield : (m ? m->armor : 0));
    }
    if (it->condition_max) {
        log_format(g.info[line++], INFO_LINE_CHARS, "Condition: %d%%", (int)((df_u32)it->condition * 100u / it->condition_max));
    }
    g.info_shown = 1;
}

/* What was worn changed: the doll is drawn again, and the armour and the hand follow. */
static void wear_changed(void)
{
    player_wear_changed(g.player);
    g.doll_stale = 1;
    g.over = OVER_NOTHING;
    g.over_index = -1;
}

/* A thing out of the pack into the right list: never onto a body or treasure, which are only emptied. */
static void drop_item(int i)
{
    Inventory *pack;

    pack = &g.player->pack;
    if (g.searched && !loot_takes_things(g.searched)) {
        g.note = "You cannot leave things here.";
        return;
    }
    if (!pile_room(g.searched, g_remote.count + 1) || inventory_add(&g_remote, &pack->item[i]) < 0) {
        g.note = "There is no room to drop it.";
        return;
    }
    inventory_remove(pack, i);
    wear_changed();
}

/* A thing off the right list into the pack, if the player can carry it. Gold goes straight to the purse. */
static void take_item(int i)
{
    Inventory *pack;
    const Item *it;
    int n;

    pack = &g.player->pack;
    it = &g.remote->item[i];
    if (it->template_id == ITEM_GOLD) {
        pack->gold += it->count;
    } else {
        if (inventory_weight(pack) + it->weight * (float)it->count
            > rules_max_carry(g.player->sheet.attribute[ATTR_STRENGTH])) {
            g.note = "You cannot carry any more.";
            return;
        }
        if (inventory_add(pack, it) < 0) {
            g.note = "Your pack is full.";
            return;
        }
    }
    if (g.searched) {
        loot_taken(g.searched, g_origin[i]);
        for (n = i; n + 1 < g.remote->count; n++) {
            g_origin[n] = g_origin[n + 1];
        }
    }
    inventory_remove(g.remote, i);
    g.over = OVER_NOTHING;
    g.over_index = -1;
}

/* A click on one of the player's things, by the mode. */
static void click_local(int i)
{
    switch (g.mode) {
    case BUTTON_INFO:
        show_info(&g.player->pack.item[i]);
        break;
    case BUTTON_EQUIP:
        g.note = character_refuses(&g.player->sheet, &g.player->pack.item[i]);
        if (g.note) {
            break;
        }
        if (inventory_wear(&g.player->pack, i)) {
            wear_changed();
        } else {
            g.note = "You cannot wear that.";
        }
        break;
    case BUTTON_REMOVE:
        drop_item(i);
        break;
    default:
        g.note = "You cannot use that.";
        break;
    }
}

/* A click on a worn thing: comes off, or off and away, by the mode. */
static void click_doll(int i)
{
    switch (g.mode) {
    case BUTTON_INFO:
        show_info(&g.player->pack.item[i]);
        break;
    case BUTTON_REMOVE:
        drop_item(i);
        break;
    default:
        inventory_take_off(&g.player->pack, inventory_worn_at(&g.player->pack, i));
        wear_changed();
        break;
    }
}

/* A click on the arrows of a list, if it was on them: its rows move one. 1 when it was. */
static int click_arrows(int x, int y)
{
    int list;

    for (list = 0; list < LISTS; list++) {
        if (x < g_list_x[list] || x >= g_list_x[list] + ARROW_W) {
            continue;
        }
        if (y >= LIST_Y && y < LIST_Y + ARROW_H) {
            g.scroll[list]--;
            return 1;
        }
        if (y >= LIST_Y + LIST_H - ARROW_H && y < LIST_Y + LIST_H) {
            g.scroll[list]++;
            return 1;
        }
    }
    return 0;
}

/* A click on a button, if it was on one: a mode chosen, or a word why not. 1 when it was. */
static int click_button(int x, int y)
{
    int b;

    for (b = 0; b < BUTTONS; b++) {
        if (!rect_has(&g_button_rect[b], x, y)) {
            continue;
        }
        if (b == BUTTON_WAGON) {
            g.note = "You have no wagon.";
        } else if (b == BUTTON_GOLD) {
            g.note = "Your gold stays with you for now.";
        } else {
            g.mode = b;
        }
        return 1;
    }
    return 0;
}

int bag_click(int x, int y)
{
    int list;
    int i;

    g.note = 0;
    g.changed = 1;
    /* An info box shown takes the click that puts it away. */
    if (g.info_shown) {
        g.info_shown = 0;
        return BAG_STAY;
    }
    if (rect_has(&g_exit_rect, x, y)) {
        return BAG_LEAVE;
    }
    for (i = 0; i < TABS; i++) {
        if (rect_has(&g_tab_rect[i], x, y)) {
            g.tab = i;
            g.scroll[LIST_LOCAL] = 0;
            return BAG_STAY;
        }
    }
    if (click_button(x, y) || click_arrows(x, y)) {
        return BAG_STAY;
    }
    list = list_at(x, y, &i);
    if (i >= 0) {
        if (list == LIST_LOCAL) {
            click_local(i);
        } else if (g.mode == BUTTON_INFO) {
            show_info(&g.remote->item[i]);
        } else {
            take_item(i);
        }
        return BAG_STAY;
    }
    i = doll_item_at(x, y);
    if (i >= 0) {
        click_doll(i);
    }
    return BAG_STAY;
}
