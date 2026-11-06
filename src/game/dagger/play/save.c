#include "game/dagger/play/save.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/atlas.h"
#include "game/dagger/play/wagon.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/lodging.h"
#include "game/dagger/play/guild.h"
#include "game/dagger/play/quest.h"
#include "game/dagger/play/ills.h"
#include "engine/scene/automap.h"
#include "engine/scene/stream.h"
#include "game/dagger/play/pile.h"
#include "engine/assets/recfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"
#include "engine/scene/keep.h"

/* See save.h. */

/* What these chunks mean. A change that a zero in a new field cannot carry makes older saves refused. */
#define SAVE_VERSION 1
/* How much of a file's start holds the head and META, for the list: far more than they take. */
#define PEEK_BYTES 4096
/* The snapshot's rows: SAVE_THUMB_W x SAVE_THUMB_H pixels, THUMB_ROW to a row. */
#define THUMB_ROW 240
#define THUMB_ROWS ((SAVE_THUMB_W * SAVE_THUMB_H) / THUMB_ROW)

static char g_file[SAVE_PATH];

void save_use_file(const char *name)
{
    int n;

    n = 0;
    while (name[n] && n < SAVE_PATH - 1) {
        g_file[n] = name[n];
        n++;
    }
    g_file[n] = 0;
}

/* ---- The rows: each chunk's first field is its id, as a record file wants. ---- */

/* META: the one row the list reads. */
typedef struct SaveMetaRow {
    df_u16 id;
    df_u32 char_id;
    char name[SAVE_NAME];
    df_i32 level;
    df_i32 stamp[6];
    char where[SAVE_WHERE];
    df_i32 game_day;
    df_i32 game_minute;
    df_i32 has_thumb;
} SaveMetaRow;

static const RecField g_meta_fields[] = {
    REC_FIELD("id", REC_UINT, SaveMetaRow, id),
    REC_FIELD("char_id", REC_UINT, SaveMetaRow, char_id),
    REC_FIELD("name", REC_TEXT, SaveMetaRow, name),
    REC_FIELD("level", REC_INT, SaveMetaRow, level),
    REC_ARRAY("stamp", REC_INT, SaveMetaRow, stamp),
    REC_FIELD("where", REC_TEXT, SaveMetaRow, where),
    REC_FIELD("game_day", REC_INT, SaveMetaRow, game_day),
    REC_FIELD("game_minute", REC_INT, SaveMetaRow, game_minute),
    REC_FIELD("has_thumb", REC_INT, SaveMetaRow, has_thumb)
};
static const RecSchema g_meta_schema = {
    "META", "meta", g_meta_fields, (int)(sizeof(g_meta_fields) / sizeof(g_meta_fields[0])), (df_u16)sizeof(SaveMetaRow), 0
};

/* THMB: a row of the snapshot, in order from the top. */
typedef struct SaveThumbRow {
    df_u16 id;
    df_u16 px[THUMB_ROW];
} SaveThumbRow;

static const RecField g_thumb_fields[] = {
    REC_FIELD("id", REC_UINT, SaveThumbRow, id),
    REC_ARRAY("px", REC_UINT, SaveThumbRow, px)
};
static const RecSchema g_thumb_schema = {
    "THMB", "thumb", g_thumb_fields, 2, (df_u16)sizeof(SaveThumbRow), 0
};

/* GAME: the one row of what SaveState holds beside the tables. */
typedef struct SaveGameRow {
    df_u16 id;
    /* Which place (TravelWhere); a double of the way back as whole chunks and what is left, a record file having no doubles. */
    df_i32 place;
    df_i32 located;
    df_i32 region;
    df_i32 index;
    df_i32 block;
    df_i32 record;
    df_i32 has_back;
    df_i32 back_chunk[2];
    float back_rest[2];
    float back_yaw;
    df_i32 origin[2];
    float feet[3];
    float yaw;
    float pitch;
    float view_yaw;
    float view_pitch;
    float minute;
    df_i32 day;
    df_i32 day_zero;
    df_u32 seed;
    df_u32 gold;
    df_i32 worn[EQUIP_COUNT];
    df_i32 use_left;
    /* The ship owned (bank.h); a save from before has none. */
    df_i32 ship;
    /* The game minute the player last ate at a tavern (lodging.h). */
    df_i32 meal;
    /* The game minute of the last guild training (guild.h). */
    df_i32 trained;
} SaveGameRow;

static const RecField g_game_fields[] = {
    REC_FIELD("id", REC_UINT, SaveGameRow, id),
    REC_FIELD("place", REC_INT, SaveGameRow, place),
    REC_FIELD("located", REC_INT, SaveGameRow, located),
    REC_FIELD("region", REC_INT, SaveGameRow, region),
    REC_FIELD("index", REC_INT, SaveGameRow, index),
    REC_FIELD("block", REC_INT, SaveGameRow, block),
    REC_FIELD("record", REC_INT, SaveGameRow, record),
    REC_FIELD("has_back", REC_INT, SaveGameRow, has_back),
    REC_ARRAY("back_chunk", REC_INT, SaveGameRow, back_chunk),
    REC_ARRAY("back_rest", REC_FLOAT, SaveGameRow, back_rest),
    REC_FIELD("back_yaw", REC_FLOAT, SaveGameRow, back_yaw),
    REC_ARRAY("origin", REC_INT, SaveGameRow, origin),
    REC_ARRAY("feet", REC_FLOAT, SaveGameRow, feet),
    REC_FIELD("yaw", REC_FLOAT, SaveGameRow, yaw),
    REC_FIELD("pitch", REC_FLOAT, SaveGameRow, pitch),
    REC_FIELD("view_yaw", REC_FLOAT, SaveGameRow, view_yaw),
    REC_FIELD("view_pitch", REC_FLOAT, SaveGameRow, view_pitch),
    REC_FIELD("minute", REC_FLOAT, SaveGameRow, minute),
    REC_FIELD("day", REC_INT, SaveGameRow, day),
    REC_FIELD("day_zero", REC_INT, SaveGameRow, day_zero),
    REC_FIELD("seed", REC_UINT, SaveGameRow, seed),
    REC_FIELD("gold", REC_UINT, SaveGameRow, gold),
    REC_ARRAY("worn", REC_INT, SaveGameRow, worn),
    REC_FIELD("use_left", REC_INT, SaveGameRow, use_left),
    REC_FIELD("ship", REC_INT, SaveGameRow, ship),
    REC_FIELD("meal", REC_INT, SaveGameRow, meal),
    REC_FIELD("trained", REC_INT, SaveGameRow, trained)
};
static const RecSchema g_game_schema = {
    "GAME", "game", g_game_fields, (int)(sizeof(g_game_fields) / sizeof(g_game_fields[0])), (df_u16)sizeof(SaveGameRow), 0
};

/* CHAR: the sheet, whole. There is one row, so any integer can be its id: its race. */
#define S(name, type, member) REC_FIELD(name, type, Character, member)
#define SA(name, type, member) REC_ARRAY(name, type, Character, member)
static const RecField g_sheet_fields[] = {
    S("id", REC_UINT, id),
    S("race", REC_INT, race),
    S("name", REC_TEXT, name),
    S("class", REC_INT, klass),
    S("female", REC_INT, female),
    S("level", REC_INT, level),
    SA("attribute", REC_INT, attribute),
    SA("skill", REC_INT, skill),
    S("health", REC_INT, health),
    S("health_max", REC_INT, health_max),
    S("fatigue", REC_INT, fatigue),
    S("fatigue_max", REC_INT, fatigue_max),
    S("magicka", REC_INT, magicka),
    S("magicka_max", REC_INT, magicka_max),
    S("face", REC_INT, face),
    S("reflexes", REC_INT, reflexes),
    SA("skill_uses", REC_INT, skill_uses),
    S("level_sum_start", REC_INT, level_sum_start),
    S("skills_checked", REC_FLOAT, skills_checked),
    S("level_due", REC_INT, level_due),
    SA("armor", REC_INT, armor)
};
#undef S
#undef SA
static const RecSchema g_sheet_schema = {
    "CHAR", "character", g_sheet_fields, (int)(sizeof(g_sheet_fields) / sizeof(g_sheet_fields[0])), (df_u16)sizeof(Character), 0
};

/* PACK: an item and its place in the pack. */
typedef struct SaveItemRow {
    df_u16 id;
    Item item;
} SaveItemRow;

#define I(name, type, member) REC_FIELD(name, type, SaveItemRow, item.member)
static const RecField g_item_fields[] = {
    REC_FIELD("id", REC_UINT, SaveItemRow, id),
    I("template", REC_UINT, template_id),
    I("material", REC_UINT, material),
    I("variant", REC_UINT, variant),
    I("weight", REC_FLOAT, weight),
    I("value", REC_UINT, value),
    I("condition", REC_UINT, condition),
    I("condition_max", REC_UINT, condition_max),
    I("count", REC_UINT, count)
};
#undef I
/* WAGN, below: what is in the cart's wagon (wagon.h), rows as PACK's. */
static const RecSchema g_item_schema = {
    "PACK", "pack", g_item_fields, (int)(sizeof(g_item_fields) / sizeof(g_item_fields[0])), (df_u16)sizeof(SaveItemRow), 0
};
static const RecSchema g_wagon_schema = {
    "WAGN", "wagon", g_item_fields, (int)(sizeof(g_item_fields) / sizeof(g_item_fields[0])), (df_u16)sizeof(SaveItemRow), 0
};

/* KEEP: a row of the keep table, numbered as it lies. */
typedef struct SaveKeepRow {
    df_u32 id;
    Keep keep;
} SaveKeepRow;

#define K(name, type, member) REC_FIELD(name, type, SaveKeepRow, keep.member)
static const RecField g_keep_fields[] = {
    REC_FIELD("id", REC_UINT, SaveKeepRow, id),
    K("place", REC_UINT, key.place),
    K("thing", REC_UINT, key.thing),
    K("kind", REC_UINT, kind),
    K("at_end", REC_UINT, at_end),
    K("lock", REC_INT, lock),
    K("x", REC_FLOAT, x),
    K("y", REC_FLOAT, y),
    K("z", REC_FLOAT, z),
    K("yaw", REC_FLOAT, yaw),
    K("dead", REC_UINT, dead),
    K("moving", REC_UINT, moving),
    K("act", REC_UINT, act),
    K("hold", REC_UINT, hold),
    K("frame", REC_UINT, frame),
    K("placed", REC_UINT, placed),
    REC_ARRAY("game", REC_UINT, SaveKeepRow, keep.game)
};
#undef K
static const RecSchema g_keep_schema = {
    "KEEP", "keep", g_keep_fields, (int)(sizeof(g_keep_fields) / sizeof(g_keep_fields[0])), (df_u16)sizeof(SaveKeepRow), 0
};

/* PILE: one dropped item, and its pile's key. */
typedef struct SavePileRow {
    df_u32 id;
    PileItem pile;
} SavePileRow;

#define P(name, type, member) REC_FIELD(name, type, SavePileRow, pile.member)
static const RecField g_pile_fields[] = {
    REC_FIELD("id", REC_UINT, SavePileRow, id),
    P("place", REC_UINT, pile.place),
    P("thing", REC_UINT, pile.thing),
    P("template", REC_UINT, item.template_id),
    P("material", REC_UINT, item.material),
    P("variant", REC_UINT, item.variant),
    P("weight", REC_FLOAT, item.weight),
    P("value", REC_UINT, item.value),
    P("condition", REC_UINT, item.condition),
    P("condition_max", REC_UINT, item.condition_max),
    P("count", REC_UINT, item.count)
};
#undef P
/* FACT: a faction whose reputation is not the file's. SOCL: the groups' and the lasting bonus, one row. REGN: a region's law and bank. */
typedef struct SaveFactionRow {
    df_u32 id;
    df_i32 rep;
} SaveFactionRow;
static const RecField g_faction_fields[] = {
    REC_FIELD("id", REC_UINT, SaveFactionRow, id),
    REC_FIELD("rep", REC_INT, SaveFactionRow, rep)
};
static const RecSchema g_faction_schema = {
    "FACT", "faction", g_faction_fields, 2, (df_u16)sizeof(SaveFactionRow), 0
};

typedef struct SaveSocialRow {
    df_u32 id;
    df_i32 social[SOCIAL_COUNT];
    df_i32 bonus;
} SaveSocialRow;
static const RecField g_social_fields[] = {
    REC_FIELD("id", REC_UINT, SaveSocialRow, id),
    REC_ARRAY("social", REC_INT, SaveSocialRow, social),
    REC_FIELD("bonus", REC_INT, SaveSocialRow, bonus)
};
static const RecSchema g_social_schema = {
    "SOCL", "social", g_social_fields, 3, (df_u16)sizeof(SaveSocialRow), 0
};

typedef struct SaveRegionRow {
    df_u32 id;
    df_i32 legal;
    df_i32 banished;
    df_i32 defaulted;
    df_i32 gold;
    df_i32 loan;
    df_i32 due_day;
    df_i32 prices;
} SaveRegionRow;
#define R(name, member) REC_FIELD(name, REC_INT, SaveRegionRow, member)
static const RecField g_region_fields[] = {
    REC_FIELD("id", REC_UINT, SaveRegionRow, id),
    R("legal", legal),
    R("banished", banished),
    R("defaulted", defaulted),
    R("gold", gold),
    R("loan", loan),
    R("due_day", due_day),
    R("prices", prices)
};
#undef R
static const RecSchema g_region_schema = {
    "REGN", "region", g_region_fields, 8, (df_u16)sizeof(SaveRegionRow), 0
};

/* AMAP: a model the automap has seen (engine/scene/automap.h), every place's. */
typedef struct SaveSeenRow {
    df_u32 id;
    AutomapRow seen;
} SaveSeenRow;
#define A(name, type, member) REC_FIELD(name, type, SaveSeenRow, seen.member)
static const RecField g_seen_fields[] = {
    REC_FIELD("id", REC_UINT, SaveSeenRow, id),
    A("place", REC_UINT, place),
    A("mesh", REC_UINT, mesh),
    A("x", REC_INT, x),
    A("y", REC_INT, y),
    A("z", REC_INT, z)
};
#undef A
static const RecSchema g_seen_schema = {
    "AMAP", "seen", g_seen_fields, 6, (df_u16)sizeof(SaveSeenRow), 0
};

/* DISC: a word of the places known (atlas.h) that is not a new game's. */
typedef struct SaveKnownRow {
    df_u32 id;
    df_u32 bits;
} SaveKnownRow;
static const RecField g_known_fields[] = {
    REC_FIELD("id", REC_UINT, SaveKnownRow, id),
    REC_FIELD("bits", REC_UINT, SaveKnownRow, bits)
};
static const RecSchema g_known_schema = {
    "DISC", "known", g_known_fields, 2, (df_u16)sizeof(SaveKnownRow), 0
};

static const RecSchema g_pile_schema = {
    "PILE", "pile", g_pile_fields, (int)(sizeof(g_pile_fields) / sizeof(g_pile_fields[0])), (df_u16)sizeof(SavePileRow), 0
};

/* ---- Writing. ---- */

static int put_file(void *ctx, const void *src, df_u32 n)
{
    (void)ctx;
    return fs_append(src, n);
}

/* The chunks, each built in the scratch heap current. 0 when one did not write or there was no memory. */
static int write_chunks(const SaveState *state)
{
    static IllsSickRow sick[DISEASE_COUNT];
    static LodgingRoom rooms[LODGING_ROOMS];
    int nrooms;
    static GuildMembership members[GUILD_MEMBERSHIPS];
    int nmembers;
    static QuestRun runs[QUEST_SLOTS];
    int nruns;
    QuestDone done;
    IllsCurseRow curse;
    int nsick;
    SaveItemRow *wagon_rows;
    int nwagon;
    SaveSeenRow *seen;
    int nseen;
    SaveKnownRow *known;
    int nknown;
    static SaveRegionRow regions[STANDING_REGIONS];
    SaveFactionRow *factions;
    SaveSocialRow social;
    int nfaction;
    static SaveGameRow game;
    static SaveItemRow items[INVENTORY_MAX];
    SaveKeepRow *keeps;
    SavePileRow *piles;
    RecHead head;
    int nkeep;
    int npile;
    int i;

    mem_zero(&head, sizeof(head));
    head.role = REC_HEAD_SELF;
    head.size = SAVE_VERSION;
    df_copy(head.name, "SAVE0.SAV", REC_NAME);

    mem_zero(&game, sizeof(game));
    game.place = state->where.kind;
    game.located = state->where.located;
    game.region = state->where.region;
    game.index = state->where.index;
    game.block = state->where.block;
    game.record = state->where.record;
    game.has_back = state->where.has_back;
    game.back_chunk[0] = (df_i32)(state->where.back_x / (double)STREAM_CHUNK);
    game.back_chunk[1] = (df_i32)(state->where.back_z / (double)STREAM_CHUNK);
    game.back_rest[0] = (float)(state->where.back_x - (double)game.back_chunk[0] * (double)STREAM_CHUNK);
    game.back_rest[1] = (float)(state->where.back_z - (double)game.back_chunk[1] * (double)STREAM_CHUNK);
    game.back_yaw = state->where.back_yaw;
    game.origin[0] = state->origin[0];
    game.origin[1] = state->origin[1];
    for (i = 0; i < 3; i++) {
        game.feet[i] = state->feet[i];
    }
    game.yaw = state->yaw;
    game.pitch = state->pitch;
    game.view_yaw = state->view_yaw;
    game.view_pitch = state->view_pitch;
    game.minute = state->minute;
    game.day = state->day;
    game.day_zero = state->day_zero;
    game.seed = state->seed;
    game.gold = state->pack.gold;
    for (i = 0; i < EQUIP_COUNT; i++) {
        game.worn[i] = state->pack.worn[i];
    }
    game.use_left = state->pack.use_left;
    game.ship = bank_ship();
    game.meal = lodging_last_meal();
    game.trained = guild_last_training();
    for (i = 0; i < state->pack.count; i++) {
        items[i].id = (df_u16)i;
        items[i].item = state->pack.item[i];
    }

    nkeep = keep_count();
    keeps = (SaveKeepRow *)mem_alloc((df_u32)(nkeep + 1) * (df_u32)sizeof(SaveKeepRow));
    npile = pile_count();
    piles = (SavePileRow *)mem_alloc((df_u32)(npile + 1) * (df_u32)sizeof(SavePileRow));
    if (!keeps || !piles) {
        return 0;
    }
    nseen = automap_count();
    seen = (SaveSeenRow *)mem_alloc((df_u32)(nseen + 1) * (df_u32)sizeof(SaveSeenRow));
    if (!seen) {
        return 0;
    }
    for (i = 0; i < nseen; i++) {
        seen[i].id = (df_u32)i;
        seen[i].seen = *automap_at(i);
    }
    {
        const Inventory *wagon;

        wagon = wagon_contents();
        nwagon = wagon->count;
        nsick = ills_sick_rows(sick);
        quest_done_row(&done);
        nruns = 0;
        for (i = 0; i < QUEST_SLOTS; i++) {
            if (quest_run(i)) {
                runs[nruns++] = *quest_run(i);
            }
        }
        nmembers = guild_count();
        for (i = 0; i < nmembers; i++) {
            members[i] = *guild_at(i);
        }
        nrooms = lodging_count();
        for (i = 0; i < nrooms; i++) {
            rooms[i] = *lodging_at(i);
        }
        ills_curse_row(&curse);
        wagon_rows = (SaveItemRow *)mem_alloc((df_u32)(nwagon + 1) * (df_u32)sizeof(SaveItemRow));
        if (!wagon_rows) {
            return 0;
        }
        for (i = 0; i < nwagon; i++) {
            wagon_rows[i].id = (df_u16)i;
            wagon_rows[i].item = wagon->item[i];
        }
    }
    nknown = atlas_changed_count();
    known = (SaveKnownRow *)mem_alloc((df_u32)(nknown + 1) * (df_u32)sizeof(SaveKnownRow));
    if (!known) {
        return 0;
    }
    for (i = 0; i < nknown; i++) {
        int word;

        atlas_changed_at(i, &word, &known[i].bits);
        known[i].id = (df_u32)word;
    }
    nfaction = standing_changed_count();
    factions = (SaveFactionRow *)mem_alloc((df_u32)(nfaction + 1) * (df_u32)sizeof(SaveFactionRow));
    if (!factions) {
        return 0;
    }
    for (i = 0; i < nfaction; i++) {
        int faction;
        int rep;

        standing_changed_at(i, &faction, &rep);
        factions[i].id = (df_u32)faction;
        factions[i].rep = rep;
    }
    mem_zero(&social, sizeof(social));
    for (i = 0; i < SOCIAL_COUNT; i++) {
        social.social[i] = standing_socials()[i];
    }
    social.bonus = standing_reaction_bonus();
    for (i = 0; i < STANDING_REGIONS; i++) {
        const RegionStanding *r;

        r = standing_region(i);
        regions[i].id = (df_u32)i;
        regions[i].legal = r->legal;
        regions[i].banished = r->banished;
        regions[i].defaulted = r->defaulted;
        regions[i].gold = r->gold;
        regions[i].loan = r->loan;
        regions[i].due_day = r->due_day;
        regions[i].prices = r->prices;
    }
    for (i = 0; i < nkeep; i++) {
        keeps[i].id = (df_u32)i;
        keeps[i].keep = *keep_at(i);
    }
    for (i = 0; i < npile; i++) {
        piles[i].id = (df_u32)i;
        piles[i].pile = *pile_at(i);
    }
    /* Who and when, then the snapshot: first, so the list reads only the start of the file. */
    {
        FsTime now;
        static SaveMetaRow meta;
        static SaveThumbRow thumb[THUMB_ROWS];

        fs_local_time(&now);
        mem_zero(&meta, sizeof(meta));
        meta.char_id = state->sheet.id;
        df_copy(meta.name, state->sheet.name, SAVE_NAME);
        meta.level = state->sheet.level;
        meta.stamp[0] = now.year;
        meta.stamp[1] = now.month;
        meta.stamp[2] = now.day;
        meta.stamp[3] = now.hour;
        meta.stamp[4] = now.minute;
        meta.stamp[5] = now.second;
        df_copy(meta.where, state->where_words, SAVE_WHERE);
        meta.game_day = state->day;
        meta.game_minute = df_ifloor(state->minute);
        meta.has_thumb = state->thumb != 0;
        if (state->thumb) {
            for (i = 0; i < THUMB_ROWS; i++) {
                thumb[i].id = (df_u16)i;
                mem_copy(thumb[i].px, state->thumb + (df_u32)i * THUMB_ROW, THUMB_ROW * sizeof(df_u16));
            }
        }
        if (!rec_put_begin(put_file, 0) || !rec_put_rows(put_file, 0, &rec_head_schema, &head, 1, 0, 0)
            || !rec_put_rows(put_file, 0, &g_meta_schema, &meta, 1, 0, 0)
            || (state->thumb && !rec_put_rows(put_file, 0, &g_thumb_schema, thumb, THUMB_ROWS, 0, 0))) {
            return 0;
        }
    }
    return rec_put_rows(put_file, 0, &g_game_schema, &game, 1, 0, 0)
           && rec_put_rows(put_file, 0, &g_sheet_schema, &state->sheet, 1, 0, 0)
           && rec_put_rows(put_file, 0, &g_item_schema, items, state->pack.count, 0, 0)
           && rec_put_rows(put_file, 0, &g_keep_schema, keeps, nkeep, 0, 0)
           && rec_put_rows(put_file, 0, &g_pile_schema, piles, npile, 0, 0)
           && rec_put_rows(put_file, 0, &g_faction_schema, factions, nfaction, 0, 0)
           && rec_put_rows(put_file, 0, &g_social_schema, &social, 1, 0, 0)
           && rec_put_rows(put_file, 0, &g_region_schema, regions, STANDING_REGIONS, 0, 0)
           && rec_put_rows(put_file, 0, &g_known_schema, known, nknown, 0, 0)
           && rec_put_rows(put_file, 0, &g_wagon_schema, wagon_rows, nwagon, 0, 0)
           && rec_put_rows(put_file, 0, &g_seen_schema, seen, nseen, 0, 0)
           && rec_put_rows(put_file, 0, &ills_sick_schema, sick, nsick, 0, 0)
           && rec_put_rows(put_file, 0, &ills_curse_schema, &curse, 1, 0, 0)
           && rec_put_rows(put_file, 0, &lodging_schema, rooms, nrooms, 0, 0)
           && rec_put_rows(put_file, 0, &guild_membership_schema, members, nmembers, 0, 0)
           && rec_put_rows(put_file, 0, &quest_run_schema, runs, nruns, 0, 0)
           && rec_put_rows(put_file, 0, &quest_done_schema, &done, 1, 0, 0)
           && (state->sheet.klass != CLASS_CUSTOM || !career_class(CLASS_CUSTOM)
               || rec_put_rows(put_file, 0, &class_schema, career_class(CLASS_CUSTOM), 1, 0, 0));
}

/* The folder part of a path: everything before the last backslash, into out (SAVE_PATH); 0 when there is none. */
static int folder_of(const char *path, char *out)
{
    int last;
    int i;

    last = -1;
    for (i = 0; path[i]; i++) {
        if (path[i] == '\\') {
            last = i;
        }
    }
    if (last <= 0) {
        return 0;
    }
    for (i = 0; i < last; i++) {
        out[i] = path[i];
    }
    out[last] = 0;
    return 1;
}

int save_write(const SaveState *state)
{
    MemHeap *scratch;
    MemHeap *was;
    char folder[SAVE_PATH];
    char temp[SAVE_PATH + 8];
    int ok;

    scratch = mem_heap_new();
    if (!scratch) {
        return 0;
    }
    /* Into a temporary name first, and moved over the real one when it is whole. */
    log_format(temp, (int)sizeof(temp), "%s.TMP", g_file);
    was = mem_heap_use(scratch);
    ok = (!folder_of(g_file, folder) || fs_make_dirs(folder)) && fs_create(temp) && write_chunks(state);
    mem_heap_use(was);
    mem_heap_delete(scratch);
    if (!ok) {
        fs_abort();
        log_print(LOG_WARN, "save: %s could not be written", g_file);
        return 0;
    }
    fs_finish();
    if (!fs_move(temp, g_file)) {
        fs_delete(temp);
        log_print(LOG_WARN, "save: %s could not be moved into place", g_file);
        return 0;
    }
    return 1;
}

/* ---- Reading. ---- */

/* Whether a slot's file is there. */
int save_exists(void)
{
    return fs_exists(g_file);
}

/* The tables a file is merged into: kept for the process, as a record table is. */
static RecTable g_game_table;
static RecTable g_sheet_table;
static RecTable g_item_table;
static RecTable g_keep_table;
static RecTable g_pile_table;
static RecTable g_faction_table;
static RecTable g_social_table;
static RecTable g_region_table;
static RecTable g_known_table;
static RecTable g_wagon_table;
static RecTable g_seen_table;
static RecTable g_sick_table;
static RecTable g_room_table;
static RecTable g_member_table;
static RecTable g_quest_table;
static RecTable g_done_table;
static RecTable g_curse_table;
static RecTable g_head_table;
/* A custom class, when the save's character has one: the same record as a class of the list. */
static RecTable g_custom_table;

/* A table ready for a file: made the first time, emptied after. 0 with no memory. */
static int ready_table(RecTable *t, const RecSchema *s, int cap)
{
    if (!t->schema && !rec_table_init(t, s, cap)) {
        return 0;
    }
    rec_table_clear(t);
    return 1;
}

/* A chunk into the table of its tag. */
static void merge_chunk(const RecChunk *c)
{
    static RecTable *const tables[] = {
        &g_head_table, &g_game_table, &g_sheet_table, &g_item_table, &g_keep_table, &g_pile_table, &g_faction_table, &g_social_table, &g_region_table, &g_known_table, &g_seen_table, &g_wagon_table, &g_sick_table, &g_curse_table, &g_room_table, &g_member_table, &g_quest_table, &g_done_table, &g_custom_table
    };
    int i;

    for (i = 0; i < (int)(sizeof(tables) / sizeof(tables[0])); i++) {
        const char *tag;

        tag = tables[i]->schema->tag;
        if (c->tag[0] == tag[0] && c->tag[1] == tag[1] && c->tag[2] == tag[2] && c->tag[3] == tag[3]) {
            rec_merge(tables[i], c, 0);
            return;
        }
    }
}

/* What the merged tables hold into state and the live tables. */
static void apply_tables(SaveState *state)
{
    const SaveGameRow *game;
    int i;

    game = (const SaveGameRow *)g_game_table.rows;
    mem_zero(state, sizeof(*state));
    state->where.kind = game->place;
    state->where.located = game->located;
    state->where.region = game->region;
    state->where.index = game->index;
    state->where.block = game->block;
    state->where.record = game->record;
    state->where.has_back = game->has_back;
    state->where.back_x = (double)game->back_chunk[0] * (double)STREAM_CHUNK + (double)game->back_rest[0];
    state->where.back_z = (double)game->back_chunk[1] * (double)STREAM_CHUNK + (double)game->back_rest[1];
    state->where.back_yaw = game->back_yaw;
    state->origin[0] = game->origin[0];
    state->origin[1] = game->origin[1];
    for (i = 0; i < 3; i++) {
        state->feet[i] = game->feet[i];
    }
    state->yaw = game->yaw;
    state->pitch = game->pitch;
    state->view_yaw = game->view_yaw;
    state->view_pitch = game->view_pitch;
    state->minute = game->minute;
    state->day = game->day;
    state->day_zero = game->day_zero;
    state->seed = game->seed;
    state->sheet = *(const Character *)g_sheet_table.rows;
    inventory_clear(&state->pack);
    state->pack.gold = game->gold;
    for (i = 0; i < g_item_table.count; i++) {
        inventory_add(&state->pack, &((const SaveItemRow *)g_item_table.rows)[i].item);
    }
    for (i = 0; i < EQUIP_COUNT; i++) {
        state->pack.worn[i] = game->worn[i];
    }
    state->pack.use_left = game->use_left;
    bank_set_ship(game->ship);
    if (g_custom_table.count) {
        career_set_custom((const CareerClass *)g_custom_table.rows);
    }
    keep_clear();
    for (i = 0; i < g_keep_table.count; i++) {
        keep_put(&((const SaveKeepRow *)g_keep_table.rows)[i].keep);
    }
    pile_new_game();
    for (i = 0; i < g_pile_table.count; i++) {
        pile_put(&((const SavePileRow *)g_pile_table.rows)[i].pile);
    }
    /* The wagon: empty, then what was in it. A save from before has nothing in it. */
    wagon_new_game();
    for (i = 0; i < g_wagon_table.count; i++) {
        inventory_add(wagon_contents(), &((const SaveItemRow *)g_wagon_table.rows)[i].item);
    }
    /* What ails the player: nothing, then the saved state. A save from before has none. */
    ills_new_game();
    for (i = 0; i < g_sick_table.count; i++) {
        ills_put_sick(&((const IllsSickRow *)g_sick_table.rows)[i]);
    }
    if (g_curse_table.count) {
        ills_put_curse((const IllsCurseRow *)g_curse_table.rows);
    }
    /* The rooms held: none, then the saved ones. */
    lodging_new_game();
    for (i = 0; i < g_room_table.count; i++) {
        lodging_put(&((const LodgingRoom *)g_room_table.rows)[i]);
    }
    /* A save from before has a zero here: a meal long ago enough not to matter. */
    lodging_set_last_meal(game->meal);
    /* The guilds belonged to: none, then the saved ones; the last training, long ago in a save from before. */
    guild_new_game();
    for (i = 0; i < g_member_table.count; i++) {
        guild_put(&((const GuildMembership *)g_member_table.rows)[i]);
    }
    guild_set_last_training(game->trained);
    /* The quests running: none, then the saved ones, their logic read again from ARENA2. */
    quest_new_game();
    if (g_done_table.count) {
        quest_put_done((const QuestDone *)g_done_table.rows);
    }
    for (i = 0; i < g_quest_table.count; i++) {
        quest_put(&((const QuestRun *)g_quest_table.rows)[i]);
    }
    /* The automap's seen models: none, then the saved ones. A save from before has none. */
    automap_clear();
    for (i = 0; i < g_seen_table.count; i++) {
        automap_put(&((const SaveSeenRow *)g_seen_table.rows)[i].seen);
    }
    /* The places known: a new game's, the saved words over it. A save from before P10 has none. */
    atlas_new_game();
    for (i = 0; i < g_known_table.count; i++) {
        atlas_put((int)((const SaveKnownRow *)g_known_table.rows)[i].id, ((const SaveKnownRow *)g_known_table.rows)[i].bits);
    }
    /* What the world thinks: a new game's, the saved rows over it. A save from before P9 has none, and is that. */
    standing_new_game(state->seed);
    for (i = 0; i < g_faction_table.count; i++) {
        const SaveFactionRow *row;

        row = &((const SaveFactionRow *)g_faction_table.rows)[i];
        standing_put_rep((int)row->id, row->rep);
    }
    if (g_social_table.count) {
        const SaveSocialRow *row;
        short social[SOCIAL_COUNT];

        row = (const SaveSocialRow *)g_social_table.rows;
        for (i = 0; i < SOCIAL_COUNT; i++) {
            social[i] = (short)row->social[i];
        }
        standing_put_socials(social, row->bonus);
    }
    for (i = 0; i < g_region_table.count; i++) {
        const SaveRegionRow *row;
        RegionStanding *r;

        row = &((const SaveRegionRow *)g_region_table.rows)[i];
        r = standing_region((int)row->id);
        if (r) {
            r->legal = (short)row->legal;
            r->banished = (df_u8)row->banished;
            r->defaulted = (df_u8)row->defaulted;
            r->gold = row->gold;
            r->loan = row->loan;
            r->due_day = row->due_day;
            r->prices = (short)row->prices;
        }
    }
}

int save_read(SaveState *state)
{
    MemHeap *scratch;
    MemHeap *was;
    const df_u8 *bytes;
    const RecHead *head;
    RecChunk chunk;
    df_u32 size;
    df_u32 at;
    int ok;

    if (!ready_table(&g_head_table, &rec_head_schema, 1) || !ready_table(&g_game_table, &g_game_schema, 1)
        || !ready_table(&g_sheet_table, &g_sheet_schema, 1) || !ready_table(&g_item_table, &g_item_schema, INVENTORY_MAX)
        || !ready_table(&g_keep_table, &g_keep_schema, 256) || !ready_table(&g_pile_table, &g_pile_schema, PILE_ITEMS)
        || !ready_table(&g_custom_table, &class_schema, 1) || !ready_table(&g_faction_table, &g_faction_schema, 64)
        || !ready_table(&g_social_table, &g_social_schema, 1) || !ready_table(&g_region_table, &g_region_schema, STANDING_REGIONS)
        || !ready_table(&g_known_table, &g_known_schema, 64) || !ready_table(&g_seen_table, &g_seen_schema, 1024)
        || !ready_table(&g_wagon_table, &g_wagon_schema, INVENTORY_MAX) || !ready_table(&g_sick_table, &ills_sick_schema, DISEASE_COUNT)
        || !ready_table(&g_curse_table, &ills_curse_schema, 1) || !ready_table(&g_room_table, &lodging_schema, LODGING_ROOMS)
        || !ready_table(&g_member_table, &guild_membership_schema, GUILD_MEMBERSHIPS)
        || !ready_table(&g_quest_table, &quest_run_schema, QUEST_SLOTS) || !ready_table(&g_done_table, &quest_done_schema, 1)) {
        return 0;
    }
    /* The bytes in a heap of its own: a file read beside the program otherwise lives until exit. */
    scratch = mem_heap_new();
    if (!scratch) {
        return 0;
    }
    was = mem_heap_use(scratch);
    bytes = vfs_read_beside(g_file, &size);
    ok = bytes && rec_is_file(bytes, size);
    if (ok) {
        at = 0;
        while (rec_next(bytes, size, &at, &chunk)) {
            merge_chunk(&chunk);
        }
    }
    mem_heap_use(was);
    mem_heap_delete(scratch);
    head = (const RecHead *)g_head_table.rows;
    if (!ok || !g_head_table.count || head->size != SAVE_VERSION || !g_game_table.count || !g_sheet_table.count) {
        log_print(LOG_WARN, "save: %s is not a save this game reads", g_file);
        return 0;
    }
    apply_tables(state);
    return 1;
}

/* The first bytes of a file beside the program into a buffer; its length, 0 when it will not open. */
static df_u32 read_start(const char *name, df_u8 *buf, df_u32 cap)
{
    FsFile *file;
    df_u32 size;

    file = fs_open_beside(name);
    if (!file) {
        return 0;
    }
    size = fs_size(file);
    if (size > cap) {
        size = cap;
    }
    if (!fs_read_at(file, 0, buf, size)) {
        size = 0;
    }
    fs_close(file);
    return size;
}

int save_peek(const char *name, SaveMeta *out)
{
    static df_u8 buf[PEEK_BYTES];
    static RecTable meta_table;
    RecChunk chunk;
    const SaveMetaRow *row;
    df_u32 size;
    df_u32 at;

    size = read_start(name, buf, sizeof(buf));
    if (size < 16 || !rec_is_file(buf, size) || !ready_table(&meta_table, &g_meta_schema, 1)) {
        return 0;
    }
    at = 0;
    while (rec_next(buf, size, &at, &chunk)) {
        if (chunk.tag[0] == 'M' && chunk.tag[1] == 'E' && chunk.tag[2] == 'T' && chunk.tag[3] == 'A') {
            rec_merge(&meta_table, &chunk, 0);
            break;
        }
    }
    if (!meta_table.count) {
        return 0;
    }
    row = (const SaveMetaRow *)meta_table.rows;
    mem_zero(out, sizeof(*out));
    out->char_id = row->char_id;
    df_copy(out->name, row->name, SAVE_NAME);
    out->level = row->level;
    out->year = row->stamp[0];
    out->month = row->stamp[1];
    out->day = row->stamp[2];
    out->hour = row->stamp[3];
    out->minute = row->stamp[4];
    out->second = row->stamp[5];
    df_copy(out->where, row->where, SAVE_WHERE);
    out->game_day = row->game_day;
    out->game_minute = row->game_minute;
    out->has_thumb = row->has_thumb;
    return 1;
}

int save_peek_thumb(const char *name, df_u16 *out)
{
    static RecTable thumb_table;
    MemHeap *scratch;
    MemHeap *was;
    df_u8 *buf;
    RecChunk chunk;
    df_u32 cap;
    df_u32 size;
    df_u32 at;
    int i;

    cap = PEEK_BYTES + (df_u32)THUMB_ROWS * (df_u32)(sizeof(SaveThumbRow) + 8) + 64u;
    if (!ready_table(&thumb_table, &g_thumb_schema, THUMB_ROWS)) {
        return 0;
    }
    scratch = mem_heap_new();
    if (!scratch) {
        return 0;
    }
    was = mem_heap_use(scratch);
    buf = (df_u8 *)mem_alloc(cap);
    size = buf ? read_start(name, buf, cap) : 0;
    if (size >= 16 && rec_is_file(buf, size)) {
        at = 0;
        while (rec_next(buf, size, &at, &chunk)) {
            if (chunk.tag[0] == 'T' && chunk.tag[1] == 'H' && chunk.tag[2] == 'M' && chunk.tag[3] == 'B') {
                rec_merge(&thumb_table, &chunk, 0);
                break;
            }
        }
    }
    mem_heap_use(was);
    mem_heap_delete(scratch);
    if (thumb_table.count != THUMB_ROWS) {
        return 0;
    }
    for (i = 0; i < THUMB_ROWS; i++) {
        const SaveThumbRow *row;

        row = (const SaveThumbRow *)(thumb_table.rows + (df_u32)i * (df_u32)sizeof(SaveThumbRow));
        if (row->id >= THUMB_ROWS) {
            return 0;
        }
        mem_copy(out + (df_u32)row->id * THUMB_ROW, row->px, THUMB_ROW * sizeof(df_u16));
    }
    return 1;
}
