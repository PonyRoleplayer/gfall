#ifndef DF_BANK_H
#define DF_BANK_H

#include "game/dagger/play/player.h"

/*
 * Classic's bank (BANK00I0.IMG) in memory: gold in and out of the region's
 * account, a letter of credit, a loan. Read docs/game.md (People, Banks).
 *
 * Each region has its own account (standing.h): Daggerfall Unity's
 * DaggerfallBankManager's rules. A letter of credit is a thing carried, worth
 * what it says, a hundredth more to write it. A loan is at least 100 and at
 * most 50000 a level, a tenth more to pay back within a year; a loan left unpaid
 * is a default, a crime against the region (bank_check_loans).
 *
 * A ship is bought here (DaggerfallBankManager): a small ship 100,000 gold or
 * a large galley 200,000, paid from the gold carried, then the account; sold
 * back for 85% into the account. One at a time. Owning one, a sea voyage on the
 * travel map pays no fare. DFU's ship to stand on is not built. Houses are
 * not for sale yet.
 *
 * Plain C: hostcheck drives it by clicks.
 */

/* What came of a transaction. */
enum {
    BANK_DONE,
    BANK_NOT_ENOUGH_GOLD,
    BANK_NOT_ENOUGH_ACCOUNT,
    BANK_TOO_HEAVY,
    BANK_LOAN_TOO_LOW,
    BANK_LOAN_TOO_HIGH,
    BANK_ALREADY_LOAN,
    BANK_DEFAULTED,
    BANK_LETTER_TOO_SMALL,
    BANK_NO_LOAN,
    BANK_PACK_FULL,
    BANK_ALREADY_SHIP,
    BANK_NO_SHIP
};

/* The ships (DFU's ShipType, one up). */
enum {
    BANK_SHIP_NONE,
    BANK_SHIP_SMALL,
    BANK_SHIP_LARGE
};
/* The ship owned, BANK_SHIP_*; the game's one, saved with it. */
int bank_ship(void);
void bank_set_ship(int ship);
/* A ship's price, and what the bank gives back for it. */
int bank_ship_price(int ship);
int bank_ship_sale(int ship);
/* Buys one, the gold carried first, then the region's account. */
int bank_buy_ship(Player *p, int region, int ship);
/* Sells the one owned into the region's account. */
int bank_sell_ship(int region);

/* The sums, without the screen. A region's number from the map; amount in gold; day clock_day. */
int bank_deposit(Player *p, int region, int amount);
int bank_withdraw(Player *p, int region, int amount);
int bank_write_letter(Player *p, int region, int amount);
int bank_deposit_letters(Player *p, int region);
int bank_borrow(Player *p, int region, int amount, int day);
int bank_repay(Player *p, int region, int amount);
/* Once a day: each loan past its day becomes a default, a crime in its region. Which region defaulted, or -1. */
int bank_check_loans(int day);

enum {
    BANK_STAY,
    BANK_LEAVE
};

int bank_open(Player *p, int region);
void bank_close(void);
void bank_compose(void);
const unsigned char *bank_pixels(void);
int bank_changed(void);
int bank_click(int x, int y);
/* A key: digits for the amount, Backspace, Enter, Escape. */
int bank_key(int vk);
const char *bank_note(void);

#endif
