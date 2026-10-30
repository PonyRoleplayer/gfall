#include "game/dagger/play/bag.h"
#include "game/dagger/play/combat.h"
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
/* A shop's column (INVE08I0.IMG and its like) and the cost panel, where DFU's trade window has them. */
#define TRADE_COLUMN_X 222
#define TRADE_COLUMN_Y 10
#define TRADE_COLUMN_W 39
#define TRADE_COLUMN_H 190
#define COST_PANEL_X 49
#define COST_PANEL_Y 13
#define COST_PANEL_W 111
#define COST_PANEL_H 9
static const Rect g_trade_wagon = { 226, 14, 31, 14 };
static const Rect g_trade_info = { 226, 36, 31, 14 };
static const Rect g_trade_select = { 226, 58, 31, 14 };
static const Rect g_trade_steal = { 226, 112, 31, 14 };
static const Rect g_trade_action = { 226, 134, 31, 14 };
static const Rect g_trade_clear = { 226, 156, 31, 14 };

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
/* A shop's column has other buttons: the small panel stands between them, higher up to leave Steal clear. */
#define PANEL_Y_BUYING 76
#define PANEL_Y_TRADING 87
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
#define INFO_W 110
/* From the scroll's burnt edge to its words. */
#define INFO_MARGIN 10
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
    /* The Gold button's question, while it shows, and the digits typed. */
    int asking_gold;
    char gold_typed[9];
    /* The info box's lines, while it shows. */
    int info_shown;
    char info[INFO_LINES][INFO_LINE_CHARS];
    int open;
    /* TRADE_* (bag.h); 0 for the plain screen. The shop, its action column and its cost panel. */
    int trade;
    Shop shop;
    const df_u8 *trade_column;
    const df_u8 *cost_panel;
    /* Trading, 1 after the thief was seen: the game opens the court when this closes. */
    int stole;
} g;

/* What the right list shows, and which of the searched thing's rolled things each is (loot.c). */
static Inventory g_remote;
static df_u8 g_origin[INVENTORY_MAX];
/* Buying: what is chosen off the shelves, the left list, and which rolled thing each is. */
static Inventory g_basket;
static df_u8 g_basket_origin[INVENTORY_MAX];

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
    if (g.trade) {
        /* What was laid on the counter and not sold goes back; a basket not paid for was never taken. */
        if (g.trade != TRADE_BUY) {
            int i;

            for (i = 0; i < g_remote.count; i++) {
                inventory_add(&g.player->pack, &g_remote.item[i]);
            }
        }
    } else if (g.searched) {
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

int bag_open_trade(Player *p, const Shop *shop, int trade, int day)
{
    if (!bag_open(p, 0)) {
        return 0;
    }
    g.trade = trade;
    g.shop = *shop;
    g.mode = BUTTON_EQUIP;
    inventory_clear(&g_basket);
    inventory_clear(&g_remote);
    if (trade == TRADE_BUY) {
        shop_stock(shop, p->sheet.level, day, &g_remote, g_origin);
    }
    g.trade_column = composer_read(&g.composer, trade == TRADE_BUY ? "INVE08I0.IMG" : (trade == TRADE_SELL ? "INVE10I0.IMG" : "INVE12I0.IMG"));
    g.cost_panel = composer_read(&g.composer, "SHOP00I0.IMG");
    if (!g.trade_column || !g.cost_panel) {
        log_text(LOG_WARN, "trade: its pictures did not read");
        bag_close();
        return 0;
    }
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

/* The inventory the left list shows: the player's, or the basket while buying. */
static Inventory *local_inventory(void)
{
    return g.trade == TRADE_BUY ? &g_basket : &g.player->pack;
}

/* 1 when the player's list shows item i now: not worn, and under the chosen tab. */
static int local_shows(int i)
{
    const Item *it;

    if (g.trade == TRADE_BUY) {
        return g_basket.item[i].count > 0;
    }

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
    for (i = 0; i < local_inventory()->count; i++) {
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
    for (i = 0; i < local_inventory()->count; i++) {
        if (local_shows(i) && place-- == 0) {
            return i;
        }
    }
    return -1;
}

/* The inventory a list reads. */
static Inventory *list_inventory(int list)
{
    return list == LIST_REMOTE ? g.remote : local_inventory();
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
    canvas_label(r->x + 1, r->y + 2, g.trade == TRADE_BUY ? "Shelves" : (g.trade ? "Counter" : (g.searched ? loot_label(g.searched) : "Dropped")));
    log_format(line, (int)sizeof(line), "%d kg", df_iround(inventory_weight(g.remote)));
    canvas_label(r->x + 1, r->y + 2 + LINE_H, line);
}

/* The item the pointer rests on, or 0. */
static const Item *item_under_pointer(void)
{
    if (g.over == OVER_LOCAL) {
        return g.over_index >= 0 ? &local_inventory()->item[g.over_index] : 0;
    }
    if (g.over == OVER_DOLL) {
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
    int panel_y;

    panel_y = g.trade == TRADE_BUY ? PANEL_Y_BUYING : (g.trade ? PANEL_Y_TRADING : g_panel_rect.y);
    for (row = 0; row < g_panel_rect.h; row++) {
        for (col = 0; col < g_panel_rect.w; col++) {
            /* The stone is cut larger than the panel: shrunk to it, as DFU scales it. */
            int sx;
            int sy;

            sx = g_panel_stone.x + col * g_panel_stone.w / g_panel_rect.w;
            sy = g_panel_stone.y + row * g_panel_stone.h / g_panel_rect.h;
            g.composer.screen[(panel_y + row) * CANVAS_W + g_panel_rect.x + col] = g.stone[sy * CANVAS_W + sx];
        }
    }
    it = item_under_pointer();
    if (it) {
        item_name(it, line, (int)sizeof(line));
        {
            Rect inside;

            inside.x = (df_u16)(g_panel_rect.x + 1);
            inside.y = (df_u16)(panel_y + 1);
            inside.w = (df_u16)(g_panel_rect.w - 2);
            inside.h = g_panel_rect.h;
            canvas_text(&inside, line);
        }
        return;
    }
    canvas_label(g_panel_rect.x + 1, panel_y + 1, "Gold");
    log_format(line, (int)sizeof(line), "%u", (unsigned)g.player->pack.gold);
    canvas_label(g_panel_rect.x + 1, panel_y + 1 + LINE_H, line);
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
    box.x = INFO_X - INFO_MARGIN;
    box.y = INFO_Y - INFO_MARGIN;
    box.w = INFO_W + 2 * INFO_MARGIN;
    box.h = (df_u16)(lines * LINE_H + 2 * INFO_MARGIN);
    canvas_scroll(&box);
    for (line = 0; line < lines; line++) {
        canvas_label(INFO_X + 3, INFO_Y + 2 + line * LINE_H, g.info[line]);
    }
}

/* What the lists come to, bargained: what is paid while buying or repairing, what is got while selling. */
static int trade_price(void)
{
    const Inventory *list;
    int cost;
    int i;

    list = g.trade == TRADE_BUY ? &g_basket : &g_remote;
    cost = 0;
    for (i = 0; i < list->count; i++) {
        cost += g.trade == TRADE_REPAIR ? shop_repair_cost(&g.shop, &list->item[i]) : shop_cost(&g.shop, &list->item[i]);
    }
    return cost ? shop_trade_price(&g.shop, cost, &g.player->sheet, g.trade == TRADE_SELL) : 0;
}

/* The shop's column over the inventory's, and the price and the gold in its cost panel. */
static void compose_trade(void)
{
    char line[16];

    canvas_copy_part(g.trade_column, TRADE_COLUMN_W, 0, 0, TRADE_COLUMN_W, TRADE_COLUMN_H, TRADE_COLUMN_X, TRADE_COLUMN_Y);
    canvas_copy_part(g.cost_panel, COST_PANEL_W, 0, 0, COST_PANEL_W, COST_PANEL_H, COST_PANEL_X, COST_PANEL_Y);
    log_format(line, (int)sizeof(line), "%d", trade_price());
    canvas_label(COST_PANEL_X + 28, COST_PANEL_Y + 2, line);
    log_format(line, (int)sizeof(line), "%u", (unsigned)g.player->pack.gold);
    canvas_label(COST_PANEL_X + 68, COST_PANEL_Y + 2, line);
}

void bag_compose(void)
{
    canvas_use(&g.composer);
    canvas_copy_all(g.grey);
    canvas_copy_rect(g.gold, &g_tab_rect[g.tab]);
    if (!g.trade) {
        canvas_copy_rect(g.gold, &g_button_rect[g.mode]);
    }
    compose_doll();
    compose_targets();
    compose_list(LIST_LOCAL);
    compose_list(LIST_REMOTE);
    if (g.trade) {
        compose_trade();
    }
    compose_panel();
    if (g.info_shown) {
        compose_info();
    }
    if (g.asking_gold) {
        char line[40];
        Rect box;

        box.x = INFO_X - INFO_MARGIN;
        box.y = INFO_Y - INFO_MARGIN;
        box.w = INFO_W + 2 * INFO_MARGIN;
        box.h = 2 * LINE_H + 2 * INFO_MARGIN;
        canvas_scroll(&box);
        canvas_label(INFO_X + 3, INFO_Y + 2, "How much gold?");
        log_format(line, (int)sizeof(line), "%s_", g.gold_typed);
        canvas_label(INFO_X + 3, INFO_Y + 2 + LINE_H, line);
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
            /* The doll shows the hand in use: say where one that went to the other is. */
            if (g.player->pack.worn[EQUIP_LEFT_HAND] == i && inventory_hand_place(&g.player->pack) != EQUIP_LEFT_HAND
                && weapon_template(g.player->pack.item[i].template_id)) {
                g.note = "In your other hand: swap hands to use it.";
            }
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
            if (g.searched && !loot_takes_things(g.searched)) {
                g.note = "You cannot leave things here.";
            } else {
                g.asking_gold = 1;
                g.gold_typed[0] = 0;
            }
        } else {
            g.mode = b;
        }
        return 1;
    }
    return 0;
}

/* The gold typed goes to the right list, as a thing lying there (DFU: not more than is carried). */
static void drop_gold(void)
{
    Item it;
    df_u32 coins;
    int i;

    coins = 0;
    for (i = 0; g.gold_typed[i]; i++) {
        coins = coins * 10u + (df_u32)(g.gold_typed[i] - '0');
    }
    g.asking_gold = 0;
    if (coins < 1) {
        return;
    }
    if (coins > g.player->pack.gold || coins > 0xffffu) {
        g.note = "You do not have that much gold.";
        return;
    }
    if (!pile_room(g.searched, g_remote.count + 1) || !loot_gold_item(&it, (int)coins) || inventory_add(&g_remote, &it) < 0) {
        g.note = "There is no room to drop it.";
        return;
    }
    g.player->pack.gold -= coins;
}

int bag_asking(void)
{
    return g.asking_gold;
}

void bag_key(int vk)
{
    int n;

    if (!g.asking_gold) {
        return;
    }
    g.changed = 1;
    n = 0;
    while (g.gold_typed[n]) {
        n++;
    }
    if (vk == 0x1B) {
        g.asking_gold = 0;
    } else if (vk == 0x0D) {
        drop_gold();
    } else if (vk == 0x08) {
        if (n > 0) {
            g.gold_typed[n - 1] = 0;
        }
    } else if (vk >= '0' && vk <= '9' && n < (int)sizeof(g.gold_typed) - 1) {
        g.gold_typed[n] = (char)vk;
        g.gold_typed[n + 1] = 0;
    }
}

/* ---- A shop's clicks. ---- */

/* Item i of the shelves into the basket. */
static void to_basket(int i)
{
    int at;
    int n;

    at = inventory_add(&g_basket, &g_remote.item[i]);
    if (at < 0) {
        g.note = "You cannot carry any more.";
        return;
    }
    g_basket_origin[at] = g_origin[i];
    for (n = i; n + 1 < g_remote.count; n++) {
        g_origin[n] = g_origin[n + 1];
    }
    inventory_remove(&g_remote, i);
}

/* Item i of the basket back on the shelves. */
static void from_basket(int i)
{
    int at;
    int n;

    at = inventory_add(&g_remote, &g_basket.item[i]);
    if (at >= 0) {
        g_origin[at] = g_basket_origin[i];
    }
    for (n = i; n + 1 < g_basket.count; n++) {
        g_basket_origin[n] = g_basket_origin[n + 1];
    }
    inventory_remove(&g_basket, i);
}

/* Item i of the pack laid on the counter, if the shop deals in it or it is worn enough to mend. */
static void to_counter(int i)
{
    const Item *it;

    it = &g.player->pack.item[i];
    if (g.trade == TRADE_SELL && !shop_buys(&g.shop, it)) {
        g.note = "This shop does not deal in that.";
        return;
    }
    if (g.trade == TRADE_REPAIR && it->condition >= it->condition_max) {
        g.note = "That needs no repair.";
        return;
    }
    if (inventory_add(&g_remote, it) >= 0) {
        inventory_remove(&g.player->pack, i);
    }
}

/* Everything in the basket into the pack, and off the shelves for good. 0, with a note, when it will not all go. */
static int take_basket(void)
{
    float weight;
    int i;

    weight = inventory_weight(&g.player->pack);
    for (i = 0; i < g_basket.count; i++) {
        weight += g_basket.item[i].weight * (float)g_basket.item[i].count;
    }
    if (weight > rules_max_carry(g.player->sheet.attribute[ATTR_STRENGTH])
        || g.player->pack.count + g_basket.count > INVENTORY_MAX) {
        g.note = "You cannot carry any more.";
        return 0;
    }
    for (i = 0; i < g_basket.count; i++) {
        inventory_add(&g.player->pack, &g_basket.item[i]);
        shop_taken(&g.shop, g_basket_origin[i]);
    }
    inventory_clear(&g_basket);
    return 1;
}

/* The column's action: pay for the basket, or sell, or repair what is on the counter. */
static void trade_act(void)
{
    int price;
    int i;

    price = trade_price();
    if (!price) {
        return;
    }
    if (g.trade == TRADE_SELL) {
        g.player->pack.gold += (df_u32)price;
        inventory_clear(&g_remote);
    } else {
        if (g.shop.closed) {
            g.note = "The shop is closed.";
            return;
        }
        if (price > (int)g.player->pack.gold) {
            g.note = "You do not have enough gold.";
            return;
        }
        if (g.trade == TRADE_BUY) {
            if (!take_basket()) {
                return;
            }
        } else {
            for (i = g_remote.count - 1; i >= 0; i--) {
                g_remote.item[i].condition = g_remote.item[i].condition_max;
                inventory_add(&g.player->pack, &g_remote.item[i]);
            }
            inventory_clear(&g_remote);
        }
        g.player->pack.gold -= (df_u32)price;
    }
    character_tally(&g.player->sheet, SKILL_MERCANTILE, 1);
    wear_changed();
}

/* The Steal button: away with the basket, or seen. Either way the screen closes. */
static int trade_steal(void)
{
    float weight;
    int i;

    if (!g_basket.count) {
        return BAG_STAY;
    }
    weight = 0.0f;
    for (i = 0; i < g_basket.count; i++) {
        weight += g_basket.item[i].weight * (float)g_basket.item[i].count;
    }
    character_tally(&g.player->sheet, SKILL_PICKPOCKET, 1);
    if (combat_roll(100) < shop_seen_chance(&g.shop, &g.player->sheet, weight, g_basket.count)) {
        g.stole = 1;
        return BAG_LEAVE;
    }
    take_basket();
    return BAG_LEAVE;
}

static int click_trade(int x, int y)
{
    int list;
    int i;

    if (rect_has(&g_exit_rect, x, y)) {
        return BAG_LEAVE;
    }
    if (rect_has(&g_trade_info, x, y)) {
        g.mode = BUTTON_INFO;
        return BAG_STAY;
    }
    if (rect_has(&g_trade_select, x, y)) {
        g.mode = BUTTON_EQUIP;
        return BAG_STAY;
    }
    if (rect_has(&g_trade_wagon, x, y)) {
        g.note = "You have no wagon.";
        return BAG_STAY;
    }
    if (rect_has(&g_trade_action, x, y)) {
        trade_act();
        return BAG_STAY;
    }
    if (rect_has(&g_trade_clear, x, y)) {
        /* Everything back where it came from. */
        if (g.trade == TRADE_BUY) {
            while (g_basket.count) {
                from_basket(g_basket.count - 1);
            }
        } else {
            for (i = 0; i < g_remote.count; i++) {
                inventory_add(&g.player->pack, &g_remote.item[i]);
            }
            inventory_clear(&g_remote);
        }
        return BAG_STAY;
    }
    if (g.trade == TRADE_BUY && rect_has(&g_trade_steal, x, y)) {
        return trade_steal();
    }
    if (click_arrows(x, y)) {
        return BAG_STAY;
    }
    list = list_at(x, y, &i);
    if (i < 0) {
        return BAG_STAY;
    }
    if (g.mode == BUTTON_INFO) {
        show_info(list == LIST_LOCAL ? &local_inventory()->item[i] : &g_remote.item[i]);
    } else if (g.trade == TRADE_BUY) {
        if (list == LIST_REMOTE) {
            to_basket(i);
        } else {
            from_basket(i);
        }
    } else if (list == LIST_LOCAL) {
        to_counter(i);
    } else {
        inventory_add(&g.player->pack, &g_remote.item[i]);
        inventory_remove(&g_remote, i);
    }
    return BAG_STAY;
}

int bag_stole(void)
{
    return g.stole;
}

int bag_click(int x, int y)
{
    int list;
    int i;

    g.note = 0;
    g.changed = 1;
    /* The question takes the click that puts it away. */
    if (g.asking_gold) {
        g.asking_gold = 0;
        return BAG_STAY;
    }
    /* An info box shown takes the click that puts it away. */
    if (g.info_shown) {
        g.info_shown = 0;
        return BAG_STAY;
    }
    if (g.trade) {
        return click_trade(x, y);
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
