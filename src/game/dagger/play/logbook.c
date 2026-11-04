#include "game/dagger/play/logbook.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/quest.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See logbook.h. */

#define TEXT_MOST 3072
#define BOX_W 296
#define BOX_X ((CANVAS_W - BOX_W) / 2)
#define BOX_Y 10
#define BOX_H 180
#define MARGIN 12
/* The bottom edge that closes it. */
#define EDGE_H 14

static struct {
    int open;
    Composer composer;
    int slots[QUEST_SLOTS];
    int count;
    int page;
    int now;
    char text[TEXT_MOST];
    int changed;
} g;

static const Rect g_box = { BOX_X, BOX_Y, BOX_W, BOX_H };

/* The page's words: the quest's lines and its time left. */
static void build_text(void)
{
    char line[512];
    int i;
    int at;
    int slot;
    int left;

    g.text[0] = 0;
    if (!g.count) {
        df_copy(g.text, "You have no quests.", TEXT_MOST);
        return;
    }
    slot = g.slots[g.page];
    at = 0;
    for (i = 0; i < QUEST_LOG; i++) {
        int k;

        if (!quest_log_line(slot, i, line, (int)sizeof(line))) {
            continue;
        }
        for (k = 0; line[k] && at < TEXT_MOST - 4; k++) {
            g.text[at++] = line[k] == '\n' ? ' ' : line[k];
        }
        g.text[at++] = '\n';
        g.text[at++] = '\n';
    }
    g.text[at] = 0;
    left = quest_minutes_left(slot, g.now);
    if (left >= 0) {
        log_format(line, (int)sizeof(line), "%d days left.", (left + 1439) / 1440);
        df_copy(g.text + at, line, TEXT_MOST - at);
    }
}

int logbook_open(int now)
{
    int slot;

    logbook_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.now = now;
    for (slot = 0; slot < QUEST_SLOTS; slot++) {
        if (quest_run(slot)) {
            g.slots[g.count++] = slot;
        }
    }
    build_text();
    g.changed = 1;
    return 1;
}

void logbook_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

int logbook_pages(void)
{
    return g.count;
}

int logbook_page(void)
{
    return g.page;
}

void logbook_compose(void)
{
    static const Rect whole = { 0, 0, CANVAS_W, CANVAS_H };
    Rect inside;
    char head[48];

    canvas_use(&g.composer);
    canvas_box(&whole, 0);
    canvas_scroll(&g_box);
    inside.x = BOX_X + MARGIN;
    inside.y = BOX_Y + MARGIN + CANVAS_LINE + 4;
    inside.w = BOX_W - 2 * MARGIN;
    inside.h = BOX_H - 2 * MARGIN - CANVAS_LINE - 4 - EDGE_H;
    log_format(head, (int)sizeof(head), "%s %d / %d", g.count ? quest_file(g.slots[g.page]) : "Logbook", g.count ? g.page + 1 : 0, g.count);
    canvas_label(BOX_X + MARGIN, BOX_Y + MARGIN, head);
    canvas_text(&inside, g.text);
    g.changed = 0;
}

const unsigned char *logbook_pixels(void)
{
    return g.composer.screen;
}

int logbook_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

static void turn(int by)
{
    if (!g.count) {
        return;
    }
    g.page = (g.page + by + g.count) % g.count;
    build_text();
    g.changed = 1;
}

int logbook_click(int x, int y)
{
    if (y >= BOX_Y + BOX_H - EDGE_H) {
        return LOGBOOK_LEAVE;
    }
    turn(x < CANVAS_W / 2 ? -1 : 1);
    return LOGBOOK_STAY;
}

int logbook_key(int vk)
{
    if (vk == 0x1B) {
        return LOGBOOK_LEAVE;
    }
    if (vk == 0x27 || vk == 0x22) {
        turn(1);
    } else if (vk == 0x25 || vk == 0x21) {
        turn(-1);
    }
    return LOGBOOK_STAY;
}
