#include "pics.h"

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "game/dagger/formats/texture.h"

/* See pics.h. One open and two bytes a file, not the whole archive. */

int tex_list(TexArc *out, int cap)
{
    int file;
    int n;

    n = 0;
    for (file = 0; file < TEX_SCAN; file++) {
        char name[16];
        VfsFile *f;
        df_u8 head[2];
        int count;

        texture_filename(file, name, (int)sizeof(name));
        f = vfs_open(name);
        if (!f) {
            continue;
        }
        count = 0;
        if (vfs_size(f) >= 2 && vfs_read_at(f, 0, head, 2)) {
            count = (int)df_ri16(head);
        }
        vfs_close(f);
        if (count < 0 || count > 1024) {
            count = 0;
        }
        if (out && n < cap) {
            out[n].file = file;
            out[n].records = count;
        }
        n++;
    }
    return n;
}
