#include "check_ogstate.h"
#include "check_ogshelf.h"
#include "check_ogmarket.h"
#include "check_ogbank.h"
#include "check_oglodging.h"
#include "check_oglaw.h"
#include "check_ogtalk.h"
#include "check_ogsteal.h"
#include "check_ognames.h"
#include "check_ogguild.h"
#include "check_ogtalkwin.h"
#include "game/ogfall/play/standing.h"
#include "check_util.h"
#include "engine/core/mem.h"
#include "engine/platform/fs.h"
#include "engine/scene/clock.h"
#include "game/ogfall/play/options.h"
#include "game/ogfall/play/save.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/spellbook.h"
#include <string.h>

static OgState g_initial;
static OgState g_saved;
static OgState g_bad;
static int g_minutes_seen;
static int g_minute_order;
static df_u32 g_previous_minute;

static void observe_minute(const OgEvent *event)
{
    if (event->kind != OG_EVENT_MINUTE) {
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

    og_random_seed(1);
    for (i = 0; i < 6; i++) {
        check_expect(og_random_range(0, 32767) == expected[i], "Watcom rand reference sequence");
    }
    og_random_seed(0);
    check_expect(og_random_range(0, 32767) == 0, "Watcom srand accepts zero");
    g_saved = *og_state();
    i = og_random_range(-5, 5);
    check_expect(og_state_restore(&g_saved), "restore the random stream");
    check_expect(og_random_range(-5, 5) == i, "restoring state replays the next random draw");
    og_state_restore(&g_initial);
}

static void check_objects(void)
{
    df_u32 a, b, id;
    int n;

    check_expect(og_state_valid(og_state()), "chargen builds a valid session");
    check_expect(og_object(og_state()->eye_id)->parent == og_state()->move_id,
                 "the eye belongs to the movement record");
    a = og_object_new(51, og_state()->inventory_id, 9, 100);
    b = og_item_create(a, 3, 5, 0);
    check_expect(a && b && a != b, "objects have distinct stable IDs");
    check_expect(!og_object_reparent(a, b), "ownership cannot become a cycle");
    check_expect(!og_object_delete(a), "a container with children cannot be deleted");
    check_expect(!og_object_reparent(b, 999u), "an owner must exist");
    check_expect(!og_object_delete(og_state()->entity_id), "mandatory player records survive deletion");
    check_expect(og_object_reparent(b, OG_WORLD_ROOT), "an item can leave the player's container");
    check_expect(og_object_delete(a), "an empty container can be deleted");
    check_expect(og_object(b)->parent == OG_WORLD_ROOT, "moving ownership preserves the item's identity");
    g_saved = *og_state();
    g_saved.next_low = 62999;
    check_expect(og_state_restore(&g_saved), "restore an ID counter at the wrap boundary");
    id = og_object_new(2, OG_WORLD_ROOT, 0, 100);
    check_expect((id & 65535u) == 62999u, "last original low ID is 62999");
    id = og_object_new(2, OG_PLAYER_ROOT, 0, 100);
    check_expect((id & 65535u) >= 10000u && id != og_state()->move_id && id != b,
                 "wrapped IDs avoid existing IDs in both roots");
    n = 0;
    while (og_object_new(2, OG_WORLD_ROOT, 0, 101)) {
        n++;
    }
    check_expect(n > 0 && n < OG_OBJECTS, "the object pool has a bounded capacity");
    check_expect(og_object(b) != 0, "pool exhaustion does not overwrite an existing object");
    og_state_restore(&g_initial);
}

static void rejects_snapshot(const char *reason)
{
    g_saved = *og_state();
    check_expect(!og_state_restore(&g_bad), reason);
    check_expect(memcmp(&g_saved, og_state(), sizeof(g_saved)) == 0,
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
    g_bad.depth = OG_MODES + 1;
    rejects_snapshot("an oversized modal stack is refused");
}

static void check_time(void)
{
    df_u32 start;
    int i, year, month, day, weekday;

    start = og_state()->minutes;
    og_state_date(&year, &month, &day, &weekday);
    check_expect(year == 401 && month == 0 && day == 3 && weekday == 6,
                 "calendar derives the original epoch and continuous weekday");
    check_expect(start == 523530u, "new-game clock uses the original initial value");
    og_state_advance_ms(4943);
    check_expect(og_state()->minutes == start, "89 BIOS ticks do not advance a game minute");
    og_state_advance_ms(1);
    check_expect(og_state()->minutes == start + 1, "90 BIOS ticks advance one game minute");
    og_options_open();
    og_options_open();
    check_expect(og_state()->mode == OG_GAME_OPTIONS && og_state()->depth == 1,
                 "opening one panel twice owns only one modal frame");
    g_saved = *og_state();
    og_state_frame(0.1f);
    og_state_advance_ms(60000);
    check_expect(memcmp(&g_saved, og_state(), sizeof(g_saved)) == 0,
                 "menus pause minutes and their fractional tick remainder");
    og_options_close();
    check_expect(og_state()->mode == OG_GAME_WORLD && !og_state()->depth,
                 "closing Options restores the prior game mode");
    for (i = 0; i < OG_MODES; i++) {
        check_expect(og_state_mode_push(OG_GAME_INVENTORY), "bounded modal stack pushes");
    }
    check_expect(!og_state_mode_push(OG_GAME_SHEET), "modal stack overflow is refused");
    for (i = 0; i < OG_MODES; i++) {
        check_expect(og_state_mode_pop(), "modal stack restores nested modes");
    }
    check_expect(!og_state_mode_pop(), "modal stack underflow is refused");
    g_previous_minute = og_state()->minutes;
    g_minutes_seen = 0;
    g_minute_order = 1;
    og_state_observe(observe_minute);
    og_state_pass_minutes(61);
    check_expect(g_minute_order && g_minutes_seen == 61,
                 "elapsed-minute notifications retain chronological order");
    check_expect(clock_scale() == 0 && (int)clock_minute() == (int)(og_state()->minutes % 1440),
                 "the engine displays session time without advancing it again");
    og_state_observe(0);
    og_state_restore(&g_initial);
    og_state_advance_ms(10000);
    g_saved = *og_state();
    og_state_restore(&g_initial);
    for (i = 0; i < 1000; i++) {
        og_state_advance_ms(10);
    }
    check_expect(memcmp(&g_saved, og_state(), sizeof(g_saved)) == 0,
                 "timer outcome is independent of frame partitioning");
    og_state_restore(&g_initial);
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
    df_u32 spells[OG_OBJECTS];
    /* Standing, from schema 8, sits between the magic rows and the world: ready, a count, a pair a faction and a pair a region. */
    df_u32 stand = 8u + (og_state()->standing.ready ? (df_u32)og_faction_count()*8u + OG_REGIONS*8u : 0u) + (OG_REGIONS*16u + 16u) + (OG_ROOMS_MOST*12u + 4u) + (4u + OG_REGIONS*4u) + (OG_MEMBERSHIPS*24u + 16u);
    check_expect(size > 24u+1272u+256u*124u && df_ru32(current+24+1272) == OG_WORLD_ROOT,
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
        memcpy(legacy+to,current+from,size-from-OG_WORLD_WIRE_BASE-og_state()->world_state.map_count*20-stand-(version < 6 ? OG_MAGIC_WIRE_BYTES : 0)-(version < 5 ? OG_STATE5_WIRE_BYTES : 0));
        to += size-from-OG_WORLD_WIRE_BASE-og_state()->world_state.map_count*20-stand-(version < 6 ? OG_MAGIC_WIRE_BYTES : 0)-(version < 5 ? OG_STATE5_WIRE_BYTES : 0);
        legacy_word(legacy+8,version); legacy_word(legacy+24,version); legacy_word(legacy+12,to-24);
        hash = 2166136261u;
        for (i = 24; i < to; i++) { hash = (hash ^ legacy[i])*16777619u; }
        legacy_word(legacy+16,hash);
        check_expect(fs_create("ogfall\\saves\\S0003.SAV") && fs_append(legacy,to),"write an independent version-2/version-3 save fixture");
        fs_finish();
        check_expect(og_save_read(3) && og_state()->version == OG_STATE_VERSION && og_character()->gold == 731,
                     "the previous OGFALL schema upgrades without losing the character");
        spell_count = og_spell_rows(og_inventory_book(),spells,OG_OBJECTS);
        check_expect(og_state()->equipped[17] && spell_count > 0,
                     "upgrading materialises equipment and starting spells once");
        check_expect(og_save_write(3) && og_save_read(3) && og_spell_rows(og_inventory_book(),spells,OG_OBJECTS) == spell_count,
                     "resaving an upgraded session does not duplicate spells");
        check_expect(og_object(spells[0])->name[0] && og_object(spells[0])->spell_icon >= 0 && og_object(spells[0])->spell_icon < 69,
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
    legacy_word(dead+24+8+OG_NAME+(3+OG_ATTRS+OG_SKILLS)*4,0);
    hash = 2166136261u;
    for (i = 24; i < size; i++) { hash = (hash ^ dead[i])*16777619u; }
    legacy_word(dead+16,hash);
    check_expect(fs_create("ogfall\\saves\\S0003.SAV") && fs_append(dead,size),"write a checksum-valid dead save from the old writer");
    fs_finish();
    health = og_character()->health;
    check_expect(!og_save_read(3) && strstr(og_save_error(),"dead") && og_character()->health == health,
                 "old dead saves are rejected without replacing the running character");
}

static void check_disk(const char *root)
{
    FsFile *file;
    df_u32 size, id;
    int roll;
    static df_u8 data[65536];
    OgClass custom;

    /* The filesystem's executable directory is build/ for this test only. */
    fs_set_root("build/ARENA2");
    id = og_item_create(og_state()->inventory_id, 3, 17, 0);
    og_state_pose(12.5f, 82.0f, -400.25f, 700, -40);
    og_character()->gold = 731;
    og_character()->health -= 3;
    og_options_set_flags(3);
    og_options_set_mouse(1);
    /* Standing goes through the save too: a reputation, a legal standing and a price (people and law, slice 1). */
    og_faction_change_reputation(og_faction_def(5)->id, 7);
    og_region_add_legal(17, -4);
    og_standing()->price[17] = 1230;
    /* A membership, a crime count and an invitation go through it too (slice 9). */
    {
        OgGuildStep joined;

        og_guilds_clear();
        og_guild_join(2, 41, 777u, &joined);
        og_guild_find_faction(41)->rank = 3;
        og_guilds()->thieves_count = 5;
        og_guilds()->brotherhood_invite = 99999u;
    }
    g_saved = *og_state();
    check_expect(og_save_write(1), "write an OGFALL session");
    check_expect(fs_exists("ogfall\\saves\\S0001.SAV"), "saves live in ogfall/saves");
    check_expect(!fs_exists("saves\\S0001.SAV"), "OGFALL does not use GFALL's saves folder");
    roll = og_random_range(0, 999);
    og_state_clear();
    check_expect(og_save_read(1), "load after clearing the live session");
    check_expect(memcmp(&g_saved, og_state(), sizeof(g_saved)) == 0,
                 "every serialized field survives a disk round trip");
    check_expect(og_guild_find_faction(41) && og_guild_find_faction(41)->rank == 3 && og_guilds()->thieves_count == 5 && og_guilds()->brotherhood_invite == 99999u,
                 "guilds: a membership and the counts survive a save");
    check_expect(og_options_flags() == 3 && og_options_mouse() == 1, "saved control and HUD settings are live after loading");
    check_expect(og_faction_rep(og_faction_def(5)->id) >= 7 && og_region_legal(17) == -4 && og_region_price(17) == 1230,
                 "standing: reputation, legal standing and prices survive a save");
    check_expect(og_random_range(0, 999) == roll && og_object(id),
                 "save/load retains random continuation and item identity");

    file = fs_open_beside("ogfall\\saves\\S0001.SAV");
    if (file) {
        size = fs_size(file);
        check_expect(size < sizeof(data) && fs_read_at(file,0,data,size),"read the current inventory save as a legacy fixture source");
        fs_close(file);
        check_legacy_save(data,size);
        check_expect(og_save_read(1),"return to the unmodified current save after migration");
        check_dead_save(data,size);
    }

    check_expect(fs_make_dir("ogfall\\saves\\S0001.TMP"), "block a temporary write with a directory");
    og_character()->gold = 1;
    check_expect(!og_save_write(1), "a failed temporary write is reported");
    check_expect(fs_remove_dir("ogfall\\saves\\S0001.TMP"), "remove the write-failure fixture");
    check_expect(og_save_read(1) && og_character()->gold == 731, "a failed write preserves the previous save");

    /* A different character and custom class cannot overwrite slot one's state. */
    custom = *og_class(0);
    df_copy(custom.name, "Test Career", 16);
    custom.forbidden_materials = 0x246;
    custom.resistance = 0x55;
    og_class_set(18, &custom);
    og_character()->klass = 18;
    df_copy(og_character()->name, "Second Character", OG_NAME);
    check_expect(og_state_begin() && og_save_write(2), "save an independent custom character");
    og_class_clear();
    check_expect(og_save_read(2), "a saved career does not depend on loaded class tables");
    check_expect(og_class(18) && og_class(18)->forbidden_materials == 0x246 &&
                 og_state()->career.resistance == 0x55, "custom restrictions survive loading");
    check_expect(og_save_read(1), "reload the first character independently");
    check_expect(memcmp(&g_saved, og_state(), sizeof(g_saved)) == 0,
                 "a second character has not changed the first save");

    file = fs_open_beside("ogfall\\saves\\S0001.SAV");
    check_expect(file != 0, "open the actual save for corruption tests");
    if (file) {
        size = fs_size(file);
        check_expect(size < sizeof(data) && fs_read_at(file, 0, data, size), "read save test bytes");
        fs_close(file);
        data[size - 1] ^= 1;
        check_expect(fs_create("ogfall\\saves\\S0003.SAV") && fs_append(data, size), "write damaged fixture");
        fs_finish();
        check_expect(!og_save_read(3), "checksum detects payload corruption");
        check_expect(memcmp(&g_saved, og_state(), sizeof(g_saved)) == 0,
                     "damaged file cannot partly replace the live session");
        data[size - 1] ^= 1;
        data[8] = OG_STATE_VERSION + 1;
        check_expect(fs_create("ogfall\\saves\\S0003.SAV") && fs_append(data, size), "write future version fixture");
        fs_finish();
        check_expect(!og_save_read(3), "unknown OGFALL save version is explicitly rejected");
        check_expect(fs_create("ogfall\\saves\\S0003.SAV") && fs_append(data, 12), "write truncated fixture");
        fs_finish();
        check_expect(!og_save_read(3), "truncated header is rejected");
    }
    check_expect(!og_save_write(0) && !og_save_read(10000), "save slots cannot escape their numeric names");
    check_expect(fs_move("ogfall\\saves\\S0001.SAV", "ogfall\\saves\\S0001.SAV.OLD"), "stage interrupted replacement");
    check_expect(og_save_read(1), "Win98 interrupted replacement recovers the previous save");
    check_expect(og_save_write(1), "replace a save with a verified new file");
    fs_delete("ogfall\\saves\\S0001.SAV");
    fs_delete("ogfall\\saves\\S0001.SAV.OLD");
    fs_delete("ogfall\\saves\\S0002.SAV");
    fs_delete("ogfall\\saves\\S0003.SAV");
    fs_remove_dir("ogfall\\saves");
    fs_remove_dir("ogfall");
    fs_set_root(root);
    /* Standard tables were intentionally removed above; restore only the used row. */
    og_class_set(g_initial.who.klass, &g_initial.career);
    og_state_restore(&g_initial);
}

void check_ogstate(const char *root)
{
    g_initial = *og_state();
    check_random();
    check_objects();
    check_validation();
    check_time();
    check_ogshelf();
    og_state_restore(&g_initial);
    check_ogmarket();
    check_ogmarket_screen();
    og_state_restore(&g_initial);
    check_ogbank();
    og_state_restore(&g_initial);
    check_ogbank_screen();
    og_state_restore(&g_initial);
    check_oglodging();
    og_state_restore(&g_initial);
    check_oglaw();
    og_state_restore(&g_initial);
    check_oglaw_guards();
    og_state_restore(&g_initial);
    check_ogtalk();
    check_ogsteal();
    check_ognames();
    check_ogguild();
    check_ogtalkwin();
    og_state_restore(&g_initial);
    og_state_restore(&g_initial);
    check_disk(root);
}
