#ifndef GFALL_POSSESSIONS_INTERNAL_H
#define GFALL_POSSESSIONS_INTERNAL_H

#include "../possessions.h"

/* Shared window state and geometry. */
extern Possessions possessions_view;
int possessions_hit(int x, int y, int left, int top, int right, int bottom);

/* Modal input. */
void possessions_dialog_clear(void);
void possessions_prompt_frame(const PlayIn *in, int click);
void possessions_notice_frame(const PlayIn *in, int click);
void possessions_message_frame(void);

/* Inventory and trade. */
void possessions_inventory_frame(const PlayIn *in, int click);
void possessions_trade_quote(void);
void possessions_trade_answer(int accepted);
void possessions_inventory_notice_closed(void);
int possessions_drop_gold(int value);

/* People and services. */
void possessions_service_frame(const PlayIn *in, int click);
void possessions_talk_frame(const PlayIn *in, int click);
void possessions_bank_frame(const PlayIn *in, int click);
void possessions_bank_run(int op, int amount, int confirmed);
void possessions_court_answer(int yes);
void possessions_court_notice_closed(void);

/* Spellbook and makers. */
int possessions_spellbook_frame(const PlayIn *in, int click);
void possessions_spellshop_frame(const PlayIn *in, int click);
void possessions_itemmaker_frame(const PlayIn *in, int click);
void possessions_spellmaker_frame(const PlayIn *in, int click);
void possessions_alchemy_frame(const PlayIn *in, int click);
void possessions_itemmaker_name(void);
void possessions_spellmaker_name(void);
void possessions_spellbook_name(void);

/* Character sheet and books. */
void possessions_sheet_frame(const PlayIn *in, int click);
void possessions_skill_frame(const PlayIn *in, int click);
int possessions_read_open(int record);
void possessions_read_close(void);
void possessions_book_page(int page);
void possessions_reading_frame(const PlayIn *in, int click);

#endif
