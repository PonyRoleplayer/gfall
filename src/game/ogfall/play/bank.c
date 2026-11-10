#include "game/ogfall/play/bank.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"

/* See bank.h. */

int og_bank_valid(const OgBankState *bank)
{
    int i;

    for (i = 0; i < OG_REGIONS; i++) {
        const OgBankAccount *a;

        a = &bank->account[i];
        if (a->loan_owed < 0 || (a->loan_owed == 0 && a->loan_due != 0) || (a->loan_owed != 0 && a->loan_due == 0)) {
            return 0;
        }
    }
    return bank->house_price >= 0 && bank->ship_price >= 0 && bank->ship >= 0 && bank->ship <= 2;
}

static OgBankAccount *account(int region)
{
    return region >= 0 && region < OG_REGIONS ? &og_bank()->account[region] : 0;
}

int og_gold_can_carry(int amount)
{
    return og_inventory_weight(og_state()->inventory_id) + amount / 100 <= og_inventory_capacity() * 4;
}

static int refuse(int *text, int id)
{
    *text = id;
    return OG_BANK_REFUSED;
}

int og_bank_deposit(int region, int amount, int *text)
{
    OgBankAccount *a;

    *text = 0;
    a = account(region);
    if (!a || amount < 1) {
        return OG_BANK_REFUSED;
    }
    if (og_character()->gold < amount) {
        return refuse(text, OG_BANK_TEXT_CANNOT_AFFORD);
    }
    a->balance += amount;
    og_character()->gold -= amount;
    return OG_BANK_DONE;
}

int og_bank_withdraw(int region, int amount, int confirmed, int *text)
{
    OgBankAccount *a;

    *text = 0;
    a = account(region);
    if (!a || a->balance <= 0 || amount < 1) {
        return OG_BANK_REFUSED;
    }
    if (a->balance < amount) {
        if (!confirmed) {
            *text = OG_BANK_TEXT_ASK_ALL;
            return OG_BANK_ASK;
        }
        amount = a->balance;
    }
    if (!og_gold_can_carry(amount)) {
        /* msgbox_show_string: the original's own line, not a record. */
        return refuse(text, -1);
    }
    a->balance -= amount;
    og_character()->gold += amount;
    return OG_BANK_DONE;
}

int og_bank_deposit_letters(int region, int confirmed, int *text)
{
    OgBankAccount *a;
    df_u32 ids[OG_OBJECTS];
    int n, i, taken;

    *text = 0;
    a = account(region);
    if (!a) {
        return OG_BANK_REFUSED;
    }
    if (!confirmed) {
        *text = OG_BANK_TEXT_ASK_LETTERS;
        return OG_BANK_ASK;
    }
    /* object_foreach over the player's children: every letter (group 27, index 2) carried. */
    n = og_inventory_rows(og_state()->inventory_id, -1, ids, OG_OBJECTS);
    taken = 0;
    for (i = 0; i < n && i < OG_OBJECTS; i++) {
        const OgObject *item;

        item = og_object(ids[i]);
        if (item && item->group == 27 && item->index == 2) {
            a->balance += item->value;
            og_object_delete(ids[i]);
            taken++;
        }
    }
    return OG_BANK_DONE;
}

int og_bank_letter(int region, int amount, int *text)
{
    OgBankAccount *a;
    df_u32 id;
    int fee;

    *text = 0;
    a = account(region);
    if (!a || a->balance <= 0 || amount < 1) {
        return OG_BANK_REFUSED;
    }
    if (amount < OG_LETTER_MIN) {
        return refuse(text, OG_BANK_TEXT_LETTER_MIN);
    }
    fee = amount / 100 + 1;
    if (fee + amount > a->balance) {
        return refuse(text, OG_BANK_TEXT_FEE);
    }
    id = og_item_create(og_state()->inventory_id, 27, 2, 0);
    if (!id) {
        return OG_BANK_REFUSED;
    }
    og_object_edit(id)->value = amount;
    a->balance -= fee + amount;
    return OG_BANK_DONE;
}

int og_bank_borrow(int region, int amount, int *text)
{
    OgBankAccount *a;

    *text = 0;
    a = account(region);
    if (!a) {
        return OG_BANK_REFUSED;
    }
    if (a->flags & 1) {
        return refuse(text, OG_BANK_TEXT_FLAGGED);
    }
    if (a->loan_due != 0) {
        return refuse(text, OG_BANK_TEXT_LOAN_OUT);
    }
    if (amount < 1) {
        return OG_BANK_REFUSED;
    }
    if (amount < OG_LETTER_MIN) {
        return refuse(text, OG_BANK_TEXT_LOAN_MIN);
    }
    if (og_character()->level * OG_LOAN_PER_LEVEL < amount) {
        return refuse(text, OG_BANK_TEXT_LOAN_TOO_BIG);
    }
    a->loan_due = og_state()->minutes + OG_LOAN_MINUTES;
    a->loan_owed = amount + (amount * 10) / 100;
    a->balance += amount;
    return OG_BANK_DONE;
}

int og_bank_repay(int region, int amount, int *text)
{
    OgBankAccount *a;
    int trimmed;

    *text = 0;
    a = account(region);
    if (!a || a->loan_owed == 0 || amount < 1) {
        return OG_BANK_REFUSED;
    }
    /* The balance and the gold carried (coins and letters) together must cover it (gold_total_alias). */
    if (a->balance + og_gold_total() < amount) {
        return refuse(text, OG_BANK_TEXT_CANNOT_AFFORD);
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
        og_gold_spend(-a->balance);
        a->balance = 0;
    }
    if (a->loan_owed == 0) {
        a->loan_due = 0;
    }
    /* "Only what is owed is taken" is said, and the payment made. */
    *text = trimmed ? OG_BANK_TEXT_REPAY_TRIMMED : 0;
    return OG_BANK_DONE;
}

static int sell(int region, int *price, int *owned, int ask, int confirmed, int *text)
{
    OgBankAccount *a;

    *text = 0;
    a = account(region);
    if (!a || !*owned) {
        return OG_BANK_REFUSED;
    }
    if (!confirmed) {
        *text = ask;
        return OG_BANK_ASK;
    }
    a->balance += *price - (*price * 15) / 100;
    *price = 0;
    *owned = 0;
    return OG_BANK_DONE;
}

int og_bank_sell_house(int region, int confirmed, int *text)
{
    OgBankState *b;
    int owned, r;

    b = og_bank();
    owned = b->house != 0;
    r = sell(region, &b->house_price, &owned, OG_BANK_TEXT_ASK_SELL_HOUSE, confirmed, text);
    if (r == OG_BANK_DONE) {
        b->house = 0;
    }
    return r;
}

int og_bank_sell_ship(int region, int confirmed, int *text)
{
    OgBankState *b;

    b = og_bank();
    return sell(region, &b->ship_price, &b->ship, OG_BANK_TEXT_ASK_SELL_SHIP, confirmed, text);
}

int og_gold_total(void)
{
    df_u32 ids[OG_OBJECTS];
    int n, i, total;

    total = og_character()->gold;
    n = og_inventory_rows(og_state()->inventory_id, -1, ids, OG_OBJECTS);
    for (i = 0; i < n && i < OG_OBJECTS; i++) {
        const OgObject *item;

        item = og_object(ids[i]);
        if (item && item->group == 27 && item->index == 2) {
            total += item->value;
        }
    }
    return total;
}
