#include "game/dagger/bind.h"
#include "engine/assets/loader.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/palette.h"
#include "game/dagger/texture.h"

/*
 * The three formats this game knows how to turn into engine objects.
 * Registered once, then world and block loading call them by name.
 */

static int load_col(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    (void)name;
    return palette_load(bytes, size, (Palette *)out);
}

static int load_arch(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    ArchLoad *load;

    (void)name;
    load = (ArchLoad *)out;
    if (!load) {
        return 0;
    }
    load->mesh = arch3d_load(bytes, size, load->id);
    return load->mesh != 0;
}

static int load_texture(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    TexLoad *load;

    (void)name;
    (void)bytes;
    (void)size;
    load = (TexLoad *)out;
    if (!load) {
        return 0;
    }
    load->out = texture_load(load->file, load->image);
    return load->out != 0;
}

static int g_bound;

void dagger_bind_loaders(void)
{
    if (g_bound) {
        return;
    }
    loader_register("col", load_col);
    loader_register("arch3d", load_arch);
    loader_register("texture", load_texture);
    g_bound = 1;
}
