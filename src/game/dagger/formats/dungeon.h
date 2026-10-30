#ifndef DF_DUNGEON_H
#define DF_DUNGEON_H

/* RMB names run to TEMP + letters + a 3-digit number + ".RMB". */
#define DUN_NAME_MAX 16
#define DUN_MAX_BLOCKS 64

typedef struct DunBlock {
    char name[DUN_NAME_MAX];
    int x;
    int z;
    int start;
} DunBlock;

typedef struct Dungeon {
    char title[33];
    DunBlock block[DUN_MAX_BLOCKS];
    int nblocks;
    /* LocationNum in the dungeon record, and the climate id of its map pixel. */
    int location_id;
    /*
     * The location of the outside it lies under: a dungeon's parent, when
     * it names one, else its own number; an outside's own number.
     */
    int exterior_id;
    int climate;
    /*
     * A dungeon's kind (DUN_TYPE_*, MAPTABLE's byte at +12 for its
     * location): which creatures its random markers draw from
     * (docs/entities.md). DUN_TYPE_NONE for an outside.
     */
    int dungeon_type;
    /* 0 = RDB tiles of 2048. 1 = RMB tiles of 4096. */
    int outdoor;
    /* Where the map's lists have it: dungeon_load_at's or exterior_load_at's two numbers. */
    int region;
    int index;
} Dungeon;

/* Daggerfall's dungeon kinds (DFU's DFRegion.DungeonTypes), 0..18; 255 for none. */
enum {
    DUN_TYPE_CRYPT, DUN_TYPE_ORC_STRONGHOLD, DUN_TYPE_HUMAN_STRONGHOLD, DUN_TYPE_PRISON,
    DUN_TYPE_DESECRATED_TEMPLE, DUN_TYPE_MINE, DUN_TYPE_NATURAL_CAVE, DUN_TYPE_COVEN,
    DUN_TYPE_VAMPIRE_HAUNT, DUN_TYPE_LABORATORY, DUN_TYPE_HARPY_NEST, DUN_TYPE_RUINED_CASTLE,
    DUN_TYPE_SPIDER_NEST, DUN_TYPE_GIANT_STRONGHOLD, DUN_TYPE_DRAGONS_DEN,
    DUN_TYPE_BARBARIAN_STRONGHOLD, DUN_TYPE_VOLCANIC_CAVES, DUN_TYPE_SCORPION_NEST,
    DUN_TYPE_CEMETERY,
    DUN_TYPE_NONE = 255
};

/* One place. region is the file number (MAPDITEM.017 is 17).
   index is the record inside that file. outdoor is 0 for MAPDITEM
   and 1 for MAPPITEM. Climate is filled by the load call, not the list. */
typedef struct DunRef {
    char name[33];
    int region;
    int index;
    int nblocks;
    int location_id;
    int exterior_id;
    int outdoor;
} DunRef;

/* Classic region files are MAPDITEM.000 through MAPDITEM.061. */
#define DUN_REGIONS 62

/* Privateer's Hold from MAPDITEM.017. */
int dungeon_load(Dungeon *out);

/* Every dungeon record that has a block list. out may be null to count.
   A cap that is too small still returns the full count; extra records
   are not written. Bytes read for the scan stay allocated until exit. */
int dungeon_list(DunRef *out, int cap);

/* One dungeon record, including climate. 0 leaves dungeon_error set. */
int dungeon_load_at(int region, int index, Dungeon *out);

/* Every MAPPITEM location that has a block grid. Same cap rules as
   dungeon_list. Names come from MAPNAMES, which can be 32 bytes with
   no trailing NUL. */
int exterior_list(DunRef *out, int cap);

/* One outdoor location. Blocks are RMB names on a width*height grid. */
int exterior_load_at(int region, int index, Dungeon *out);
/*
 * The two halves of a location, within one region: the index of the outside
 * whose number is exterior_id (a dungeon's Dungeon.exterior_id), and of the
 * dungeon under the town numbered location_id. -1 when the map has none.
 */
int dungeon_outside(int region, int exterior_id);
int dungeon_under(int region, int location_id);
/*
 * On: MAPS.BSA stays open and the last region's location records stay
 * read, for exterior_load_at after exterior_load_at (a streamed world).
 * Off (or on again) closes and frees them. Both on the current heap's
 * side: the archive on the current heap, the records in their own.
 */
void exterior_keep(int on);

const char *dungeon_error(void);

#endif
