#include "game/dagger/play/msg.h"

static char g_line[MESSAGE_LEN];

void msg_set(const char *text)
{
    int i;

    if (!text) {
        g_line[0] = 0;
        return;
    }
    for (i = 0; i < MESSAGE_LEN - 1 && text[i]; i++) {
        g_line[i] = text[i];
    }
    g_line[i] = 0;
}

const char *message_line(void)
{
    return g_line;
}

void msg_clear(void)
{
    g_line[0] = 0;
}
