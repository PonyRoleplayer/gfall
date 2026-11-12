#ifndef GFALL_SAVE_H
#define GFALL_SAVE_H

/* GFALL session serialization. Read docs/gfall.md before editing. */
#define SAVE_DIR "gfall\\saves"

#include "game/dagger/play/state.h"
typedef struct SaveInfo {
    int status;
    char label[32], character[NAME_LEN];
    int level;
    df_u32 minutes;
    int preview_valid;
    df_u16 preview[THUMB_PIXELS];
} SaveInfo;
/* Nondestructive preview: 1 valid, 0 missing, -1 damaged/unsupported. */
int save_info(int slot, SaveInfo *info);

/* Numeric slots 1..9999: gfall/saves/S0001.SAV beside the executable.
 * Temporary write + verified reread + replacing rename. Failed loads leave
 * the live state intact. This API restores data, not the rendered Level. */
int save_write(int slot);
int save_read(int slot);
const char *save_error(void);


#endif
