#include "game/dagger/enemies.h"
#include "game/dagger/dungeon.h"

/* See enemies.h and docs/entities.md. */

/* Mobile id 99 in a fixed marker: no creature (DFU skips it). */
#define INVALID_KIND 99
/* A marker's action byte for one that attacks only when attacked (DFU's EnemyReactionTypes). */
#define PASSIVE 99

/* What a kind is: its archives, and how it stands and moves. */
#define K_IDLE 1
#define K_FLY 2
#define K_SPECTRAL 4
#define K_AQUATIC 8

typedef struct Kind {
    df_u16 male;
    df_u16 female;
    df_u8 traits;
} Kind;

/*
 * EnemyBasics' table: monsters 0..42 (39, the horse, has no pictures:
 * 294 is missing), then the classes 128..146, by mobile id. A monster's
 * two archives are the same.
 */
static const Kind g_monster[43] = {
    { 255, 255, K_IDLE },              /* 0 rat */
    { 256, 256, K_FLY },               /* 1 imp */
    { 257, 257, K_IDLE },              /* 2 spriggan */
    { 258, 258, K_FLY },               /* 3 giant bat */
    { 259, 259, K_IDLE },              /* 4 grizzly bear */
    { 260, 260, K_IDLE },              /* 5 sabertooth tiger */
    { 261, 261, K_IDLE },              /* 6 spider */
    { 262, 262, K_IDLE },              /* 7 orc */
    { 263, 263, K_IDLE },              /* 8 centaur */
    { 264, 264, 0 },                   /* 9 werewolf */
    { 265, 265, K_IDLE },              /* 10 nymph */
    { 266, 266, K_AQUATIC },           /* 11 slaughterfish */
    { 267, 267, K_IDLE },              /* 12 orc sergeant */
    { 268, 268, K_FLY },               /* 13 harpy */
    { 269, 269, 0 },                   /* 14 wereboar */
    { 270, 270, K_IDLE },              /* 15 skeletal warrior */
    { 271, 271, K_IDLE },              /* 16 giant */
    { 272, 272, K_IDLE },              /* 17 zombie */
    { 273, 273, K_SPECTRAL },          /* 18 ghost */
    { 274, 274, K_IDLE },              /* 19 mummy */
    { 275, 275, 0 },                   /* 20 giant scorpion */
    { 276, 276, K_IDLE },              /* 21 orc shaman */
    { 277, 277, K_IDLE },              /* 22 gargoyle */
    { 278, 278, K_SPECTRAL },          /* 23 wraith */
    { 279, 279, K_IDLE },              /* 24 orc warlord */
    { 280, 280, K_IDLE },              /* 25 frost daedra */
    { 281, 281, K_IDLE },              /* 26 fire daedra */
    { 282, 282, K_IDLE },              /* 27 daedroth */
    { 283, 283, K_IDLE },              /* 28 vampire */
    { 284, 284, K_IDLE },              /* 29 daedra seducer */
    { 285, 285, K_IDLE },              /* 30 vampire ancient */
    { 286, 286, K_IDLE },              /* 31 daedra lord */
    { 287, 287, K_IDLE },              /* 32 lich */
    { 288, 288, K_IDLE },              /* 33 ancient lich */
    { 289, 289, K_FLY },               /* 34 dragonling */
    { 290, 290, K_IDLE },              /* 35 fire atronach */
    { 291, 291, K_IDLE },              /* 36 iron atronach */
    { 292, 292, K_IDLE },              /* 37 flesh atronach */
    { 293, 293, K_IDLE },              /* 38 ice atronach */
    { 0, 0, 0 },                       /* 39 horse: none */
    { 295, 295, K_FLY },               /* 40 dragonling, the other */
    { 296, 296, K_AQUATIC },           /* 41 dreugh */
    { 297, 297, K_AQUATIC }            /* 42 lamia */
};

/* Classes 128..146: a man's archive, a woman's; all have standing pictures. */
static const Kind g_class[19] = {
    { 486, 485, K_IDLE },              /* 128 mage */
    { 476, 475, K_IDLE },              /* 129 spellsword */
    { 490, 489, K_IDLE },              /* 130 battlemage */
    { 478, 477, K_IDLE },              /* 131 sorcerer */
    { 486, 485, K_IDLE },              /* 132 healer */
    { 490, 489, K_IDLE },              /* 133 nightblade */
    { 484, 483, K_IDLE },              /* 134 bard */
    { 484, 483, K_IDLE },              /* 135 burglar */
    { 480, 479, K_IDLE },              /* 136 rogue */
    { 484, 483, K_IDLE },              /* 137 acrobat */
    { 484, 483, K_IDLE },              /* 138 thief */
    { 480, 479, K_IDLE },              /* 139 assassin */
    { 488, 487, K_IDLE },              /* 140 monk */
    { 482, 481, K_IDLE },              /* 141 archer */
    { 482, 481, K_IDLE },              /* 142 ranger */
    { 488, 487, K_IDLE },              /* 143 barbarian */
    { 488, 487, K_IDLE },              /* 144 warrior */
    { 488, 487, K_IDLE },              /* 145 knight */
    { 399, 399, K_IDLE }               /* 146 city watch */
};

/*
 * RandomEncounters' tables for dungeons, by DUN_TYPE_*, then the
 * underwater one (19). Twenty kinds each, weakest first; the cemetery
 * has eleven.
 */
#define TABLE_LEN 20
#define UNDERWATER 19
static const df_u8 g_table[20][TABLE_LEN] = {
    /* crypt */
    { 0, 15, 3, 0, 15, 3, 19, 15, 6, 17, 18, 17, 17, 18, 18, 23, 23, 28, 30, 32 },
    /* orc stronghold */
    { 0, 3, 7, 12, 4, 7, 15, 7, 12, 5, 16, 21, 6, 21, 24, 20, 27, 20, 21, 24 },
    /* human stronghold */
    { 144, 136, 0, 4, 141, 133, 3, 129, 8, 145, 144, 136, 143, 24, 141, 23, 129, 21, 144, 28 },
    /* prison */
    { 3, 0, 134, 15, 135, 6, 133, 6, 143, 138, 37, 138, 139, 17, 133, 36, 23, 18, 17, 135 },
    /* desecrated temple */
    { 3, 1, 132, 140, 131, 15, 13, 140, 19, 140, 21, 22, 23, 27, 29, 140, 25, 26, 34, 31 },
    /* mine */
    { 0, 3, 4, 6, 15, 6, 133, 7, 12, 16, 138, 16, 144, 38, 36, 26, 138, 28, 133, 30 },
    /* natural cave */
    { 0, 3, 4, 5, 4, 6, 7, 9, 143, 13, 14, 16, 20, 21, 18, 144, 140, 143, 34, 32 },
    /* coven */
    { 1, 3, 128, 131, 13, 133, 10, 37, 35, 36, 10, 27, 131, 129, 29, 26, 130, 128, 25, 31 },
    /* vampire haunt */
    { 0, 0, 3, 6, 9, 133, 15, 15, 18, 19, 23, 18, 28, 23, 28, 28, 30, 28, 30, 30 },
    /* laboratory */
    { 1, 128, 131, 130, 17, 128, 131, 37, 130, 38, 35, 36, 22, 27, 131, 32, 128, 131, 32, 33 },
    /* harpy nest */
    { 0, 144, 3, 13, 3, 13, 15, 6, 135, 13, 20, 21, 133, 13, 136, 28, 29, 13, 29, 31 },
    /* ruined castle */
    { 0, 3, 144, 7, 15, 6, 12, 9, 145, 14, 17, 16, 145, 18, 23, 144, 145, 32, 33, 30 },
    /* spider nest */
    { 0, 3, 3, 6, 15, 2, 142, 6, 20, 136, 13, 6, 138, 18, 19, 6, 23, 6, 139, 32 },
    /* giant stronghold */
    { 0, 4, 5, 7, 16, 12, 20, 16, 14, 16, 24, 20, 21, 16, 27, 26, 34, 16, 24, 25 },
    /* dragon's den */
    { 0, 3, 8, 135, 20, 9, 145, 13, 6, 22, 131, 10, 133, 128, 145, 28, 130, 138, 33, 31 },
    /* barbarian stronghold */
    { 144, 143, 0, 3, 144, 8, 136, 141, 9, 143, 141, 14, 6, 144, 28, 136, 143, 30, 131, 33 },
    /* volcanic caves */
    { 3, 1, 5, 5, 143, 35, 13, 14, 16, 18, 20, 36, 35, 128, 36, 23, 27, 26, 31, 33 },
    /* scorpion nest */
    { 0, 3, 6, 15, 20, 138, 20, 17, 1, 18, 20, 22, 23, 132, 20, 133, 29, 20, 27, 31 },
    /* cemetery: eleven */
    { 0, 15, 3, 19, 6, 17, 18, 23, 28, 30, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* underwater */
    { 11, 11, 15, 11, 11, 41, 41, 11, 17, 41, 11, 38, 11, 41, 42, 41, 42, 17, 23, 18 }
};

static int table_len(int t)
{
    return t == DUN_TYPE_CEMETERY ? 11 : TABLE_LEN;
}

static const Kind *kind_of(int kind)
{
    if (kind >= 0 && kind < 43) {
        return g_monster[kind].male ? &g_monster[kind] : 0;
    }
    if (kind >= ENEMY_FIRST_CLASS && kind < ENEMY_KINDS) {
        return &g_class[kind - ENEMY_FIRST_CLASS];
    }
    return 0;
}

int enemy_archive(int kind, int female)
{
    const Kind *k;

    k = kind_of(kind);
    if (!k) {
        return -1;
    }
    return (int)(female ? k->female : k->male);
}

int enemy_has_idle(int kind)
{
    const Kind *k;

    k = kind_of(kind);
    return k && (k->traits & K_IDLE) != 0;
}

int enemy_flies(int kind)
{
    const Kind *k;

    k = kind_of(kind);
    return k && (k->traits & K_FLY) != 0;
}

int enemy_spectral(int kind)
{
    const Kind *k;

    k = kind_of(kind);
    return k && (k->traits & K_SPECTRAL) != 0;
}

int enemy_aquatic(int kind)
{
    const Kind *k;

    k = kind_of(kind);
    return k && (k->traits & K_AQUATIC) != 0;
}

/*
 * Classic's rand (Watcom's, DFU's DFRandom): the high 15 bits of an LCG.
 * The same as world.c's for the face archives; one dungeon seeds it
 * with its location id.
 */
static df_u32 g_rng;

static int classic_rand(void)
{
    g_rng = g_rng * 0x41C64E6Du + 0x3039u;
    return (int)((g_rng >> 16) & 0x7fffu);
}

static int range_inclusive(int lo, int hi)
{
    return classic_rand() % (hi - lo + 1) + lo;
}

/* The draws, by a random marker's slot (its flags, 1..255; 0 picks one of 1..6). */
static df_u8 g_dry[256];
static df_u8 g_wet[256];
static int g_level = 1;

/*
 * ChooseRandomEnemyType: four times in five a kind near the player's
 * level (three either side); else from the table's start up to a
 * step past it; one time in twenty, a low-level player, two past. At
 * level 1, always one of the first six.
 */
static int draw_kind(int t)
{
    int lo;
    int hi;
    int roll;
    int len;

    roll = range_inclusive(1, 100);
    lo = 0;
    if (roll > 95 && g_level <= 5) {
        hi = g_level + 2;
    } else if (roll > 80) {
        hi = g_level + 1;
    } else {
        lo = g_level - 3;
        hi = g_level + 3;
    }
    if (lo < 0) {
        lo = 0;
        hi = 5;
    } else if (hi > 19) {
        lo = 14;
        hi = 19;
    }
    /* Not classic (it read past the cemetery's eleven): kept within the table. */
    len = table_len(t);
    if (hi > len - 1) {
        hi = len - 1;
        lo = lo > hi ? hi : lo;
    }
    return (int)g_table[t][range_inclusive(lo, hi)];
}

void enemy_dungeon(int location_id, int dungeon_type, int level)
{
    int t;
    int i;

    g_level = level < 1 ? 1 : level;
    g_rng = (df_u32)location_id;
    t = dungeon_type >= 0 && dungeon_type <= DUN_TYPE_CEMETERY ? dungeon_type : -1;
    for (i = 0; i < 256; i++) {
        /* A dungeon of no known kind: none at its random markers. */
        g_dry[i] = (df_u8)(t >= 0 ? draw_kind(t) : INVALID_KIND);
    }
    for (i = 0; i < 256; i++) {
        g_wet[i] = (df_u8)draw_kind(UNDERWATER);
    }
}

int enemy_water(const Marker *marks, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (marks[i].record == 10) {
            int sound;

            /* The start marker's sound byte: 8 units a step down, 0 for none. */
            sound = (int)(marks[i].id >> 8);
            return sound ? -8 * sound : ENEMY_DRY;
        }
    }
    return ENEMY_DRY;
}

int enemy_at(const Marker *mk, int water, int *female, int *passive)
{
    int kind;
    int file_y;
    int gendered;

    *female = 0;
    *passive = 0;
    /* Classic's height: down is positive. */
    file_y = -(int)mk->y;
    gendered = 0;
    if (mk->record == ENEMY_FIXED_MARKER) {
        /* The low byte; the high one is garbage in Daggerfall's files. */
        kind = (int)(mk->id & 0xffu);
        if (kind == INVALID_KIND) {
            return -1;
        }
        gendered = 1;
    } else if (mk->record == ENEMY_RANDOM_MARKER) {
        int slot;

        slot = (int)(mk->flags & 0xffu);
        if (slot == 0) {
            slot = range_inclusive(1, 6);
        }
        /* Below the water's surface (a greater y is lower): the underwater draw. */
        kind = water < file_y ? (int)g_wet[slot] : (int)g_dry[slot];
    } else {
        return -1;
    }
    if (enemy_archive(kind, 0) < 0) {
        return -1;
    }
    /* A water dweller needs water, its marker no more than 20 above the surface. */
    if (enemy_aquatic(kind) && (water == ENEMY_DRY || water - 20 > file_y)) {
        return -1;
    }
    /*
     * A class's gender: a fixed marker's flags (1 a woman, 2 a man); a
     * random one's, or a fixed one that names neither, a coin.
     */
    if (kind >= ENEMY_FIRST_CLASS) {
        if (gendered && (mk->flags & 2u)) {
            *female = 0;
        } else if (gendered && (mk->flags & 1u)) {
            *female = 1;
        } else {
            *female = classic_rand() & 1;
        }
    }
    *passive = mk->action == PASSIVE;
    return kind;
}
