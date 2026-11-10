#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/describe.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/tables/itemdefs.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/ailments.h"
#include "game/ogfall/play/ground.h"
#include "game/ogfall/play/move.h"

static OgPossessions g;
static void (*g_answer)(const char *);
static OgBook g_reading;
static df_u8 g_owner[110 * 184];
const OgPossessions *og_possessions(void) { return &g; }
const OgBook *og_reading(void) { return &g_reading; }
df_u8 *og_doll_owners(void) { return g_owner; }

void og_possessions_reset(void)
{
    g_answer = 0;
    og_book_close(&g_reading);
    mem_zero(&g, sizeof(g));
    g.skill_group = -1;
    g.revision++;
}

static int game_mode(int page)
{
    if (page == OG_VIEW_SHEET) { return OG_GAME_SHEET; }
    if (page == OG_VIEW_INVENTORY) { return OG_GAME_INVENTORY; }
    if (page == OG_VIEW_BOOK || page == OG_VIEW_MESSAGE) { return OG_GAME_READING; }
    return OG_GAME_SPELLBOOK;
}

void og_possessions_close(void)
{
    int mode;
    if (!g.page) { return; }
    if (g.page == OG_VIEW_SHEET && !og_progress_finish()) {
        df_copy(g.notice,"You have not distributed all your attribute points.",sizeof(g.notice));
        g.revision++; return;
    }
    g_answer = 0;
    if (g.page == OG_VIEW_BOOK) { og_book_close(&g_reading); }
    og_state_mode_pop();
    mode = og_state()->mode;
    g.page = mode == OG_GAME_SHEET ? OG_VIEW_SHEET : mode == OG_GAME_INVENTORY ? OG_VIEW_INVENTORY : OG_VIEW_NONE;
    g.notice[0] = g.input[0] = 0;
    g.prompt = 0;
    g.skill_group = -1;
    g.drag = g.repeat_step = 0;
    g.notice_page = 0;
    g.painting = 0;
    g.revision++;
}

int og_possessions_open(int page, df_u32 remote)
{
    if (!og_state()->ready || page < OG_VIEW_INVENTORY || page > OG_VIEW_MESSAGE) { return 0; }
    if (page == OG_VIEW_SPELLS) {
        df_u32 ids[OG_OBJECTS];
        g.book = og_inventory_book();
        if (!g.book || !og_spell_rows(g.book, ids, OG_OBJECTS)) {
            if (g.page) { df_copy(g.notice, "Your spellbook contains no spells.", sizeof(g.notice)); g.revision++; }
            else { og_msg_set("Your spellbook contains no spells."); }
            return 0;
        }
    }
    if (!og_state_mode_push(game_mode(page))) { return 0; }
    g.underlay = g.page;
    g.page = page;
    g.notice[0] = g.input[0] = 0;
    g.prompt = 0;
    g.skill_group = -1;
    g.skill_scroll = 0;
    g.selected = 0;
    g.drag = g.repeat_step = g.notice_page = 0;
    g.painting = 0;
    if (page == OG_VIEW_SPELLS) { g.scroll[0] = 0; }
    if (page == OG_VIEW_INVENTORY) {
        g.remote = remote;
        g.scroll[0] = g.scroll[1] = 0;
        g.tab = 0;
        g.action = OG_ITEM_EQUIP;
    }
    /* inventory_open and an ordinary sheet_open are silent. Only the two
     * book screens play OpenBook (DAGGER.SND ID 237) on entry. */
    if (page == OG_VIEW_SPELLS || page == OG_VIEW_BOOK) { og_sound(237); }
    g.revision++;
    return 1;
}

int og_possessions_cast(void)
{
    if (!og_possessions_open(OG_VIEW_SPELLS,0)) { return 0; }
    g.casting = 1; return 1;
}

int og_possessions_message(const char *text, void (*answer)(const char *))
{
    if (!text || !*text || g.page || !og_possessions_open(OG_VIEW_MESSAGE,0)) { return 0; }
    df_copy(g.notice,text,sizeof(g.notice)); g_answer = answer;
    if (answer) { g.prompt = OG_PROMPT_ANSWER; }
    g.revision++;
    return 1;
}

static void submit_answer(int accepted)
{
    char text[32];
    void (*callback)(const char *);
    df_copy(text,g.input,sizeof(text)); callback = g_answer; g_answer = 0;
    og_possessions_close();
    if (callback) { callback(accepted ? text : 0); }
}

static df_u32 ground(void)
{
    if (!g.remote) {
        g.remote = og_ground_container(og_state()->x,
            og_world_state()->where.kind == OG_PLACE_WORLD ? og_move_body()->feet[1] : og_state()->eye_y-32,
            og_state()->z);
    }
    return g.remote;
}

static void item_info(const OgObject *item)
{
    g.painting = item->group == 13 ? og_painting_info(item->id,g.notice,sizeof(g.notice)) : 0;
    if (!g.painting) { og_item_info(item->id,g.notice,sizeof(g.notice)); }
    g.notice_page = 0;
}

int og_possessions_use(df_u32 id)
{
    const OgObject *item;
    item = og_object(id);
    if (!item || item->type != OG_RECORD_ITEM || (item->parent != og_state()->inventory_id &&
        !(g.page == OG_VIEW_INVENTORY && item->parent == g.remote))) { return 0; }
    if (item->group == 0 && item->index >= 0 && item->index < 4) {
        /* inven.c consumes the entire drug record, even when level-one
         * protection or class immunity rejects its poison. */
        og_poison_apply(-1,item->index+136,1);
        og_object_delete(id);
        df_copy(g.notice,"You consume the drug.",sizeof(g.notice)); g.revision++;
        return 1;
    }
    if (item->group == 27 && item->index == 0) {
        if (item->parent != og_state()->inventory_id) { return 0; }
        return og_possessions_open(OG_VIEW_SPELLS,0);
    }
    if (item->group == 7) {
        if (!og_book_open(&g_reading, item->message) || !og_book_read(&g_reading, 0)) {
            og_book_close(&g_reading);
            df_copy(g.notice, "This book could not be read.", sizeof(g.notice)); g.revision++; return 0;
        }
        if (!og_possessions_open(OG_VIEW_BOOK, 0)) { og_book_close(&g_reading); return 0; }
        g.book_page = 0;
        return 1;
    }
    if (item->group == 6 || item->group == 12) {
        /* Cycling variants changes the actual picture, never its template. */
        OgObject *changed;
        changed = og_object_edit(id);
        changed->inventory_image = (changed->inventory_image & ~127) |
            og_clothing_next(changed->group,changed->index,changed->inventory_image & 127);
        g.revision++;
        return 1;
    }
    df_copy(g.notice, "This item has no usable effect.", sizeof(g.notice)); g.revision++;
    return 0;
}

static void select_item(df_u32 id, int remote)
{
    const OgObject *item;
    item = og_object(id);
    if (!item) { return; }
    if (g.action == OG_ITEM_INFO) { item_info(item); }
    else if (g.action == OG_ITEM_USE) { og_possessions_use(id); }
    else if (remote) {
        if (!og_inventory_transfer(id, og_state()->inventory_id)) { df_copy(g.notice, og_inventory_error(), sizeof(g.notice)); }
        else if (g.action == OG_ITEM_EQUIP && og_object(id) && og_item_slot(item->group,item->index) >= 0 &&
                 !og_inventory_equip(id)) { df_copy(g.notice,og_inventory_error(),sizeof(g.notice)); }
    } else if (g.action == OG_ITEM_EQUIP) {
        if (!og_inventory_equip(id)) { df_copy(g.notice, og_inventory_error(), sizeof(g.notice)); }
    } else if (g.action == OG_ITEM_REMOVE) {
        if (ground() && !og_inventory_transfer(id, g.remote)) { df_copy(g.notice, og_inventory_error(), sizeof(g.notice)); }
    }
    g.revision++;
}

static int inside(int x, int y, int left, int top, int right, int bottom)
{
    return x > left && x < right && y > top && y < bottom;
}

static void inventory_click(int x, int y)
{
    static const int button_y[4] = {36,58,80,103};
    df_u32 ids[OG_OBJECTS];
    int i, side, count, at, owner;
    if (y < 10) {
        g.tab = x < 93 ? 0 : x < 163 ? 1 : x < 255 ? 2 : 3;
        g.scroll[0] = g.scroll[1] = 0;
        return;
    }
    if (inside(x,y,222,178,261,200)) { og_possessions_close(); return; }
    if (inside(x,y,226,14,257,28)) { df_copy(g.notice, "You do not have a wagon here.", sizeof(g.notice)); return; }
    for (i = 0; i < 4; i++) {
        if (inside(x,y,226,button_y[i],257,button_y[i]+14)) { g.action = i; return; }
    }
    if (inside(x,y,226,126,257,140)) { g.prompt = OG_PROMPT_GOLD; g.input[0] = 0; return; }
    if (x >= 0 && x < 46 && y >= 10) {
        owner = ((y-10)/31)*2 + (x >= 23);
        if (owner < 12 && y < 10+(owner/2)*31+22 && og_state()->equipped[owner]) {
            if (g.action == OG_ITEM_EQUIP) { og_inventory_unequip(og_state()->equipped[owner]); }
            else { select_item(og_state()->equipped[owner],0); }
        }
        return;
    }
    if (inside(x,y,49,13,159,197)) {
        owner = g_owner[(y-13)*110+x-49];
        if (owner < 27) {
            if (g.action == OG_ITEM_EQUIP) { og_inventory_unequip(og_state()->equipped[owner]); }
            else { select_item(og_state()->equipped[owner], 0); }
        }
        return;
    }
    side = x >= 261 ? 1 : x >= 163 && x < 222 ? 0 : -1;
    if (side < 0 || y < 48) { return; }
    count = og_inventory_list(side ? g.remote : og_state()->inventory_id, g.tab, ids, OG_OBJECTS);
    if (x < (side ? 270 : 172)) {
        if (y < 64) { g.scroll[side]--; }
        else if (y > 183) { g.scroll[side]++; }
        if (g.scroll[side] > count-4) { g.scroll[side] = count-4; }
        if (g.scroll[side] < 0) { g.scroll[side] = 0; }
        return;
    }
    at = g.scroll[side] + (y-48)/38;
    if (at < count) { select_item(ids[at], side); }
    count = og_inventory_list(side ? g.remote : og_state()->inventory_id,g.tab,ids,OG_OBJECTS);
    if (g.scroll[side] > count-4) { g.scroll[side] = count > 4 ? count-4 : 0; }
}

static void sheet_click(int x, int y)
{
    int i;
    if (og_progress()->active && inside(x,y,140,5,190,195)) {
        i = (y-5)/24;
        if (i < OG_ATTRS) {
            g.selected = i;
            if (x >= 176) { og_progress_adjust(i,(y-5)%24 < 12 ? 1 : -1); }
        }
        return;
    }
    if (og_progress()->active && !inside(x,y,50,179,89,198)) { return; }
    if (inside(x,y,50,179,89,198)) { og_possessions_close(); }
    else if (inside(x,y,1,150,67,160)) {
        og_possessions_close(); og_possessions_open(OG_VIEW_INVENTORY,0);
    } else if (inside(x,y,68,150,134,160)) { og_possessions_open(OG_VIEW_SPELLS,0); }
    else if (inside(x,y,9,3,135,12)) { g.prompt = OG_PROMPT_NAME; df_copy(g.input, og_character()->name, sizeof(g.input)); }
    else if (inside(x,y,2,83,134,93)) { df_copy(g.notice, "You have no guild affiliations.", sizeof(g.notice)); }
    else if (inside(x,y,69,164,134,178)) {
        log_format(g.notice,sizeof(g.notice),"%s\n%s %s\n%s\nCommoners: %d\nMerchants: %d\nScholars: %d\nNobility: %d\nUnderworld: %d",
                   og_character()->name, og_character()->female ? "Female" : "Male", og_race_name(og_character()->race),
                   og_state()->career.name, og_character()->reputation[0], og_character()->reputation[1],
                   og_character()->reputation[2],og_character()->reputation[3],og_character()->reputation[4]);
    } else if (inside(x,y,1,164,67,178)) { df_copy(g.notice, "Your quest log is empty.", sizeof(g.notice)); }
    else if (inside(x,y,140,5,170,195)) {
        static const char *attribute[] = {"Strength","Intelligence","Willpower","Agility","Endurance","Personality","Speed","Luck"};
        i = (y-5)/24;
        if (i < OG_ATTRS) {
            log_format(g.notice,sizeof(g.notice),"%s: %d\nBase: %d",attribute[i],og_character()->attribute[i],og_state()->base_attribute[i]);
        }
    } else if (inside(x,y,9,62,135,71)) {
        log_format(g.notice,sizeof(g.notice),"Health: %d / %d\n%s",og_character()->health,og_character()->max_health,og_words(18));
    } else if (inside(x,y,9,52,135,61)) {
        log_format(g.notice,sizeof(g.notice),"Fatigue: %d / %d",og_character()->fatigue>>6,
                   og_character()->attribute[OG_STR]+og_character()->attribute[OG_END]);
    }
    else {
        for (i = 0; i < 4; i++) {
            if (inside(x,y,10,105+i*10,126,114+i*10)) { g.skill_group = i; g.skill_scroll = 0; }
        }
    }
}

static void spell_click(int x, int y, int right)
{
    df_u32 ids[OG_OBJECTS];
    int count, at;
    count = og_spell_rows(g.book, ids, OG_OBJECTS);
    if (!count) { og_possessions_close(); return; }
    if (g.selected >= count) { g.selected = count-1; }
    if (inside(x,y,242,170,278,179)) { og_possessions_close(); }
    else if (inside(x,y,145,19,280,25)) {
        g.prompt = OG_PROMPT_SPELL_NAME;
        df_copy(g.input,og_object(ids[g.selected])->name,sizeof(g.input));
    } else if (inside(x,y,172,32,187,47)) { og_spell_cycle_icon(ids[g.selected],right ? -1 : 1); }
    else if (x > 161 && x < 277 && y > 58 && y < 162) {
        at = (y-58)/38;
        if ((y-58)%38 < 27) { og_spell_help(ids[g.selected],at,g.notice,sizeof(g.notice)); g.notice_page = 0; }
    } else if (inside(x,y,27,30,138,161)) {
        at = g.scroll[0] + (y-30)/9;
        if (at < count) {
            if (g.casting && g.selected == at) {
                og_possessions_close(); og_magic_ready(ids[at]);
            } else { g.selected = at; }
        }
    } else if (inside(x,y,23,167,67,182)) {
        if (og_spell_delete(ids[g.selected])) {
            if (count == 1) { og_possessions_close(); return; }
            g.selected = g.scroll[0] = 0;
        }
    } else if (inside(x,y,68,167,112,182)) {
        if (og_spell_move(ids[g.selected],-1)) { g.selected = g.scroll[0] = 0; }
    } else if (inside(x,y,151,167,195,182)) {
        if (og_spell_move(ids[g.selected],1)) { g.selected = g.scroll[0] = 0; }
    }
    if (g.selected < g.scroll[0]) { g.scroll[0] = g.selected; }
    if (g.selected >= g.scroll[0]+14) { g.scroll[0] = g.selected-13; }
}

void og_spell_scrollbar(int *top, int *height)
{
    df_u32 ids[OG_OBJECTS];
    int count;
    count = og_spell_rows(g.book,ids,OG_OBJECTS);
    *height = count > 14 ? 104*14/count : 104;
    if (*height < 6) { *height = 6; }
    *top = 45 + (count > 14 ? g.scroll[0]*(104-*height)/(count-14) : 0);
}

static void spell_scroll(const OgIn *in)
{
    df_u32 ids[OG_OBJECTS];
    int count, limit, top, height, old, step;
    if (!in->key && !(in->mouse_pressed & 3) && !(in->mouse_held & 3) && !g.drag && !g.repeat_step) { return; }
    count = og_spell_rows(g.book,ids,OG_OBJECTS);
    limit = count > 14 ? count-14 : 0;
    old = g.scroll[0]; step = 0;
    og_spell_scrollbar(&top,&height);
    if (!(in->mouse_held & 3)) { g.drag = g.repeat_step = 0; }
    if ((in->mouse_pressed & 3) && in->on_picture && in->cx > 144 && in->cx < 153) {
        if (in->cy > 29 && in->cy < 45) { step = g.repeat_step = -1; }
        else if (in->cy > 150 && in->cy < 166) { step = g.repeat_step = 1; }
        else if (in->cy >= 45 && in->cy < 149) {
            if (in->cy >= top && in->cy < top+height) { g.drag = 1; g.drag_offset = in->cy-top; }
            else { step = in->cy < top ? -14 : 14; }
        }
        g.repeat_time = 0.35f;
    } else if (g.repeat_step && in->on_picture && in->cx > 144 && in->cx < 153 &&
               (g.repeat_step < 0 ? in->cy > 29 && in->cy < 45 : in->cy > 150 && in->cy < 166)) {
        g.repeat_time -= in->dt;
        if (g.repeat_time <= 0) { step = g.repeat_step; g.repeat_time = 0.08f; }
    }
    if (g.drag && limit && in->on_picture) { g.scroll[0] = (in->cy-g.drag_offset-45)*limit/(104-height); }
    else { g.scroll[0] += step; }
    if (in->key == OG_VK_PRIOR) { g.scroll[0] -= 14; }
    if (in->key == OG_VK_NEXT) { g.scroll[0] += 14; }
    if (g.scroll[0] < 0) { g.scroll[0] = 0; }
    if (g.scroll[0] > limit) { g.scroll[0] = limit; }
    if (g.scroll[0] != old) { g.revision++; }
}

int og_notice_lines(const char *text, int first, char out[][96], int capacity)
{
    int count, length, n, split;
    count = 0;
    while (*text) {
        while (*text == '\n' || *text == ' ') { text++; }
        if (!*text) { break; }
        for (length = 0; text[length] && text[length] != '\n' && length < 40; length++) { }
        split = length;
        if (length == 40 && text[length] && text[length] != '\n') {
            for (n = length; n > 0; n--) { if (text[n] == ' ') { split = n; break; } }
        }
        if (out && count >= first && count-first < capacity) {
            for (n = 0; n < split; n++) { out[count-first][n] = text[n]; }
            while (n && out[count-first][n-1] == ' ') { n--; }
            out[count-first][n] = 0;
        }
        text += split; count++;
    }
    return count;
}

static void book_page(int page)
{
    if (page < 0 || page >= g_reading.pages || page == g.book_page) { return; }
    if (og_book_read(&g_reading,page)) { g.book_page = page; og_sound(205); }
    else { df_copy(g.notice, "This page is damaged.",sizeof(g.notice)); }
}

static void prompt_accept(void)
{
    int value, i;
    df_u32 id, rows[OG_OBJECTS];
    if (g.prompt == OG_PROMPT_ANSWER) { submit_answer(1); return; }
    value = 0;
    for (i = 0; g.input[i] && value < 10000000; i++) { value = value * 10 + g.input[i] - '0'; }
    if (g.prompt == OG_PROMPT_NAME) {
        if (!g.input[0]) { return; }
        df_copy(og_character()->name, g.input, OG_NAME);
    } else if (g.prompt == OG_PROMPT_SPELL_NAME) {
        i = og_spell_rows(g.book,rows,OG_OBJECTS);
        if (g.selected < i) { og_spell_rename(rows[g.selected],g.input); }
    } else if (g.prompt == OG_PROMPT_GOLD) {
        if (value < 1 || value > og_character()->gold) { return; }
        if (!ground()) { return; }
        id = og_item_create(g.remote,28,0,0);
        if (!id) { return; }
        og_object_edit(id)->value = value;
        og_character()->gold -= value;
        og_state_event(OG_EVENT_GOLD,id,g.remote,-value);
    } else if (g.prompt == OG_PROMPT_PAGE) { book_page(value-1); }
    g.prompt = 0; g.input[0] = 0;
}

void og_possessions_frame(const OgIn *in)
{
    int n, click;
    if (!g.page) { return; }
    click = (in->mouse_pressed & (g.page == OG_VIEW_SPELLS ? 3 : 1)) && in->on_picture;
    if (click) { og_sound(g.page == OG_VIEW_SPELLS ? 205 : 203); }
    if (!g.notice[0]) { g.notice_page = 0; }
    if (g.prompt) {
        if (in->ch == 27) {
            if (g.prompt == OG_PROMPT_ANSWER) { submit_answer(0); }
            else { g.prompt = 0; }
        }
        else if (in->ch == 13 || (click && inside(in->cx,in->cy,84,116,151,134))) { prompt_accept(); }
        else if (click && inside(in->cx,in->cy,169,116,236,134)) {
            if (g.prompt == OG_PROMPT_ANSWER) { submit_answer(0); }
            else { g.prompt = 0; }
        }
        else {
            for (n = 0; g.input[n]; n++) { }
            if (in->ch == 8 && n) { g.input[n-1] = 0; }
            else if (in->ch >= 32 && in->ch <= 126 && n < (int)sizeof(g.input)-1 &&
                     (g.prompt == OG_PROMPT_NAME || (g.prompt == OG_PROMPT_ANSWER && n < 16) ||
                      (g.prompt == OG_PROMPT_SPELL_NAME && n < 24) ||
                      (g.prompt != OG_PROMPT_SPELL_NAME && g.prompt != OG_PROMPT_ANSWER && in->ch >= '0' && in->ch <= '9'))) {
                g.input[n] = (char)in->ch; g.input[n+1] = 0;
            }
        }
    } else if (g.notice[0]) {
        int pages;
        pages = (og_notice_lines(g.notice,0,0,0)+OG_NOTICE_LINES-1)/OG_NOTICE_LINES;
        if (in->key == OG_VK_PRIOR) { if (g.notice_page) { g.notice_page--; } }
        else if (in->key == OG_VK_NEXT) { if (g.notice_page+1 < pages) { g.notice_page++; } }
        else if (in->ch == 27 || (in->mouse_pressed & 2)) { g.notice[0] = 0; }
        else if (in->ch == 13 || in->ch == ' ' || click) {
            if (g.notice_page+1 < pages) { g.notice_page++; } else { g.notice[0] = 0; }
        }
    } else if (g.skill_group >= 0) {
        if (in->ch == 27 || click) { g.skill_group = -1; }
        else if (in->key == OG_VK_NEXT || in->key == OG_VK_DOWN) { if (g.skill_group == 3 && g.skill_scroll < 7) { g.skill_scroll++; } }
        else if (in->key == OG_VK_PRIOR || in->key == OG_VK_UP) { if (g.skill_scroll) { g.skill_scroll--; } }
    } else if (in->ch == 27) { og_possessions_close(); }
    else if (g.page == OG_VIEW_INVENTORY) {
        if (in->pressed[OG_INVENTORY]) { og_possessions_close(); }
        else if (in->pressed[OG_SHEET]) { og_possessions_close(); og_possessions_open(OG_VIEW_SHEET,0); }
        else if (click) { inventory_click(in->cx,in->cy); }
    } else if (g.page == OG_VIEW_SHEET) {
        if (in->pressed[OG_SHEET]) { og_possessions_close(); }
        else if (in->pressed[OG_INVENTORY] && !og_progress()->active) { og_possessions_close(); og_possessions_open(OG_VIEW_INVENTORY,0); }
        else if (click) { sheet_click(in->cx,in->cy); }
    } else if (g.page == OG_VIEW_SPELLS) {
        if (g.casting && in->ch == 13) {
            df_u32 ids[OG_OBJECTS];
            int count;
            count = og_spell_rows(g.book,ids,OG_OBJECTS);
            if (g.selected < count) {
                df_u32 id;
                id = ids[g.selected]; og_possessions_close(); og_magic_ready(id);
            }
            return;
        }
        spell_scroll(in);
        if (click && !(in->cx > 144 && in->cx < 153 && in->cy > 29 && in->cy < 166)) {
            spell_click(in->cx,in->cy,(in->mouse_pressed & 2) != 0);
        }
    } else if (g.page == OG_VIEW_BOOK) {
        if (in->key == OG_VK_PRIOR || in->key == OG_VK_LEFT) { book_page(g.book_page-1); }
        else if (in->key == OG_VK_NEXT || in->key == OG_VK_RIGHT) { book_page(g.book_page+1); }
        else if (click) {
            if (inside(in->cx,in->cy,181,188,193,196)) { book_page(g.book_page-1); }
            else if (inside(in->cx,in->cy,209,188,222,196)) { book_page(g.book_page+1); }
            else if (inside(in->cx,in->cy,240,187,269,196)) { g.prompt = OG_PROMPT_PAGE; g.input[0] = 0; }
            else if (inside(in->cx,in->cy,278,187,307,196)) { og_possessions_close(); }
        }
    }
    if (g.page == OG_VIEW_MESSAGE && !g.notice[0] && !g.prompt) { og_possessions_close(); }
    if (click || in->ch || in->key) { g.revision++; }
}
