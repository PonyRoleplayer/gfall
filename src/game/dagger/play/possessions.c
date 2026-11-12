#include "game/dagger/play/possessions.h"
#include "game/dagger/play/potion.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/describe.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/tables/itemdefs.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/guild.h"
#include "game/dagger/play/spellmaker.h"
#include "game/dagger/play/itemmaker.h"
#include "game/dagger/play/enchant.h"
#include "game/dagger/play/quest.h"
#include "game/dagger/play/magicitem.h"
#include "game/dagger/tables/magicdefs.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/ailments.h"
#include "game/dagger/play/ground.h"
#include "game/dagger/play/move.h"
static void sheet_affiliations(void);

static Possessions g;
static void (*g_answer)(const char *);
static BookInfo g_reading;
static df_u8 g_owner[110 * 184];
const Possessions *possessions_state(void) { return &g; }
const BookInfo *reading_state(void) { return &g_reading; }
df_u8 *doll_owners(void) { return g_owner; }

/* Messages waiting behind the one shown (a service that has several things to say). */
static char g_queue[4][512];
static int g_queued;

void possessions_reset(void)
{
    g_queued = 0;
    g_answer = 0;
    book_close(&g_reading);
    mem_zero(&g, sizeof(g));
    g.skill_group = -1;
    g.revision++;
}

static int game_mode(int page)
{
    if (page == VIEW_SHEET) { return GAME_SHEET; }
    if (page == VIEW_INVENTORY) { return GAME_INVENTORY; }
    if (page == VIEW_BOOK || page == VIEW_MESSAGE || page == VIEW_BANK || page == VIEW_COURT || page == VIEW_TALK || page == VIEW_SERVICE || page == VIEW_POTION || page == VIEW_SPELLSHOP || page == VIEW_MAKER || page == VIEW_ITEMMAKER) { return GAME_READING; }
    return GAME_SPELLBOOK;
}

void possessions_close(void)
{
    int mode;
    if (!g.page) { return; }
    if (g.page == VIEW_SHEET && !progress_finish()) {
        df_copy(g.notice,"You have not distributed all your attribute points.",sizeof(g.notice));
        g.revision++; return;
    }
    g_answer = 0;
    g_queued = 0;
    if (g.page == VIEW_INVENTORY && g.trade) { market_close(); g.trade = 0; g.price = 0; g.offer[0] = 0; }
    if (g.page == VIEW_BOOK) { book_close(&g_reading); }
    if (g.page == VIEW_TALK) { talkwin_close(); }
    state_mode_pop();
    mode = game_state()->mode;
    g.page = mode == GAME_SHEET ? VIEW_SHEET : mode == GAME_INVENTORY ? VIEW_INVENTORY : VIEW_NONE;
    g.notice[0] = g.input[0] = 0;
    g.prompt = 0;
    g.skill_group = -1;
    g.drag = g.repeat_step = 0;
    g.notice_page = 0;
    g.painting = 0;
    g.revision++;
}

int possessions_open(int page, df_u32 remote)
{
    if (!game_state()->ready || page < VIEW_INVENTORY || page > VIEW_ITEMMAKER) { return 0; }
    if (page == VIEW_SPELLS) {
        df_u32 ids[OBJECTS];
        g.book = inventory_book();
        if (!g.book || !spell_rows(g.book, ids, OBJECTS)) {
            if (g.page) { df_copy(g.notice, "Your spellbook contains no spells.", sizeof(g.notice)); g.revision++; }
            else { msg_set("Your spellbook contains no spells."); }
            return 0;
        }
    }
    if (!state_mode_push(game_mode(page))) { return 0; }
    g.underlay = g.page;
    g.page = page;
    g.notice[0] = g.input[0] = 0;
    g.prompt = 0;
    g.skill_group = -1;
    g.skill_scroll = 0;
    g.selected = 0;
    g.drag = g.repeat_step = g.notice_page = 0;
    g.painting = 0;
    if (page == VIEW_SPELLS) { g.scroll[0] = 0; }
    if (page == VIEW_INVENTORY) {
        itemmaker_refresh_values();
        g.remote = remote;
        g.scroll[0] = g.scroll[1] = 0;
        g.tab = 0;
        g.action = ITEM_EQUIP;
    }
    /* inventory_open and an ordinary sheet_open are silent. Only the two
     * book screens play OpenBook (DAGGER.SND ID 237) on entry. */
    if (page == VIEW_SPELLS || page == VIEW_BOOK) { play_sound(237); }
    g.revision++;
    return 1;
}

/* ---- The court (slice 5): the original's questions and records, one after another. ---- */
enum { CS_NONE, CS_SURRENDER, CS_ASK, CS_PLEA, CS_TEXT };
static int (*g_court_say)(int text, const Court *court, char *out, int cap);
static void (*g_court_release)(int where);
static CourtStep g_step;
static CourtGuilds g_guilds = { -1, -1 };

/* The guilds' ranks as the court reads them (crime_free_by_guild): -1 for a guild the player is not in. */
static void court_guilds(void)
{
    Membership *m;

    m = guild_find_kind(0);
    g_guilds.brotherhood_rank = m ? m->rank : -1;
    m = guild_find_kind(3);
    g_guilds.thieves_rank = m ? m->rank : -1;
}

void possessions_set_court(int (*say)(int text, const Court *court, char *out, int cap), void (*release)(int where))
{
    g_court_say = say;
    g_court_release = release;
}

/* A record's words with the case's macros said, else the raw first variant. */
static void court_words(int text, char *out, int cap)
{
    if (g_court_say && g_court_say(text, &g.court, out, cap) >= 0) { return; }
    df_copy(out, words_state(text), cap);
}

static void court_question(int stage, int text, const char *yes, const char *no)
{
    g.court_stage = stage;
    court_words(text, g.offer, (int)sizeof(g.offer));
    df_copy(g.yes, yes, sizeof(g.yes));
    df_copy(g.no, no, sizeof(g.no));
    g.prompt = PROMPT_ASK;
    g.revision++;
}

/* The court is over: its days pass in prison one after another, then court_close's 240 minutes; the player is put where it says. */
static void court_finish(const CourtStep *s)
{
    int day;
    int fatigue;

    if (s->serve_days > 0) {
        law_serving(1);
        for (day = 0; day < s->serve_days; day++) { state_pass_minutes(1440); }
        law_serving(0);
    }
    fatigue = character_state()->fatigue;
    state_pass_minutes(COURT_CLOSE_MINUTES);
    character_state()->fatigue = fatigue;
    if (s->release && g_court_release) { g_court_release(s->release); }
    guards_dismiss();
    g.court_stage = CS_NONE;
    possessions_close();
}

/* What the last step asks of the screen: its records one by one (a click goes on), then the next question or the end. */
static void court_show(const CourtStep *s)
{
    int i;

    g_step = *s;
    for (i = 0; i < 3; i++) { g.court_texts[i] = s->text[i]; }
    g.court_text_at = 0;
    g.court_stage = CS_TEXT;
}

static void court_next(void)
{
    char words[400];
    int text;

    while (g.court_text_at < 3 && g.court_texts[g.court_text_at]) {
        text = g.court_texts[g.court_text_at++];
        /* The accusation is the first question, not a message. */
        if (text == TEXT_COURT_ASK) { continue; }
        court_words(text, words, (int)sizeof(words));
        df_copy(g.notice, words, sizeof(g.notice));
        g.notice_page = 0;
        g.revision++;
        return;
    }
    if (g_step.done || !g.court.open) { court_finish(&g_step); return; }
    if (g.court.open) { court_question(CS_ASK, TEXT_COURT_ASK, "Guilty", "Not guilty"); }
}

/* An answer of the court's questions: 1 the first button, 0 the second. */
static void court_answer(int yes)
{
    CourtStep step;
    const CharacterState *c;

    c = character_state();
    g.prompt = 0; g.offer[0] = 0;
    switch (g.court_stage) {
    case CS_SURRENDER:
        if (!yes) { g.court_stage = CS_NONE; possessions_close(); return; }
        court_guilds();
        if (law_guards_or_court(1, game_state()->region, &g_guilds, &g.court, &step) == ARREST_DEATH) {
            g.court_stage = CS_NONE; possessions_close(); player_slain(); return;
        }
        court_show(&step); court_next();
        break;
    case CS_ASK:
        if (yes) { court_accept(&g.court, &step); court_show(&step); court_next(); }
        else { court_question(CS_PLEA, TEXT_COURT_PLEA, "Polite", "Blunt"); }
        break;
    case CS_PLEA:
        /* The first button argues with Etiquette (skill 1), the second with Streetwise (skill 2): one use of it, then the roll. */
        skill_use(yes ? 1 : 2);
        court_plead(&g.court, !yes, c->skill[yes ? 1 : 2], c->attribute[5], &step);
        court_show(&step); court_next();
        break;
    default: break;
    }
}

int possessions_surrender(void)
{
    if (g.page || !possessions_open(VIEW_COURT, 0)) { return 0; }
    mem_zero(&g.court, sizeof(g.court));
    court_question(CS_SURRENDER, 15, "Yes", "No");
    return 1;
}

int possessions_arrested(void)
{
    CourtStep step;

    if (g.page && g.page != VIEW_COURT) { return 0; }
    if (!g.page && !possessions_open(VIEW_COURT, 0)) { return 0; }
    mem_zero(&g.court, sizeof(g.court));
    court_guilds();
    if (law_guards_or_court(0, game_state()->region, &g_guilds, &g.court, &step) == ARREST_DEATH) {
        possessions_close(); player_slain(); return 1;
    }
    court_show(&step); court_next();
    return 1;
}

/* ---- The bank (slice 3): the original's ten buttons (bank_buttons[0..10]), strictly inside their rectangles. ---- */
enum { BK_DEPOSIT, BK_WITHDRAW, BK_LETTERS, BK_LETTER, BK_REPAY, BK_BORROW, BK_BUY_HOUSE, BK_SELL_HOUSE, BK_BUY_SHIP, BK_SELL_SHIP, BK_EXIT, BK_NONE };

int possessions_bank(int region)
{
    if (g.page || region < 0 || region >= REGIONS || !possessions_open(VIEW_BANK, 0)) { return 0; }
    g.bank_region = region;
    g.bank_op = BK_NONE;
    return 1;
}

static int bank_button(int x, int y)
{
    static const int box[11][4] = {
        {166,60,216,74},{217,60,268,74},{165,78,216,92},{217,78,268,92},{165,96,216,110},
        {217,96,268,110},{165,114,216,128},{217,114,268,128},{165,132,216,146},{217,132,268,146},{141,164,179,182}
    };
    int i;

    for (i = 0; i < 11; i++) {
        if (x > box[i][0] && x < box[i][2] && y > box[i][1] && y < box[i][3]) { return i; }
    }
    return BK_NONE;
}

/* A message the bank shows (msgbox_show_rsc): the record, or the original's own line for a purse too heavy. */
static void bank_say(int text)
{
    df_copy(g.notice, text > 0 ? words_state(text) : "You cannot carry so much gold.", sizeof(g.notice));
    g.notice_page = 0;
}

/* One bank action; a question becomes the Accept/Reject box, a refusal a message, a trimmed repayment its message too. */
static void bank_run(int op, int amount, int confirmed)
{
    int text, r;

    text = 0;
    switch (op) {
    case BK_DEPOSIT: r = bank_deposit(g.bank_region, amount, &text); break;
    case BK_WITHDRAW: r = bank_withdraw(g.bank_region, amount, confirmed, &text); break;
    case BK_LETTERS: r = bank_deposit_letters(g.bank_region, confirmed, &text); break;
    case BK_LETTER: r = bank_letter(g.bank_region, amount, &text); break;
    case BK_REPAY: r = bank_repay(g.bank_region, amount, &text); break;
    case BK_BORROW: r = bank_borrow(g.bank_region, amount, &text); break;
    case BK_SELL_HOUSE: r = bank_sell_house(g.bank_region, confirmed, &text); break;
    case BK_SELL_SHIP: r = bank_sell_ship(g.bank_region, confirmed, &text); break;
    default: return;
    }
    if (r == BANK_ASK) {
        df_copy(g.offer, words_state(text), sizeof(g.offer));
        g.bank_op = op;
        g.bank_amount = amount;
        g.prompt = PROMPT_ASK;
    } else if (text) { bank_say(text); }
    g.revision++;
}

static void bank_click(int x, int y)
{
    int b;

    b = bank_button(x, y);
    if (b == BK_EXIT) { possessions_close(); return; }
    if (b == BK_NONE) { return; }
    if (b == BK_BUY_HOUSE) {
        /* Houses for sale are read from a town's blocks, which come with the wider world: none, or one owned. */
        bank_say(bank_state()->house ? BANK_TEXT_HOUSE_OWNED : BANK_TEXT_NO_HOUSE);
        return;
    }
    if (b == BK_BUY_SHIP) { bank_say(bank_state()->ship ? 284 : 285); return; }
    if (b == BK_LETTERS || b == BK_SELL_HOUSE || b == BK_SELL_SHIP) { bank_run(b, 0, 0); return; }
    /* The others take an amount, typed where the original asks (157, 152). */
    g.bank_op = b;
    g.prompt = PROMPT_AMOUNT;
    g.input[0] = 0;
    g.revision++;
}

/* ---- A shopkeeper's panel ---- */
static int inside(int x, int y, int left, int top, int right, int bottom);
static void (*g_service_act)(int what);
static int g_service_kind;

void possessions_set_service(void (*act)(int what)) { g_service_act = act; }

int possessions_service(int kind)
{
    if (g.page || kind < SERVICE_REPAIR || kind > SERVICE_TAVERN || !possessions_open(VIEW_SERVICE, 0)) { return 0; }
    g_service_kind = kind;
    g.service = kind;
    return 1;
}

int possessions_guild(int member, const char *label)
{
    if (!possessions_service(SERVICE_GUILD)) { return 0; }
    g.service_member = member;
    df_copy(g.service_label, label ? label : "", sizeof(g.service_label));
    return 1;
}

/* The buttons' rectangles (repair_menu_buttons, service_menu_buttons), strictly inside; what a click asks the game to do, or -1 to leave. */
static int service_button(int x, int y)
{
    if (g_service_kind == SERVICE_REPAIR) {
        if (inside(x,y,99,51,220,59)) { return SERVICE_REPAIR_ITEM; }
        if (inside(x,y,99,60,220,68)) { return SERVICE_TALK; }
        if (inside(x,y,99,69,220,77)) { return SERVICE_SELL_ITEMS; }
    } else if (g_service_kind == SERVICE_TAVERN) {
        /* tavern_buttons: Room, Talk, Food and drinks, Goodbye, at x 99..220 from y 51, nine pixels a row. */
        if (inside(x,y,99,51,220,59)) { return SERVICE_ROOM; }
        if (inside(x,y,99,60,220,68)) { return SERVICE_TALK; }
        if (inside(x,y,99,69,220,77)) { return SERVICE_FOOD; }
        return inside(x,y,99,78,220,86) ? -1 : -2;
    } else if (g_service_kind == SERVICE_GUILD) {
        /* guild_menu_buttons: Join (a member cannot), Talk, the service, Exit. */
        if (inside(x,y,99,51,220,59) && !g.service_member) { return SERVICE_JOIN; }
        if (inside(x,y,99,60,220,68)) { return SERVICE_TALK; }
        if (inside(x,y,99,69,220,77)) { return SERVICE_DO; }
    } else {
        if (inside(x,y,99,50,220,68)) { return SERVICE_TALK; }
        if (inside(x,y,99,69,220,77)) { return SERVICE_DO; }
    }
    return inside(x,y,139,80,181,94) ? -1 : -2;
}

/* ---- The spell shop (spellshop_update) ---- */
static void (*g_shop_buy)(int id);

int possessions_spellshop(const int *ids, int count, void (*buy)(int id))
{
    int i;

    if (g.page || count < 1 || count > 96 || !buy || !possessions_open(VIEW_SPELLSHOP, 0)) { return 0; }
    for (i = 0; i < count; i++) { g.shop[i] = ids[i]; }
    g.shop_count = count;
    g.selected = 0; g.scroll[0] = 0;
    g_shop_buy = buy;
    return 1;
}

static void spellshop_click(int x, int y)
{
    int at;
    void (*buy)(int);

    if (inside(x,y,242,170,278,178)) { possessions_close(); return; }
    if (inside(x,y,23,167,65,181)) { if (g_shop_buy) { g_shop_buy(g.shop[g.selected]); } return; }
    if (x > 161 && x < 277 && y > 58 && y < 162) {
        at = (y-58)/38;
        if ((y-58)%38 < 27) {
            spell_help_def(spell_def(g.shop[g.selected]),at,g.notice,sizeof(g.notice)); g.notice_page = 0;
        }
        return;
    }
    if (inside(x,y,144,29,152,44)) { if (g.scroll[0]) { g.scroll[0]--; } return; }
    if (inside(x,y,144,150,152,165)) { if (g.scroll[0]+14 < g.shop_count) { g.scroll[0]++; } return; }
    if (inside(x,y,27,30,138,161)) {
        at = g.scroll[0] + (y-30)/9;
        if (at < g.shop_count) { g.selected = at; }
    }
    (void)buy;
}

/* ---- The potion maker (potionmaker_update, MASK00I0.IMG) ---- */

int possessions_potion(void)
{
    df_u32 ids[512];

    if (g.page) { return 0; }
    if (!ingredient_list(ids, 512) && !recipes_known(ids, 512)) {
        return possessions_message(words_state(34), 0);
    }
    if (!possessions_open(VIEW_POTION, 0)) { return 0; }
    cauldron_clear();
    g.scroll[0] = 0;
    return 1;
}

/* ---- The spell maker (spells.c): its buttons are spellmaker_buttons, its numbers spellmaker_setting_buttons, both read from FALL.EXE. ---- */
static int g_maker_types[LIST_MOST];

static void maker_say(int text)
{
    const char *words;

    words = text < 0 ? "You do not have a spellbook!" : words_state(text);
    if (words && words[0]) { df_copy(g.notice, words, sizeof(g.notice)); g.notice_page = 0; g.revision++; }
}

/* spellmaker_pick_subtype_cb; a list left with Escape takes the effect out again (list_popup_poll). */
static void maker_subtype_picked(int row)
{
    if (row < 0) { maker_delete(maker_state()->slot); }
    else { maker_pick_subtype(row); }
    g.revision++;
}

/* spellmaker_pick_effect_cb: the effect goes in; with subtypes they are asked for, else its numbers open. */
static void maker_effect_picked(int row)
{
    static char names[LIST_MOST][LIST_NAME];
    int type, slot, n, i;

    if (row < 0 || row >= LIST_MOST) { return; }
    type = g_maker_types[row];
    slot = maker_pick_effect(type);
    g.revision++;
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
    g.revision++;
}

static void maker_click(int x, int y)
{
    static char names[LIST_MOST][LIST_NAME];
    int k, n, types[LIST_MOST];

    g.revision++;
    if (maker_state()->kind) {
        /* spellmaker_settings_update: the close button, then the up and down regions of the eleven numbers. */
        static const int left[11] = {64, 104, 160, 64, 104, 160, 64, 104, 144, 184, 240};
        static const int top[11] = {94, 94, 94, 114, 114, 114, 134, 134, 134, 134, 134};

        if (inside(x,y,281,94,304,109)) { maker_close_panel(); return; }
        for (k = 0; k < 11; k++) {
            if (x > left[k] && x < left[k]+23) {
                if (y > top[k] && y < top[k]+5) { if (maker_spin_allowed(k)) { maker_adjust(k, 1); } return; }
                if (y > top[k]+10 && y < top[k]+15) { if (maker_spin_allowed(k)) { maker_adjust(k, -1); } return; }
            }
        }
        return;
    }
    for (k = 0; k < 5; k++) {
        if (inside(x,y,299,114+k*16,315,130+k*16)) { maker_set_element(k); return; }
        if (inside(x,y,275,114+k*16,299,130+k*16)) { maker_set_target(k); return; }
    }
    if (inside(x,y,244,147,268,163)) { k = maker_buy(); if (k) { maker_say(k); } return; }
    if (inside(x,y,244,163,268,179)) { maker_new(); return; }
    if (inside(x,y,244,179,268,195)) { possessions_close(); return; }
    if (inside(x,y,244,114,271,144)) {
        if (maker_free_slot() < 0) { maker_say(1707); return; }
        n = maker_effect_list(types, LIST_MOST);
        for (k = 0; k < n; k++) { g_maker_types[k] = types[k]; df_copy(names[k], effect_name(types[k]), LIST_NAME); }
        possessions_list((const char (*)[LIST_NAME])names, n, maker_effect_picked);
        return;
    }
    if (inside(x,y,288,94,302,108)) { maker_icon(1); return; }
    if (inside(x,y,270,94,280,99)) { maker_icon(1); return; }
    if (inside(x,y,270,100,280,108)) { maker_icon(-1); return; }
    if (inside(x,y,59,184,166,191)) { g.prompt = PROMPT_SPELL_NAME; df_copy(g.input, "", sizeof(g.input)); return; }
    for (k = 0; k < 3; k++) {
        if (maker_state()->spell[k*2] != 255 && y > k*32+30 && y < k*32+39) {
            static const char choice[2][LIST_NAME] = {"Edit", "Delete"};

            maker_select(k);
            possessions_list(choice, 2, maker_row_chosen);
            return;
        }
    }
}

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

    if (g.page) { return 0; }
    n = 0;
    for (i = 0; i < OBJECTS && n < LIST_MOST; i++) {
        item = &game_state()->object[i];
        if (!item->id || item->type != RECORD_ITEM || item->parent != game_state()->inventory_id) { continue; }
        for (k = 0; item_enchantment(item->id, k, &type, &param); k++) {
            if (type == 0) { g_magic_items[n] = item->id; item_name(item, names[n], LIST_NAME); n++; break; }
        }
    }
    if (!n) { return 0; }
    return possessions_list((const char (*)[LIST_NAME])names, n, magic_item_picked);
}

int possessions_spellmaker(void)
{
    if (g.page || !possessions_open(VIEW_MAKER, 0)) { return 0; }
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
        if (!item->id || item->type != RECORD_ITEM || item->parent != game_state()->inventory_id || item_tab(item) != g.im_tab) { continue; }
        if (item->id == itemmaker_state()->item || item->group == 23 || (item->group == 27 && item->index == 1)) { continue; }
        if (item->group == 3 && item->index == 17) { continue; }
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
    if (words && words[0]) { df_copy(g.notice, words, sizeof(g.notice)); g.notice_page = 0; }
}

/* The parameter chosen from the popup: the maker takes it (a soul's powers may not fit), or Escape takes the half-made slot out again. */
static void im_param_picked(int row)
{
    int made;

    g.revision++;
    if (row < 0) { itemmaker_cancel(); return; }
    made = itemmaker_choose(g_im_params[row]);
    if (made < 0) { df_copy(g.notice, "No room in item...", sizeof(g.notice)); g.notice_page = 0; }
}

static void im_open_params(void)
{
    static char names[LIST_MOST][LIST_NAME];
    int n, i;

    n = itemmaker_params(g_im_params, LIST_MOST);
    if (!n) {
        if (g_im_side && g_im_kind == 0) { df_copy(g.notice, "You have no captured spirits.", sizeof(g.notice)); g.notice_page = 0; }
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

    g.revision++;
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
    at = g.im_scroll + row;
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

    g.revision++;
    itemmaker_set_souls(im_souls);
    if (inside(x,y,3,1,163,9)) {
        if (!itemmaker_state()->item) { im_say(IM_NO_ITEM); return; }
        g.prompt = PROMPT_SPELL_NAME; df_copy(g.input, object_at(itemmaker_state()->item)->name, sizeof(g.input)); g.input[23] = 0;
        return;
    }
    for (side = 0; side < 2; side++) {
        if (inside(x,y,side ? 106 : 8,58,side ? 182 : 84,177)) {
            r = y >= 60 ? (y - 60) / 7 : -1;
            n = possessions_im_rows(side, rows, 40);
            if (r >= 0 && r < n && rows[r].slot >= 0) { itemmaker_remove(rows[r].slot); }
            return;
        }
        if (inside(x,y,side ? 106 : 9,183,side ? 182 : 84,191)) { im_add(side); return; }
    }
    if (inside(x,y,201,68,250,104)) { itemmaker_reset(); return; }
    if (inside(x,y,204,115,246,129)) {
        int said;

        said = itemmaker_enchant(im_consume);
        if (said) { im_say(said); if (said == IM_DONE) { play_sound(207); } }
        return;
    }
    if (inside(x,y,206,176,244,197)) { possessions_close(); return; }
    for (k = 0; k < 4; k++) {
        if (inside(x,y,264,rowtop[k],311,rowtop[k]+34)) { im_pick(k); return; }
        if (inside(x,y,tabs[k][0],tabs[k][1],tabs[k][2],tabs[k][3])) { g.im_tab = k; g.im_scroll = 0; return; }
    }
    count = possessions_im_list(ids, OBJECTS);
    if (inside(x,y,253,49,261,122)) { if (g.im_scroll) { g.im_scroll--; } return; }
    if (inside(x,y,253,123,261,196)) { if (count - 4 > g.im_scroll) { g.im_scroll++; } return; }
}

int possessions_itemmaker(void)
{
    if (g.page || !possessions_open(VIEW_ITEMMAKER, 0)) { return 0; }
    g.im_tab = g.im_scroll = 0;
    itemmaker_reset();
    itemmaker_set_souls(im_souls);
    return 1;
}

static void potion_say(const char *text)
{
    df_copy(g.notice, text, sizeof(g.notice)); g.notice_page = 0; g.revision++;
}

/* potion_make: a known recipe's ingredients loaded into the cauldron, or "you do not have the ingredients". */
static void potion_make(int recipe)
{
    if (!recipe_load(recipe)) { potion_say("You do not have the ingredients required."); }
    g.revision++;
}

static void potion_recipe_picked(int row)
{
    df_u32 ids[LIST_MOST];
    int count;

    if (row < 0) { return; }
    count = recipes_known(ids, LIST_MOST);
    if (row < count) { potion_make(recipe_of(ids[row])); }
}

static void potion_click(int x, int y)
{
    df_u32 ids[512], recipes[LIST_MOST];
    int i, row, col, count, cells[8], n, brewed, potion_id;
    const Cauldron *c;

    /* The five buttons (potionmaker_buttons), strictly inside. */
    if (inside(x,y,169,26,204,41)) {
        static char names[LIST_MOST][LIST_NAME];

        play_sound(205);
        count = recipes_known(recipes, LIST_MOST);
        if (!count) { potion_say("You have no recipies."); return; }
        if (count == 1) { potion_make(recipe_of(recipes[0])); return; }
        for (i = 0; i < count; i++) { df_copy(names[i], potion_name(recipe_of(recipes[i])), LIST_NAME); }
        possessions_list((const char (*)[LIST_NAME])names, count, potion_recipe_picked);
        return;
    }
    if (inside(x,y,169,42,204,57)) {
        df_u32 made;

        brewed = brew_state(&n, &made);
        (void)potion_id;
        if (brewed != BREW_NOTHING) { play_sound(208); potion_say("Your potion has been mixed."); }
        return;
    }
    if (inside(x,y,6,30,15,45)) { if (g.scroll[0]) { g.scroll[0] -= 3; } return; }
    if (inside(x,y,6,156,15,171)) {
        count = ingredient_list(ids, 512);
        if (count - g.scroll[0] > 12) { g.scroll[0] += 3; }
        return;
    }
    if (inside(x,y,290,178,306,202)) { possessions_close(); return; }
    c = cauldron_state();
    /* A cell of the cauldron (two across from x 221, 38 pixels down from y 30): taking it out. The cells shown are the full ones in order. */
    if (x > 221 && x < 304 && y > 30 && y < 171) {
        col = (x - 221) / 56; if ((x - 221) % 56 > 27) { return; }
        row = (y - 30) / 38; if ((y - 30) % 38 > 24) { return; }
        n = 0;
        for (i = 0; i < CAULDRON; i++) { if (c->item[i]) { cells[n++] = i; } }
        i = col + row * 2;
        if (i < n) { cauldron_remove(cells[i]); }
        return;
    }
    /* The ingredients (three across from x 16, 38 pixels down): putting one in. */
    if (x >= 16 && x <= 155 && y >= 30 && y <= 171) {
        col = (x - 16) / 56; if ((x - 16) % 56 > 27) { return; }
        row = (y - 30) / 38; if ((y - 30) % 38 > 27) { return; }
        count = ingredient_list(ids, 512);
        i = col + row * 3 + g.scroll[0];
        if (i < count && cauldron_count() != CAULDRON) { cauldron_add(ids[i]); }
    }
}

int possessions_talk(const TalkWho *who, const TalkPlayer *player, int witnessed, int *text)
{
    if (g.page || !talkwin_open(who, player, witnessed, text)) { return 0; }
    if (!possessions_open(VIEW_TALK, 0)) { talkwin_close(); return 0; }
    return 1;
}

int possessions_trade(int mode, const ShopState *shop, df_u32 shelves)
{
    if (g.page || !market_open(mode, shop, shelves)) { return 0; }
    if (!possessions_open(VIEW_INVENTORY, shelves)) { market_close(); return 0; }
    g.trade = mode;
    g.price = 0;
    g.offer[0] = 0;
    return 1;
}

int possessions_cast(void)
{
    if (!possessions_open(VIEW_SPELLS,0)) { return 0; }
    g.casting = 1; return 1;
}

/* A dialog over the world: the message page already up and between questions is used again (an answer's follow-up), else one is opened. */
static int dialog_ready(void)
{
    if ((g.page == VIEW_MESSAGE || g.page == VIEW_POTION || g.page == VIEW_SPELLSHOP || g.page == VIEW_MAKER || g.page == VIEW_ITEMMAKER) && !g.prompt) { return 1; }
    return !g.page && possessions_open(VIEW_MESSAGE,0);
}

static void (*g_ask_cb)(int yes);
static void (*g_number_cb)(int amount);
static void (*g_list_cb)(int row);

int possessions_list(const char names[][LIST_NAME], int count, void (*pick)(int row))
{
    int i;

    if (!names || count < 1 || count > LIST_MOST || !pick || !dialog_ready()) { return 0; }
    for (i = 0; i < count; i++) { df_copy(g.list[i],names[i],LIST_NAME); }
    g.list_count = count; g.list_top = 0; g.prompt = PROMPT_LIST; g_list_cb = pick; g.revision++;
    return 1;
}

int possessions_ask(const char *words, void (*answer)(int yes))
{
    if (!words || !*words || !answer || !dialog_ready()) { return 0; }
    df_copy(g.offer,words,sizeof(g.offer));
    df_copy(g.yes,"Yes",sizeof(g.yes)); df_copy(g.no,"No",sizeof(g.no));
    g.prompt = PROMPT_ASK; g_ask_cb = answer; g.revision++;
    return 1;
}

int possessions_number(const char *words, void (*answer)(int amount))
{
    return possessions_number_from(words, 0, answer);
}

int possessions_number_from(const char *words, int initial, void (*answer)(int amount))
{
    if (!words || !*words || !answer || !dialog_ready()) { return 0; }
    df_copy(g.offer,words,sizeof(g.offer)); g.input[0] = 0;
    if (initial > 0) { log_format(g.input,sizeof(g.input),"%d",initial); }
    g.prompt = PROMPT_NUMBER; g_number_cb = answer; g.revision++;
    return 1;
}

int possessions_message(const char *text, void (*answer)(const char *))
{
    /* A message said while another is up waits its turn (up to four); said over an answered question it is shown at once. */
    /* Over a window that is up (the potion maker, the spell shop), the words are shown over it. */
    if (text && *text && (g.page == VIEW_POTION || g.page == VIEW_SPELLSHOP) && !g.prompt && !g.notice[0]) {
        df_copy(g.notice,text,sizeof(g.notice)); g.notice_page = 0; g.revision++; return 1;
    }
    if (text && *text && g.page == VIEW_MESSAGE && !g.prompt) {
        if (!g.notice[0]) { df_copy(g.notice,text,sizeof(g.notice)); g.notice_page = 0; g.revision++; return 1; }
        if (g_queued < 4) { df_copy(g_queue[g_queued++],text,sizeof(g_queue[0])); return 1; }
        return 0;
    }
    if (!text || !*text || g.page || !possessions_open(VIEW_MESSAGE,0)) { return 0; }
    df_copy(g.notice,text,sizeof(g.notice)); g_answer = answer;
    if (answer) { g.prompt = PROMPT_ANSWER; }
    g.revision++;
    return 1;
}

static void submit_answer(int accepted)
{
    char text[32];
    void (*callback)(const char *);
    df_copy(text,g.input,sizeof(text)); callback = g_answer; g_answer = 0;
    possessions_close();
    if (callback) { callback(accepted ? text : 0); }
}

static df_u32 ground(void)
{
    if (!g.remote) {
        g.remote = ground_container(game_state()->x,
            world_where()->where.kind == WHERE_WORLD ? move_body()->feet[1] : game_state()->eye_y-32,
            game_state()->z);
    }
    return g.remote;
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

    if (g_offer_say && g_offer_say(text, price, g.offer, (int)sizeof(g.offer)) >= 0) {
        return;
    }
    src = words_state(text);
    to = g.offer;
    n = 0;
    while (src && *src && n < (int)sizeof(g.offer) - 12) {
        if (src[0] == '%' && src[1] == 'a') {
            n += log_format(to + n, (int)sizeof(g.offer) - n, "%d", price);
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
        if (g.trade != MARKET_BUY && id[i] == TB_STEAL) { continue; }
        if (y > top[i] && y < top[i] + 13) { return id[i]; }
    }
    return TB_NONE;
}

static void trade_click(int x, int y)
{
    int b, text;

    b = trade_button(x, y);
    if (b == TB_EXIT) { possessions_close(); return; }
    if (b == TB_WAGON) { df_copy(g.notice, "You do not have a wagon here.", sizeof(g.notice)); return; }
    if (b == TB_INFO) { g.action = ITEM_INFO; return; }
    if (b == TB_SELECT) { g.action = ITEM_EQUIP; return; }
    if (b == TB_CLEAR) { market_clear(); return; }
    if (b == TB_STEAL) {
        StealState taken;
        int r;

        r = market_steal(&taken);
        if (r < 0) { return; }
        if (r) {
            df_copy(g.notice, taken.line, sizeof(g.notice));
            g.revision++;
        } else {
            /* trade_steal_button: a failure leaves the screen (the goods back on the shelves) with the watch called. */
            possessions_close();
            df_copy(g.notice, taken.line, sizeof(g.notice));
            g.revision++;
        }
        return;
    }
    if (b == TB_ACT) {
        text = market_offer();
        if (text) {
            offer_words(text, market_quote().price);
            g.prompt = PROMPT_OFFER;
        }
    }
}

static char g_info_next[600];

static void show_item_info(const ObjectInfo *item)
{
    g.painting = item->group == 13 ? painting_info(item->id,g.notice,sizeof(g.notice)) : 0;
    if (!g.painting) { item_info(item->id,g.notice,sizeof(g.notice)); }
    g.notice_page = 0;
    /* inven.c 971: an enchanted item has a second box, TEXT.RSC 1016 "Item powers: %mpw", once the first is let go. */
    g_info_next[0] = 0;
    if (item_enchanted(item->id)) {
        static char powers[400];
        const char *raw, *at;
        int n, k;

        item_powers_text(item->id, powers, sizeof(powers));
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
    item = object_at(id);
    if (!item || item->type != RECORD_ITEM || (item->parent != game_state()->inventory_id &&
        !(g.page == VIEW_INVENTORY && item->parent == g.remote))) { return 0; }
    /* An item used is marked (item flag 512), which a quest's op 57 looks for. */
    object_edit(id)->flags |= 512;
    /* inv_use_item: an enchanted item's powers fire first; an item with nothing else to use is then used. */
    if (item_enchanted(id) && item->parent == game_state()->inventory_id) {
        int used, pending;

        used = item_use(id);
        pending = magic_pending();
        if (used && !(item->group == 7 || item->group == 27 || item->group == 6 || item->group == 12)) {
            g.revision++;
            /* A spell that has to be fired is fired from the view: the inventory is put away. */
            if (pending && g.page == VIEW_INVENTORY) { possessions_close(); }
            return 1;
        }
        if (!object_at(id)) { g.revision++; return 1; }
    }
    if (item->group == 0 && item->index >= 0 && item->index < 4) {
        /* inven.c consumes the entire drug record, even when level-one
         * protection or class immunity rejects its poison. */
        poison_apply(-1,item->index+136,1);
        object_delete(id);
        df_copy(g.notice,"You consume the drug.",sizeof(g.notice)); g.revision++;
        return 1;
    }
    if (potion_of(id) >= 0) {
        /* inven.c: a bottle with a recipe is drunk and gone. */
        potion_drink(id);
        g.revision++;
        return 1;
    }
    if (item->group == 27 && item->index == 0) {
        if (item->parent != game_state()->inventory_id) { return 0; }
        return possessions_open(VIEW_SPELLS,0);
    }
    if (item->group == 9 && item->index == 5 && item->message > 0 && g_letter_text) {
        /* A quest's letter: its message is shown, with the quest's own names filled in. */
        static char words[800];

        if (g_letter_text(id, words, (int)sizeof(words))) { possessions_message(words, 0); g.revision++; return 1; }
    }
    if (item->group == 7) {
        if (!book_open(&g_reading, item->message) || !book_read(&g_reading, 0)) {
            book_close(&g_reading);
            df_copy(g.notice, "This book could not be read.", sizeof(g.notice)); g.revision++; return 0;
        }
        if (!possessions_open(VIEW_BOOK, 0)) { book_close(&g_reading); return 0; }
        g.book_page = 0;
        return 1;
    }
    if (item->group == 6 || item->group == 12) {
        /* Cycling variants changes the actual picture, never its template. */
        ObjectInfo *changed;
        changed = object_edit(id);
        changed->inventory_image = (changed->inventory_image & ~127) |
            clothing_next(changed->group,changed->index,changed->inventory_image & 127);
        g.revision++;
        return 1;
    }
    df_copy(g.notice, "This item has no usable effect.", sizeof(g.notice)); g.revision++;
    return 0;
}

static void select_item(df_u32 id, int remote)
{
    const ObjectInfo *item;
    item = object_at(id);
    if (!item) { return; }
    if (g.trade) {
        /* A shop: Info says what it is; Select moves it between the shelves or the counter and the pack. */
        if (g.action == ITEM_INFO) { show_item_info(item); }
        else {
            int moved;
            if (g.trade == MARKET_BUY) { moved = remote ? market_take(id) : market_return(id); }
            else { moved = remote ? market_return(id) : market_take(id); }
            if (!moved && remote == (g.trade == MARKET_BUY) && inventory_error()[0]) {
                df_copy(g.notice, inventory_error(), sizeof(g.notice));
            }
        }
        g.revision++;
        return;
    }
    if (g.action == ITEM_INFO) { show_item_info(item); }
    else if (g.action == ITEM_USE) { possessions_use(id); }
    else if (remote) {
        if (!inventory_transfer(id, game_state()->inventory_id)) { df_copy(g.notice, inventory_error(), sizeof(g.notice)); }
        else if (g.action == ITEM_EQUIP && object_at(id) && item_slot(item->group,item->index) >= 0 &&
                 !inventory_equip(id)) { df_copy(g.notice,inventory_error(),sizeof(g.notice)); }
    } else if (g.action == ITEM_EQUIP) {
        if (!inventory_equip(id)) { df_copy(g.notice, inventory_error(), sizeof(g.notice)); }
    } else if (g.action == ITEM_REMOVE) {
        if (ground() && !inventory_transfer(id, g.remote)) { df_copy(g.notice, inventory_error(), sizeof(g.notice)); }
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
    df_u32 ids[OBJECTS];
    int i, side, count, at, owner;
    if (y < 10) {
        g.tab = x < 93 ? 0 : x < 163 ? 1 : x < 255 ? 2 : 3;
        g.scroll[0] = g.scroll[1] = 0;
        return;
    }
    /* A shop re-uses the column for its own buttons (inv_mode_buttons[mode]). */
    if (g.trade && trade_button(x,y) != TB_NONE) { trade_click(x,y); return; }
    if (!g.trade) {
        if (inside(x,y,222,178,261,200)) { possessions_close(); return; }
        if (inside(x,y,226,14,257,28)) { df_copy(g.notice, "You do not have a wagon here.", sizeof(g.notice)); return; }
        for (i = 0; i < 4; i++) {
            if (inside(x,y,226,button_y[i],257,button_y[i]+14)) { g.action = i; return; }
        }
        if (inside(x,y,226,126,257,140)) { g.prompt = PROMPT_GOLD; g.input[0] = 0; return; }
    }
    if (x >= 0 && x < 46 && y >= 10) {
        owner = ((y-10)/31)*2 + (x >= 23);
        if (owner < 12 && y < 10+(owner/2)*31+22 && game_state()->equipped[owner]) {
            if (g.action == ITEM_EQUIP) { inventory_unequip(game_state()->equipped[owner]); }
            else { select_item(game_state()->equipped[owner],0); }
        }
        return;
    }
    if (inside(x,y,49,13,159,197)) {
        owner = g_owner[(y-13)*110+x-49];
        if (owner < 27) {
            if (g.action == ITEM_EQUIP) { inventory_unequip(game_state()->equipped[owner]); }
            else { select_item(game_state()->equipped[owner], 0); }
        }
        return;
    }
    side = x >= 261 ? 1 : x >= 163 && x < 222 ? 0 : -1;
    if (side < 0 || y < 48) { return; }
    count = inventory_list(side ? g.remote : game_state()->inventory_id, g.tab, ids, OBJECTS);
    if (x < (side ? 270 : 172)) {
        if (y < 64) { g.scroll[side]--; }
        else if (y > 183) { g.scroll[side]++; }
        if (g.scroll[side] > count-4) { g.scroll[side] = count-4; }
        if (g.scroll[side] < 0) { g.scroll[side] = 0; }
        return;
    }
    at = g.scroll[side] + (y-48)/38;
    if (at < count) { select_item(ids[at], side); }
    count = inventory_list(side ? g.remote : game_state()->inventory_id,g.tab,ids,OBJECTS);
    if (g.scroll[side] > count-4) { g.scroll[side] = count > 4 ? count-4 : 0; }
}

static void sheet_click(int x, int y)
{
    int i;
    if (progress_state()->active && inside(x,y,140,5,190,195)) {
        i = (y-5)/24;
        if (i < ATTRS) {
            g.selected = i;
            if (x >= 176) { progress_adjust(i,(y-5)%24 < 12 ? 1 : -1); }
        }
        return;
    }
    if (progress_state()->active && !inside(x,y,50,179,89,198)) { return; }
    if (inside(x,y,50,179,89,198)) { possessions_close(); }
    else if (inside(x,y,1,150,67,160)) {
        possessions_close(); possessions_open(VIEW_INVENTORY,0);
    } else if (inside(x,y,68,150,134,160)) { possessions_open(VIEW_SPELLS,0); }
    else if (inside(x,y,9,3,135,12)) { g.prompt = PROMPT_NAME; df_copy(g.input, character_state()->name, sizeof(g.input)); }
    else if (inside(x,y,2,83,134,93)) { sheet_affiliations(); }
    else if (inside(x,y,69,164,134,178)) {
        log_format(g.notice,sizeof(g.notice),"%s\n%s %s\n%s\nCommoners: %d\nMerchants: %d\nScholars: %d\nNobility: %d\nUnderworld: %d",
                   character_state()->name, character_state()->female ? "Female" : "Male", race_name(character_state()->race),
                   game_state()->career.name, character_state()->reputation[0], character_state()->reputation[1],
                   character_state()->reputation[2],character_state()->reputation[3],character_state()->reputation[4]);
    } else if (inside(x,y,1,164,67,178)) {
        /* logbook_open: the entries of the quests that are running, or an empty log. */
        if (logbook_text(g.notice, sizeof(g.notice)) <= 0) { df_copy(g.notice, "Your quest log is empty.", sizeof(g.notice)); }
        g.notice_page = 0;
    }
    else if (inside(x,y,140,5,170,195)) {
        static const char *attribute[] = {"Strength","Intelligence","Willpower","Agility","Endurance","Personality","Speed","Luck"};
        i = (y-5)/24;
        if (i < ATTRS) {
            log_format(g.notice,sizeof(g.notice),"%s: %d\nBase: %d",attribute[i],character_state()->attribute[i],game_state()->base_attribute[i]);
        }
    } else if (inside(x,y,9,62,135,71)) {
        log_format(g.notice,sizeof(g.notice),"Health: %d / %d\n%s",character_state()->health,character_state()->max_health,words_state(18));
    } else if (inside(x,y,9,52,135,61)) {
        log_format(g.notice,sizeof(g.notice),"Fatigue: %d / %d",character_state()->fatigue>>6,
                   character_state()->attribute[ATTR_STR]+character_state()->attribute[ATTR_END]);
    }
    else {
        for (i = 0; i < 4; i++) {
            if (inside(x,y,10,105+i*10,126,114+i*10)) { g.skill_group = i; g.skill_scroll = 0; }
        }
    }
}

static void spell_click(int x, int y, int right)
{
    df_u32 ids[OBJECTS];
    int count, at;
    count = spell_rows(g.book, ids, OBJECTS);
    if (!count) { possessions_close(); return; }
    if (g.selected >= count) { g.selected = count-1; }
    if (inside(x,y,242,170,278,179)) { possessions_close(); }
    else if (inside(x,y,145,19,280,25)) {
        g.prompt = PROMPT_SPELL_NAME;
        df_copy(g.input,object_at(ids[g.selected])->name,sizeof(g.input));
    } else if (inside(x,y,172,32,187,47)) { spell_cycle_icon(ids[g.selected],right ? -1 : 1); }
    else if (x > 161 && x < 277 && y > 58 && y < 162) {
        at = (y-58)/38;
        if ((y-58)%38 < 27) { spell_help(ids[g.selected],at,g.notice,sizeof(g.notice)); g.notice_page = 0; }
    } else if (inside(x,y,27,30,138,161)) {
        at = g.scroll[0] + (y-30)/9;
        if (at < count) {
            if (g.casting && g.selected == at) {
                possessions_close(); magic_ready(ids[at]);
            } else { g.selected = at; }
        }
    } else if (inside(x,y,23,167,67,182)) {
        if (spell_delete(ids[g.selected])) {
            if (count == 1) { possessions_close(); return; }
            g.selected = g.scroll[0] = 0;
        }
    } else if (inside(x,y,68,167,112,182)) {
        if (spell_move(ids[g.selected],-1)) { g.selected = g.scroll[0] = 0; }
    } else if (inside(x,y,151,167,195,182)) {
        if (spell_move(ids[g.selected],1)) { g.selected = g.scroll[0] = 0; }
    }
    if (g.selected < g.scroll[0]) { g.scroll[0] = g.selected; }
    if (g.selected >= g.scroll[0]+14) { g.scroll[0] = g.selected-13; }
}

void spell_scrollbar(int *top, int *height)
{
    df_u32 ids[OBJECTS];
    int count;
    count = spell_rows(g.book,ids,OBJECTS);
    *height = count > 14 ? 104*14/count : 104;
    if (*height < 6) { *height = 6; }
    *top = 45 + (count > 14 ? g.scroll[0]*(104-*height)/(count-14) : 0);
}

static void spell_scroll(const PlayIn *in)
{
    df_u32 ids[OBJECTS];
    int count, limit, top, height, old, step;
    if (!in->key && !(in->mouse_pressed & 3) && !(in->mouse_held & 3) && !g.drag && !g.repeat_step) { return; }
    count = spell_rows(g.book,ids,OBJECTS);
    limit = count > 14 ? count-14 : 0;
    old = g.scroll[0]; step = 0;
    spell_scrollbar(&top,&height);
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
    if (in->key == KEY_PRIOR) { g.scroll[0] -= 14; }
    if (in->key == KEY_NEXT) { g.scroll[0] += 14; }
    if (g.scroll[0] < 0) { g.scroll[0] = 0; }
    if (g.scroll[0] > limit) { g.scroll[0] = limit; }
    if (g.scroll[0] != old) { g.revision++; }
}

int notice_lines(const char *text, int first, char out[][96], int capacity)
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
    if (book_read(&g_reading,page)) { g.book_page = page; play_sound(205); }
    else { df_copy(g.notice, "This page is damaged.",sizeof(g.notice)); }
}

static void prompt_accept(void)
{
    int value, i;
    df_u32 id, rows[OBJECTS];
    if (g.prompt == PROMPT_ANSWER) { submit_answer(1); return; }
    if (g.prompt == PROMPT_AMOUNT) {
        int amount, k;
        amount = 0;
        for (k = 0; g.input[k] && amount < 100000000; k++) { amount = amount * 10 + g.input[k] - '0'; }
        g.prompt = 0; g.input[0] = 0;
        bank_run(g.bank_op, amount, 0);
        return;
    }
    if (g.prompt == PROMPT_NUMBER) {
        int amount, k;
        void (*cb)(int);

        amount = 0;
        for (k = 0; g.input[k] && amount < 100000000; k++) { amount = amount * 10 + g.input[k] - '0'; }
        cb = g_number_cb; g_number_cb = 0;
        g.prompt = 0; g.offer[0] = 0; g.input[0] = 0;
        if (cb) { cb(amount); }
        return;
    }
    if (g.prompt == PROMPT_ASK) {
        if (g.page == VIEW_COURT) { court_answer(1); return; }
        if (g.page == VIEW_MESSAGE && g_ask_cb) {
            void (*cb)(int);

            cb = g_ask_cb; g_ask_cb = 0;
            g.prompt = 0; g.offer[0] = 0;
            cb(1);
            return;
        }
        g.prompt = 0; g.offer[0] = 0;
        bank_run(g.bank_op, g.bank_amount, 1);
        return;
    }
    if (g.prompt == PROMPT_OFFER) {
        /* trade_settle_offer: a yes pays; with no gold the reason is TEXT.RSC 454. */
        if (!market_confirm(1) && market_refusal()) { df_copy(g.notice, words_state(market_refusal()), sizeof(g.notice)); }
        g.prompt = 0; g.offer[0] = 0; g.price = 0;
        g.revision++;
        return;
    }
    value = 0;
    for (i = 0; g.input[i] && value < 10000000; i++) { value = value * 10 + g.input[i] - '0'; }
    if (g.prompt == PROMPT_NAME) {
        if (!g.input[0]) { return; }
        df_copy(character_state()->name, g.input, NAME_LEN);
    } else if (g.prompt == PROMPT_SPELL_NAME && g.page == VIEW_ITEMMAKER) {
        /* itemmaker_enter_name: the item's name is edited in place, 23 characters. */
        ObjectInfo *named;

        named = object_edit(itemmaker_state()->item);
        if (named) { df_copy(named->name, g.input, 24); }
    } else if (g.prompt == PROMPT_SPELL_NAME && g.page == VIEW_MAKER) {
        maker_set_name(g.input);
    } else if (g.prompt == PROMPT_SPELL_NAME) {
        i = spell_rows(g.book,rows,OBJECTS);
        if (g.selected < i) { spell_rename(rows[g.selected],g.input); }
    } else if (g.prompt == PROMPT_GOLD) {
        if (value < 1 || value > character_state()->gold) { return; }
        if (!ground()) { return; }
        id = item_create(g.remote,28,0,0);
        if (!id) { return; }
        object_edit(id)->value = value;
        character_state()->gold -= value;
        state_event(EVENT_GOLD,id,g.remote,-value);
    } else if (g.prompt == PROMPT_PAGE) { book_page(value-1); }
    g.prompt = 0; g.input[0] = 0;
}

/* The no, Escape, or the Reject button: whatever is asked is answered no. */
static void prompt_reject(void)
{
    if (g.prompt == PROMPT_ANSWER) { submit_answer(0); return; }
    if (g.prompt == PROMPT_OFFER) { market_confirm(0); }
    if (g.prompt == PROMPT_ASK && g.page == VIEW_COURT) { court_answer(0); return; }
    if (g.prompt == PROMPT_ASK && g.page == VIEW_MESSAGE && g_ask_cb) {
        void (*cb)(int);

        cb = g_ask_cb; g_ask_cb = 0;
        g.offer[0] = 0; g.prompt = 0;
        cb(0);
        return;
    }
    if (g.prompt == PROMPT_LIST) {
        void (*cb)(int);

        cb = g_list_cb; g_list_cb = 0;
        g.prompt = 0; g.list_count = 0;
        if (cb) { cb(-1); }
        return;
    }
    if (g.prompt == PROMPT_NUMBER) { g_number_cb = 0; }
    g.offer[0] = 0; g.prompt = 0;
}

void possessions_frame(const PlayIn *in)
{
    int n, click;
    if (!g.page) { return; }
    if (g.trade) {
        /* The cost panel shows trade_total (inventory_draw), the base total, not the price paid: that is in the offer. */
        n = market_quote().total;
        if (n != g.price && g.prompt != PROMPT_OFFER) { g.price = n; g.revision++; }
    }
    click = (in->mouse_pressed & (g.page == VIEW_SPELLS ? 3 : 1)) && in->on_picture;
    if (click) { play_sound(g.page == VIEW_SPELLS ? 205 : 203); }
    if (!g.notice[0]) { g.notice_page = 0; }
    if (g.prompt) {
        if (g.prompt == PROMPT_LIST) {
            /* The rows are 8 pixels from y 34; a click on one picks it, Escape none. */
            int shown;

            shown = g.list_count < LIST_SHOWN ? g.list_count : LIST_SHOWN;
            if (in->ch == 27) { prompt_reject(); }
            else if (in->key == KEY_DOWN || in->key == KEY_NEXT) { if (g.list_top+shown < g.list_count) { g.list_top += in->key == KEY_NEXT ? shown : 1; if (g.list_top+shown > g.list_count) { g.list_top = g.list_count-shown; } g.revision++; } }
            else if (in->key == KEY_UP || in->key == KEY_PRIOR) { if (g.list_top) { g.list_top -= in->key == KEY_PRIOR ? shown : 1; if (g.list_top < 0) { g.list_top = 0; } g.revision++; } }
            else if (click && in->cx > 100 && in->cx < 220 && in->cy >= 34 && (in->cy-34)/8 < shown) {
                void (*cb)(int);
                int row;

                row = g.list_top+(in->cy-34)/8;
                cb = g_list_cb; g_list_cb = 0;
                g.prompt = 0; g.list_count = 0;
                if (cb) { cb(row); }
            }
        }
        else if (in->ch == 27) { prompt_reject(); }
        else if (in->ch == 13 || (click && inside(in->cx,in->cy,84,116,151,134))) { prompt_accept(); }
        else if (click && inside(in->cx,in->cy,169,116,236,134)) { prompt_reject(); }
        else {
            for (n = 0; g.input[n]; n++) { }
            if (in->ch == 8 && n) { g.input[n-1] = 0; }
            else if (in->ch >= 32 && in->ch <= 126 && n < (int)sizeof(g.input)-1 &&
                     (g.prompt == PROMPT_NAME || (g.prompt == PROMPT_ANSWER && n < 16) ||
                      (g.prompt == PROMPT_SPELL_NAME && n < 24) ||
                      (g.prompt != PROMPT_SPELL_NAME && g.prompt != PROMPT_ANSWER && g.prompt != PROMPT_OFFER && g.prompt != PROMPT_ASK && in->ch >= '0' && in->ch <= '9' && n < 10))) {
                g.input[n] = (char)in->ch; g.input[n+1] = 0;
            }
        }
    } else if (g.notice[0]) {
        int pages;
        pages = (notice_lines(g.notice,0,0,0)+NOTICE_LINES-1)/NOTICE_LINES;
        if (in->key == KEY_PRIOR) { if (g.notice_page) { g.notice_page--; } }
        else if (in->key == KEY_NEXT) { if (g.notice_page+1 < pages) { g.notice_page++; } }
        else if (in->ch == 27 || (in->mouse_pressed & 2)) { g.notice[0] = 0; }
        else if (in->ch == 13 || in->ch == ' ' || click) {
            if (g.notice_page+1 < pages) { g.notice_page++; } else { g.notice[0] = 0; }
        }
        if (!g.notice[0] && g_info_next[0]) { df_copy(g.notice, g_info_next, sizeof(g.notice)); g_info_next[0] = 0; g.notice_page = 0; g.revision++; }
        if (!g.notice[0] && g.page == VIEW_COURT && g.court_stage == CS_TEXT) { court_next(); }
    } else if (g.skill_group >= 0) {
        if (in->ch == 27 || click) { g.skill_group = -1; }
        else if (in->key == KEY_NEXT || in->key == KEY_DOWN) { if (g.skill_group == 3 && g.skill_scroll < 7) { g.skill_scroll++; } }
        else if (in->key == KEY_PRIOR || in->key == KEY_UP) { if (g.skill_scroll) { g.skill_scroll--; } }
    } else if (g.page == VIEW_SPELLSHOP) {
        if (in->ch == 27) { possessions_close(); }
        else if (in->ch == 13) {
            void (*buy)(int);
            int id;

            /* picklist_frame picked: the shop closes, then the spell is bought. */
            buy = g_shop_buy; id = g.shop[g.selected];
            possessions_close();
            if (buy) { buy(id); }
        }
        else if (in->key == KEY_DOWN) { if (g.selected+1 < g.shop_count) { g.selected++; if (g.selected >= g.scroll[0]+14) { g.scroll[0]++; } } }
        else if (in->key == KEY_UP) { if (g.selected) { g.selected--; if (g.selected < g.scroll[0]) { g.scroll[0]--; } } }
        else if (click) { spellshop_click(in->cx,in->cy); }
    } else if (g.page == VIEW_ITEMMAKER) {
        if (in->ch == 27) { possessions_close(); }
        else if (click) { itemmaker_click(in->cx,in->cy); }
    } else if (g.page == VIEW_MAKER) {
        if (in->ch == 27) { if (maker_state()->kind) { maker_close_panel(); g.revision++; } else { possessions_close(); } }
        else if (click) { maker_click(in->cx,in->cy); }
        if (maker_state()->kind) { maker_recompute(); g.revision++; }
    } else if (g.page == VIEW_POTION) {
        if (in->ch == 27) { possessions_close(); }
        else if (click) { potion_click(in->cx,in->cy); }
    } else if (g.page == VIEW_SERVICE) {
        /* Escape and Exit leave; the others close the panel and hand over (the original closes the menu first). */
        int what;

        what = in->ch == 27 ? -1 : click ? service_button(in->cx,in->cy) : -2;
        if (what != -2) {
            possessions_close();
            if (what >= 0 && g_service_act) { g_service_act(what); }
        }
    } else if (g.page == VIEW_TALK) {
        /* The window's own keys and its 19 buttons; Goodbye or Escape on the categories ends it. */
        if (in->ch == 27) { talkwin_key(TW_KEY_ESC); }
        else if (in->ch == 13) { talkwin_key(TW_KEY_ENTER); }
        else if (in->key == KEY_UP) { talkwin_key(TW_KEY_UP); }
        else if (in->key == KEY_DOWN) { talkwin_key(TW_KEY_DOWN); }
        else if (in->key == KEY_PRIOR) { talkwin_key(TW_KEY_PGUP); }
        else if (in->key == KEY_NEXT) { talkwin_key(TW_KEY_PGDN); }
        else if (click) { talkwin_click(in->cx,in->cy); }
        if (!talkwin_state()->open) { possessions_close(); }
    } else if (in->ch == 27) { possessions_close(); }
    else if (g.page == VIEW_INVENTORY) {
        if (in->pressed[INVENTORY]) { possessions_close(); }
        else if (in->pressed[SHEET]) { possessions_close(); possessions_open(VIEW_SHEET,0); }
        else if (click) { inventory_click(in->cx,in->cy); }
    } else if (g.page == VIEW_BANK) {
        if (click) { bank_click(in->cx,in->cy); }
    } else if (g.page == VIEW_SHEET) {
        if (in->pressed[SHEET]) { possessions_close(); }
        else if (in->pressed[INVENTORY] && !progress_state()->active) { possessions_close(); possessions_open(VIEW_INVENTORY,0); }
        else if (click) { sheet_click(in->cx,in->cy); }
    } else if (g.page == VIEW_SPELLS) {
        if (g.casting && in->ch == 13) {
            df_u32 ids[OBJECTS];
            int count;
            count = spell_rows(g.book,ids,OBJECTS);
            if (g.selected < count) {
                df_u32 id;
                id = ids[g.selected]; possessions_close(); magic_ready(id);
            }
            return;
        }
        spell_scroll(in);
        if (click && !(in->cx > 144 && in->cx < 153 && in->cy > 29 && in->cy < 166)) {
            spell_click(in->cx,in->cy,(in->mouse_pressed & 2) != 0);
        }
    } else if (g.page == VIEW_BOOK) {
        if (in->key == KEY_PRIOR || in->key == KEY_LEFT) { book_page(g.book_page-1); }
        else if (in->key == KEY_NEXT || in->key == KEY_RIGHT) { book_page(g.book_page+1); }
        else if (click) {
            if (inside(in->cx,in->cy,181,188,193,196)) { book_page(g.book_page-1); }
            else if (inside(in->cx,in->cy,209,188,222,196)) { book_page(g.book_page+1); }
            else if (inside(in->cx,in->cy,240,187,269,196)) { g.prompt = PROMPT_PAGE; g.input[0] = 0; }
            else if (inside(in->cx,in->cy,278,187,307,196)) { possessions_close(); }
        }
    }
    if (g.page == VIEW_MESSAGE && !g.notice[0] && !g.prompt && g_queued) {
        int k;

        df_copy(g.notice,g_queue[0],sizeof(g.notice)); g.notice_page = 0;
        for (k = 1; k < g_queued; k++) { df_copy(g_queue[k-1],g_queue[k],sizeof(g_queue[0])); }
        g_queued--; g.revision++;
    }
    if (g.page == VIEW_MESSAGE && !g.notice[0] && !g.prompt) { possessions_close(); }
    if (click || in->ch || in->key) { g.revision++; }
}

/* sheet_show_affiliations: "Affiliation  Rank", then a line for each guild or order the player belongs to (faction name and rank title); none is TEXT.RSC 19. */
static void sheet_affiliations(void)
{
    const FactionDef *faction;
    char text[400];
    int i, n, count;

    n = 0; count = 0;
    n += log_format(text + n, (int)sizeof(text) - n, "Affiliation        Rank");
    for (i = 0; i < MEMBERSHIPS; i++) {
        const Membership *m;

        m = &guilds_state()->m[i];
        if (!m->used) { continue; }
        faction = faction_def(faction_index(m->faction));
        n += log_format(text + n, (int)sizeof(text) - n, "\n%s: %s", faction ? faction->name : "?", guild_rank_name(m->kind, m->rank));
        count++;
    }
    df_copy(g.notice, count ? text : words_state(19), sizeof(g.notice));
}
