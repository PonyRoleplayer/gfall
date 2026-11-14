#ifndef GFALL_MSG_H
#define GFALL_MSG_H

/*
 * The notification feed: what interaction_mode_cycle, the HUD and the rules
 * say to the player. At most MSG_FEED entries; the newest is on top and each
 * older one a row lower. An entry fades over its last MSG_FADE seconds and is
 * gone after MSG_LIFE. A sixth drops the oldest to make room; a line equal to
 * the newest is not added again, it renews that one (a held key's repeat).
 * Drawn by the app with ui_string_alpha. msg_tick is called once a frame in
 * real seconds, so the feed fades under a page too.
 */

#define MESSAGE_LEN 80
#define MSG_FEED 5
#define MSG_LIFE 7.0f
#define MSG_FADE 2.0f

/* Adds an entry; null clears the feed. */
void msg_set(const char *text);
void msg_tick(float dt);
/* Entry 0 is the newest. Null past the last live one; *alpha is 1..255 when given. */
const char *msg_entry(int index, int *alpha);
/* The newest entry's text, "" when there is none. */
const char *message_line(void);
void msg_clear(void);

#endif
