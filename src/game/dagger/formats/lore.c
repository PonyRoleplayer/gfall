#include "game/dagger/formats/lore.h"
#include "engine/core/mem.h"

int painting_read(int picture, df_u8 choices[40])
{
    FsFile *file;
    int good, i;
    if (picture < 0 || picture >= 180) { return 0; }
    file = fs_open("PAINT.DAT");
    if (!file) { return 0; }
    good = fs_read_at(file,(df_u32)picture*40,choices,40);
    fs_close(file);
    for (i = 0; good && i < 4; i++) { if (choices[i*10] == 255) { good = 0; } }
    return good;
}

int name_open(NameParts *parts, int bank)
{
    df_u8 header[48];
    int i;
    mem_zero(parts,sizeof(*parts));
    if (bank < 0 || bank >= 11) { return 0; }
    parts->file = fs_open("NAMEGEN.DAT");
    if (!parts->file) { return 0; }
    parts->size = fs_size(parts->file);
    if (!fs_read_at(parts->file,bank*48,header,48)) { name_close(parts); return 0; }
    for (i = 0; i < 6; i++) {
        parts->offset[i] = df_ru32(header+i*4); parts->count[i] = df_ru32(header+24+i*4);
        if (parts->offset[i] > parts->size || parts->count[i] > (parts->size-parts->offset[i])/10) {
            name_close(parts); return 0;
        }
    }
    return 1;
}
void name_close(NameParts *parts)
{
    if (parts->file) { fs_close(parts->file); }
    mem_zero(parts,sizeof(*parts));
}
int name_piece(NameParts *parts, int set, df_u32 pick, char text[11])
{
    text[0] = 0;
    if (!parts->file || set < 0 || set >= 6 || !parts->count[set]) { return 0; }
    if (!fs_read_at(parts->file,parts->offset[set]+(pick%parts->count[set])*10,text,10)) { return 0; }
    text[10] = 0;
    return 1;
}
