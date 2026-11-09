#include "game/ogfall/play/files.h"
#include "game/ogfall/play/play.h"
#include "engine/core/df.h"
#include "engine/core/mem.h"

static OgSaveInfo g_info[6];
static int g_saving, g_selected, g_editing;
static int g_last_click = -1;
static float g_since_click;
static char g_name[32];
static char g_error[128];
static unsigned g_revision, g_generation;

void og_files_open(int saving)
{
    int i;

    g_saving = saving;
    g_selected = 0;
    g_editing = 0;
    g_last_click = -1;
    g_error[0] = 0;
    for (i = 0; i < 6; i++) {
        mem_zero(&g_info[i], sizeof(g_info[i]));
        g_info[i].status = og_save_info(i + 1, &g_info[i]);
    }
    g_revision++;
    g_generation++;
}

int og_files_saving(void)
{
    return g_saving;
}

int og_files_selected(void)
{
    return g_selected;
}

int og_files_editing(void)
{
    return g_editing;
}

const char *og_files_name(void)
{
    return g_name;
}

const char *og_files_error(void)
{
    return g_error;
}

unsigned og_files_revision(void)
{
    return g_revision;
}

unsigned og_files_generation(void)
{
    return g_generation;
}

const OgSaveInfo *og_files_info(int slot)
{
    return slot >= 0 && slot < 6 ? &g_info[slot] : 0;
}

void og_files_failed(const char *message)
{
    df_copy(g_error, message, sizeof(g_error));
    g_editing = 0;
    g_revision++;
}

static int confirm(void)
{
    if (g_saving) {
        df_copy(g_name, g_info[g_selected].status == 1 ? g_info[g_selected].label : og_character()->name, sizeof(g_name));
        g_editing = 1;
        g_revision++;
        return OG_FILES_NONE;
    }
    if (!g_info[g_selected].status) {
        og_files_failed("This slot is empty.");
        return OG_FILES_NONE;
    }
    return OG_FILES_LOAD;
}

int og_files_frame(const OgIn *input)
{
    int ch, n, x, y, slot;

    ch = input->ch;
    x = input->cx;
    y = input->cy;
    g_since_click += input->dt;
    if (g_editing) {
        n = 0;
        while (g_name[n]) {
            n++;
        }
        if (ch == 27) {
            g_editing = 0;
        }
        else if (ch == 13 && n) {
            return OG_FILES_SAVE;
        }
        else if (ch == 8 && n) {
            g_name[n-1] = 0;
        }
        else if (ch >= 32 && ch <= 126 && n < 31) {
            g_name[n] = (char)ch;
            g_name[n+1] = 0;
        }
        if (input->mouse_pressed & 1) {
            if (x > OG_FILE_ACCEPT_X && x < OG_FILE_ACCEPT_X + OG_FILE_BUTTON_W &&
                y > OG_FILE_BUTTON_Y && y < OG_FILE_BUTTON_Y + OG_FILE_BUTTON_H && n) { return OG_FILES_SAVE; }
            if (x > OG_FILE_REJECT_X && x < OG_FILE_REJECT_X + OG_FILE_BUTTON_W &&
                y > OG_FILE_BUTTON_Y && y < OG_FILE_BUTTON_Y + OG_FILE_BUTTON_H) { g_editing = 0; }
        }
        if (ch || input->mouse_pressed) {
            g_revision++;
        }
        return OG_FILES_NONE;
    }
    if (ch == 27) {
        return OG_FILES_CANCEL;
    }
    if (ch == 9 || ch == 1002) {
        g_selected = (g_selected + 1) % 6;
        g_error[0] = 0;
        g_revision++;
    } else if (ch == 1001) {
        g_selected = (g_selected + 5) % 6;
        g_error[0] = 0;
        g_revision++;
    } else if (ch == 13) {
        return confirm();
    }
    if (!(input->mouse_pressed & 1) || !input->on_picture) {
        return OG_FILES_NONE;
    }
    if (x > 133 && x < 187 && y > 150 && y < 168) {
        return OG_FILES_CANCEL;
    }
    if (x > 126 && x < 193 && y > 5 && y < 15) {
        return confirm();
    }
    slot = -1;
    if (x > 5 && x < 120) {
        slot = 0;
    } else if (x > 166 && x < 317) {
        slot = 3;
    }
    if (slot >= 0 && y > 3 && y < 195) {
        slot += y / 65;
        if (slot >= 6) {
            return OG_FILES_NONE;
        }
        g_selected = slot;
        g_error[0] = 0;
        g_revision++;
        if (slot == g_last_click && g_since_click < 0.35f) {
            g_last_click = -1;
            return confirm();
        }
        g_last_click = slot;
        g_since_click = 0;
    }
    return OG_FILES_NONE;
}
