#include "render/texcache.h"
#include "game/world.h"
#include "render/glide.h"

#define TEX_SLOTS 256

static df_u32 g_addr[TEX_SLOTS];
static int g_live[TEX_SLOTS];
static int g_bound = -1;
static const char *g_why = "texture upload failed";

const char *texcache_error(void)
{
    return g_why;
}

int texcache_upload(const World *world)
{
    const Palette *pal;
    int i;
    int n;

    g_bound = -1;
    pal = world_palette(world);
    if (!pal || !glide_palette(pal->r, pal->g, pal->b)) {
        g_why = glide_error();
        return 0;
    }
    n = world_image_count(world);
    if (n < 1 || n > TEX_SLOTS) {
        g_why = "texture count";
        return 0;
    }
    for (i = 0; i < n; i++) {
        const Image *img;

        img = world_image(world, i);
        if (!img || !glide_upload(img->pot, img->pot_w, img->pot_h,
                                  world_image_cutout(world, i),
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
