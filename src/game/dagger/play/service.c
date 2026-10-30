#include "game/dagger/play/service.h"
#include "game/dagger/play/canvas.h"
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

static struct {
    int open;
    int panel;
    Composer composer;
    const df_u8 *picture;
    int changed;
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
    g.picture = composer_read(&g.composer, panel == SERVICE_PANEL_REPAIR ? "REPR01I0.IMG" : "GNRC01I0.IMG");
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
    if (g.panel == SERVICE_PANEL_REPAIR) {
        canvas_copy_part(g.picture, PANEL_W, 0, 0, PANEL_W, REPAIR_H, PANEL_X, REPAIR_Y);
    } else {
        Rect row;

        canvas_copy_part(g.picture, PANEL_W, 0, 0, PANEL_W, SELL_H, PANEL_X, SELL_Y);
        /* The picture leaves its second row bare: the program names it. */
        row.x = PANEL_X + ROW_X;
        row.y = SELL_Y + 14;
        row.w = ROW_W;
        row.h = ROW_H;
        canvas_label_centred(&row, 0, g.panel == SERVICE_PANEL_BANK ? "BANKING" : "SELL");
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

int service_click(int x, int y)
{
    Rect exit;

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
        return g.panel == SERVICE_PANEL_BANK ? SERVICE_BANK : SERVICE_SELL;
    }
    return SERVICE_STAY;
}
