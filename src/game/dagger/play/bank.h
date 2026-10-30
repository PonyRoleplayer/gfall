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
 * Houses and ships are for sale in classic and not here yet.
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
    BANK_PACK_FULL
};

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
