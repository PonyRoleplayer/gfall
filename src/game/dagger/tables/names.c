#include "game/dagger/tables/names.h"
#include "game/dagger/tables/careers.h"

/* See names.h and docs/data.md. */

static const RecField g_name_fields[] = {
    REC_FIELD("id", REC_UINT, NamePiece, id),
    REC_FIELD("bank", REC_UINT, NamePiece, bank),
    REC_FIELD("set", REC_UINT, NamePiece, set),
    REC_FIELD("text", REC_TEXT, NamePiece, text)
};
const RecSchema name_schema = {
    "NAME", "name", g_name_fields, (int)(sizeof(g_name_fields) / sizeof(g_name_fields[0])), (df_u16)sizeof(NamePiece), 0
};

static RecTable g_names;

RecTable *name_table(void)
{
    if (!g_names.schema) {
        rec_table_init(&g_names, &name_schema, 1024);
    }
    return &g_names;
}

int name_bank_of_race(int race)
{
    switch (race) {
    case RACE_REDGUARD:
        return NAME_BANK_REDGUARD;
    case RACE_NORD:
        return NAME_BANK_NORD;
    case RACE_DARK_ELF:
        return NAME_BANK_DARK_ELF;
    case RACE_HIGH_ELF:
        return NAME_BANK_HIGH_ELF;
    case RACE_WOOD_ELF:
        return NAME_BANK_WOOD_ELF;
    case RACE_KHAJIIT:
        return NAME_BANK_KHAJIIT;
    case RACE_ARGONIAN:
        return NAME_BANK_IMPERIAL;
    default:
        return NAME_BANK_BRETON;
    }
}

/* FALL.EXE's region races (DFU's MapsFile.RegionRaces): each of the 62 regions' people, 0 Breton, 1 Redguard. */
static const unsigned char g_region_races[] = {
    1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1,
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 1
};

int name_bank_of_region(int region)
{
    if (region < 0 || region >= (int)sizeof(g_region_races)) {
        return NAME_BANK_BRETON;
    }
    return g_region_races[region] ? NAME_BANK_REDGUARD : NAME_BANK_BRETON;
}

/* The seed's next number (an LCG of its own: names must not move the game's dice). */
static unsigned next(unsigned *seed)
{
    *seed = *seed * 1103515245u + 12345u;
    return (*seed >> 16) & 0x7fff;
}

/* How many pieces the set has: the rows from place 0 on, until one is missing. */
static int pieces_in(int bank, int set)
{
    int n;

    for (n = 0; n < NAME_PER_SET && rec_row(name_table(), bank * 512 + set * NAME_PER_SET + n); n++) {
    }
    return n;
}

/* A piece of the set at random onto out; 0 when the set is empty. */
static int add_piece(int bank, int set, unsigned *seed, char *out, int cap, int *at)
{
    const NamePiece *p;
    int n;
    int k;

    n = pieces_in(bank, set);
    if (!n) {
        return 0;
    }
    p = (const NamePiece *)rec_row(name_table(), bank * 512 + set * NAME_PER_SET + (int)(next(seed) % (unsigned)n));
    for (k = 0; p && p->text[k] && *at < cap - 1; k++) {
        out[(*at)++] = p->text[k];
    }
    out[*at] = 0;
    return 1;
}

static void add_text(const char *text, char *out, int cap, int *at)
{
    while (*text && *at < cap - 1) {
        out[(*at)++] = *text++;
    }
    out[*at] = 0;
}

void name_make(int bank, int female, unsigned seed, char *out, int cap)
{
    int at;

    at = 0;
    if (cap <= 0) {
        return;
    }
    out[0] = 0;
    /* Some calls in so neighbouring seeds part ways at once. */
    next(&seed);
    next(&seed);
    if (bank == NAME_BANK_REDGUARD) {
        /* One name: 0 + 1 + 2, and 3 for a man three times in four, 4 for a woman. */
        add_piece(bank, 0, &seed, out, cap, &at);
        add_piece(bank, 1, &seed, out, cap, &at);
        add_piece(bank, 2, &seed, out, cap, &at);
        if (female) {
            add_piece(bank, 4, &seed, out, cap, &at);
        } else if (next(&seed) % 100 < 75) {
            add_piece(bank, 3, &seed, out, cap, &at);
        }
        return;
    }
    add_piece(bank, female ? 2 : 0, &seed, out, cap, &at);
    add_piece(bank, female ? 3 : 1, &seed, out, cap, &at);
    if (bank == NAME_BANK_NORD) {
        /* A Nord's surname: a father's name and "sen". */
        add_text(" ", out, cap, &at);
        add_piece(bank, 0, &seed, out, cap, &at);
        add_piece(bank, 1, &seed, out, cap, &at);
        add_text("sen", out, cap, &at);
        return;
    }
    if (pieces_in(bank, 4) && pieces_in(bank, 5)) {
        add_text(" ", out, cap, &at);
        add_piece(bank, 4, &seed, out, cap, &at);
        add_piece(bank, 5, &seed, out, cap, &at);
    }
}
