#include "check_bank.h"
#include "check_util.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/tables/datedefs.h"
#include "engine/core/mem.h"
#include "../apps/gfall/seam.h"
#include <stdio.h>
#include <string.h>

/* By hand, from docs/economy-math.md ("Bank, houses, ships, loans"). Region 17 is the test's bank. */
void check_bank(void)
{
    BankState *b;
    int text, r, gold, letters;
    df_u32 ids[OBJECTS];
    const int region = 17;

    b = bank_state();
    character_state()->level = 4;
    character_state()->gold = 1000;

    /* Deposit: gold moves 1:1; more than carried is refused with 454. */
    check_expect(bank_deposit(region, 400, &text) == BANK_DONE && b->account[region].balance == 400 && character_state()->gold == 600, "bank: a deposit");
    check_expect(bank_deposit(region, 900, &text) == BANK_REFUSED && text == BANK_TEXT_CANNOT_AFFORD && b->account[region].balance == 400, "bank: a deposit past the gold carried was not refused with 454");
    check_expect(bank_deposit(region, 0, &text) == BANK_REFUSED && !text, "bank: a deposit of nothing spoke");

    /* Withdraw: over the balance asks (290) and, said yes, is trimmed to the balance. */
    r = bank_withdraw(region, 1000, 0, &text);
    check_expect(r == BANK_ASK && text == BANK_TEXT_ASK_ALL && b->account[region].balance == 400, "bank: a withdrawal over the balance did not ask");
    r = bank_withdraw(region, 1000, 1, &text);
    check_expect(r == BANK_DONE && b->account[region].balance == 0 && character_state()->gold == 1000, "bank: a confirmed withdrawal was not trimmed to the balance");
    check_expect(bank_withdraw(region, 10, 0, &text) == BANK_REFUSED && !text, "bank: an empty account paid out");

    /* Letters of credit: under 100 refused (293); the fee is amount / 100 + 1 on top (292 if the balance cannot); the letter is an item of its value. */
    bank_deposit(region, 1000, &text);
    check_expect(bank_letter(region, 50, &text) == BANK_REFUSED && text == BANK_TEXT_LETTER_MIN, "bank: a letter under 100 was not refused with 293");
    check_expect(bank_letter(region, 1000, &text) == BANK_REFUSED && text == BANK_TEXT_FEE, "bank: a letter the fee could not be paid on was not refused with 292");
    check_expect(bank_letter(region, 500, &text) == BANK_DONE && b->account[region].balance == 1000 - 500 - 6, "bank: a letter of 500 did not cost 500 and a fee of 6");
    letters = inventory_rows(game_state()->inventory_id, -1, ids, OBJECTS);
    check_expect(letters > 0, "bank: no letter was made");
    {
        int i, found;

        found = 0;
        for (i = 0; i < letters; i++) {
            const ObjectInfo *it;

            it = object_at(ids[i]);
            found += it && it->group == 27 && it->index == 2 && it->value == 500;
        }
        check_expect(found == 1, "bank: the letter is not an item of group 27, index 2, worth 500");
    }
    /* Depositing letters asks (291) and puts their value in. */
    check_expect(bank_deposit_letters(region, 0, &text) == BANK_ASK && text == BANK_TEXT_ASK_LETTERS, "bank: depositing letters did not ask");
    check_expect(bank_deposit_letters(region, 1, &text) == BANK_DONE && b->account[region].balance == 494 + 500, "bank: the letter's value was not deposited");

    /* Borrowing: at least 100, up to 50000 a level; a tenth is added to what is owed, due in a game year; one loan a bank. */
    check_expect(bank_borrow(region, 50, &text) == BANK_REFUSED && text == BANK_TEXT_LOAN_MIN, "bank: a loan under 100 was not refused with 296");
    check_expect(bank_borrow(region, 4 * 50000 + 1, &text) == BANK_REFUSED && text == BANK_TEXT_LOAN_TOO_BIG, "bank: a loan over 50000 a level was not refused with 295");
    gold = b->account[region].balance;
    state_pass_minutes(1000);
    check_expect(bank_borrow(region, 2000, &text) == BANK_DONE && b->account[region].loan_owed == 2200 &&
                 b->account[region].balance == gold + 2000 && b->account[region].loan_due == game_state()->minutes + LOAN_MINUTES,
                 "bank: a loan of 2000 is not 2200 owed, due in 518400 minutes");
    check_expect(bank_borrow(region, 500, &text) == BANK_REFUSED && text == BANK_TEXT_LOAN_OUT, "bank: a second loan was not refused with 289");
    check_expect(bank_borrow(region + 1, 500, &text) == BANK_DONE, "bank: another region's bank did not lend");

    /* Repaying: the balance first; a payment over what is owed is trimmed (294) and made; clearing the debt clears the due day. */
    check_expect(bank_repay(region, 200, &text) == BANK_DONE && b->account[region].loan_owed == 2000, "bank: a repayment of 200");
    check_expect(bank_repay(region, 2500, &text) == BANK_DONE && text == BANK_TEXT_REPAY_TRIMMED && b->account[region].loan_owed == 0 &&
                 b->account[region].loan_due == 0, "bank: the rest of a loan was not trimmed, paid and cleared");
    /* The balance that falls short is made good from the gold carried. */
    b->account[region].balance = 100;
    b->account[region].loan_owed = 300;
    b->account[region].loan_due = 5;
    character_state()->gold = 500;
    check_expect(bank_repay(region, 300, &text) == BANK_DONE && b->account[region].balance == 0 && character_state()->gold == 300,
                 "bank: a repayment bigger than the balance was not made good from the gold");

    /* Selling a house or a ship: the question, then the price less 15% into the account. */
    b->house = 123;
    b->house_price = 1000;
    check_expect(bank_sell_house(region, 0, &text) == BANK_ASK && text == BANK_TEXT_ASK_SELL_HOUSE && b->house == 123, "bank: selling a house did not ask");
    b->account[region].balance = 0;
    check_expect(bank_sell_house(region, 1, &text) == BANK_DONE && b->account[region].balance == 850 && b->house == 0 && b->house_price == 0,
                 "bank: a house of 1000 did not sell for 850");
    check_expect(bank_sell_house(region, 1, &text) == BANK_REFUSED, "bank: sold a house not owned");
    b->ship = 1;
    b->ship_price = 100000;
    check_expect(bank_sell_ship(region, 1, &text) == BANK_DONE && b->account[region].balance == 850 + 85000 && b->ship == 0, "bank: a ship of 100000 did not sell for 85000");

    /* Gold that is carried has weight: a hundredth of it; past four times the capacity it is not carried. */
    check_expect(gold_can_carry(0) && !gold_can_carry(1000000), "bank: the carrying limit");
    check_expect(bank_valid(b), "bank: the state is not valid after use");
}

static void shot(const char *name)
{
    const df_u8 *pixels, *palette;
    char path[96];
    FILE *file;
    int i;

    seam_present();
    pixels = seam_screen(&palette);
    snprintf(path, sizeof(path), "build/gfall-%s.ppm", name);
    file = fopen(path, "wb");
    if (!file || !pixels || !palette) {
        if (file) {
            fclose(file);
        }
        return;
    }
    fprintf(file, "P6\n320 200\n255\n");
    for (i = 0; i < 64000; i++) {
        fwrite(palette + pixels[i] * 3, 1, 3, file);
    }
    fclose(file);
}

static void click(int cx, int cy)
{
    PlayIn in;

    mem_zero(&in, sizeof(in));
    in.cx = cx;
    in.cy = cy;
    in.on_picture = 1;
    in.mouse_pressed = 1;
    possessions_frame(&in);
}

static void key(int ch)
{
    PlayIn in;

    mem_zero(&in, sizeof(in));
    in.ch = ch;
    possessions_frame(&in);
}

static void type(const char *digits)
{
    for (; *digits; digits++) {
        key(*digits);
    }
    key(13);
}

/* The bank by clicks: the original's ten buttons, an amount typed where it asks, the questions, a message dismissed, Exit. */
void check_bank_screen(void)
{
    const Possessions *view;
    BankState *b;
    char text[64];
    const int region = 17;

    b = bank_state();
    view = possessions_state();
    possessions_reset();
    memset(b, 0, sizeof(*b));
    character_state()->gold = 700;
    character_state()->level = 2;
    check_expect(possessions_bank(region) && view->page == VIEW_BANK, "bank screen: it did not open");

    /* Deposit (166..216, 60..74): the amount is typed, Enter makes it. */
    click(190, 67);
    check_expect(view->prompt == PROMPT_AMOUNT, "bank screen: Deposit did not ask for an amount");
    type("250");
    check_expect(!view->prompt && b->account[region].balance == 250 && character_state()->gold == 450, "bank screen: a typed deposit of 250");
    /* A deposit past the gold: the 454 message, dismissed by a click. */
    click(190, 67);
    type("9000");
    check_expect(view->notice[0] && b->account[region].balance == 250, "bank screen: a deposit past the gold had no message");
    click(100, 100);
    check_expect(!view->notice[0], "bank screen: a click did not dismiss the message");

    /* Borrow (217..268, 96..110): a loan; the screen shows what is owed and its date. */
    click(240, 103);
    type("1000");
    check_expect(b->account[region].loan_owed == 1100 && b->account[region].loan_due != 0, "bank screen: a loan of 1000");
    date_text(b->account[region].loan_due, text, sizeof(text));
    check_expect(strstr(text, " the ") && strstr(text, " of "), "bank screen: the due date is not 'day the 3rd of Month'");
    shot("bank-main");

    /* Withdraw more than the balance: a question (290) and Accept; Reject changes nothing. */
    click(240, 67);
    type("99999");
    check_expect(view->prompt == PROMPT_ASK && view->offer[0], "bank screen: withdrawing past the balance did not ask");
    shot("bank-ask");
    click(203, 125);
    check_expect(!view->prompt && b->account[region].balance == 1250, "bank screen: Reject withdrew");
    click(240, 67);
    type("99999");
    click(117, 125);
    check_expect(!view->prompt && b->account[region].balance == 0, "bank screen: Accept did not withdraw it all");

    /* Houses and ships: no house for sale (287); Exit (141..179, 164..182) leaves. */
    click(190, 121);
    check_expect(view->notice[0], "bank screen: Buy House said nothing");
    click(100, 100);
    click(160, 173);
    check_expect(view->page == VIEW_NONE, "bank screen: Exit did not leave");
}
