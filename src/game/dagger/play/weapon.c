#include "game/dagger/play/weapon.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/tables/items.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"

/* See weapon.h. */

/* Classic's swings, groups 1..6 of a weapon file. */
#define SWINGS 6
/* A frame no larger than classic's screen. */
#define FRAME_MAX (320 * 200)
/* A frame shows at least this long, whatever the Speed. */
#define FRAME_TIME_MIN 0.02f

/* A picture on the card: its id, and the part of its frame it holds (cif_bounds). */
typedef struct Shown {
    int pic;
    int x;
    int y;
    int w;
    int h;
} Shown;

static CifSet g_cif;
/* The file and material held; -1 when none is. */
static int g_cif_no = -1;
static int g_metal = -1;
static Shown g_idle;
/* The swing frame on the card, and its index in g_cif. */
static Shown g_frame;
static int g_frame_at = -1;
/* The swing running (0: at rest), its first frame, its count, the frame showing, the time on it. */
static int g_swing;
static int g_first;
static int g_count;
static int g_step;
static float g_time;
static df_u8 g_rgb[768];
static int g_have_rgb;
static df_u8 *g_buf;
/* The random swings: a seed stirred by when each click comes. */
static df_u32 g_seed = 0x5EED1u;
static float g_clock;

/*
 * Frame index of g_cif cropped to its drawn pixels (cif_bounds: a swing
 * frame fills about 30% of the box it is stored in), recoloured to the
 * material, and sent as a cutout. s->pic is 0 when nothing was sent.
 */
static void load_frame(int index, Shown *s)
{
    const CifFrame *f;
    int row;
    int col;

    mem_zero(s, sizeof(*s));
    f = cif_frame(&g_cif, index);
    if (!f || f->w * f->h > FRAME_MAX || !cif_bounds(f, &s->x, &s->y, &s->w, &s->h)) {
        return;
    }
    if (!g_buf) {
        g_buf = (df_u8 *)mem_keep(FRAME_MAX);
    }
    if (!g_buf || !g_have_rgb) {
        return;
    }
    for (row = 0; row < s->h; row++) {
        const df_u8 *src;
        df_u8 *dst;

        src = f->pixels + (s->y + row) * f->w + s->x;
        dst = g_buf + row * s->w;
        for (col = 0; col < s->w; col++) {
            dst[col] = (df_u8)cif_metal(src[col], g_metal);
        }
    }
    /* Index 0 is clear, as round every sprite. */
    s->pic = pic_load(g_buf, s->w, s->h, g_rgb, 1);
}

static void drop(Shown *s)
{
    if (s->pic) {
        pic_free(s->pic);
    }
    s->pic = 0;
}

static void drop_frame(void)
{
    drop(&g_frame);
    g_frame_at = -1;
}

static void show_frame(int index)
{
    if (index == g_frame_at) {
        return;
    }
    drop_frame();
    load_frame(index, &g_frame);
    g_frame_at = index;
}

void weapon_free(void)
{
    drop_frame();
    drop(&g_idle);
    cif_close(&g_cif);
    g_cif_no = -1;
    g_metal = -1;
    g_swing = 0;
}

void weapon_hold(const Item *it)
{
    const WeaponTemplate *w;
    char name[16];
    int cif;
    int metal;
    int first;

    w = it ? weapon_template(it->template_id) : 0;
    cif = w ? w->cif : WEAPON_FISTS_CIF;
    metal = w ? it->material : MATERIAL_IRON;
    if (cif == g_cif_no && metal == g_metal) {
        return;
    }
    weapon_free();
    if (!g_have_rgb) {
        g_have_rgb = img_art_palette(g_rgb);
    }
    log_format(name, (int)sizeof(name), "WEAPON%02d.CIF", cif);
    if (!cif_open(name, &g_cif)) {
        log_print(LOG_WARN, "weapon: no %s", name);
        return;
    }
    g_cif_no = cif;
    g_metal = metal;
    /* The armed picture, group 0; a bow has none and shows only while it shoots. */
    if (cif_group_span(&g_cif, 0, &first)) {
        load_frame(first, &g_idle);
    }
}

int weapon_attack(void)
{
    int swings;
    int pick;

    if (g_cif_no < 0 || g_swing) {
        return 0;
    }
    swings = g_cif.groups - 1;
    if (swings > SWINGS) {
        swings = SWINGS;
    }
    if (swings < 1) {
        return 0;
    }
    g_seed = g_seed * 1103515245u + 12345u + (df_u32)df_ifloor(g_clock * 1000.0f);
    pick = 1 + (int)((g_seed >> 16) % (df_u32)swings);
    g_count = cif_group_span(&g_cif, pick, &g_first);
    if (g_count < 1) {
        return 0;
    }
    g_swing = pick;
    g_step = 0;
    g_time = 0.0f;
    show_frame(g_first);
    return pick;
}

void weapon_step(float dt, int speed)
{
    float each;

    g_clock += dt;
    if (!g_swing) {
        return;
    }
    each = rules_swing_frame_time(speed);
    if (each < FRAME_TIME_MIN) {
        each = FRAME_TIME_MIN;
    }
    g_time += dt;
    while (g_time >= each) {
        g_time -= each;
        g_step++;
        if (g_step >= g_count) {
            /* No frame held at the end: back to rest (DFU's FPSWeapon). */
            g_swing = 0;
            drop_frame();
            return;
        }
    }
    show_frame(g_first + g_step);
}

int weapon_swinging(void)
{
    return g_swing != 0;
}

int weapon_shown(int *pic, int *left, int *top, int *w, int *h)
{
    const CifFrame *f;
    const Shown *s;
    int at;

    /* The frame's own place on classic's screen, moved by where its crop starts. */
    if (g_swing && g_frame.pic) {
        s = &g_frame;
        at = g_frame_at;
    } else if (!g_swing && g_idle.pic) {
        s = &g_idle;
        cif_group_span(&g_cif, 0, &at);
    } else {
        return 0;
    }
    f = cif_frame(&g_cif, at);
    if (!f) {
        return 0;
    }
    *pic = s->pic;
    *left = f->x + s->x;
    *top = f->y + s->y;
    *w = s->w;
    *h = s->h;
    return 1;
}
