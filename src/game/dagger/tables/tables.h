#ifndef GFALL_TABLES_H
#define GFALL_TABLES_H

/*
 * The character and the class rows chargen and the jump read.
 * Not FALL.EXE's 634-byte character: the fields slice 1 uses.
 * The seam fills the 18 standard CLASS*.CFG rows; row 18 is the custom
 * editor. Chargen writes the character. See docs/game.md.
 */

#define NAME_LEN 32
#define ATTRS 8
#define SKILLS 35
#define CLASSES 19

enum {
    ATTR_STR,
    ATTR_INT,
    ATTR_WIL,
    ATTR_AGI,
    ATTR_END,
    ATTR_PER,
    ATTR_SPD,
    ATTR_LUC
};

/* The player's race: 0 is Breton, the High Rock pixel (a different numbering from careers.h's RACE_*, which are record ids and start at 1; never pass one to career_race).
 * A fighter's race is a player's or a mobile id, so the original's tests of race 1, 3 and 5 meet a mobile of that id too. */
enum {
    PLAYER_RACE_BRETON,
    PLAYER_RACE_REDGUARD,
    PLAYER_RACE_NORD,
    PLAYER_RACE_DARK_ELF,
    PLAYER_RACE_HIGH_ELF,
    PLAYER_RACE_WOOD_ELF,
    PLAYER_RACE_KHAJIIT,
    PLAYER_RACE_ARGONIAN,
    PLAYER_RACES
};

/* ClassInfo.flags, CLASS*.CFG's abilities word: the bits the rules read. */
enum {
    CLASS_JUMP_BONUS = 2,
    CLASS_ADRENALINE_RUSH = 4,
    CLASS_NO_MAGICKA_REGEN = 8,
    CLASS_SUN_DAMAGE = 16,
    CLASS_HOLY_DAMAGE = 32
};

/* ClassInfo.resistance, immunity, tolerance and weakness: one bit a kind of harm (Daggerfall Unity's DFCareer.EffectFlags), as magic_resist takes them. */
enum {
    EFFECT_BIT_PARALYSIS = 1,
    EFFECT_BIT_POISON = 4,
    EFFECT_BIT_DISEASE = 64
};

typedef struct ClassInfo {
    char name[16];
    int attribute[ATTRS];
    int hp_per_level;
    /* CLASS*.CFG abilities word. Bit 2 (value 2) is the jump bonus. */
    int flags;
    int primary[3];
    int major[3];
    int minor[6];
    int resistance, immunity, tolerance, weakness;
    int absorption, rapid, regeneration, attack, expert;
    /* CLASS*.CFG byte 8: retained for the remaining advantage handlers. */
    int special_flags;
    int forbidden_materials, forbidden_equipment;
    int advancement;
} ClassInfo;

typedef struct CharacterState {
    char name[NAME_LEN];
    /* 0 is Breton, the High Rock pixel. */
    int race;
    int female;
    int klass;
    int attribute[ATTRS];
    int skill[SKILLS];
    int health;
    int max_health;
    /* Classic's fatigue counter. Its max is (STR + END) << 6. */
    int fatigue;
    int magicka;
    int max_magicka;
    int spell_points_bonus;
    int level;
    int gold;
    /* 0 very high .. 4 very low. A new character is 2, average. */
    int reflexes;
    int class_flags;
    /* Acrobat is bit 0x10000, as player_movement_update tests it. */
    int conditions;
    int face;
    /* Equipped right-hand weapon, for the ready sound. 0 for none. */
    int weapon;
    int weapon_sound;
    int reputation[10];
    int reaction;
    /* Creation gifts, retained until an inventory consumes them. */
    int arrows;
    int gifts;
    struct { int group, index, material; } gift[32];
} CharacterState;

void gfall_tables_startup(void);
void gfall_tables_shutdown(void);

void class_clear(void);
void class_set(int index, const ClassInfo *row);
const ClassInfo *class_at(int index);
/* "Breton" for 0. Empty past the eight races. */
const char *race_name(int race);
/*
 * TEXT.RSC record for the confirm box. title_menu uses
 * 2000 + D_0017B4B8[pixel - 1], and the race stored is pixel - 1.
 */
int race_text(int race);

typedef struct Special {
    const char *name;
    int count;
    const char *choice[10];
    int cost[10];
} Special;
const Special *special_state(int list, int kind);
int nearest_class(int klass);

void question_set(int number, const char *text);
const char *question_text(int number);
int question_kind(int number, int answer);
void class_answers_set(const unsigned char *triples);
int class_from_answers(const int *counts);
const char *skill_name(int skill);
int class_skill(int klass, int slot);
int magicka_state(int intelligence, int flags);
/* head_bob_update's 1000-ms cycle, the int32 table at FALL.EXE 0x18767C. */
int bob_sample(int phase_ms);

#endif
