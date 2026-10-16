#include "engine/gfx/texcache.h"
#include "engine/assets/level.h"
#include "engine/gfx/camera.h"
#include "engine/gfx/glide.h"

#define TEX_SLOTS 256
#define TINY 16
/* World caps in world.c. A nearer model is chosen from this list. */
#define NEAR_MAX 8192
#define BAND 512
#define BANDS 16

static df_u32 g_addr[TEX_SLOTS];
static df_u32 g_sharp[TEX_SLOTS];
static df_u32 g_bytes[TEX_SLOTS];
static int g_live[TEX_SLOTS];
static int g_sharp_live[TEX_SLOTS];
static int g_want[TEX_SLOTS];
static int g_small[TEX_SLOTS];
static const Image *g_img[TEX_SLOTS];
static int g_cut[TEX_SLOTS];
static const df_u8 *g_pr;
static const df_u8 *g_pg;
static const df_u8 *g_pb;
static int g_n;
static int g_bound_slot = -1;
static int g_bound_sharp;
static int g_cell_ok;
static int g_all;
static int g_cx, g_cy, g_cz;
static df_u8 g_model_near[NEAR_MAX];
static df_u8 g_flat_near[NEAR_MAX];
static int g_nmodels;
static int g_nflats;
static df_u8 g_shrink[TINY * TINY];
static const char *g_why = "texture upload failed";

const char *texcache_error(void)
{
    return g_why;
}

static int tiny_side(int pot)
{
    if (pot <= TINY) {
        return pot;
    }
    return TINY;
}

static void shrink_pot(const df_u8 *pot, int pot_w, int pot_h, int side)
{
    int y;

    for (y = 0; y < side; y++) {
        int x;
        int sy;

        sy = y * pot_h / side;
        for (x = 0; x < side; x++) {
            int sx;

            sx = x * pot_w / side;
            g_shrink[y * side + x] = pot[sy * pot_w + sx];
        }
    }
}

int texcache_upload(const Level *level)
{
    const Palette *pal;
    int i;
    int n;

    g_bound_slot = -1;
    g_cell_ok = 0;
    g_all = 0;
    g_n = 0;
    g_nmodels = 0;
    g_nflats = 0;
    for (i = 0; i < TEX_SLOTS; i++) {
        g_live[i] = 0;
        g_sharp_live[i] = 0;
        g_want[i] = 0;
        g_small[i] = 1;
        g_bytes[i] = 0;
        g_img[i] = 0;
    }
    if (!level || !level->palette) {
        g_why = "texture upload failed";
        return 0;
    }
    pal = level->palette;
    g_pr = pal->r;
    g_pg = pal->g;
    g_pb = pal->b;
    if (!glide_palette(pal->r, pal->g, pal->b)) {
        g_why = glide_error();
        return 0;
    }
    n = level->nimages;
    if (n < 1 || n > TEX_SLOTS) {
        g_why = "texture count";
        return 0;
    }
    /*
     * Every record goes up at 16 by 16. A city of full squares does not
     * fit in 2MB. The real square stays in the image and is sent later
     * for whatever is near the camera.
     */
    for (i = 0; i < n; i++) {
        const Image *img;
        int cutout;
        int side;
        const df_u8 *pixels;

        img = level->images[i];
        if (!img || !img->pot || img->pot_w < 1 || img->pot_h < 1) {
            continue;
        }
        cutout = level->cutout && level->cutout[i] ? 1 : 0;
        g_img[i] = img;
        g_cut[i] = cutout;
        side = tiny_side(img->pot_w);
        if (side == img->pot_w && side == img->pot_h) {
            pixels = img->pot;
            g_small[i] = 1;
        } else {
            shrink_pot(img->pot, img->pot_w, img->pot_h, side);
            pixels = g_shrink;
            g_small[i] = 0;
            if (!glide_texture_bytes(img->pot_w, img->pot_h, cutout, &g_bytes[i])) {
                g_bytes[i] = (df_u32)img->pot_w * (df_u32)img->pot_h;
                if (cutout) {
                    g_bytes[i] *= 2u;
                }
            }
        }
        if (!glide_upload(pixels, side, side, cutout,
                          pal->r, pal->g, pal->b, &g_addr[i])) {
            g_why = glide_error();
            return 0;
        }
        g_live[i] = 1;
        if (g_small[i]) {
            g_sharp[i] = g_addr[i];
            g_sharp_live[i] = 1;
        }
    }
    g_n = n;
    return 1;
}

static int take(int slot, df_u32 *used, df_u32 room)
{
    df_u32 next;

    if (slot < 0 || slot >= g_n || !g_live[slot] || g_want[slot]) {
        return 1;
    }
    next = *used + g_bytes[slot];
    if (next < *used || next > room) {
        return 0;
    }
    g_want[slot] = 1;
    *used = next;
    return 1;
}

/* 1 when every new record on the mesh fit. Records that fit are kept. */
static int take_mesh(const Mesh *mesh, df_u32 *used, df_u32 room)
{
    int t;
    int ok;
    df_u8 seen[TEX_SLOTS];

    if (!mesh) {
        return 1;
    }
    ok = 1;
    for (t = 0; t < TEX_SLOTS; t++) {
        seen[t] = 0;
    }
    for (t = 0; t < mesh->ntris; t++) {
        int slot;

        slot = (int)mesh->tris[t].slot;
        if (slot < 0 || slot >= g_n || seen[slot]) {
            continue;
        }
        seen[slot] = 1;
        if (!take(slot, used, room)) {
            ok = 0;
        }
    }
    return ok;
}

static float dist2(float x, float y, float z, const Camera *cam)
{
    float dx;
    float dy;
    float dz;

    dx = x - cam->x;
    dy = y - cam->y;
    dz = z - cam->z;
    return dx * dx + dy * dy + dz * dz;
}

static int in_band(float d2, int band)
{
    float hi;
    float lo;

    hi = (float)((band + 1) * BAND);
    hi = hi * hi;
    if (band == 0) {
        return d2 <= hi;
    }
    lo = (float)(band * BAND);
    lo = lo * lo;
    return d2 > lo && d2 <= hi;
}

static void choose_near(const Camera *cam, const Level *level, df_u32 room)
{
    int band;
    int i;
    int nmodels;
    int nflats;
    df_u32 used;
    int room_left;

    nmodels = level->scene.nmodels;
    nflats = level->scene.nsprites;
    if (nmodels > NEAR_MAX) {
        nmodels = NEAR_MAX;
    }
    if (nflats > NEAR_MAX) {
        nflats = NEAR_MAX;
    }
    g_nmodels = nmodels;
    g_nflats = nflats;
    for (i = 0; i < nmodels; i++) {
        g_model_near[i] = 0;
    }
    for (i = 0; i < nflats; i++) {
        g_flat_near[i] = 0;
    }
    for (i = 0; i < g_n; i++) {
        g_want[i] = 0;
    }
    used = 0;
    room_left = 1;
    for (band = 0; band < BANDS && room_left; band++) {
        for (i = 0; i < nmodels && room_left; i++) {
            const Instance *in;
            float d2;

            in = &level->scene.models[i];
            d2 = dist2(in->x, in->y, in->z, cam);
            if (!in_band(d2, band)) {
                continue;
            }
            if (!take_mesh(in->mesh, &used, room)) {
                room_left = 0;
            }
            g_model_near[i] = 1;
        }
        for (i = 0; i < nflats && room_left; i++) {
            const Flat *fl;
            float d2;

            fl = &level->scene.sprites[i];
            d2 = dist2(fl->x, fl->y, fl->z, cam);
            if (!in_band(d2, band)) {
                continue;
            }
            if (!take((int)fl->slot, &used, room)) {
                room_left = 0;
            }
            g_flat_near[i] = 1;
        }
    }
}

static void upload_near(void)
{
    int i;

    glide_rewind();
    g_bound_slot = -1;
    for (i = 0; i < g_n; i++) {
        const Image *img;

        if (g_small[i]) {
            g_sharp[i] = g_addr[i];
            g_sharp_live[i] = g_live[i];
            continue;
        }
        g_sharp_live[i] = 0;
        if (!g_want[i] || !g_live[i]) {
            continue;
        }
        img = g_img[i];
        if (!img) {
            continue;
        }
        if (!glide_upload(img->pot, img->pot_w, img->pot_h, g_cut[i],
                          g_pr, g_pg, g_pb, &g_sharp[i])) {
            break;
        }
        g_sharp_live[i] = 1;
    }
}

static int everything_fits(const Level *level, df_u32 room)
{
    df_u32 sum;
    int i;
    int nmodels;
    int nflats;

    sum = 0;
    for (i = 0; i < g_n; i++) {
        df_u32 next;

        next = sum + g_bytes[i];
        if (next < sum) {
            return 0;
        }
        sum = next;
    }
    if (sum > room) {
        return 0;
    }
    nmodels = level->scene.nmodels;
    nflats = level->scene.nsprites;
    if (nmodels > NEAR_MAX) {
        nmodels = NEAR_MAX;
    }
    if (nflats > NEAR_MAX) {
        nflats = NEAR_MAX;
    }
    g_nmodels = nmodels;
    g_nflats = nflats;
    for (i = 0; i < nmodels; i++) {
        g_model_near[i] = 1;
    }
    for (i = 0; i < nflats; i++) {
        g_flat_near[i] = 1;
    }
    for (i = 0; i < g_n; i++) {
        g_want[i] = g_live[i];
    }
    return 1;
}

void texcache_detail(const Camera *cam, const Level *level)
{
    int cx;
    int cy;
    int cz;
    df_u32 room;

    if (!cam || !level || g_n < 1) {
        return;
    }
    if (g_all && g_cell_ok) {
        return;
    }
    cx = (int)(cam->x / (float)BAND);
    cy = (int)(cam->y / (float)BAND);
    cz = (int)(cam->z / (float)BAND);
    if (g_cell_ok && cx == g_cx && cy == g_cy && cz == g_cz) {
        return;
    }
    room = glide_detail_room();
    if (everything_fits(level, room)) {
        g_all = 1;
    } else {
        choose_near(cam, level, room);
    }
    upload_near();
    g_cx = cx;
    g_cy = cy;
    g_cz = cz;
    g_cell_ok = 1;
}

int texcache_near_model(int index)
{
    if (index < 0 || index >= g_nmodels) {
        return 0;
    }
    return g_model_near[index] ? 1 : 0;
}

int texcache_near_flat(int index)
{
    if (index < 0 || index >= g_nflats) {
        return 0;
    }
    return g_flat_near[index] ? 1 : 0;
}

void texcache_bind(int slot, int sharp)
{
    df_u32 addr;

    if (slot < 0 || slot >= g_n || !g_live[slot]) {
        return;
    }
    if (sharp && g_sharp_live[slot]) {
        addr = g_sharp[slot];
    } else {
        addr = g_addr[slot];
        sharp = 0;
    }
    if (slot == g_bound_slot && sharp == g_bound_sharp) {
        return;
    }
    glide_bind(addr);
    g_bound_slot = slot;
    g_bound_sharp = sharp;
}

void texcache_forget(void)
{
    g_bound_slot = -1;
}
