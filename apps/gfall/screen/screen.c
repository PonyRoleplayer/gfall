#include "game/dagger/play/state.h"
#include "game/dagger/play/ui/movie.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/travel/maps.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/ui/hud.h"
#include "game/dagger/play/ui/options.h"
#include "game/dagger/play/input/controls.h"
#include "game/dagger/play/save/files.h"
#include "game/dagger/play/character/rest.h"
#include "../seam.h"
#include "../sound.h"
#include "screen.h"
#include "callbacks.h"
#include "internal.h"
#include "game/dagger/play/ui/canvas.h"

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/ui/pic.h"

#include "game/dagger/play/ui/hud.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/classcfg.h"
#include "game/dagger/formats/biog.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/smallfont.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/play/travel/travelmap.h"

#include "game/dagger/play/character/chargen.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/play.h"
#include "game/dagger/tables/tables.h"

#ifndef DF_HOST
#include "game/dagger/formats/fnt.h"
#endif

static const df_u8 *g_map;
static int g_map_w;
static int g_map_h;
static const df_u8 *g_land;
static const df_u8 *g_map_draw;
static ImgPicture g_title_art, g_class_art;
static int g_land_w;
static int g_land_h;
static SmallFont g_font;
static CifSet g_overlay;
static const df_u8 *g_spop;
static const df_u8 *g_buttons;
static df_u8 g_map_rgb[768];
static df_u8 g_art_rgb[768];
static df_u8 g_chgn_rgb[768];
static int g_have_map_rgb;
static int g_have_art_rgb;
static int g_have_chgn_rgb;
static ImgPicture g_pick;
static ImgPicture g_butn2;
static ImgPicture g_biog_pic;
static ImgPicture g_chgn;
static ImgPicture g_sheet[6];
static int g_have_pick;
static int g_have_butn2;
static int g_have_biog_pic;
static int g_have_chgn;
static int g_have_sheet[6];
static df_u8 *g_buf;
static int g_comp[3];
static int g_comp_cutout[3];
static unsigned g_revision;
static int g_present_mode = -1;
static int g_screen_kind;
static int g_hud_loaded;
static ImgAnimation g_compass_data;
static int g_compass_pic, g_compass_frame = -1;
static ImgPicture g_options_art, g_controls_art, g_controls_view, g_mouse_art;
static ImgPicture g_files_art, g_save_label_art;
static CifSet g_pointer_art;
static int g_preview[6];
static unsigned g_preview_generation;
static unsigned g_panel_revision;
static int g_last_panel = -1;
static CifSet g_faceset;
static int g_faces_race;
static int g_faces_sex;
static int g_faces_open;
static Biog g_biog;
static int g_biog_class;
static int g_biog_ok;
static ImgPicture g_attr_arrows;
static ImgPicture g_skill_arrows;
static ImgPicture g_reflex;
static ImgPicture g_custom_screen[4];
static ImgPicture g_dagger;
static int g_face_index = -1;
static int g_hud_race = -1, g_hud_sex = -1;
static ImgPicture g_rest_art[3];
static void ensure_biog(int klass);
static void ensure_faces(int race, int female);
static int rgb_of(const ImgPicture *pic, df_u8 *rgb)
{
    if (pic->palette) {
        return img_palette(pic, rgb);
    }
    return img_art_palette(rgb);
}

static int load_img(const char *name, int cutout)
{
    ImgPicture pic;
    df_u8 rgb[768];

    if (!img_load(name, &pic) || !rgb_of(&pic, rgb)) {
        log_print(LOG_WARN, "gfall: %s did not load", name);
        return 0;
    }
    return pic_load(pic.pixels, pic.width, pic.height, rgb, cutout);
}

static int menu_picture(const char *name, ImgPicture *art)
{
    df_u8 rgb[768];
    if (!art->pixels && !img_load(name,art)) { return 0; }
    if (g_have_art_rgb && !art->palette) { return pic_load(art->pixels,art->width,art->height,g_art_rgb,0); }
    if (!rgb_of(art,rgb)) { return 0; }
    return pic_load(art->pixels,art->width,art->height,rgb,0);
}

/* MAP.PAL is 6-bit, the same shift title_menu applies before the map. */
static int map_palette(df_u8 *rgb)
{
    const df_u8 *pal;
    df_u32 n;
    int i;

    pal = vfs_read("MAP.PAL", &n);
    if (!pal || n < 768) {
        return img_art_palette(rgb);
    }
    for (i = 0; i < 768; i++) {
        int v;

        v = (int)pal[i] * 4;
        rgb[i] = (df_u8)(v > 255 ? 255 : v);
    }
    return 1;
}

/*
 * TAMRIEL2.IMG is the click mask. title_menu reads a pixel and never
 * blits it. Index 0 is empty water; 1..8 are the races. Drawn under
 * MAP.PAL those indices are all near black, which is the blank screen.
 */
static void load_click_map(const char *name)
{
    ImgPicture pic;

    if (!img_load(name, &pic)) {
        log_print(LOG_WARN, "gfall: %s did not load", name);
        return;
    }
    g_map = pic.pixels;
    g_map_w = pic.width;
    g_map_h = pic.height;
}

/* title_menu: text_draw_centred_coloured of this line at (160, 16), inks 145 and 156. */
static void paint_prompt(df_u8 *pixels, int w, int h)
{
    static const char text[] = "Please select your home province...";
    int wide;
    int x;

    if (!g_font.bytes || w < 1) {
        return;
    }
    wide = smallfont_width(&g_font, text);
    x = (w - wide) / 2;
    smallfont_draw(&g_font, pixels, w, h, x, 16, 145, 156, text);
}

/* MAP100I0.IMG under MAP.PAL is the picture title_menu copies to the screen. */
static void load_province_map(void)
{
    ImgPicture pic;
    df_u8 rgb[768];
    df_u8 *composed;
    df_u32 bytes;
    int id;

    if (!img_load("MAP100I0.IMG", &pic) || !map_palette(rgb)) {
        log_print(LOG_WARN, "gfall: MAP100I0.IMG did not load");
        return;
    }
    id = pic_load(pic.pixels, pic.width, pic.height, rgb, 0);
    pic_set(PIC_BACK, id);
    g_land = pic.pixels;
    g_map_draw = pic.pixels;
    g_land_w = pic.width;
    g_land_h = pic.height;
    if (map_palette(g_map_rgb)) {
        g_have_map_rgb = 1;
    }
    if (pic.width < 1 || pic.height < 1) {
        return;
    }
    bytes = (df_u32)pic.width * (df_u32)pic.height;
    composed = (df_u8 *)mem_keep(bytes);
    if (!composed) {
        pic_set(PIC_MAP, id);
        return;
    }
    mem_copy(composed, pic.pixels, bytes);
    paint_prompt(composed, pic.width, pic.height);
    g_map_draw = composed;
    pic_set(PIC_MAP, pic_load(composed, pic.width, pic.height, rgb, 0));
}

static void load_classes(void)
{
    int i;

    class_clear();
    for (i = 0; i < 18; i++) {
        char file[16];
        const df_u8 *b;
        df_u32 n;
        ClassCfg cfg;
        ClassInfo row;
        int k;

        classcfg_name(i, file);
        b = vfs_read(file, &n);
        if (!b || !classcfg_read(b, n, &cfg)) {
            continue;
        }
        mem_zero(&row, sizeof(row));
        df_copy(row.name, cfg.name, 16);
        row.name[15] = 0;
        for (k = 0; k < ATTRS; k++) {
            row.attribute[k] = cfg.attribute[k];
        }
        row.hp_per_level = cfg.hp_per_level;
        row.flags = cfg.abilities;
        row.resistance = cfg.resist;
        row.immunity = cfg.immune;
        row.tolerance = cfg.low_tolerance;
        row.weakness = cfg.weakness;
        row.absorption = cfg.absorption;
        row.rapid = cfg.rapid_healing;
        row.regeneration = cfg.regeneration;
        row.attack = cfg.attack_modifier;
        row.forbidden_materials = cfg.forbidden_materials;
        /* The shared reader combines +0D expertise and +0E restrictions. */
        row.expert = (int)(cfg.weapons >> 16);
        row.forbidden_equipment = (int)(cfg.weapons & 0xffffu);
        row.special_flags = b[8];
        row.advancement = (int)cfg.advancement;
        for (k = 0; k < 3; k++) {
            row.primary[k] = cfg.primary[k];
            row.major[k] = cfg.major[k];
        }
        for (k = 0; k < 6; k++) {
            row.minor[k] = cfg.minor[k];
        }
        class_set(i, &row);
    }
}

static void load_class_questions(void)
{
    char *text;
    df_u32 size;
    const df_u8 *answers;
    int number;
    int i;

    text = (char *)mem_keep(32768);
    if (!text || textrsc_read(9000, text, 32768) < 0) { return; }
    for (i = 0; text[i]; i++) {
        if (text[i] != '{') { continue; }
        text[i] = 0;
        i++;
        number = 0;
        while (text[i] >= '0' && text[i] <= '9') { number = number * 10 + text[i++] - '0'; }
        if (text[i] == '.') { i++; }
        question_set(number, text + i);
    }
    answers = vfs_read("CLASSES.DAT", &size);
    if (answers && size >= 216) { class_answers_set(answers + 18); }
}

static void load_compass(void)
{
    df_u8 rgb[768];
    int i;
    if (!img_load_bss("CMPA00I0.BSS", &g_compass_data) || !img_art_palette(rgb)) { return; }
    g_compass_pic = pic_load(g_compass_data.pixels, g_compass_data.width, g_compass_data.height, rgb, 1);
    g_compass_frame = 0;
    for (i = 0; i < g_compass_data.frames && i < 32; i++) {
        compass_set(i, g_compass_pic, g_compass_data.width, g_compass_data.height);
    }
}

static void update_compass(void)
{
    int frame;
    frame = hud_compass_frame(move_yaw());
    if (g_compass_pic && frame != g_compass_frame && frame < g_compass_data.frames) {
        pic_update(g_compass_pic, g_compass_data.pixels + frame * g_compass_data.width * g_compass_data.height);
        g_compass_frame = frame;
    }
}

static void load_panels(void)
{
    df_u8 rgb[768];
    const CifFrame *frame;
    int i;

    img_load("OPTN00I0.IMG", &g_options_art);
    img_load("CNFG00I0.IMG", &g_controls_art);
    img_load("CNFG00I1.IMG", &g_controls_view);
    img_load("CNFG01I0.IMG", &g_mouse_art);
    img_load("LOAD00I0.IMG", &g_files_art);
    img_load("SAVE00I0.IMG", &g_save_label_art);
    if (!cif_open("PNTER.CIF", &g_pointer_art) || !img_art_palette(rgb)) { return; }
    for (i = 0; i < 9; i++) {
        frame = cif_frame(&g_pointer_art, i);
        if (frame) { pic_set(PIC_CURSOR0 + i, pic_load(frame->pixels, frame->w, frame->h, rgb, 1)); }
    }
}

/* BUTTONS.RCI is 32 x 16 frames, DFU's order. Yes is 3, No 4, Male 6, Female 7. */
static void load_choice_buttons(void)
{
    const df_u8 *b;
    df_u32 n;
    df_u8 rgb[768];
    static const int which[4] = { 3, 4, 6, 7 };
    static const int slot[4] = { PIC_YES, PIC_NO, PIC_MALE, PIC_FEMALE };
    int i;

    b = vfs_read("BUTTONS.RCI", &n);
    g_buttons = b;
    if (!b || !img_art_palette(rgb)) {
        log_print(LOG_WARN, "gfall: BUTTONS.RCI did not load");
        return;
    }
    for (i = 0; i < 4; i++) {
        int off;

        off = which[i] * 32 * 16;
        if (off + 32 * 16 > (int)n) {
            continue;
        }
        pic_set(slot[i], pic_load(b + off, 32, 16, rgb, 0));
    }
}

/* Pictures title_menu, list_popup and chargen_name_character load. */
static void load_chargen_screens(void)
{
    df_u32 n;
    static const char *sheet[] = {
        "CHAR00I0.IMG", "CHAR01I0.IMG", "CHAR02I0.IMG",
        "CHAR03I0.IMG", "CHAR04I0.IMG", "CHAR05I0.IMG"
    };
    int i;

    g_spop = vfs_read("SPOP.RCI", &n);
    if (!g_spop || n < 22u * 22u * 9u) {
        g_spop = 0;
        log_print(LOG_WARN, "gfall: SPOP.RCI did not load");
    }
    g_have_art_rgb = img_art_palette(g_art_rgb);
    if (g_have_art_rgb) {
        df_u8 pixel;
        pixel = 145; pic_set(PIC_BREATH,pic_load(&pixel,1,1,g_art_rgb,0));
        pixel = 246; pic_set(PIC_BREATH_LOW,pic_load(&pixel,1,1,g_art_rgb,0));
    }
    g_have_pick = img_load("PICK00I0.IMG", &g_pick);
    g_have_butn2 = img_load("BUTN02I0.IMG", &g_butn2);
    g_have_biog_pic = img_load("BIOG00I0.IMG", &g_biog_pic);
    g_have_chgn = img_load("CHGN00I0.IMG", &g_chgn);
    if (g_have_chgn && img_palette(&g_chgn, g_chgn_rgb)) {
        g_have_chgn_rgb = 1;
    }
    for (i = 0; i < 6; i++) {
        g_have_sheet[i] = img_load(sheet[i], &g_sheet[i]);
    }
    for (i = 0; i < 4; i++) {
        char file[20];
        log_format(file, sizeof(file), "CUST%02dI0.IMG", i);
        img_load(file, &g_custom_screen[i]);
    }
    img_load("CUST08I0.IMG", &g_dagger);
    img_load("CUST09I0.IMG", &g_attr_arrows);
    img_load("CHAR03I1.IMG", &g_skill_arrows);
    img_load("CHAR05I1.IMG", &g_reflex);
    g_faces_race = -1;
    g_biog_class = -1;
}

static void load_overlay(void)
{
    const CifFrame *frame;
    df_u8 rgb[768];

    if (!cif_open("MCOL00I0.CIF", &g_overlay) || !img_art_palette(rgb)) {
        return;
    }
    {
        int i;
        for (i = 0; i < 4; i++) {
            frame = cif_frame(&g_overlay,i);
            if (frame) { pic_set(PIC_OVERLAY+i,pic_load(frame->pixels,frame->w,frame->h,rgb,1)); }
        }
    }
}

int seam_map_pixel(int x, int y)
{
    if (!g_map || x < 0 || y < 0 || x >= g_map_w || y >= g_map_h) {
        return 0;
    }
    return g_map[y * g_map_w + x];
}

/* career_background_summary/career_apply_answer, through the existing BIOG reader.
 * The callback exposes plain character fields; GFALL never includes dagger. */
int gfall_background(int klass, int question, int answer)
{
    const BiogAnswer *a;
    CharacterState *who;
    int i;

    if (klass == 18) { klass = nearest_class(18); }
    ensure_biog(klass);
    if (!g_biog_ok || question < 0 || question >= g_biog.questions) {
        return 1;
    }
    if (answer < 0) {
        return g_biog.question[question].answers;
    }
    if (answer >= g_biog.question[question].answers) {
        return 0;
    }
    a = &g_biog.question[question].answer[answer];
    who = character_state();
    for (i = 0; i < a->effects; i++) {
        const BiogEffect *e;
        e = &a->effect[i];
        if (e->kind == BIOG_SKILL && e->a >= 0 && e->a < SKILLS) {
            /* Applied before zero skills get their 3..6 roll in chargen_name_character. */
            who->skill[e->a] += e->b;
        } else if (e->kind == BIOG_GOLD) {
            who->gold += e->a;
        } else if (e->kind == BIOG_SOCIAL && e->a >= 0 && e->a < 10) {
            who->reputation[e->a] += e->b;
        } else if (e->kind == BIOG_REACTION) {
            who->reaction += e->a;
        } else if (e->kind == BIOG_RESIST_DISEASE) {
            who->bio_resist_disease += e->a;
        } else if (e->kind == BIOG_RESIST_MAGIC) {
            who->bio_resist_magic += e->a;
        } else if (e->kind == BIOG_TO_HIT_AGAINST) {
            who->bio_to_hit_against += e->a;
        } else if (e->kind == BIOG_RESIST_POISON) {
            who->bio_resist_poison += e->a;
        } else if (e->kind == BIOG_FATIGUE) {
            who->bio_fatigue += e->a;
        } else if (e->kind == BIOG_ITEM && who->gifts < 32) {
            who->gift[who->gifts].group = e->a;
            who->gift[who->gifts].index = e->b;
            who->gift[who->gifts].material = e->c;
            who->gifts++;
        }
    }
    return 1;
}
void seam_title(void)
{
    static const int which[4] = {3,4,6,7};
    static const int slots[4] = {PIC_YES,PIC_NO,PIC_MALE,PIC_FEMALE};
    int i;
    gfall_screen_clear_composition();
    /* Re-upload retained menu art; repeated deaths allocate no new sources. */
    pic_set(PIC_TITLE,menu_picture("PICK03I0.IMG",&g_title_art));
    pic_set(PIC_BACK,pic_load(g_land,g_land_w,g_land_h,g_map_rgb,0));
    pic_set(PIC_MAP,pic_load(g_map_draw,g_land_w,g_land_h,g_map_rgb,0));
    pic_set(PIC_BUTTONS,menu_picture("BUTN01I0.IMG",&g_class_art));
    if (g_buttons) {
        for (i = 0; i < 4; i++) { pic_set(slots[i],pic_load(g_buttons+which[i]*512,32,16,g_art_rgb,0)); }
    }
    g_present_mode = -1; gfall_screen_reset_possessions(); g_last_panel = -1;
    pic_set(PIC_SCREEN,0);
    music_stop();
}
void gfall_screen_put_px(int x, int y, int colour)
{
    if (!g_buf || x < 0 || y < 0 || x >= 320 || y >= 200) {
        return;
    }
    g_buf[y * 320 + x] = (df_u8)colour;
}

void gfall_screen_blit_ix(int dx, int dy, const df_u8 *src, int sw, int sh, int key)
{
    int row;
    int col;

    if (!src || sw < 1 || sh < 1) {
        return;
    }
    for (row = 0; row < sh; row++) {
        for (col = 0; col < sw; col++) {
            int colour;

            colour = src[row * sw + col];
            if (key && colour == 0) {
                continue;
            }
            gfall_screen_put_px(dx + col, dy + row, colour);
        }
    }
}

void gfall_screen_cover_screen(const df_u8 *src, int w, int h)
{
    if (!g_buf) {
        return;
    }
    if (src && w == 320 && h == 200) {
        mem_copy(g_buf, src, CANVAS_BYTES);
        return;
    }
    mem_zero(g_buf, CANVAS_BYTES);
    if (src) {
        gfall_screen_blit_ix(0, 0, src, w, h, 0);
    }
}

static void lay_map(void)
{
    gfall_screen_cover_screen(g_land, g_land_w, g_land_h);
}

/* SPOP.RCI's nine 22 x 22 tiles, the same corners msgbox_render uses. */
void gfall_screen_lay_paper(int x0, int y0, int cols, int rows)
{
    int row;
    int col;

    if (!g_spop || cols < 2 || rows < 2) {
        return;
    }
    for (row = 0; row < rows; row++) {
        int piece_row;

        piece_row = row == 0 ? 0 : (row == rows - 1 ? 2 : 1);
        for (col = 0; col < cols; col++) {
            int piece_col;
            int piece;

            piece_col = col == 0 ? 0 : (col == cols - 1 ? 2 : 1);
            piece = piece_row * 3 + piece_col;
            gfall_screen_blit_ix(x0 + col * 22, y0 + row * 22, g_spop + piece * 22 * 22, 22, 22, 1);
        }
    }
}

void gfall_screen_lay_button(int frame, int x, int y)
{
    if (!g_buttons || frame < 0) {
        return;
    }
    gfall_screen_blit_ix(x, y, g_buttons + frame * 32 * 16, 32, 16, 0);
}

static int take_lines(const char *text, char lines[][96], int cap)
{
    int n;
    int i;

    n = 0;
    i = 0;
    if (!text) {
        return 0;
    }
    while (text[i] && n < cap) {
        int k;

        k = 0;
        while (text[i] && text[i] != '\n' && k < 95) {
            lines[n][k] = text[i];
            k++;
            i++;
        }
        lines[n][k] = 0;
        if (text[i] == '\n') {
            i++;
        }
        if (k > 0) {
            n++;
        }
    }
    return n;
}

/*
 * msgbox_render's tile box. buttons is kind 5 (the +20). The button row
 * then sits at box_button_y, which is text_y + lines * 7 + 4.
 */
void gfall_screen_paper_box(char lines[][96], int n, int buttons, int *x0, int *y0, int *cols, int *rows, int *text_y)
{
    int i;
    int wide;
    int mh;
    int mw;
    int ww;
    int hh;
    int c;
    int r;
    int boxw;
    int boxh;

    wide = 0;
    for (i = 0; i < n; i++) {
        int w;

        w = smallfont_width(&g_font, lines[i]);
        if (w > wide) {
            wide = w;
        }
    }
    mh = n * 7 + 10;
    mw = wide + 10;
    if (buttons) {
        mh += 20;
    }
    ww = mw + 10;
    c = (ww + 21) / 22;
    if (c < 2) {
        c = 2;
    }
    hh = mh + 10;
    r = (hh + 21) / 22;
    if (r < 2) {
        r = 2;
    }
    boxw = c * 22;
    boxh = r * 22;
    *cols = c;
    *rows = r;
    *x0 = 160 - (boxw >> 1);
    *y0 = 100 - (boxh >> 1);
    *text_y = *y0 + 5 + ((boxh - hh + 10) >> 1);
}

void gfall_screen_paint_lines(char lines[][96], int n, int centered, int x0, int text_y, int ink)
{
    int i;
    int y;

    y = text_y;
    for (i = 0; i < n; i++) {
        int x;

        x = centered ? 160 - smallfont_width(&g_font, lines[i]) / 2 : x0 + 8;
        smallfont_draw(&g_font, g_buf, CANVAS_W, CANVAS_H, x, y, ink, 156, lines[i]);
        y += 7;
    }
}

/* Race, gender and class confirms: TEXT.RSC on SPOP, BUTTONS.RCI under it. */
static void paint_parchment(const char *text, int centered, int buttons, int left_frame, int right_frame)
{
    char lines[16][96];
    int n;
    int x0;
    int y0;
    int cols;
    int rows;
    int text_y;

    n = take_lines(text, lines, 16);
    if (n < 1) {
        n = 1;
        lines[0][0] = 0;
    }
    chargen_set_lines(n);
    gfall_screen_paper_box(lines, n, buttons, &x0, &y0, &cols, &rows, &text_y);
    lay_map();
    gfall_screen_lay_paper(x0, y0, cols, rows);
    gfall_screen_paint_lines(lines, n, centered, x0, text_y, 146);
    if (buttons) {
        int by;

        by = chargen_choice_y();
        gfall_screen_lay_button(left_frame, CHOICE_X0, by);
        gfall_screen_lay_button(right_frame, CHOICE_X1, by);
    }
}

/* picklist_update_thumb/picklist_draw, including the original bevel colours. */
static void paint_scrollbar(int top, int count)
{
    int y, h, row, col, ink;
    chargen_scroll_thumb(top, count, &y, &h);
    for (row = 0; row < h; row++) {
        for (col = 0; col < 5; col++) {
            ink = col == 0 || row == h - 1 ? 123 : (col == 4 || row == 0 ? 112 : 114);
            gfall_screen_put_px(241 + col, y + row, ink);
        }
    }
}

static void paint_class_list(void)
{
    int i;
    int top;
    int sel;
    int shown;

    lay_map();
    if (g_have_pick) {
        gfall_screen_blit_ix(g_pick.x, g_pick.y, g_pick.pixels, g_pick.width, g_pick.height, 0);
    }
    top = chargen_list_top();
    sel = chargen_list_sel();
    paint_scrollbar(top, LIST_N);
    shown = LIST_VIS;
    for (i = 0; i < shown; i++) {
        int row;
        const char *name;
        int ink;

        row = top + i;
        if (row >= LIST_N) {
            break;
        }
        if (row == 18) {
            name = "Custom";
        } else {
            const ClassInfo *klass;

            klass = class_at(row);
            name = klass ? klass->name : "";
        }
        ink = row == sel ? 244 : 146;
        /* picklist_draw: shadow one down-right, then the line at x+1, y starting at 1. */
        smallfont_draw(&g_font, g_buf, CANVAS_W, CANVAS_H, LIST_X + 1, LIST_Y + 1 + i * LIST_ROW, ink, 156, name);
    }
}

static void ensure_biog(int klass)
{
    if (klass < 0) {
        klass = 0;
    }
    if (g_biog_ok && g_biog_class == klass) {
        return;
    }
    g_biog_ok = biog_load(klass, &g_biog);
    g_biog_class = klass;
}

static void paint_biography(void)
{
    int q;
    int i;
    static const int box_x[] = { 10, 160, 10, 160, 10, 160, 10, 160 };
    static const int box_y[] = { 71, 71, 95, 95, 119, 119, 143, 143 };
    const BiogQuestion *question;

    if (g_have_biog_pic) {
        gfall_screen_cover_screen(g_biog_pic.pixels, g_biog_pic.width, g_biog_pic.height);
    } else {
        lay_map();
    }
    ensure_biog(character_state()->klass == 18 ? nearest_class(18) : character_state()->klass);
    if (!g_biog_ok) {
        return;
    }
    q = chargen_question();
    if (q < 0 || q >= g_biog.questions) {
        return;
    }
    question = &g_biog.question[q];
    smallfont_draw(&g_font, g_buf, CANVAS_W, CANVAS_H, 31, 23, 145, 141, question->line[0]);
    smallfont_draw(&g_font, g_buf, CANVAS_W, CANVAS_H, 31, 33, 145, 141, question->line[1]);
    for (i = 0; i < question->answers && i < 8; i++) {
        smallfont_draw(&g_font, g_buf, CANVAS_W, CANVAS_H, box_x[i] + 21, box_y[i] + 5, 145, 141, question->answer[i].text);
    }
}

static void ensure_faces(int race, int female)
{
    char name[16];

    if (race < 0 || race > 7) {
        race = 0;
    }
    female = female ? 1 : 0;
    if (g_faces_open && g_faces_race == race && g_faces_sex == female) {
        return;
    }
    if (g_faces_open) {
        cif_close(&g_faceset);
        g_faces_open = 0;
    }
    /* chargen_name_character: face%d%di0.cif, gender then race. */
    name[0] = 'F';
    name[1] = 'A';
    name[2] = 'C';
    name[3] = 'E';
    name[4] = female ? '1' : '0';
    name[5] = (char)('0' + race);
    name[6] = 'I';
    name[7] = '0';
    name[8] = '.';
    name[9] = 'C';
    name[10] = 'I';
    name[11] = 'F';
    name[12] = 0;
    g_faces_open = cif_open(name, &g_faceset);
    g_faces_race = race;
    g_faces_sex = female;
}

/* chargen_draw_face: the CIF frame's own place, shifted into the panel. */
static void paint_portrait(int index)
{
    const CifFrame *frame;
    int k;
    int x;
    int y;

    if (!g_faces_open) {
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= g_faceset.n) {
        index = g_faceset.n - 1;
    }
    frame = cif_frame(&g_faceset, index);
    if (!frame) {
        return;
    }
    k = frame->y + 16 + frame->h;
    if (k >= 63) {
        k = 16 - (k - 63);
    } else {
        k = 16;
    }
    x = frame->x + 24;
    y = frame->y + k;
    gfall_screen_blit_ix(x, y, frame->pixels, frame->w, frame->h, 1);
}

static void paint_sheet(int which)
{
    if (which >= 0 && which < 6 && g_have_sheet[which]) {
        gfall_screen_cover_screen(g_sheet[which].pixels, g_sheet[which].width, g_sheet[which].height);
    } else {
        gfall_screen_cover_screen(0, 0, 0);
    }
}

void gfall_screen_text_at(int x, int y, const char *text)
{
    smallfont_draw(&g_font, g_buf, CANVAS_W, CANVAS_H, x, y, 145, 141, text);
}

void gfall_screen_number_at(int x, int y, int value, int centered, int sign)
{
    char text[20];

    log_format(text, sizeof(text), sign && value > 0 ? "+%d" : "%d", value);
    if (centered) { x -= smallfont_width(&g_font, text) / 2; }
    gfall_screen_text_at(x, y, text);
}

/* generate.c/run_0009169B: fields sit at the same coordinates in the summary. */
static void paint_attributes(int summary)
{
    CharacterState *who;
    int i;
    int y;

    who = character_state();
    for (i = 0; i < 8; i++) {
        gfall_screen_number_at(24, chargen_attr_y(i) + 20 - g_font.height, who->attribute[i], 1, 0);
    }
    y = chargen_attr_y(chargen_attr_selected()) + 1;
    gfall_screen_blit_ix(44, y, g_attr_arrows.pixels, g_attr_arrows.width, g_attr_arrows.height, 1);
    gfall_screen_number_at(51, y + 14 - g_font.height, chargen_attr_pool(), 1, 0);
    if (summary) { return; }
    gfall_screen_number_at(83, 22, who->attribute[ATTR_STR] / 10 - 5, 0, 1);
    gfall_screen_number_at(103, 32, who->attribute[ATTR_STR] * 15 / 10, 0, 0);
    gfall_screen_number_at(112, 49, magicka_state(who->attribute[ATTR_INT], who->class_flags), 0, 0);
    gfall_screen_number_at(121, 71, who->attribute[ATTR_WIL] / 10, 0, 0);
    gfall_screen_number_at(97, 93, who->attribute[ATTR_AGI] / 10 - 5, 0, 1);
    gfall_screen_number_at(101, 110, who->attribute[ATTR_END] / 10 - 5, 0, 1);
    gfall_screen_number_at(122, 120, who->attribute[ATTR_END] / 10 - 5, 0, 1);
}

static void paint_skills(void)
{
    CharacterState *who;
    int i;
    int skill;
    int y;

    who = character_state();
    for (i = 0; i < 12; i++) {
        skill = class_skill(who->klass, i);
        if (skill < 0 || skill >= SKILLS) { continue; }
        y = chargen_skill_y(i) + 1;
        gfall_screen_text_at(68, y, skill_name(skill));
        gfall_screen_number_at(192, y, who->skill[skill], 1, 0);
    }
    for (i = 0; i < 3; i++) {
        y = chargen_skill_y(chargen_skill_selected(i));
        gfall_screen_blit_ix(203, y, g_skill_arrows.pixels, g_skill_arrows.width, g_skill_arrows.height, 0);
        gfall_screen_number_at(221, y + 9 - g_font.height, 6 - chargen_skill_bonus(i), 1, 0);
    }
}

static void paint_custom(void)
{
    const ClassInfo *c;
    const Special *s;
    int page, list, kind, param, i, y, top, limit, skill;
    int advance;
    char text[96];

    c = class_at(18);
    if (!c) { lay_map(); return; }
    page = chargen_maker_page();
    list = chargen_maker_list();
    kind = chargen_maker_kind();
    gfall_screen_cover_screen(g_custom_screen[0].pixels, CANVAS_W, CANVAS_H);
    if (page == 6) {
        gfall_screen_blit_ix(39, 5, g_custom_screen[3].pixels, g_custom_screen[3].width, g_custom_screen[3].height, 0);
    } else if (page == 2 || page == 3 || page == 4 || page == 5) {
        gfall_screen_blit_ix(0, 0, g_custom_screen[1].pixels, g_custom_screen[1].width, g_custom_screen[1].height, 0);
        if (!list) { gfall_screen_blit_ix(0, 0, g_custom_screen[2].pixels, g_custom_screen[2].width, g_custom_screen[2].height, 0); }
    }
    if (page == 0 || page == 7 || page == 1 || page == 4 || page == 5) {
        gfall_screen_text_at(94, 5, c->name);
        if (page == 7) { gfall_screen_text_at(94 + smallfont_width(&g_font, c->name), 5, "_"); }
        for (i = 0; i < 8; i++) {
            gfall_screen_number_at(24, chargen_attr_y(i) + 20 - g_font.height, c->attribute[i], 1, 0);
        }
        y = chargen_attr_y(chargen_attr_selected()) + 1;
        gfall_screen_blit_ix(44, y, g_attr_arrows.pixels, g_attr_arrows.width, g_attr_arrows.height, 1);
        gfall_screen_number_at(51, y + 14 - g_font.height, chargen_attr_pool(), 1, 0);
        for (i = 0; i < 12; i++) {
            gfall_screen_text_at(68, chargen_skill_y(i) + 1, skill_name(class_skill(18, i)));
        }
        gfall_screen_number_at(286, 55, c->hp_per_level, 1, 0);
        advance = c->advancement;
        y = advance >= 65536 ? 115 - (advance - 65536) * 69 / 262144 : 115 + (65536 - advance) * 69 / 65536;
        if (y < 46) { y = 46; } if (y > 175) { y = 175; }
        gfall_screen_blit_ix(219, y, g_dagger.pixels, g_dagger.width, g_dagger.height, 1);
    }
    if (page == 2 || page == 3) {
        for (i = 0; i < chargen_special_count(list); i++) {
            kind = chargen_special_kind(list, i); param = chargen_special_param(list, i);
            s = special_state(list, kind);
            gfall_screen_text_at(5, 33 + i * 20, s->name);
            gfall_screen_text_at(5, 42 + i * 20, s->choice[param]);
        }
    } else if (page == 6) {
        int total, value, x, yy, h, start;
        total = 0;
        for (i = 0; i < 5; i++) {
            value = chargen_maker_reputation(i);
            total += value;
            h = (value < 0 ? -value : value) * 5;
            start = value > 0 ? 81 - h : 82;
            for (yy = start; yy < start + h; yy++) {
                for (x = 42 + i * 33; x <= 46 + i * 33; x++) { gfall_screen_put_px(x, yy, value > 0 ? 197 : 246); }
            }
            gfall_screen_number_at(58 + i * 33, 149, value, 1, 0);
        }
        gfall_screen_number_at(105, 179, -total, 1, 0);
    } else if (page == 1 || page == 4 || page == 5) {
        if (g_have_pick) { gfall_screen_blit_ix(g_pick.x, g_pick.y, g_pick.pixels, g_pick.width, g_pick.height, 0); }
        top = chargen_maker_top();
        limit = page == 4 ? 11 : (page == 5 ? special_state(list, kind)->count : 0);
        if (page == 1) { while (chargen_maker_skill(limit) >= 0) { limit++; } }
        paint_scrollbar(top, limit);
        for (i = 0; i < 9 && top + i < limit; i++) {
            if (page == 1) {
                skill = chargen_maker_skill(top + i);
                df_copy(text, skill_name(skill), sizeof(text));
            } else if (page == 4) {
                df_copy(text, special_state(list, top + i)->name, sizeof(text));
            } else {
                df_copy(text, special_state(list, kind)->choice[top + i], sizeof(text));
            }
            gfall_screen_text_at(86, 63 + i * 8, text);
        }
    }
}

static void paint_question(void)
{
    /* Process-lived scratch, used only by this single-threaded composer.
     * Automatic storage was inlined into seam_present: its 15 KB frame
     * skipped Win98's stack guard page even on the title's early return. */
    static df_u8 strip[320 * 48];
    const char *text;
    char line[160];
    int i;
    int y;

    mem_zero(strip, sizeof(strip));
    text = question_text(chargen_question_number());
    y = -chargen_question_scroll();
    while (*text) {
        i = 0;
        while (*text && *text != '\n') {
            if (i < (int)sizeof(line) - 1) { line[i++] = *text; }
            text++;
        }
        line[i] = 0;
        if (*text) { text++; }
        smallfont_draw(&g_font, strip, 320, 48, 20, y, 255, -1, line);
        y += 7;
    }
    gfall_screen_blit_ix(0, 135, strip, 320, 48, 0);
}

static void paint_reflex(int summary)
{
    int r;
    int x;
    int y;

    r = character_state()->reflexes;
    x = summary ? 246 : 127;
    y = (summary ? 95 : 148) + r * 9;
    if (g_reflex.pixels) {
        gfall_screen_blit_ix(x, y, g_reflex.pixels + r * 66 * 9, 66, 9, 0);
    }
    if (!summary) {
        char lines[16][96];
        int n, x0, y0, cols, rows, ty, shift;
        n = take_lines(words_state(307), lines, 16);
        gfall_screen_paper_box(lines, n, 0, &x0, &y0, &cols, &rows, &ty);
        /* generate.c anchors kind-4 help at y=125 (D_00195F34).
         * Preserve the selector/OK underneath instead of drawing naked text. */
        shift = 125 - rows * 22 - y0;
        gfall_screen_lay_paper(x0, y0 + shift, cols, rows);
        gfall_screen_paint_lines(lines, n, 1, x0, ty + shift, 145);
    }
}

/* Snapshot is the actual indexed surface uploaded by publish, useful for host QA. */
const df_u8 *seam_screen(const df_u8 **palette)
{
    if (palette) {
        *palette = maps_up() ? gfall_screen_map_palette() : g_screen_kind == 2 ? g_chgn_rgb : (g_screen_kind == 1 ? g_art_rgb : g_map_rgb);
    }
    return g_buf;
}

static int g_hud_form = -1;

/* The 32 x 32 portrait of the bar: the race's face, a vampire's VAMP00I0.CIF frame (the sex's eight, the race's), a beast's WERE01I0.IMG (werewolf) or WERE00I0.IMG (wereboar) (docs/curses.md 5.1, 7.3). */
static void selected_hud_face(void)
{
    const CifFrame *frame;
    const df_u8 *source;
    CifSet vampire;
    ImgPicture beast;
    df_u8 pixels[32 * 32];
    int x;
    int y;
    int sx;
    int sy;
    int sw;
    int sh;
    int id;
    int form;
    int opened;
    CharacterState *who;

    who = character_state();
    form = curse_form();
    opened = 0;
    if (g_face_index == who->face && g_hud_race == who->race && g_hud_sex == who->female && g_hud_form == form) { return; }
    source = 0; sw = sh = 0; frame = 0;
    if (form > 8 && img_load(form == 9 ? "WERE01I0.IMG" : "WERE00I0.IMG", &beast) && beast.width > 0 && beast.height > 0) {
        source = beast.pixels; sw = beast.width; sh = beast.height;
    } else if (form == 8 && cif_open("VAMP00I0.CIF", &vampire)) {
        opened = 1;
        frame = cif_frame(&vampire, (who->female ? 0 : 8) + who->race);
        if (frame) { source = frame->pixels; sw = frame->w; sh = frame->h; }
    } else {
        ensure_faces(who->race, who->female);
        frame = g_faces_open ? cif_frame(&g_faceset, who->face) : 0;
        if (frame) { source = frame->pixels; sw = frame->w; sh = frame->h; }
    }
    if (source) {
        mem_zero(pixels, sizeof(pixels));
        for (y = 0; y < 32; y++) {
            sy = y * sh / 32;
            for (x = 0; x < 32; x++) {
                sx = x * sw / 32;
                pixels[y * 32 + x] = source[sy * sw + sx];
            }
        }
        id = play_pic(PIC_FACE);
        if (id) { pic_free(id); }
        pic_set(PIC_FACE, pic_load(pixels, 32, 32, g_art_rgb, 1));
        g_face_index = who->face;
        g_hud_race = who->race;
        g_hud_sex = who->female;
        g_hud_form = form;
    }
    if (opened) { cif_close(&vampire); }
}

static int screen_kind(int mode)
{
    if (mode == UI_QUESTIONS) {
        return 2;
    }
    if (mode == UI_MAKER || mode == UI_NAME || mode == UI_FACE || mode == UI_ATTR || mode == UI_SKILL
        || mode == UI_REFLEX || mode == UI_SUMMARY) {
        return 1;
    }
    return 0;
}

void gfall_screen_publish_surface(int kind, int cutout)
{
    df_u8 *pal;
    int i;

    for (i = 0; i < 3; i++) {
        if (i != kind && g_comp[i]) { pic_free(g_comp[i]); g_comp[i] = 0; }
    }

    if (kind == 2 && g_have_chgn_rgb) {
        pal = g_chgn_rgb;
    } else if (kind == 1 && g_have_art_rgb) {
        pal = g_art_rgb;
    } else if (g_have_map_rgb) {
        pal = g_map_rgb;
    } else if (g_have_art_rgb) {
        pal = g_art_rgb;
    } else {
        pic_set(PIC_SCREEN, 0);
        return;
    }
    if (g_comp[kind] && g_comp_cutout[kind] != cutout) { pic_free(g_comp[kind]); g_comp[kind] = 0; }
    g_comp_cutout[kind] = cutout;
    if (!g_comp[kind]) {
        g_comp[kind] = pic_load(g_buf, CANVAS_W, CANVAS_H, pal, cutout);
    } else {
        pic_update(g_comp[kind], g_buf);
    }
    g_screen_kind = kind;
    pic_set(PIC_SCREEN, g_comp[kind]);
}

/*
 * A page fills the whole 320 x 200 with no clear texel (inventory, sheet,
 * book, service, an open court, the spellbook on its underlay): nothing
 * of the world shows, so the app draws the frame with no place. On the
 * 233 the world behind cost about 10 ms a frame that nobody saw. A
 * cutout page (talk, bank, the message, rest, death) keeps the world.
 */
int seam_screen_covers(void)
{
    int page;

    /* A movie or a prince's scene is the whole picture (movie.h): no world is drawn behind it, so DAG2's side bars and the letterbox are black. */
    if (movie_active()) { return 1; }
    page = possessions_state()->page;
    if (!page || page == VIEW_MESSAGE || maps_up() || options_up() || !g_comp[g_screen_kind]) {
        return 0;
    }
    return play_pic(PIC_SCREEN) == g_comp[g_screen_kind] && !g_comp_cutout[g_screen_kind];
}

static void publish(int kind) { gfall_screen_publish_surface(kind,0); }

void gfall_screen_fill_box(int x, int y, int w, int h, int color)
{
    int row, col;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) { gfall_screen_put_px(x + col, y + row, color); }
    }
}

void gfall_screen_outline(int x, int y, int w, int h, int color)
{
    gfall_screen_fill_box(x, y, w, 1, color);
    gfall_screen_fill_box(x, y + h - 1, w, 1, color);
    gfall_screen_fill_box(x, y, 1, h, color);
    gfall_screen_fill_box(x + w - 1, y, 1, h, color);
}

void gfall_screen_centered_text(int x, int y, const char *text, int ink)
{
    smallfont_draw(&g_font, g_buf, CANVAS_W, CANVAS_H, x - smallfont_width(&g_font, text)/2, y, ink, -1, text);
}

/* A bounded message over a panel; controls consume the next click to dismiss. */
static void panel_notice(const char *message)
{
    char lines[8][96];
    char wrapped[512];
    int i, col, last_space, n, x, y, cols, rows, ty;

    i = col = 0;
    last_space = -1;
    while (*message && i < (int)sizeof(wrapped)-2) {
        wrapped[i] = *message++;
        if (wrapped[i] == ' ') { last_space = i; }
        if (++col >= 40 && last_space >= 0) {
            wrapped[last_space] = '\n';
            col = i - last_space;
            last_space = -1;
        }
        i++;
    }
    wrapped[i] = 0;
    n = take_lines(wrapped, lines, 8);
    gfall_screen_paper_box(lines, n, 0, &x, &y, &cols, &rows, &ty);
    gfall_screen_lay_paper(x, y, cols, rows);
    gfall_screen_paint_lines(lines, n, 1, x, ty, 146);
}

static void paint_options(void)
{
    static df_u8 pixels[150 * 84];
    const Config *config;
    int i, x, y, id;

    config = game_config();
    gfall_screen_cover_screen(0, 0, 0);
    gfall_screen_blit_ix(85, 40, g_options_art.pixels, g_options_art.width, g_options_art.height, 0);
    for (i = 0; i < 2; i++) {
        if (options_flags() & (1 << i)) { gfall_screen_fill_box(i ? 225 : 154, 90, 3, 3, 246); }
    }
    gfall_screen_fill_box(91, 64, config->sound_volume * 107 / 100, 3, 246);
    gfall_screen_fill_box(91, 72, config->music_volume * 107 / 100, 3, 246);
    gfall_screen_fill_box(91, 80, config->detail * 108 / 128, 3, 246);
    for (y = 0; y < 84; y++) {
        for (x = 0; x < 150; x++) { pixels[y * 150 + x] = g_buf[(40 + y) * 320 + 85 + x]; }
    }
    id = play_pic(PIC_OPTIONS);
    if (id) { pic_update(id, pixels); }
    else { pic_set(PIC_OPTIONS, pic_load(pixels, 150, 84, g_art_rgb, 0)); }
    pic_set(PIC_SCREEN, 0);
    g_screen_kind = 1;
}

static void paint_controls(void)
{
    const ControlRect *rect;
    char name[32];
    int i, ink;

    gfall_screen_cover_screen(g_controls_art.pixels, g_controls_art.width, g_controls_art.height);
    if (options_mouse()) {
        gfall_screen_blit_ix(g_controls_view.x, g_controls_view.y, g_controls_view.pixels,
                g_controls_view.width, g_controls_view.height, 0);
    }
    rect = control_rect(SPRINT);
    gfall_screen_fill_box(rect->x0, rect->y0, rect->x1 - rect->x0 + 1, rect->y1 - rect->y0 + 1, 93);
    gfall_screen_outline(rect->x0, rect->y0, rect->x1 - rect->x0 + 1, rect->y1 - rect->y0 + 1, 120);
    gfall_screen_centered_text((rect->x0 + rect->x1) / 2, rect->y0 - 9, "SPRINT", 146);
    for (i = 0; i < ACTS; i++) {
        rect = control_rect(i);
        if (i == controls_editing()) { df_copy(name, "Press key", sizeof(name)); ink = 246; }
        else { control_key_name(i, name, sizeof(name)); ink = controls_duplicate(i) ? 244 : 146; }
        gfall_screen_centered_text((rect->x0 + rect->x1) / 2, rect->y0 + 2, name, ink);
    }
    if (controls_notice()[0]) { panel_notice(controls_notice()); }
    publish(1);
}

static void paint_mouse(void)
{
    static df_u8 pixels[MOUSE_PANEL_W * MOUSE_PANEL_H];
    const Config *config;
    const char *bow;
    int i, x, y, id, split;

    config = game_config();
    gfall_screen_cover_screen(0, 0, 0);
    split = MOUSE_BOW_Y - g_mouse_art.y;
    if (g_mouse_art.pixels && split > 0 && split < g_mouse_art.height) {
        gfall_screen_blit_ix(g_mouse_art.x, g_mouse_art.y, g_mouse_art.pixels, g_mouse_art.width, split, 0);
        gfall_screen_blit_ix(g_mouse_art.x, MOUSE_BOW_Y + MOUSE_BOW_H, g_mouse_art.pixels + split * g_mouse_art.width,
                            g_mouse_art.width, g_mouse_art.height - split, 0);
    }
    gfall_screen_fill_box(MOUSE_PANEL_X, MOUSE_BOW_Y, MOUSE_PANEL_W, MOUSE_BOW_H, 122);
    gfall_screen_fill_box(72, MOUSE_BOW_Y + 1, 176, MOUSE_BOW_H - 2, 93);
    gfall_screen_outline(72, MOUSE_BOW_Y + 1, 176, MOUSE_BOW_H - 2, 120);
    if (config->bow_style == BOW_STYLE_AUTO) {
        bow = config_bow_draw() ? "Bow: Auto (Draw and release)" : "Bow: Auto (Classic)";
    } else {
        bow = config_bow_draw() ? "Bow: Draw and release" : "Bow: Classic";
    }
    gfall_screen_centered_text(160, MOUSE_BOW_Y + 4, bow, 146);
    gfall_screen_fill_box(config->mouse_mode ? 220 : 134, 47, 5, 5, 246);
    for (i = 0; i < config->sensitivity_x; i++) { gfall_screen_fill_box(139 + i * 7, 108, 5, 6, 246); }
    for (i = 0; i < config->sensitivity_y; i++) { gfall_screen_fill_box(139 + i * 7, 121, 5, 6, 246); }
    for (i = 0; i < config->turn_rate; i++) { gfall_screen_fill_box(139 + i * 7, 134, 5, 6, 246); }
    if (config->reverse_y) { gfall_screen_outline(72, 147 + MOUSE_BOW_H, 76, 15, 246); }
    for (y = 0; y < MOUSE_PANEL_H; y++) {
        for (x = 0; x < MOUSE_PANEL_W; x++) {
            pixels[y * MOUSE_PANEL_W + x] = g_buf[(MOUSE_PANEL_Y + y) * 320 + MOUSE_PANEL_X + x];
        }
    }
    id = play_pic(PIC_MOUSE);
    if (id) { pic_update(id, pixels); }
    else { pic_set(PIC_MOUSE, pic_load(pixels, MOUSE_PANEL_W, MOUSE_PANEL_H, g_art_rgb, 0)); }
    pic_set(PIC_SCREEN, 0);
    g_screen_kind = 1;
}

static void free_previews(void)
{
    int i;
    for (i = 0; i < 6; i++) {
        if (g_preview[i]) { pic_free(g_preview[i]); g_preview[i] = 0; }
        preview_pic_set(i, 0);
    }
    g_preview_generation = 0;
}

static void prepare_previews(void)
{
    const SaveInfo *info;
    int i;
    if (g_preview_generation == files_generation()) { return; }
    free_previews();
    for (i = 0; i < 6; i++) {
        info = files_info(i);
        if (info->status == 1 && info->preview_valid) {
            g_preview[i] = pic_load_565(info->preview, THUMB_W, THUMB_H);
            preview_pic_set(i, g_preview[i]);
        }
    }
    g_preview_generation = files_generation();
}

static void paint_files(void)
{
    const SaveInfo *info;
    char text[64];
    int i, x, y;

    gfall_screen_cover_screen(g_files_art.pixels, g_files_art.width, g_files_art.height);
    if (files_saving()) {
        gfall_screen_blit_ix(g_save_label_art.x, g_save_label_art.y, g_save_label_art.pixels,
                g_save_label_art.width, g_save_label_art.height, 0);
    }
    for (i = 0; i < 6; i++) {
        x = i < 3 ? 80 : 240;
        y = (i % 3) * 65;
        info = files_info(i);
        if (info->status == 1) {
            /* Names are clipped to each slot's label strip. */
            df_copy(text, info->label, sizeof(text));
            while (smallfont_width(&g_font, text) > 112) {
                int n;
                n = 0; while (text[n]) { n++; }
                if (!n) { break; }
                text[n-1] = 0;
            }
            gfall_screen_centered_text(x, y + 57, text, 145);
            if (!info->preview_valid) { gfall_screen_centered_text(x, y + 23, info->character, 145); }
        } else {
            gfall_screen_centered_text(x, y + 57, info->status < 0 ? "Damaged save" : "Empty slot", info->status < 0 ? 244 : 146);
        }
        if (i == files_selected()) { gfall_screen_outline(x - 41, y + 3, 82, 52, 146); }
    }
    if (files_error()[0]) { panel_notice(files_error()); }
    if (files_editing()) {
        gfall_screen_lay_paper(28, 66, 12, 4);
        gfall_screen_centered_text(160, 82, "Name this saved game:", 146);
        gfall_screen_centered_text(160, 101, files_name(), 146);
        gfall_screen_lay_button(0, FILE_ACCEPT_X, FILE_BUTTON_Y);
        gfall_screen_lay_button(1, FILE_REJECT_X, FILE_BUTTON_Y);
    }
    publish(1);
}

static int present_panel(void)
{
    unsigned revision;
    int panel;

    panel = options_panel();
    if ((panel != PANEL_CONTROLS || !controls_mouse()) && play_pic(PIC_MOUSE)) {
        pic_free(play_pic(PIC_MOUSE));
        pic_set(PIC_MOUSE, 0);
    }
    if (panel == PANEL_FILES) { prepare_previews(); }
    else if (g_preview_generation) { free_previews(); }
    revision = options_revision();
    if (panel == PANEL_CONTROLS) { revision += controls_revision(); }
    if (panel == PANEL_FILES) { revision += files_revision(); }
    if (g_last_panel == panel && g_panel_revision == revision) { return 1; }
    g_last_panel = panel;
    g_panel_revision = revision;
    if (!g_buf) { g_buf = (df_u8 *)mem_keep(CANVAS_BYTES); }
    if (!g_buf) { return 1; }
    if (panel == PANEL_OPTIONS) { paint_options(); }
    else if (panel == PANEL_FILES) { paint_files(); }
    else if (controls_mouse()) { paint_mouse(); }
    else { paint_controls(); }
    return 1;
}

void seam_present(void)
{
    int mode;
    int kind;
    CharacterState *who;
    const char *text;
    static int death_present;
    static int rest_present;
    static unsigned rest_revision;

    if (maps_up()) { gfall_screen_present_maps(); return; }
    mode = chargen_mode();
    if (mode == UI_WORLD) { selected_hud_face(); update_compass(); }
    if (options_up()) { death_present = 0; present_panel(); return; }
    if (rest_state()->open) {
        if (!rest_present || rest_revision != rest_state()->revision) {
            const RestState *rest;
            const ImgPicture *art;
            char line[48];
            rest = rest_state();
            if (!g_buf) { g_buf = (df_u8 *)mem_keep(CANVAS_BYTES); }
            if (!g_buf) { return; }
            gfall_screen_cover_screen(0,0,0);
            art = &g_rest_art[rest->stage == REST_CHOOSE ? 0 : rest->until_healed ? 1 : 2];
            gfall_screen_blit_ix(art->x,art->y,art->pixels,art->width,art->height,0);
            if (rest->stage == REST_HOURS) {
                gfall_screen_lay_paper(72,96,8,3);
                gfall_screen_centered_text(160,105,rest->loiter ? "Loiter how many hours? (1-3)" : "Rest how many hours? (1-99)",146);
                log_format(line,sizeof(line),"%s_  Enter to accept",rest->input); gfall_screen_centered_text(160,121,line,146);
            } else if (rest->stage == REST_RUNNING) {
                log_format(line,sizeof(line),"%d",rest->until_healed ? rest->elapsed : rest->hours-rest->elapsed);
                gfall_screen_centered_text(118,62,line,146);
            }
            gfall_screen_publish_surface(1,1); rest_revision = rest->revision; rest_present = 1;
        }
        return;
    }
    rest_present = 0;
    if (play_dead()) {
        if (!death_present) {
            if (!g_buf) { g_buf = (df_u8 *)mem_keep(CANVAS_BYTES); }
            if (g_buf) {
                gfall_screen_cover_screen(0,0,0);
                gfall_screen_lay_paper(50,45,10,6);
                gfall_screen_centered_text(160,64,"Thy life has ended.",146);
                gfall_screen_centered_text(160,90,"Load game (L)",146);
                gfall_screen_centered_text(160,114,"Return to title (N)",146);
                gfall_screen_centered_text(160,138,"Quit (Q)",146);
                gfall_screen_publish_surface(1,1); death_present = 1;
            }
        }
        return;
    }
    death_present = 0;
    if (gfall_screen_present_possessions()) { return; }
    if (g_preview_generation) { free_previews(); }
    if (mode == UI_WORLD && g_last_panel >= 0) {
        if (g_comp[1]) { pic_free(g_comp[1]); g_comp[1] = 0; }
        if (play_pic(PIC_MOUSE)) { pic_free(play_pic(PIC_MOUSE)); pic_set(PIC_MOUSE, 0); }
    }
    g_last_panel = -1;
    if (mode == UI_TITLE || mode == UI_MAP || mode == UI_CLASS_HOW
        || mode == UI_WORLD) {
        pic_set(PIC_SCREEN, 0);
        return;
    }
    if (g_present_mode == mode && g_revision == chargen_revision() && play_pic(PIC_SCREEN)) {
        return;
    }
    g_present_mode = mode;
    g_revision = chargen_revision();
    if (!g_buf) {
        g_buf = (df_u8 *)mem_keep(CANVAS_BYTES);
    }
    if (!g_buf) {
        return;
    }
    who = character_state();
    if (mode == UI_LOAD) {
        gfall_screen_cover_screen(g_files_art.pixels, g_files_art.width, g_files_art.height);
    } else if (mode == UI_RACE) {
        text = words_state(chargen_text_id());
        paint_parchment(text, 1, 1, 3, 4);
    } else if (mode == UI_GENDER) {
        text = words_state(2200);
        paint_parchment(text, 1, 1, 6, 7);
    } else if (mode == UI_CLASS_LIST) {
        paint_class_list();
    } else if (mode == UI_CLASS_YES) {
        text = words_state(chargen_text_id());
        paint_parchment(text, 1, 1, 3, 4);
    } else if (mode == UI_QUESTIONS) {
        if (g_have_chgn) {
            gfall_screen_cover_screen(g_chgn.pixels, g_chgn.width, g_chgn.height);
        } else {
            lay_map();
        }
        paint_question();
    } else if (mode == UI_MAKER) {
        paint_custom();
    } else if (mode == UI_BG_HOW) {
        lay_map();
        if (g_have_butn2) {
            gfall_screen_blit_ix(g_butn2.x, g_butn2.y, g_butn2.pixels, g_butn2.width, g_butn2.height, 0);
        }
    } else if (mode == UI_BIOG) {
        paint_biography();
    } else if (mode == UI_REPUTE) {
        char plain[512];
        log_format(plain, sizeof(plain),
            "Your reputation has changed as follows:\nCommoners: %d\nMerchants: %d\nScholars: %d\nNobility: %d\nUnderworld: %d",
            who->reputation[0], who->reputation[1], who->reputation[2], who->reputation[3], who->reputation[4]);
        paint_parchment(plain, 0, 0, 0, 0);
    } else if (mode == UI_NAME) {
        paint_sheet(0);
        gfall_screen_text_at(80, 5, who->name);
        gfall_screen_text_at(80 + smallfont_width(&g_font, who->name), 5, "_");
    } else if (mode == UI_FACE) {
        paint_sheet(1);
        ensure_faces(who->race, who->female);
        paint_portrait(who->face);
    } else if (mode == UI_ATTR) {
        paint_sheet(2);
        paint_attributes(0);
    } else if (mode == UI_SKILL) {
        paint_sheet(3);
        paint_skills();
    } else if (mode == UI_SUMMARY) {
        paint_sheet(4);
        ensure_faces(who->race, who->female);
        paint_portrait(who->face);
        gfall_screen_text_at(80, 5, who->name);
        paint_attributes(1);
        paint_skills();
        paint_reflex(1);
    } else if (mode == UI_REFLEX) {
        paint_sheet(5);
        paint_reflex(0);
    } else {
        lay_map();
    }
    if (chargen_notice()[0]) {
        char lines[16][96];
        int n, x, y, cols, rows, ty;
        n = take_lines(chargen_notice(), lines, 16);
        gfall_screen_paper_box(lines, n, 0, &x, &y, &cols, &rows, &ty);
        gfall_screen_lay_paper(x, y, cols, rows);
        gfall_screen_paint_lines(lines, n, 1, x, ty, 146);
    }
    kind = screen_kind(mode);
    publish(kind);
}

const char *gfall_screen_boot(void)
{
    ImgPicture raw;
    df_u8 rgb[768];

#ifndef DF_HOST
    fnt_install();
#endif
    smallfont_load(&g_font, "FONT0003.FNT");
    pic_set(PIC_TITLE, menu_picture("PICK03I0.IMG", &g_title_art));
    load_click_map("TAMRIEL2.IMG");
    load_province_map();
    pic_set(PIC_BUTTONS, menu_picture("BUTN01I0.IMG", &g_class_art));
    load_choice_buttons();
    load_chargen_screens();
    if (!gfall_screen_load_overlays()) { return "Cannot read the spell casting pictures"; }
    load_panels();
    gfall_screen_load_possessions();
    if (img_load_raw("ARROW.RAW", 10, 10, &raw) && img_art_palette(rgb)) {
        pic_set(PIC_ARROW, pic_load(raw.pixels, raw.width, raw.height, rgb, 1));
    }

    return play_pic(PIC_TITLE) ? 0 : "PICK03I0.IMG";
}

void gfall_screen_load_classes(void)
{
    load_classes();
    load_class_questions();
}

void gfall_screen_enter_world(void)
{
    {
        static const int menus[] = { PIC_TITLE, PIC_BACK, PIC_MAP, PIC_BUTTONS,
            PIC_YES, PIC_NO, PIC_MALE, PIC_FEMALE };
        int i;
        /* Menu pictures give back the card before the hold and its HUD load. */
        gfall_screen_clear_composition();
        pic_set(PIC_SCREEN, 0);
        for (i = 0; i < (int)(sizeof(menus) / sizeof(menus[0])); i++) {
            pic_free(play_pic(menus[i])); pic_set(menus[i], 0);
        }
    }
    if (!g_hud_loaded) {
        pic_set(PIC_BAR, load_img("MAIN00I0.IMG", 0));
        pic_set(PIC_MODES, load_img("MAIN01I0.IMG", 0));
        pic_set(PIC_HEALTH, load_img("MAIN03I0.IMG", 0));
        pic_set(PIC_FATIGUE, load_img("MAIN04I0.IMG", 0));
        pic_set(PIC_MAGICKA, load_img("MAIN05I0.IMG", 0));
        pic_set(PIC_COMPASS_WIDE, load_img("COMPASS.IMG", 0));
        pic_set(PIC_COMPBOX, load_img("COMPBOX.IMG", 0));
        pic_set(PIC_OPTIONS, load_img("OPTN00I0.IMG", 0));
        load_compass();
        load_overlay();
        g_hud_loaded = 1;
    }
}

int gfall_talk_width(const char *text)
{
    return smallfont_width(&g_font, text);
}

int gfall_text_width(const char *text)
{
    int n;

    if (g_font.bytes) { return gfall_talk_width(text); }
    for (n = 0; text[n]; n++) { }
    return n * 6;
}

df_u8 *gfall_screen_pixels(void) { return g_buf; }
const SmallFont *gfall_screen_font(void) { return &g_font; }
const df_u8 *gfall_screen_art_palette(void) { return g_art_rgb; }
int gfall_screen_have_art_palette(void) { return g_have_art_rgb; }
int gfall_screen_paper_ready(void) { return g_spop && g_buttons; }

int gfall_screen_prepare_pixels(void)
{
    if (!g_buf) { g_buf = (df_u8 *)mem_keep(CANVAS_BYTES); }
    return g_buf != 0;
}

void gfall_screen_clear_composition(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (g_comp[i]) { pic_free(g_comp[i]); g_comp[i] = 0; }
    }
}

void gfall_screen_copy_map(const df_u8 *pixels)
{
    if (gfall_screen_prepare_pixels()) { mem_copy(g_buf, pixels, CANVAS_BYTES); }
    g_screen_kind = 1;
}

void gfall_screen_attribute_arrows(int x, int y)
{
    gfall_screen_blit_ix(x,y,g_attr_arrows.pixels,g_attr_arrows.width,g_attr_arrows.height,1);
}

void gfall_screen_load_rest(void)
{
    char file[20];
    int i;

    for (i = 0; i < 3; i++) {
        log_format(file,sizeof(file),"REST%02dI0.IMG",i);
        img_load(file,&g_rest_art[i]);
    }
}
