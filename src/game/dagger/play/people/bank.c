#include "game/dagger/play/people/bank.h"
#include "engine/core/mem.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/tables/enemies.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/items/inventory.h"

/* See bank.h. */

int bank_valid(const BankState *bank)
{
    int i;

    for (i = 0; i < REGIONS; i++) {
        const BankAccount *a;

        a = &bank->account[i];
        if (a->loan_owed < 0 || (a->loan_owed == 0 && a->loan_due != 0) || (a->loan_owed != 0 && a->loan_due == 0)) {
            return 0;
        }
    }
    return bank->house_price >= 0 && bank->ship_price >= 0 && bank->ship >= 0 && bank->ship <= 2;
}

static BankAccount *account(int region)
{
    return region >= 0 && region < REGIONS ? &bank_state()->account[region] : 0;
}

int gold_can_carry(int amount)
{
    return inventory_weight(game_state()->inventory_id) + amount / GOLD_PER_WEIGHT <= inventory_capacity() * WEIGHT_UNITS_PER_KG;
}

static int refuse(int *text, int id)
{
    *text = id;
    return BANK_REFUSED;
}

int bank_deposit(int region, int amount, int *text)
{
    BankAccount *a;

    *text = 0;
    a = account(region);
    if (!a || amount < 1) {
        return BANK_REFUSED;
    }
    if (character_state()->gold < amount) {
        return refuse(text, BANK_TEXT_CANNOT_AFFORD);
    }
    a->balance += amount;
    character_state()->gold -= amount;
    return BANK_DONE;
}

int bank_withdraw(int region, int amount, int confirmed, int *text)
{
    BankAccount *a;

    *text = 0;
    a = account(region);
    if (!a || a->balance <= 0 || amount < 1) {
        return BANK_REFUSED;
    }
    if (a->balance < amount) {
        if (!confirmed) {
            *text = BANK_TEXT_ASK_ALL;
            return BANK_ASK;
        }
        amount = a->balance;
    }
    if (!gold_can_carry(amount)) {
        /* msgbox_show_string: the original's own line, not a record. */
        return refuse(text, -1);
    }
    a->balance -= amount;
    character_state()->gold += amount;
    return BANK_DONE;
}

int bank_deposit_letters(int region, int confirmed, int *text)
{
    BankAccount *a;
    df_u32 ids[OBJECTS];
    int n, i, taken;

    *text = 0;
    a = account(region);
    if (!a) {
        return BANK_REFUSED;
    }
    if (!confirmed) {
        *text = BANK_TEXT_ASK_LETTERS;
        return BANK_ASK;
    }
    /* object_foreach over the player's children: every letter (group 27, index 2) carried. */
    n = inventory_rows(game_state()->inventory_id, -1, ids, OBJECTS);
    taken = 0;
    for (i = 0; i < n && i < OBJECTS; i++) {
        const ObjectInfo *item;

        item = object_at(ids[i]);
        if (item && item->group == ITEM_GROUP_MISC && item->index == MISC_LETTER_OF_CREDIT) {
            a->balance += item->value;
            object_delete(ids[i]);
            taken++;
        }
    }
    return BANK_DONE;
}

int bank_letter(int region, int amount, int *text)
{
    BankAccount *a;
    df_u32 id;
    int fee;

    *text = 0;
    a = account(region);
    if (!a || a->balance <= 0 || amount < 1) {
        return BANK_REFUSED;
    }
    if (amount < LETTER_MIN) {
        return refuse(text, BANK_TEXT_LETTER_MIN);
    }
    fee = amount / LETTER_FEE_DIV + 1;
    if (fee + amount > a->balance) {
        return refuse(text, BANK_TEXT_FEE);
    }
    id = item_create(game_state()->inventory_id, ITEM_GROUP_MISC, MISC_LETTER_OF_CREDIT, 0);
    if (!id) {
        return BANK_REFUSED;
    }
    object_edit(id)->value = amount;
    a->balance -= fee + amount;
    return BANK_DONE;
}

void bank_loans_due(df_u32 start, df_u32 end)
{
    int region;

    for (region = 0; region < REGIONS; region++) {
        df_u32 due;

        due = bank_state()->account[region].loan_due;
        if (due && due > start && due <= end) { region_add_legal(region, -LOAN_DUE_PENALTY); }
    }
}

int bank_collectors(df_u32 now, int region, int in_dungeon, int *kinds)
{
    static const int collector[COLLECTOR_KINDS] = { ENEMY_FIRST_CLASS + 1, ENEMY_FIRST_CLASS + 16, ENEMY_FIRST_CLASS + 2, ENEMY_FIRST_CLASS + 11 };
    static df_u32 next;
    int n, i;

    if (in_dungeon) { return 0; }
    /* Not kept in saves: after a load the first check is a day off, as an arrival in a place does in the original. */
    if (!next || next > now + COLLECTOR_CHECK_MAX) { next = now + (df_u32)random_range(COLLECTOR_CHECK_MIN, COLLECTOR_CHECK_MAX); return 0; }
    if (next > now) { return 0; }
    next = now + (df_u32)random_range(COLLECTOR_CHECK_MIN, COLLECTOR_CHECK_MAX);
    if (region < 0 || region >= REGIONS || !bank_state()->account[region].loan_due || bank_state()->account[region].loan_due >= now) { return 0; }
    n = random_range(COLLECTORS_MIN, COLLECTORS_MAX);
    for (i = 0; i < n; i++) { kinds[i] = collector[random_range(0, COLLECTOR_KINDS - 1)]; }
    return n;
}

int bank_borrow(int region, int amount, int *text)
{
    BankAccount *a;

    *text = 0;
    a = account(region);
    if (!a) {
        return BANK_REFUSED;
    }
    if (a->flags & 1) {
        return refuse(text, BANK_TEXT_FLAGGED);
    }
    if (a->loan_due != 0) {
        return refuse(text, BANK_TEXT_LOAN_OUT);
    }
    if (amount < 1) {
        return BANK_REFUSED;
    }
    if (amount < LETTER_MIN) {
        return refuse(text, BANK_TEXT_LOAN_MIN);
    }
    if (character_state()->level * LOAN_PER_LEVEL < amount) {
        return refuse(text, BANK_TEXT_LOAN_TOO_BIG);
    }
    a->loan_due = game_state()->minutes + LOAN_MINUTES;
    a->loan_owed = amount + (amount * LOAN_INTEREST_PERCENT) / PERCENT;
    a->balance += amount;
    return BANK_DONE;
}

int bank_repay(int region, int amount, int *text)
{
    BankAccount *a;
    int trimmed;

    *text = 0;
    a = account(region);
    if (!a || a->loan_owed == 0 || amount < 1) {
        return BANK_REFUSED;
    }
    /* The balance and the gold carried (coins and letters) together must cover it (gold_total_alias). */
    if (a->balance + gold_total() < amount) {
        return refuse(text, BANK_TEXT_CANNOT_AFFORD);
    }
    trimmed = 0;
    if (amount > a->loan_owed) {
        amount = a->loan_owed;
        trimmed = 1;
    }
    a->balance -= amount;
    a->loan_owed -= amount;
    if (a->balance < 0) {
        /* gold_spend of what the account could not pay, the balance back to nothing. */
        gold_spend(-a->balance);
        a->balance = 0;
    }
    if (a->loan_owed == 0) {
        a->loan_due = 0;
    }
    /* "Only what is owed is taken" is said, and the payment made. */
    *text = trimmed ? BANK_TEXT_REPAY_TRIMMED : 0;
    return BANK_DONE;
}

static int sell(int region, int *price, int *owned, int ask, int confirmed, int *text)
{
    BankAccount *a;

    *text = 0;
    a = account(region);
    if (!a || !*owned) {
        return BANK_REFUSED;
    }
    if (!confirmed) {
        *text = ask;
        return BANK_ASK;
    }
    a->balance += *price - (*price * SALE_FEE_PERCENT) / PERCENT;
    *price = 0;
    *owned = 0;
    return BANK_DONE;
}

static BankHouses g_houses;
static int (*g_port)(void);
void bank_set_houses(BankHouses houses) { g_houses = houses; }
void bank_set_port(int (*port)(void)) { g_port = port; }
int bank_houses(df_u32 *ids, int *prices, int cap) { return g_houses ? g_houses(ids, prices, cap < HOUSES_MOST ? cap : HOUSES_MOST) : 0; }
int bank_in_port(void) { return g_port ? g_port() : 0; }

static void (*g_names)(char *region, char *town, int cap);
void bank_set_names(void (*names)(char *region, char *town, int cap)) { g_names = names; }

/* A house is owned from now: its names are kept and its deed is in the pack. */
static void own_house(void)
{
    BankState *b = bank_state();

    mem_zero(b->house_region, sizeof(b->house_region)); mem_zero(b->house_town, sizeof(b->house_town));
    if (g_names) { g_names(b->house_region, b->house_town, BANK_PLACE_NAME); }
    item_create(game_state()->inventory_id, ITEM_GROUP_MISC, MISC_HOUSE_DEED, -1);
}

/* A house sold: its deed goes from the pack. */
static void disown_house(void)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *o = &game_state()->object[i];

        if (o->id && o->type == RECORD_ITEM && o->group == ITEM_GROUP_MISC && o->index == MISC_HOUSE_DEED) { object_delete(o->id); }
    }
    mem_zero(bank_state()->house_region, BANK_PLACE_NAME); mem_zero(bank_state()->house_town, BANK_PLACE_NAME);
}

int bank_buy_house(int region, df_u32 id, int price, int *text)
{
    BankAccount *a;
    BankState *b;

    *text = 0;
    b = bank_state();
    a = account(region);
    if (!a) { return BANK_REFUSED; }
    if (b->house) { *text = BANK_TEXT_HOUSE_OWNED; return BANK_REFUSED; }
    if (a->balance < price) { *text = BANK_TEXT_CANNOT_AFFORD; return BANK_REFUSED; }
    a->balance -= price;
    b->house = id;
    b->house_price = price;
    own_house();
    *text = TEXT_HOUSE_BOUGHT;
    return BANK_DONE;
}

int bank_buy_ship(int region, int kind, int *text)
{
    BankAccount *a;
    BankState *b;
    int price;

    *text = 0;
    b = bank_state();
    a = account(region);
    if (!a || (kind != SHIP_SMALL && kind != SHIP_LARGE)) { return BANK_REFUSED; }
    if (b->ship) { *text = TEXT_SHIP_OWNED; return BANK_REFUSED; }
    if (!bank_in_port()) { *text = TEXT_NOT_A_PORT; return BANK_REFUSED; }
    price = kind == SHIP_SMALL ? SHIP_SMALL_PRICE : SHIP_LARGE_PRICE;
    if (a->balance < price) { *text = BANK_TEXT_CANNOT_AFFORD; return BANK_REFUSED; }
    a->balance -= price;
    b->ship = kind;
    b->ship_price = price;
    *text = TEXT_SHIP_BOUGHT;
    return BANK_DONE;
}

int bank_gift_house(void)
{
    df_u32 ids[HOUSES_MOST];
    int prices[HOUSES_MOST], n, pick;

    n = bank_houses(ids, prices, HOUSES_MOST);
    if (!n) { return 0; }
    pick = random_range(0, n - 1);
    /* The Seneschal's house is at its list price (the original left the previous owner's price, so its sale offered 85 % of a stale one). */
    bank_state()->house = ids[pick]; bank_state()->house_price = prices[pick];
    own_house();
    return 1;
}

/* A sold house's chests and cupboards are emptied and forgotten (TEXT 298 promises it; the original left the contents in the store): every container of the house's place and what is in it. */
static void empty_house(df_u32 house)
{
    int i, j;

    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *box = &game_state()->object[i];

        if (!box->id || box->type != RECORD_CONTAINER || box->place_key != house || box->parent != WORLD_ROOT) { continue; }
        for (j = 0; j < OBJECTS; j++) {
            if (game_state()->object[j].id && game_state()->object[j].parent == box->id) { object_delete(game_state()->object[j].id); }
        }
        object_delete(box->id);
    }
}

int bank_sell_house(int region, int confirmed, int *text)
{
    BankState *b;
    int owned, r;

    b = bank_state();
    owned = b->house != 0;
    r = sell(region, &b->house_price, &owned, BANK_TEXT_ASK_SELL_HOUSE, confirmed, text);
    if (r == BANK_DONE) {
        empty_house(b->house);
        disown_house();
        b->house = 0;
    }
    return r;
}

int bank_sell_ship(int region, int confirmed, int *text)
{
    BankState *b;

    b = bank_state();
    return sell(region, &b->ship_price, &b->ship, BANK_TEXT_ASK_SELL_SHIP, confirmed, text);
}

int gold_total(void)
{
    df_u32 ids[OBJECTS];
    int n, i, total;

    total = character_state()->gold;
    n = inventory_rows(game_state()->inventory_id, -1, ids, OBJECTS);
    for (i = 0; i < n && i < OBJECTS; i++) {
        const ObjectInfo *item;

        item = object_at(ids[i]);
        if (item && item->group == ITEM_GROUP_MISC && item->index == MISC_LETTER_OF_CREDIT) {
            total += item->value;
        }
    }
    return total;
}
