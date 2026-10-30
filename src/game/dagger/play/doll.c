#include "game/dagger/play/doll.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/tables/items.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See doll.h. */

/* The corner of the doll on classic's screen: every picture's place has this taken off. */
#define ORIGIN_X 200
#define ORIGIN_Y 8
/* Where the doll starts inside the background picture (DFU's PaperDoll). */
#define BACKGROUND_X 8
#define BACKGROUND_Y 7
/* The files a screen can read in one sitting: an armour archive, weapons, a few on the ground. */
#define READ_FILES 8

static int g_read_file[READ_FILES];
static int g_read_files;

Image *doll_read_picture(int archive, int record)
{
    if (!texture_cached(archive) && g_read_files < READ_FILES) {
        g_read_file[g_read_files++] = archive;
    }
    return texture_load(archive, record);
}

void doll_forget_files(void)
{
    int i;

    for (i = 0; i < g_read_files; i++) {
        texture_forget(g_read_file[i]);
    }
    g_read_files = 0;
}

/* What is being drawn on: the doll's pixels, who drew each, and the background a mask shows again. */
typedef struct Canvas {
    df_u8 *pixels;
    df_u8 *owner;
    ImgPicture background;
} Canvas;

/* The background's pixel at a place on the doll. */
static int background_at(const Canvas *c, int x, int y)
{
    return c->background.pixels[(y + BACKGROUND_Y) * c->background.width + x + BACKGROUND_X];
}

/*
 * A picture's pixels onto the doll at left, top, index 0 clear. metal, when
 * not negative, recolours the picture's metal (cif_metal). owner_id, when
 * not DOLL_NO_OWNER, is a worn thing: its mask pixels cut down to the
 * background, and each pixel it draws is marked as its own.
 */
static void lay(Canvas *c, const df_u8 *src, int w, int h, int left, int top, int metal, int owner_id)
{
    int row;

    for (row = 0; row < h; row++) {
        int y;
        int col;

        y = top + row;
        if (y < 0 || y >= DOLL_H) {
            continue;
        }
        for (col = 0; col < w; col++) {
            int x;
            int index;
            int at;

            x = left + col;
            index = src[row * w + col];
            if (x < 0 || x >= DOLL_W || index == 0) {
                continue;
            }
            at = y * DOLL_W + x;
            if (owner_id != DOLL_NO_OWNER && index == DOLL_MASK) {
                c->pixels[at] = (df_u8)background_at(c, x, y);
                c->owner[at] = DOLL_NO_OWNER;
                continue;
            }
            c->pixels[at] = (df_u8)(metal >= 0 ? cif_metal(index, metal) : index);
            if (owner_id != DOLL_NO_OWNER) {
                c->owner[at] = (df_u8)owner_id;
            }
        }
    }
}

/* An IMG laid at the place its header gives. 0 when it did not read. */
static int lay_img(Canvas *c, const char *name)
{
    ImgPicture pic;

    if (!img_load(name, &pic)) {
        return 0;
    }
    lay(c, pic.pixels, pic.width, pic.height, pic.x - ORIGIN_X, pic.y - ORIGIN_Y, -1, DOLL_NO_OWNER);
    return 1;
}

/* The background, cropped to the doll: every pixel its own, nothing clear. */
static int lay_background(Canvas *c, const char *name)
{
    int row;

    if (!img_load(name, &c->background) || c->background.width < BACKGROUND_X + DOLL_W
        || c->background.height < BACKGROUND_Y + DOLL_H) {
        return 0;
    }
    for (row = 0; row < DOLL_H; row++) {
        mem_copy(c->pixels + row * DOLL_W,
                 c->background.pixels + (row + BACKGROUND_Y) * c->background.width + BACKGROUND_X, DOLL_W);
    }
    return 1;
}

/* The head: the character's face, a frame of the race's face file. */
static void lay_head(Canvas *c, const char *name, int which)
{
    CifSet faces;
    const CifFrame *face;

    if (!cif_open(name, &faces)) {
        return;
    }
    face = cif_frame(&faces, which);
    if (face) {
        lay(c, face->pixels, face->w, face->h, face->x - ORIGIN_X, face->y - ORIGIN_Y, -1, DOLL_NO_OWNER);
    }
    cif_close(&faces);
}

/* One worn thing, at the place its record says. */
static void lay_worn(Canvas *c, const Character *who, const Item *it, int place)
{
    const Image *picture;
    int archive;
    int record;
    int x;
    int y;

    if (!item_picture(it, who->race, who->female, &archive, &record)) {
        return;
    }
    picture = doll_read_picture(archive, record);
    if (!picture || !texture_offset(archive, record, &x, &y)) {
        log_print(LOG_WARN, "doll: no picture %d of TEXTURE.%d", record, archive);
        return;
    }
    lay(c, picture->index, picture->width, picture->height, x - ORIGIN_X, y - ORIGIN_Y, item_metal(it), place);
}

/* The worn places, lowest draw order first. Ties keep their place's order. */
static int places_in_order(const Inventory *pack, int *order)
{
    int n;
    int place;

    n = 0;
    for (place = 0; place < EQUIP_COUNT; place++) {
        const ItemTemplate *t;
        int at;

        if (pack->worn[place] < 0) {
            continue;
        }
        t = item_template(pack->item[pack->worn[place]].template_id);
        at = n++;
        while (at > 0) {
            const ItemTemplate *before;

            before = item_template(pack->item[pack->worn[order[at - 1]]].template_id);
            if (!t || !before || before->draw_order <= t->draw_order) {
                break;
            }
            order[at] = order[at - 1];
            at--;
        }
        order[at] = place;
    }
    return n;
}

int doll_compose(df_u8 *pixels, df_u8 *owner, const Character *c, const Inventory *pack)
{
    const CareerRace *race;
    Canvas canvas;
    int order[EQUIP_COUNT];
    int worn;
    int i;

    race = career_race(c->race);
    canvas.pixels = pixels;
    canvas.owner = owner;
    if (!race || !lay_background(&canvas, race->background)) {
        return 0;
    }
    for (i = 0; i < DOLL_W * DOLL_H; i++) {
        owner[i] = DOLL_NO_OWNER;
    }
    if (!lay_img(&canvas, c->female ? race->body_female : race->body_male)) {
        return 0;
    }
    /* Classic shows the body in its underclothes: they always go on over it. */
    lay_img(&canvas, c->female ? race->body_female_clothed : race->body_male_clothed);
    lay_head(&canvas, c->female ? race->heads_female : race->heads_male, c->face);
    worn = places_in_order(pack, order);
    for (i = 0; i < worn; i++) {
        lay_worn(&canvas, c, &pack->item[pack->worn[order[i]]], order[i]);
    }
    return 1;
}
