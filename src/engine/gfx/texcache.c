#include "engine/gfx/texcache.h"
#include "engine/assets/level.h"
#include "engine/gfx/glide.h"
#include "engine/core/log.h"

/*
 * One upload per picture, full size, one mip level. A chain is not
 * used: on a 2MB TMU the odd levels overwrite the base image, which
 * scrambled the ground and the font. A city that does not fit keeps
 * the pictures that did. 192KB is left for the font sheet.
 */
#define TEX_SLOTS 384
#define FONT_KEEP (192u * 1024u)

static df_u32 g_addr[TEX_SLOTS];
static int g_live[TEX_SLOTS];
static int g_n;
static int g_nlive;
static int g_unfit;
static df_u32 g_bytes;
static int g_bound_slot = -1;
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

    g_bound_slot = -1;
    g_n = 0;
    g_nlive = 0;
    g_unfit = 0;
    g_bytes = 0;
    for (i = 0; i < TEX_SLOTS; i++) {
        g_live[i] = 0;
        g_addr[i] = 0;
    }
    if (!level || !level->palette) {
        g_why = "texture upload failed";
        return 0;
    }
    pal = level->palette;
    if (!glide_palette(pal->r, pal->g, pal->b)) {
        g_why = glide_error();
        log_print(LOG_ERROR, "palette: %s", g_why);
        return 0;
    }
    n = level->nimages;
    if (n < 1 || n > TEX_SLOTS) {
        g_why = "texture count";
        log_print(LOG_ERROR, "textures: %d pictures, the cache holds 1..%d", n, TEX_SLOTS);
        return 0;
    }
    for (i = 0; i < n; i++) {
        const Image *img;
        int cutout;
        df_u32 need;
        df_u32 freeb;

        img = level->images[i];
        if (!img || !img->pot || img->pot_w < 1 || img->pot_h < 1) {
            continue;
        }
        if (img->pot_w != img->pot_h) {
            continue;
        }
        cutout = level->cutout && level->cutout[i] ? 1 : 0;
        if (!glide_texture_bytes(img->pot_w, img->pot_h, cutout, &need)) {
            continue;
        }
        freeb = glide_tex_free();
        if (freeb < need + FONT_KEEP) {
            g_unfit++;
            continue;
        }
        if (!glide_upload(img->pot, img->pot_w, img->pot_h, cutout,
                          pal->r, pal->g, pal->b, &g_addr[i])) {
            g_unfit++;
            continue;
        }
        g_live[i] = 1;
        g_nlive++;
        g_bytes += need;
    }
    g_n = n;
    log_print(LOG_INFO, "textures: %d of %d on the card, %u KB",
              g_nlive, n, (unsigned int)(g_bytes / 1024u));
    if (g_unfit > 0) {
        log_print(LOG_WARN, "textures: %d did not fit and draw with the last bound one",
                  g_unfit);
    }
    return 1;
}

void texcache_bind(int slot)
{
    if (slot < 0 || slot >= g_n || !g_live[slot]) {
        return;
    }
    if (slot == g_bound_slot) {
        return;
    }
    glide_bind(g_addr[slot]);
    g_bound_slot = slot;
}

void texcache_stats(int *live, int *unfit, int *total, df_u32 *bytes)
{
    if (live) {
        *live = g_nlive;
    }
    if (unfit) {
        *unfit = g_unfit;
    }
    if (total) {
        *total = g_n;
    }
    if (bytes) {
        *bytes = g_bytes;
    }
}

void texcache_forget(void)
{
    g_bound_slot = -1;
}
