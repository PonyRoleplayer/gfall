#ifndef GFALL_NAMES_H
#define GFALL_NAMES_H

#include "engine/core/df.h"

/*
 * The names of people and buildings. Slice 8 of people and law (docs/gfall-people.md), from name_generate,
 * name_generate_first, name_generate_surname and namegen_read_part (src/lifted/names.c, src/hand/func_0008B65C.c),
 * npc_display_name (src/lifted/spfx.c 959) and building_name (names.c 169). The pieces come from NAMEGEN.DAT, which
 * the seam hands over as bytes (names_set_data); the words shops and taverns are named from are read out of
 * FALL.EXE (tables/namedata.h). Every name is drawn on the session's random stream seeded as the original
 * does (srand(seed), then the stream put back), so a person or a shop has the same name every time.
 */

#define NAME_MAX 48

/* NAMEGEN.DAT's bytes, kept by the caller: eleven banks of 48 bytes (six offsets, six counts), then the 10-byte pieces. */
void names_set_data(const df_u8 *bytes, df_u32 size);

/* The bank a region's people are named from (the original's table by region). */
int region_bank(int region);

/* name_generate: a given name and, in most banks, a surname. "" when the file is not loaded. */
void name_generate(int bank, int female, char *out, int cap);
/* name_generate_seeded: the same on a stream started at seed and put back after. */
void name_seeded(int bank, int female, df_u32 seed, char *out, int cap);

typedef struct NpcName {
    /* The person's record type (8 person, 53 a quest's person), faction id (0 none) and what the faction table says of it. */
    int type, faction, faction_is_individual;
    /* The individual's own name when the faction is one (type 4), else NULL. */
    const char *faction_name;
    /* The region the person is met in, the record's id and name seed, a twin (a quest's copy: named from name_seed and its own sex bit), the sex bits. */
    int region, twin;
    df_u32 id, name_seed;
    int female, twin_female;
} NpcName;

/* npc_display_name: an individual faction's name, else a generated one; 0 and "" for a thing that is no person. */
int npc_name(const NpcName *npc, char *out, int cap);

typedef struct BuildingName {
    int type, faction;
    df_u32 name_seed;
    /* The region's name (for the bank), the faction's name (a guild hall, a temple) and the temple's first child (14). */
    const char *region_name, *faction_name, *child_name;
    /* The player's house, a house for sale, and membership of the Dark Brotherhood or the Thieves Guild (a hall is then named for them). */
    int player_house, for_sale, brotherhood, thieves;
    /* A resident's surname for a quest site's residence, "" for none (a plain house is "Residence"). */
    const char *resident;
} BuildingName;

/* The words a shop or a tavern is named by hold macros ("%ef's", "%cn's Best"); the game's text expander says them. */
void names_set_expand(void (*expand)(const char *in, char *out, int cap));

/* building_name: the sign over a door. The shop and the tavern words are drawn on the stream seeded by the name seed. */
void building_name(const BuildingName *building, char *out, int cap);

#endif
