#include "game/dagger/play/msg.h"
#include "engine/core/df.h"

/* Oldest first: g_entry[0] is the oldest live one, g_entry[g_count - 1] the newest. */
static char g_entry[MSG_FEED][MESSAGE_LEN];
static float g_age[MSG_FEED];
static int g_count;

static int same(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

void msg_set(const char *text)
{
    int i;

    if (!text) {
        g_count = 0;
        return;
    }
    if (!text[0]) { return; }
    if (g_count && same(g_entry[g_count - 1], text)) {
        g_age[g_count - 1] = 0;
        return;
    }
    if (g_count == MSG_FEED) {
        for (i = 1; i < MSG_FEED; i++) {
            int k;
            for (k = 0; k < MESSAGE_LEN; k++) { g_entry[i - 1][k] = g_entry[i][k]; }
            g_age[i - 1] = g_age[i];
        }
        g_count--;
    }
    for (i = 0; i < MESSAGE_LEN - 1 && text[i]; i++) { g_entry[g_count][i] = text[i]; }
    g_entry[g_count][i] = 0;
    g_age[g_count++] = 0;
}

void msg_tick(float dt)
{
    int i, kept;

    if (dt <= 0) { return; }
    /* Entries age together, so the live ones are always the newest run. */
    kept = 0;
    for (i = 0; i < g_count; i++) {
        g_age[i] += dt;
        if (g_age[i] < MSG_LIFE) {
            if (kept != i) {
                int k;
                for (k = 0; k < MESSAGE_LEN; k++) { g_entry[kept][k] = g_entry[i][k]; }
                g_age[kept] = g_age[i];
            }
            kept++;
        }
    }
    g_count = kept;
}

const char *msg_entry(int index, int *alpha)
{
    float left;
    int slot;

    slot = g_count - 1 - index;
    if (index < 0 || slot < 0) { return 0; }
    if (alpha) {
        left = MSG_LIFE - g_age[slot];
        *alpha = left >= MSG_FADE ? 255 : df_ifloor(left * 255.0f / MSG_FADE) + 1;
        if (*alpha > 255) { *alpha = 255; }
    }
    return g_entry[slot];
}

const char *message_line(void)
{
    return g_count ? g_entry[g_count - 1] : "";
}

void msg_clear(void)
{
    g_count = 0;
}
