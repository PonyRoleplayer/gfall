#include "game/dagger/play/service.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/lodging.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See service.h. */

/* Where the pictures' own headers put them, and DFU's popup windows' buttons inside. */
#define PANEL_W 130
#define PANEL_X 95
#define SELL_Y 56
#define SELL_H 42
#define REPAIR_Y 47
#define REPAIR_H 51
#define ROW_H 7
#define ROW_W 120
#define ROW_X 5
#define EXIT_X 44
#define EXIT_W 43
#define EXIT_H 15
/* The tavern's panel (DaggerfallTavernWindow): 130 x 44 at y 50, four rows of 7 from y 5, 9 apart. */
#define TAVERN_Y 50
#define TAVERN_H 44
#define TAVERN_ROW_Y 5
#define TAVERN_ROW_GAP 9
/* Its questions: a scroll under the panel; the menu a line a thing. */
static const Rect g_tavern_scroll = { 95, 98, 130, 100 };
#define FOOD_LINE 8
#define DAYS_TEXT 4
/* The guild's panel (DaggerfallGuildServicePopupWindow): 130 x 51 at y 50, rows of 7 from y 5, 9 apart, Exit below. */
#define GUILD_Y 50
#define GUILD_H 51
#define LIST_MOST 12

enum {
    MODE_MENU,
    MODE_DAYS,
    MODE_FOOD,
    MODE_LIST
};

static struct {
    int open;
    int panel;
    Composer composer;
    const df_u8 *picture;
    int changed;
    /* A tavern's: the menu, the days asked, or the food listed; the days typed and the thing chosen. */
    int mode;
    char typed[DAYS_TEXT];
    int food;
    /* A guild's service row, and a list shown over the panel and the line picked. */
    char label[24];
    const char *const *list;
    int listed;
    int picked;
} g;

int service_open(int panel)
{
    service_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.panel = panel;
    g.picture = composer_read(&g.composer, panel == SERVICE_PANEL_REPAIR ? "REPR01I0.IMG"
                                           : panel == SERVICE_PANEL_TAVERN ? "TVRN00I0.IMG"
                                           : panel == SERVICE_PANEL_GUILD ? "GILD00I0.IMG"
                                           : panel == SERVICE_PANEL_GUILD_MEMBER ? "GILD01I0.IMG" : "GNRC01I0.IMG");
    if (!g.picture) {
        log_text(LOG_WARN, "service: its picture did not read");
        service_close();
        return 0;
    }
    g.changed = 1;
    return 1;
}

void service_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

void service_compose(void)
{
    static const Rect whole = { 0, 0, CANVAS_W, CANVAS_H };

    canvas_use(&g.composer);
    /* As rest does: classic's panels stand on black here, the world not sent to the card under them. */
    canvas_box(&whole, 0);
    if (g.panel == SERVICE_PANEL_GUILD || g.panel == SERVICE_PANEL_GUILD_MEMBER) {
        Rect row;

        canvas_copy_part(g.picture, PANEL_W, 0, 0, PANEL_W, GUILD_H, PANEL_X, GUILD_Y);
        /* The third row is bare: the person's service is written on it. */
        row.x = PANEL_X + ROW_X;
        row.y = GUILD_Y + TAVERN_ROW_Y + 2 * TAVERN_ROW_GAP;
        row.w = ROW_W;
        row.h = ROW_H;
        canvas_label_centred(&row, 0, g.label);
        if (g.mode == MODE_LIST) {
            int i;

            canvas_scroll(&g_tavern_scroll);
            for (i = 0; i < g.listed; i++) {
                canvas_label(g_tavern_scroll.x + 10, g_tavern_scroll.y + 6 + i * FOOD_LINE, g.list[i]);
            }
        }
    } else if (g.panel == SERVICE_PANEL_TAVERN) {
        canvas_copy_part(g.picture, PANEL_W, 0, 0, PANEL_W, TAVERN_H, PANEL_X, TAVERN_Y);
        if (g.mode == MODE_DAYS) {
            char line[32];

            canvas_scroll(&g_tavern_scroll);
            log_format(line, (int)sizeof(line), "How many days? %s_", g.typed);
            canvas_label(g_tavern_scroll.x + 10, g_tavern_scroll.y + 12, line);
        } else if (g.mode == MODE_FOOD) {
            int i;

            canvas_scroll(&g_tavern_scroll);
            for (i = 0; i < LODGING_FOODS; i++) {
                char line[24];

                log_format(line, (int)sizeof(line), "%s", lodging_food_name(i));
                canvas_label(g_tavern_scroll.x + 10, g_tavern_scroll.y + 6 + i * FOOD_LINE, line);
                log_format(line, (int)sizeof(line), "%d", lodging_food_price(i));
                canvas_label(g_tavern_scroll.x + 100, g_tavern_scroll.y + 6 + i * FOOD_LINE, line);
            }
        }
    } else if (g.panel == SERVICE_PANEL_REPAIR) {
        canvas_copy_part(g.picture, PANEL_W, 0, 0, PANEL_W, REPAIR_H, PANEL_X, REPAIR_Y);
    } else {
        Rect row;

        canvas_copy_part(g.picture, PANEL_W, 0, 0, PANEL_W, SELL_H, PANEL_X, SELL_Y);
        /* The picture leaves its second row bare: the program names it. */
        row.x = PANEL_X + ROW_X;
        row.y = SELL_Y + 14;
        row.w = ROW_W;
        row.h = ROW_H;
        canvas_label_centred(&row, 0, g.panel == SERVICE_PANEL_BANK ? "BANKING" : (g.panel == SERVICE_PANEL_CURE ? "CURE DISEASE" : "SELL"));
    }
    g.changed = 0;
}

const unsigned char *service_pixels(void)
{
    return g.composer.screen;
}

int service_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

/* 1 when x, y falls in the panel's row at down, down from y0. */
static int in_row(int x, int y, int y0, int down)
{
    Rect r;

    r.x = PANEL_X + ROW_X;
    r.y = (df_u16)(y0 + down);
    r.w = ROW_W;
    r.h = ROW_H;
    return rect_has(&r, x, y);
}

/* The days typed as a number. */
static int typed_days(void)
{
    int n;
    int i;

    n = 0;
    for (i = 0; g.typed[i]; i++) {
        n = n * 10 + (g.typed[i] - '0');
    }
    return n;
}

int service_days(void)
{
    return typed_days();
}

int service_food(void)
{
    return g.food;
}

int service_key(int vk)
{
    int n;

    if (g.panel != SERVICE_PANEL_TAVERN || g.mode != MODE_DAYS) {
        return SERVICE_STAY;
    }
    g.changed = 1;
    for (n = 0; g.typed[n]; n++) {
    }
    if (vk >= '0' && vk <= '9' && n < DAYS_TEXT - 1) {
        g.typed[n] = (char)vk;
        g.typed[n + 1] = 0;
    } else if (vk == 0x08 && n > 0) {
        g.typed[n - 1] = 0;
    } else if (vk == 0x0D) {
        g.mode = MODE_MENU;
        return typed_days() > 0 ? SERVICE_ROOM : SERVICE_STAY;
    }
    return SERVICE_STAY;
}

/* A tavern's click: a row of its menu, a line of the food, or anywhere else back to the rows. */
static int tavern_click(int x, int y)
{
    Rect row;
    int i;

    g.changed = 1;
    if (g.mode == MODE_FOOD) {
        i = (y - (g_tavern_scroll.y + 6)) / FOOD_LINE;
        g.mode = MODE_MENU;
        if (rect_has(&g_tavern_scroll, x, y) && y >= g_tavern_scroll.y + 6 && i >= 0 && i < LODGING_FOODS) {
            g.food = i;
            return SERVICE_FOOD;
        }
        return SERVICE_STAY;
    }
    if (g.mode == MODE_DAYS) {
        g.mode = MODE_MENU;
        return SERVICE_STAY;
    }
    row.x = PANEL_X + ROW_X;
    row.w = ROW_W;
    row.h = ROW_H;
    for (i = 0; i < 4; i++) {
        row.y = (df_u16)(TAVERN_Y + TAVERN_ROW_Y + i * TAVERN_ROW_GAP);
        if (!rect_has(&row, x, y)) {
            continue;
        }
        switch (i) {
        case 0:
            /* DFU's box starts at one day. */
            g.mode = MODE_DAYS;
            g.typed[0] = '1';
            g.typed[1] = 0;
            return SERVICE_STAY;
        case 1:
            return SERVICE_TALK;
        case 2:
            g.mode = MODE_FOOD;
            return SERVICE_STAY;
        default:
            return SERVICE_LEAVE;
        }
    }
    return SERVICE_STAY;
}

void service_set_label(const char *label)
{
    df_copy(g.label, label, (int)sizeof(g.label));
    g.changed = 1;
}

void service_list(const char *const *what, int n)
{
    g.list = what;
    g.listed = n > LIST_MOST ? LIST_MOST : n;
    g.mode = MODE_LIST;
    g.changed = 1;
}

int service_picked(void)
{
    return g.picked;
}

/* A guild's click: Join Guild (not shown to a member), Talk, the service, Exit; or a line of a list. */
static int guild_click(int x, int y)
{
    Rect row;
    Rect exit;
    int i;

    g.changed = 1;
    if (g.mode == MODE_LIST) {
        i = (y - (g_tavern_scroll.y + 6)) / FOOD_LINE;
        g.mode = MODE_MENU;
        if (rect_has(&g_tavern_scroll, x, y) && y >= g_tavern_scroll.y + 6 && i >= 0 && i < g.listed) {
            g.picked = i;
            return SERVICE_PICKED;
        }
        return SERVICE_STAY;
    }
    row.x = PANEL_X + ROW_X;
    row.w = ROW_W;
    row.h = ROW_H;
    for (i = 0; i < 3; i++) {
        row.y = (df_u16)(GUILD_Y + TAVERN_ROW_Y + i * TAVERN_ROW_GAP);
        if (rect_has(&row, x, y)) {
            if (i == 0) {
                return g.panel == SERVICE_PANEL_GUILD ? SERVICE_JOIN : SERVICE_STAY;
            }
            return i == 1 ? SERVICE_TALK : SERVICE_GUILD;
        }
    }
    exit.x = PANEL_X + EXIT_X;
    exit.y = GUILD_Y + 33;
    exit.w = EXIT_W;
    exit.h = EXIT_H;
    return rect_has(&exit, x, y) ? SERVICE_LEAVE : SERVICE_STAY;
}

int service_click(int x, int y)
{
    Rect exit;

    if (g.panel == SERVICE_PANEL_TAVERN) {
        return tavern_click(x, y);
    }
    if (g.panel == SERVICE_PANEL_GUILD || g.panel == SERVICE_PANEL_GUILD_MEMBER) {
        return guild_click(x, y);
    }
    exit.x = PANEL_X + EXIT_X;
    exit.w = EXIT_W;
    exit.h = EXIT_H;
    if (g.panel == SERVICE_PANEL_REPAIR) {
        exit.y = REPAIR_Y + 33;
        if (rect_has(&exit, x, y)) {
            return SERVICE_LEAVE;
        }
        if (in_row(x, y, REPAIR_Y, 5)) {
            return SERVICE_REPAIR;
        }
        if (in_row(x, y, REPAIR_Y, 14)) {
            return SERVICE_TALK;
        }
        return in_row(x, y, REPAIR_Y, 23) ? SERVICE_SELL : SERVICE_STAY;
    }
    exit.y = SELL_Y + 24;
    if (rect_has(&exit, x, y)) {
        return SERVICE_LEAVE;
    }
    if (in_row(x, y, SELL_Y, 5)) {
        return SERVICE_TALK;
    }
    if (in_row(x, y, SELL_Y, 14)) {
        return g.panel == SERVICE_PANEL_BANK ? SERVICE_BANK : (g.panel == SERVICE_PANEL_CURE ? SERVICE_CURE : SERVICE_SELL);
    }
    return SERVICE_STAY;
}
