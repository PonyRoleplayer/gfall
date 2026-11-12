#include "game/dagger/play/msg.h"

static char g_line[OG_MSG];

void og_msg_set(const char *text)
{
    int i;

    if (!text) {
        g_line[0] = 0;
        return;
    }
    for (i = 0; i < OG_MSG - 1 && text[i]; i++) {
        g_line[i] = text[i];
    }
    g_line[i] = 0;
}

const char *og_msg(void)
{
    return g_line;
}

void og_msg_clear(void)
{
    g_line[0] = 0;
}
