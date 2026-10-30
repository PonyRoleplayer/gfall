#ifndef DF_FACTIONFILE_H
#define DF_FACTIONFILE_H

/*
 * FACTION.TXT: Tamriel's factions, read from ARENA2 once and kept for the
 * program. Read docs/game.md (People) before editing.
 *
 * The file is a tree written by indentation: a line "#id" opens a faction,
 * as many tabs in as it is deep, and its parent is the nearest one fewer tabs
 * in above it. Under that come "key: value" lines; the file spells "type" and
 * "Type" both. A faction has at most three allies and three enemies.
 *
 * What the player has done to them is standing.c's, not this table's.
 *
 * Plain C: hostcheck runs it.
 */

/* Faction.type, by the file's own comment and DFU's FactionTypes. */
enum {
    FACTION_DAEDRA = 0,
    FACTION_GOD = 1,
    FACTION_GROUP = 2,
    FACTION_SUBGROUP = 3,
    FACTION_INDIVIDUAL = 4,
    FACTION_OFFICIAL = 5,
    FACTION_VAMPIRES = 6,
    /* A province's ruling house: the file's "noble". */
    FACTION_PROVINCE = 7,
    FACTION_WITCHES = 8,
    FACTION_TEMPLE = 9,
    FACTION_GENERIC = 12,
    FACTION_THIEVES = 13,
    FACTION_COURTS = 14,
    FACTION_PEOPLE = 15
};

/* Social groups, the file's sgroup; past these (the gods' 6) a person counts as a merchant. */
enum {
    SOCIAL_COMMONERS,
    SOCIAL_MERCHANTS,
    SOCIAL_SCHOLARS,
    SOCIAL_NOBILITY,
    SOCIAL_UNDERWORLD,
    SOCIAL_COUNT
};

#define FACTION_NAME 26
#define FACTION_LINKS 3

typedef struct Faction {
    short id;
    /* The faction over it, 0 for a root. */
    short parent;
    short type;
    /* The map's region number it belongs to, -1 for none: the file counts from 1, this from 0. */
    short region;
    short rep;
    short power;
    short flags;
    /* Its own face, -1 for none; the flats it shows as, a man's and a woman's (archive, record), 0 for none. */
    short face;
    short flat_archive[2];
    short flat_record[2];
    short sgroup;
    short ggroup;
    short ally[FACTION_LINKS];
    short enemy[FACTION_LINKS];
    char name[FACTION_NAME];
} Faction;

/* Reads the file, the first time. 0 when it does not read. */
int factionfile_open(void);
int faction_count(void);
const Faction *faction_at(int i);
/* By id; 0 for none. */
const Faction *faction_find(int id);
/* A region's "People of ...", and its court; 0 for none. */
const Faction *faction_people_of(int region);
const Faction *faction_court_of(int region);
/* A region's ruling house, whose name is the province's. */
const Faction *faction_province_of(int region);

#endif
