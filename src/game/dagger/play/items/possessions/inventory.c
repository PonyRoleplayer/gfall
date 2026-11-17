#include "internal.h"
#include "game/dagger/play/magic/potion.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/items/describe.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/log.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/magic/enchant.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/items/ground.h"
#include "game/dagger/play/input/move.h"

static df_u8 g_owner[110 * 184];
df_u8 *doll_owners(void) { return g_owner; }

/* The key for a magic item (control action 31, magic_items_open): a list of the carried items with a Cast when used, each by its name; the pick is used. */
static df_u32 g_magic_items[LIST_MOST];

static void magic_item_picked(int row)
{
    df_u32 id;

    if (row < 0 || row >= LIST_MOST) { return; }
    id = g_magic_items[row];
    possessions_close();
    if (object_at(id)) { item_use(id); }
}

int possessions_magic_items(void)
{
    static char names[LIST_MOST][LIST_NAME];
    int i, n, k, type, param;
    const ObjectInfo *item;

    if (possessions_view.page) { return 0; }
    n = 0;
    for (i = 0; i < OBJECTS && n < LIST_MOST; i++) {
        item = &game_state()->object[i];
        if (!item->id || item->type != RECORD_ITEM || item->parent != game_state()->inventory_id) { continue; }
        for (k = 0; item_enchantment(item->id, k, &type, &param); k++) {
            if (type == 0 || (k == 0 && type == 26 && (param == 3 || param == 4 || param == 8))) { g_magic_items[n] = item->id; item_name(item, names[n], LIST_NAME); n++; break; }
        }
    }
    if (!n) { return 0; }
    return possessions_list((const char (*)[LIST_NAME])names, n, magic_item_picked);
}

int possessions_trade(int mode, const ShopState *shop, df_u32 shelves)
{
    if (possessions_view.page || !market_open(mode, shop, shelves)) { return 0; }
    if (!possessions_open(VIEW_INVENTORY, shelves)) { market_close(); return 0; }
    possessions_view.trade = mode;
    possessions_view.price = 0;
    possessions_view.offer[0] = 0;
    return 1;
}

static df_u32 ground(void)
{
    if (!possessions_view.remote) {
        possessions_view.remote = ground_container(game_state()->x,
            world_where()->where.kind == WHERE_WORLD ? move_body()->feet[1] : game_state()->eye_y-32,
            game_state()->z);
    }
    return possessions_view.remote;
}

/* The offer box's words: TEXT.RSC's record with %a, the price, put in (macro_a_price). */
static int (*g_offer_say)(int text, int price, char *out, int cap);
static int (*g_letter_text)(df_u32 item, char *out, int cap);
void possessions_set_letter(int (*text)(df_u32 item, char *out, int cap)) { g_letter_text = text; }

void possessions_set_offer_words(int (*say)(int text, int price, char *out, int cap))
{
    g_offer_say = say;
}

static void offer_words(int text, int price)
{
    const char *src;
    char *to;
    int n;

    if (g_offer_say && g_offer_say(text, price, possessions_view.offer, (int)sizeof(possessions_view.offer)) >= 0) {
        return;
    }
    src = words_state(text);
    to = possessions_view.offer;
    n = 0;
    while (src && *src && n < (int)sizeof(possessions_view.offer) - 12) {
        if (src[0] == '%' && src[1] == 'a') {
            n += log_format(to + n, (int)sizeof(possessions_view.offer) - n, "%d", price);
            src += 2;
        } else {
            to[n++] = *src++;
        }
    }
    to[n] = 0;
}

/* The original's trade buttons (inv_mode_buttons[mode]); a click is strictly inside, x 226..256. */
enum { TB_WAGON, TB_INFO, TB_SELECT, TB_STEAL, TB_ACT, TB_CLEAR, TB_EXIT, TB_NONE };
static int trade_button(int x, int y)
{
    static const int top[7] = {14, 36, 58, 112, 134, 156, 182};
    static const int id[7] = {TB_WAGON, TB_INFO, TB_SELECT, TB_STEAL, TB_ACT, TB_CLEAR, TB_EXIT};
    int i;

    if (x <= 226 || x >= 256) { return TB_NONE; }
    for (i = 0; i < 7; i++) {
        /* Selling and repair have no Steal; the others follow it up the column unchanged (buttons 3.. of modes 2 and 3 are Act, Clear, Exit). */
        if (possessions_view.trade != MARKET_BUY && id[i] == TB_STEAL) { continue; }
        if (y > top[i] && y < top[i] + 13) { return id[i]; }
    }
    return TB_NONE;
}

static void trade_click(int x, int y)
{
    int b, text;

    b = trade_button(x, y);
    if (b == TB_EXIT) { possessions_close(); return; }
    if (b == TB_WAGON) { df_copy(possessions_view.notice, "You do not have a wagon here.", sizeof(possessions_view.notice)); return; }
    if (b == TB_INFO) { possessions_view.action = ITEM_INFO; return; }
    if (b == TB_SELECT) { possessions_view.action = ITEM_EQUIP; return; }
    if (b == TB_CLEAR) { market_clear(); return; }
    if (b == TB_STEAL) {
        StealState taken;
        int r;

        r = market_steal(&taken);
        if (r < 0) { return; }
        if (r) {
            df_copy(possessions_view.notice, taken.line, sizeof(possessions_view.notice));
            possessions_view.revision++;
        } else {
            /* trade_steal_button: a failure leaves the screen (the goods back on the shelves) with the watch called. */
            possessions_close();
            df_copy(possessions_view.notice, taken.line, sizeof(possessions_view.notice));
            possessions_view.revision++;
        }
        return;
    }
    if (b == TB_ACT) {
        text = market_offer();
        if (text) {
            offer_words(text, market_quote().price);
            possessions_view.prompt = PROMPT_OFFER;
        }
    }
}

static char g_info_next[600];

static void show_item_info(const ObjectInfo *item)
{
    possessions_view.painting = item->group == 13 ? painting_info(item->id,possessions_view.notice,sizeof(possessions_view.notice)) : 0;
    if (!possessions_view.painting) { item_info(item->id,possessions_view.notice,sizeof(possessions_view.notice)); }
    possessions_view.notice_page = 0;
    /* inven.c 971: an enchanted item has a second box, TEXT.RSC 1016 "Item powers: %mpw", once the first is let go. */
    g_info_next[0] = 0;
    if (item_enchanted(item->id)) {
        static char powers[400];
        const char *raw, *at;
        int n, k, artifact;

        artifact = magic_artifact_text(item->id);
        if (artifact) {
            df_copy(powers,words_state(artifact),sizeof(powers));
        } else { item_powers_text(item->id, powers, sizeof(powers)); }
        raw = words_state(1016);
        n = 0;
        for (at = raw; *at && n < (int)sizeof(g_info_next) - 1; ) {
            if (at[0] == '%' && at[1] == 'm' && at[2] == 'p' && at[3] == 'w') {
                for (k = 0; powers[k] && n < (int)sizeof(g_info_next) - 1; k++) { g_info_next[n++] = powers[k]; }
                at += 4;
            } else { g_info_next[n++] = *at++; }
        }
        g_info_next[n] = 0;
    }
}

int possessions_use(df_u32 id)
{
    const ObjectInfo *item;
    int artifact;
    item = object_at(id);
    if (!item || item->type != RECORD_ITEM || (item->parent != game_state()->inventory_id &&
        !(possessions_view.page == VIEW_INVENTORY && item->parent == possessions_view.remote))) { return 0; }
    if ((item->flags & ITEM_UNPAID) && item_enchanted(id)) {
        possessions_message("You must pay for this item before using it.",0);
        return 0;
    }
    artifact = item->parent == game_state()->inventory_id ? magic_artifact_power(id) : -1;
    if (artifact == 5) {
        ProgressState *progress;
        int i;

        progress = progress_state();
        if (progress->active || !possessions_open(VIEW_SHEET,0)) { return 0; }
        progress->active = 1; progress->points = OGHMA_POINTS;
        for (i = 0; i < ATTRS; i++) { progress->floor[i] = game_state()->base_attribute[i]; }
        object_delete(id);
        return 1;
    }
    if (artifact == 9) {
        int i;

        for (i = 0; i < OBJECTS; i++) {
            const ObjectInfo *soul;

            soul = &game_state()->object[i];
            if (soul->id && soul->type == RECORD_SOUL && soul->parent == id) { break; }
        }
        if (i < OBJECTS) { object_delete(game_state()->object[i].id); }
        possessions_message(words_state(i < OBJECTS ? 32 : 20),0);
        return 1;
    }
    /* An item used is marked (item flag 512), which a quest's op 57 looks for. */
    object_edit(id)->flags |= 512;
    /* inv_use_item: an enchanted item's powers fire first; an item with nothing else to use is then used. */
    if (item_enchanted(id) && item->parent == game_state()->inventory_id) {
        int used, pending;

        used = item_use(id);
        pending = magic_pending();
        if (used && !(item->group == 7 || item->group == 27 || item->group == 6 || item->group == 12)) {
            possessions_view.revision++;
            /* A spell that has to be fired is fired from the view: the inventory is put away. */
            if (pending && possessions_view.page == VIEW_INVENTORY) { possessions_close(); }
            return 1;
        }
        if (!object_at(id)) { possessions_view.revision++; return 1; }
    }
    if (item->group == 0 && item->index >= 0 && item->index < 4) {
        /* inven.c consumes the entire drug record, even when level-one
         * protection or class immunity rejects its poison. */
        poison_apply(-1,item->index+136,1);
        object_delete(id);
        df_copy(possessions_view.notice,"You consume the drug.",sizeof(possessions_view.notice)); possessions_view.revision++;
        return 1;
    }
    if (potion_of(id) >= 0) {
        /* inven.c: a bottle with a recipe is drunk and gone. */
        potion_drink(id);
        possessions_view.revision++;
        return 1;
    }
    if (item->group == 27 && item->index == 0) {
        if (item->parent != game_state()->inventory_id) { return 0; }
        return possessions_open(VIEW_SPELLS,0);
    }
    if (item->group == 9 && item->index == 5 && item->message > 0 && g_letter_text) {
        /* A quest's letter: its message is shown, with the quest's own names filled in. */
        static char words[800];

        if (g_letter_text(id, words, (int)sizeof(words))) { possessions_message(words, 0); possessions_view.revision++; return 1; }
    }
    if (item->group == 7) {
        return possessions_read_open(item->message);
    }
    if (item->group == 6 || item->group == 12) {
        /* Cycling variants changes the actual picture, never its template. */
        ObjectInfo *changed;
        changed = object_edit(id);
        changed->inventory_image = (changed->inventory_image & ~127) |
            clothing_next(changed->group,changed->index,changed->inventory_image & 127);
        possessions_view.revision++;
        return 1;
    }
    df_copy(possessions_view.notice, "This item has no usable effect.", sizeof(possessions_view.notice)); possessions_view.revision++;
    return 0;
}

static void select_item(df_u32 id, int remote)
{
    const ObjectInfo *item;
    item = object_at(id);
    if (!item) { return; }
    if (possessions_view.trade) {
        /* A shop: Info says what it is; Select moves it between the shelves or the counter and the pack. */
        if (possessions_view.action == ITEM_INFO) { show_item_info(item); }
        else {
            int moved;
            if (possessions_view.trade == MARKET_BUY) { moved = remote ? market_take(id) : market_return(id); }
            else { moved = remote ? market_return(id) : market_take(id); }
            if (!moved && remote == (possessions_view.trade == MARKET_BUY) && inventory_error()[0]) {
                df_copy(possessions_view.notice, inventory_error(), sizeof(possessions_view.notice));
            }
        }
        possessions_view.revision++;
        return;
    }
    if (possessions_view.action == ITEM_INFO) { show_item_info(item); }
    else if (possessions_view.action == ITEM_USE) { possessions_use(id); }
    else if (remote) {
        if (!inventory_transfer(id, game_state()->inventory_id)) { df_copy(possessions_view.notice, inventory_error(), sizeof(possessions_view.notice)); }
        else if (possessions_view.action == ITEM_EQUIP && object_at(id) && item_slot(item->group,item->index) >= 0 &&
                 !inventory_equip(id)) { df_copy(possessions_view.notice,inventory_error(),sizeof(possessions_view.notice)); }
    } else if (possessions_view.action == ITEM_EQUIP) {
        if (!inventory_equip(id)) { df_copy(possessions_view.notice, inventory_error(), sizeof(possessions_view.notice)); }
    } else if (possessions_view.action == ITEM_REMOVE) {
        if (ground() && !inventory_transfer(id, possessions_view.remote)) { df_copy(possessions_view.notice, inventory_error(), sizeof(possessions_view.notice)); }
    }
    possessions_view.revision++;
}

static void inventory_click(int x, int y)
{
    static const int button_y[4] = {36,58,80,103};
    df_u32 ids[OBJECTS];
    int i, side, count, at, owner;
    if (y < 10) {
        possessions_view.tab = x < 93 ? 0 : x < 163 ? 1 : x < 255 ? 2 : 3;
        possessions_view.scroll[0] = possessions_view.scroll[1] = 0;
        return;
    }
    /* A shop re-uses the column for its own buttons (inv_mode_buttons[mode]). */
    if (possessions_view.trade && trade_button(x,y) != TB_NONE) { trade_click(x,y); return; }
    if (!possessions_view.trade) {
        if (possessions_hit(x,y,222,178,261,200)) { possessions_close(); return; }
        if (possessions_hit(x,y,226,14,257,28)) { df_copy(possessions_view.notice, "You do not have a wagon here.", sizeof(possessions_view.notice)); return; }
        for (i = 0; i < 4; i++) {
            if (possessions_hit(x,y,226,button_y[i],257,button_y[i]+14)) { possessions_view.action = i; return; }
        }
        if (possessions_hit(x,y,226,126,257,140)) { possessions_view.prompt = PROMPT_GOLD; possessions_view.input[0] = 0; return; }
    }
    if (x >= 0 && x < 46 && y >= 10) {
        owner = ((y-10)/31)*2 + (x >= 23);
        if (owner < 12 && y < 10+(owner/2)*31+22 && game_state()->equipped[owner]) {
            if (possessions_view.action == ITEM_EQUIP) { inventory_unequip(game_state()->equipped[owner]); }
            else { select_item(game_state()->equipped[owner],0); }
        }
        return;
    }
    if (possessions_hit(x,y,49,13,159,197)) {
        owner = g_owner[(y-13)*110+x-49];
        if (owner < 27) {
            if (possessions_view.action == ITEM_EQUIP) { inventory_unequip(game_state()->equipped[owner]); }
            else { select_item(game_state()->equipped[owner], 0); }
        }
        return;
    }
    side = x >= 261 ? 1 : x >= 163 && x < 222 ? 0 : -1;
    if (side < 0 || y < 48) { return; }
    count = inventory_list(side ? possessions_view.remote : game_state()->inventory_id, possessions_view.tab, ids, OBJECTS);
    if (x < (side ? 270 : 172)) {
        if (y < 64) { possessions_view.scroll[side]--; }
        else if (y > 183) { possessions_view.scroll[side]++; }
        if (possessions_view.scroll[side] > count-4) { possessions_view.scroll[side] = count-4; }
        if (possessions_view.scroll[side] < 0) { possessions_view.scroll[side] = 0; }
        return;
    }
    at = possessions_view.scroll[side] + (y-48)/38;
    if (at < count) { select_item(ids[at], side); }
    count = inventory_list(side ? possessions_view.remote : game_state()->inventory_id,possessions_view.tab,ids,OBJECTS);
    if (possessions_view.scroll[side] > count-4) { possessions_view.scroll[side] = count > 4 ? count-4 : 0; }
}

void possessions_trade_answer(int accepted)
{
    if (!accepted) { market_confirm(0); return; }
    /* trade_settle_offer: a yes pays; with no gold the reason is TEXT.RSC 454. */
    if (!market_confirm(1) && market_refusal()) { df_copy(possessions_view.notice, words_state(market_refusal()), sizeof(possessions_view.notice)); }
    possessions_view.prompt = 0; possessions_view.offer[0] = 0; possessions_view.price = 0;
    possessions_view.revision++;
}

int possessions_drop_gold(int value)
{
    df_u32 id;

    if (value < 1 || value > character_state()->gold) { return 0; }
    if (!ground()) { return 0; }
    id = item_create(possessions_view.remote,28,0,0);
    if (!id) { return 0; }
    object_edit(id)->value = value;
    character_state()->gold -= value;
    state_event(EVENT_GOLD,id,possessions_view.remote,-value);
    return 1;
}

void possessions_inventory_notice_closed(void)
{
    if (!possessions_view.notice[0] && g_info_next[0]) { df_copy(possessions_view.notice, g_info_next, sizeof(possessions_view.notice)); g_info_next[0] = 0; possessions_view.notice_page = 0; possessions_view.revision++; }
}

void possessions_trade_quote(void)
{
    int total;

    /* The cost panel shows the base total. The offer shows the price paid. */
    total = market_quote().total;
    if (total != possessions_view.price && possessions_view.prompt != PROMPT_OFFER) {
        possessions_view.price = total;
        possessions_view.revision++;
    }
}

void possessions_inventory_frame(const PlayIn *in, int click)
{
    if (in->pressed[INVENTORY]) { possessions_close(); }
    else if (in->pressed[SHEET]) { possessions_close(); possessions_open(VIEW_SHEET,0); }
    else if (click) { inventory_click(in->cx,in->cy); }
}
