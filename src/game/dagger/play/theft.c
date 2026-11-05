#include "game/dagger/play/theft.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/court.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/commerce.h"
#include "engine/core/log.h"

/* See theft.h and docs/game.md (Steal and theft). */

/* A street holds fewer people than this at once; the oldest tried is let go when it is full. */
#define TRIED_MOST 64

static unsigned g_tried[TRIED_MOST];
static int g_count;
static int g_next;

/* What a pocket held when it held nothing: ours, in the manner of UESP's list of what a pickpocket finds. */
static const char *const g_nothing[] = {
    "You find only a handkerchief.",
    "Nothing but pebbles.",
    "There is a letter, and nothing of value.",
    "Their pockets are empty."
};
#define NOTHING_LINES ((int)(sizeof(g_nothing) / sizeof(g_nothing[0])))

void theft_forget(void)
{
    g_count = 0;
    g_next = 0;
}

static int was_tried(unsigned who)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_tried[i] == who) {
            return 1;
        }
    }
    return 0;
}

static void remember(unsigned who)
{
    if (g_count < TRIED_MOST) {
        g_tried[g_count++] = who;
        return;
    }
    g_tried[g_next] = who;
    g_next = (g_next + 1) % TRIED_MOST;
}

int theft_pickpocket(Player *p, unsigned who, int level_gap, int region, int *gold)
{
    int chance;
    int found;

    *gold = 0;
    if (was_tried(who)) {
        return THEFT_TRIED;
    }
    remember(who);
    character_tally(&p->sheet, SKILL_PICKPOCKET, 1);
    chance = rules_pickpocket_chance(p->sheet.skill[SKILL_PICKPOCKET], level_gap);
    if (combat_roll(100) >= chance) {
        if (region >= 0) {
            court_note(CRIME_PICKPOCKETING, region);
        }
        return THEFT_CAUGHT;
    }
    found = rules_pickpocket_gold(combat_roll(100), combat_roll(6));
    if (found <= 0) {
        return THEFT_NOTHING;
    }
    p->pack.gold += (df_u32)found;
    *gold = found;
    return THEFT_GOLD;
}

int theft_pick_inside(Player *p, int lock)
{
    if (lock >= RULES_LOCK_MAGICAL) {
        return LOCK_MAGIC;
    }
    character_tally(&p->sheet, SKILL_LOCKPICKING, 1);
    if (combat_roll(100) >= rules_inside_lock_chance(p->sheet.skill[SKILL_LOCKPICKING], p->sheet.level, lock)) {
        return LOCK_FAILED;
    }
    return LOCK_PICKED;
}

int theft_lock_heard(const Player *p)
{
    return combat_roll(100) < rules_lock_alert_chance(p->sheet.skill[SKILL_STEALTH]);
}

void theft_words(int result, int gold, char *out, int cap)
{
    switch (result) {
    case THEFT_TRIED:
        log_format(out, cap, "You have already tried them.");
        break;
    case THEFT_GOLD:
        log_format(out, cap, gold == 1 ? "You take 1 gold piece." : "You take %d gold pieces.", gold);
        break;
    case THEFT_CAUGHT:
        log_format(out, cap, "You are not successful.");
        break;
    default:
        log_format(out, cap, "%s", g_nothing[combat_roll(NOTHING_LINES)]);
        break;
    }
}
