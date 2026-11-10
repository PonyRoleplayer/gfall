#include "game/ogfall/play/names.h"
#include "game/ogfall/play/state.h"
#include "engine/core/mem.h"
#include "game/ogfall/tables/namedata.h"

/* See names.h. */

static const df_u8 *g_data;
static df_u32 g_size;
static void (*g_expand)(const char *in, char *out, int cap);

/* One bank's six sets, as namegen_read_part has them in namegen_part_offsets and _counts. */
static df_u32 g_offset[6], g_count[6];

void og_names_set_data(const df_u8 *bytes, df_u32 size)
{
    g_data = bytes;
    g_size = size;
}

void og_names_set_expand(void (*expand)(const char *in, char *out, int cap))
{
    g_expand = expand;
}

int og_region_bank(int region)
{
    return region >= 0 && region < 62 ? g_region_bank[region] : 0;
}

/* rand() % n on the session's stream, as the original writes it. */
static int rand_mod(int n)
{
    return og_random_range(0, 32767) % n;
}

static df_u32 word(const df_u8 *p)
{
    return (df_u32)p[0] | (df_u32)p[1] << 8 | (df_u32)p[2] << 16 | (df_u32)p[3] << 24;
}

/* Append src to dst, never past cap bytes in all (func_000A1054, strncat at the call's limit). */
static void append(char *dst, int cap, const char *src)
{
    int n, i;

    n = 0;
    while (dst[n]) {
        n++;
    }
    for (i = 0; src[i] && n < cap - 1; i++) {
        dst[n++] = src[i];
    }
    dst[n] = 0;
}

/* namegen_read_part: one piece of the set, picked at random, into piece (at most 10 characters). */
static void read_part(int set, char *piece)
{
    df_u32 at;
    int i;

    piece[0] = 0;
    if (!g_count[set]) {
        return;
    }
    at = g_offset[set] + (df_u32)rand_mod((int)g_count[set]) * 10u;
    for (i = 0; i < 10 && at + (df_u32)i < g_size && g_data[at + (df_u32)i]; i++) {
        piece[i] = (char)g_data[at + (df_u32)i];
    }
    piece[i] = 0;
}

/* The part to the name: copied when first, else appended. */
static void part(char *name, int cap, int set, int first)
{
    char piece[11];

    read_part(set, piece);
    if (first) {
        name[0] = 0;
    }
    append(name, cap, piece);
}

/* name_generate_first, by bank. */
static void given(int bank, int female, char *name, int cap)
{
    int p;

    switch (bank) {
    case 1:
        part(name, cap, 0, 1);
        part(name, cap, 1, 0);
        part(name, cap, 2, 0);
        if (!female && rand_mod(100) < 75) {
            part(name, cap, 3, 0);
        }
        if (female) {
            part(name, cap, 4, 0);
        }
        break;
    case 2:
        p = female ? 2 : 0;
        part(name, cap, p, 1);
        part(name, cap, p + 1, 0);
        break;
    case 8:
        part(name, cap, 0, 1);
        if (rand_mod(50) < 25) {
            part(name, cap, 1, 0);
        }
        part(name, cap, 2, 0);
        break;
    case 9:
        part(name, cap, 0, 1);
        if (rand_mod(50) < 25) {
            part(name, cap, 1, 0);
        }
        part(name, cap, 2, 0);
        if (female) {
            part(name, cap, 3, 0);
        }
        break;
    case 10:
        if (!female && rand_mod(100) < 25) {
            part(name, cap, 3, 1);
            append(name, cap, " ");
            part(name, cap, 0, 0);
        } else {
            part(name, cap, 0, 1);
        }
        part(name, cap, 1, 0);
        part(name, cap, 2, 0);
        break;
    default:
        p = female ? 2 : 0;
        part(name, cap, p, 1);
        part(name, cap, p + 1, 0);
        break;
    }
}

/* name_generate_surname: banks 1, 8, 9 and 10 have none; the Nords' is two pieces and "sen"; the rest 4 and 5. */
static void family(int bank, char *name, int cap)
{
    name[0] = 0;
    switch (bank) {
    case 1: case 8: case 9: case 10:
        break;
    case 2:
        part(name, cap, 0, 0);
        part(name, cap, 1, 0);
        append(name, cap, g_surname_sen);
        break;
    default:
        part(name, cap, 4, 0);
        part(name, cap, 5, 0);
        break;
    }
}

void og_name_generate(int bank, int female, char *out, int cap)
{
    char first[30], last[30];
    const df_u8 *b;
    int i;

    out[0] = 0;
    if (!g_data || bank < 0 || bank >= 11 || g_size < (df_u32)(bank + 1) * 48u) {
        return;
    }
    b = g_data + bank * 48;
    for (i = 0; i < 6; i++) {
        g_offset[i] = word(b + i * 4);
        g_count[i] = word(b + 24 + i * 4);
    }
    first[0] = 0;
    given(bank, female, first, 30);
    /* The given name is copied out (40 characters at most) before the surname reuses the part buffer. */
    for (i = 0; first[i] && i < 39 && i < cap - 1; i++) {
        out[i] = first[i];
    }
    out[i] = 0;
    family(bank, last, 30);
    if (last[0]) {
        append(out, cap < 41 ? cap : 41, " ");
        append(out, cap < 41 ? cap : 41, last);
    }
}

void og_name_seeded(int bank, int female, df_u32 seed, char *out, int cap)
{
    int saved;

    saved = og_random_range(0, 32767);
    og_random_seed(seed);
    og_name_generate(bank, female, out, cap);
    og_random_seed((df_u32)saved);
}

int og_npc_name(const OgNpcName *npc, char *out, int cap)
{
    int i;

    out[0] = 0;
    if (npc->type != 8 && npc->type != 53) {
        return 0;
    }
    if (npc->faction && npc->faction_is_individual && npc->faction_name) {
        for (i = 0; npc->faction_name[i] && i < cap - 1; i++) {
            out[i] = npc->faction_name[i];
        }
        out[i] = 0;
        return 1;
    }
    if (npc->twin) {
        og_name_seeded(og_region_bank(npc->region), npc->twin_female, npc->name_seed, out, cap);
    } else {
        og_name_seeded(og_region_bank(npc->region), npc->female, npc->id, out, cap);
    }
    return 1;
}

/* An entry of a list ended by a null pointer, drawn by str_list_random: one rand_range(0, count - 1). */
static const char *pick(const char *const *list)
{
    int n;

    n = 0;
    while (list[n]) {
        n++;
    }
    return list[og_random_range(0, n - 1)];
}

static void copy(char *out, int cap, const char *text)
{
    out[0] = 0;
    append(out, cap, text);
}

/* "%s %s" of two words, run through the macro expander when the game gave one. */
static void two_words(char *out, int cap, const char *first, const char *second)
{
    char line[96];

    copy(line, (int)sizeof(line), first);
    append(line, (int)sizeof(line), " ");
    append(line, (int)sizeof(line), second);
    if (g_expand) {
        g_expand(line, out, cap);
    } else {
        copy(out, cap, line);
    }
}

void og_building_name(const OgBuildingName *b, char *out, int cap)
{
    int saved;
    const char *first;
    const char *last;

    out[0] = 0;
    saved = og_random_range(0, 32767);
    if (b->player_house) {
        copy(out, cap, g_your_house);
        og_random_seed((df_u32)saved);
        return;
    }
    og_random_seed(b->name_seed);
    switch (b->type) {
    case 0: case 2: case 5: case 6: case 7: case 8: case 9: case 10: case 12: case 13:
        first = pick(g_shop_first);
        last = pick(g_shop_last[b->type]);
        two_words(out, cap, first, last);
        break;
    case 1:
        copy(out, cap, b->for_sale ? g_house_for_sale : g_your_house);
        break;
    case 3:
        /* "The Bank of %s": the region's name takes the place of %s (the format's own words are read from FALL.EXE). */
        copy(out, cap, g_bank_of);
        {
            int n;

            n = 0;
            while (out[n] && !(out[n] == '%' && out[n + 1] == 's')) {
                n++;
            }
            out[n] = 0;
            append(out, cap, b->region_name ? b->region_name : "");
        }
        break;
    case 11:
        if (b->faction_name) {
            copy(out, cap, b->faction_name);
        }
        break;
    case 14:
        if (b->child_name) {
            copy(out, cap, b->child_name);
        } else if (b->faction_name) {
            copy(out, cap, b->faction_name);
        }
        break;
    case 15:
        first = pick(g_tavern_first);
        last = pick(g_tavern_last);
        two_words(out, cap, first, last);
        break;
    case 16:
        copy(out, cap, g_palace);
        break;
    case 23:
        copy(out, cap, g_city_wall);
        break;
    default:
        if (b->faction == 108 && b->brotherhood) {
            copy(out, cap, g_brotherhood);
        } else if (b->faction == 42 && b->thieves) {
            copy(out, cap, g_thieves);
        } else if (b->resident && b->resident[0]) {
            copy(out, cap, "The ");
            append(out, cap, b->resident);
            append(out, cap, " Residence");
        } else {
            copy(out, cap, g_residence);
        }
        break;
    }
    og_random_seed((df_u32)saved);
}
