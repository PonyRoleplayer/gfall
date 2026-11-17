#include "internal.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/log.h"
#include "game/dagger/play/magic/spellmaker.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/tables/magicdefs.h"

/* ---- The spell maker (spells.c): its buttons are spellmaker_buttons, its numbers spellmaker_setting_buttons, both read from FALL.EXE. ---- */
static int g_maker_types[LIST_MOST];

static void maker_say(int text)
{
    const char *words;

    words = text < 0 ? "You do not have a spellbook!" : words_state(text);
    if (words && words[0]) { df_copy(possessions_view.notice, words, sizeof(possessions_view.notice)); possessions_view.notice_page = 0; possessions_view.revision++; }
}

/* spellmaker_pick_subtype_cb; a list left with Escape takes the effect out again (list_popup_poll). */
static void maker_subtype_picked(int row)
{
    if (row < 0) { maker_delete(maker_state()->slot); }
    else { maker_pick_subtype(row); }
    possessions_view.revision++;
}

/* spellmaker_pick_effect_cb: the effect goes in; with subtypes they are asked for, else its numbers open. */
static void maker_effect_picked(int row)
{
    static char names[LIST_MOST][LIST_NAME];
    int type, slot, n, i;

    if (row < 0 || row >= LIST_MOST) { return; }
    type = g_maker_types[row];
    slot = maker_pick_effect(type);
    possessions_view.revision++;
    if (slot < 0) { return; }
    n = maker_subtype_count(type);
    if (!n) { return; }
    for (i = 0; i < n; i++) { df_copy(names[i], effect_sub_name(type, i), LIST_NAME); }
    possessions_list((const char (*)[LIST_NAME])names, n, maker_subtype_picked);
}

/* The choice on an effect's row (msgbox_choice_rsc 1708): edit its numbers, or delete it. */
static void maker_row_chosen(int row)
{
    if (row == 0) { maker_edit(maker_state()->slot); }
    else if (row == 1) { maker_delete(maker_state()->slot); }
    possessions_view.revision++;
}

static void maker_click(int x, int y)
{
    static char names[LIST_MOST][LIST_NAME];
    int k, n, types[LIST_MOST];

    possessions_view.revision++;
    if (maker_state()->kind) {
        /* spellmaker_settings_update: the close button, then the up and down regions of the eleven numbers. */
        static const int left[11] = {64, 104, 160, 64, 104, 160, 64, 104, 144, 184, 240};
        static const int top[11] = {94, 94, 94, 114, 114, 114, 134, 134, 134, 134, 134};

        if (possessions_hit(x,y,281,94,304,109)) { maker_close_panel(); return; }
        for (k = 0; k < 11; k++) {
            if (x > left[k] && x < left[k]+23) {
                if (y > top[k] && y < top[k]+5) { if (maker_spin_allowed(k)) { maker_adjust(k, 1); } return; }
                if (y > top[k]+10 && y < top[k]+15) { if (maker_spin_allowed(k)) { maker_adjust(k, -1); } return; }
            }
        }
        return;
    }
    for (k = 0; k < 5; k++) {
        if (possessions_hit(x,y,299,114+k*16,315,130+k*16)) { maker_set_element(k); return; }
        if (possessions_hit(x,y,275,114+k*16,299,130+k*16)) { maker_set_target(k); return; }
    }
    if (possessions_hit(x,y,244,147,268,163)) { k = maker_buy(); if (k) { maker_say(k); } return; }
    if (possessions_hit(x,y,244,163,268,179)) { maker_new(); return; }
    if (possessions_hit(x,y,244,179,268,195)) { possessions_close(); return; }
    if (possessions_hit(x,y,244,114,271,144)) {
        if (maker_free_slot() < 0) { maker_say(1707); return; }
        n = maker_effect_list(types, LIST_MOST);
        for (k = 0; k < n; k++) { g_maker_types[k] = types[k]; df_copy(names[k], effect_name(types[k]), LIST_NAME); }
        possessions_list((const char (*)[LIST_NAME])names, n, maker_effect_picked);
        return;
    }
    if (possessions_hit(x,y,288,94,302,108)) { maker_icon(1); return; }
    if (possessions_hit(x,y,270,94,280,99)) { maker_icon(1); return; }
    if (possessions_hit(x,y,270,100,280,108)) { maker_icon(-1); return; }
    if (possessions_hit(x,y,59,184,166,191)) { possessions_view.prompt = PROMPT_SPELL_NAME; df_copy(possessions_view.input, "", sizeof(possessions_view.input)); return; }
    for (k = 0; k < 3; k++) {
        if (maker_state()->spell[k*2] != 255 && y > k*32+30 && y < k*32+39) {
            static const char choice[2][LIST_NAME] = {"Edit", "Delete"};

            maker_select(k);
            possessions_list(choice, 2, maker_row_chosen);
            return;
        }
    }
}

int possessions_spellmaker(void)
{
    if (possessions_view.page || !possessions_open(VIEW_MAKER, 0)) { return 0; }
    maker_new();
    return 1;
}

/* ---- The item maker (itemmakr.c): its buttons are itemmaker_buttons, read from FALL.EXE; the rules are itemmaker.c. ---- */
static int g_im_kinds[16], g_im_side, g_im_params[64], g_im_kind;

/* Whether an object is the player's: its parents lead to the player. */
static int owned_by_player(const ObjectInfo *item)
{
    int depth;

    for (depth = 0; item && depth < OBJECTS; depth++) {
        if (item->parent == game_state()->entity_id) { return 1; }
        item = object_at(item->parent);
    }
    return 0;
}

/* The player's captured souls (RECORD_SOUL in a soul trap), by creature, at most 63 (itemmaker_soul_list_cb). */
static int im_souls(int *ids, int cap)
{
    int i, n;
    const ObjectInfo *soul;

    n = 0;
    for (i = 0; i < OBJECTS && n < cap; i++) {
        soul = &game_state()->object[i];
        if (soul->id && soul->type == RECORD_SOUL && owned_by_player(soul)) { ids[n++] = soul->index; }
    }
    return n;
}

/* itemmaker_consume_soul: the soul of that creature goes, and the trap with it unless it is an artifact. */
static void im_consume(int creature)
{
    int i;
    const ObjectInfo *soul;

    for (i = 0; i < OBJECTS; i++) {
        soul = &game_state()->object[i];
        if (soul->id && soul->type == RECORD_SOUL && soul->index == creature && owned_by_player(soul)) {
            df_u32 gem;
            int def;

            gem = soul->parent;
            object_delete(soul->id);
            def = magic_item_def(gem);
            if (def < 0 || !magic_def_artifact(def)) { object_delete(gem); }
            return;
        }
    }
}

/* inv_list_left_item while the maker is open: what has no enchantment and some points, but not the item in the maker, arrows, soul traps and carts or ships. */
int possessions_im_list(df_u32 *ids, int cap)
{
    int i, n;
    const ObjectInfo *item;

    n = 0;
    for (i = 0; i < OBJECTS; i++) {
        item = &game_state()->object[i];
        if (!item->id || item->type != RECORD_ITEM || item->parent != game_state()->inventory_id || item_tab(item) != possessions_view.im_tab) { continue; }
        if (item->id == itemmaker_state()->item || item->group == ITEM_GROUP_TRANSPORT || (item->group == ITEM_GROUP_MISC && item->index == MISC_SOUL_TRAP)) { continue; }
        if (item->group == ITEM_GROUP_WEAPON && item->index == 17) { continue; }
        if (!itemmaker_capacity(item->id) || itemmaker_row(item->id) || magic_item_def(item->id) >= 0) { continue; }
        if (n < cap) { ids[n] = item->id; }
        n++;
    }
    return n < cap ? n : cap;
}

int possessions_im_rows(int side, ImRow *rows, int cap)
{
    const ItemMaker *maker;
    int s, n, kind;

    maker = itemmaker_state();
    n = 0;
    for (s = 0; s < 10; s++) {
        if (maker->kind[s] != side) { continue; }
        kind = maker->type[s];
        if (n < cap) { df_copy(rows[n].text, itemmaker_name(side, kind), sizeof(rows[n].text)); rows[n].slot = s; rows[n].soul = maker->soul[s]; n++; }
        if (maker->param[s] >= 0 && n < cap) {
            log_format(rows[n].text, sizeof(rows[n].text), "  %s", itemmaker_param_name(side, kind, maker->param[s]));
            rows[n].slot = s; rows[n].soul = maker->soul[s]; n++;
        }
        if (n < cap) { rows[n].text[0] = 0; rows[n].slot = -1; rows[n].soul = 0; n++; }
    }
    return n;
}

static void im_say(int text)
{
    const char *words;

    words = words_state(text);
    if (words && words[0]) { df_copy(possessions_view.notice, words, sizeof(possessions_view.notice)); possessions_view.notice_page = 0; }
}

/* The parameter chosen from the popup: the maker takes it (a soul's powers may not fit), or Escape takes the half-made slot out again. */
static void im_param_picked(int row)
{
    int made;

    possessions_view.revision++;
    if (row < 0) { itemmaker_cancel(); return; }
    made = itemmaker_choose(g_im_params[row]);
    if (made < 0) { df_copy(possessions_view.notice, "No room in item...", sizeof(possessions_view.notice)); possessions_view.notice_page = 0; }
}

static void im_open_params(void)
{
    static char names[LIST_MOST][LIST_NAME];
    int n, i;

    n = itemmaker_params(g_im_params, LIST_MOST);
    if (!n) {
        if (g_im_side && g_im_kind == 0) { df_copy(possessions_view.notice, "You have no captured spirits.", sizeof(possessions_view.notice)); possessions_view.notice_page = 0; }
        itemmaker_cancel();
        return;
    }
    for (i = 0; i < n; i++) { df_copy(names[i], itemmaker_param_name(g_im_side, g_im_kind, g_im_params[i]), LIST_NAME); }
    possessions_list((const char (*)[LIST_NAME])names, n, im_param_picked);
}

/* A power or a side effect chosen from the list: the first free slot, and its parameters asked for. */
static void im_kind_picked(int row)
{
    int slot;

    possessions_view.revision++;
    if (row < 0 || row >= 16) { return; }
    g_im_kind = g_im_kinds[row];
    slot = itemmaker_begin(g_im_side, g_im_kind);
    if (slot < 0) { im_say(-slot); return; }
    if (itemmaker_state()->pending >= 0) { im_open_params(); }
}

static void im_add(int side)
{
    static char names[LIST_MOST][LIST_NAME];
    int n, i;

    if (!itemmaker_state()->item) { im_say(IM_NO_ITEM); return; }
    if (!itemmaker_free_slots()) { im_say(side ? IM_NO_SIDE_SLOT : IM_NO_POWER_SLOT); return; }
    g_im_side = side;
    n = itemmaker_kinds(side, g_im_kinds, 16);
    for (i = 0; i < n; i++) { df_copy(names[i], itemmaker_name(side, g_im_kinds[i]), LIST_NAME); }
    if (n) { possessions_list((const char (*)[LIST_NAME])names, n, im_kind_picked); }
}

static void im_pick(int row)
{
    df_u32 ids[OBJECTS];
    int n, at, said, k;

    n = possessions_im_list(ids, OBJECTS);
    at = possessions_view.im_scroll + row;
    if (at >= n) { return; }
    said = itemmaker_pick(ids[at]);
    if (said) { im_say(said); return; }
    /* The item comes off: nothing it wore is undone, for it holds no enchantment. */
    for (k = 0; k < 1; k++) { if (item_equipped(ids[at]) >= 0) { inventory_unequip(ids[at]); } }
}

static void itemmaker_click(int x, int y)
{
    static const int tabs[4][4] = {{175,6,255,14},{175,15,255,23},{175,24,255,32},{175,33,255,41}};
    static const int rowtop[4] = {50, 87, 124, 161};
    ImRow rows[40];
    df_u32 ids[OBJECTS];
    int k, side, r, n, count;

    possessions_view.revision++;
    itemmaker_set_souls(im_souls);
    if (possessions_hit(x,y,3,1,163,9)) {
        if (!itemmaker_state()->item) { im_say(IM_NO_ITEM); return; }
        possessions_view.prompt = PROMPT_SPELL_NAME; df_copy(possessions_view.input, object_at(itemmaker_state()->item)->name, sizeof(possessions_view.input)); possessions_view.input[23] = 0;
        return;
    }
    for (side = 0; side < 2; side++) {
        if (possessions_hit(x,y,side ? 106 : 8,58,side ? 182 : 84,177)) {
            r = y >= 60 ? (y - 60) / 7 : -1;
            n = possessions_im_rows(side, rows, 40);
            if (r >= 0 && r < n && rows[r].slot >= 0) { itemmaker_remove(rows[r].slot); }
            return;
        }
        if (possessions_hit(x,y,side ? 106 : 9,183,side ? 182 : 84,191)) { im_add(side); return; }
    }
    if (possessions_hit(x,y,201,68,250,104)) { itemmaker_reset(); return; }
    if (possessions_hit(x,y,204,115,246,129)) {
        int said;

        said = itemmaker_enchant(im_consume);
        if (said) { im_say(said); if (said == IM_DONE) { play_sound(207); } }
        return;
    }
    if (possessions_hit(x,y,206,176,244,197)) { possessions_close(); return; }
    for (k = 0; k < 4; k++) {
        if (possessions_hit(x,y,264,rowtop[k],311,rowtop[k]+34)) { im_pick(k); return; }
        if (possessions_hit(x,y,tabs[k][0],tabs[k][1],tabs[k][2],tabs[k][3])) { possessions_view.im_tab = k; possessions_view.im_scroll = 0; return; }
    }
    count = possessions_im_list(ids, OBJECTS);
    if (possessions_hit(x,y,253,49,261,122)) { if (possessions_view.im_scroll) { possessions_view.im_scroll--; } return; }
    if (possessions_hit(x,y,253,123,261,196)) { if (count - 4 > possessions_view.im_scroll) { possessions_view.im_scroll++; } return; }
}

int possessions_itemmaker(void)
{
    if (possessions_view.page || !possessions_open(VIEW_ITEMMAKER, 0)) { return 0; }
    possessions_view.im_tab = possessions_view.im_scroll = 0;
    itemmaker_reset();
    itemmaker_set_souls(im_souls);
    return 1;
}

void possessions_itemmaker_name(void)
{
    /* itemmaker_enter_name edits the item record in place. */
    ObjectInfo *named;

    named = object_edit(itemmaker_state()->item);
    if (named) { df_copy(named->name, possessions_view.input, 24); }
}

void possessions_spellmaker_name(void)
{
    maker_set_name(possessions_view.input);
}

void possessions_itemmaker_frame(const PlayIn *in, int click)
{
    if (click) { itemmaker_click(in->cx,in->cy); }
}

void possessions_spellmaker_frame(const PlayIn *in, int click)
{
    if (in->ch == 27) { if (maker_state()->kind) { maker_close_panel(); possessions_view.revision++; } else { possessions_close(); } }
    else if (click) { maker_click(in->cx,in->cy); }
    if (maker_state()->kind) { maker_recompute(); possessions_view.revision++; }
}
