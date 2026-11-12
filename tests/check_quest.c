#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "check_quest.h"
#include "check_util.h"
#include "game/dagger/formats/qbn.h"
#include "game/dagger/play/quest.h"
#include "game/dagger/play/talk.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/state.h"
#include "game/dagger/formats/textrsc.h"

static int g_said, g_asked;
static char g_text[200];

static int give_text(const char *quest, int message, int in_text_rsc, char *out, int cap)
{
    g_asked = message;
    (void)quest; (void)in_text_rsc; (void)cap;
    strcpy(out, "Well done.");
    return 10;
}

static void questor(int who) { quest_set_questor(who, 851, "Sir Test", "Wayrest"); }

static void take_say(const char *text) { g_said++; strncpy(g_text, text, sizeof(g_text) - 1); }

static int name_place(int slot, int index, int form, char *out, int cap) { (void)slot; (void)cap; strcpy(out, form >= 32 ? "Dungeon of Test" : "Test Tower"); return index + 1; }
static int name_person(int slot, int index, int form, char *out, int cap) { (void)slot; (void)cap; strcpy(out, form >= 32 ? "Wayrest" : "Sir Test"); return index + 1; }
static const char *where_now(void) { return "Testown"; }
static int g_in_rsc;
static int real_text(const char *quest, int message, int in_text_rsc, char *out, int cap)
{
    char file[16];

    g_in_rsc = in_text_rsc;
    if (in_text_rsc) { return textrsc_read(message, out, cap); }
    sprintf(file, "%s.QRC", quest);
    return textrsc_read_in(file, message, out, cap);
}

static int w_count(void) { return 5; }
static int w_location(int ordinal, LocationInfo *out)
{
    static const int type[5] = {0, 4, 10, 4, 1};
    static const char *const name[5] = {"Test Town", "Dungeon One", "Dungeon Two", "Dungeon Three", "Test Hamlet"};

    if (ordinal < 0 || ordinal > 4) { return 0; }
    memset(out, 0, sizeof(*out));
    strcpy(out->name, name[ordinal]); out->region = 17; out->index = 10 + ordinal; out->px = 100 + ordinal; out->py = 200; out->type = type[ordinal];
    return 1;
}
static void w_here(int *region, int *index, int *px, int *py) { *region = 17; *index = 10; *px = 100; *py = 200; }
static int w_region(int region, char *out, int cap) { (void)region; (void)cap; strcpy(out, "Test Province"); return 1; }
static int w_journey(int sx, int sy, int ex, int ey) { (void)sx; (void)sy; (void)ex; (void)ey; return 100; }
static int g_revealed[8], g_revealed_count;
static void w_reveal(int ordinal) { if (g_revealed_count < 8) { g_revealed[g_revealed_count++] = ordinal; } }
static int g_spawned, g_spawn_kind[16];
static df_u32 g_spawn_thing[16];
static int carried_by_player(df_u32 id)
{
    const ObjectInfo *o;

    o = object_at(id);
    return o && o->parent == game_state()->inventory_id;
}
static int g_went_region = -1, g_went_index = -1;
static int w_go(int region, int index) { g_went_region = region; g_went_index = index; return 1; }
static void w_here_out(int *region, int *index, int *px, int *py) { *region = 17; *index = 99; *px = 100; *py = 200; }
static int w_find_number(int number, int *region, int *index)
{
    /* The stub's locations: 50050 (a dungeon) lies under the outside the player is in, 50026 is the town beside it. */
    *region = 17;
    if (number == 50050) { *index = 10; return 1; }
    if (number == 50026) { *index = 11; return 1; }
    return 0;
}
static int w_pick_building(int region, int index, int type, unsigned seed, int *block, int *record) { (void)region; (void)index; (void)type; *block = 3 + (int)(seed % 2u) * 0; *record = 7; return 1; }
static int g_out;
static int w_outdoors(void) { return g_out; }
static int g_site_mode = -1, g_site_value, g_site_none;
static const int a51_none[5] = {-1, -2, -2, -2, -2};
static int w_pick_site(int region, int index, int mode, int value, unsigned seed, int *block, int *record, int *faction)
{
    (void)region; (void)index; (void)seed;
    if (g_site_none) { return 0; }
    g_site_mode = mode; g_site_value = value;
    *block = 5; *record = 9; *faction = mode == 2 ? value : 4242;
    return 1;
}
static void w_person_name(int who, int faction, int female, char *out, int cap) { (void)who; (void)faction; (void)cap; strcpy(out, female ? "Lady Test" : "Sir Test"); }
static int w_marker_at(int marker, int record, unsigned seed, float *x, float *y, float *z)
{
    (void)seed;
    if (marker == 0 || (marker < 0 && record == 11)) { *x = 1000.0f; *y = 20.0f; *z = 2000.0f; return 1; }
    if (marker == 1 || (marker < 0 && record == 18)) { *x = 3000.0f; *y = 30.0f; *z = 4000.0f; return 1; }
    return 0;
}
static int g_floor, g_at_made;
static float g_at_x, g_at_z;
static df_u32 g_at_thing[8];
static int w_spawn_at(int kind, df_u32 thing, float x, float y, float z) { (void)kind; (void)y; if (!g_floor) { return 0; } g_at_x = x; g_at_z = z; if (g_at_made < 8) { g_at_thing[g_at_made] = thing; } g_at_made++; return 1; }
static df_u32 w_place_at(float x, float y, float z) { (void)x; (void)y; (void)z; return 0; }
static df_u32 g_pile_made;
static df_u32 w_place_item(void) { g_pile_made++; return 0; }
static int w_spawn(int kind, df_u32 thing) { if (g_spawned < 16) { g_spawn_kind[g_spawned] = kind; g_spawn_thing[g_spawned++] = thing; } return 1; }

static void (*g_answer)(int yes);
static int take_ask(const char *words, void (*answer)(int yes)) { g_answer = answer; strncpy(g_text, words, sizeof(g_text) - 1); return 1; }

static void put16(df_u8 *p, int v) { p[0] = (df_u8)v; p[1] = (df_u8)(v >> 8); }
static void put32(df_u8 *p, df_u32 v) { p[0] = (df_u8)v; p[1] = (df_u8)(v >> 8); p[2] = (df_u8)(v >> 16); p[3] = (df_u8)(v >> 24); }

/* An operand naming state n (negated or not), "always" (-1) or "none" (-2). */
static void put_arg(df_u8 *p, int value, int negate)
{
    p[0] = (df_u8)negate;
    put32(p + 1, value < 0 ? (df_u32)0x00000900u | (value == -1 ? 0xFFu : 0xFEu) : 0x900u | (df_u32)value);
    put16(p + 5, 9);
    put32(p + 7, (df_u32)value);
}

/* An operand that is a literal number. */
static void put_lit(df_u8 *p, int value)
{
    p[0] = 0; put32(p + 1, 0x12345678u); put16(p + 5, 0); put32(p + 7, (df_u32)value);
}

static void put_op(df_u8 *p, int opcode, int message, const int *args, const int *negate)
{
    int k;

    put16(p, opcode); put16(p + 81, message);
    for (k = 0; k < 5; k++) { put_arg(p + 6 + k * 15, args[k], negate ? negate[k] : 0); }
}

void check_quest(void)
{
    static Qbn q;
    static df_u8 file[4096];
    DIR *dir;
    struct dirent *entry;
    int files, bad, slot, i;
    char name[16];

    /* Every QBN in ARENA2 parses within the record tables, with the argument counts of COUNT.DAT. */
    files = 0; bad = 0;
    dir = opendir("daggerfall/DAGGER/ARENA2");
    while (dir && (entry = readdir(dir)) != 0) {
        size_t n;

        n = strlen(entry->d_name);
        if (n < 5 || strcmp(entry->d_name + n - 4, ".QBN") || n > 12) { continue; }
        memcpy(name, entry->d_name, n - 4); name[n - 4] = 0;
        files++;
        if (!qbn_load(name, &q)) { bad++; check_expect(0, name); }
    }
    if (dir) { closedir(dir); }
    check_expect(files == 232 && !bad, "quest: not every one of the 232 QBN files parsed");
    /* M0B00Y00 as the document decodes it: one item, one person, two places, one timer, three foes, twelve states, twenty ops. */
    check_expect(qbn_load("M0B00Y00", &q) && q.n_item == 1 && q.n_person == 1 && q.n_place == 2 && q.n_timer == 1 && q.n_foe == 3 && q.n_state == 12 && q.n_op == 20,
                 "quest: M0B00Y00 is not 1 item, 1 person, 2 places, 1 timer, 3 foes, 12 states and 20 ops");
    check_expect(q.item[0].flags == 2 && q.item[0].symbol == qbn_hash("reward") && q.person[0].kind == 21 && q.person[0].symbol == qbn_hash("questgiver") &&
                 q.place[0].scope == 2 && q.place[0].symbol == qbn_hash("mondung") && q.timer[0].flags == 0x11 && q.timer[0].type == 2 && q.timer[0].hash == qbn_hash("1stparton") &&
                 q.foe[0].type == 9 && q.foe[1].type == 14 && q.foe[2].type == 4,
                 "quest: M0B00Y00's resources are not the ones the document names");
    check_expect(q.op[0].opcode == 12 && q.op[1].opcode == 23 && q.op[1].arg[1].value == 1030 && q.op[2].opcode == 34 && q.op[18].opcode == 6 && q.op[19].opcode == 6 &&
                 qbn_hash("reward") == 6932 && qbn_hash("questgiver") == 115084 && q.state[3].hash == qbn_hash("1stparton"),
                 "quest: M0B00Y00's ops are not 12, 23, 34 ... 6, 6, or the hashes are not 6932 and 115084");
    /* A quest built in memory: S0 set by a random pick of itself, S1 = S0 and not S2, a message when S1, the end when S1. */
    memset(file, 0, sizeof(file));
    put16(file + 0x10 + 8 * 2, 4); put16(file + 0x10 + 9 * 2, 3);
    put16(file + 0x24 + 8 * 2, 0x80); put16(file + 0x24 + 9 * 2, 0x80 + 4 * 87);
    for (i = 0; i < 3; i++) { put16(file + 0x80 + 4 * 87 + i * 8, i); put32(file + 0x80 + 4 * 87 + i * 8 + 4, (df_u32)(100 + i)); }
    {
        int a34[5] = {-1, 0, 0, -2, -2}, a52[5] = {1, 0, 2, -2, -2}, n52[5] = {0, 0, 1, 0, 0}, a51[5] = {1, -2, -2, -2, -2}, a6[5] = {1, -2, -2, -2, -2};

        put_op(file + 0x80, 34, -1, a34, 0);
        put_op(file + 0x80 + 87, 52, -1, a52, n52);
        put_op(file + 0x80 + 174, 51, 1030, a51, 0);
        put_op(file + 0x80 + 261, 6, -1, a6, 0);
    }
    check_expect(qbn_parse(file, 0x80 + 4 * 87 + 3 * 8, &q) && q.n_op == 4 && q.n_state == 3 && q.op[1].argc == 5 && q.op[1].arg[2].negate == 1, "quest: the quest made in memory did not parse");
    /* Run it as a started quest: the file path needs a file, so the slot is made from the parsed bytes by writing it where qbn_load reads (a test file beside ARENA2). */
    quest_set_text(give_text); quest_set_say(take_say);
    quest_reset();
    questor(1);
    slot = quest_start("M0B00Y00", 8);
    questor(2);
    check_expect(slot == 0 && quest_at(0)->qbn.id == 1 && quest_at(0)->qbn.faction == 8 && quest_start("M0B00Y00", 8) == 1 && quest_at(1)->qbn.id == 2, "quest: started quests did not get ids 1 and 2 and the faction");
    g_said = 0;
    quest_run();
    {
        int set6, set7, set8;

        /* Op 2 picks one of S9, S10 (three times), once. */
        set6 = quest_state(0, 9) + quest_state(0, 10);
        check_expect(set6 == 1, "quest: op 34 did not set exactly one of its given states");
        set7 = quest_state(0, 9); set8 = quest_state(0, 10);
        quest_run(); quest_run();
        check_expect(quest_state(0, 9) == set7 && quest_state(0, 10) == set8, "quest: op 34 ran again after its once");
    }
    /* An op 52 with an unused operand is not held false by it; op 51 says its message once; op 6 ends the quest. */
    quest_end(1);
    quest_end(0);
    g_said = 0; g_asked = -1;
    slot = quest_start_bytes("TEST0001", file, 0x80 + 4 * 87 + 3 * 8, 0);
    check_expect(slot == 0, "quest: the quest made in memory did not start");
    quest_run();
    check_expect(g_said == 1 && g_asked == 1006 && !strcmp(g_text, "Well done.") && !quest_at(0), "quest: S1 = S0 and not S2 did not say its message once and end the quest");
    /* Step 9's first ops in a quest made in memory: 29 asks yes or no and the answer sets a state, 65 changes the region's legal reputation once when that state is set, 80 says its message
     * while its state is set, and 82 lets a person's talk pass while set. */
    {
        static df_u8 f2[4096];
        int a29[5] = {-1, 1, 2, -2, -2}, a65[5] = {1, 0, -2, -2, -2}, a80[5] = {2, -2, -2, -2, -2}, a6[5] = {1, -2, -2, -2, -2};
        int before;

        quest_end(0);
        memset(f2, 0, sizeof(f2));
        put16(f2 + 0x10 + 8 * 2, 4); put16(f2 + 0x10 + 9 * 2, 3);
        put16(f2 + 0x24 + 8 * 2, 0x80); put16(f2 + 0x24 + 9 * 2, 0x80 + 4 * 87);
        for (i = 0; i < 3; i++) { put16(f2 + 0x80 + 4 * 87 + i * 8, i); put32(f2 + 0x80 + 4 * 87 + i * 8 + 4, (df_u32)(100 + i)); }
        put_op(f2 + 0x80, 29, 1040, a29, 0);
        put_op(f2 + 0x80 + 87, 65, -1, a65, 0); put_lit(f2 + 0x80 + 87 + 6 + 15, -5);
        put_op(f2 + 0x80 + 174, 80, 1041, a80, 0);
        put_op(f2 + 0x80 + 261, 6, -1, a6, 0);
        quest_set_ask(take_ask); g_answer = 0; quest_set_world(0);
        slot = quest_start_bytes("TEST0029", f2, 0x80 + 4 * 87 + 3 * 8, 0);
        before = region_legal(0);
        quest_run();
        check_expect(slot == 0 && g_answer != 0 && quest_state(0, 1) == 0, "quest: op 29 did not open its box, or answered before it was");
        g_said = 0;
        g_answer(1);
        quest_run();
        check_expect(region_legal(0) == before - 5, "quest: yes did not set its state, or op 65 did not lower the region's legal reputation by 5");
        quest_run();
        check_expect(region_legal(0) == before - 5 && !quest_at(0), "quest: op 65 ran twice, or op 6 did not end the quest");
        quest_end(0);
        region_add_legal(0, 5);
    }
    /* Items: one made when the quest begins and hung from the quest tree; op 26 gives it (the pack's GIVE event sets op 3's state), op 70 sees it carried, op 36 deletes it, and a quest that
     * ends takes what it still holds. */
    {
        static df_u8 f3[4096];
        int a3[5] = {1, 0, -2, -2, -2}, a70[5] = {-1, 2, 0, -2, -2}, a26[5] = {0, 0, -2, -2, -2}, a36[5] = {3, 0, -2, -2, -2};
        df_u32 id;
        int saved_str;

        quest_end(0);
        memset(f3, 0, sizeof(f3));
        put16(f3 + 0x10 + 0 * 2, 1); put16(f3 + 0x24 + 0 * 2, 0x80);
        put16(f3 + 0x10 + 8 * 2, 4); put16(f3 + 0x24 + 8 * 2, 0x100);
        put16(f3 + 0x10 + 9 * 2, 4); put16(f3 + 0x24 + 9 * 2, 0x100 + 4 * 87);
        put16(f3 + 0x80 + 3, 3); put16(f3 + 0x80 + 5, 0);
        for (i = 0; i < 4; i++) { put16(f3 + 0x100 + 4 * 87 + i * 8, i); put32(f3 + 0x100 + 4 * 87 + i * 8 + 4, (df_u32)(100 + i)); }
        put_op(f3 + 0x100, 3, -1, a3, 0);
        put_op(f3 + 0x100 + 87, 70, -1, a70, 0);
        put_op(f3 + 0x100 + 174, 26, -1, a26, 0);
        put_op(f3 + 0x100 + 261, 36, -1, a36, 0);
        quest_set_world(0);
        /* The pack an earlier test filled is heavy: a strength that carries it. */
        saved_str = character_state()->attribute[ATTR_STR];
        character_state()->attribute[ATTR_STR] = 5000;
        slot = quest_start_bytes("TEST0003", f3, 0x100 + 4 * 87 + 4 * 8, 0);
        check_expect(slot == 0 && quest_at(0)->item[0].object != 0 && !carried_by_player(quest_at(0)->item[0].object), "quest: the quest's item was not made, or the player already has it");
        if (slot == 0 && quest_at(0)->item[0].object) {
            id = quest_at(0)->item[0].object;
            quest_run();
            check_expect(quest_state(0, 2) == 0 && quest_state(0, 1) == 0, "quest: op 70 saw an item the player does not have");
            quest_set_state(0, 0, 1);
            quest_run(); quest_run();
            check_expect(carried_by_player(id) && quest_state(0, 1) == 1 && quest_state(0, 2) == 1, "quest: op 26 did not give the item, or ops 3 and 70 did not see it");
            quest_set_state(0, 3, 1);
            quest_run();
            check_expect(!object_at(id) && quest_at(0)->item[0].object == 0, "quest: op 36 did not delete the item");
        }
        quest_end(0);
        slot = quest_start_bytes("TEST0003", f3, 0x100 + 4 * 87 + 4 * 8, 0);
        if (slot == 0 && quest_at(0)->item[0].object) {
            id = quest_at(0)->item[0].object;
            quest_set_state(0, 0, 1);
            quest_run();
            quest_end(0);
            check_expect(!object_at(id), "quest: a quest that ended left its item in the pack");
        }
        /* Op 57 sees an item used (flag 512), and op 1 takes the item carried from the player when its person (a faction's, here 7) is talked to. */
        {
            int b26[5] = {0, 0, -2, -2, -2}, b57[5] = {-1, 1, 0, -2, -2}, b1[5] = {2, 0, 0, -2, -2};

            quest_end(0);
            memset(f3, 0, sizeof(f3));
            put16(f3 + 0x10 + 0 * 2, 1); put16(f3 + 0x24 + 0 * 2, 0x80);
            put16(f3 + 0x10 + 3 * 2, 1); put16(f3 + 0x24 + 3 * 2, 0xC0);
            put16(f3 + 0x10 + 8 * 2, 3); put16(f3 + 0x24 + 8 * 2, 0x100);
            put16(f3 + 0x10 + 9 * 2, 3); put16(f3 + 0x24 + 9 * 2, 0x100 + 3 * 87);
            put16(f3 + 0x80 + 3, 3); put16(f3 + 0x80 + 5, 0);
            put16(f3 + 0xC0 + 4, (df_u32)-8); put16(f3 + 0xC0 + 6, 7);
            for (i = 0; i < 3; i++) { put16(f3 + 0x100 + 3 * 87 + i * 8, i); put32(f3 + 0x100 + 3 * 87 + i * 8 + 4, (df_u32)(100 + i)); }
            put_op(f3 + 0x100, 26, -1, b26, 0);
            put_op(f3 + 0x100 + 87, 57, -1, b57, 0);
            put_op(f3 + 0x100 + 174, 1, -1, b1, 0);
            slot = quest_start_bytes("TEST0057", f3, 0x100 + 3 * 87 + 3 * 8, 0);
            if (slot == 0 && quest_at(0)->item[0].object) {
                id = quest_at(0)->item[0].object;
                quest_set_state(0, 0, 1);
                quest_run();
                quest_run();
                check_expect(carried_by_player(id) && quest_state(0, 1) == 0, "quest: op 57 saw an item that was not used");
                object_edit(id)->flags |= 512;
                quest_run();
                check_expect(quest_state(0, 1) == 1, "quest: op 57 did not see the item used");
                check_expect(!quest_talk(1, 9) && object_at(id), "quest: another faction's person took the item");
                check_expect(quest_talk(1, 7) && !object_at(id) && quest_state(0, 2) == 1 && quest_at(0)->item[0].object == 0, "quest: op 1 did not take the item for the person and set its state");
            } else { check_expect(0, "quest: the item quest did not start"); }
            quest_end(0);
        }
        /* Topics, rumours, faces and the countdown: op 11 hides the item's topic and op 10 shows it again, op 25 counts three runs, ops 55 and 56 put a face up and take it away, and the quest's start
         * left a rumour that people tell as news. */
        {
            int c11[5] = {0, -1, -1, 0, -2}, c10[5] = {1, -1, -1, 0, -2}, c25[5] = {-1, 2, 0, -2, -2}, c55[5] = {3, 0, -2, -2, -2}, c56[5] = {4, 0, -2, -2, -2};
            char text[300], name[64];

            quest_end(0);
            memset(f3, 0, sizeof(f3));
            put16(f3 + 0x10 + 0 * 2, 1); put16(f3 + 0x24 + 0 * 2, 0x80);
            put16(f3 + 0x10 + 3 * 2, 1); put16(f3 + 0x24 + 3 * 2, 0xC0);
            put16(f3 + 0x10 + 8 * 2, 5); put16(f3 + 0x24 + 8 * 2, 0x100);
            put16(f3 + 0x10 + 9 * 2, 5); put16(f3 + 0x24 + 9 * 2, 0x100 + 5 * 87);
            put16(f3 + 0x80 + 3, 3); put16(f3 + 0x80 + 5, 0); put16(f3 + 0x80 + 15, 1050);
            put16(f3 + 0xC0 + 4, (df_u32)-8); put16(f3 + 0xC0 + 6, 7);
            for (i = 0; i < 5; i++) { put16(f3 + 0x100 + 5 * 87 + i * 8, i); put32(f3 + 0x100 + 5 * 87 + i * 8 + 4, (df_u32)(100 + i)); }
            put_op(f3 + 0x100, 11, -1, c11, 0);
            put_op(f3 + 0x100 + 87, 10, -1, c10, 0);
            put_op(f3 + 0x100 + 174, 25, -1, c25, 0); put_lit(f3 + 0x100 + 174 + 6 + 30, 3);
            put_op(f3 + 0x100 + 261, 55, -1, c55, 0);
            put_op(f3 + 0x100 + 348, 56, -1, c56, 0);
            quest_set_names(0);
            slot = quest_start_bytes("TEST0010", f3, 0x100 + 5 * 87 + 5 * 8, 0);
            check_expect(slot == 0 && quest_news(0, text, sizeof(text)) == 1, "quest: the quest's start left no rumour to tell as news");
            check_expect(quest_topic_count() == 1 && quest_topic_answer(0, text, sizeof(text)) == 1 && text[0], "quest: the item with a message was not a topic with an answer");
            quest_run(); quest_run();
            check_expect(quest_state(0, 2) == 0, "quest: op 25 set its flag before three runs");
            quest_run();
            check_expect(quest_state(0, 2) == 1, "quest: op 25 did not set its flag at the third run");
            quest_set_state(0, 0, 1);
            quest_run();
            check_expect(quest_topic_count() == 0, "quest: op 11 did not hide the topic");
            quest_set_state(0, 1, 1);
            quest_run();
            check_expect(quest_topic_count() == 1, "quest: op 10 did not show the topic again");
            quest_set_state(0, 3, 1);
            quest_run();
            check_expect(quest_face(0) && quest_face(0)->kind == 1 && quest_face(0)->index == 0, "quest: op 55 did not put the person's face up");
            quest_set_state(0, 4, 1);
            quest_run();
            check_expect(!quest_face(0), "quest: op 56 did not take the face away");
            quest_set_state(0, 3, 1);
            quest_end(0);
            {
                int r, left;

                left = 0;
                for (r = 0; r < RUMOURS; r++) { if (rumour_at(r) && !strcmp(rumour_at(r)->name, "TEST0010")) { left++; } }
                check_expect(left == 0, "quest: a quest that never finished left its start rumour");
            }
            (void)name;
        }
        /* Op 9 repeats a foe (every ten minutes, twice in all, two of them each time), op 17 grants access to a place's building, op 30 puts the questor in a place and takes the player to it,
         * op 81 marks the place and person open, op 8 starts a child quest. */
        {
            int d9[5] = {-1, 0, 0, 0, 0}, d17[5] = {1, 0, -2, -2, -2}, d30[5] = {2, 0, 0, -2, -2}, d30b[5] = {5, -1, 0, -2, -2}, d81[5] = {4, 0, 0, -2, -2}, d8[5] = {3, -2, -2, -2, -2};
            int before_spawn;

            quest_end(0);
            memset(f3, 0, sizeof(f3));
            put16(f3 + 0x10 + 3 * 2, 1); put16(f3 + 0x24 + 3 * 2, 0xC0);
            put16(f3 + 0x10 + 4 * 2, 1); put16(f3 + 0x24 + 4 * 2, 0xE0);
            put16(f3 + 0x10 + 7 * 2, 1); put16(f3 + 0x24 + 7 * 2, 0x120);
            put16(f3 + 0x10 + 8 * 2, 6); put16(f3 + 0x24 + 8 * 2, 0x140);
            put16(f3 + 0x10 + 9 * 2, 6); put16(f3 + 0x24 + 9 * 2, 0x140 + 6 * 87);
            put16(f3 + 0xC0 + 4, 21); put16(f3 + 0xC0 + 6, 7);
            f3[0xE0 + 3] = 2; put16(f3 + 0xE0 + 4, 0); put16(f3 + 0xE0 + 6, 15); put16(f3 + 0xE0 + 8, 0);
            f3[0x120 + 3] = 4; f3[0x120 + 4] = 2;
            for (i = 0; i < 6; i++) { put16(f3 + 0x140 + 6 * 87 + i * 8, i); put32(f3 + 0x140 + 6 * 87 + i * 8 + 4, (df_u32)(100 + i)); }
            put_op(f3 + 0x140, 9, -1, d9, 0);
            put_lit(f3 + 0x140 + 6 + 2 * 15, 10); put_lit(f3 + 0x140 + 6 + 3 * 15, 100); put_lit(f3 + 0x140 + 6 + 4 * 15, 2);
            put_op(f3 + 0x140 + 87, 17, -1, d17, 0);
            put_lit(f3 + 0x140 + 87 + 6 + 2 * 15, 4); put_lit(f3 + 0x140 + 87 + 6 + 3 * 15, 100);
            put_op(f3 + 0x140 + 174, 30, -1, d30, 0);
            put_op(f3 + 0x140 + 261, 30, -1, d30b, 0);
            put_op(f3 + 0x140 + 348, 81, -1, d81, 0);
            put_op(f3 + 0x140 + 435, 8, -1, d8, 0);
            put_lit(f3 + 0x140 + 435 + 6 + 1 * 15, 101); put_lit(f3 + 0x140 + 435 + 6 + 2 * 15, 101);
            {
                static const QuestWorld w9 = { w_count, w_location, w_here, w_region, w_journey, w_reveal, w_spawn, w_go, w_place_item, w_find_number, 0, 0, 0, w_pick_building, w_pick_site, w_person_name, w_outdoors };

                quest_set_world(&w9);
            }
            questor(11);
            g_spawned = 0;
            slot = quest_start_bytes("TEST0009", f3, 0x140 + 6 * 87 + 6 * 8, 0);
            check_expect(slot == 0 && quest_at(0)->place[0].bound, "quest: the repeat quest did not start with its place");
            if (slot == 0) {
                quest_run();
                before_spawn = g_spawned;
                state_pass_minutes(10);
                check_expect(g_spawned == before_spawn + 2, "quest: op 9 did not place two foes after ten minutes");
                state_pass_minutes(10);
                state_pass_minutes(10);
                check_expect(g_spawned == before_spawn + 4, "quest: op 9 placed more than its two times, or fewer");
                quest_set_state(0, 1, 1);
                quest_run();
                check_expect(quest_access(0) && quest_access(0)->access == 4 && quest_access(0)->index == quest_at(0)->place[0].index && quest_access(0)->block == 3 &&
                             quest_building_access(17, quest_at(0)->place[0].index, 3, 7, game_state()->minutes) == 4 && quest_building_access(17, quest_at(0)->place[0].index, 3, 8, game_state()->minutes) == 0 &&
                             quest_building_access(17, quest_at(0)->place[0].index, 3, 7, game_state()->minutes + 200u) == 0, "quest: op 17 did not grant the building's access for its minutes");
                quest_set_state(0, 2, 1);
                quest_run();
                check_expect(quest_at(0)->person[0].at == 1, "quest: op 30 did not put the person in the place");
                quest_set_state(0, 5, 1);
                g_went_region = -1;
                quest_run();
                check_expect(g_went_region == quest_at(0)->place[0].region && g_went_index == quest_at(0)->place[0].index, "quest: op 30 did not take the player to the person's place");
                quest_set_state(0, 4, 1);
                quest_run();
                check_expect((quest_at(0)->qbn.place[0].flags & 64) && (quest_at(0)->qbn.person[0].flags & 0x4000) && quest_building_open(17, quest_at(0)->place[0].index, 3, 7) && !quest_building_open(17, quest_at(0)->place[0].index, 3, 8),
                             "quest: op 81 did not mark the place and the person, or their building is not open to the quest");
                quest_set_state(0, 3, 1);
                quest_run();
                check_expect(quest_at(1) && quest_at(1)->qbn.name[0] == 'S' && quest_at(1)->qbn.name[7] == '1', "quest: op 8 did not start the child quest S0000101");
            }
            quest_set_world(0);
            {
                int k;

                for (k = 0; k < quest_count(); k++) { quest_end(k); }
            }
        }
        /* Who offers what: a person's seed picks the same quest each time, a temple's letter picks its quests and a Julianos one the 0 file, a non-member of a temple the C quests. */
        {
            char a[9], b[9], c[9];

            check_expect(quest_pick_seeded('A', 'K', 'C', 9, 77u, a) && quest_pick_seeded('A', 'K', 'C', 9, 77u, b) && !strcmp(a, b) && (a[0] == 'A' || a[0] == 'K') && a[2] == 'C',
                         "quest: a person's seed did not pick the same commoner's quest each time");
            check_expect(quest_pick_seeded('E', 0, 'B', 0, 0u, c) && c[0] == 'E' && c[2] == 'B' && quest_pick('0', 0, 'B', 0, c) && c[0] == '0' && c[2] == 'B' && quest_pick('E', 'C', 'B', 0, c) && (c[0] == 'E' || c[0] == 'C'),
                         "quest: Arkay's member's quest is not E0B, or Julianos's not 00B");
            check_expect(quest_pick('R', 0, 'C', 9, c) && c[0] == 'R' && c[2] == 'C', "quest: a type 4 faction's person's quest is not an R quest");
        }
        /* The quests of a new game: _BRISIEN then _TUTOR__ begin at the first run, their fixed places found by the location's number (low word 2 read as the location), _BRISIEN's person made
         * (kind -1, her flags 0x101) though standing nowhere, and its ops 6 and 7, which wait while the player is in the hold (op 43 sets state 5 there), start the main story's S0000977 and S0000999 once they are out of it. */
        {
            static const QuestWorld w10 = { w_count, w_location, w_here_out, w_region, w_journey, w_reveal, w_spawn, w_go, w_place_item, w_find_number, 0, 0, 0, w_pick_building, w_pick_site, w_person_name, w_outdoors };
            int k, found999, found977;

            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            quest_set_world(&w10);
            quest_start_initial_later();
            quest_run(); quest_run(); quest_run();
            check_expect(quest_at(0) && !strcmp(quest_at(0)->qbn.name, "_BRISIEN") && (!quest_at(1) || strcmp(quest_at(1)->qbn.name, "_TUTOR__")), "quest: a new game did not start _BRISIEN, or started the tutorial");
            check_expect(quest_at(0) && quest_at(0)->place[0].bound && quest_at(0)->place[0].index == 10 && quest_at(0)->place[0].marker == 0xFA01 && quest_at(0)->person[0].bound && quest_at(0)->person[0].who != 0,
                         "quest: _BRISIEN's fixed place or her person did not bind");
            check_expect(quest_at(0) && quest_at(0)->place[0].marker == 0xFA01, "quest: _BRISIEN's place 0 is not the hold's first quest marker");
            found999 = 0; found977 = 0;
            for (k = 0; k < quest_count(); k++) {
                if (quest_at(k) && !strcmp(quest_at(k)->qbn.name, "S0000999")) { found999 = 1; }
                if (quest_at(k) && !strcmp(quest_at(k)->qbn.name, "S0000977")) { found977 = 1; }
            }
            check_expect(found999 && found977, "quest: _BRISIEN out of the hold did not start S0000977 and S0000999");
            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            quest_set_world(0);
        }
        /* A person with a body: Brisienna's kind (-1, a faction of an individual, flags 0x101: any building, a woman asked for by neither sex bit so tossed) put by op 30 in a tavern of another
         * town stands in that building (block and record) when it is entered, with her name, and her flat's tag names her. */
        {
            static const QuestWorld w11 = { w_count, w_location, w_here, w_region, w_journey, w_reveal, w_spawn, w_go, w_place_item, w_find_number, 0, 0, 0, w_pick_building, w_pick_site, w_person_name, w_outdoors };
            int e30[5] = {-1, 0, 0, -2, -2};
            QuestHere here[4];
            int who, faction, female, tag, k;
            char name[40];

            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            memset(f3, 0, sizeof(f3));
            put16(f3 + 0x10 + 3 * 2, 1); put16(f3 + 0x24 + 3 * 2, 0xC0);
            put16(f3 + 0x10 + 4 * 2, 1); put16(f3 + 0x24 + 4 * 2, 0xE0);
            put16(f3 + 0x10 + 8 * 2, 1); put16(f3 + 0x24 + 8 * 2, 0x140);
            put16(f3 + 0x10 + 9 * 2, 1); put16(f3 + 0x24 + 9 * 2, 0x140 + 87);
            put16(f3 + 0xC0 + 2, 0x201); put16(f3 + 0xC0 + 4, (df_u32)-1); put16(f3 + 0xC0 + 6, 352);
            f3[0xE0 + 3] = 2; put16(f3 + 0xE0 + 4, 0); put16(f3 + 0xE0 + 6, 15); put16(f3 + 0xE0 + 8, 0);
            put16(f3 + 0x140 + 87, 0); put32(f3 + 0x140 + 87 + 4, 100u);
            put_op(f3 + 0x140, 30, -1, e30, 0);
            quest_set_world(&w11);
            slot = quest_start_bytes("TEST0030", f3, 0x140 + 87 + 8, 0);
            check_expect(slot == 0 && quest_at(0)->person[0].bound && quest_at(0)->person[0].female == 1 && !strcmp(quest_at(0)->person[0].name, "Lady Test") && quest_at(0)->place[0].block == 3,
                         "quest: the person (a woman, named) or the tavern's building were not bound");
            if (slot == 0) {
                check_expect(quest_people_in(17, quest_at(0)->place[0].index, 3, 7, here, 4) == 0, "quest: the person stood in the building before op 30 put her there");
                quest_run();
                check_expect(quest_people_in(17, quest_at(0)->place[0].index, 3, 7, here, 4) == 1 && here[0].slot == 0 && here[0].person == 0 && here[0].faction == 352 && here[0].female == 1,
                             "quest: op 30 did not stand the person in the building");
                check_expect(quest_people_in(17, quest_at(0)->place[0].index, 3, 8, here, 4) == 0, "quest: the person stood in another building of the town");
                tag = QUEST_PERSON_TAG(0, 0);
                check_expect(quest_person_of_tag(tag, &who, &faction, &female, name, sizeof(name)) && faction == 352 && female == 1 && !strcmp(name, "Lady Test") && who == quest_at(0)->person[0].who,
                             "quest: the person's flat tag did not name her");
            }
            {
                char surname[24];
                TalkBuilding rows[3];
                TalkTopic topics[8];
                int nt, listed, q;

                check_expect(quest_building_site(17, quest_at(0)->person[0].home_index, 3, 7, surname, sizeof(surname)) == 1 && !strcmp(surname, "Test") && quest_building_site(17, quest_at(0)->person[0].home_index, 3, 8, surname, sizeof(surname)) == 0,
                             "quest: a building a quest person stands in is not a quest site named for their surname, or another one is");
                /* The talk window lists a house that is a quest site (flag 8) under category 13, and no other house. */
                memset(rows, 0, sizeof(rows));
                rows[0].id = 1; rows[0].type = 17; rows[0].flags = 8; rows[0].distance = 10;
                rows[1].id = 2; rows[1].type = 17; rows[1].flags = 0; rows[1].distance = 20;
                rows[2].id = 3; rows[2].type = 15; rows[2].flags = 0; rows[2].distance = 30;
                nt = talk_topics(topics, 8, rows, 3);
                listed = 0;
                for (q = 0; q < nt; q++) { if (topics[q].building == 1 && topics[q].category == 13) { listed++; } if (topics[q].building == 2) { listed += 10; } }
                check_expect(listed == 1, "quest: a house that is a quest site was not listed under category 13 in Where is, or a plain house was");
            }
            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            quest_set_world(0);
        }
        /* A foe on its place's quest marker: with the floor not loaded it waits (nothing made), and when it is loaded both are made on the marker with the foe's keys, once. */
        {
            static const QuestWorld w12 = { w_count, w_location, w_here, w_region, w_journey, w_reveal, w_spawn, w_go, w_place_item, w_find_number, w_marker_at, w_spawn_at, w_place_at, w_pick_building, w_pick_site, w_person_name, w_outdoors };
            int f22[5] = {-1, 0, 0, -2, -2}, k;

            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            memset(f3, 0, sizeof(f3));
            put16(f3 + 0x10 + 4 * 2, 1); put16(f3 + 0x24 + 4 * 2, 0xE0);
            put16(f3 + 0x10 + 7 * 2, 1); put16(f3 + 0x24 + 7 * 2, 0x120);
            put16(f3 + 0x10 + 8 * 2, 1); put16(f3 + 0x24 + 8 * 2, 0x140);
            put16(f3 + 0x10 + 9 * 2, 1); put16(f3 + 0x24 + 9 * 2, 0x140 + 87);
            f3[0xE0 + 3] = 1;
            f3[0x120 + 3] = 4; f3[0x120 + 4] = 2;
            put16(f3 + 0x140 + 87, 0); put32(f3 + 0x140 + 87 + 4, 100u);
            put_op(f3 + 0x140, 22, -1, f22, 0);
            quest_set_world(&w12);
            g_floor = 0; g_at_made = 0; g_spawned = 0;
            slot = quest_start_bytes("TEST0022", f3, 0x140 + 87 + 8, 0);
            quest_run();
            check_expect(slot == 0 && quest_at(0)->foe[0].placed && quest_at(0)->foe[0].made == 0 && g_at_made == 0 && g_spawned == 0, "quest: a foe was made where there was no floor yet, or near the player instead of its marker");
            g_floor = 1;
            quest_run(); quest_run();
            check_expect(quest_at(0)->foe[0].made == 2 && g_at_made == 2 && g_at_x == 1000.0f && g_at_z == 2000.0f && g_at_thing[0] == QUEST_FOE_THING(quest_at(0)->foe[0].number, 0) && g_at_thing[1] == QUEST_FOE_THING(quest_at(0)->foe[0].number, 1),
                         "quest: the foe's two creatures were not made on its quest marker with their keys, once");
            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            quest_set_world(0);
        }
        /* Persons that pick a site at the start: a tavern (kind 15) in the town the player is in, a person linked to it by their faction (-5, anywhere), and a faction of a type in another town (-2, low
         * byte 1): each homed in a building of the town, the second asking for buildings of the first's faction. */
        {
            static const QuestWorld w13 = { w_count, w_location, w_here, w_region, w_journey, w_reveal, w_spawn, w_go, w_place_item, w_find_number, 0, 0, 0, w_pick_building, w_pick_site, w_person_name, w_outdoors };
            const QuestState *pq;
            int k;

            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            memset(f3, 0, sizeof(f3));
            put16(f3 + 0x10 + 3 * 2, 3); put16(f3 + 0x24 + 3 * 2, 0xC0);
            put16(f3 + 0x10 + 8 * 2, 1); put16(f3 + 0x24 + 8 * 2, 0x140);
            put16(f3 + 0x10 + 9 * 2, 1); put16(f3 + 0x24 + 9 * 2, 0x140 + 87);
            put16(f3 + 0xC0 + 2, 0); put16(f3 + 0xC0 + 4, 15); put16(f3 + 0xC0 + 6, 0);
            put16(f3 + 0xC0 + 20 + 2, 0x100); put16(f3 + 0xC0 + 20 + 4, (df_u32)-5); put16(f3 + 0xC0 + 20 + 6, 0);
            put16(f3 + 0xC0 + 40 + 2, 0x101); put16(f3 + 0xC0 + 40 + 4, (df_u32)-2); put16(f3 + 0xC0 + 40 + 6, 8);
            put16(f3 + 0x140 + 87, 0); put32(f3 + 0x140 + 87 + 4, 100u);
            put_op(f3 + 0x140, 51, -1, a51_none, 0);
            quest_set_world(&w13);
            g_site_mode = -1;
            slot = quest_start_bytes("TEST0015", f3, 0x140 + 87 + 8, 0);
            pq = quest_at(0);
            check_expect(slot == 0 && pq && pq->person[0].homed && pq->person[0].home_index == 10 && pq->person[0].home_block == 5 && pq->person[0].home_record == 9 && pq->person[0].faction == 4242,
                         "quest: a tavern person was not homed in a building of the town the player is in, with the building's faction");
            check_expect(pq && pq->person[1].homed && pq->person[1].faction == 4242, "quest: the linked person (-5) did not take the first one's faction and a site");
            check_expect(pq && pq->person[2].homed && pq->person[2].home_index != 10, "quest: the person asked for in another town (low byte 1) was homed in the town the player is in");
            {
                QuestHere in[4];

                check_expect(pq && quest_people_in(17, 10, 5, 9, in, 4) == 2 && in[0].takeover == 1 && in[1].takeover == 0, "quest: the town's building 5, 9 does not hold the two persons homed there, the tavern's one taking over a person of it");
            }
            g_site_none = 1;
            check_expect(quest_start_bytes("TEST0015", f3, 0x140 + 87 + 8, 0) == -1, "quest: a quest whose person has no site anywhere was started");
            g_site_none = 0;
            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            quest_set_world(0);
        }
        /* A dungeon action's global state (Mantellan Crux's sets 36): set, kept in the globals and through a pack. */
        {
            static df_u8 gb[QUEST_BLOB];
            int len;

            quest_globals_clear();
            quest_global_set(36); quest_global_set(99);
            len = quest_pack(gb, sizeof(gb));
            quest_globals_clear();
            check_expect(quest_globals()[36] == 0 && quest_unpack(gb, len) == 1 && quest_globals()[36] == 1 && quest_globals()[35] == 0, "quest: a global state set by a dungeon action was not kept");
            quest_globals_clear();
            questor(1);
        }
        /* The guilds' invitations: a Thieves Guild invitation falling due out of doors starts O0A0AL00 (no prompt), and its op 4 joins the guild instead of rewarding. */
        {
            static const QuestWorld w14 = { w_count, w_location, w_here, w_region, w_journey, w_reveal, w_spawn, w_go, w_place_item, w_find_number, 0, 0, 0, w_pick_building, w_pick_site, w_person_name, w_outdoors };
            int k, started;

            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            guilds_clear();
            quest_set_world(&w14);
            guilds_state()->thieves_count = 6; guilds_state()->thieves_invite = game_state()->minutes - 1u;
            g_out = 0;
            quest_run();
            check_expect(!quest_at(0), "quest: an invitation came indoors");
            g_out = 1;
            quest_run();
            started = 0;
            for (k = 0; k < quest_count(); k++) { if (quest_at(k) && !strcmp(quest_at(k)->qbn.name, "O0A0AL00")) { started = 1; } }
            check_expect(started && guilds_state()->thieves_count == 100 && guilds_state()->thieves_invite == 0, "quest: the Thieves Guild's invitation quest was not started out of doors, or its counter was not closed");
            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            g_out = 0;
            quest_set_world(0);
        }
        /* A reward item (op 4) goes into the pack and is the quest's no more; a letter (group 9, index 5) reads its QRC message from the quest that holds it. */
        {
            int g4[5] = {-1, 0, -2, -2, -2}, k;
            char words[300];
            df_u32 reward, letter;

            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            memset(f3, 0, sizeof(f3));
            put16(f3 + 0x10 + 0 * 2, 2); put16(f3 + 0x24 + 0 * 2, 0x80);
            put16(f3 + 0x10 + 8 * 2, 1); put16(f3 + 0x24 + 8 * 2, 0x140);
            put16(f3 + 0x10 + 9 * 2, 1); put16(f3 + 0x24 + 9 * 2, 0x140 + 87);
            put16(f3 + 0x80 + 3, 3); put16(f3 + 0x80 + 5, 0);
            put16(f3 + 0x80 + 19 + 3, 9); put16(f3 + 0x80 + 19 + 5, 5); put16(f3 + 0x80 + 19 + 17, 1020);
            put16(f3 + 0x140 + 87, 0); put32(f3 + 0x140 + 87 + 4, 100u);
            put_op(f3 + 0x140, 4, -1, g4, 0);
            slot = quest_start_bytes("TEST0004", f3, 0x140 + 87 + 8, 0);
            check_expect(slot == 0 && quest_at(0)->item[0].object && quest_at(0)->item[1].object, "quest: the reward item or the letter was not made");
            if (slot == 0) {
                reward = quest_at(0)->item[0].object; letter = quest_at(0)->item[1].object;
                check_expect(object_at(letter) && object_at(letter)->message == 1020 && quest_item_text(letter, words, sizeof(words)) == 1 && words[0] && !quest_item_text(reward, words, sizeof(words)),
                             "quest: a letter's message was not its QRC text, or a plain item had one");
                quest_run();
                check_expect(carried_by_player(reward) && quest_at(0)->item[0].object == 0 && quest_at(0)->item[1].object == letter, "quest: op 4 did not put the reward item in the pack and let the quest forget it");
            }
            for (k = 0; k < quest_count(); k++) { quest_end(k); }
        }
        /* A quest person put in a dungeon place (op 30) stands on its quest marker: homed in the dungeon (block -2) with the marker's point. */
        {
            static const QuestWorld w15 = { w_count, w_location, w_here, w_region, w_journey, w_reveal, w_spawn, w_go, w_place_item, w_find_number, w_marker_at, w_spawn_at, w_place_at, w_pick_building, w_pick_site, w_person_name, w_outdoors };
            int h30[5] = {-1, 0, 0, -2, -2}, k;
            QuestHere dh[4];

            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            memset(f3, 0, sizeof(f3));
            put16(f3 + 0x10 + 3 * 2, 1); put16(f3 + 0x24 + 3 * 2, 0xC0);
            put16(f3 + 0x10 + 4 * 2, 1); put16(f3 + 0x24 + 4 * 2, 0xE0);
            put16(f3 + 0x10 + 8 * 2, 1); put16(f3 + 0x24 + 8 * 2, 0x140);
            put16(f3 + 0x10 + 9 * 2, 1); put16(f3 + 0x24 + 9 * 2, 0x140 + 87);
            put16(f3 + 0xC0 + 2, 0x100); put16(f3 + 0xC0 + 4, (df_u32)-8 & 0xffffu); put16(f3 + 0xC0 + 6, 7);
            put16(f3 + 0xC0 + 4, (df_u32)-1);
            f3[0xE0 + 3] = 2; put16(f3 + 0xE0 + 4, 1); put16(f3 + 0xE0 + 6, (df_u32)-1); put16(f3 + 0xE0 + 8, 0);
            put16(f3 + 0x140 + 87, 0); put32(f3 + 0x140 + 87 + 4, 100u);
            put_op(f3 + 0x140, 30, -1, h30, 0);
            quest_set_world(&w15);
            slot = quest_start_bytes("TEST0031", f3, 0x140 + 87 + 8, 0);
            if (slot == 0) {
                quest_run();
                check_expect(quest_people_in(17, quest_at(0)->place[0].index, -2, -1, dh, 4) == 1 && dh[0].has_point && dh[0].x == 1000.0f && dh[0].z == 2000.0f &&
                             quest_people_in(17, quest_at(0)->place[0].index, 3, 7, dh, 4) == 0,
                             "quest: a person put in a dungeon place is not homed there with their quest marker's point");
            } else { check_expect(0, "quest: the dungeon person quest did not start"); }
            for (k = 0; k < quest_count(); k++) { quest_end(k); }
            quest_set_world(0);
        }
        character_state()->attribute[ATTR_STR] = saved_str;
    }
    slot = quest_start("M0B00Y00", 8);
    check_expect(!quest_at(1) && quest_at(0), "quest: a quest ended took another with it");
    quest_reset();
    /* Choosing and offering: the name's letters and rank, a Y quest asked (1000) and answered (1002 or 1001), a no ending it, a quest not started twice. */
    quest_set_ask(take_ask);
    {
        int k;
        char picked[9];
        int low_only, high_seen;

        low_only = 1; high_seen = 0;
        for (k = 0; k < 200; k++) {
            if (!quest_pick('M', 0, 'B', 0, picked) || picked[0] != 'M' || picked[1] != '0' || picked[2] != 'B' || picked[3] != '0') { low_only = 0; }
            if (quest_pick('M', 0, 'B', 9, picked) && picked[3] > '0') { high_seen = 1; }
        }
        check_expect(low_only && high_seen, "quest: Fighters quests of rank 0 are not all M0B0, or rank 9 offers no higher one");
        check_expect(quest_pick('N', 0, 'C', 3, picked) && picked[0] == 'N' && picked[2] == 'C' && !quest_pick('Z', 0, 'Q', 0, picked), "quest: a quest for a non-member was not found, or one for no group was");
    }
    g_said = 0; g_answer = 0; g_asked = -1;
    questor(3);
    slot = quest_begin("M0B00Y00", 851);
    check_expect(slot >= 0 && quest_at(slot)->qbn.faction == 851 && g_answer && quest_begin("M0B00Y00", 851) == -1, "quest: M0B00Y00 was not started with the giver's faction and asked, or was started twice");
    g_answer(1);
    check_expect(quest_at(slot) && g_asked == 1002 && g_said == 1, "quest: yes did not keep the quest and say QRC 1002");
    quest_reset();
    questor(3);
    slot = quest_begin("M0B00Y00", 851); g_asked = -1;
    g_answer(0);
    check_expect(!quest_at(slot) && g_asked == 1001, "quest: no did not say QRC 1001 and end the quest");
    /* Persons: the questor is bound to who gave the quest; they are busy until it ends, another quest is refused them, and meeting them is event 28. */
    quest_reset();
    questor(7);
    slot = quest_begin("M0B00Y00", 851);
    check_expect(slot >= 0 && quest_person_busy(7) && !quest_person_busy(8), "quest: the questgiver was not bound and busy");
    check_expect(quest_begin("M0B00Y00", 851) == -1 && !quest_talk(8, 851), "quest: a busy questor was given another quest, or a stranger's talk was taken");
    quest_end(slot);
    check_expect(!quest_person_busy(7), "quest: the questor stayed busy after the quest ended");
    quest_reset();
    check_expect(quest_begin("N0C00Y01", 0) == -1 && quest_begin("", 0) == -1, "quest: N0C00Y01 or an empty name was started");
    quest_reset();
    /* Text: symbols said by what they name (a place at form 32 is its town, a timer with = its days), %qdt and %cn, and the logbook of M0B00Y00's first entry. */
    {
        static const QuestNames names = { 0, name_place, name_person };
        char raw[1024], said[1024], log[2048];
        int s2, k;

        quest_reset();
        quest_set_world(0);
        quest_set_names(&names); quest_set_text(real_text); quest_set_say(take_say); quest_set_place(where_now);
        questor(4);
        s2 = quest_start("M0B00Y00", 851);
        check_expect(s2 == 0, "quest: M0B00Y00 did not start for the text test");
        real_text("M0B00Y00", 1002, 0, raw, sizeof(raw));
        quest_expand(s2, raw, said, sizeof(said), 0, 0);
        check_expect(strstr(said, "Dungeon of Test") != 0, "quest: ___mondung_ was not said by the place's name");
        check_expect(strstr(said, "in 0 days") != 0 && !strchr(said, '_') && !strchr(said, '='), "quest: =1stparton_ was not its days, or a symbol was left in the text");
        quest_expand(s2, "a_nosuch_b and =x", said, sizeof(said), 0, 0);
        check_expect(!strcmp(said, "ab and =x"), "quest: a symbol that names nothing was not blank, or an unfinished one was changed");
        g_said = 0;
        quest_run();
        check_expect(logbook_slot(0) && logbook_slot(0)->used && logbook_slot(0)->message[0] == 1030 && !strcmp(logbook_slot(0)->place[0], "Testown"), "quest: op 23 did not write entry 0 (1030) with the place");
        k = logbook_text(log, sizeof(log));
        check_expect(k > 0 && strstr(log, "Fighters Guild of") && strstr(log, "Wayrest") && strstr(log, "Dungeon of Test") && !strstr(log, "%qdt"), "quest: the log's entry is not 1030 with its date, the town and the dungeon");
        quest_end(s2);
        check_expect(logbook_text(log, sizeof(log)) <= 0 && !logbook_slot(0)->used, "quest: the log kept the entries of a quest that ended");
        quest_set_text(give_text);
        quest_reset();
    }
    /* Places and timers: M0B00Y00's two dungeons bound to two different dungeons of the region other than the one the player is in, its timer's delay the travel estimate and a week
     * for the kind, doubled; op 19 shows the first on the map; at the delay the timer's state ends the quest (op 6). */
    {
        static const QuestWorld world = { w_count, w_location, w_here, w_region, w_journey, w_reveal, w_spawn, w_go, w_place_item, w_find_number, 0, 0, 0, w_pick_building, w_pick_site, w_person_name, w_outdoors };
        const QuestState *q2;
        int s2, spent;

        quest_reset();
        quest_set_world(&world); quest_set_names(0); quest_set_text(real_text); quest_set_say(take_say); quest_set_ask(take_ask);
        g_revealed_count = 0;
        questor(5);
        s2 = quest_begin("M0B00Y00", 851);
        check_expect(s2 == 0, "quest: M0B00Y00 did not start against the stub map");
        q2 = quest_at(s2);
        check_expect(q2 && q2->place[0].bound && q2->place[1].bound && q2->place[0].index != 10 && q2->place[1].index != 10 && q2->place[0].index != q2->place[1].index &&
                     (q2->place[0].type == 4 || q2->place[0].type == 10) && !strcmp(q2->place[0].name, q2->place[0].index == 11 ? "Dungeon One" : q2->place[0].index == 12 ? "Dungeon Two" : "Dungeon Three"),
                     "quest: the two dungeons were not two different dungeons of the region other than the current location");
        check_expect(q2 && q2->timer[0].delay == (((100u + 2880u) * 384u >> 8) + 10080u) * 2u && !(q2->timer[0].flags & 64), "quest: the timer's delay is not the estimate, a week for the kind, doubled, or it runs before op 12");
        g_spawned = 0;
        quest_run();
        check_expect((q2->timer[0].flags & 64) && g_revealed_count == 1 && g_revealed[0] == q2->place[0].ordinal, "quest: op 12 did not start the timer or op 19 did not show the first dungeon");
        {
            char said2[1024], raw2[1024];

            real_text("M0B00Y00", 1002, 0, raw2, sizeof(raw2));
            quest_expand(s2, raw2, said2, sizeof(said2), 0, 0);
            check_expect(strstr(said2, q2->place[0].name) != 0 && strstr(said2, "in 21 days") != 0, "quest: the offer's ___mondung_ and =1stparton_ are not the dungeon's name and the days (14550 * 2 = 29100 minutes is 21 days)");
        }
        /* Foes: op 22 placed one of the three beasts in the first dungeon, not yet made; when that dungeon is loaded its creature is made with the foe's key, and its death sets the state of
         * the op that waits for it (1020 for a werebeast, 1021 for the bear). */
        {
            int f, placed, number;

            placed = -1;
            for (f = 0; f < 3; f++) { if (q2->foe[f].placed) { placed = f; } }
            check_expect(placed >= 0 && g_spawned == 0, "quest: op 22 did not place one beast, or made it before its dungeon was loaded");
            quest_location_loaded(q2->place[0].region, q2->place[0].index);
            check_expect(g_spawned == 1 && g_spawn_kind[0] == q2->qbn.foe[placed].type && g_spawn_thing[0] == QUEST_FOE_THING(q2->foe[placed].number, 0), "quest: the beast was not made with its type and key when its dungeon loaded");
            quest_location_loaded(q2->place[0].region, q2->place[0].index);
            check_expect(g_spawned == 1, "quest: the beast was made twice");
            number = q2->foe[placed].number;
            g_said = 0;
            quest_foe_hit(number);
            quest_foe_killed(number);
            check_expect(quest_state(s2, 1) == 1 && q2->qbn.foe[placed].killed == 1 && g_said >= 1, "quest: the beast's death did not set S1 and say its message");
        }
        spent = 0;
        state_pass_minutes(29100);
        quest_run();
        check_expect(quest_at(s2) != 0 && !quest_state_by_hash(s2, qbn_hash("1stparton")), "quest: the timer expired at its delay, not after it");
        state_pass_minutes(2);
        quest_run();
        check_expect(!quest_at(s2), "quest: the expired timer's state did not end the quest by op 6");
        (void)spent;
        /* Won: the beast killed, the questgiver spoken to (event 28), then op 14, op 4 (+5 with the faction, the gold paid) and op 6 end it. Never rewarded costs -2 instead. */
        quest_reset();
        questor(9);
        s2 = quest_begin("M0B00Y00", 851);
        check_expect(s2 >= 0, "quest: M0B00Y00 did not start for the win");
        if (s2 >= 0) {
            int rep, gold, amount, f;

            quest_run();
            amount = quest_at(s2)->item[0].amount;
            check_expect(quest_at(s2)->item[0].gold && amount >= 1, "quest: the reward gold was not rolled");
            rep = faction_rep(851); gold = character_state()->gold;
            for (f = 0; f < 3; f++) { quest_foe_killed(quest_at(s2)->foe[f].number); }
            quest_run();
            check_expect(quest_at(s2) != 0 && character_state()->gold == gold, "quest: the reward came before the questgiver was spoken to");
            /* Saved mid-quest and read back: the quest, its rolled gold, bound places, timer, foes, states and the logbook come back, and it can still be finished. */
            {
                static df_u8 blob[QUEST_BLOB];
                int len, s3, st1, ord0, delay;
                char log1[2048], log2[2048];

                st1 = quest_state(s2, 1); ord0 = quest_at(s2)->place[0].ordinal; delay = (int)quest_at(s2)->timer[0].delay;
                logbook_text(log1, sizeof(log1));
                len = quest_pack(blob, sizeof(blob));
                check_expect(len > 64 && len < 4096, "quest: the packed quests are not a few hundred bytes");
                quest_reset();
                check_expect(quest_unpack(blob, len) == 1, "quest: the packed quests were not read back");
                s3 = s2;
                check_expect(quest_at(s3) != 0 && !strcmp(quest_at(s3)->qbn.name, "M0B00Y00") && quest_at(s3)->qbn.faction == 851 && quest_at(s3)->item[0].amount == amount &&
                             quest_state(s3, 1) == st1 && quest_at(s3)->place[0].ordinal == ord0 && (int)quest_at(s3)->timer[0].delay == delay && quest_person_busy(9),
                             "quest: the quest read back is not the one saved");
                logbook_text(log2, sizeof(log2));
                check_expect(!strcmp(log1, log2) && log1[0], "quest: the logbook was not kept through the pack");
                check_expect(quest_unpack(blob, len - 1) == 0 && !quest_at(s3) && quest_unpack(blob, 0) == 1, "quest: damaged bytes were accepted, or no bytes left a quest running");
                quest_unpack(blob, len);
            }
            check_expect(quest_talk(9, 851), "quest: the questgiver's talk was not taken");
            quest_run();
            check_expect(!quest_at(s2) && character_state()->gold == gold + amount && faction_rep(851) > rep, "quest: the win did not pay the gold, raise the reputation and end the quest");
        }
        quest_reset();
        questor(10);
        s2 = quest_begin("M0B00Y00", 851);
        if (s2 >= 0) {
            int rep;

            quest_run();
            rep = faction_rep(851);
            state_pass_minutes(60000);
            quest_run(); quest_run();
            check_expect(!quest_at(s2) && faction_rep(851) < rep, "quest: a quest that ran out of time did not cost reputation");
        }
        quest_reset();
        quest_set_world(0);
    }
}
