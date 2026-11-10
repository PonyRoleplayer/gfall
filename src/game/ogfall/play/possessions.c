#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/potion.h"
#include "game/ogfall/play/bank.h"
#include "game/ogfall/play/combat.h"
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

/* Messages waiting behind the one shown (a service that has several things to say). */
static char g_queue[4][512];
static int g_queued;

void og_possessions_reset(void)
{
    g_queued = 0;
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
    if (page == OG_VIEW_BOOK || page == OG_VIEW_MESSAGE || page == OG_VIEW_BANK || page == OG_VIEW_COURT || page == OG_VIEW_TALK || page == OG_VIEW_SERVICE || page == OG_VIEW_POTION || page == OG_VIEW_SPELLSHOP) { return OG_GAME_READING; }
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
    g_queued = 0;
    if (g.page == OG_VIEW_INVENTORY && g.trade) { og_market_close(); g.trade = 0; g.price = 0; g.offer[0] = 0; }
    if (g.page == OG_VIEW_BOOK) { og_book_close(&g_reading); }
    if (g.page == OG_VIEW_TALK) { og_talkwin_close(); }
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
    if (!og_state()->ready || page < OG_VIEW_INVENTORY || page > OG_VIEW_SPELLSHOP) { return 0; }
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

/* ---- The court (slice 5): the original's questions and records, one after another. ---- */
enum { CS_NONE, CS_SURRENDER, CS_ASK, CS_PLEA, CS_TEXT };
static int (*g_court_say)(int text, const OgCourt *court, char *out, int cap);
static void (*g_court_release)(int where);
static OgCourtStep g_step;
static OgCourtGuilds g_guilds = { -1, -1 };

/* The guilds' ranks as the court reads them (crime_free_by_guild): -1 for a guild the player is not in. */
static void court_guilds(void)
{
    OgMembership *m;

    m = og_guild_find_kind(0);
    g_guilds.brotherhood_rank = m ? m->rank : -1;
    m = og_guild_find_kind(3);
    g_guilds.thieves_rank = m ? m->rank : -1;
}

void og_possessions_set_court(int (*say)(int text, const OgCourt *court, char *out, int cap), void (*release)(int where))
{
    g_court_say = say;
    g_court_release = release;
}

/* A record's words with the case's macros said, else the raw first variant. */
static void court_words(int text, char *out, int cap)
{
    if (g_court_say && g_court_say(text, &g.court, out, cap) >= 0) { return; }
    df_copy(out, og_words(text), cap);
}

static void court_question(int stage, int text, const char *yes, const char *no)
{
    g.court_stage = stage;
    court_words(text, g.offer, (int)sizeof(g.offer));
    df_copy(g.yes, yes, sizeof(g.yes));
    df_copy(g.no, no, sizeof(g.no));
    g.prompt = OG_PROMPT_ASK;
    g.revision++;
}

/* The court is over: its days pass in prison one after another, then court_close's 240 minutes; the player is put where it says. */
static void court_finish(const OgCourtStep *s)
{
    int day;
    int fatigue;

    if (s->serve_days > 0) {
        og_law_serving(1);
        for (day = 0; day < s->serve_days; day++) { og_state_pass_minutes(1440); }
        og_law_serving(0);
    }
    fatigue = og_character()->fatigue;
    og_state_pass_minutes(OG_COURT_CLOSE_MINUTES);
    og_character()->fatigue = fatigue;
    if (s->release && g_court_release) { g_court_release(s->release); }
    og_guards_dismiss();
    g.court_stage = CS_NONE;
    og_possessions_close();
}

/* What the last step asks of the screen: its records one by one (a click goes on), then the next question or the end. */
static void court_show(const OgCourtStep *s)
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
        if (text == OG_TEXT_COURT_ASK) { continue; }
        court_words(text, words, (int)sizeof(words));
        df_copy(g.notice, words, sizeof(g.notice));
        g.notice_page = 0;
        g.revision++;
        return;
    }
    if (g_step.done || !g.court.open) { court_finish(&g_step); return; }
    if (g.court.open) { court_question(CS_ASK, OG_TEXT_COURT_ASK, "Guilty", "Not guilty"); }
}

/* An answer of the court's questions: 1 the first button, 0 the second. */
static void court_answer(int yes)
{
    OgCourtStep step;
    const OgCharacter *c;

    c = og_character();
    g.prompt = 0; g.offer[0] = 0;
    switch (g.court_stage) {
    case CS_SURRENDER:
        if (!yes) { g.court_stage = CS_NONE; og_possessions_close(); return; }
        court_guilds();
        if (og_law_guards_or_court(1, og_state()->region, &g_guilds, &g.court, &step) == OG_ARREST_DEATH) {
            g.court_stage = CS_NONE; og_possessions_close(); og_player_slain(); return;
        }
        court_show(&step); court_next();
        break;
    case CS_ASK:
        if (yes) { og_court_accept(&g.court, &step); court_show(&step); court_next(); }
        else { court_question(CS_PLEA, OG_TEXT_COURT_PLEA, "Polite", "Blunt"); }
        break;
    case CS_PLEA:
        /* The first button argues with Etiquette (skill 1), the second with Streetwise (skill 2): one use of it, then the roll. */
        og_skill_use(yes ? 1 : 2);
        og_court_plead(&g.court, !yes, c->skill[yes ? 1 : 2], c->attribute[5], &step);
        court_show(&step); court_next();
        break;
    default: break;
    }
}

int og_possessions_surrender(void)
{
    if (g.page || !og_possessions_open(OG_VIEW_COURT, 0)) { return 0; }
    mem_zero(&g.court, sizeof(g.court));
    court_question(CS_SURRENDER, 15, "Yes", "No");
    return 1;
}

int og_possessions_arrested(void)
{
    OgCourtStep step;

    if (g.page && g.page != OG_VIEW_COURT) { return 0; }
    if (!g.page && !og_possessions_open(OG_VIEW_COURT, 0)) { return 0; }
    mem_zero(&g.court, sizeof(g.court));
    court_guilds();
    if (og_law_guards_or_court(0, og_state()->region, &g_guilds, &g.court, &step) == OG_ARREST_DEATH) {
        og_possessions_close(); og_player_slain(); return 1;
    }
    court_show(&step); court_next();
    return 1;
}

/* ---- The bank (slice 3): the original's ten buttons (bank_buttons[0..10]), strictly inside their rectangles. ---- */
enum { BK_DEPOSIT, BK_WITHDRAW, BK_LETTERS, BK_LETTER, BK_REPAY, BK_BORROW, BK_BUY_HOUSE, BK_SELL_HOUSE, BK_BUY_SHIP, BK_SELL_SHIP, BK_EXIT, BK_NONE };

int og_possessions_bank(int region)
{
    if (g.page || region < 0 || region >= OG_REGIONS || !og_possessions_open(OG_VIEW_BANK, 0)) { return 0; }
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
    df_copy(g.notice, text > 0 ? og_words(text) : "You cannot carry so much gold.", sizeof(g.notice));
    g.notice_page = 0;
}

/* One bank action; a question becomes the Accept/Reject box, a refusal a message, a trimmed repayment its message too. */
static void bank_run(int op, int amount, int confirmed)
{
    int text, r;

    text = 0;
    switch (op) {
    case BK_DEPOSIT: r = og_bank_deposit(g.bank_region, amount, &text); break;
    case BK_WITHDRAW: r = og_bank_withdraw(g.bank_region, amount, confirmed, &text); break;
    case BK_LETTERS: r = og_bank_deposit_letters(g.bank_region, confirmed, &text); break;
    case BK_LETTER: r = og_bank_letter(g.bank_region, amount, &text); break;
    case BK_REPAY: r = og_bank_repay(g.bank_region, amount, &text); break;
    case BK_BORROW: r = og_bank_borrow(g.bank_region, amount, &text); break;
    case BK_SELL_HOUSE: r = og_bank_sell_house(g.bank_region, confirmed, &text); break;
    case BK_SELL_SHIP: r = og_bank_sell_ship(g.bank_region, confirmed, &text); break;
    default: return;
    }
    if (r == OG_BANK_ASK) {
        df_copy(g.offer, og_words(text), sizeof(g.offer));
        g.bank_op = op;
        g.bank_amount = amount;
        g.prompt = OG_PROMPT_ASK;
    } else if (text) { bank_say(text); }
    g.revision++;
}

static void bank_click(int x, int y)
{
    int b;

    b = bank_button(x, y);
    if (b == BK_EXIT) { og_possessions_close(); return; }
    if (b == BK_NONE) { return; }
    if (b == BK_BUY_HOUSE) {
        /* Houses for sale are read from a town's blocks, which come with the wider world: none, or one owned. */
        bank_say(og_bank()->house ? OG_BANK_TEXT_HOUSE_OWNED : OG_BANK_TEXT_NO_HOUSE);
        return;
    }
    if (b == BK_BUY_SHIP) { bank_say(og_bank()->ship ? 284 : 285); return; }
    if (b == BK_LETTERS || b == BK_SELL_HOUSE || b == BK_SELL_SHIP) { bank_run(b, 0, 0); return; }
    /* The others take an amount, typed where the original asks (157, 152). */
    g.bank_op = b;
    g.prompt = OG_PROMPT_AMOUNT;
    g.input[0] = 0;
    g.revision++;
}

/* ---- A shopkeeper's panel ---- */
static int inside(int x, int y, int left, int top, int right, int bottom);
static void (*g_service_act)(int what);
static int g_service_kind;

void og_possessions_set_service(void (*act)(int what)) { g_service_act = act; }

int og_possessions_service(int kind)
{
    if (g.page || kind < OG_SERVICE_REPAIR || kind > OG_SERVICE_TAVERN || !og_possessions_open(OG_VIEW_SERVICE, 0)) { return 0; }
    g_service_kind = kind;
    g.service = kind;
    return 1;
}

int og_possessions_guild(int member, const char *label)
{
    if (!og_possessions_service(OG_SERVICE_GUILD)) { return 0; }
    g.service_member = member;
    df_copy(g.service_label, label ? label : "", sizeof(g.service_label));
    return 1;
}

/* The buttons' rectangles (repair_menu_buttons, service_menu_buttons), strictly inside; what a click asks the game to do, or -1 to leave. */
static int service_button(int x, int y)
{
    if (g_service_kind == OG_SERVICE_REPAIR) {
        if (inside(x,y,99,51,220,59)) { return OG_SERVICE_REPAIR_ITEM; }
        if (inside(x,y,99,60,220,68)) { return OG_SERVICE_TALK; }
        if (inside(x,y,99,69,220,77)) { return OG_SERVICE_SELL_ITEMS; }
    } else if (g_service_kind == OG_SERVICE_TAVERN) {
        /* tavern_buttons: Room, Talk, Food and drinks, Goodbye, at x 99..220 from y 51, nine pixels a row. */
        if (inside(x,y,99,51,220,59)) { return OG_SERVICE_ROOM; }
        if (inside(x,y,99,60,220,68)) { return OG_SERVICE_TALK; }
        if (inside(x,y,99,69,220,77)) { return OG_SERVICE_FOOD; }
        return inside(x,y,99,78,220,86) ? -1 : -2;
    } else if (g_service_kind == OG_SERVICE_GUILD) {
        /* guild_menu_buttons: Join (a member cannot), Talk, the service, Exit. */
        if (inside(x,y,99,51,220,59) && !g.service_member) { return OG_SERVICE_JOIN; }
        if (inside(x,y,99,60,220,68)) { return OG_SERVICE_TALK; }
        if (inside(x,y,99,69,220,77)) { return OG_SERVICE_DO; }
    } else {
        if (inside(x,y,99,50,220,68)) { return OG_SERVICE_TALK; }
        if (inside(x,y,99,69,220,77)) { return OG_SERVICE_DO; }
    }
    return inside(x,y,139,80,181,94) ? -1 : -2;
}

/* ---- The spell shop (spellshop_update) ---- */
static void (*g_shop_buy)(int id);

int og_possessions_spellshop(const int *ids, int count, void (*buy)(int id))
{
    int i;

    if (g.page || count < 1 || count > 96 || !buy || !og_possessions_open(OG_VIEW_SPELLSHOP, 0)) { return 0; }
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

    if (inside(x,y,242,170,278,178)) { og_possessions_close(); return; }
    if (inside(x,y,23,167,65,181)) { if (g_shop_buy) { g_shop_buy(g.shop[g.selected]); } return; }
    if (x > 161 && x < 277 && y > 58 && y < 162) {
        at = (y-58)/38;
        if ((y-58)%38 < 27) {
            og_spell_help_def(og_spell_def(g.shop[g.selected]),at,g.notice,sizeof(g.notice)); g.notice_page = 0;
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

int og_possessions_potion(void)
{
    df_u32 ids[512];

    if (g.page) { return 0; }
    if (!og_ingredients(ids, 512) && !og_recipes_known(ids, 512)) {
        return og_possessions_message(og_words(34), 0);
    }
    if (!og_possessions_open(OG_VIEW_POTION, 0)) { return 0; }
    og_cauldron_clear();
    g.scroll[0] = 0;
    return 1;
}

static void potion_say(const char *text)
{
    df_copy(g.notice, text, sizeof(g.notice)); g.notice_page = 0; g.revision++;
}

/* potion_make: a known recipe's ingredients loaded into the cauldron, or "you do not have the ingredients". */
static void potion_make(int recipe)
{
    if (!og_recipe_load(recipe)) { potion_say("You do not have the ingredients required."); }
    g.revision++;
}

static void potion_recipe_picked(int row)
{
    df_u32 ids[OG_LIST_MOST];
    int count;

    if (row < 0) { return; }
    count = og_recipes_known(ids, OG_LIST_MOST);
    if (row < count) { potion_make(og_recipe_of(ids[row])); }
}

static void potion_click(int x, int y)
{
    df_u32 ids[512], recipes[OG_LIST_MOST];
    int i, row, col, count, cells[8], n, brewed, potion_id;
    const OgCauldron *c;

    /* The five buttons (potionmaker_buttons), strictly inside. */
    if (inside(x,y,169,26,204,41)) {
        static char names[OG_LIST_MOST][OG_LIST_NAME];

        og_sound(205);
        count = og_recipes_known(recipes, OG_LIST_MOST);
        if (!count) { potion_say("You have no recipies."); return; }
        if (count == 1) { potion_make(og_recipe_of(recipes[0])); return; }
        for (i = 0; i < count; i++) { df_copy(names[i], og_potion_name(og_recipe_of(recipes[i])), OG_LIST_NAME); }
        og_possessions_list((const char (*)[OG_LIST_NAME])names, count, potion_recipe_picked);
        return;
    }
    if (inside(x,y,169,42,204,57)) {
        df_u32 made;

        brewed = og_brew(&n, &made);
        (void)potion_id;
        if (brewed != OG_BREW_NOTHING) { og_sound(208); potion_say("Your potion has been mixed."); }
        return;
    }
    if (inside(x,y,6,30,15,45)) { if (g.scroll[0]) { g.scroll[0] -= 3; } return; }
    if (inside(x,y,6,156,15,171)) {
        count = og_ingredients(ids, 512);
        if (count - g.scroll[0] > 12) { g.scroll[0] += 3; }
        return;
    }
    if (inside(x,y,290,178,306,202)) { og_possessions_close(); return; }
    c = og_cauldron();
    /* A cell of the cauldron (two across from x 221, 38 pixels down from y 30): taking it out. The cells shown are the full ones in order. */
    if (x > 221 && x < 304 && y > 30 && y < 171) {
        col = (x - 221) / 56; if ((x - 221) % 56 > 27) { return; }
        row = (y - 30) / 38; if ((y - 30) % 38 > 24) { return; }
        n = 0;
        for (i = 0; i < OG_CAULDRON; i++) { if (c->item[i]) { cells[n++] = i; } }
        i = col + row * 2;
        if (i < n) { og_cauldron_remove(cells[i]); }
        return;
    }
    /* The ingredients (three across from x 16, 38 pixels down): putting one in. */
    if (x >= 16 && x <= 155 && y >= 30 && y <= 171) {
        col = (x - 16) / 56; if ((x - 16) % 56 > 27) { return; }
        row = (y - 30) / 38; if ((y - 30) % 38 > 27) { return; }
        count = og_ingredients(ids, 512);
        i = col + row * 3 + g.scroll[0];
        if (i < count && og_cauldron_count() != OG_CAULDRON) { og_cauldron_add(ids[i]); }
    }
}

int og_possessions_talk(const OgTalkWho *who, const OgTalkPlayer *player, int witnessed, int *text)
{
    if (g.page || !og_talkwin_open(who, player, witnessed, text)) { return 0; }
    if (!og_possessions_open(OG_VIEW_TALK, 0)) { og_talkwin_close(); return 0; }
    return 1;
}

int og_possessions_trade(int mode, const OgShop *shop, df_u32 shelves)
{
    if (g.page || !og_market_open(mode, shop, shelves)) { return 0; }
    if (!og_possessions_open(OG_VIEW_INVENTORY, shelves)) { og_market_close(); return 0; }
    g.trade = mode;
    g.price = 0;
    g.offer[0] = 0;
    return 1;
}

int og_possessions_cast(void)
{
    if (!og_possessions_open(OG_VIEW_SPELLS,0)) { return 0; }
    g.casting = 1; return 1;
}

/* A dialog over the world: the message page already up and between questions is used again (an answer's follow-up), else one is opened. */
static int dialog_ready(void)
{
    if ((g.page == OG_VIEW_MESSAGE || g.page == OG_VIEW_POTION || g.page == OG_VIEW_SPELLSHOP) && !g.prompt) { return 1; }
    return !g.page && og_possessions_open(OG_VIEW_MESSAGE,0);
}

static void (*g_ask_cb)(int yes);
static void (*g_number_cb)(int amount);
static void (*g_list_cb)(int row);

int og_possessions_list(const char names[][OG_LIST_NAME], int count, void (*pick)(int row))
{
    int i;

    if (!names || count < 1 || count > OG_LIST_MOST || !pick || !dialog_ready()) { return 0; }
    for (i = 0; i < count; i++) { df_copy(g.list[i],names[i],OG_LIST_NAME); }
    g.list_count = count; g.prompt = OG_PROMPT_LIST; g_list_cb = pick; g.revision++;
    return 1;
}

int og_possessions_ask(const char *words, void (*answer)(int yes))
{
    if (!words || !*words || !answer || !dialog_ready()) { return 0; }
    df_copy(g.offer,words,sizeof(g.offer));
    df_copy(g.yes,"Yes",sizeof(g.yes)); df_copy(g.no,"No",sizeof(g.no));
    g.prompt = OG_PROMPT_ASK; g_ask_cb = answer; g.revision++;
    return 1;
}

int og_possessions_number(const char *words, void (*answer)(int amount))
{
    if (!words || !*words || !answer || !dialog_ready()) { return 0; }
    df_copy(g.offer,words,sizeof(g.offer)); g.input[0] = 0;
    g.prompt = OG_PROMPT_NUMBER; g_number_cb = answer; g.revision++;
    return 1;
}

int og_possessions_message(const char *text, void (*answer)(const char *))
{
    /* A message said while another is up waits its turn (up to four); said over an answered question it is shown at once. */
    /* Over a window that is up (the potion maker, the spell shop), the words are shown over it. */
    if (text && *text && (g.page == OG_VIEW_POTION || g.page == OG_VIEW_SPELLSHOP) && !g.prompt && !g.notice[0]) {
        df_copy(g.notice,text,sizeof(g.notice)); g.notice_page = 0; g.revision++; return 1;
    }
    if (text && *text && g.page == OG_VIEW_MESSAGE && !g.prompt) {
        if (!g.notice[0]) { df_copy(g.notice,text,sizeof(g.notice)); g.notice_page = 0; g.revision++; return 1; }
        if (g_queued < 4) { df_copy(g_queue[g_queued++],text,sizeof(g_queue[0])); return 1; }
        return 0;
    }
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

/* The offer box's words: TEXT.RSC's record with %a, the price, put in (macro_a_price). */
static int (*g_offer_say)(int text, int price, char *out, int cap);

void og_possessions_set_offer_words(int (*say)(int text, int price, char *out, int cap))
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
    src = og_words(text);
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
        if (g.trade != OG_MARKET_BUY && id[i] == TB_STEAL) { continue; }
        if (y > top[i] && y < top[i] + 13) { return id[i]; }
    }
    return TB_NONE;
}

static void trade_click(int x, int y)
{
    int b, text;

    b = trade_button(x, y);
    if (b == TB_EXIT) { og_possessions_close(); return; }
    if (b == TB_WAGON) { df_copy(g.notice, "You do not have a wagon here.", sizeof(g.notice)); return; }
    if (b == TB_INFO) { g.action = OG_ITEM_INFO; return; }
    if (b == TB_SELECT) { g.action = OG_ITEM_EQUIP; return; }
    if (b == TB_CLEAR) { og_market_clear(); return; }
    if (b == TB_STEAL) {
        OgSteal taken;
        int r;

        r = og_market_steal(&taken);
        if (r < 0) { return; }
        if (r) {
            df_copy(g.notice, taken.line, sizeof(g.notice));
            g.revision++;
        } else {
            /* trade_steal_button: a failure leaves the screen (the goods back on the shelves) with the watch called. */
            og_possessions_close();
            df_copy(g.notice, taken.line, sizeof(g.notice));
            g.revision++;
        }
        return;
    }
    if (b == TB_ACT) {
        text = og_market_offer();
        if (text) {
            offer_words(text, og_market_quote().price);
            g.prompt = OG_PROMPT_OFFER;
        }
    }
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
    if (og_potion_of(id) >= 0) {
        /* inven.c: a bottle with a recipe is drunk and gone. */
        og_potion_drink(id);
        g.revision++;
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
    if (g.trade) {
        /* A shop: Info says what it is; Select moves it between the shelves or the counter and the pack. */
        if (g.action == OG_ITEM_INFO) { item_info(item); }
        else {
            int moved;
            if (g.trade == OG_MARKET_BUY) { moved = remote ? og_market_take(id) : og_market_return(id); }
            else { moved = remote ? og_market_return(id) : og_market_take(id); }
            if (!moved && remote == (g.trade == OG_MARKET_BUY) && og_inventory_error()[0]) {
                df_copy(g.notice, og_inventory_error(), sizeof(g.notice));
            }
        }
        g.revision++;
        return;
    }
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
    /* A shop re-uses the column for its own buttons (inv_mode_buttons[mode]). */
    if (g.trade && trade_button(x,y) != TB_NONE) { trade_click(x,y); return; }
    if (!g.trade) {
        if (inside(x,y,222,178,261,200)) { og_possessions_close(); return; }
        if (inside(x,y,226,14,257,28)) { df_copy(g.notice, "You do not have a wagon here.", sizeof(g.notice)); return; }
        for (i = 0; i < 4; i++) {
            if (inside(x,y,226,button_y[i],257,button_y[i]+14)) { g.action = i; return; }
        }
        if (inside(x,y,226,126,257,140)) { g.prompt = OG_PROMPT_GOLD; g.input[0] = 0; return; }
    }
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
    if (g.prompt == OG_PROMPT_AMOUNT) {
        int amount, k;
        amount = 0;
        for (k = 0; g.input[k] && amount < 100000000; k++) { amount = amount * 10 + g.input[k] - '0'; }
        g.prompt = 0; g.input[0] = 0;
        bank_run(g.bank_op, amount, 0);
        return;
    }
    if (g.prompt == OG_PROMPT_NUMBER) {
        int amount, k;
        void (*cb)(int);

        amount = 0;
        for (k = 0; g.input[k] && amount < 100000000; k++) { amount = amount * 10 + g.input[k] - '0'; }
        cb = g_number_cb; g_number_cb = 0;
        g.prompt = 0; g.offer[0] = 0; g.input[0] = 0;
        if (cb) { cb(amount); }
        return;
    }
    if (g.prompt == OG_PROMPT_ASK) {
        if (g.page == OG_VIEW_COURT) { court_answer(1); return; }
        if (g.page == OG_VIEW_MESSAGE && g_ask_cb) {
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
    if (g.prompt == OG_PROMPT_OFFER) {
        /* trade_settle_offer: a yes pays; with no gold the reason is TEXT.RSC 454. */
        if (!og_market_confirm(1) && og_market_refusal()) { df_copy(g.notice, og_words(og_market_refusal()), sizeof(g.notice)); }
        g.prompt = 0; g.offer[0] = 0; g.price = 0;
        g.revision++;
        return;
    }
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

/* The no, Escape, or the Reject button: whatever is asked is answered no. */
static void prompt_reject(void)
{
    if (g.prompt == OG_PROMPT_ANSWER) { submit_answer(0); return; }
    if (g.prompt == OG_PROMPT_OFFER) { og_market_confirm(0); }
    if (g.prompt == OG_PROMPT_ASK && g.page == OG_VIEW_COURT) { court_answer(0); return; }
    if (g.prompt == OG_PROMPT_ASK && g.page == OG_VIEW_MESSAGE && g_ask_cb) {
        void (*cb)(int);

        cb = g_ask_cb; g_ask_cb = 0;
        g.offer[0] = 0; g.prompt = 0;
        cb(0);
        return;
    }
    if (g.prompt == OG_PROMPT_LIST) {
        void (*cb)(int);

        cb = g_list_cb; g_list_cb = 0;
        g.prompt = 0; g.list_count = 0;
        if (cb) { cb(-1); }
        return;
    }
    if (g.prompt == OG_PROMPT_NUMBER) { g_number_cb = 0; }
    g.offer[0] = 0; g.prompt = 0;
}

void og_possessions_frame(const OgIn *in)
{
    int n, click;
    if (!g.page) { return; }
    if (g.trade) {
        /* The cost panel shows trade_total (inventory_draw), the base total, not the price paid: that is in the offer. */
        n = og_market_quote().total;
        if (n != g.price && g.prompt != OG_PROMPT_OFFER) { g.price = n; g.revision++; }
    }
    click = (in->mouse_pressed & (g.page == OG_VIEW_SPELLS ? 3 : 1)) && in->on_picture;
    if (click) { og_sound(g.page == OG_VIEW_SPELLS ? 205 : 203); }
    if (!g.notice[0]) { g.notice_page = 0; }
    if (g.prompt) {
        if (g.prompt == OG_PROMPT_LIST) {
            /* The rows are 8 pixels from y 34; a click on one picks it, Escape none. */
            if (in->ch == 27) { prompt_reject(); }
            else if (click && in->cx > 100 && in->cx < 220 && in->cy >= 34 && (in->cy-34)/8 < g.list_count) {
                void (*cb)(int);
                int row;

                row = (in->cy-34)/8;
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
                     (g.prompt == OG_PROMPT_NAME || (g.prompt == OG_PROMPT_ANSWER && n < 16) ||
                      (g.prompt == OG_PROMPT_SPELL_NAME && n < 24) ||
                      (g.prompt != OG_PROMPT_SPELL_NAME && g.prompt != OG_PROMPT_ANSWER && g.prompt != OG_PROMPT_OFFER && g.prompt != OG_PROMPT_ASK && in->ch >= '0' && in->ch <= '9' && n < 10))) {
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
        if (!g.notice[0] && g.page == OG_VIEW_COURT && g.court_stage == CS_TEXT) { court_next(); }
    } else if (g.skill_group >= 0) {
        if (in->ch == 27 || click) { g.skill_group = -1; }
        else if (in->key == OG_VK_NEXT || in->key == OG_VK_DOWN) { if (g.skill_group == 3 && g.skill_scroll < 7) { g.skill_scroll++; } }
        else if (in->key == OG_VK_PRIOR || in->key == OG_VK_UP) { if (g.skill_scroll) { g.skill_scroll--; } }
    } else if (g.page == OG_VIEW_SPELLSHOP) {
        if (in->ch == 27) { og_possessions_close(); }
        else if (in->ch == 13) {
            void (*buy)(int);
            int id;

            /* picklist_frame picked: the shop closes, then the spell is bought. */
            buy = g_shop_buy; id = g.shop[g.selected];
            og_possessions_close();
            if (buy) { buy(id); }
        }
        else if (in->key == OG_VK_DOWN) { if (g.selected+1 < g.shop_count) { g.selected++; if (g.selected >= g.scroll[0]+14) { g.scroll[0]++; } } }
        else if (in->key == OG_VK_UP) { if (g.selected) { g.selected--; if (g.selected < g.scroll[0]) { g.scroll[0]--; } } }
        else if (click) { spellshop_click(in->cx,in->cy); }
    } else if (g.page == OG_VIEW_POTION) {
        if (in->ch == 27) { og_possessions_close(); }
        else if (click) { potion_click(in->cx,in->cy); }
    } else if (g.page == OG_VIEW_SERVICE) {
        /* Escape and Exit leave; the others close the panel and hand over (the original closes the menu first). */
        int what;

        what = in->ch == 27 ? -1 : click ? service_button(in->cx,in->cy) : -2;
        if (what != -2) {
            og_possessions_close();
            if (what >= 0 && g_service_act) { g_service_act(what); }
        }
    } else if (g.page == OG_VIEW_TALK) {
        /* The window's own keys and its 19 buttons; Goodbye or Escape on the categories ends it. */
        if (in->ch == 27) { og_talkwin_key(OG_TW_KEY_ESC); }
        else if (in->ch == 13) { og_talkwin_key(OG_TW_KEY_ENTER); }
        else if (in->key == OG_VK_UP) { og_talkwin_key(OG_TW_KEY_UP); }
        else if (in->key == OG_VK_DOWN) { og_talkwin_key(OG_TW_KEY_DOWN); }
        else if (in->key == OG_VK_PRIOR) { og_talkwin_key(OG_TW_KEY_PGUP); }
        else if (in->key == OG_VK_NEXT) { og_talkwin_key(OG_TW_KEY_PGDN); }
        else if (click) { og_talkwin_click(in->cx,in->cy); }
        if (!og_talkwin()->open) { og_possessions_close(); }
    } else if (in->ch == 27) { og_possessions_close(); }
    else if (g.page == OG_VIEW_INVENTORY) {
        if (in->pressed[OG_INVENTORY]) { og_possessions_close(); }
        else if (in->pressed[OG_SHEET]) { og_possessions_close(); og_possessions_open(OG_VIEW_SHEET,0); }
        else if (click) { inventory_click(in->cx,in->cy); }
    } else if (g.page == OG_VIEW_BANK) {
        if (click) { bank_click(in->cx,in->cy); }
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
    if (g.page == OG_VIEW_MESSAGE && !g.notice[0] && !g.prompt && g_queued) {
        int k;

        df_copy(g.notice,g_queue[0],sizeof(g.notice)); g.notice_page = 0;
        for (k = 1; k < g_queued; k++) { df_copy(g_queue[k-1],g_queue[k],sizeof(g_queue[0])); }
        g_queued--; g.revision++;
    }
    if (g.page == OG_VIEW_MESSAGE && !g.notice[0] && !g.prompt) { og_possessions_close(); }
    if (click || in->ch || in->key) { g.revision++; }
}
