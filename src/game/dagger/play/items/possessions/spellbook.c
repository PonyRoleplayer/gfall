#include "internal.h"
#include "game/dagger/play/items/describe.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/quests/quest.h"

static void (*g_shop_buy)(int id);

int possessions_spellshop(const int *ids, int count, void (*buy)(int id))
{
    int i;

    if (possessions_view.page || count < 1 || count > 96 || !buy || !possessions_open(VIEW_SPELLSHOP, 0)) { return 0; }
    for (i = 0; i < count; i++) { possessions_view.shop[i] = ids[i]; }
    possessions_view.shop_count = count;
    possessions_view.selected = 0; possessions_view.scroll[0] = 0;
    g_shop_buy = buy;
    return 1;
}

static void spellshop_click(int x, int y)
{
    int at;
    void (*buy)(int);

    if (possessions_hit(x,y,242,170,278,178)) { possessions_close(); return; }
    if (possessions_hit(x,y,23,167,65,181)) { if (g_shop_buy) { g_shop_buy(possessions_view.shop[possessions_view.selected]); } return; }
    if (x > 161 && x < 277 && y > 58 && y < 162) {
        at = (y-58)/38;
        if ((y-58)%38 < 27) {
            spell_help_def(spell_def(possessions_view.shop[possessions_view.selected]),at,possessions_view.notice,sizeof(possessions_view.notice)); possessions_view.notice_page = 0;
        }
        return;
    }
    if (possessions_hit(x,y,144,29,152,44)) { if (possessions_view.scroll[0]) { possessions_view.scroll[0]--; } return; }
    if (possessions_hit(x,y,144,150,152,165)) { if (possessions_view.scroll[0]+14 < possessions_view.shop_count) { possessions_view.scroll[0]++; } return; }
    if (possessions_hit(x,y,27,30,138,161)) {
        at = possessions_view.scroll[0] + (y-30)/9;
        if (at < possessions_view.shop_count) { possessions_view.selected = at; }
    }
    (void)buy;
}

/* ---- The potion maker (potionmaker_update, MASK00I0.IMG) ---- */

int possessions_cast(void)
{
    if (!possessions_open(VIEW_SPELLS,0)) { return 0; }
    possessions_view.casting = 1; return 1;
}

static void spell_click(int x, int y, int right)
{
    df_u32 ids[OBJECTS];
    int count, at;
    count = spell_rows(possessions_view.book, ids, OBJECTS);
    if (!count) { possessions_close(); return; }
    if (possessions_view.selected >= count) { possessions_view.selected = count-1; }
    if (possessions_hit(x,y,242,170,278,179)) { possessions_close(); }
    else if (possessions_hit(x,y,145,19,280,25)) {
        possessions_view.prompt = PROMPT_SPELL_NAME;
        df_copy(possessions_view.input,object_at(ids[possessions_view.selected])->name,sizeof(possessions_view.input));
    } else if (possessions_hit(x,y,172,32,187,47)) { spell_cycle_icon(ids[possessions_view.selected],right ? -1 : 1); }
    else if (x > 161 && x < 277 && y > 58 && y < 162) {
        at = (y-58)/38;
        if ((y-58)%38 < 27) { spell_help(ids[possessions_view.selected],at,possessions_view.notice,sizeof(possessions_view.notice)); possessions_view.notice_page = 0; }
    } else if (possessions_hit(x,y,27,30,138,161)) {
        at = possessions_view.scroll[0] + (y-30)/9;
        if (at < count) {
            if (possessions_view.casting && possessions_view.selected == at) {
                possessions_close(); magic_ready(ids[at]);
            } else { possessions_view.selected = at; }
        }
    } else if (possessions_hit(x,y,23,167,67,182)) {
        if (spell_delete(ids[possessions_view.selected])) {
            if (count == 1) { possessions_close(); return; }
            possessions_view.selected = possessions_view.scroll[0] = 0;
        }
    } else if (possessions_hit(x,y,68,167,112,182)) {
        if (spell_move(ids[possessions_view.selected],-1)) { possessions_view.selected = possessions_view.scroll[0] = 0; }
    } else if (possessions_hit(x,y,151,167,195,182)) {
        if (spell_move(ids[possessions_view.selected],1)) { possessions_view.selected = possessions_view.scroll[0] = 0; }
    }
    if (possessions_view.selected < possessions_view.scroll[0]) { possessions_view.scroll[0] = possessions_view.selected; }
    if (possessions_view.selected >= possessions_view.scroll[0]+14) { possessions_view.scroll[0] = possessions_view.selected-13; }
}

void spell_scrollbar(int *top, int *height)
{
    df_u32 ids[OBJECTS];
    int count;
    count = spell_rows(possessions_view.book,ids,OBJECTS);
    *height = count > 14 ? 104*14/count : 104;
    if (*height < 6) { *height = 6; }
    *top = 45 + (count > 14 ? possessions_view.scroll[0]*(104-*height)/(count-14) : 0);
}

static void spell_scroll(const PlayIn *in)
{
    df_u32 ids[OBJECTS];
    int count, limit, top, height, old, step;
    if (!in->key && !(in->mouse_pressed & 3) && !(in->mouse_held & 3) && !possessions_view.drag && !possessions_view.repeat_step) { return; }
    count = spell_rows(possessions_view.book,ids,OBJECTS);
    limit = count > 14 ? count-14 : 0;
    old = possessions_view.scroll[0]; step = 0;
    spell_scrollbar(&top,&height);
    if (!(in->mouse_held & 3)) { possessions_view.drag = possessions_view.repeat_step = 0; }
    if ((in->mouse_pressed & 3) && in->on_picture && in->cx > 144 && in->cx < 153) {
        if (in->cy > 29 && in->cy < 45) { step = possessions_view.repeat_step = -1; }
        else if (in->cy > 150 && in->cy < 166) { step = possessions_view.repeat_step = 1; }
        else if (in->cy >= 45 && in->cy < 149) {
            if (in->cy >= top && in->cy < top+height) { possessions_view.drag = 1; possessions_view.drag_offset = in->cy-top; }
            else { step = in->cy < top ? -14 : 14; }
        }
        possessions_view.repeat_time = 0.35f;
    } else if (possessions_view.repeat_step && in->on_picture && in->cx > 144 && in->cx < 153 &&
               (possessions_view.repeat_step < 0 ? in->cy > 29 && in->cy < 45 : in->cy > 150 && in->cy < 166)) {
        possessions_view.repeat_time -= in->dt;
        if (possessions_view.repeat_time <= 0) { step = possessions_view.repeat_step; possessions_view.repeat_time = 0.08f; }
    }
    if (possessions_view.drag && limit && in->on_picture) { possessions_view.scroll[0] = (in->cy-possessions_view.drag_offset-45)*limit/(104-height); }
    else { possessions_view.scroll[0] += step; }
    if (in->key == KEY_PRIOR) { possessions_view.scroll[0] -= 14; }
    if (in->key == KEY_NEXT) { possessions_view.scroll[0] += 14; }
    if (possessions_view.scroll[0] < 0) { possessions_view.scroll[0] = 0; }
    if (possessions_view.scroll[0] > limit) { possessions_view.scroll[0] = limit; }
    if (possessions_view.scroll[0] != old) { possessions_view.revision++; }
}

void possessions_spellbook_name(void)
{
    df_u32 rows[OBJECTS];
    int count;

    count = spell_rows(possessions_view.book, rows, OBJECTS);
    if (possessions_view.selected < count) { spell_rename(rows[possessions_view.selected], possessions_view.input); }
}

void possessions_spellshop_frame(const PlayIn *in, int click)
{
    if (in->ch == 13) {
        void (*buy)(int);
        int id;

        /* picklist_frame picked: the shop closes, then the spell is bought. */
        buy = g_shop_buy; id = possessions_view.shop[possessions_view.selected];
        possessions_close();
        if (buy) { buy(id); }
    }
    else if (in->key == KEY_DOWN) { if (possessions_view.selected+1 < possessions_view.shop_count) { possessions_view.selected++; if (possessions_view.selected >= possessions_view.scroll[0]+14) { possessions_view.scroll[0]++; } } }
    else if (in->key == KEY_UP) { if (possessions_view.selected) { possessions_view.selected--; if (possessions_view.selected < possessions_view.scroll[0]) { possessions_view.scroll[0]--; } } }
    else if (click) { spellshop_click(in->cx,in->cy); }
}

int possessions_spellbook_frame(const PlayIn *in, int click)
{
    if (possessions_view.casting && in->ch == 13) {
        df_u32 ids[OBJECTS];
        int count;
        count = spell_rows(possessions_view.book,ids,OBJECTS);
        if (possessions_view.selected < count) {
            df_u32 id;
            id = ids[possessions_view.selected]; possessions_close(); magic_ready(id);
        }
        return 1;
    }
    spell_scroll(in);
    if (click && !(in->cx > 144 && in->cx < 153 && in->cy > 29 && in->cy < 166)) {
        spell_click(in->cx,in->cy,(in->mouse_pressed & 2) != 0);
    }
    return 0;
}
