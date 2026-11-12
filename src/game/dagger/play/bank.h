#ifndef GFALL_BANK_H
#define GFALL_BANK_H

#include "engine/core/df.h"
#include "game/dagger/play/standing.h"

/*
 * The regional banks: an account a region (balance, a loan and when it is due), letters of credit,
 * the player's house and ship. Slice 3 of people and law (docs/game-people.md). The rules are the
 * decomp's, src/lifted/bank.c 218..410 and src/hand/func_0006BF30.c, with its notes in
 * docs/economy-math.md ("Bank, houses, ships, loans", "Carrying gold"). Pure rules over the
 * session; the screen asks them, so a screen and a test do the same things.
 *
 * Every action says how it went in the original's own way: 1 done, 0 refused (with the TEXT.RSC
 * record the box shows, or 0 for silence), and -1 for a question the player has to answer yes first
 * (the record to ask in *text); the call is made again with confirmed set.
 */

#define BANK_DONE 1
#define BANK_REFUSED 0
#define BANK_ASK (-1)

/* Records the original shows (msgbox_show_rsc / bank_confirm). */
enum {
    BANK_TEXT_HOUSE_OWNED = 286,   /* no house to buy: you own one */
    BANK_TEXT_NO_HOUSE = 287,      /* none for sale here */
    BANK_TEXT_FLAGGED = 288,       /* the account cannot borrow */
    BANK_TEXT_LOAN_OUT = 289,      /* a loan is out already */
    BANK_TEXT_ASK_ALL = 290,       /* withdraw all of it? */
    BANK_TEXT_ASK_LETTERS = 291,   /* deposit the letters of credit? */
    BANK_TEXT_FEE = 292,           /* not the balance for the fee */
    BANK_TEXT_LETTER_MIN = 293,    /* a letter is 100 or more */
    BANK_TEXT_REPAY_TRIMMED = 294, /* only what is owed is taken */
    BANK_TEXT_LOAN_TOO_BIG = 295,
    BANK_TEXT_LOAN_MIN = 296,
    BANK_TEXT_ASK_SELL_HOUSE = 298,
    BANK_TEXT_ASK_SELL_SHIP = 299,
    BANK_TEXT_CANNOT_AFFORD = 454
};

typedef struct BankAccount {
    int balance, loan_owed;
    df_u32 loan_due;
    int flags;
} BankAccount;

/* What the player owns that the bank deals in. A price of 0 is none owned; the ship is its kind, 0 for none. */
typedef struct BankState {
    BankAccount account[REGIONS];
    df_u32 house;
    int house_price;
    int ship, ship_price;
} BankState;

BankState *bank_state(void);
int bank_valid(const BankState *bank);

/* The loan's term (518400 minutes, a game year) and its 10% added at once; a loan is up to 50000 a level. */
#define LOAN_MINUTES 518400u
#define LOAN_PER_LEVEL 50000
/* A letter of credit is at least 100; its fee is a hundredth of it, and 1. */
#define LETTER_MIN 100
/* A sale pays the price less 15%. */
#define SALE_PERCENT 85

/* bank_deposit_gold / bank_withdraw_gold: gold carried to the account and back. A withdrawal over the balance asks (290) and is trimmed to it. */
int bank_deposit(int region, int amount, int *text);
int bank_withdraw(int region, int amount, int confirmed, int *text);
/* bank_deposit_letters_of_credit: asks (291), then every letter carried goes into the account. How many letters. */
int bank_deposit_letters(int region, int confirmed, int *text);
/* bank_withdraw_letter_of_credit: a letter made of amount, costing it and its fee. */
int bank_letter(int region, int amount, int *text);
/* bank_borrow and bank_repay_loan. */
int bank_borrow(int region, int amount, int *text);
int bank_repay(int region, int amount, int *text);
/* bank_sell_house and bank_sell_ship: the price less 15% into the account, after the question (298, 299). */
int bank_sell_house(int region, int confirmed, int *text);
int bank_sell_ship(int region, int confirmed, int *text);
/* gold_total: the coins carried and every letter of credit's value, what the bank shows as the player's gold. */
int gold_total(void);
/* gold_spend (law.h) takes coins and letters; the bank asks it to make good a short balance. */
void gold_spend(int amount);
/* gold_can_carry: whether that much more gold fits (the pack's weight and a hundredth of it, against four times the capacity). */
int gold_can_carry(int amount);

#endif
