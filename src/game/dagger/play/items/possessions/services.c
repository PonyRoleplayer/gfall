#include "internal.h"
#include "game/dagger/play/people/bank.h"
#include "engine/core/log.h"
#include "game/dagger/play/combat/combat.h"
#include "engine/core/mem.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/people/standing.h"

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
    if (g_court_say && g_court_say(text, &possessions_view.court, out, cap) >= 0) { return; }
    df_copy(out, words_state(text), cap);
}

static void court_question(int stage, int text, const char *yes, const char *no)
{
    possessions_view.court_stage = stage;
    court_words(text, possessions_view.offer, (int)sizeof(possessions_view.offer));
    df_copy(possessions_view.yes, yes, sizeof(possessions_view.yes));
    df_copy(possessions_view.no, no, sizeof(possessions_view.no));
    possessions_view.prompt = PROMPT_ASK;
    possessions_view.revision++;
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
    possessions_view.court_stage = CS_NONE;
    possessions_close();
}

/* What the last step asks of the screen: its records one by one (a click goes on), then the next question or the end. */
static void court_show(const CourtStep *s)
{
    int i;

    g_step = *s;
    for (i = 0; i < 3; i++) { possessions_view.court_texts[i] = s->text[i]; }
    possessions_view.court_text_at = 0;
    possessions_view.court_stage = CS_TEXT;
}

static void court_next(void)
{
    char words[400];
    int text;

    while (possessions_view.court_text_at < 3 && possessions_view.court_texts[possessions_view.court_text_at]) {
        text = possessions_view.court_texts[possessions_view.court_text_at++];
        /* The accusation is the first question, not a message. */
        if (text == TEXT_COURT_ASK) { continue; }
        court_words(text, words, (int)sizeof(words));
        df_copy(possessions_view.notice, words, sizeof(possessions_view.notice));
        possessions_view.notice_page = 0;
        possessions_view.revision++;
        return;
    }
    if (g_step.done || !possessions_view.court.open) { court_finish(&g_step); return; }
    if (possessions_view.court.open) { court_question(CS_ASK, TEXT_COURT_ASK, "Guilty", "Not guilty"); }
}

/* An answer of the court's questions: 1 the first button, 0 the second. */
void possessions_court_answer(int yes)
{
    CourtStep step;
    const CharacterState *c;

    c = character_state();
    possessions_view.prompt = 0; possessions_view.offer[0] = 0;
    switch (possessions_view.court_stage) {
    case CS_SURRENDER:
        if (!yes) { possessions_view.court_stage = CS_NONE; possessions_close(); return; }
        court_guilds();
        if (law_guards_or_court(1, game_state()->region, &g_guilds, &possessions_view.court, &step) == ARREST_DEATH) {
            possessions_view.court_stage = CS_NONE; possessions_close(); player_slain(); return;
        }
        court_show(&step); court_next();
        break;
    case CS_ASK:
        if (yes) { court_accept(&possessions_view.court, &step); court_show(&step); court_next(); }
        else { court_question(CS_PLEA, TEXT_COURT_PLEA, "Polite", "Blunt"); }
        break;
    case CS_PLEA:
        /* The first button argues with Etiquette (skill 1), the second with Streetwise (skill 2): one use of it, then the roll. */
        skill_use(yes ? 1 : 2);
        court_plead(&possessions_view.court, !yes, c->skill[yes ? 1 : 2], c->attribute[5], &step);
        court_show(&step); court_next();
        break;
    default: break;
    }
}

int possessions_surrender(void)
{
    if (possessions_view.page || !possessions_open(VIEW_COURT, 0)) { return 0; }
    mem_zero(&possessions_view.court, sizeof(possessions_view.court));
    court_question(CS_SURRENDER, 15, "Yes", "No");
    return 1;
}

int possessions_arrested(void)
{
    CourtStep step;

    if (possessions_view.page && possessions_view.page != VIEW_COURT) { return 0; }
    if (!possessions_view.page && !possessions_open(VIEW_COURT, 0)) { return 0; }
    mem_zero(&possessions_view.court, sizeof(possessions_view.court));
    court_guilds();
    if (law_guards_or_court(0, game_state()->region, &g_guilds, &possessions_view.court, &step) == ARREST_DEATH) {
        possessions_close(); player_slain(); return 1;
    }
    court_show(&step); court_next();
    return 1;
}

/* ---- The bank (slice 3): the original's ten buttons (bank_buttons[0..10]), strictly inside their rectangles. ---- */
enum { BK_DEPOSIT, BK_WITHDRAW, BK_LETTERS, BK_LETTER, BK_REPAY, BK_BORROW, BK_BUY_HOUSE, BK_SELL_HOUSE, BK_BUY_SHIP, BK_SELL_SHIP, BK_EXIT, BK_NONE };

int possessions_bank(int region)
{
    if (possessions_view.page || region < 0 || region >= REGIONS || !possessions_open(VIEW_BANK, 0)) { return 0; }
    possessions_view.bank_region = region;
    possessions_view.bank_op = BK_NONE;
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
    df_copy(possessions_view.notice, text > 0 ? words_state(text) : "You cannot carry so much gold.", sizeof(possessions_view.notice));
    possessions_view.notice_page = 0;
}

/* One bank action; a question becomes the Accept/Reject box, a refusal a message, a trimmed repayment its message too. */
void possessions_bank_run(int op, int amount, int confirmed)
{
    int text, r;

    text = 0;
    switch (op) {
    case BK_DEPOSIT: r = bank_deposit(possessions_view.bank_region, amount, &text); break;
    case BK_WITHDRAW: r = bank_withdraw(possessions_view.bank_region, amount, confirmed, &text); break;
    case BK_LETTERS: r = bank_deposit_letters(possessions_view.bank_region, confirmed, &text); break;
    case BK_LETTER: r = bank_letter(possessions_view.bank_region, amount, &text); break;
    case BK_REPAY: r = bank_repay(possessions_view.bank_region, amount, &text); break;
    case BK_BORROW: r = bank_borrow(possessions_view.bank_region, amount, &text); break;
    case BK_SELL_HOUSE: r = bank_sell_house(possessions_view.bank_region, confirmed, &text); break;
    case BK_SELL_SHIP: r = bank_sell_ship(possessions_view.bank_region, confirmed, &text); break;
    default: return;
    }
    if (r == BANK_ASK) {
        df_copy(possessions_view.offer, words_state(text), sizeof(possessions_view.offer));
        possessions_view.bank_op = op;
        possessions_view.bank_amount = amount;
        possessions_view.prompt = PROMPT_ASK;
    } else if (text) { bank_say(text); }
    possessions_view.revision++;
}

/* The houses for sale and the ships, as the original lists them (rows of "Price : %ld gold"); a row picked is bought from the balance. */
static df_u32 g_house_id[HOUSES_MOST];
static int g_house_price[HOUSES_MOST];

static void house_picked(int row)
{
    int text;

    if (row < 0 || row >= HOUSES_MOST) { return; }
    bank_buy_house(possessions_view.bank_region, g_house_id[row], g_house_price[row], &text);
    if (text) { bank_say(text); }
    possessions_view.revision++;
}

static void ship_picked(int row)
{
    int text;

    if (row < 0 || row > 1) { return; }
    bank_buy_ship(possessions_view.bank_region, row == 0 ? SHIP_SMALL : SHIP_LARGE, &text);
    if (text) { bank_say(text); }
    possessions_view.revision++;
}

static void bank_buy_house_click(void)
{
    static char rows[HOUSES_MOST][LIST_NAME];
    int n, i;

    if (bank_state()->house) { bank_say(BANK_TEXT_HOUSE_OWNED); return; }
    n = bank_houses(g_house_id, g_house_price, HOUSES_MOST);
    if (!n) { bank_say(BANK_TEXT_NO_HOUSE); return; }
    for (i = 0; i < n; i++) { log_format(rows[i], LIST_NAME, "Price : %d gold", g_house_price[i]); }
    possessions_list((const char (*)[LIST_NAME])rows, n, house_picked);
}

static void bank_buy_ship_click(void)
{
    static char rows[2][LIST_NAME];

    if (bank_state()->ship) { bank_say(TEXT_SHIP_OWNED); return; }
    if (!bank_in_port()) { bank_say(TEXT_NOT_A_PORT); return; }
    log_format(rows[0], LIST_NAME, "%d", SHIP_SMALL_PRICE);
    log_format(rows[1], LIST_NAME, "%d", SHIP_LARGE_PRICE);
    possessions_list((const char (*)[LIST_NAME])rows, 2, ship_picked);
}

static void bank_click(int x, int y)
{
    int b;

    b = bank_button(x, y);
    if (b == BK_EXIT) { possessions_close(); return; }
    if (b == BK_NONE) { return; }
    if (b == BK_BUY_HOUSE) {
        bank_buy_house_click();
        return;
    }
    if (b == BK_BUY_SHIP) { bank_buy_ship_click(); return; }
    if (b == BK_LETTERS || b == BK_SELL_HOUSE || b == BK_SELL_SHIP) { possessions_bank_run(b, 0, 0); return; }
    /* The others take an amount, typed where the original asks (157, 152). */
    possessions_view.bank_op = b;
    possessions_view.prompt = PROMPT_AMOUNT;
    possessions_view.input[0] = 0;
    possessions_view.revision++;
}

/* ---- A shopkeeper's panel ---- */
static void (*g_service_act)(int what);
static int g_service_kind;

void possessions_set_service(void (*act)(int what)) { g_service_act = act; }

int possessions_service(int kind)
{
    if (possessions_view.page || kind < SERVICE_REPAIR || kind > SERVICE_COVEN || !possessions_open(VIEW_SERVICE, 0)) { return 0; }
    g_service_kind = kind;
    possessions_view.service = kind;
    return 1;
}

int possessions_guild(int member, const char *label)
{
    if (!possessions_service(SERVICE_GUILD)) { return 0; }
    possessions_view.service_member = member;
    df_copy(possessions_view.service_label, label ? label : "", sizeof(possessions_view.service_label));
    return 1;
}

/* The buttons' rectangles (repair_menu_buttons, service_menu_buttons), strictly inside; what a click asks the game to do, or -1 to leave. */
static int service_button(int x, int y)
{
    if (g_service_kind == SERVICE_REPAIR) {
        if (possessions_hit(x,y,99,51,220,59)) { return SERVICE_REPAIR_ITEM; }
        if (possessions_hit(x,y,99,60,220,68)) { return SERVICE_TALK; }
        if (possessions_hit(x,y,99,69,220,77)) { return SERVICE_SELL_ITEMS; }
    } else if (g_service_kind == SERVICE_TAVERN) {
        /* tavern_buttons: Room, Talk, Food and drinks, Goodbye, at x 99..220 from y 51, nine pixels a row. */
        if (possessions_hit(x,y,99,51,220,59)) { return SERVICE_ROOM; }
        if (possessions_hit(x,y,99,60,220,68)) { return SERVICE_TALK; }
        if (possessions_hit(x,y,99,69,220,77)) { return SERVICE_FOOD; }
        return possessions_hit(x,y,99,78,220,86) ? -1 : -2;
    } else if (g_service_kind == SERVICE_COVEN) {
        /* coven_menu_buttons: Talk, Summon, Quest, Exit. */
        if (possessions_hit(x,y,99,51,220,59)) { return SERVICE_TALK; }
        if (possessions_hit(x,y,99,60,220,68)) { return SERVICE_SUMMON; }
        if (possessions_hit(x,y,99,69,220,77)) { return SERVICE_QUEST; }
    } else if (g_service_kind == SERVICE_GUILD) {
        /* guild_menu_buttons: Join (a member cannot), Talk, the service, Exit. */
        if (possessions_hit(x,y,99,51,220,59) && !possessions_view.service_member) { return SERVICE_JOIN; }
        if (possessions_hit(x,y,99,60,220,68)) { return SERVICE_TALK; }
        if (possessions_hit(x,y,99,69,220,77)) { return SERVICE_DO; }
    } else {
        if (possessions_hit(x,y,99,50,220,68)) { return SERVICE_TALK; }
        if (possessions_hit(x,y,99,69,220,77)) { return SERVICE_DO; }
    }
    return possessions_hit(x,y,139,80,181,94) ? -1 : -2;
}

/* ---- The spell shop (spellshop_update) ---- */
int possessions_talk(const TalkWho *who, const TalkPlayer *player, int witnessed, int *text)
{
    if (possessions_view.page || !talkwin_open(who, player, witnessed, text)) { return 0; }
    if (!possessions_open(VIEW_TALK, 0)) { talkwin_close(); return 0; }
    return 1;
}

void possessions_court_notice_closed(void)
{
    if (possessions_view.court_stage == CS_TEXT) { court_next(); }
}

void possessions_service_frame(const PlayIn *in, int click)
{
    /* Exit leaves. Other buttons close the panel before handing over. */
    int what;

    what = click ? service_button(in->cx,in->cy) : -2;
    if (what != -2) {
        possessions_close();
        if (what >= 0 && g_service_act) { g_service_act(what); }
    }
}

void possessions_talk_frame(const PlayIn *in, int click)
{
    /* The window's own keys and its 19 buttons; Goodbye or Escape on the categories ends it. */
    if (in->ch == 27) { talkwin_key(TW_KEY_ESC); }
    else if (in->ch == 13) { talkwin_key(TW_KEY_ENTER); }
    else if (in->key == KEY_UP) { talkwin_key(TW_KEY_UP); }
    else if (in->key == KEY_DOWN) { talkwin_key(TW_KEY_DOWN); }
    else if (in->key == KEY_PRIOR) { talkwin_key(TW_KEY_PGUP); }
    else if (in->key == KEY_NEXT) { talkwin_key(TW_KEY_PGDN); }
    else if (click) { talkwin_click(in->cx,in->cy); }
    if (!talkwin_state()->open) { possessions_close(); }
}

void possessions_bank_frame(const PlayIn *in, int click)
{
    if (click) { bank_click(in->cx,in->cy); }
}
