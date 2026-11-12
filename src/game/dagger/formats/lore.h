#ifndef GFALL_LORE_H
#define GFALL_LORE_H

/* PAINT.DAT's four lists per picture and NAMEGEN.DAT's bank/set directory. */
#include "engine/platform/fs.h"
typedef struct NameParts {
    FsFile *file;
    df_u32 offset[6], count[6], size;
} NameParts;
int painting_read(int picture, df_u8 choices[40]);
int name_open(NameParts *parts, int bank);
void name_close(NameParts *parts);
int name_piece(NameParts *parts, int set, df_u32 pick, char text[11]);
#endif
