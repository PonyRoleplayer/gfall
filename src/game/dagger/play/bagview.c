#include "game/dagger/play/bagview.h"
#include "game/dagger/play/menu.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/formats/img.h"
#include "engine/ui/font.h"
#include "engine/ui/pic.h"

/* See bagview.h. */

static int g_open;
/* The composed screen on the card; ART_PAL.COL, the palette all its pieces are in. */
static int g_pic;
static df_u8 g_rgb[768];
static int g_point_x;
static int g_point_y;

/* The composed screen up on the card, in place of the last one. */
static void send_screen(void)
{
    if (g_pic) {
        pic_free(g_pic);
    }
    g_pic = pic_load(bag_pixels(), SCREEN_W, SCREEN_H, g_rgb, 0);
}

void bagview_open(Player *p, const Entity *body)
{
    if (g_open || !img_art_palette(g_rgb) || !bag_open(p, body)) {
        return;
    }
    g_open = 1;
    bag_compose();
    send_screen();
}

void bagview_close(void)
{
    if (!g_open) {
        return;
    }
    g_open = 0;
    if (g_pic) {
        pic_free(g_pic);
        g_pic = 0;
    }
    bag_close();
}

int bagview_is_open(void)
{
    return g_open;
}

int bagview_frame(const Input *in)
{
    int px;
    int py;
    int leave;

    input_pointer(&px, &py);
    screen_from_pointer(px, py, &g_point_x, &g_point_y);
    bag_point(g_point_x, g_point_y);
    leave = (in->pressed & (INPUT_BIT(INPUT_MENU) | INPUT_BIT(INPUT_INVENTORY))) != 0;
    if ((in->pressed & INPUT_BIT(INPUT_ATTACK)) && bag_click(g_point_x, g_point_y) == BAG_LEAVE) {
        leave = 1;
    }
    if (!leave && bag_changed()) {
        bag_compose();
        send_screen();
    }
    return leave;
}

void bagview_draw(void)
{
    const char *note;

    screen_draw(g_pic, 0, 0, SCREEN_W, SCREEN_H);
    note = bag_note();
    if (note) {
        ui_string((DF_VIEW_W - ui_string_width(note)) / 2, ui_line_height() + 8, note);
    }
    menu_draw_pointer(g_point_x, g_point_y);
}
