#ifndef GFALL_DESCRIBE_H
#define GFALL_DESCRIBE_H

/* Original TEXT.RSC item/effect descriptions, expanded from the selected
 * persistent record. No renderer or shared scratch-object globals. */
#include "game/dagger/play/spellbook.h"
#include "engine/core/df.h"
int item_info_record(df_u32 item);
void item_info(df_u32 item, char *out, int capacity);
void spell_help(df_u32 spell, int effect, char *out, int capacity);
/* The same for a definition not yet owned (the spell shop's). */
void spell_help_def(const SpellDef *def, int effect, char *out, int capacity);
/* Caption from the painting's saved seed; returns its CIF picture index + 1. */
int painting_info(df_u32 item, char *out, int capacity);
#endif
