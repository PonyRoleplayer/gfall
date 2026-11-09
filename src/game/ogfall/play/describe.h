#ifndef OGFALL_DESCRIBE_H
#define OGFALL_DESCRIBE_H

/* Original TEXT.RSC item/effect descriptions, expanded from the selected
 * persistent record. No renderer or shared scratch-object globals. */
#include "engine/core/df.h"
int og_item_info_record(df_u32 item);
void og_item_info(df_u32 item, char *out, int capacity);
void og_spell_help(df_u32 spell, int effect, char *out, int capacity);
/* Caption from the painting's saved seed; returns its CIF picture index + 1. */
int og_painting_info(df_u32 item, char *out, int capacity);
#endif
