#include "game/dagger/tables/tables.h"

#include "engine/core/df.h"
#include "engine/core/mem.h"

/*
 * Race order is the province pixel minus one (title_menu stores pick - 1).
 * High Rock's pixel on TAMRIEL2.IMG is 1, so Breton is 0. The message
 * byte is FALL.EXE's D_0017B4B8, and the record is 2000 + that byte.
 */
static const char *const g_race[] = {
    "Breton", "Redguard", "Nord", "Dark Elf",
    "High Elf", "Wood Elf", "Khajiit", "Argonian"
};
static const int g_race_msg[] = { 3, 2, 0, 7, 6, 5, 1, 4 };

static OgClass g_class[OG_CLASSES];
static int g_class_set[OG_CLASSES];

void ogfall_tables_startup(void)
{
}

void ogfall_tables_shutdown(void)
{
}

void og_class_clear(void)
{
    mem_zero(g_class, sizeof(g_class));
    mem_zero(g_class_set, sizeof(g_class_set));
}

void og_class_set(int index, const OgClass *row)
{
    if (index < 0 || index >= OG_CLASSES || !row) {
        return;
    }
    g_class[index] = *row;
    g_class_set[index] = 1;
}

const OgClass *og_class(int index)
{
    if (index < 0 || index >= OG_CLASSES || !g_class_set[index]) {
        return 0;
    }
    return &g_class[index];
}

const char *og_race_name(int race)
{
    if (race < 0 || race >= (int)(sizeof(g_race) / sizeof(g_race[0]))) {
        return "";
    }
    return g_race[race];
}

int og_race_text(int race)
{
    if (race < 0 || race >= (int)(sizeof(g_race_msg) / sizeof(g_race_msg[0]))) {
        return 0;
    }
    return 2000 + g_race_msg[race];
}

/* Skill order and the fixed-point spell table in FALL.EXE, D_001788D3.
 * init.c character_reset_magicka uses integer multiplication then /256. */
const char *og_skill_name(int skill)
{
    static const char *const names[OG_SKILLS] = {
        "Medical", "Etiquette", "Streetwise", "Jumping", "Orcish", "Harpy",
        "Giantish", "Dragonish", "Nymph", "Daedric", "Spriggan", "Centaurian",
        "Impish", "Lockpicking", "Mercantile", "Pickpocket", "Stealth",
        "Swimming", "Climbing", "Backstabbing", "Dodging", "Running",
        "Destruction", "Restoration", "Illusion", "Alteration", "Thaumaturgy",
        "Mysticism", "Short Blade", "Long Blade", "Hand-to-Hand", "Axe",
        "Blunt Weapon", "Archery", "Critical Strike"
    };
    return skill >= 0 && skill < OG_SKILLS ? names[skill] : "";
}

int og_class_skill(int klass, int slot)
{
    const OgClass *row;

    row = og_class(klass);
    if (!row || slot < 0 || slot >= 12) {
        return -1;
    }
    if (slot < 3) {
        return row->primary[slot];
    }
    if (slot < 6) {
        return row->major[slot - 3];
    }
    return row->minor[slot - 6];
}

int og_magicka(int intelligence, int flags)
{
    static const int multiplier[] = { 768, 512, 448, 384, 256, 128, 256, 0 };
    return intelligence * multiplier[(flags >> 10) & 7] / 256;
}

static const char *g_questions[40];
static unsigned char g_class_answers[66 * 3];

void og_question_set(int number, const char *text)
{
    if (number >= 1 && number <= 40) { g_questions[number - 1] = text; }
}

const char *og_question_text(int number)
{
    return number >= 1 && number <= 40 && g_questions[number - 1] ? g_questions[number - 1] : "";
}

/* question.c/class_questions_run: the biased table base is D_0018520B;
 * answer indices 1..3 make the first real byte 0x18520C. */
int og_question_kind(int number, int answer)
{
    static const unsigned char kinds[40][3] = {
        { 0, 2, 1 },
        { 0, 2, 1 },
        { 0, 1, 2 },
        { 2, 0, 1 },
        { 0, 1, 2 },
        { 1, 0, 2 },
        { 0, 1, 2 },
        { 2, 0, 1 },
        { 0, 2, 1 },
        { 0, 1, 2 },
        { 0, 1, 2 },
        { 0, 1, 2 },
        { 0, 2, 1 },
        { 0, 1, 2 },
        { 1, 0, 2 },
        { 1, 2, 0 },
        { 2, 0, 1 },
        { 1, 0, 2 },
        { 0, 1, 2 },
        { 2, 0, 1 },
        { 1, 0, 2 },
        { 0, 1, 2 },
        { 0, 2, 1 },
        { 2, 1, 0 },
        { 1, 0, 2 },
        { 0, 2, 1 },
        { 2, 1, 0 },
        { 2, 0, 1 },
        { 2, 1, 0 },
        { 2, 1, 0 },
        { 2, 0, 1 },
        { 1, 0, 2 },
        { 0, 2, 1 },
        { 2, 1, 0 },
        { 1, 2, 0 },
        { 2, 0, 1 },
        { 2, 0, 1 },
        { 1, 2, 0 },
        { 0, 1, 2 },
        { 1, 2, 0 },
    };
    if (number < 1 || number > 40 || answer < 0 || answer > 2) { return 0; }
    return kinds[number - 1][answer];
}

void og_class_answers_set(const unsigned char *triples)
{
    if (triples) { mem_copy(g_class_answers, triples, sizeof(g_class_answers)); }
}

int og_class_from_answers(const int *counts)
{
    static const int ids[] = { 17, 16, 15, 14, 10, 7, 11, 9, 0, 3, 5, 4, 13, 12, 6, 8, 2, 1 };
    int i;
    int index;

    for (i = 0; i < 66; i++) {
        if (counts[0] == g_class_answers[i * 3] && counts[1] == g_class_answers[i * 3 + 1]
            && counts[2] == g_class_answers[i * 3 + 2]) {
            index = i < 48 ? i / 4 : 12 + (i - 48) / 4;
            return ids[index];
        }
    }
    return -1;
}

/* custom.c's name/sublist and 16.16 cost tables, FALL.EXE 1.07.213:
 * D_0018084E/D_0018092E, D_00185370/D_001853A0, D_0018549C/D_001854C8. */
static const OgSpecial g_special0[11] = {
    { "Resistance", 7, { "to Paralysis", "to Magic", "to Poison", "to Fire", "to Frost", "to Shock", "to Disease" }, { 16384, 16384, 16384, 16384, 16384, 16384, 16384 } },
    { "Immunity", 7, { "to Paralysis", "to Magic", "to Poison", "to Fire", "to Frost", "to Shock", "to Disease" }, { 32768, 32768, 32768, 32768, 32768, 32768, 32768 } },
    { "Acute Hearing", 1, { "" }, { 3277 } },
    { "Spell Absorption", 3, { "in Light", "in Darkness", "General" }, { 26214, 39322, 45875 } },
    { "Rapid Healing", 3, { "in Light", "in Darkness", "General" }, { 3277, 9830, 13108 } },
    { "Regenerate health", 4, { "in Lights", "in Darkness", "while Immersed in Water", "General" }, { 19661, 32768, 6554, 45875 } },
    { "Bonus to hit", 4, { "Undead", "Daedra", "Humanoid", "Animals" }, { 19661, 9830, 19661, 19661 } },
    { "Atheleticism", 1, { "" }, { 13108 } },
    { "Increased Magery", 5, { "3x Int in Spell Points", "2x Int in Spell Points", "1.75x Int in Spell Points", "1.5x Int in Spell Points", "Int in Spell Points" }, { 32768, 26214, 19661, 13108, 6554 } },
    { "Adrenaline Rush", 1, { "" }, { 13108 } },
    { "Expertise in", 6, { "Short Blade", "Long Blade", "Hand-to-Hand", "Axe", "Blunt Weapon", "Missile Weapon" }, { 6554, 6554, 6554, 6554, 6554, 6554 } },
};
static const OgSpecial g_special1[11] = {
    { "Inability to Regen Spell Points", 1, { "" }, { 45875 } },
    { "Damage", 2, { "from Sunlight", "from Holy Places" }, { 32768, 19661 } },
    { "Phobia", 4, { "Undead", "Daedra", "Humanoid", "Animals" }, { 13108, 13108, 13108, 13108 } },
    { "Light-powered Magery", 2, { "Unable to use magic in darkness", "Lower magic ability in darkness" }, { 45875, 32768 } },
    { "Darkness-powered Magery", 2, { "Unable to use magic in daylight", "Lower magic ability in daylight" }, { 32768, 22938 } },
    { "Forbidden Weaponry", 6, { "Short Blade", "Long Blade", "Hand-to-Hand", "Axe", "Blunt Weapon", "Missile Weapon" }, { 6554, 6554, 6554, 6554, 6554, 6554 } },
    { "Low Tolerance", 7, { "to Paralysis", "to Magic", "to Poison", "to Fire", "to Frost", "to Shock", "to Disease" }, { 16384, 16384, 16384, 16384, 16384, 16384, 16384 } },
    { "Critical Weakness", 7, { "to Paralysis", "to Magic", "to Poison", "to Fire", "to Frost", "to Shock", "to Disease" }, { 45875, 45875, 45875, 45875, 45875, 45875, 45875 } },
    { "Forbidden Armor Type", 3, { "Leather", "Chain", "Plate" }, { 3277, 6554, 16384 } },
    { "Forbidden Shield Types", 4, { "Buckler", "Round Shield", "Kite Shield", "Tower Shield" }, { 3277, 3277, 3277, 3277 } },
    { "Forbidden Material", 10, { "Iron", "Steel", "Silver", "Elven", "Dwarven", "Mithril", "Adamantium", "Ebony", "Orcish", "Daedric" }, { 3277, 32768, 19661, 29491, 22938, 19661, 16384, 13108, 9830, 6554 } },
};

const OgSpecial *og_special(int list, int kind)
{
    if (kind < 0 || kind >= 11) { return 0; }
    return list ? &g_special1[kind] : &g_special0[kind];
}

/* career_nearest_class: primary/major/minor weights, first class wins ties. */
int og_nearest_class(int klass)
{
    int i, j, k, w, v, score, best, result, d;
    best = -1;
    result = 0;
    for (j = 0; j < 18; j++) {
        score = 0;
        for (i = 0; i < 12; i++) {
            w = i < 3 ? 3 : (i < 6 ? 2 : 1);
            for (k = 0; k < 12; k++) {
                if (og_class_skill(klass, i) != og_class_skill(j, k)) { continue; }
                v = k < 3 ? 3 : (k < 6 ? 2 : 1);
                d = v > w ? v - w : w - v;
                score += v == w ? w : 3 - d;
                break;
            }
        }
        if (score > best) { best = score; result = j; }
    }
    return result;
}

int og_bob_sample(int phase_ms)
{
    static const int sample[10] = { 0, 2, 4, 6, 8, 6, 4, 2, 0, 0 };
    if (phase_ms < 0 || phase_ms >= 1000) { return 0; }
    return sample[phase_ms / 100];
}
