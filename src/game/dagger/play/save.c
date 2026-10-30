#include "game/dagger/play/save.h"
#include "engine/scene/stream.h"
#include "game/dagger/play/pile.h"
#include "engine/assets/recfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"
#include "engine/scene/keep.h"

/* See save.h. */

#define SAVE_DIR "DATA"
#define SAVE_PREFIX "DATA\\SAVE"
/* What these chunks mean. A change that a zero in a new field cannot carry makes older saves refused. */
#define SAVE_VERSION 1

static const char *g_prefix = SAVE_PREFIX;
static int g_slot;
static char g_file[32];

/* The file of a slot, into g_file. */
static const char *file_of(int slot)
{
    log_format(g_file, (int)sizeof(g_file), "%s%d.SAV", g_prefix, slot);
    return g_file;
}

void save_use_prefix(const char *prefix)
{
    g_prefix = prefix;
}

void save_use_slot(int slot)
{
    g_slot = slot;
}

/* ---- The rows: each chunk's first field is its id, as a record file wants. ---- */

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
    float minute;
    df_i32 day;
    df_i32 day_zero;
    df_u32 seed;
    df_u32 gold;
    df_i32 worn[EQUIP_COUNT];
    df_i32 use_left;
    char name[SAVE_NAME];
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
    REC_FIELD("minute", REC_FLOAT, SaveGameRow, minute),
    REC_FIELD("day", REC_INT, SaveGameRow, day),
    REC_FIELD("day_zero", REC_INT, SaveGameRow, day_zero),
    REC_FIELD("seed", REC_UINT, SaveGameRow, seed),
    REC_FIELD("gold", REC_UINT, SaveGameRow, gold),
    REC_ARRAY("worn", REC_INT, SaveGameRow, worn),
    REC_FIELD("use_left", REC_INT, SaveGameRow, use_left),
    REC_FIELD("name", REC_TEXT, SaveGameRow, name)
};
static const RecSchema g_game_schema = {
    "GAME", "game", g_game_fields, (int)(sizeof(g_game_fields) / sizeof(g_game_fields[0])), (df_u16)sizeof(SaveGameRow), 0
};

/* CHAR: the sheet, whole. There is one row, so any integer can be its id: its race. */
#define S(name, type, member) REC_FIELD(name, type, Character, member)
#define SA(name, type, member) REC_ARRAY(name, type, Character, member)
static const RecField g_sheet_fields[] = {
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
static const RecSchema g_item_schema = {
    "PACK", "pack", g_item_fields, (int)(sizeof(g_item_fields) / sizeof(g_item_fields[0])), (df_u16)sizeof(SaveItemRow), 0
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
    game.minute = state->minute;
    game.day = state->day;
    game.day_zero = state->day_zero;
    game.seed = state->seed;
    game.gold = state->pack.gold;
    for (i = 0; i < EQUIP_COUNT; i++) {
        game.worn[i] = state->pack.worn[i];
    }
    game.use_left = state->pack.use_left;
    df_copy(game.name, state->name, SAVE_NAME);
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
    for (i = 0; i < nkeep; i++) {
        keeps[i].id = (df_u32)i;
        keeps[i].keep = *keep_at(i);
    }
    for (i = 0; i < npile; i++) {
        piles[i].id = (df_u32)i;
        piles[i].pile = *pile_at(i);
    }
    return rec_put_begin(put_file, 0) && rec_put_rows(put_file, 0, &rec_head_schema, &head, 1, 0, 0)
           && rec_put_rows(put_file, 0, &g_game_schema, &game, 1, 0, 0)
           && rec_put_rows(put_file, 0, &g_sheet_schema, &state->sheet, 1, 0, 0)
           && rec_put_rows(put_file, 0, &g_item_schema, items, state->pack.count, 0, 0)
           && rec_put_rows(put_file, 0, &g_keep_schema, keeps, nkeep, 0, 0)
           && rec_put_rows(put_file, 0, &g_pile_schema, piles, npile, 0, 0)
           && (state->sheet.klass != CLASS_CUSTOM || !career_class(CLASS_CUSTOM)
               || rec_put_rows(put_file, 0, &class_schema, career_class(CLASS_CUSTOM), 1, 0, 0));
}

int save_write(const SaveState *state)
{
    MemHeap *scratch;
    MemHeap *was;
    int ok;

    scratch = mem_heap_new();
    if (!scratch) {
        return 0;
    }
    was = mem_heap_use(scratch);
    ok = fs_make_dir(SAVE_DIR) && fs_create(file_of(g_slot)) && write_chunks(state);
    mem_heap_use(was);
    mem_heap_delete(scratch);
    if (!ok) {
        fs_abort();
        log_print(LOG_WARN, "save: slot %d could not be written", g_slot);
        return 0;
    }
    fs_finish();
    return 1;
}

/* ---- Reading. ---- */

/* Whether a slot's file is there. */
static int slot_exists(int slot)
{
    VfsFile *file;

    file = vfs_open_beside(file_of(slot));
    if (!file) {
        return 0;
    }
    vfs_close(file);
    return 1;
}

int save_exists(void)
{
    return slot_exists(g_slot);
}

int save_any(void)
{
    int slot;

    for (slot = 0; slot < SAVE_SLOTS; slot++) {
        if (slot_exists(slot)) {
            return 1;
        }
    }
    return 0;
}

/* The tables a file is merged into: kept for the process, as a record table is. */
static RecTable g_game_table;
static RecTable g_sheet_table;
static RecTable g_item_table;
static RecTable g_keep_table;
static RecTable g_pile_table;
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
        &g_head_table, &g_game_table, &g_sheet_table, &g_item_table, &g_keep_table, &g_pile_table, &g_custom_table
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
    df_copy(state->name, game->name, SAVE_NAME);
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
        || !ready_table(&g_custom_table, &class_schema, 1)) {
        return 0;
    }
    /* The bytes in a heap of its own: a file read beside the program otherwise lives until exit. */
    scratch = mem_heap_new();
    if (!scratch) {
        return 0;
    }
    was = mem_heap_use(scratch);
    bytes = vfs_read_beside(file_of(g_slot), &size);
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
        log_print(LOG_WARN, "save: slot %d is not a save this game reads", g_slot);
        return 0;
    }
    apply_tables(state);
    return 1;
}

int save_peek_name(int slot, char *out)
{
    MemHeap *scratch;
    MemHeap *was;
    const df_u8 *bytes;
    RecChunk chunk;
    df_u32 size;
    df_u32 at;
    int found;

    out[0] = 0;
    if (!slot_exists(slot) || !ready_table(&g_game_table, &g_game_schema, 1)) {
        return 0;
    }
    scratch = mem_heap_new();
    if (!scratch) {
        return 0;
    }
    was = mem_heap_use(scratch);
    bytes = vfs_read_beside(file_of(slot), &size);
    found = 0;
    if (bytes && rec_is_file(bytes, size)) {
        at = 0;
        while (rec_next(bytes, size, &at, &chunk)) {
            if (chunk.tag[0] == 'G' && chunk.tag[1] == 'A' && chunk.tag[2] == 'M' && chunk.tag[3] == 'E') {
                rec_merge(&g_game_table, &chunk, 0);
                found = g_game_table.count > 0;
            }
        }
    }
    mem_heap_use(was);
    mem_heap_delete(scratch);
    if (found) {
        df_copy(out, ((const SaveGameRow *)g_game_table.rows)->name, SAVE_NAME);
    }
    return found;
}
