#include "game/dagger/play/atlas.h"
#include "game/dagger/formats/worldmap.h"
#include "engine/core/mem.h"

/* See atlas.h. */

/* A bit a location, in words; and a new game's, to tell a save what changed. */
static df_u32 *g_bits;
static df_u32 *g_start;
static int g_words;

/* The bits ready, the first time: the map read and a new game's known places. */
static int ready(void)
{
    int n;
    int k;

    if (g_bits) {
        return 1;
    }
    if (!worldmap_open()) {
        return 0;
    }
    n = worldmap_locations();
    g_words = (n + 31) / 32;
    g_bits = (df_u32 *)mem_keep((df_u32)(g_words > 0 ? g_words : 1) * 4u);
    g_start = (df_u32 *)mem_keep((df_u32)(g_words > 0 ? g_words : 1) * 4u);
    if (!g_bits || !g_start) {
        g_bits = 0;
        return 0;
    }
    mem_zero(g_start, (df_u32)g_words * 4u);
    for (k = 0; k < n; k++) {
        int px;
        int py;
        int region;
        int index;
        int type;
        int known;

        if (worldmap_location_info(k, &px, &py, &region, &index, &type, &known) && known) {
            g_start[k >> 5] |= 1u << (k & 31);
        }
    }
    for (k = 0; k < g_words; k++) {
        g_bits[k] = g_start[k];
    }
    return 1;
}

void atlas_new_game(void)
{
    int k;

    if (!ready()) {
        return;
    }
    for (k = 0; k < g_words; k++) {
        g_bits[k] = g_start[k];
    }
}

int atlas_known(int k)
{
    return ready() && k >= 0 && (k >> 5) < g_words && (g_bits[k >> 5] >> (k & 31)) & 1u;
}

void atlas_learn(int k)
{
    if (ready() && k >= 0 && (k >> 5) < g_words) {
        g_bits[k >> 5] |= 1u << (k & 31);
    }
}

int atlas_changed_count(void)
{
    int n;
    int k;

    n = 0;
    for (k = 0; ready() && k < g_words; k++) {
        n += g_bits[k] != g_start[k];
    }
    return n;
}

void atlas_changed_at(int i, int *word, df_u32 *bits)
{
    int k;

    *word = -1;
    *bits = 0;
    for (k = 0; ready() && k < g_words; k++) {
        if (g_bits[k] != g_start[k] && i-- == 0) {
            *word = k;
            *bits = g_bits[k];
            return;
        }
    }
}

void atlas_put(int word, df_u32 bits)
{
    if (ready() && word >= 0 && word < g_words) {
        g_bits[word] = bits;
    }
}
