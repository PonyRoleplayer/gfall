#ifndef OGFALL_MSG_H
#define OGFALL_MSG_H

/*
 * The one status line interaction_mode_cycle and the HUD write.
 * Drawn by the app with ui_string. Slice 1 does not colour it.
 */

#define OG_MSG 80

void og_msg_set(const char *text);
const char *og_msg(void);
void og_msg_clear(void);

#endif
