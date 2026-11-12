#ifndef GFALL_PLAY_H
#define GFALL_PLAY_H

#include "game/dagger/play/steer.h"

#include "engine/gfx/camera.h"
#include "engine/scene/scene.h"

/*
 * Which screen has the input. The world runs only while the mode is
 * the dungeon and options are closed. The app calls gfx_set_pause
 * from play_paused, gfx_set_extra to play_draw, hud_set(0) and
 * gfx_set_use_key(0). Those live in the app because the host build
 * does not link gfx.c.
 *
 * quests_start_initial is not run. See chargen.h.
 */

enum {
    PIC_TITLE,
    PIC_BACK,
    PIC_MAP,
    PIC_BUTTONS,
    PIC_BAR,
    PIC_MODES,
    PIC_HEALTH,
    PIC_FATIGUE,
    PIC_MAGICKA,
    PIC_OVERLAY, PIC_OVERLAY_1, PIC_OVERLAY_2, PIC_OVERLAY_3,
    PIC_COMPASS,
    PIC_COMPASS_WIDE,
    PIC_COMPBOX,
    PIC_ARROW,
    PIC_OPTIONS,
    PIC_MOUSE,
    PIC_FACE,
    PIC_CURSOR0, PIC_CURSOR1, PIC_CURSOR2,
    PIC_CURSOR3, PIC_CURSOR4, PIC_CURSOR5,
    PIC_CURSOR6, PIC_CURSOR7, PIC_CURSOR8,
    /* BUTTONS.RCI. Race confirm is Yes/No. Gender is Male/Female. */
    PIC_YES,
    PIC_NO,
    PIC_MALE,
    PIC_FEMALE,
    /* The chargen screen composed for this frame, 320 x 200. 0 on the title and the map. */
    PIC_SCREEN,
    PIC_BREATH, PIC_BREATH_LOW,
    PIC_COUNT
};

enum {
    FRAME_MENU,
    FRAME_WORLD,
    FRAME_ENTER,
    FRAME_LOAD,
    FRAME_TRAVEL,
    FRAME_TITLE,
    FRAME_QUIT
};

typedef struct PlayIn {
    int cx;
    int cy;
    int on_picture;
    int mouse_held;
    int mouse_pressed;
    int mouse_released;
    int wheel;
    int reset;
    unsigned char held[ACTS];
    unsigned char pressed[ACTS];
    int ch;
    int key;
    int toggle_fullscreen;
    int mouse_dx;
    int mouse_dy;
    /* The province map's pixel under the cursor. Other screens ignore it. */
    int pixel;
    float dt;
} PlayIn;

void gfall_play_startup(void);
void gfall_play_shutdown(void);

void set_sound(void (*fn)(int id));
void play_sound(int id);
void set_sound_at(void (*fn)(int id, float x, float y, float z));
void sound_at(int id, float x, float y, float z);
int play_dead(void);
void set_save_capture(int (*fn)(void));
/* -1 means the ordinary arrow; 0..8 select PNTER.CIF. */
int pointer_region(void);
/* TEXT.RSC into dst. The confirm draws record ids through this. */
void set_words(int (*fn)(int id, char *dst, int cap));
void set_word_variants(int (*fn)(int id, int variant, char *dst, int cap));
/* The record, or empty when it did not read. Kept until id/variant changes. */
const char *words_state(int id);
const char *words_variant(int id, int variant);
void pic_set(int slot, int id);
int play_pic(int slot);
/* One compass frame. CMPA00I0 is 32 frames and too tall for one picture. */
void preview_pic_set(int slot, int pic);
void compass_set(int frame, int pic, int w, int h);

int play_frame(const PlayIn *in, Camera *cam, const Scene *scene);
int play_paused(void);
/* 1 while the game draws the arrow and the engine must not look. */
int wants_pointer(void);
int cursor_free(void);
const char *status_line(void);
/* The Status key's first box (status_show: TEXT.RSC 22 with the town, time, date, region and the law's opinion said): the game fills dst. */
void set_status_intro(int (*fn)(char *dst, int cap));
int status_intro(char *dst, int cap);
void play_draw(void);
/* The engine pointer, lower-left. Drawn when the game owns the arrow. */
void point_at(int x, int y);

#endif
