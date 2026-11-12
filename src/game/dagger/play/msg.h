#ifndef GFALL_MSG_H
#define GFALL_MSG_H

/*
 * The one status line interaction_mode_cycle and the HUD write.
 * Drawn by the app with ui_string. Slice 1 does not colour it.
 */

#define MESSAGE_LEN 80

void msg_set(const char *text);
const char *message_line(void);
void msg_clear(void);

#endif
