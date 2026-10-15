#include "engine/gfx/texcache.h"
#include "engine/assets/level.h"
#include "engine/gfx/glide.h"

#define TEX_SLOTS 256

static df_u32 g_addr[TEX_SLOTS];
static int g_live[TEX_SLOTS];
static int g_bound = -1;
static const char *g_why = "texture upload failed";

const char *texcache_error(void)
{
    return g_why;
}

int texcache_upload(const Level *level)
{
    const Palette *pal;
    int i;
    int n;

    g_bound = -1;
    for (i = 0; i < TEX_SLOTS; i++) {
        g_live[i] = 0;
    }
    if (!level || !level->palette) {
        g_why = "texture upload failed";
        return 0;
    }
    pal = level->palette;
    if (!glide_palette(pal->r, pal->g, pal->b)) {
        g_why = glide_error();
        return 0;
    }
    n = level->nimages;
    if (n < 1 || n > TEX_SLOTS) {
        g_why = "texture count";
        return 0;
    }
    for (i = 0; i < n; i++) {
        const Image *img;
        int cutout;

        img = level->images[i];
        if (!img) {
            continue;
        }
        cutout = level->cutout && level->cutout[i] ? 1 : 0;
        if (!glide_upload(img->pot, img->pot_w, img->pot_h,
                                  cutout,
                                  pal->r, pal->g, pal->b, &g_addr[i])) {
            g_why = glide_error();
            return 0;
        }
        g_live[i] = 1;
    }
    return 1;
}

void texcache_bind(int slot)
{
    if (slot < 0 || slot >= TEX_SLOTS || !g_live[slot] || slot == g_bound) {
        return;
    }
    glide_bind(g_addr[slot]);
    g_bound = slot;
}

void texcache_forget(void)
{
    g_bound = -1;
}
