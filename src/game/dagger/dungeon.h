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
    /* 0 = RDB tiles of 2048. 1 = RMB tiles of 4096. */
    int outdoor;
} Dungeon;

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

const char *dungeon_error(void);

#endif
