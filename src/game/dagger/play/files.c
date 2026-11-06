#include "game/dagger/play/files.h"
#include "game/dagger/play/ui.h"
#include "engine/core/log.h"
#include "engine/scene/clock.h"

/* See files.h and docs/game.md (Saves, The list). */

/* Where things stand in the shell's body, 640 x 480 top-down: the snapshot's frame and picture, the details, the list and the buttons. */
#define THUMB_FRAME_X 176
#define THUMB_FRAME_Y 84
#define THUMB_X 182
#define THUMB_Y 90
#define DETAIL_X 178
#define DETAIL_W 168
#define LIST_X 360
#define LIST_Y 88
#define LIST_W 228
#define LIST_ROWS 12
#define ROW_H 24
#define BUTTON_Y 384
#define BUTTON_W 108
#define BUTTON_H 34
/* A second click on the same row within this is a double click. */
#define DOUBLE_MS 450

/* A row of the list: a character, or one of their saves. */
typedef struct Row {
    short kind;
    short character;
    short entry;
} Row;
enum { ROW_CHARACTER, ROW_SAVE };

static Row g_row[SAVES_CHARS + SAVES_MAX];
static int g_nrow;
static int g_sel = -1;
static UiList g_list;
static int g_can_save;
static int g_confirm;
static const char *g_note;
static char g_note_text[96];
static char g_chosen[SAVE_PATH];
static int g_last_id = -1;
static df_u32 g_last_now;

static df_u16 g_thumb[SAVE_THUMB_W * SAVE_THUMB_H];
static int g_thumb_ok;
static df_u32 g_thumb_serial;
static char g_thumb_path[SAVE_PATH];

void files_say(const char *line)
{
    int n;

    if (!line) {
        g_note = 0;
        return;
    }
    for (n = 0; line[n] && n < (int)sizeof(g_note_text) - 1; n++) {
        g_note_text[n] = line[n];
    }
    g_note_text[n] = 0;
    g_note = g_note_text;
}

const char *files_note(void)
{
    return g_note;
}

const char *files_chosen(void)
{
    return g_chosen;
}

const df_u16 *files_thumb(void)
{
    return g_thumb_ok ? g_thumb : 0;
}

df_u32 files_thumb_serial(void)
{
    return g_thumb_serial;
}

void files_thumb_rect(int *x, int *y, int *w, int *h)
{
    *x = THUMB_X;
    *y = THUMB_Y;
    *w = SAVE_THUMB_W;
    *h = SAVE_THUMB_H;
}

int files_rows(void)
{
    return g_nrow;
}

int files_selected_row(void)
{
    return g_sel;
}

int files_row_is_character(int row)
{
    return row >= 0 && row < g_nrow && g_row[row].kind == ROW_CHARACTER;
}

int files_confirming(void)
{
    return g_confirm;
}

/* ---- The list ---- */

/* The save a row stands for: its own, or a character's newest; -1 for none. */
static int entry_of(int row)
{
    if (row < 0 || row >= g_nrow) {
        return -1;
    }
    return g_row[row].kind == ROW_SAVE ? g_row[row].entry : saves_char(g_row[row].character)->first;
}

static void load_thumb(void)
{
    const SaveEntry *e;
    int entry;

    entry = entry_of(g_sel);
    e = saves_entry(entry);
    if (!e) {
        if (g_thumb_ok || g_thumb_path[0]) {
            g_thumb_ok = 0;
            g_thumb_path[0] = 0;
            g_thumb_serial++;
        }
        return;
    }
    /* The same save as the picture already read: nothing to do. */
    {
        int same;
        int i;

        same = 1;
        for (i = 0; e->path[i] || g_thumb_path[i]; i++) {
            same = same && e->path[i] == g_thumb_path[i];
        }
        if (same) {
            return;
        }
    }
    {
        int i;

        for (i = 0; e->path[i] && i < SAVE_PATH - 1; i++) {
            g_thumb_path[i] = e->path[i];
        }
        g_thumb_path[i] = 0;
    }
    g_thumb_ok = e->meta.has_thumb && save_peek_thumb(e->path, g_thumb);
    g_thumb_serial++;
}

static void select_row(int row)
{
    g_sel = row >= 0 && row < g_nrow ? row : -1;
    g_confirm = 0;
    if (g_sel >= 0) {
        ui_list_show(&g_list, g_sel);
    }
    load_thumb();
}

/* The rows from the saves just scanned, and the list's own numbers. */
static void build_rows(void)
{
    int c;
    int i;

    g_nrow = 0;
    for (c = 0; c < saves_char_count(); c++) {
        const SaveChar *ch;

        ch = saves_char(c);
        g_row[g_nrow].kind = ROW_CHARACTER;
        g_row[g_nrow].character = (short)c;
        g_row[g_nrow].entry = (short)ch->first;
        g_nrow++;
        for (i = ch->first; i < ch->first + ch->count; i++) {
            g_row[g_nrow].kind = ROW_SAVE;
            g_row[g_nrow].character = (short)c;
            g_row[g_nrow].entry = (short)i;
            g_nrow++;
        }
    }
    g_list.x = LIST_X;
    g_list.y = LIST_Y;
    g_list.w = LIST_W;
    g_list.rows = LIST_ROWS;
    g_list.row_h = ROW_H;
    g_list.count = g_nrow;
    ui_list_move(&g_list, 0);
}

/* The row of the save at this path, or -1. */
static int row_of_path(const char *path)
{
    int r;

    for (r = 0; path && r < g_nrow; r++) {
        const SaveEntry *e;
        int i;
        int same;

        if (g_row[r].kind != ROW_SAVE) {
            continue;
        }
        e = saves_entry(g_row[r].entry);
        same = 1;
        for (i = 0; e->path[i] || path[i]; i++) {
            same = same && e->path[i] == path[i];
        }
        if (same) {
            return r;
        }
    }
    return -1;
}

void files_open(int can_save, df_u32 current, const char *select)
{
    int row;
    int c;

    g_can_save = can_save;
    g_confirm = 0;
    g_note = 0;
    g_last_id = -1;
    g_thumb_path[0] = 0;
    g_thumb_ok = 0;
    g_thumb_serial++;
    saves_scan();
    g_list.scroll = 0;
    build_rows();
    row = row_of_path(select);
    for (c = 0; row < 0 && current && c < saves_char_count(); c++) {
        int r;

        if (saves_char(c)->id != current) {
            continue;
        }
        for (r = 0; r < g_nrow; r++) {
            if (g_row[r].kind == ROW_SAVE && g_row[r].character == c) {
                row = r;
                break;
            }
        }
    }
    if (row < 0 && g_nrow > 1) {
        row = 1;
    }
    select_row(row);
}

/* After a change to the saves: the list again, a row near row selected. */
static void rebuild(int near_row, const char *select)
{
    int row;

    saves_scan();
    build_rows();
    row = row_of_path(select);
    if (row < 0) {
        row = near_row >= g_nrow ? g_nrow - 1 : near_row;
        if (row < 0) {
            row = -1;
        }
    }
    select_row(row);
}

/* ---- Drawing ---- */

static void date_line(const SaveMeta *m, char *out, int cap)
{
    log_format(out, cap, "%04d-%02d-%02d %02d:%02d", m->year, m->month, m->day, m->hour, m->minute);
}

void files_draw(int hover)
{
    char line[64];
    int i;
    int row;
    int sel_entry;
    const SaveEntry *e;

    /* The snapshot's frame, with a word where there is none. */
    ui_panel(THUMB_FRAME_X, THUMB_FRAME_Y, SAVE_THUMB_W + 12, SAVE_THUMB_H + 12, 0);
    if (!g_thumb_ok) {
        ui_text(THUMB_X + 18, THUMB_Y + 50, 130, ui_tr(g_sel >= 0 ? "NO SNAPSHOT" : "NO SAVED GAME"), 1);
    }
    ui_text(LIST_X, 64, LIST_W, ui_tr("CHARACTERS AND SAVES"), 0);

    sel_entry = entry_of(g_sel);
    e = saves_entry(sel_entry);
    if (e) {
        ui_text(DETAIL_X, 226, DETAIL_W, e->meta.name, 0);
        log_format(line, (int)sizeof(line), "Level %d", e->meta.level);
        ui_text(DETAIL_X, 246, DETAIL_W, line, 1);
        ui_text(DETAIL_X, 260, DETAIL_W, e->meta.where[0] ? e->meta.where : "", 1);
        log_format(line, (int)sizeof(line), "Saved %04d-%02d-%02d %02d:%02d", e->meta.year, e->meta.month, e->meta.day,
                   e->meta.hour, e->meta.minute);
        ui_text(DETAIL_X, 274, DETAIL_W, line, 1);
        {
            int year;
            int month;
            int day;

            clock_date_of(e->meta.game_day, &year, &month, &day);
            log_format(line, (int)sizeof(line), "Game: %d %s %d", day + 1, clock_month_name(month), year);
            ui_text(DETAIL_X, 288, DETAIL_W, line, 1);
            log_format(line, (int)sizeof(line), "at %02d:%02d", e->meta.game_minute / 60 % 24, e->meta.game_minute % 60);
            ui_text(DETAIL_X, 301, DETAIL_W, line, 1);
        }
    }
    if (g_note) {
        ui_paragraph(DETAIL_X, 322, DETAIL_W, 56, g_note);
    }

    /* The list. */
    if (g_nrow == 0) {
        ui_paragraph(LIST_X, 100, LIST_W, 80, "No saved games yet. Save from the game and they will be listed here.");
    }
    for (i = 0; i < LIST_ROWS; i++) {
        int y;
        int top;

        row = g_list.scroll + i;
        if (row >= g_nrow) {
            break;
        }
        y = LIST_Y + i * ROW_H;
        if (g_row[row].kind == ROW_CHARACTER) {
            const SaveChar *ch;

            ch = saves_char(g_row[row].character);
            ui_panel(LIST_X, y, LIST_W, ROW_H, 1);
            ui_text(LIST_X + 8, y + 5, LIST_W - 50, ch->name, 0);
            log_format(line, (int)sizeof(line), "%d", ch->count);
            ui_text(LIST_X + LIST_W - 34, y + 8, 28, line, 1);
        } else {
            const SaveEntry *s;

            s = saves_entry(g_row[row].entry);
            ui_panel(LIST_X + 14, y, LIST_W - 14, ROW_H, 0);
            date_line(&s->meta, line, (int)sizeof(line));
            ui_text(LIST_X + 22, y + 8, LIST_W - 60, line, 1);
            log_format(line, (int)sizeof(line), "L%d", s->meta.level);
            ui_text(LIST_X + LIST_W - 40, y + 8, 34, line, 1);
        }
        top = y;
        if (row == g_sel) {
            ui_flat(LIST_X + 2, top + 2, LIST_W - 4, 2, 4);
            ui_flat(LIST_X + 2, top + ROW_H - 4, LIST_W - 4, 2, 4);
        } else if (hover == FILES_ID_ROW + row) {
            ui_flat(LIST_X + 4, top + ROW_H - 3, LIST_W - 8, 1, 4);
        }
        ui_region(FILES_ID_ROW + row, LIST_X, y, LIST_W, ROW_H);
    }
    ui_list_bar(&g_list, FILES_ID_UP, FILES_ID_DOWN, hover == FILES_ID_UP, hover == FILES_ID_DOWN);

    /* The buttons: only those that mean something now answer. */
    ui_button(176, BUTTON_Y, BUTTON_W, BUTTON_H, "LOAD", 0, hover == FILES_ID_LOAD);
    if (g_sel >= 0) {
        ui_region(FILES_ID_LOAD, 176, BUTTON_Y, BUTTON_W, BUTTON_H);
    }
    if (g_can_save) {
        ui_button(290, BUTTON_Y, BUTTON_W, BUTTON_H, "SAVE", 2, hover == FILES_ID_SAVE);
        ui_region(FILES_ID_SAVE, 290, BUTTON_Y, BUTTON_W, BUTTON_H);
    }
    ui_button(404, BUTTON_Y, BUTTON_W + 40, BUTTON_H,
              g_confirm ? (g_sel >= 0 && g_row[g_sel].kind == ROW_CHARACTER ? "DELETE ALL?" : "DELETE?") : "DELETE", 1,
              hover == FILES_ID_DELETE);
    if (g_sel >= 0) {
        ui_region(FILES_ID_DELETE, 404, BUTTON_Y, BUTTON_W + 40, BUTTON_H);
    }
}

/* ---- Doing ---- */

static int choose_load(void)
{
    const SaveEntry *e;
    int i;

    e = saves_entry(entry_of(g_sel));
    if (!e) {
        return FILES_NONE;
    }
    for (i = 0; e->path[i] && i < SAVE_PATH - 1; i++) {
        g_chosen[i] = e->path[i];
    }
    g_chosen[i] = 0;
    return FILES_LOAD;
}

/* DELETE: the first click asks, the second does it. */
static void delete_click(void)
{
    if (g_sel < 0) {
        return;
    }
    if (!g_confirm) {
        g_confirm = 1;
        if (g_row[g_sel].kind == ROW_CHARACTER) {
            files_say("Click Delete All to delete every saved game of this character.");
        } else {
            files_say("Click Delete to delete this saved game.");
        }
        return;
    }
    g_confirm = 0;
    if (g_row[g_sel].kind == ROW_CHARACTER) {
        int ok;

        ok = saves_delete_char(g_row[g_sel].character);
        rebuild(g_sel, 0);
        files_say(ok ? "The character's saved games are deleted." : "Some saved games could not be deleted.");
    } else {
        int ok;

        ok = saves_delete(g_row[g_sel].entry);
        rebuild(g_sel, 0);
        files_say(ok ? "The saved game is deleted." : "The saved game could not be deleted.");
    }
}

int files_click(int id, df_u32 now, df_u32 per_ms)
{
    int was_confirming;

    was_confirming = g_confirm;
    if (id != FILES_ID_DELETE) {
        g_confirm = 0;
        if (was_confirming) {
            files_say(0);
        }
    }
    if (id >= FILES_ID_ROW && id < FILES_ID_ROW + g_nrow) {
        int quick;
        int row;

        row = id - FILES_ID_ROW;
        quick = per_ms != 0 && g_last_id == id && (df_u32)(now - g_last_now) <= (df_u32)DOUBLE_MS * per_ms;
        g_last_id = id;
        g_last_now = now;
        files_say(0);
        select_row(row);
        return quick && g_row[row].kind == ROW_SAVE ? choose_load() : FILES_NONE;
    }
    g_last_id = -1;
    switch (id) {
    case FILES_ID_LOAD:
        return choose_load();
    case FILES_ID_SAVE:
        return g_can_save ? FILES_SAVE : FILES_NONE;
    case FILES_ID_DELETE:
        delete_click();
        return FILES_NONE;
    case FILES_ID_UP:
        ui_list_move(&g_list, -1);
        return FILES_NONE;
    case FILES_ID_DOWN:
        ui_list_move(&g_list, 1);
        return FILES_NONE;
    default:
        return FILES_NONE;
    }
}

int files_key(int vk)
{
    if (vk == 0x26 && g_sel > 0) {
        files_say(0);
        select_row(g_sel - 1);
    } else if (vk == 0x28 && g_sel >= 0 && g_sel < g_nrow - 1) {
        files_say(0);
        select_row(g_sel + 1);
    } else if (vk == 0x0D) {
        return choose_load();
    } else if (vk == 0x2E) {
        delete_click();
    }
    return FILES_NONE;
}

void files_wheel(int notches)
{
    ui_list_move(&g_list, -notches);
}
