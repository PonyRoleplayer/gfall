#include "game/dagger/play/slots.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/save.h"
#include "engine/core/log.h"

/* See slots.h. The rectangles are DFU's load window's. */

static const Rect g_slot_rect[SAVE_SLOTS] = {
    { 1, 56, 158, 9 }, { 1, 121, 158, 9 }, { 1, 186, 158, 9 },
    { 162, 56, 158, 9 }, { 162, 121, 158, 9 }, { 162, 186, 158, 9 }
};
static const Rect g_cancel = { 133, 150, 56, 19 };
/* SAVE00I0.IMG's title, where LOAD00I0.IMG has its own. */
#define TITLE_X 126
#define TITLE_Y 5
#define TITLE_W 68
#define TITLE_H 11

static struct {
    int open;
    Composer composer;
    const df_u8 *back;
    const df_u8 *save_title;
    int saving;
    int has_save[SAVE_SLOTS];
    char name[SAVE_SLOTS][SAVE_NAME];
    /* The slot being named, or -1. */
    int typing;
    int chosen;
    int changed;
    const char *note;
} g;

int slots_open(int saving)
{
    int i;

    slots_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.saving = saving;
    g.typing = -1;
    g.back = composer_read(&g.composer, "LOAD00I0.IMG");
    g.save_title = composer_read(&g.composer, "SAVE00I0.IMG");
    if (!g.back || !g.save_title) {
        log_text(LOG_WARN, "saves: their pictures did not read");
        slots_close();
        return 0;
    }
    for (i = 0; i < SAVE_SLOTS; i++) {
        g.has_save[i] = save_peek_name(i, g.name[i]);
    }
    return 1;
}

void slots_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

void slots_compose(void)
{
    int i;

    canvas_use(&g.composer);
    canvas_copy_all(g.back);
    if (g.saving) {
        canvas_copy_part(g.save_title, TITLE_W, 0, 0, TITLE_W, TITLE_H, TITLE_X, TITLE_Y);
    }
    for (i = 0; i < SAVE_SLOTS; i++) {
        char line[SAVE_NAME + 2];

        if (i == g.typing) {
            log_format(line, (int)sizeof(line), "%s_", g.name[i]);
            canvas_label(g_slot_rect[i].x + 2, g_slot_rect[i].y + 1, line);
        } else if (g.has_save[i]) {
            canvas_label(g_slot_rect[i].x + 2, g_slot_rect[i].y + 1, g.name[i]);
        }
    }
    g.changed = 0;
}

const unsigned char *slots_pixels(void)
{
    return g.composer.screen;
}

int slots_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

const char *slots_note(void)
{
    return g.note;
}

int slots_chosen(void)
{
    return g.chosen;
}

const char *slots_chosen_name(void)
{
    return g.name[g.chosen];
}

int slots_click(int x, int y)
{
    int i;

    g.note = 0;
    g.changed = 1;
    if (rect_has(&g_cancel, x, y)) {
        return SLOTS_LEAVE;
    }
    for (i = 0; i < SAVE_SLOTS; i++) {
        if (!rect_has(&g_slot_rect[i], x, y)) {
            continue;
        }
        if (g.saving) {
            g.typing = i;
        } else if (g.has_save[i]) {
            g.chosen = i;
            return SLOTS_CHOSEN;
        } else {
            g.note = "There is no saved game there.";
        }
    }
    return SLOTS_STAY;
}

int slots_key(int vk)
{
    int n;

    if (vk == 0x1B) {
        if (g.typing < 0) {
            return SLOTS_LEAVE;
        }
        g.typing = -1;
        g.changed = 1;
        return SLOTS_STAY;
    }
    if (g.typing < 0) {
        return SLOTS_STAY;
    }
    n = 0;
    while (g.name[g.typing][n]) {
        n++;
    }
    g.changed = 1;
    if (vk == 0x0D) {
        if (!n) {
            g.note = "A saved game needs a name.";
            return SLOTS_STAY;
        }
        g.chosen = g.typing;
        return SLOTS_CHOSEN;
    }
    if (vk == 0x08) {
        if (n > 0) {
            g.name[g.typing][n - 1] = 0;
        }
    } else if (n < SAVE_NAME - 1 && ((vk >= '0' && vk <= '9') || vk == 0x20 || (vk >= 'A' && vk <= 'Z'))) {
        int starts_word;

        starts_word = n == 0 || g.name[g.typing][n - 1] == ' ';
        g.name[g.typing][n] = (char)(vk >= 'A' && vk <= 'Z' && !starts_word ? vk + ('a' - 'A') : vk);
        g.name[g.typing][n + 1] = 0;
    }
    return SLOTS_STAY;
}
