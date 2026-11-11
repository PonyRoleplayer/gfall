#ifndef OGFALL_PLAY_H
#define OGFALL_PLAY_H

#include "game/ogfall/play/steer.h"

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * Which screen has the input. The world runs only while the mode is
 * the dungeon and options are closed. The app calls gfx_set_pause
 * from og_paused, gfx_set_extra to og_draw, hud_set(0) and
 * gfx_set_use_key(0). Those live in the app because the host build
 * does not link gfx.c.
 *
 * quests_start_initial is not run. See chargen.h.
 */

enum {
    OG_PIC_TITLE,
    OG_PIC_BACK,
    OG_PIC_MAP,
    OG_PIC_BUTTONS,
    OG_PIC_BAR,
    OG_PIC_MODES,
    OG_PIC_HEALTH,
    OG_PIC_FATIGUE,
    OG_PIC_MAGICKA,
    OG_PIC_OVERLAY, OG_PIC_OVERLAY_1, OG_PIC_OVERLAY_2, OG_PIC_OVERLAY_3,
    OG_PIC_COMPASS,
    OG_PIC_COMPASS_WIDE,
    OG_PIC_COMPBOX,
    OG_PIC_ARROW,
    OG_PIC_OPTIONS,
    OG_PIC_MOUSE,
    OG_PIC_FACE,
    OG_PIC_CURSOR0, OG_PIC_CURSOR1, OG_PIC_CURSOR2,
    OG_PIC_CURSOR3, OG_PIC_CURSOR4, OG_PIC_CURSOR5,
    OG_PIC_CURSOR6, OG_PIC_CURSOR7, OG_PIC_CURSOR8,
    /* BUTTONS.RCI. Race confirm is Yes/No. Gender is Male/Female. */
    OG_PIC_YES,
    OG_PIC_NO,
    OG_PIC_MALE,
    OG_PIC_FEMALE,
    /* The chargen screen composed for this frame, 320 x 200. 0 on the title and the map. */
    OG_PIC_SCREEN,
    OG_PIC_BREATH, OG_PIC_BREATH_LOW,
    OG_PIC_COUNT
};

enum {
    OG_FRAME_MENU,
    OG_FRAME_WORLD,
    OG_FRAME_ENTER,
    OG_FRAME_LOAD,
    OG_FRAME_TRAVEL,
    OG_FRAME_TITLE,
    OG_FRAME_QUIT
};

typedef struct OgIn {
    int cx;
    int cy;
    int on_picture;
    int mouse_held;
    int mouse_pressed;
    int mouse_released;
    int wheel;
    int reset;
    unsigned char held[OG_ACTS];
    unsigned char pressed[OG_ACTS];
    int ch;
    int key;
    int toggle_fullscreen;
    int mouse_dx;
    int mouse_dy;
    /* The province map's pixel under the cursor. Other screens ignore it. */
    int pixel;
    float dt;
} OgIn;

void ogfall_play_startup(void);
void ogfall_play_shutdown(void);

void og_set_sound(void (*fn)(int id));
void og_sound(int id);
void og_set_sound_at(void (*fn)(int id, float x, float y, float z));
void og_sound_at(int id, float x, float y, float z);
int og_dead(void);
void og_set_save_capture(int (*fn)(void));
/* -1 means the ordinary arrow; 0..8 select PNTER.CIF. */
int og_pointer_region(void);
/* TEXT.RSC into dst. The confirm draws record ids through this. */
void og_set_words(int (*fn)(int id, char *dst, int cap));
void og_set_word_variants(int (*fn)(int id, int variant, char *dst, int cap));
/* The record, or empty when it did not read. Kept until id/variant changes. */
const char *og_words(int id);
const char *og_words_variant(int id, int variant);
void og_pic_set(int slot, int id);
int og_pic(int slot);
/* One compass frame. CMPA00I0 is 32 frames and too tall for one picture. */
void og_preview_pic_set(int slot, int pic);
void og_compass_set(int frame, int pic, int w, int h);

int og_frame(const OgIn *in, Camera *cam, const Scene *scene);
int og_paused(void);
/* 1 while the game draws the arrow and the engine must not look. */
int og_wants_pointer(void);
int og_cursor_free(void);
const char *og_status_line(void);
/* The Status key's first box (status_show: TEXT.RSC 22 with the town, time, date, region and the law's opinion said): the game fills dst. */
void og_set_status_intro(int (*fn)(char *dst, int cap));
int og_status_intro(char *dst, int cap);
void og_draw(void);
/* The engine pointer, lower-left. Drawn when the game owns the arrow. */
void og_point_at(int x, int y);

#endif
