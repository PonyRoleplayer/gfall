#ifndef OGFALL_SAVE_H
#define OGFALL_SAVE_H

/* OGFALL session serialization. Read docs/ogfall.md before editing. */
#define OG_SAVE_DIR "ogfall\\saves"

#include "game/dagger/play/state.h"
typedef struct OgSaveInfo {
    int status;
    char label[32], character[OG_NAME];
    int level;
    df_u32 minutes;
    int preview_valid;
    df_u16 preview[OG_THUMB_PIXELS];
} OgSaveInfo;
/* Nondestructive preview: 1 valid, 0 missing, -1 damaged/unsupported. */
int og_save_info(int slot, OgSaveInfo *info);

/* Numeric slots 1..9999: ogfall/saves/S0001.SAV beside the executable.
 * Temporary write + verified reread + replacing rename. Failed loads leave
 * the live state intact. This API restores data, not the rendered Level. */
int og_save_write(int slot);
int og_save_read(int slot);
const char *og_save_error(void);


#endif
