#include "game/dagger/play/combat/weapon.h"
#include "game/dagger/play/ui/screen.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/ui/options.h"
#include "../seam.h"
#include "../sound.h"
#include "screen.h"
#include "callbacks.h"

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"

#include "game/dagger/formats/cif.h"
#include "game/dagger/play/input/move.h"

#include "internal.h"
#include "game/dagger/play/ui/canvas.h"

static CifSet g_weapon_art;
static int g_weapon_file = -1, g_weapon_pic, g_weapon_frame = -1;
static int g_weapon_color = -1, g_weapon_hand = -1, g_weapon_bar = -1;
static int g_hurt_pic;
static df_u8 *g_weapon_pixels;
static CfaPicture g_mount_art[2];
static int g_mount_pic, g_mount_kind, g_mount_frame;

void gfall_screen_draw_mount(void)
{
    static const char *const names[] = {"MRED00I0.CFA", "MRED01I0.CFA"};
    CfaPicture *art;
    const df_u8 *pixels;
    int kind = move_mounted(), frame;

    if (!kind) { return; }
    art = &g_mount_art[kind - 1];
    if (!art->pixels && !cfa_load(names[kind - 1], art)) { return; }
    frame = move_mount_speed() > 2 ? (int)((game_state()->minutes * 45u + (game_state()->ticks >> 1)) & 3u) : 0;
    pixels = art->pixels + (frame % art->frames) * art->width * art->height;
    if (kind != g_mount_kind) {
        if (g_mount_pic) { pic_free(g_mount_pic); g_mount_pic = 0; }
        g_mount_kind = kind;
    }
    if (!g_mount_pic) { g_mount_pic = pic_load(pixels, art->width, art->height, gfall_screen_art_palette(), 1); }
    else if (frame != g_mount_frame) { pic_update(g_mount_pic, pixels); }
    g_mount_frame = frame;
    hud_blit(g_mount_pic, art->x, art->y + ((options_flags() & 1) ? 46 : 0), art->width, art->height);
}

const df_u8 *seam_weapon_screen(const df_u8 **palette)
{
    if (palette) { *palette = gfall_screen_art_palette(); }
    return g_weapon_pixels;
}

static CifFrame g_cast_art[5][6];
static df_u8 *g_cast_pixels;
static int g_cast_pic, g_cast_element = -1, g_cast_frame = -1, g_cast_bar;

int gfall_screen_load_overlays(void)
{
    static const char *names[5] = {"FIRE00C6.CIF","FRST00C6.CIF","POIS00C6.CIF","SHOK00C6.CIF","MJIC00C6.CIF"};
    const df_u8 *bytes;
    df_u8 *pixels;
    df_u32 size, at;
    CifFrame *f;
    int element, frame, length;
    for (element = 0; element < 5; element++) {
        bytes = vfs_read(names[element],&size); at = 0;
        if (!bytes) { return 0; }
        for (frame = 0; frame < 6; frame++) {
            if (at+12 > size) { return 0; }
            f = &g_cast_art[element][frame];
            f->x = df_ri16(bytes+at); f->y = df_ri16(bytes+at+2);
            f->w = df_ru16(bytes+at+4); f->h = df_ru16(bytes+at+6); length = df_ru16(bytes+at+10);
            if (f->w < 1 || f->w > 320 || f->h < 1 || f->h > 200 || at+12+length > size) { return 0; }
            pixels = (df_u8 *)mem_keep(f->w*f->h);
            if (!pixels || !cif_rle(bytes+at+12,length,pixels,f->w*f->h)) { return 0; }
            f->pixels = pixels; at += 12+length;
        }
    }
    return 1;
}

const df_u8 *seam_cast_screen(const df_u8 **palette)
{
    if (palette) { *palette = gfall_screen_art_palette(); }
    return g_cast_pixels;
}

void gfall_cast_draw(int element, int frame)
{
    const CifFrame *f;
    int x, y, px, py, bar;
    df_u8 pixel;
    if (element < 0 || element >= 5 || frame < 0 || frame >= 6) { return; }
    if (!g_cast_pixels) { g_cast_pixels = (df_u8 *)mem_keep(CANVAS_BYTES); }
    if (!g_cast_pixels) { return; }
    bar = !(options_flags() & 1); f = &g_cast_art[element][frame];
    if (!g_cast_pic || element != g_cast_element || frame != g_cast_frame || bar != g_cast_bar) {
        mem_zero(g_cast_pixels,CANVAS_BYTES);
        for (y = 0; y < f->h; y++) {
            py = f->y+y-(bar ? 46 : 0); if (py < 0 || py >= 200) { continue; }
            for (x = 0; x < f->w; x++) {
                px = f->x+x; if (px < 0 || px >= 320) { continue; }
                pixel = f->pixels[y*f->w+x];
                g_cast_pixels[py*320+px] = pixel; g_cast_pixels[py*320+319-px] = pixel;
            }
        }
        if (g_weapon_pic) { pic_free(g_weapon_pic); g_weapon_pic = 0; g_weapon_frame = -1; }
        if (!g_cast_pic) { g_cast_pic = pic_load(g_cast_pixels,CANVAS_W,CANVAS_H,gfall_screen_art_palette(),1); }
        else { pic_update(g_cast_pic,g_cast_pixels); }
        g_cast_element = element; g_cast_frame = frame; g_cast_bar = bar;
    }
    hud_blit(g_cast_pic,0,0,CANVAS_W,CANVAS_H);
}

void gfall_weapon_draw(const WeaponView *view, float flash)
{
    const CifFrame *frame;
    df_u8 red[768], pixel;
    char name[20];
    int first, count, at, x, y, px, py, bar;
    if (view->visible) {
        if (g_cast_pic) { pic_free(g_cast_pic); g_cast_pic = 0; }
        if (g_weapon_file != view->cif + (view->enchanted ? 100 : 0)) {
            cif_close(&g_weapon_art);
            /* An enchanted weapon (not a bow) is drawn from WEAPO1##.CIF; the plain picture when there is none. */
            g_weapon_file = -1;
            if (view->enchanted) {
                log_format(name,sizeof(name),"WEAPO1%02d.CIF",view->cif);
                if (cif_open(name,&g_weapon_art)) { g_weapon_file = view->cif + 100; }
            }
            if (g_weapon_file < 0) {
                log_format(name,sizeof(name),"WEAPON%02d.CIF",view->cif);
                g_weapon_file = cif_open(name,&g_weapon_art) ? view->cif + (view->enchanted ? 100 : 0) : -1;
            }
            g_weapon_frame = -1;
        }
        count = cif_group_span(&g_weapon_art,view->cif == 9 ? 1 : view->group,&first);
        at = count ? first + (view->frame < count ? view->frame : count-1) : -1;
        frame = cif_frame(&g_weapon_art,at);
        bar = !(options_flags() & 1);
        if (!g_weapon_pixels) { g_weapon_pixels = (df_u8 *)mem_keep(CANVAS_BYTES); }
        if (frame && g_weapon_pixels && (at != g_weapon_frame || view->color != g_weapon_color ||
            view->hand != g_weapon_hand || bar != g_weapon_bar || !g_weapon_pic)) {
            mem_zero(g_weapon_pixels,CANVAS_BYTES);
            for (y = 0; y < frame->h; y++) {
                py = frame->y+y-(bar ? 46 : 0);
                if (py < 0 || py >= 200) { continue; }
                for (x = 0; x < frame->w; x++) {
                    px = frame->x+x;
                    if (view->hand) { px = 319-px; }
                    if (px < 0 || px >= 320) { continue; }
                    pixel = frame->pixels[y*frame->w+x];
                    g_weapon_pixels[py*320+px] = view->color ? (df_u8)item_ink(view->color,pixel) : pixel;
                }
            }
            if (!g_weapon_pic) { g_weapon_pic = pic_load(g_weapon_pixels,CANVAS_W,CANVAS_H,gfall_screen_art_palette(),1); }
            else { pic_update(g_weapon_pic,g_weapon_pixels); }
            g_weapon_frame = at; g_weapon_color = view->color; g_weapon_hand = view->hand; g_weapon_bar = bar;
        }
        if (frame) { hud_blit(g_weapon_pic,0,0,CANVAS_W,CANVAS_H); }
    }
    if (flash > 0) {
        if (!g_hurt_pic) {
            mem_zero(red,sizeof(red)); red[3] = 192; pixel = 1;
            g_hurt_pic = pic_load(&pixel,1,1,red,0);
        }
        pic_draw_part(g_hurt_pic,0,0,1,1,0,df_view_h(),df_view_w(),df_view_h(),(int)(flash*120));
    }
}

void gfall_screen_leave_world(void)
{
    cif_close(&g_weapon_art); g_weapon_file = -1;
    if (g_weapon_pic) { pic_free(g_weapon_pic); g_weapon_pic = 0; }
    g_weapon_frame = -1;
    if (g_cast_pic) { pic_free(g_cast_pic); g_cast_pic = 0; }
    if (g_mount_pic) { pic_free(g_mount_pic); g_mount_pic = 0; }
    mem_zero(g_mount_art, sizeof(g_mount_art));
}
