#ifndef GFALL_FOEGEAR_H
#define GFALL_FOEGEAR_H

/* monster_give_equipment's tiers, kept as items on the eventual corpse.
 * See docs/game.md. Slot annotations are GFALL metadata, not DOS ABI. */
#include "game/dagger/play/combat/combat.h"
void foe_equipment(int kind, df_u32 container, df_u32 *slots, FighterInfo *fighter);
/*
 * A poisoned weapon (monster_give_equipment, moninit.c 236..255, monster_poison_weapon): once the player is level POISON_WEAPON_LEVEL, a class enemy's weapon is poisoned when 1..100 is under
 * POISON_WEAPON_ASSASSIN for the assassin and under POISON_WEAPON_OTHER for the rest, and an orc, centaur or orc sergeant's under POISON_WEAPON_OTHER; the poison is POISON_FIRST + 0..POISON_WEAPON_KINDS - 1.
 * It is kept on the weapon (flags WEAPON_POISONED and the poison in the bits from WEAPON_POISON_SHIFT) and is spent by the first blow that wounds (qkey.c 403..408: poison_apply, then the child is freed).
 */
#define POISON_WEAPON_LEVEL 2
#define POISON_WEAPON_ASSASSIN 60
#define POISON_WEAPON_OTHER 5
#define POISON_WEAPON_KINDS 8
#define WEAPON_POISONED 0x100000
#define WEAPON_POISON_SHIFT 24
#define WEAPON_POISON_MASK 0x7
/* The poison a weapon carries (an id from POISON_FIRST), 0 for none. */
int weapon_poison(df_u32 weapon);
/* The weapon's poison spent (after a blow that wounded). */
void weapon_poison_spend(df_u32 weapon);
#endif
