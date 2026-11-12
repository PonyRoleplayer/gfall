#include "game/dagger/play/bank.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"

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
    return inventory_weight(game_state()->inventory_id) + amount / 100 <= inventory_capacity() * 4;
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
        if (item && item->group == 27 && item->index == 2) {
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
    fee = amount / 100 + 1;
    if (fee + amount > a->balance) {
        return refuse(text, BANK_TEXT_FEE);
    }
    id = item_create(game_state()->inventory_id, 27, 2, 0);
    if (!id) {
        return BANK_REFUSED;
    }
    object_edit(id)->value = amount;
    a->balance -= fee + amount;
    return BANK_DONE;
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
    a->loan_owed = amount + (amount * 10) / 100;
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
    a->balance += *price - (*price * 15) / 100;
    *price = 0;
    *owned = 0;
    return BANK_DONE;
}

int bank_sell_house(int region, int confirmed, int *text)
{
    BankState *b;
    int owned, r;

    b = bank_state();
    owned = b->house != 0;
    r = sell(region, &b->house_price, &owned, BANK_TEXT_ASK_SELL_HOUSE, confirmed, text);
    if (r == BANK_DONE) {
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
        if (item && item->group == 27 && item->index == 2) {
            total += item->value;
        }
    }
    return total;
}
