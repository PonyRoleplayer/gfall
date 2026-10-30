#include "game/dagger/play/bank.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/court.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/tables/items.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/clock.h"

/* See bank.h. */

#define VK_BACK_KEY 0x08
#define VK_RETURN_KEY 0x0D
#define VK_ESCAPE_KEY 0x1B
/* The smallest loan and letter; a loan's term in days (a year of 12 months of 30). */
#define LEAST 100
#define LOAN_DAYS 360
/* What a piece of gold weighs: classic's 400 to the kilogram. */
#define GOLD_WEIGHT 0.0025f
#define TYPED_MOST 9

/* Where the picture's header puts it, and DFU's rectangles inside it. */
#define PANEL_X 48
#define PANEL_Y 5
#define PANEL_W 225
#define PANEL_H 181
#define BUTTON_W 45
#define BUTTON_H 8

static const Rect g_exit_rect = { 92, 159, 40, 19 };
static const Rect g_input_rect = { 113, 146, 103, 12 };

enum {
    ASK_NOTHING,
    ASK_DEPOSIT,
    ASK_WITHDRAW,
    ASK_LETTER,
    ASK_REPAY,
    ASK_BORROW
};

/* The buttons, in DFU's order: two columns, five rows. */
enum {
    B_DEPOSIT,
    B_WITHDRAW,
    B_LETTER_IN,
    B_LETTER_OUT,
    B_REPAY,
    B_BORROW,
    B_HOUSE_BUY,
    B_HOUSE_SELL,
    B_SHIP_BUY,
    B_SHIP_SELL,
    B_COUNT
};
static const df_u8 g_button_at[B_COUNT][2] = {
    { 120, 58 }, { 172, 58 }, { 120, 76 }, { 172, 76 }, { 120, 94 }, { 172, 94 },
    { 120, 112 }, { 172, 112 }, { 120, 130 }, { 172, 130 }
};

static struct {
    int open;
    Player *player;
    int region;
    Composer composer;
    const df_u8 *picture;
    int asking;
    char typed[TYPED_MOST + 1];
    const char *note;
    int changed;
} g;

static const char *say_result(int result)
{
    switch (result) {
    case BANK_NOT_ENOUGH_GOLD:
        return "You do not have enough gold.";
    case BANK_NOT_ENOUGH_ACCOUNT:
        return "You do not have enough in your account.";
    case BANK_TOO_HEAVY:
        return "You could not carry that much gold.";
    case BANK_LOAN_TOO_LOW:
        return "We do not lend less than 100 gold.";
    case BANK_LOAN_TOO_HIGH:
        return "We could not lend you that much.";
    case BANK_ALREADY_LOAN:
        return "You already have a loan here.";
    case BANK_DEFAULTED:
        return "You have defaulted on a loan here.";
    case BANK_LETTER_TOO_SMALL:
        return "A letter must be for at least 100 gold.";
    case BANK_NO_LOAN:
        return "You have no loan here.";
    case BANK_PACK_FULL:
        return "Your pack is full.";
    default:
        return 0;
    }
}

int bank_deposit(Player *p, int region, int amount)
{
    RegionStanding *r;

    r = standing_region(region);
    if (!r || amount > (int)p->pack.gold) {
        return BANK_NOT_ENOUGH_GOLD;
    }
    r->gold += amount;
    p->pack.gold -= (df_u32)amount;
    return BANK_DONE;
}

int bank_withdraw(Player *p, int region, int amount)
{
    RegionStanding *r;

    r = standing_region(region);
    if (!r || amount > r->gold) {
        return BANK_NOT_ENOUGH_ACCOUNT;
    }
    if (inventory_weight(&p->pack) + (float)amount * GOLD_WEIGHT > rules_max_carry(p->sheet.attribute[ATTR_STRENGTH])) {
        return BANK_TOO_HEAVY;
    }
    r->gold -= amount;
    p->pack.gold += (df_u32)amount;
    return BANK_DONE;
}

int bank_write_letter(Player *p, int region, int amount)
{
    RegionStanding *r;
    Item it;

    r = standing_region(region);
    if (!r || rules_letter_cost(amount) > r->gold) {
        return BANK_NOT_ENOUGH_ACCOUNT;
    }
    if (amount < LEAST) {
        return BANK_LETTER_TOO_SMALL;
    }
    if (!item_make(&it, ITEM_LETTER, 0)) {
        return BANK_PACK_FULL;
    }
    /* A letter's worth is its own, not its template's. */
    it.value = (df_u32)amount;
    if (inventory_add(&p->pack, &it) < 0) {
        return BANK_PACK_FULL;
    }
    r->gold -= rules_letter_cost(amount);
    return BANK_DONE;
}

int bank_deposit_letters(Player *p, int region)
{
    RegionStanding *r;
    int i;

    r = standing_region(region);
    if (!r) {
        return BANK_DONE;
    }
    for (i = p->pack.count - 1; i >= 0; i--) {
        if (p->pack.item[i].template_id == ITEM_LETTER) {
            r->gold += (df_i32)p->pack.item[i].value;
            inventory_remove(&p->pack, i);
        }
    }
    return BANK_DONE;
}

int bank_borrow(Player *p, int region, int amount, int day)
{
    RegionStanding *r;

    r = standing_region(region);
    if (!r || r->defaulted) {
        return BANK_DEFAULTED;
    }
    if (r->loan > 0) {
        return BANK_ALREADY_LOAN;
    }
    if (amount < LEAST) {
        return BANK_LOAN_TOO_LOW;
    }
    if (amount > rules_max_loan(p->sheet.level)) {
        return BANK_LOAN_TOO_HIGH;
    }
    r->gold += amount;
    r->loan = rules_loan_repayment(amount);
    r->due_day = day + LOAN_DAYS;
    return BANK_DONE;
}

int bank_repay(Player *p, int region, int amount)
{
    RegionStanding *r;
    int from_gold;

    r = standing_region(region);
    if (!r || r->loan <= 0) {
        return BANK_NO_LOAN;
    }
    if (amount > r->gold + (int)p->pack.gold) {
        return BANK_NOT_ENOUGH_GOLD;
    }
    /* Paying more than is owed takes only what is owed. */
    amount = amount > r->loan ? r->loan : amount;
    r->loan -= amount;
    /* What is carried pays first. */
    from_gold = amount > (int)p->pack.gold ? (int)p->pack.gold : amount;
    p->pack.gold -= (df_u32)from_gold;
    r->gold -= amount - from_gold;
    if (r->loan <= 0) {
        r->due_day = 0;
        r->defaulted = 0;
    }
    return BANK_DONE;
}

int bank_check_loans(int day)
{
    int i;
    int which;

    which = -1;
    for (i = 0; i < STANDING_REGIONS; i++) {
        RegionStanding *r;

        r = standing_region(i);
        if (r->loan > 0 && !r->defaulted && day > r->due_day) {
            r->defaulted = 1;
            court_commit(CRIME_LOAN_DEFAULT, i);
            which = i;
        }
    }
    return which;
}

/* ---- The screen. ---- */

int bank_open(Player *p, int region)
{
    bank_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.player = p;
    g.region = region;
    g.picture = composer_read(&g.composer, "BANK00I0.IMG");
    if (!g.picture) {
        log_text(LOG_WARN, "bank: its picture did not read");
        bank_close();
        return 0;
    }
    g.changed = 1;
    return 1;
}

void bank_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

void bank_compose(void)
{
    static const Rect whole = { 0, 0, CANVAS_W, CANVAS_H };
    const RegionStanding *r;
    char line[32];

    canvas_use(&g.composer);
    canvas_box(&whole, 0);
    canvas_copy_part(g.picture, PANEL_W, 0, 0, PANEL_W, PANEL_H, PANEL_X, PANEL_Y);
    r = standing_region(g.region);
    if (r) {
        log_format(line, (int)sizeof(line), "%d", r->gold);
        canvas_label(PANEL_X + 150, PANEL_Y + 16, line);
        log_format(line, (int)sizeof(line), "%u", (unsigned)g.player->pack.gold);
        canvas_label(PANEL_X + 156, PANEL_Y + 26, line);
        if (r->loan > 0) {
            log_format(line, (int)sizeof(line), "%d", r->loan);
            canvas_label(PANEL_X + 96, PANEL_Y + 36, line);
            log_format(line, (int)sizeof(line), "day %d", r->due_day);
            canvas_label(PANEL_X + 71, PANEL_Y + 46, line);
        }
    }
    if (g.asking != ASK_NOTHING) {
        log_format(line, (int)sizeof(line), "%s_", g.typed);
        canvas_label(PANEL_X + g_input_rect.x, PANEL_Y + g_input_rect.y + 2, line);
    }
    g.changed = 0;
}

const unsigned char *bank_pixels(void)
{
    return g.composer.screen;
}

int bank_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

/* A click on button b: most ask for an amount. */
static void press(int b)
{
    const RegionStanding *r;

    r = standing_region(g.region);
    g.typed[0] = 0;
    switch (b) {
    case B_DEPOSIT:
        g.asking = ASK_DEPOSIT;
        break;
    case B_WITHDRAW:
        g.asking = ASK_WITHDRAW;
        break;
    case B_LETTER_IN:
        bank_deposit_letters(g.player, g.region);
        g.note = "Your letters are in your account.";
        break;
    case B_LETTER_OUT:
        g.asking = ASK_LETTER;
        break;
    case B_REPAY:
        if (r && r->loan > 0) {
            g.asking = ASK_REPAY;
        } else {
            g.note = say_result(BANK_NO_LOAN);
        }
        break;
    case B_BORROW:
        if (r && r->defaulted) {
            g.note = say_result(BANK_DEFAULTED);
        } else if (r && r->loan > 0) {
            g.note = say_result(BANK_ALREADY_LOAN);
        } else {
            g.asking = ASK_BORROW;
        }
        break;
    default:
        g.note = b == B_HOUSE_BUY || b == B_HOUSE_SELL ? "No houses are for sale." : "No ships are for sale.";
        break;
    }
}

int bank_click(int x, int y)
{
    int b;

    g.changed = 1;
    g.note = 0;
    x -= PANEL_X;
    y -= PANEL_Y;
    /* While an amount is asked for, only Enter or Escape goes on. */
    if (g.asking != ASK_NOTHING) {
        return BANK_STAY;
    }
    if (rect_has(&g_exit_rect, x, y)) {
        return BANK_LEAVE;
    }
    for (b = 0; b < B_COUNT; b++) {
        Rect r;

        r.x = g_button_at[b][0];
        r.y = g_button_at[b][1];
        r.w = BUTTON_W;
        r.h = BUTTON_H;
        if (rect_has(&r, x, y)) {
            press(b);
            break;
        }
    }
    return BANK_STAY;
}

/* The typed amount done. */
static void transact(void)
{
    int amount;
    int result;
    int i;

    amount = 0;
    for (i = 0; g.typed[i]; i++) {
        amount = amount * 10 + (g.typed[i] - '0');
    }
    result = BANK_DONE;
    if (amount >= 1) {
        switch (g.asking) {
        case ASK_DEPOSIT:
            result = bank_deposit(g.player, g.region, amount);
            break;
        case ASK_WITHDRAW:
            result = bank_withdraw(g.player, g.region, amount);
            break;
        case ASK_LETTER:
            result = bank_write_letter(g.player, g.region, amount);
            break;
        case ASK_REPAY:
            result = bank_repay(g.player, g.region, amount);
            break;
        case ASK_BORROW:
            result = bank_borrow(g.player, g.region, amount, clock_day());
            break;
        default:
            break;
        }
    }
    g.note = say_result(result);
    g.asking = ASK_NOTHING;
}

int bank_key(int vk)
{
    int n;

    g.changed = 1;
    if (g.asking == ASK_NOTHING) {
        return vk == VK_ESCAPE_KEY ? BANK_LEAVE : BANK_STAY;
    }
    for (n = 0; g.typed[n]; n++) {
    }
    if (vk >= '0' && vk <= '9') {
        if (n < TYPED_MOST) {
            g.typed[n] = (char)vk;
            g.typed[n + 1] = 0;
        }
    } else if (vk == VK_BACK_KEY) {
        if (n > 0) {
            g.typed[n - 1] = 0;
        }
    } else if (vk == VK_RETURN_KEY) {
        transact();
    } else if (vk == VK_ESCAPE_KEY) {
        g.asking = ASK_NOTHING;
    }
    return BANK_STAY;
}

const char *bank_note(void)
{
    return g.note;
}
