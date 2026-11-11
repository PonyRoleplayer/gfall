#include <dirent.h>
#include <string.h>
#include "check_ogquest.h"
#include "check_util.h"
#include "game/ogfall/formats/qbn.h"
#include "game/ogfall/play/quest.h"
#include "game/ogfall/play/state.h"

static int g_said, g_asked;
static char g_text[200];

static int give_text(const char *quest, int message, int in_text_rsc, char *out, int cap)
{
    g_asked = message;
    (void)quest; (void)in_text_rsc; (void)cap;
    strcpy(out, "Well done.");
    return 10;
}

static void take_say(const char *text) { g_said++; strncpy(g_text, text, sizeof(g_text) - 1); }

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

static void put_op(df_u8 *p, int opcode, int message, const int *args, const int *negate)
{
    int k;

    put16(p, opcode); put16(p + 81, message);
    for (k = 0; k < 5; k++) { put_arg(p + 6 + k * 15, args[k], negate ? negate[k] : 0); }
}

void check_ogquest(void)
{
    static OgQbn q;
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
        if (!og_qbn_load(name, &q)) { bad++; check_expect(0, name); }
    }
    if (dir) { closedir(dir); }
    check_expect(files == 232 && !bad, "quest: not every one of the 232 QBN files parsed");
    /* M0B00Y00 as the document decodes it: one item, one person, two places, one timer, three foes, twelve states, twenty ops. */
    check_expect(og_qbn_load("M0B00Y00", &q) && q.n_item == 1 && q.n_person == 1 && q.n_place == 2 && q.n_timer == 1 && q.n_foe == 3 && q.n_state == 12 && q.n_op == 20,
                 "quest: M0B00Y00 is not 1 item, 1 person, 2 places, 1 timer, 3 foes, 12 states and 20 ops");
    check_expect(q.item[0].flags == 2 && q.item[0].symbol == og_qbn_hash("reward") && q.person[0].kind == 21 && q.person[0].symbol == og_qbn_hash("questgiver") &&
                 q.place[0].scope == 2 && q.place[0].symbol == og_qbn_hash("mondung") && q.timer[0].flags == 0x11 && q.timer[0].type == 2 && q.timer[0].hash == og_qbn_hash("1stparton") &&
                 q.foe[0].type == 9 && q.foe[1].type == 14 && q.foe[2].type == 4,
                 "quest: M0B00Y00's resources are not the ones the document names");
    check_expect(q.op[0].opcode == 12 && q.op[1].opcode == 23 && q.op[1].arg[1].value == 1030 && q.op[2].opcode == 34 && q.op[18].opcode == 6 && q.op[19].opcode == 6 &&
                 og_qbn_hash("reward") == 6932 && og_qbn_hash("questgiver") == 115084 && q.state[3].hash == og_qbn_hash("1stparton"),
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
    check_expect(og_qbn_parse(file, 0x80 + 4 * 87 + 3 * 8, &q) && q.n_op == 4 && q.n_state == 3 && q.op[1].argc == 5 && q.op[1].arg[2].negate == 1, "quest: the quest made in memory did not parse");
    /* Run it as a started quest: the file path needs a file, so the slot is made from the parsed bytes by writing it where og_qbn_load reads (a test file beside ARENA2). */
    og_quest_set_text(give_text); og_quest_set_say(take_say);
    og_quest_reset();
    slot = og_quest_start("M0B00Y00", 8);
    check_expect(slot == 0 && og_quest(0)->qbn.id == 1 && og_quest(0)->qbn.faction == 8 && og_quest_start("M0B00Y00", 8) == 1 && og_quest(1)->qbn.id == 2, "quest: started quests did not get ids 1 and 2 and the faction");
    g_said = 0;
    og_quest_run();
    {
        int set6, set7, set8;

        /* Op 2 picks one of S9, S10 (three times), once. */
        set6 = og_quest_state(0, 9) + og_quest_state(0, 10);
        check_expect(set6 == 1, "quest: op 34 did not set exactly one of its given states");
        set7 = og_quest_state(0, 9); set8 = og_quest_state(0, 10);
        og_quest_run(); og_quest_run();
        check_expect(og_quest_state(0, 9) == set7 && og_quest_state(0, 10) == set8, "quest: op 34 ran again after its once");
    }
    /* An op 52 with an unused operand is not held false by it; op 51 says its message once; op 6 ends the quest. */
    og_quest_end(1);
    og_quest_end(0);
    g_said = 0; g_asked = -1;
    slot = og_quest_start_bytes("TEST0001", file, 0x80 + 4 * 87 + 3 * 8, 0);
    check_expect(slot == 0, "quest: the quest made in memory did not start");
    og_quest_run();
    check_expect(g_said == 1 && g_asked == 1030 && !strcmp(g_text, "Well done.") && !og_quest(0), "quest: S1 = S0 and not S2 did not say its message once and end the quest");
    slot = og_quest_start("M0B00Y00", 8);
    check_expect(!og_quest(1) && og_quest(0), "quest: a quest ended took another with it");
    og_quest_reset();
    /* Choosing and offering: the name's letters and rank, a Y quest asked (1000) and answered (1002 or 1001), a no ending it, a quest not started twice. */
    og_quest_set_ask(take_ask);
    {
        int k;
        char picked[9];
        int low_only, high_seen;

        low_only = 1; high_seen = 0;
        for (k = 0; k < 200; k++) {
            if (!og_quest_pick('M', 0, 'B', 0, picked) || picked[0] != 'M' || picked[1] != '0' || picked[2] != 'B' || picked[3] != '0') { low_only = 0; }
            if (og_quest_pick('M', 0, 'B', 9, picked) && picked[3] > '0') { high_seen = 1; }
        }
        check_expect(low_only && high_seen, "quest: Fighters quests of rank 0 are not all M0B0, or rank 9 offers no higher one");
        check_expect(og_quest_pick('N', 0, 'C', 3, picked) && picked[0] == 'N' && picked[2] == 'C' && !og_quest_pick('Z', 0, 'Q', 0, picked), "quest: a quest for a non-member was not found, or one for no group was");
    }
    g_said = 0; g_answer = 0; g_asked = -1;
    slot = og_quest_begin("M0B00Y00", 851);
    check_expect(slot >= 0 && og_quest(slot)->qbn.faction == 851 && g_answer && og_quest_begin("M0B00Y00", 851) == -1, "quest: M0B00Y00 was not started with the giver's faction and asked, or was started twice");
    g_answer(1);
    check_expect(og_quest(slot) && g_asked == 1002 && g_said == 1, "quest: yes did not keep the quest and say QRC 1002");
    og_quest_reset();
    slot = og_quest_begin("M0B00Y00", 851); g_asked = -1;
    g_answer(0);
    check_expect(!og_quest(slot) && g_asked == 1001, "quest: no did not say QRC 1001 and end the quest");
    check_expect(og_quest_begin("N0C00Y01", 0) == -1 && og_quest_begin("", 0) == -1, "quest: N0C00Y01 or an empty name was started");
    og_quest_reset();
}
