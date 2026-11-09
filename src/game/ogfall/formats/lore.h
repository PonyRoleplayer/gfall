#ifndef OGFALL_LORE_H
#define OGFALL_LORE_H

/* PAINT.DAT's four lists per picture and NAMEGEN.DAT's bank/set directory. */
#include "engine/platform/fs.h"
typedef struct OgNameParts {
    FsFile *file;
    df_u32 offset[6], count[6], size;
} OgNameParts;
int og_painting_read(int picture, df_u8 choices[40]);
int og_name_open(OgNameParts *parts, int bank);
void og_name_close(OgNameParts *parts);
int og_name_piece(OgNameParts *parts, int set, df_u32 pick, char text[11]);
#endif
