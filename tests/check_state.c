/* A buffer for what quest_pack makes in these tests (the save itself sizes its own). */
#define TEST_QUEST_BUFFER 65536
#include "check_state.h"
#include "check_shelf.h"
#include "check_market.h"
#include "check_bank.h"
#include "check_lodging.h"
#include "check_law.h"
#include "check_talk.h"
#include "check_steal.h"
#include "check_names.h"
#include "check_guild.h"
#include "check_potion.h"
#include "check_spellshop.h"
#include "check_magicitem.h"
#include "game/dagger/play/quests/quest.h"
#include "check_quest.h"
#include "check_entry.h"
#include "check_talkwin.h"
#include "game/dagger/play/people/standing.h"
#include "check_util.h"
#include "game/dagger/play/travel/worldstate.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"
#include "engine/scene/clock.h"
#include "game/dagger/play/ui/options.h"
#include "game/dagger/play/save/save.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/magic/spellbook.h"
#include "engine/scene/keep.h"
#include "engine/scene/automap.h"
#include <string.h>
#include <signal.h>
#include <sys/resource.h>

static GameState g_initial;
static GameState g_saved;
static GameState g_bad;
static int g_minutes_seen;
static int g_minute_order;
static df_u32 g_previous_minute;

static void observe_minute(const QuestEvent *event)
{
    if (event->kind != EVENT_MINUTE) {
        return;
    }
    if ((df_u32)event->value != g_previous_minute + 1) {
        g_minute_order = 0;
    }
    g_previous_minute = (df_u32)event->value;
    g_minutes_seen++;
}

static void check_random(void)
{
    static const int expected[] = { 16838, 5758, 10113, 17515, 31051, 5627 };
    int i;

    random_seed(1);
    for (i = 0; i < 6; i++) {
        check_expect(random_range(0, 32767) == expected[i], "Watcom rand reference sequence");
    }
    random_seed(0);
    check_expect(random_range(0, 32767) == 0, "Watcom srand accepts zero");
    g_saved = *game_state();
    i = random_range(-5, 5);
    check_expect(state_restore(&g_saved), "restore the random stream");
    check_expect(random_range(-5, 5) == i, "restoring state replays the next random draw");
    state_restore(&g_initial);
}

static void check_objects(void)
{
    df_u32 a, b, id;
    int n;

    check_expect(state_valid(game_state()), "chargen builds a valid session");
    check_expect(object_at(game_state()->eye_id)->parent == game_state()->move_id,
                 "the eye belongs to the movement record");
    a = object_new(51, game_state()->inventory_id, 9, 100);
    b = item_create(a, 3, 5, 0);
    check_expect(a && b && a != b, "objects have distinct stable IDs");
    check_expect(!object_reparent(a, b), "ownership cannot become a cycle");
    check_expect(!object_delete(a), "a container with children cannot be deleted");
    check_expect(!object_reparent(b, 999u), "an owner must exist");
    check_expect(!object_delete(game_state()->entity_id), "mandatory player records survive deletion");
    check_expect(object_reparent(b, WORLD_ROOT), "an item can leave the player's container");
    check_expect(object_delete(a), "an empty container can be deleted");
    check_expect(object_at(b)->parent == WORLD_ROOT, "moving ownership preserves the item's identity");
    g_saved = *game_state();
    g_saved.next_low = 62999;
    check_expect(state_restore(&g_saved), "restore an ID counter at the wrap boundary");
    id = object_new(2, WORLD_ROOT, 0, 100);
    check_expect((id & 65535u) == 62999u, "last original low ID is 62999");
    id = object_new(2, PLAYER_ROOT, 0, 100);
    check_expect((id & 65535u) >= 10000u && id != game_state()->move_id && id != b,
                 "wrapped IDs avoid existing IDs in both roots");
    n = 0;
    while (object_new(2, WORLD_ROOT, 0, 101)) {
        n++;
    }
    check_expect(n > 0 && n < OBJECTS, "the object pool has a bounded capacity");
    check_expect(object_at(b) != 0, "pool exhaustion does not overwrite an existing object");
    state_restore(&g_initial);
}

static void rejects_snapshot(const char *reason)
{
    g_saved = *game_state();
    check_expect(!state_restore(&g_bad), reason);
    check_expect(memcmp(&g_saved, game_state(), sizeof(g_saved)) == 0,
                 "an invalid snapshot leaves the live session untouched");
}

static void check_validation(void)
{
    g_bad = g_initial;
    g_bad.version++;
    rejects_snapshot("unknown snapshot version refused");
    g_bad = g_initial;
    g_bad.object[1].id = g_bad.object[0].id;
    rejects_snapshot("duplicate object ID refused");
    g_bad = g_initial;
    g_bad.object[2].parent = 0x12345678u;
    rejects_snapshot("missing parent refused");
    g_bad = g_initial;
    g_bad.object[2].parent = g_bad.object[3].id;
    rejects_snapshot("cyclic ownership refused");
    g_bad = g_initial;
    g_bad.inventory_id = g_bad.eye_id;
    rejects_snapshot("an inventory ID cannot name an eye");
    g_bad = g_initial;
    g_bad.depth = MODES + 1;
    rejects_snapshot("an oversized modal stack is refused");
}

static void check_time(void)
{
    df_u32 start;
    int i, year, month, day, weekday;

    start = game_state()->minutes;
    state_date(&year, &month, &day, &weekday);
    check_expect(year == 401 && month == 0 && day == 3 && weekday == 6,
                 "calendar derives the original epoch and continuous weekday");
    check_expect(start == 523530u, "new-game clock uses the original initial value");
    state_advance_ms(4943);
    check_expect(game_state()->minutes == start, "89 BIOS ticks do not advance a game minute");
    state_advance_ms(1);
    check_expect(game_state()->minutes == start + 1, "90 BIOS ticks advance one game minute");
    options_open();
    options_open();
    check_expect(game_state()->mode == GAME_OPTIONS && game_state()->depth == 1,
                 "opening one panel twice owns only one modal frame");
    g_saved = *game_state();
    state_frame(0.1f);
    state_advance_ms(60000);
    check_expect(memcmp(&g_saved, game_state(), sizeof(g_saved)) == 0,
                 "menus pause minutes and their fractional tick remainder");
    options_close();
    check_expect(game_state()->mode == GAME_WORLD && !game_state()->depth,
                 "closing Options restores the prior game mode");
    for (i = 0; i < MODES; i++) {
        check_expect(state_mode_push(GAME_INVENTORY), "bounded modal stack pushes");
    }
    check_expect(!state_mode_push(GAME_SHEET), "modal stack overflow is refused");
    for (i = 0; i < MODES; i++) {
        check_expect(state_mode_pop(), "modal stack restores nested modes");
    }
    check_expect(!state_mode_pop(), "modal stack underflow is refused");
    g_previous_minute = game_state()->minutes;
    g_minutes_seen = 0;
    g_minute_order = 1;
    state_observe(observe_minute);
    state_pass_minutes(61);
    check_expect(g_minute_order && g_minutes_seen == 61,
                 "elapsed-minute notifications retain chronological order");
    check_expect(clock_scale() == 0 && (int)clock_minute() == (int)(game_state()->minutes % 1440),
                 "the engine displays session time without advancing it again");
    state_observe(0);
    state_restore(&g_initial);
    state_advance_ms(10000);
    g_saved = *game_state();
    state_restore(&g_initial);
    for (i = 0; i < 1000; i++) {
        state_advance_ms(10);
    }
    check_expect(memcmp(&g_saved, game_state(), sizeof(g_saved)) == 0,
                 "timer outcome is independent of frame partitioning");
    state_restore(&g_initial);
}

/* Independent old-wire fixtures: a 1272-byte prefix, then 256 records.
 * Version 2 used 36-byte records, version 3 used 120; current records use 124. */
static void legacy_word(df_u8 *p, df_u32 value)
{
    p[0] = (df_u8)value; p[1] = (df_u8)(value>>8); p[2] = (df_u8)(value>>16); p[3] = (df_u8)(value>>24);
}
static void check_legacy_save(const df_u8 *current, df_u32 size)
{
    static df_u8 legacy[131072];
    df_u32 from, to, hash, i;
    int row, spell_count, version, record_bytes;
    df_u32 spells[OBJECTS];
    /* Standing, from schema 8, sits between the magic rows and the world: ready, a count, a pair a faction and a pair a region. */
    static df_u8 quest_buf[TEST_QUEST_BUFFER];
    int quest_bytes = quest_pack(quest_buf, TEST_QUEST_BUFFER);
    df_u32 stand = 8u + (game_state()->standing.ready ? (df_u32)faction_total()*8u + REGIONS*8u : 0u) + (REGIONS*16u + 16u) + (ROOMS_MOST*12u + 4u) + (4u + REGIONS*4u) + (MEMBERSHIPS*24u + 16u + 4u + 4u + 20u) + CUSTOM_SPELLS*(4u + 89u) + ENCHANTED_ITEMS*(4u + 4u + 80u) + 4u + 4u + (df_u32)quest_bytes + CURSE_WIRE_BYTES + BIO_WIRE_BYTES + 2u*BANK_PLACE_NAME + SHIP_WIRE_BYTES + (4u + (game_state()->standing.ready ? (df_u32)faction_total()*FACTION_LINKS*8u : 0u) + 4u + REGIONS*29u + REGIONS*14u + REGIONS*4u + FACTIONS_MAX + FACTIONS_MAX*4u + RUMOURS_MOST*(4u+4u+4u+3u+4u+RUMOUR_TEXT)) + 4u + 4u + 4u + (df_u32)game_state()->places * 16u + (df_u32)game_state()->world_count * 18u;
    check_expect(size > 24u+1272u+256u*124u && df_ru32(current+24+1272) == WORLD_ROOT,
                 "fixture locates the documented original object section");
    if (size <= 24u+1272u+256u*124u) { return; }
    for (version = 2; version <= 6; version++) {
        memcpy(legacy,current,24+1272);
        /* Old creation had no equipped items or starting spell records. */
        if (version == 2) { memset(legacy+24+1044,0,27*4); }
        from = to = 24+1272;
        record_bytes = version == 2 ? 36 : version == 3 ? 120 : 124;
        for (row = 0; row < 256; row++) {
            if (version == 2 && df_ru32(current+from+8) == 9) { memset(legacy+to,0,36); }
            else {
                memcpy(legacy+to,current+from,record_bytes);
                if (version == 3 && df_ru32(current+from+8) == 9) { memset(legacy+to+48,0,32); }
            }
            from += 124; to += record_bytes;
        }
        memcpy(legacy+to,current+from,size-from-WORLD_WIRE_BASE-game_state()->world_state.map_count*20-stand-(version < 6 ? MAGIC_WIRE_BYTES : 0)-(version < 5 ? STATE5_WIRE_BYTES : 0));
        to += size-from-WORLD_WIRE_BASE-game_state()->world_state.map_count*20-stand-(version < 6 ? MAGIC_WIRE_BYTES : 0)-(version < 5 ? STATE5_WIRE_BYTES : 0);
        legacy_word(legacy+8,version); legacy_word(legacy+24,version); legacy_word(legacy+12,to-24);
        hash = 2166136261u;
        for (i = 24; i < to; i++) { hash = (hash ^ legacy[i])*16777619u; }
        legacy_word(legacy+16,hash);
        check_expect(fs_create("gfall\\saves\\S0003.SAV") && fs_append(legacy,to),"write an independent version-2/version-3 save fixture");
        fs_finish();
        check_expect(save_read(3) && game_state()->version == STATE_VERSION && character_state()->gold == 731,
                     "the previous GFALL schema upgrades without losing the character");
        spell_count = spell_rows(inventory_book(),spells,OBJECTS);
        check_expect(game_state()->equipped[17] && spell_count > 0,
                     "upgrading materialises equipment and starting spells once");
        check_expect(save_write(3) && save_read(3) && spell_rows(inventory_book(),spells,OBJECTS) == spell_count,
                     "resaving an upgraded session does not duplicate spells");
        check_expect(object_at(spells[0])->name[0] && object_at(spells[0])->spell_icon >= 0 && object_at(spells[0])->spell_icon < 69,
                     "older owned spell records acquire their standard editable name and icon");
    }
}

static void check_dead_save(const df_u8 *current, df_u32 size)
{
    static df_u8 dead[131072];
    df_u32 hash, i;
    int health;
    if (size > sizeof(dead)) { return; }
    memcpy(dead,current,size);
    /* Wire header, version/ready, name, race/sex/class, attributes, skills. */
    legacy_word(dead+24+8+NAME_LEN+(3+ATTRS+SKILLS)*4,0);
    hash = 2166136261u;
    for (i = 24; i < size; i++) { hash = (hash ^ dead[i])*16777619u; }
    legacy_word(dead+16,hash);
    check_expect(fs_create("gfall\\saves\\S0003.SAV") && fs_append(dead,size),"write a checksum-valid dead save from the old writer");
    fs_finish();
    health = character_state()->health;
    check_expect(!save_read(3) && strstr(save_error(),"dead") && character_state()->health == health,
                 "old dead saves are rejected without replacing the running character");
}

static void check_disk(const char *root)
{
    FsFile *file;
    df_u32 size, id;
    int roll;
    static df_u8 data[131072];
    ClassInfo custom;

    /* The filesystem's executable directory is build/ for this test only. */
    fs_set_root("build/ARENA2");
    id = item_create(game_state()->inventory_id, 3, 17, 0);
    state_pose(12.5f, 82.0f, -400.25f, 700, -40);
    character_state()->gold = 731;
    character_state()->bio_resist_disease = -5; character_state()->bio_resist_magic = -5; character_state()->bio_to_hit_against = 5; character_state()->bio_resist_poison = -10; character_state()->bio_fatigue = 5;
    character_state()->health -= 3;
    options_set_flags(3);
    options_set_mouse(1);
    /* Standing goes through the save too: a reputation, a legal standing and a price (people and law, slice 1). */
    faction_change_reputation(faction_def(5)->id, 7);
    region_add_legal(17, -4);
    standing_state()->price[17] = 1230;
    /* A membership, a crime count and an invitation go through it too (slice 9). */
    {
        GuildStep joined;

        guilds_clear();
        guild_join(2, 41, 777u, &joined);
        guild_find_faction(41)->rank = 3;
        guilds_state()->thieves_count = 5;
        guilds_state()->brotherhood_invite = 99999u;
    }
    /* The ship's way back goes through the save too. */
    world_where()->ship.valid = 1; world_where()->ship.where.region = 17; world_where()->ship.where.index = 3; world_where()->ship.x = 12.5f; world_where()->ship.yaw = 700;
    /* The capture a save follows: the counts of the keep's and the automap's rows (the rows stay where they are and are written from there). */
    state_capture_world(); world_where()->map_count = automap_count();
    g_saved = *game_state();
    { static df_u8 quest_bytes[TEST_QUEST_BUFFER]; g_saved.quest_len = quest_pack(quest_bytes, TEST_QUEST_BUFFER); }
    check_expect(save_write(1), "write an GFALL session");
    check_expect(fs_exists("gfall\\saves\\S0001.SAV"), "saves live in gfall/saves");
    check_expect(!fs_exists("saves\\S0001.SAV"), "GFALL does not use GFALL's saves folder");
    roll = random_range(0, 999);
    state_clear();
    check_expect(save_read(1), "load after clearing the live session");
    check_expect(memcmp(&g_saved, game_state(), sizeof(g_saved)) == 0,
                 "every serialized field survives a disk round trip");
    check_expect(guild_find_faction(41) && guild_find_faction(41)->rank == 3 && guilds_state()->thieves_count == 5 && guilds_state()->brotherhood_invite == 99999u,
                 "guilds: a membership and the counts survive a save");
    check_expect(options_flags() == 3 && options_mouse() == 1, "saved control and HUD settings are live after loading");
    check_expect(faction_rep(faction_def(5)->id) >= 7 && region_legal(17) == -4 && region_price(17) == 1230,
                 "standing: reputation, legal standing and prices survive a save");
    check_expect(random_range(0, 999) == roll && object_at(id),
                 "save/load retains random continuation and item identity");

    file = fs_open_beside("gfall\\saves\\S0001.SAV");
    if (file) {
        size = fs_size(file);
        check_expect(size < sizeof(data) && fs_read_at(file,0,data,size),"read the current inventory save as a legacy fixture source");
        fs_close(file);
        check_legacy_save(data,size);
        check_expect(save_read(1),"return to the unmodified current save after migration");
        check_dead_save(data,size);
    }

    check_expect(fs_make_dir("gfall\\saves\\S0001.TMP"), "block a temporary write with a directory");
    character_state()->gold = 1;
    check_expect(!save_write(1), "a failed temporary write is reported");
    check_expect(fs_remove_dir("gfall\\saves\\S0001.TMP"), "remove the write-failure fixture");
    check_expect(save_read(1) && character_state()->gold == 731, "a failed write preserves the previous save");

    /* A different character and custom class cannot overwrite slot one's state. */
    custom = *class_at(0);
    df_copy(custom.name, "Test Career", 16);
    custom.forbidden_materials = 0x246;
    custom.resistance = 0x55;
    class_set(18, &custom);
    character_state()->klass = 18;
    df_copy(character_state()->name, "Second Character", NAME_LEN);
    check_expect(state_begin() && save_write(2), "save an independent custom character");
    class_clear();
    check_expect(save_read(2), "a saved career does not depend on loaded class tables");
    check_expect(class_at(18) && class_at(18)->forbidden_materials == 0x246 &&
                 game_state()->career.resistance == 0x55, "custom restrictions survive loading");
    check_expect(save_read(1), "reload the first character independently");
    check_expect(memcmp(&g_saved, game_state(), sizeof(g_saved)) == 0,
                 "a second character has not changed the first save");

    file = fs_open_beside("gfall\\saves\\S0001.SAV");
    check_expect(file != 0, "open the actual save for corruption tests");
    if (file) {
        size = fs_size(file);
        check_expect(size < sizeof(data) && fs_read_at(file, 0, data, size), "read save test bytes");
        fs_close(file);
        data[size - 1] ^= 1;
        check_expect(fs_create("gfall\\saves\\S0003.SAV") && fs_append(data, size), "write damaged fixture");
        fs_finish();
        check_expect(!save_read(3), "checksum detects payload corruption");
        check_expect(memcmp(&g_saved, game_state(), sizeof(g_saved)) == 0,
                     "damaged file cannot partly replace the live session");
        data[size - 1] ^= 1;
        data[8] = STATE_VERSION + 1;
        check_expect(fs_create("gfall\\saves\\S0003.SAV") && fs_append(data, size), "write future version fixture");
        fs_finish();
        check_expect(!save_read(3), "unknown GFALL save version is explicitly rejected");
        check_expect(fs_create("gfall\\saves\\S0003.SAV") && fs_append(data, 12), "write truncated fixture");
        fs_finish();
        check_expect(!save_read(3), "truncated header is rejected");
    }
    check_expect(!save_write(0) && !save_read(10000), "save slots cannot escape their numeric names");
    check_expect(fs_move("gfall\\saves\\S0001.SAV", "gfall\\saves\\S0001.SAV.OLD"), "stage interrupted replacement");
    check_expect(save_read(1), "Win98 interrupted replacement recovers the previous save");
    check_expect(save_write(1), "replace a save with a verified new file");
    fs_delete("gfall\\saves\\S0001.SAV");
    fs_delete("gfall\\saves\\S0001.SAV.OLD");
    fs_delete("gfall\\saves\\S0002.SAV");
    fs_delete("gfall\\saves\\S0003.SAV");
    fs_remove_dir("gfall\\saves");
    fs_remove_dir("gfall");
    fs_set_root(root);
    /* Standard tables were intentionally removed above; restore only the used row. */
    class_set(g_initial.who.klass, &g_initial.career);
    state_restore(&g_initial);
}

/* A long campaign (docs/game.md, Saves): far more keep rows than the old 512 and automap rows than the old 8192 go through a save and a load, and what the quests pack takes the room it needs. */
#define LONG_KEEP_ROWS 3000
#define LONG_MAP_ROWS 20000
#define LONG_MAP_PER_PLACE 100
static void check_long_campaign(void)
{
    int i, same;
    KeepKey key;
    Keep *row;
    AutomapRow seen;

    keep_clear(); automap_clear();
    for (i = 0; i < LONG_KEEP_ROWS; i++) {
        key.place = 0x40000000u + (df_u32)i; key.thing = 1u + (df_u32)(i % 7);
        row = keep_get(key, KEEP_MOVER);
        check_expect(row != 0, "long campaign: a keep row could not be made");
        if (!row) { return; }
        row->at_end = (df_u8)(i & 1); row->lock = i;
    }
    for (i = 0; i < LONG_MAP_ROWS; i++) {
        seen.place = 1u + (df_u32)(i / LONG_MAP_PER_PLACE); seen.mesh = (df_u32)i; seen.x = i; seen.y = -i; seen.z = i * 2;
        check_expect(automap_put(&seen), "long campaign: an automap row could not be made");
    }
    state_capture_world(); world_where()->map_count = automap_count();
    check_expect(game_state()->world_count == LONG_KEEP_ROWS && world_where()->map_count == LONG_MAP_ROWS, "long campaign: the counts are not the rows held");
    check_expect(save_write(7), "long campaign: a save with many rows was refused");
    keep_clear(); automap_clear();
    check_expect(save_read(7), "long campaign: a save with many rows did not load");
    check_expect(state_restore_world(), "long campaign: the keep rows were not restored");
    world_restore();
    check_expect(keep_count() == LONG_KEEP_ROWS && automap_count() == LONG_MAP_ROWS, "long campaign: rows were lost in the save");
    same = 1;
    for (i = 0; i < LONG_KEEP_ROWS && same; i++) {
        key.place = 0x40000000u + (df_u32)i; key.thing = 1u + (df_u32)(i % 7);
        row = keep_find(key);
        same = row && row->lock == i && row->at_end == (i & 1);
    }
    check_expect(same, "long campaign: a keep row came back different");
    same = 1;
    for (i = 0; i < LONG_MAP_ROWS && same; i++) {
        /* The rows come back in the order they were made (place, then mesh). */
        const AutomapRow *got = automap_at(i);
        same = got && got->place == 1u + (df_u32)(i / LONG_MAP_PER_PLACE) && got->mesh == (df_u32)i && got->x == i && got->y == -i && got->z == i * 2;
    }
    check_expect(same, "long campaign: an automap row came back different");
    keep_clear(); automap_clear();
    state_capture_world(); world_where()->map_count = 0;
}

/* A save that cannot be finished (the host's file size limit stands for a full disk): it says so, leaves no half-written file, and the slot keeps what it had. */
#define FULL_DISK_LIMIT 50000
#define FULL_DISK_SLOT 8
static void check_full_disk(void)
{
    struct rlimit was, small;
    int before;

    character_state()->gold = 4242;
    check_expect(save_write(FULL_DISK_SLOT), "full disk: the first save was refused");
    character_state()->gold = 5151;
    getrlimit(RLIMIT_FSIZE, &was);
    small = was; small.rlim_cur = FULL_DISK_LIMIT;
    signal(SIGXFSZ, SIG_IGN);
    setrlimit(RLIMIT_FSIZE, &small);
    before = save_write(FULL_DISK_SLOT);
    setrlimit(RLIMIT_FSIZE, &was);
    check_expect(!before, "full disk: a write that could not finish was reported as saved");
    check_expect(strstr(save_error(), "disk is full") != 0, "full disk: the message does not say the disk is full");
    check_expect(!fs_exists("gfall\\saves\\S0008.TMP"), "full disk: a half-written file was left behind");
    check_expect(save_read(FULL_DISK_SLOT) && character_state()->gold == 4242, "full disk: the slot did not keep its previous save");
}

/* A pile's contents as one number, whatever the object ids: what is in it and how it is. */
static df_u32 pile_sum(df_u32 container)
{
    df_u32 sum = 0u;
    int i;

    for (i = 0; i < OBJECTS; i++) {
        const ObjectInfo *o = &game_state()->object[i];

        if (o->id && o->parent == container) {
            sum = seed_mix(sum, (df_u32)(o->group * 1000 + o->index));
            sum = seed_mix(sum, (df_u32)(o->material * 100000 + o->count));
            sum = seed_mix(sum, (df_u32)(o->value + o->condition * 7 + o->color));
        }
    }
    return sum;
}

static void pile_drop(df_u32 container)
{
    int i;

    for (i = 0; i < OBJECTS; i++) {
        if (game_state()->object[i].id && game_state()->object[i].parent == container) { object_delete(game_state()->object[i].id); }
    }
    object_delete(container);
}

/* Place state (docs/gfall-place-state.md): a pile is a function of saved numbers. Opened, thrown away and opened again after the stream has moved on, it is the same pile; another thing, another place or another epoch gives another. */
#define PILE_PLACE 0x40000123u
#define PILE_THING 77u
#define PILE_TABLE 3
#define PILE_STREAM_SPENT 37
static void check_place_seeds(void)
{
    df_u32 first, again, other_thing, other_place, other_epoch, same_later, id;
    int i;

    state_restore(&g_initial); character_state()->level = 6;
    id = loot_container(PILE_PLACE, PILE_THING, 0, 0, 0, PILE_TABLE);
    check_expect(id != 0, "place seeds: a pile could not be made");
    first = pile_sum(id);
    pile_drop(id);
    for (i = 0; i < PILE_STREAM_SPENT; i++) { random_range(0, 1000); }
    id = loot_container(PILE_PLACE, PILE_THING, 0, 0, 0, PILE_TABLE);
    again = pile_sum(id);
    check_expect(first == again && first != 0u, "place seeds: a pile opened again after the stream moved on is not the same pile");
    pile_drop(id);
    id = loot_container(PILE_PLACE, PILE_THING + 1u, 0, 0, 0, PILE_TABLE); other_thing = pile_sum(id); pile_drop(id);
    id = loot_container(PILE_PLACE + 1u, PILE_THING, 0, 0, 0, PILE_TABLE); other_place = pile_sum(id); pile_drop(id);
    check_expect(other_thing != first && other_place != first, "place seeds: another thing or another place gave the same pile");
    /* A place's seeds are frozen at its first entry: an epoch later (within the grace) it is the same pile. */
    state_pass_minutes(PLACE_EPOCH_DAYS * MINUTES_PER_DAY);
    id = loot_container(PILE_PLACE, PILE_THING, 0, 0, 0, PILE_TABLE); same_later = pile_sum(id); pile_drop(id);
    check_expect(same_later == first, "place seeds: a place within its grace gave another pile");
    /* Left for more than the grace, its row is swept and the next entry is a new roll. */
    state_pass_minutes(PLACE_EPOCH_DAYS * MINUTES_PER_DAY * (PLACE_GRACE_EPOCHS + 1));
    id = loot_container(PILE_PLACE, PILE_THING, 0, 0, 0, PILE_TABLE); other_epoch = pile_sum(id); pile_drop(id);
    check_expect(other_epoch != first, "place seeds: a swept place gave the same pile");
    check_expect(seed_mix(1u, 2u) == seed_mix(1u, 2u) && seed_mix(1u, 2u) != seed_mix(2u, 1u) && seed_mix(0u, 0u) != 0u, "place seeds: the mix is not a function of its words in order");
}

/* The sweep (docs/gfall-place-state.md, 3 and 4): a place left for more than its grace is cleaned of its keep rows and piles and row, one sweep however many days pass, never the place the player is in, and a full table drops the oldest that is not. */
#define SWEEP_PLACE_A 0x40000201u
#define SWEEP_PLACE_B 0x40000202u
static int place_has_things(df_u32 place)
{
    int i, found = 0;

    for (i = 0; i < keep_count(); i++) { if (keep_at(i)->key.place == place) { found = 1; } }
    for (i = 0; i < OBJECTS; i++) { if (game_state()->object[i].id && game_state()->object[i].place_key == place) { found = 1; } }
    return found;
}
static void place_dirty(df_u32 place)
{
    KeepKey key;
    Keep *row;

    key.place = place; key.thing = 9u;
    row = keep_get(key, KEEP_MOVER);
    if (row) { row->at_end = 1; }
    loot_container(place, 5u, 0, 0, 0, PILE_TABLE);
}
static void check_place_sweep(void)
{
    df_u32 day = MINUTES_PER_DAY * PLACE_EPOCH_DAYS;
    int i, base;

    state_restore(&g_initial); keep_clear(); character_state()->level = 4;
    /* Other suites may have left rows; the counts below are against what is there. */
    state_pass_minutes(day * 20u);
    base = game_state()->places;
    place_dirty(SWEEP_PLACE_A);
    check_expect(place_has_things(SWEEP_PLACE_A) && game_state()->places == base + 1 && place_roll_level(SWEEP_PLACE_A) == 4, "place sweep: a place entered has no row, things or level");
    /* The level is the one it was entered at, whatever the player becomes. */
    character_state()->level = 9;
    check_expect(place_roll_level(SWEEP_PLACE_A) == 4, "place sweep: a place's roll level followed the player");
    /* Within its grace, left: it stays. */
    place_leave(SWEEP_PLACE_A);
    state_pass_minutes(day);
    check_expect(place_has_things(SWEEP_PLACE_A) && game_state()->places == base + 1, "place sweep: a place within its grace was swept");
    /* Past it, one sweep however many days a long rest spans. */
    state_pass_minutes(day * 5u);
    check_expect(!place_has_things(SWEEP_PLACE_A) && game_state()->places == base, "place sweep: a place left for more than its grace was kept");
    /* The place the player is in is not swept, however long they wait; it is stamped when they leave. */
    place_dirty(SWEEP_PLACE_B);
    object_edit(WORLD_ROOT)->place_key = SWEEP_PLACE_B;
    state_pass_minutes(day * 4u);
    check_expect(place_has_things(SWEEP_PLACE_B), "place sweep: the place the player is in was swept");
    place_leave(SWEEP_PLACE_B);
    object_edit(WORLD_ROOT)->place_key = 0;
    state_pass_minutes(day);
    check_expect(place_has_things(SWEEP_PLACE_B), "place sweep: a place just left was swept at once");
    state_pass_minutes(day * 2u);
    check_expect(!place_has_things(SWEEP_PLACE_B), "place sweep: a place left past its grace was kept");
    /* Never swept: what is the player's own. */
    check_expect(game_state()->inventory_id != 0 && object_at(game_state()->inventory_id) != 0, "place sweep: the player's pack was swept");
    /* A full table drops the place left longest ago, never the one the player is in. */
    keep_clear();
    place_dirty(0x40001000u);
    for (i = 1; game_state()->places < PLACE_ROWS && i < PLACE_ROWS * 2; i++) { place_roll_level(0x40001000u + (df_u32)i); state_pass_minutes(1); }
    check_expect(game_state()->places == PLACE_ROWS, "place sweep: the table did not fill");
    place_dirty(0x40009000u);
    check_expect(game_state()->places == PLACE_ROWS && place_has_things(0x40009000u), "place sweep: a full table did not take a new place");
    check_expect(!place_has_things(0x40001000u), "place sweep: a full table did not drop the oldest place");
    keep_clear();
}

void check_state(const char *root)
{
    g_initial = *game_state();
    check_random();
    check_objects();
    check_validation();
    check_time();
    check_shelf();
    state_restore(&g_initial);
    check_market();
    check_market_screen();
    state_restore(&g_initial);
    check_bank();
    state_restore(&g_initial);
    check_bank_screen();
    check_bank_loans();
    check_bank_property();
    state_restore(&g_initial);
    check_lodging();
    state_restore(&g_initial);
    check_law();
    state_restore(&g_initial);
    check_law_guards();
    check_law_time_pass_hook();
    state_restore(&g_initial);
    check_talk();
    check_steal();
    check_names();
    check_guild();
    check_potion();
    check_spellshop();
    check_magicitem();
    check_quest();
    check_entry();
    check_talkwin();
    state_restore(&g_initial);
    state_restore(&g_initial);
    check_disk(root);
    check_long_campaign();
    check_full_disk();
    check_place_seeds();
    check_place_sweep();
}
