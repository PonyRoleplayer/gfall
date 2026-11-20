#include "game/dagger/play/people/politics.h"
#include "game/dagger/play/people/names.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/ui/words.h"
#include "game/dagger/play/state.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/tables/politicsdata.h"
#include "engine/core/mem.h"

/* See politics.h. */

#define ST (standing_state())
#define PS (politics_state())
/* Rumours are kept only in a round that writes them (the start-up rounds 1..11 do not). */
static int g_writing;
static df_u32 g_now;
/* The most tries at a text with a macro a rumour cannot say (a variant naming a person), and at a random faction of a kind. */
#define SAY_TRIES 8
#define RANDOM_FACTION_TRIES 4096
/* A faction's type (FACTION.TXT): 2 a group, 3 a subgroup, 7 a noble, 8 a coven, 9 a temple. */
#define TYPE_GROUP 2
#define TYPE_SUBGROUP 3
#define TYPE_NOBLE 7
#define TYPE_COVEN 8
#define TYPE_TEMPLE 9
#define TYPE_INDIVIDUAL 4
/* A faction with this flag keeps its ruler (func_00019676.c: flags & 16). */
#define FACTION_KEEPS_RULER 16
#define POWER_MOST 100
#define POWER_FLOOR 1
/* region_update_prices' region count and a price roll's top. */
#define ROLL_TOP 100

static int total(void) { return faction_total(); }
static const FactionDef *def(int i) { return i >= 0 ? faction_def(i) : 0; }
static int region_of(int i) { const FactionDef *d = def(i); return d ? d->region : -1; }
static int is_noble(int i) { const FactionDef *d = def(i); return d && d->type == TYPE_NOBLE && d->region >= 0; }
static int is_party(int i) { const FactionDef *d = def(i); return d && (d->type == TYPE_GROUP || d->type == TYPE_SUBGROUP || d->type == TYPE_NOBLE); }
static int rolled(int low, int high) { return random_range(low, high); }

static int power(int i) { return i >= 0 ? ST->power[i] : 0; }
/* faction_add_power: at most 100, and a result under 0 becomes 1. */
static void add_power(int i, int amount)
{
    if (i < 0) { return; }
    ST->power[i] = (short)(ST->power[i] + amount);
    if (ST->power[i] > POWER_MOST) { ST->power[i] = POWER_MOST; }
    else if (ST->power[i] < 0) { ST->power[i] = POWER_FLOOR; }
}
/* The direct losses of a round (power-- and the battle's): the original lets them go under 0; here they stop at the floor. */
static void lose_power(int i, int amount)
{
    if (i < 0) { return; }
    ST->power[i] = (short)(ST->power[i] - amount);
    if (ST->power[i] < POWER_FLOOR) { ST->power[i] = POWER_FLOOR; }
}

static int has_ally(int f, int other)
{
    int j;

    if (f < 0 || other < 0) { return 0; }
    for (j = 0; j < FACTION_LINKS; j++) { if (ST->ally[f][j] == other) { return 1; } }
    return 0;
}
static int has_enemy(int f, int other)
{
    int j;

    if (f < 0 || other < 0) { return 0; }
    for (j = 0; j < FACTION_LINKS; j++) { if (ST->enemy[f][j] == other) { return 1; } }
    return 0;
}
static int any_has_enemy(int f, int other) { int j, n = 0; for (j = 0; j < FACTION_LINKS; j++) { n |= has_enemy(ST->ally[f][j], other); } return n; }

/* faction_shared_relations: 3 for each pair of an ally of the one that is an ally of an ally of the other, and each pair of enemies likewise. (The original returned a scratch word, not this.) */
static int shared(int f, int o)
{
    int i, j, n = 0;

    for (i = 0; i < FACTION_LINKS; i++) { for (j = 0; j < FACTION_LINKS; j++) { if (has_ally(ST->ally[f][i], ST->ally[o][j])) { n += 3; } } }
    for (i = 0; i < FACTION_LINKS; i++) { for (j = 0; j < FACTION_LINKS; j++) { if (has_enemy(ST->enemy[f][i], ST->enemy[o][j])) { n += 3; } } }
    return n;
}

static int parent_of(int i) { const FactionDef *d = def(i); return d && d->parent ? faction_index(d->parent) : -1; }
static int in_subtree(int root, int i)
{
    int guard;

    for (guard = 0; i >= 0 && guard < total(); guard++) { if (i == root) { return 1; } i = parent_of(i); }
    return 0;
}

/* faction_tree_relation: 3 when o is under f, 2 when they share a parent, 1 when o is over f, else 0. (The original's 3 could never be returned: it searched o's own subtree for o.) */
static int tree_relation(int f, int o)
{
    if (o != f && in_subtree(f, o)) { return 3; }
    if (parent_of(f) >= 0 && parent_of(o) == parent_of(f)) { return 2; }
    if (f != o && in_subtree(o, f)) { return 1; }
    return 0;
}

/* faction_max_child_power: the highest power among everything under f. */
static int max_child_power(int f)
{
    int i, best = 0;

    for (i = 0; i < total(); i++) { if (i != f && in_subtree(f, i) && power(i) > best) { best = power(i); } }
    return best;
}

int regions_border(int a, int b)
{
    int i, v;

    if (a < 0 || a >= REGIONS || b < 0 || b >= REGIONS) { return 0; }
    for (i = 0; i < REGION_NEIGHBOURS; i++) {
        v = g_region_neighbours[a][i];
        if (v == REGION_NEIGHBOUR_END) { return 0; }
        if (v == b) {
            if (b != 0) { return 1; }
            /* A 0 is padding unless region 0 lists a back. */
            for (i = 0; i < REGION_NEIGHBOURS && g_region_neighbours[0][i] != REGION_NEIGHBOUR_END; i++) { if (g_region_neighbours[0][i] == a) { return 1; } }
            return 0;
        }
    }
    return 0;
}
static int borders(int f, int o) { return region_of(f) >= 0 && region_of(o) >= 0 && regions_border(region_of(f), region_of(o)); }

/* ---- Region flags ---- */

void region_flag_clear(int region, int flag)
{
    if (region < 0 || region >= REGIONS || flag < 0 || flag >= POLITICS_FLAGS) { return; }
    PS->flags[region][flag] = 0;
    PS->groups[region][g_flag_group[flag]] = 0;
}

void region_flag_set(int region, int flag)
{
    int i;

    if (region < 0 || region >= REGIONS || flag < 0 || flag >= POLITICS_FLAGS) { return; }
    if (PS->groups[region][g_flag_group[flag]]) {
        for (i = 0; i < POLITICS_FLAGS; i++) { if (g_flag_group[i] == g_flag_group[flag]) { PS->flags[region][i] = 0; } }
    }
    PS->flags[region][flag] = 1;
    PS->groups[region][g_flag_group[flag]] = 1;
    (void)rolled(g_flag_duration[flag][0], g_flag_duration[flag][1]);
}

int region_flag(int region, int flag) { return region >= 0 && region < REGIONS && flag >= 0 && flag < POLITICS_FLAGS && PS->flags[region][flag]; }

/* ---- Rumours ---- */

/* faction_player_related: the faction is one of the player's memberships, or an ally or enemy of it. */
static int player_related(int f)
{
    int k;

    if (f < 0) { return 0; }
    for (k = 0; k < MEMBERSHIPS; k++) {
        int m;

        if (!guilds_state()->m[k].used) { continue; }
        m = faction_index(guilds_state()->m[k].faction);
        if (m == f || has_ally(m, f) || has_enemy(m, f)) { return 1; }
    }
    return 0;
}

static const char *name_of(int i) { const FactionDef *d = def(i); return d ? d->name : ""; }

/* %fl1 (the ruler named from a 16-bit seed): a noble whose first child is an individual (type 4) is that person; else a name of one of the eight banks and a sex taken from the seed (the original
 * seeds its generator with the word and draws them: the draw is ours). seed 0 is the faction's own word's low half, 1 its high half (the old ruler). */
static void ruler_name(int f, int old, char *out, int cap)
{
    int r = region_of(f), i;
    df_u32 s;

    out[0] = 0;
    if (f < 0 || r < 0) { return; }
    for (i = 0; !old && i < total(); i++) {
        const FactionDef *d = def(i);
        if (d->parent == def(f)->id && d->type == TYPE_INDIVIDUAL) { df_copy(out, d->name, cap); return; }
    }
    s = old ? PS->seed[f] >> 16 : PS->seed[f] & 0xFFFFu;
    name_seeded((int)(s & 7u), (int)((s >> 3) & 1u), s, out, cap);
}

/* ruler_titles (FALL.EXE): from a number 1..12, King and Queen, Duke and Duchess, Marquis and Marquise, Count and Countess, Baron and Baroness, Lord and Lady: odd is a man, even a woman.
 * Our factions carry no ruler number, so a region's is its own (region % 12 + 1). */
#define RULER_TITLES 12
#define RULER_FATES 6

static void rumour_add(int f1, int f2, int kind, int region, int text)
{
    NewsRumour *r;
    Words w;
    char said[RUMOUR_TEXT], ruler[40], old_ruler[40], temple[FACTION_NAME];
    const Faction *province;
    int i, tries, slot, noble, said_ok;

    if (!g_writing) { return; }
    if (kind == NEWS_GOSSIP && !player_related(f1) && !player_related(f2)) { return; }
    if (region < 0 && f1 >= 0) { region = region_of(f1); }
    if (region < 0 && f2 >= 0) { region = region_of(f2); }
    if (region < 0) { region = rolled(0, REGIONS - 1); }
    mem_zero(&w, sizeof(w));
    w.faction1 = name_of(f1); w.faction2 = name_of(f2);
    province = faction_province_of(region);
    w.region = province ? province->name : "";
    noble = f1 >= 0 && is_noble(f1) ? f1 : -1;
    if (noble < 0) { for (i = 0; i < total(); i++) { if (is_noble(i) && region_of(i) == region) { noble = i; break; } } }
    ruler_name(noble, 0, ruler, sizeof(ruler)); w.ruler = ruler;
    {
        static const char *const titles[RULER_TITLES] = { "King", "Queen", "Duke", "Duchess", "Marquis", "Marquise", "Count", "Countess", "Baron", "Baroness", "Lord", "Lady" };
        static const char *const fates[RULER_FATES] = { "died of heart failure", "struck his head", "died of the plague", "stepped down", "was overthrown", "was murdered in his bed" };
        int number = region % RULER_TITLES;

        ruler_name(noble, 1, old_ruler, sizeof(old_ruler)); w.ruler_old = old_ruler;
        w.title = titles[number]; w.pronoun = number & 1 ? "her" : "his";
        w.fate = fates[(PS->seed[noble >= 0 ? noble : 0] >> 16) % RULER_FATES];
    }
    temple[0] = 0;
    if (PS->persecuted[region] > 0) { const FactionDef *t = def(faction_index(PS->persecuted[region])); if (t) { for (i = 0; i < FACTION_NAME - 1 && t->name[i]; i++) { temple[i] = t->name[i]; } temple[i] = 0; } }
    w.temple = temple;
    words_use(&w);
    said_ok = 0;
    for (tries = 0; tries < SAY_TRIES && !said_ok; tries++) { said_ok = words_say_plain(text, said, (int)sizeof(said)) > 0; }


    /* The expansion's macros live on this frame: nothing may keep pointing at them. */
    words_use(0);
    if (!said_ok) { return; }
    slot = -1;
    for (i = 0; i < RUMOURS_MOST; i++) {
        if (!PS->rumour[i].used) { slot = i; break; }
        if (slot < 0 || PS->rumour[i].expires < PS->rumour[slot].expires) { slot = i; }
    }
    r = &PS->rumour[slot];
    mem_zero(r, sizeof(*r));
    r->used = 1; r->faction1 = (short)(f1 >= 0 ? def(f1)->id : 0); r->faction2 = (short)(f2 >= 0 ? def(f2)->id : 0); r->kind = (short)kind; r->region = (unsigned char)region;
    r->flags = (unsigned char)(kind == NEWS_PLAGUE || kind == NEWS_FAMINE || kind == NEWS_WITCHES || kind == NEWS_TEMPLE || kind == NEWS_ALLIANCE || kind == NEWS_ENEMIES || kind == NEWS_WAR_STATE ? NEWSFLAG_REGIONAL : NEWSFLAG_FACTION);
    r->expires = g_now + RUMOUR_LIFE;
    for (i = 0; said[i] && i < RUMOUR_TEXT - 1; i++) { r->text[i] = said[i]; }
}

/* rumor_file_purge: a faction's rumours stay while they last, every regional one goes. */
static void purge(void)
{
    int i;

    for (i = 0; i < RUMOURS_MOST; i++) {
        NewsRumour *r = &PS->rumour[i];

        if (r->used && ((r->flags & NEWSFLAG_REGIONAL) || !(r->flags & NEWSFLAG_FACTION) || r->expires <= g_now)) { r->used = 0; }
    }
}

int politics_rumours(void) { int i, n = 0; for (i = 0; i < RUMOURS_MOST; i++) { n += PS->rumour[i].used; } return n; }

int politics_news(char *out, int cap)
{
    int i, n, pick, ok[RUMOURS_MOST];

    n = 0;
    for (i = 0; i < RUMOURS_MOST; i++) {
        const NewsRumour *r = &PS->rumour[i];
        int roll = rolled(1, ROLL_TOP);

        ok[i] = 0;
        if (!r->used || !(r->flags & NEWSFLAG_FACTION) || r->expires <= game_state()->minutes) { continue; }
        /* A faction rumour whose faction asks for hush passes only on a high roll (rumor_is_eligible). */
        if (r->faction1 && (faction_def(faction_index(r->faction1))->flags & NEWSFLAG_HUSH) && roll <= RUMOUR_HUSH_ROLL) { continue; }
        ok[i] = 1; n++;
    }
    if (!n) { return 0; }
    pick = rolled(0, n - 1);
    for (i = 0; i < RUMOURS_MOST; i++) {
        if (ok[i] && pick-- == 0) { df_copy(out, PS->rumour[i].text, cap); return 1; }
    }
    return 0;
}

int politics_board(int region, char *out, int cap)
{
    int i, used;

    used = 0; out[0] = 0;
    for (i = 0; i < RUMOURS_MOST; i++) {
        const NewsRumour *r = &PS->rumour[i];
        int k;

        if (!r->used || !(r->flags & NEWSFLAG_REGIONAL) || r->region != region) { continue; }
        for (k = 0; r->text[k] && used < cap - 2; k++) { out[used++] = r->text[k]; }
        out[used++] = '\n';
    }
    out[used] = 0;
    return used;
}

/* ---- The round ---- */

static int random_party(void)
{
    int tries, i;

    for (tries = 0; tries < RANDOM_FACTION_TRIES; tries++) { i = rolled(0, total() - 1); if (is_party(i)) { return i; } }
    return -1;
}

static void make_alliance(int f, int slot, int o)
{
    int k;

    ST->ally[f][slot] = (short)o;
    for (k = 0; k < FACTION_LINKS; k++) { if (ST->ally[o][k] < 0) { ST->ally[o][k] = (short)f; return; } }
}
static void make_enemies(int f, int slot, int o)
{
    int k;

    ST->enemy[f][slot] = (short)o;
    for (k = 0; k < FACTION_LINKS; k++) { if (ST->enemy[o][k] < 0) { ST->enemy[o][k] = (short)f; return; } }
}
static void drop_link(short *list, int other) { int k; for (k = 0; k < FACTION_LINKS; k++) { if (list[k] == other) { list[k] = -1; } } }
static void break_alliance(int f, int slot) { int o = ST->ally[f][slot]; ST->ally[f][slot] = -1; if (o >= 0) { drop_link(ST->ally[o], f); } }
static void make_peace(int f, int slot) { int o = ST->enemy[f][slot]; ST->enemy[f][slot] = -1; if (o >= 0) { drop_link(ST->enemy[o], f); } }

/* region_reset_war: a noble's region has no war flag and no group. */
static void reset_war(int f)
{
    int r, k;

    if (!is_noble(f)) { return; }
    r = region_of(f);
    PS->groups[r][GROUP_WAR] = 0;
    for (k = REGION_WAR_BEGINNING; k <= REGION_WAR_LOST; k++) { PS->flags[r][k] = 0; }
}

static int can_war(int f, int o) { return o >= 0 && is_noble(f) && is_noble(o) && has_enemy(f, o) && borders(f, o); }

static int sum_allies(int f) { int j, n = 0; for (j = 0; j < FACTION_LINKS; j++) { n += power(ST->ally[f][j]); } return n; }

/* The war between two bordering nobles who are each other's enemies (the original read the region's flags of any faction: only a noble has a region). */
static void war(int f, int e)
{
    int rf = region_of(f), re = region_of(e), strength, enemy_strength;

    if (PS->flags[rf][REGION_WAR_WON] || PS->flags[rf][REGION_WAR_LOST]) {
        reset_war(f); reset_war(e);
        drop_link(ST->enemy[f], e); drop_link(ST->enemy[e], f);
    } else if (PS->flags[rf][REGION_WAR_BEGINNING]) {
        rumour_add(f, e, NEWS_WAR, rf, TEXT_NEWS_WAR);
        rumour_add(e, f, NEWS_WAR, re, TEXT_NEWS_WAR);
        region_flag_set(rf, REGION_WAR_ONGOING); region_flag_set(re, REGION_WAR_ONGOING);
    } else if (PS->flags[rf][REGION_WAR_ONGOING]) {
        if (rolled(1, ROLL_TOP) <= 5) {
            region_flag_clear(rf, REGION_WAR_ONGOING); region_flag_clear(re, REGION_WAR_ONGOING);
            return;
        }
        strength = sum_allies(f) / 5 + power(f);
        enemy_strength = sum_allies(e) / 5 + power(e);
        /* A battle costs each side 1 to a tenth of its strength (the original could draw from 1 to 0). */
        lose_power(f, rolled(1, strength / 10 > 1 ? strength / 10 : 1));
        lose_power(e, rolled(1, enemy_strength / 10 > 1 ? enemy_strength / 10 : 1));
        if (strength < enemy_strength) {
            if (enemy_strength - strength > strength) {
                rumour_add(e, f, NEWS_GOSSIP, 0, TEXT_NEWS_WAR_WON);
                add_power(e, power(f) / 2);
                region_flag_set(re, REGION_WAR_WON); region_flag_set(rf, REGION_WAR_LOST);
            } else { rumour_add(f, e, NEWS_GOSSIP, 0, TEXT_NEWS_WAR_GOSSIP); }
        } else if (strength - enemy_strength > enemy_strength) {
            rumour_add(f, e, NEWS_GOSSIP, 0, TEXT_NEWS_WAR_WON);
            add_power(f, power(e) / 2);
            region_flag_set(re, REGION_WAR_LOST); region_flag_set(rf, REGION_WAR_WON);
        } else { rumour_add(f, e, NEWS_GOSSIP, 0, TEXT_NEWS_WAR_GOSSIP); }
    }
}

/* One regional event of a noble's region: its famine and plague (flags 7..9 and 4..6), persecution, crime wave and witch burnings. */
static void events(int f)
{
    int r = region_of(f), allies, other, temple, j, k;
    const FactionDef *d;

    allies = sum_allies(f) / 10;
    if (PS->flags[r][REGION_FAMINE_ENDING]) { region_flag_clear(r, REGION_FAMINE_ENDING); }
    else if (PS->flags[r][REGION_FAMINE_ONGOING]) {
        if (rolled(0, ROLL_TOP) < allies + PS->factor[f] / 5 + power(f) / 5) { rumour_add(f, -1, NEWS_FAMINE, r, TEXT_NEWS_FAMINE); region_flag_set(r, REGION_FAMINE_ENDING); }
    } else if (PS->flags[r][REGION_FAMINE_BEGINNING]) { rumour_add(f, -1, NEWS_FAMINE, r, TEXT_NEWS_FAMINE); region_flag_set(r, REGION_FAMINE_ONGOING); }
    else if (rolled(1, ROLL_TOP) <= 2 && rolled(0, ROLL_TOP) > PS->factor[f] + allies) { rumour_add(f, -1, NEWS_FAMINE, r, TEXT_NEWS_FAMINE); region_flag_set(r, REGION_FAMINE_BEGINNING); }
    temple = g_temple_by_region[r] ? faction_index(g_temple_by_region[r]) : -1;
    d = def(temple);
    if (d && d->type != TYPE_TEMPLE) { temple = -1; }
    if (PS->flags[r][REGION_PLAGUE_ENDING]) { region_flag_clear(r, REGION_PLAGUE_ENDING); }
    else if (PS->flags[r][REGION_PLAGUE_ONGOING]) {
        lose_power(temple, 1); lose_power(f, 1);
        if (rolled(0, ROLL_TOP) < allies + PS->factor[f] / 5 + power(f) / 5) { rumour_add(f, -1, NEWS_PLAGUE, r, TEXT_NEWS_PLAGUE); region_flag_set(r, REGION_PLAGUE_ENDING); }
    } else if (PS->flags[r][REGION_PLAGUE_BEGINNING]) { lose_power(temple, 1); lose_power(f, 1); rumour_add(f, -1, NEWS_PLAGUE, r, TEXT_NEWS_PLAGUE); region_flag_set(r, REGION_PLAGUE_ONGOING); }
    else if (rolled(1, ROLL_TOP) <= 2 && rolled(0, ROLL_TOP) > PS->factor[f] + allies) { lose_power(temple, 1); lose_power(f, 1); rumour_add(f, -1, NEWS_PLAGUE, r, TEXT_NEWS_PLAGUE); region_flag_set(r, REGION_PLAGUE_BEGINNING); }
    if (temple >= 0) {
        if (PS->flags[r][REGION_TEMPLE_PERSECUTED]) { lose_power(temple, 1); }
        if (rolled(0, ROLL_TOP) < (power(temple) - power(f) + 5) / 5 && power(temple) < power(f) * 2) {
            /* The temple that out-powered the noble is the one outlawed (the original overwrote it with a random god). */
            PS->persecuted[r] = def(temple)->id;
            rumour_add(f, -1, NEWS_TEMPLE, r, TEXT_NEWS_TEMPLE);
            region_flag_set(r, REGION_TEMPLE_PERSECUTED);
            lose_power(temple, 1);
        } else { region_flag_clear(r, REGION_TEMPLE_PERSECUTED); }
    } else { region_flag_clear(r, REGION_TEMPLE_PERSECUTED); }
    if (PS->flags[r][REGION_CRIME_WAVE]) { lose_power(f, 1); }
    j = (power(faction_index(FACTION_THIEVES_GUILD)) + power(faction_index(FACTION_DARK_BROTHERHOOD))) / 2;
    if (rolled(0, ROLL_TOP) < (j - power(f) + 5) / 5) { rumour_add(-1, -1, NEWS_CRIME, r, TEXT_NEWS_CRIME); region_flag_set(r, REGION_CRIME_WAVE); lose_power(f, 1); }
    else { region_flag_clear(r, REGION_CRIME_WAVE); }
    other = -1;
    for (k = 0; k < total(); k++) { d = def(k); if (d && d->type == TYPE_COVEN && d->region == r) { other = k; break; } }
    if (PS->flags[r][REGION_WITCH_BURNINGS]) { add_power(other, -1); }
    if (other >= 0 && rolled(0, ROLL_TOP) < (power(other) - power(f) + 5) / 5) { rumour_add(f, -1, NEWS_WITCHES, r, TEXT_NEWS_WITCHES); region_flag_set(r, REGION_WITCH_BURNINGS); lose_power(other, 1); }
    else { region_flag_clear(r, REGION_WITCH_BURNINGS); }
}

void politics_round(df_u32 now, int write)
{
    int f, j, o, roll, bonus, count, tally;

    g_now = now; g_writing = write;
    PS->now_last = now;
    if (write) { purge(); }
    for (f = 0; f < total(); f++) {
        if (!is_party(f)) { continue; }
        /* A. Drift. */
        roll = rolled(0, ROLL_TOP);
        if (PS->factor[f] + sum_allies(f) / 10 - (power(ST->enemy[f][0]) + power(ST->enemy[f][1]) + power(ST->enemy[f][2])) / 10 + (parent_of(f) >= 0 ? power(parent_of(f)) / 10 : 0) > roll) { add_power(f, 1); }
        else { add_power(f, -1); }
        if (power(f) < max_child_power(f)) { add_power(f, 1); }
        bonus = power(f) / 5;
        /* B. Between factions. War declared becomes war ongoing, directly. */
        if (is_noble(f) && PS->flags[region_of(f)][REGION_WAR_BEGINNING]) { PS->flags[region_of(f)][REGION_WAR_BEGINNING] = 0; PS->flags[region_of(f)][REGION_WAR_ONGOING] = 1; }
        for (j = 0; j < FACTION_LINKS; j++) {
            o = ST->ally[f][j];
            if (o >= 0 && (shared(f, o) + PS->factor[f] + bonus) / 5 + 70 < rolled(0, ROLL_TOP)) { break_alliance(f, j); }
        }
        for (j = 0; j < FACTION_LINKS; j++) {
            o = ST->enemy[f][j];
            if (o >= 0 && !borders(f, o) && (shared(f, o) + PS->factor[f] + bonus) / 5 < rolled(0, ROLL_TOP)) { make_peace(f, j); }
        }
        for (j = 0; j < FACTION_LINKS; j++) {
            if (ST->ally[f][j] >= 0) { continue; }
            o = random_party();
            if (o < 0 || o == f || has_ally(f, o) || has_enemy(f, o) || any_has_enemy(f, o)) { continue; }
            { int k, skip = 0; for (k = 0; k < FACTION_LINKS; k++) { skip |= has_ally(ST->enemy[f][k], o); } if (skip || tree_relation(f, o)) { continue; } }
            if ((shared(f, o) + PS->factor[f] + bonus) / 5 <= rolled(0, ROLL_TOP)) { break; }
            if (is_noble(f)) { rumour_add(f, o, NEWS_ALLIANCE, region_of(f), TEXT_NEWS_ALLIANCE); }
            if (is_noble(o)) { rumour_add(o, f, NEWS_ALLIANCE, region_of(o), TEXT_NEWS_ALLIANCE); }
            make_alliance(f, j, o);
            break;
        }
        for (j = 0; j < FACTION_LINKS; j++) { if (can_war(f, ST->enemy[f][j])) { war(f, ST->enemy[f][j]); break; } }
        for (j = 0; j < FACTION_LINKS; j++) {
            int relation, k, skip;

            if (ST->enemy[f][j] >= 0) { continue; }
            o = random_party();
            if (o < 0 || o == f || has_ally(f, o) || has_enemy(f, o)) { continue; }
            skip = 0;
            for (k = 0; k < FACTION_LINKS; k++) { skip |= has_enemy(ST->enemy[f][k], o) | has_ally(ST->ally[f][k], o); }
            if (skip) { continue; }
            relation = tree_relation(f, o);
            if (relation == 1 || relation == 3) { continue; }
            if ((shared(f, o) + PS->factor[f] + bonus) / 5 + (relation == 2 ? 10 : 0) + 70 >= rolled(0, ROLL_TOP)) { break; }
            if (is_noble(f)) { rumour_add(f, o, NEWS_ENEMIES, region_of(f), TEXT_NEWS_ENEMIES); }
            if (is_noble(o)) { rumour_add(o, f, NEWS_ENEMIES, region_of(o), TEXT_NEWS_ENEMIES); }
            make_enemies(f, j, o);
            if (is_noble(f) && is_noble(o) && borders(f, o)) {
                rumour_add(f, o, NEWS_GOSSIP, 0, TEXT_NEWS_WAR_GOSSIP);
                rumour_add(f, o, NEWS_WAR_STATE, region_of(f), TEXT_NEWS_WAR);
                rumour_add(o, f, NEWS_WAR_STATE, region_of(o), TEXT_NEWS_WAR);
                region_flag_set(region_of(f), REGION_WAR_BEGINNING); region_flag_set(region_of(o), REGION_WAR_BEGINNING);
            }
            break;
        }
        /* C. A new ruler. */
        if (!(def(f)->flags & FACTION_KEEPS_RULER) && rolled(0, ROLL_TOP) > PS->factor[f] / 3 + 70) {
            if (is_noble(f)) { rumour_add(f, -1, NEWS_NEW_RULER, region_of(f), TEXT_NEWS_NEW_RULER); }
            PS->factor[f] = (unsigned char)(rolled(0, 50) + 20);
            PS->seed[f] = (PS->seed[f] << 16) | (df_u32)rolled(0, 32767);
            rumour_add(f, -1, NEWS_GOSSIP, 0, TEXT_NEWS_RULER_GOSSIP);
        }
        /* D. The region's events (nobles only). */
        if (is_noble(f)) { events(f); }
    }
    /* E. The Thieves and the Brotherhood by the crime waves, the Merchants by prices and by war and famine. */
    count = 0; for (j = 0; j < REGIONS; j++) { count += PS->flags[j][REGION_CRIME_WAVE]; }
    add_power(faction_index(FACTION_THIEVES_GUILD), count - 1); add_power(faction_index(FACTION_DARK_BROTHERHOOD), count - 1);
    count = 0; for (j = 0; j < REGIONS; j++) { count += PS->flags[j][REGION_PRICES_HIGH]; }
    if (count >= 3) { add_power(faction_index(FACTION_MERCHANTS), 1); }
    count = 0; for (j = 0; j < REGIONS; j++) { count += PS->flags[j][REGION_PRICES_LOW]; }
    if (count >= 3) { add_power(faction_index(FACTION_MERCHANTS), -1); }
    tally = 0; for (j = 0; j < REGIONS; j++) { tally += PS->groups[j][GROUP_FAMINE]; }
    if (tally >= 3) { add_power(faction_index(FACTION_MERCHANTS), -1); }
    tally = 0; for (j = 0; j < REGIONS; j++) { tally += PS->groups[j][GROUP_WAR]; }
    if (tally >= 3) { add_power(faction_index(FACTION_MERCHANTS), 1); }
    g_writing = 0;
}

/* ---- Prices ---- */

void politics_prices(void)
{
    int r, f, i, chance, price;

    for (r = 0; r < REGIONS; r++) {
        f = -1;
        for (i = 0; i < total(); i++) { const FactionDef *d = def(i); if (d->type == TYPE_NOBLE && d->region == r) { f = i; break; } }
        if (f < 0) { continue; }
        price = ST->price[r];
        chance = (power(faction_index(FACTION_MERCHANTS)) - power(f)) / PRICE_CHANCE_DIV;
        chance = chance + PRICE_CHANCE_BASE - (price - PRICE_NORMAL) / PRICE_CHANCE_STEP;
        price = rolled(0, ROLL_TOP) < chance ? price * PRICE_DRIFT_UP / PRICE_DRIFT_DIV : price * PRICE_DRIFT_DOWN / PRICE_DRIFT_DIV;
        if (price < PRICE_LEAST) { price = PRICE_LEAST; }
        if (price > PRICE_MOST) { price = PRICE_MOST; }
        ST->price[r] = (unsigned short)price;
        if (price > PRICE_HIGH) { region_flag_set(r, REGION_PRICES_HIGH); }
        else if (price < PRICE_LOW) { region_flag_set(r, REGION_PRICES_LOW); }
        else { region_flag_clear(r, REGION_PRICES_HIGH); region_flag_clear(r, REGION_PRICES_LOW); }
    }
}

void politics_minute(df_u32 minute)
{
    if (!ST->ready || !PS->ready) { return; }
    if (minute % MINUTES_PER_DAY == 0) { politics_prices(); }
    if (minute % POLITICS_PERIOD == 0) { politics_round(minute, 1); }
}

/* ---- The start ---- */

void politics_defaults(PoliticsState *s)
{
    int i;

    mem_zero(s, sizeof(*s));
    for (i = 0; i < faction_total(); i++) {
        s->factor[i] = (unsigned char)(random_range(0, 50) + 20);
        s->seed[i] = ((df_u32)random_range(0, 32767) << 16) | (df_u32)random_range(0, 32767);
    }
    s->ready = 1;
}

int politics_valid(const PoliticsState *s)
{
    int i;

    for (i = 0; i < RUMOURS_MOST; i++) { if (s->rumour[i].used && s->rumour[i].text[RUMOUR_TEXT - 1]) { return 0; } }
    for (i = 0; i < REGIONS; i++) { if (s->persecuted[i] < 0) { return 0; } }
    return 1;
}

void politics_start(df_u32 now)
{
    int i;

    politics_defaults(PS);
    /* init_game_data: every region starts at a price of 750..1250. */
    for (i = 0; i < REGIONS; i++) { ST->price[i] = (unsigned short)(random_range(0, 500) + 750); }
    for (i = 0; i < POLITICS_START_ROUNDS; i++) { politics_round(now, i == POLITICS_START_ROUNDS - 1); }
}
