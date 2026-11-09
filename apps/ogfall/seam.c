#include "game/ogfall/play/state.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/activate.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/weapon.h"
#include "game/ogfall/play/screen.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/tables/itemdefs.h"
#include "game/ogfall/tables/combatdefs.h"
#include "game/dagger/tables/careers.h"
#include "game/ogfall/play/config.h"
#include "game/ogfall/play/hud.h"
#include "game/ogfall/play/options.h"
#include "game/ogfall/play/controls.h"
#include "game/ogfall/play/files.h"
#include "game/ogfall/play/rest.h"
#include "seam.h"

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/ui/pic.h"
#include "engine/scene/collide.h"

#include "game/dagger/formats/bind.h"
#include "game/dagger/formats/arch3d.h"
#include "game/dagger/formats/bsa.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/classcfg.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/biog.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/rdb.h"
#include "game/dagger/formats/smallfont.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/places/world.h"
#include "game/dagger/tables/maindat.h"

#include "game/ogfall/formats/formats.h"
#include "game/ogfall/places/place.h"
#include "game/ogfall/play/chargen.h"
#include "game/ogfall/play/play.h"
#include "game/ogfall/tables/tables.h"

#ifndef DF_HOST
#include "game/dagger/formats/fnt.h"
#endif

/*
 * doors.c's kludge_fix_dungeon_door is not re-coded. The hold's doors
 * are the ones world_load already built. A disagreement with the decomp
 * stays a disagreement for this slice.
 *
 * quests_start_initial is not called. See play/chargen.h.
 */

#define MARKS 1024
#define TILE 2048

static char g_why[96];
static World *g_world;
static Level g_level;
static void weapon_draw(const OgWeaponView *view, float flash);
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
static ImgPicture g_possession_art[5];
static ImgPicture g_rest_art[3];
static SmallFont g_book_font[5];
static const df_u8 *g_spell_icons, *g_spell_masks;
static CifSet g_container_icons;
static df_u8 g_doll[110 * 184], g_doll_back[110 * 184];
static int g_panel_files[64], g_panel_file_count;
static int g_possession_page;
static unsigned g_possession_revision;
static void load_possession_art(void);
static void ensure_biog(int klass);
static void ensure_faces(int race, int female);

static void why(const char *text)
{
    df_copy(g_why, text, (int)sizeof(g_why));
}

const char *seam_error(void)
{
    return g_why;
}

static void play_sound(int id)
{
    const SoundClip *clip;

    /* FALL.EXE passes DAGGER.SND record IDs, not DFU's directory positions. */
    clip = sndfile_clip(sndfile_by_id((df_u32)id));
    if (clip) {
        sound_play(clip, 1.0f, 0.0f, 0);
    }
}

static void play_sound_at(int id, float x, float y, float z)
{
    const SoundClip *clip;
    clip = sndfile_clip(sndfile_by_id((df_u32)id));
    if (clip) { sound_play_at(clip,x,y,z,128,1024,1,0); }
}

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
        log_print(LOG_WARN, "ogfall: %s did not load", name);
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
        log_print(LOG_WARN, "ogfall: %s did not load", name);
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
        log_print(LOG_WARN, "ogfall: MAP100I0.IMG did not load");
        return;
    }
    id = pic_load(pic.pixels, pic.width, pic.height, rgb, 0);
    og_pic_set(OG_PIC_BACK, id);
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
        og_pic_set(OG_PIC_MAP, id);
        return;
    }
    mem_copy(composed, pic.pixels, bytes);
    paint_prompt(composed, pic.width, pic.height);
    g_map_draw = composed;
    og_pic_set(OG_PIC_MAP, pic_load(composed, pic.width, pic.height, rgb, 0));
}

static void load_classes(void)
{
    int i;

    og_class_clear();
    for (i = 0; i < 18; i++) {
        char file[16];
        const df_u8 *b;
        df_u32 n;
        ClassCfg cfg;
        OgClass row;
        int k;

        classcfg_name(i, file);
        b = vfs_read(file, &n);
        if (!b || !classcfg_read(b, n, &cfg)) {
            continue;
        }
        mem_zero(&row, sizeof(row));
        df_copy(row.name, cfg.name, 16);
        row.name[15] = 0;
        for (k = 0; k < OG_ATTRS; k++) {
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
        og_class_set(i, &row);
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
        og_question_set(number, text + i);
    }
    answers = vfs_read("CLASSES.DAT", &size);
    if (answers && size >= 216) { og_class_answers_set(answers + 18); }
}

static void load_compass(void)
{
    df_u8 rgb[768];
    int i;
    if (!img_load_bss("CMPA00I0.BSS", &g_compass_data) || !img_art_palette(rgb)) { return; }
    g_compass_pic = pic_load(g_compass_data.pixels, g_compass_data.width, g_compass_data.height, rgb, 1);
    g_compass_frame = 0;
    for (i = 0; i < g_compass_data.frames && i < 32; i++) {
        og_compass_set(i, g_compass_pic, g_compass_data.width, g_compass_data.height);
    }
}

static void update_compass(void)
{
    int frame;
    frame = og_hud_compass_frame(og_move_yaw());
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
        if (frame) { og_pic_set(OG_PIC_CURSOR0 + i, pic_load(frame->pixels, frame->w, frame->h, rgb, 1)); }
    }
}

/* BUTTONS.RCI is 32 x 16 frames, DFU's order. Yes is 3, No 4, Male 6, Female 7. */
static void load_choice_buttons(void)
{
    const df_u8 *b;
    df_u32 n;
    df_u8 rgb[768];
    static const int which[4] = { 3, 4, 6, 7 };
    static const int slot[4] = { OG_PIC_YES, OG_PIC_NO, OG_PIC_MALE, OG_PIC_FEMALE };
    int i;

    b = vfs_read("BUTTONS.RCI", &n);
    g_buttons = b;
    if (!b || !img_art_palette(rgb)) {
        log_print(LOG_WARN, "ogfall: BUTTONS.RCI did not load");
        return;
    }
    for (i = 0; i < 4; i++) {
        int off;

        off = which[i] * 32 * 16;
        if (off + 32 * 16 > (int)n) {
            continue;
        }
        og_pic_set(slot[i], pic_load(b + off, 32, 16, rgb, 0));
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
        log_print(LOG_WARN, "ogfall: SPOP.RCI did not load");
    }
    g_have_art_rgb = img_art_palette(g_art_rgb);
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
    /* Low health is frame 2, the only overlay slice 1 can show. */
    frame = cif_frame(&g_overlay, 2);
    if (!frame && g_overlay.n > 0) {
        frame = cif_frame(&g_overlay, 0);
    }
    if (!frame) {
        return;
    }
    og_pic_set(OG_PIC_OVERLAY, pic_load(frame->pixels, frame->w, frame->h, rgb, 1));
}

int seam_map_pixel(int x, int y)
{
    if (!g_map || x < 0 || y < 0 || x >= g_map_w || y >= g_map_h) {
        return 0;
    }
    return g_map[y * g_map_w + x];
}

/* career_background_summary/career_apply_answer, through the existing BIOG reader.
 * The callback exposes plain character fields; OGFALL never includes dagger. */
static int background(int klass, int question, int answer)
{
    const BiogAnswer *a;
    OgCharacter *who;
    int i;

    if (klass == 18) { klass = og_nearest_class(18); }
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
    who = og_character();
    for (i = 0; i < a->effects; i++) {
        const BiogEffect *e;
        e = &a->effect[i];
        if (e->kind == BIOG_SKILL && e->a >= 0 && e->a < OG_SKILLS) {
            /* Applied before zero skills get their 3..6 roll in chargen_name_character. */
            who->skill[e->a] += e->b;
        } else if (e->kind == BIOG_GOLD) {
            who->gold += e->a;
        } else if (e->kind == BIOG_SOCIAL && e->a >= 0 && e->a < 10) {
            who->reputation[e->a] += e->b;
        } else if (e->kind == BIOG_REACTION) {
            who->reaction += e->a;
        } else if (e->kind == BIOG_ITEM && who->gifts < 32) {
            who->gift[who->gifts].group = e->a;
            who->gift[who->gifts].index = e->b;
            who->gift[who->gifts].material = e->c;
            who->gifts++;
        }
    }
    return 1;
}

int seam_boot(void)
{
    ImgPicture raw;
    df_u8 rgb[768];

    g_why[0] = 0;
    ogfall_formats_startup();
    ogfall_tables_startup();
    ogfall_place_startup();
    ogfall_play_startup();
    og_set_sound(play_sound);
    og_set_sound_at(play_sound_at);
    og_weapon_set_draw(weapon_draw);
    og_set_save_capture(seam_capture_world);
    og_set_words(textrsc_read);
    og_set_word_variants(textrsc_variant);
    if (!maindat_start()) {
        why(maindat_error());
        return 0;
    }
    dagger_bind_loaders();
    load_classes();
    load_class_questions();
    og_chargen_background(background);
#ifndef DF_HOST
    fnt_install();
#endif
    smallfont_load(&g_font, "FONT0003.FNT");
    og_pic_set(OG_PIC_TITLE, menu_picture("PICK03I0.IMG", &g_title_art));
    load_click_map("TAMRIEL2.IMG");
    load_province_map();
    og_pic_set(OG_PIC_BUTTONS, menu_picture("BUTN01I0.IMG", &g_class_art));
    load_choice_buttons();
    load_chargen_screens();
    load_panels();
    load_possession_art();
    if (img_load_raw("ARROW.RAW", 10, 10, &raw) && img_art_palette(rgb)) {
        og_pic_set(OG_PIC_ARROW, pic_load(raw.pixels, raw.width, raw.height, rgb, 1));
    }

    if (!og_pic(OG_PIC_TITLE)) {
        why("PICK03I0.IMG");
        return 0;
    }
    return 1;
}

void seam_title(void)
{
    static const int which[4] = {3,4,6,7};
    static const int slots[4] = {OG_PIC_YES,OG_PIC_NO,OG_PIC_MALE,OG_PIC_FEMALE};
    int i;
    for (i = 0; i < 3; i++) {
        if (g_comp[i]) { pic_free(g_comp[i]); g_comp[i] = 0; }
    }
    /* Re-upload retained menu art; repeated deaths allocate no new sources. */
    og_pic_set(OG_PIC_TITLE,menu_picture("PICK03I0.IMG",&g_title_art));
    og_pic_set(OG_PIC_BACK,pic_load(g_land,g_land_w,g_land_h,g_map_rgb,0));
    og_pic_set(OG_PIC_MAP,pic_load(g_map_draw,g_land_w,g_land_h,g_map_rgb,0));
    og_pic_set(OG_PIC_BUTTONS,menu_picture("BUTN01I0.IMG",&g_class_art));
    if (g_buttons) {
        for (i = 0; i < 4; i++) { og_pic_set(slots[i],pic_load(g_buttons+which[i]*512,32,16,g_art_rgb,0)); }
    }
    g_present_mode = -1; g_possession_page = 0; g_last_panel = -1;
    og_pic_set(OG_PIC_SCREEN,0);
    music_stop();
}

static void marker6(float *x, float *y, float *z)
{
    Dungeon hold;
    Bsa *blocks;
    OgMark marks[MARKS];
    int n;
    int b;
    float nx;
    float ny;
    float nz;

    n = 0;
    if (!dungeon_load(&hold)) {
        return;
    }
    blocks = bsa_open("BLOCKS.BSA");
    if (!blocks) {
        return;
    }
    for (b = 0; b < hold.nblocks; b++) {
        const BsaRec *rec;
        df_u8 *bytes;
        Marker tmp[128];
        int k;
        int i;

        rec = bsa_find_name(blocks, hold.block[b].name);
        if (!rec) {
            continue;
        }
        bytes = bsa_read_scratch(blocks, rec, BSA_SCRATCH_BLOCK);
        if (!bytes) {
            continue;
        }
        k = rdb_markers(bytes, rec->size, tmp, 128);
        for (i = 0; i < k && n < MARKS; i++) {
            marks[n].record = tmp[i].record;
            marks[n].x = tmp[i].x + (float)(hold.block[b].x * TILE);
            marks[n].y = tmp[i].y;
            marks[n].z = tmp[i].z + (float)(hold.block[b].z * TILE);
            n++;
        }
    }
    bsa_close(blocks);
    if (og_place_marker6(marks, n, *x, *y, *z, &nx, &ny, &nz)) {
        *x = nx;
        *y = ny;
        *z = nz;
    }
}

#define HOLD_TREASURES 128
#define HOLD_COMBAT_IMAGES (OG_FOES * 26)
static Keep g_entity_keeps[OG_WORLD_ROWS];
static int g_entity_keep_count;
static Image *const *g_world_images;
static int g_world_image_count;
static Flat *g_hold_flats;
static Usable *g_hold_usables;
static int g_hold_flat_base, g_hold_use_base, g_hold_capacity;
static int g_hold_loot_table, g_hold_pile_slot;
static df_u32 g_hold_pile_serial;
static Mesh *g_arrow_mesh;
static Instance *g_arrow_models;
static Bound *g_arrow_bounds;
static Span *g_arrow_spans;
static int g_arrow_base, g_arrow_span;

static void hold_arrow_model(Image **images, df_u8 *cutout, int *slots)
{
    Bsa *archive;
    const BsaRec *record;
    const df_u8 *bytes;
    Tri *tri;
    Image *image;
    int i, j, slot, count;
    g_arrow_mesh = 0;
    archive = bsa_open("ARCH3D.BSA");
    if (!archive) { return; }
    record = bsa_find_id(archive,99800);
    bytes = record ? bsa_read_scratch(archive,record,BSA_SCRATCH_MODEL) : 0;
    if (bytes) { g_arrow_mesh = arch3d_load(bytes,record->size,99800); }
    bsa_close(archive);
    if (!g_arrow_mesh) { return; }
    for (i = 0; i < g_arrow_mesh->ntris; i++) {
        tri = &g_arrow_mesh->tris[i]; slot = -1;
        for (j = 0; j < i; j++) {
            if (g_arrow_mesh->tris[j].tex_file == tri->tex_file && g_arrow_mesh->tris[j].tex_image == tri->tex_image) {
                slot = g_arrow_mesh->tris[j].slot; break;
            }
        }
        if (slot < 0) {
            image = texture_load(tri->tex_file,tri->tex_image);
            if (image) { slot = (*slots)++; images[slot] = image; cutout[slot] = 0; }
        }
        tri->slot = slot >= 0 ? (df_u16)slot : 0xffff;
    }
    g_arrow_base = g_level.scene.nmodels; count = g_arrow_base+OG_ARROWS;
    g_arrow_models = (Instance *)mem_alloc(count*sizeof(Instance));
    g_arrow_bounds = (Bound *)mem_alloc(count*sizeof(Bound));
    g_arrow_spans = (Span *)mem_alloc((g_level.scene.nspans+1)*sizeof(Span));
    if (!g_arrow_models || !g_arrow_bounds || !g_arrow_spans) { g_arrow_mesh = 0; return; }
    mem_zero(g_arrow_models,count*sizeof(Instance)); mem_zero(g_arrow_bounds,count*sizeof(Bound));
    mem_copy(g_arrow_models,g_level.scene.models,g_arrow_base*sizeof(Instance));
    for (i = 0; i < g_arrow_base; i++) {
        if (g_level.scene.bounds) { g_arrow_bounds[i] = g_level.scene.bounds[i]; }
        else { instance_world_box(&g_arrow_models[i],&g_arrow_bounds[i]); }
    }
    for (i = g_arrow_base; i < count; i++) { g_arrow_bounds[i].ex = -1; }
    /* Movers must write the same instances that rendering and collision read. */
    for (i = 0; i < g_level.scene.nmovers; i++) {
        if (g_level.scene.movers[i].model >= 0 && g_level.scene.movers[i].model < g_arrow_base) {
            g_level.scene.movers[i].inst = &g_arrow_models[g_level.scene.movers[i].model];
        }
    }
    g_arrow_span = -1;
    if (g_level.scene.nspans && g_level.scene.spans) {
        mem_copy(g_arrow_spans,g_level.scene.spans,g_level.scene.nspans*sizeof(Span));
        g_arrow_span = g_level.scene.nspans++;
        mem_zero(&g_arrow_spans[g_arrow_span],sizeof(Span));
        g_arrow_spans[g_arrow_span].first = g_arrow_base; g_arrow_spans[g_arrow_span].count = OG_ARROWS;
        g_arrow_spans[g_arrow_span].ex = g_arrow_spans[g_arrow_span].ey = g_arrow_spans[g_arrow_span].ez = 4096;
        g_level.scene.spans = g_arrow_spans;
    }
    g_level.scene.models = g_arrow_models; g_level.scene.bounds = g_arrow_bounds;
    g_level.scene.nmodels = count; g_level.scene.solid_models = g_arrow_base;
}

static void hold_arrows(const Camera *cam)
{
    const OgArrow *arrow;
    Instance *model;
    float horizontal;
    int i;
    if (!g_arrow_mesh) { return; }
    if (g_arrow_span >= 0 && cam) {
        g_arrow_spans[g_arrow_span].cx = cam->x;
        g_arrow_spans[g_arrow_span].cy = cam->y;
        g_arrow_spans[g_arrow_span].cz = cam->z;
    }
    for (i = 0; i < OG_ARROWS; i++) {
        arrow = og_dungeon_arrow(i); model = &g_arrow_models[g_arrow_base+i];
        model->mesh = arrow->live ? g_arrow_mesh : 0;
        if (!arrow->live) { g_arrow_bounds[g_arrow_base+i].ex = -1; continue; }
        model->x = arrow->at[0]; model->y = arrow->at[1]; model->z = arrow->at[2];
        horizontal = df_sqrt(arrow->direction[0]*arrow->direction[0]+arrow->direction[2]*arrow->direction[2]);
        instance_set_rotation(model,-df_atan2(arrow->direction[1],horizontal),df_atan2(arrow->direction[0],arrow->direction[2]),0);
        instance_world_box(model,&g_arrow_bounds[g_arrow_base+i]);
    }
}

static CifSet g_weapon_art;
static int g_weapon_file = -1, g_weapon_pic, g_weapon_frame = -1;
static int g_weapon_color = -1, g_weapon_hand = -1, g_weapon_bar = -1;
static int g_hurt_pic;
static df_u8 *g_weapon_pixels;

const df_u8 *seam_weapon_screen(const df_u8 **palette)
{
    if (palette) { *palette = g_art_rgb; }
    return g_weapon_pixels;
}

static void weapon_draw(const OgWeaponView *view, float flash)
{
    const CifFrame *frame;
    df_u8 red[768], pixel;
    char name[20];
    int first, count, at, x, y, px, py, bar;
    if (view->visible) {
        if (g_weapon_file != view->cif) {
            cif_close(&g_weapon_art);
            log_format(name,sizeof(name),"WEAPON%02d.CIF",view->cif);
            g_weapon_file = cif_open(name,&g_weapon_art) ? view->cif : -1;
            g_weapon_frame = -1;
        }
        count = cif_group_span(&g_weapon_art,view->cif == 9 ? 1 : view->group,&first);
        at = count ? first + (view->frame < count ? view->frame : count-1) : -1;
        frame = cif_frame(&g_weapon_art,at);
        bar = !(og_options_flags() & 1);
        if (!g_weapon_pixels) { g_weapon_pixels = (df_u8 *)mem_keep(64000); }
        if (frame && g_weapon_pixels && (at != g_weapon_frame || view->color != g_weapon_color ||
            view->hand != g_weapon_hand || bar != g_weapon_bar || !g_weapon_pic)) {
            mem_zero(g_weapon_pixels,64000);
            for (y = 0; y < frame->h; y++) {
                py = frame->y+y-(bar ? 46 : 0);
                if (py < 0 || py >= 200) { continue; }
                for (x = 0; x < frame->w; x++) {
                    px = frame->x+x;
                    if (view->hand) { px = 319-px; }
                    if (px < 0 || px >= 320) { continue; }
                    pixel = frame->pixels[y*frame->w+x];
                    g_weapon_pixels[py*320+px] = view->color ? (df_u8)og_item_ink(view->color,pixel) : pixel;
                }
            }
            if (!g_weapon_pic) { g_weapon_pic = pic_load(g_weapon_pixels,320,200,g_art_rgb,1); }
            else { pic_update(g_weapon_pic,g_weapon_pixels); }
            g_weapon_frame = at; g_weapon_color = view->color; g_weapon_hand = view->hand; g_weapon_bar = bar;
        }
        if (frame) { og_hud_blit(g_weapon_pic,0,0,320,200); }
    }
    if (flash > 0) {
        if (!g_hurt_pic) {
            mem_zero(red,sizeof(red)); red[3] = 192; pixel = 1;
            g_hurt_pic = pic_load(&pixel,1,1,red,0);
        }
        pic_draw_part(g_hurt_pic,0,0,1,1,0,df_view_h(),df_view_w(),df_view_h(),(int)(flash*120));
    }
}

/* Original TEXTURE record header +22 is the ASCR step divisor in ms. */
static int creature_rate(int archive)
{
    VfsFile *file;
    char name[16];
    df_u8 bytes[4];
    df_u32 record;
    int rate;
    texture_filename(archive,name,sizeof(name));
    file = vfs_open(name); rate = 100;
    if (file) {
        if (vfs_read_at(file,26+5*20+2,bytes,4)) {
            record = df_ru32(bytes);
            if (vfs_read_at(file,record+22,bytes,2) && df_ru16(bytes)) { rate = df_ru16(bytes); }
        }
        vfs_close(file);
    }
    return rate;
}

int seam_creature_archive(int index) { return world_entity_archive(g_world,index); }

static void hold_creature_art(Image **images, df_u8 *cutout, int *slots)
{
    Entity *e;
    Image *image;
    EntityPose *pose;
    int i, j, p, previous, kind, archive, file, record, slot;
    static const int acts[5] = {ENTITY_ATTACK,ENTITY_HURT,ENTITY_CAST,ENTITY_SHOOT,ENTITY_DEAD};
    static const int records[5] = {5,10,20,25,0};
    for (i = 0; i < g_level.scene.nentities; i++) {
        e = (Entity *)&g_level.scene.entities[i];
        kind = world_entity_kind(g_world,i,0);
        archive = world_entity_archive(g_world,i);
        previous = -1;
        for (j = 0; j < i; j++) {
            if (world_entity_kind(g_world,j,0) == kind && world_entity_archive(g_world,j) == archive) { previous = j; break; }
        }
        if (previous >= 0) {
            for (p = 0; p < 5; p++) { e->pose[acts[p]] = g_level.scene.entities[previous].pose[acts[p]]; }
            continue;
        }
        for (p = 0; p < 5; p++) {
            pose = &e->pose[acts[p]]; pose->rate = 10;
            for (j = 0; j < (p == 4 ? 1 : 5); j++) {
                file = archive; record = records[p]+j;
                if (p == 4) { og_foe_corpse(kind,&file,&record); }
                image = texture_load(file,record);
                if (!image) { continue; }
                if (image->frames <= 1 || !texture_sheet(image,file,record)) { texture_pad(image); }
                slot = (*slots)++; images[slot] = image; cutout[slot] = 1;
                pose->slot[j] = (df_u16)slot;
                pose->w[j] = (df_u16)texture_extent(image->width,image->xscale);
                pose->h[j] = (df_u16)texture_extent(image->height,image->yscale);
            }
        }
    }
}

static void treasure_usable(Usable *out, const Flat *flat, int sprite, df_u32 key, int table)
{
    mem_zero(out,sizeof(*out));
    out->kind = USE_OTHER; out->model = -1; out->sprite = sprite; out->mover = -1;
    out->nz = 1;
    out->cx = flat->x; out->cy = flat->y; out->cz = flat->z;
    out->hw = flat->w / 2; out->hh = flat->h / 2; out->hd = 12;
    if (out->hw < 12) { out->hw = 12; }
    if (out->hh < 12) { out->hh = 12; }
    out->reach = 128;
    out->what = OG_USE_TREASURE + table + 1;
    out->tag = (int)key;
    out->label = "Treasure";
}

static void hold_dropped_piles(void)
{
    const OgObject *container;
    Flat *flat;
    int i, n, uses, changed;
    if (!g_world || !g_hold_flats || g_hold_pile_serial == og_state()->event_serial) { return; }
    g_hold_pile_serial = og_state()->event_serial;
    n = g_hold_flat_base; uses = g_hold_use_base; changed = 0;
    for (i = 0; i < g_level.scene.nentities; i++) {
        const Entity *e;
        Flat corpse;
        e = &g_level.scene.entities[i];
        if (!e->dead || !e->live) { continue; }
        mem_zero(&corpse,sizeof(corpse));
        corpse.x = e->x; corpse.y = e->y+12; corpse.z = e->z;
        corpse.w = 64; corpse.h = 32;
        treasure_usable(&g_hold_usables[uses],&corpse,-1,e->key.thing,og_dungeon_loot_table(i));
        g_hold_usables[uses++].label = "Corpse";
    }
    for (i = 0; i < OG_OBJECTS && n < g_hold_capacity && g_hold_pile_slot >= 0; i++) {
        container = &og_state()->object[i];
        if (!container->id || container->type != OG_RECORD_CONTAINER || container->parent != OG_WORLD_ROOT ||
            container->place_key != og_object(OG_WORLD_ROOT)->place_key || !(container->thing_key & 0x80000000u) ||
            !og_inventory_rows(container->id,-1,0,0)) { continue; }
        flat = &g_hold_flats[n];
        if (n >= g_level.scene.nsprites || flat->x != container->x || flat->y != container->y ||
            flat->z != container->z || (df_u32)g_hold_usables[uses].tag != container->thing_key) { changed = 1; }
        mem_zero(flat,sizeof(*flat));
        flat->x = container->x; flat->y = container->y; flat->z = container->z;
        flat->w = flat->h = 24; flat->file = 216; flat->image = 0;
        flat->slot = (df_u16)g_hold_pile_slot;
        treasure_usable(&g_hold_usables[uses++],flat,n,container->thing_key,-1);
        n++;
    }
    if (changed || n != g_level.scene.nsprites || uses != g_level.scene.nusables) { g_level.scene.version++; }
    g_level.scene.nsprites = n; g_level.scene.nusables = uses;
}

static int hold_treasures(void)
{
    Dungeon hold;
    Bsa *blocks;
    const BsaRec *rec;
    const df_u8 *bytes;
    Marker marks[128];
    Image **images;
    df_u8 *cutout, *glass;
    Image *image;
    int i, b, k, count, uses, slot;
    if (!dungeon_load(&hold)) { return 0; }
    og_object_edit(OG_WORLD_ROOT)->place_key = 0x40000000u | (df_u32)hold.location_id;
    g_hold_loot_table = og_dungeon_loot(hold.dungeon_type);
    g_hold_capacity = g_level.scene.nsprites + HOLD_TREASURES + OG_OBJECTS;
    g_hold_flats = (Flat *)mem_alloc(g_hold_capacity * sizeof(Flat));
    g_hold_usables = (Usable *)mem_alloc((g_level.scene.nusables+g_hold_capacity)*sizeof(Usable));
    images = (Image **)mem_alloc((g_level.nimages+HOLD_COMBAT_IMAGES+HOLD_TREASURES+4)*sizeof(Image *));
    cutout = (df_u8 *)mem_alloc(g_level.nimages+HOLD_COMBAT_IMAGES+HOLD_TREASURES+4);
    glass = (df_u8 *)mem_alloc(g_level.nimages+HOLD_COMBAT_IMAGES+HOLD_TREASURES+4);
    if (!g_hold_flats || !g_hold_usables || !images || !cutout || !glass) { return 0; }
    count = g_level.scene.nsprites; uses = g_level.scene.nusables;
    mem_copy(g_hold_flats,g_level.scene.sprites,count*sizeof(Flat));
    mem_copy(g_hold_usables,g_level.scene.usables,uses*sizeof(Usable));
    mem_copy(images,g_level.images,g_level.nimages*sizeof(Image *));
    mem_copy(cutout,g_level.cutout,g_level.nimages);
    mem_zero(glass,g_level.nimages+HOLD_COMBAT_IMAGES+HOLD_TREASURES+4);
    if (g_level.glass) { mem_copy(glass,g_level.glass,g_level.nimages); }
    g_hold_pile_slot = -1;
    for (i = 0; i < count; i++) {
        if (g_hold_flats[i].file != 216) { continue; }
        treasure_usable(&g_hold_usables[uses++],&g_hold_flats[i],i,0x100000u+i,g_hold_loot_table);
        if (g_hold_flats[i].image == 0) { g_hold_pile_slot = g_hold_flats[i].slot; }
    }
    g_world_images = g_level.images;
    g_world_image_count = g_level.nimages;
    slot = g_level.nimages;
    hold_creature_art(images,cutout,&slot);
    hold_arrow_model(images,cutout,&slot);
    if (g_hold_pile_slot < 0) {
        image = texture_load(216,0);
        if (image) { texture_pad(image); images[slot] = image; cutout[slot] = 1; g_hold_pile_slot = slot++; }
    }
    {
        int blood[2];
        for (i = 0; i < 2; i++) {
            blood[i] = -1; image = texture_load(380,i ? 2 : 0);
            if (image) { texture_pad(image); blood[i] = slot; images[slot] = image; cutout[slot++] = 1; }
        }
        og_dungeon_blood(blood[0],blood[1]);
    }
    blocks = bsa_open("BLOCKS.BSA");
    if (!blocks) { return 0; }
    collide_use(&g_level.scene);
    for (b = 0; b < hold.nblocks && count < g_hold_capacity-OG_OBJECTS; b++) {
        rec = bsa_find_name(blocks,hold.block[b].name);
        bytes = rec ? bsa_read_scratch(blocks,rec,BSA_SCRATCH_BLOCK) : 0;
        k = bytes ? rdb_markers(bytes,rec->size,marks,128) : 0;
        for (i = 0; i < k && count < g_hold_capacity-OG_OBJECTS; i++) {
            Flat *flat;
            if (marks[i].record != RDB_MARKER_TREASURE || g_hold_pile_slot < 0) { continue; }
            flat = &g_hold_flats[count]; mem_zero(flat,sizeof(*flat));
            flat->x = marks[i].x + hold.block[b].x*TILE;
            flat->y = marks[i].y; flat->z = marks[i].z + hold.block[b].z*TILE;
            flat->file = 216; flat->image = 0; flat->slot = g_hold_pile_slot;
            image = images[g_hold_pile_slot];
            flat->w = (float)texture_extent(image->width,image->xscale);
            flat->h = (float)texture_extent(image->height,image->yscale);
            {
                float floor;
                collide_gather(flat->x-1,flat->y-2048,flat->z-1,flat->x+1,flat->y+16,flat->z+1);
                if (collide_floor(flat->x,flat->z,flat->y+16,flat->y-2048,&floor)) {
                    flat->y = floor + flat->h/2;
                }
            }
            treasure_usable(&g_hold_usables[uses++],flat,count,((df_u32)(b+1)<<24)|marks[i].pos,g_hold_loot_table);
            count++;
        }
    }
    bsa_close(blocks);
    g_level.images = images; g_level.cutout = cutout; g_level.glass = glass; g_level.nimages = slot;
    g_level.scene.sprites = g_hold_flats; g_level.scene.usables = g_hold_usables;
    /* Appended sprites have no block spans: use the engine's whole-list path. */
    g_level.scene.sprite_spans = 0;
    g_hold_flat_base = count; g_hold_use_base = uses;
    g_hold_pile_serial = og_state()->event_serial-1;
    hold_dropped_piles();
    return 1;
}

int seam_open_hold(float *x, float *y, float *z)
{
    float sx;
    float sy;
    float sz;
    float yaw;
    float pitch;

    {
        static const int menus[] = { OG_PIC_TITLE, OG_PIC_BACK, OG_PIC_MAP, OG_PIC_BUTTONS,
            OG_PIC_YES, OG_PIC_NO, OG_PIC_MALE, OG_PIC_FEMALE };
        int i;
        /* Menu pictures give back the card before the hold and its HUD load. */
        for (i = 0; i < 3; i++) {
            if (g_comp[i]) { pic_free(g_comp[i]); g_comp[i] = 0; }
        }
        og_pic_set(OG_PIC_SCREEN, 0);
        for (i = 0; i < (int)(sizeof(menus) / sizeof(menus[0])); i++) {
            pic_free(og_pic(menus[i])); og_pic_set(menus[i], 0);
        }
    }
    if (!g_hud_loaded) {
        og_pic_set(OG_PIC_BAR, load_img("MAIN00I0.IMG", 0));
        og_pic_set(OG_PIC_MODES, load_img("MAIN01I0.IMG", 0));
        og_pic_set(OG_PIC_HEALTH, load_img("MAIN03I0.IMG", 0));
        og_pic_set(OG_PIC_FATIGUE, load_img("MAIN04I0.IMG", 0));
        og_pic_set(OG_PIC_MAGICKA, load_img("MAIN05I0.IMG", 0));
        og_pic_set(OG_PIC_COMPASS_WIDE, load_img("COMPASS.IMG", 0));
        og_pic_set(OG_PIC_COMPBOX, load_img("COMPBOX.IMG", 0));
        og_pic_set(OG_PIC_OPTIONS, load_img("OPTN00I0.IMG", 0));
        load_compass();
        load_overlay();
        g_hud_loaded = 1;
    }
    seam_close_hold();
    if (!og_state_restore_world() || !mem_temp_begin()) {
        why("No memory for the saved place");
        return 0;
    }
    /* The borrowed builder suppresses dead mobiles. OGFALL keeps their bodies
     * drawable, so restore entity state after it has built every sprite slot. */
    {
        int i;
        g_entity_keep_count = 0;
        for (i = keep_count()-1; i >= 0; i--) {
            if (keep_at(i)->kind == KEEP_ENTITY) {
                g_entity_keeps[g_entity_keep_count] = *keep_at(i);
                keep_forget(g_entity_keeps[g_entity_keep_count++].key);
            }
        }
    }
    g_world = world_load();
    {
        int i;
        for (i = 0; i < g_entity_keep_count; i++) { keep_put(&g_entity_keeps[i]); }
    }
    if (!g_world) {
        why(world_error()); texture_cache_clear(); mem_temp_end(); return 0;
    }
    world_as_level(g_world, &g_level);
    keep_restore(&g_level.scene,0,0,(Entity *)g_level.scene.entities,g_level.scene.nentities,0,0,0);
    {
        int kind[OG_FOES], passive[OG_FOES], rates[OG_FOES], i;
        for (i = 0; i < g_level.scene.nentities && i < OG_FOES; i++) {
            kind[i] = world_entity_kind(g_world,i,&passive[i]);
            rates[i] = creature_rate(world_entity_archive(g_world,i));
        }
        og_dungeon_open(&g_level.scene,kind,passive,rates);
        for (i = 0; i < g_level.scene.nentities && i < OG_FOES; i++) {
            if (kind[i] >= 128 && kind[i] < 146) {
                og_dungeon_variant(i,world_entity_archive(g_world,i)-432);
            }
        }
    }
    if (!hold_treasures()) { why("Cannot build the Hold treasure containers"); return 0; }
    {
        Dungeon hold;
        if (og_state()->region < 0 && dungeon_load(&hold)) {
            og_state_set_place(hold.region, hold.index, 1);
        }
    }
    world_start(g_world, &sx, &sy, &sz, &yaw, &pitch);
    marker6(&sx, &sy, &sz);
    if (x) {
        *x = sx;
    }
    if (y) {
        *y = sy;
    }
    if (z) {
        *z = sz;
    }
    return 1;
}

void seam_close_hold(void)
{
    if (!g_world) { return; }
    og_dungeon_close();
    g_arrow_mesh = 0; g_arrow_models = 0; g_arrow_bounds = 0; g_arrow_spans = 0;
    og_weapon_cancel();
    cif_close(&g_weapon_art); g_weapon_file = -1;
    if (g_weapon_pic) { pic_free(g_weapon_pic); g_weapon_pic = 0; }
    g_weapon_frame = -1;
    sound_stop_all();
    texture_cache_clear();
    g_world = 0;
    g_hold_flats = 0; g_hold_usables = 0;
    mem_zero(&g_level, sizeof(g_level));
    mem_temp_end();
}

int seam_capture_world(void)
{
    if (!g_world) { return 0; }
    keep_save(&g_level.scene,0,g_level.scene.nmovers,g_level.scene.entities,g_level.scene.nentities,0,0,0);
    return og_state_capture_world();
}

void seam_creatures(const Camera *cam)
{
    if (!g_world) {
        return;
    }
    hold_dropped_piles();
    hold_arrows(cam);
    world_creatures_step(g_world, cam, 2048.0f, 2.0f);
    {
        int i;
        for (i = 0; i < g_level.scene.nentities; i++) {
            if (og_dungeon_loaded(i) && g_level.scene.entities[i].dead) { g_hold_pile_serial--; }
        }
    }
    hold_dropped_piles();
    /* The world fills reserved creature picture slots as they stream in. */
    if (g_world_images) { mem_copy((void *)g_level.images,g_world_images,g_world_image_count*sizeof(Image *)); }
}

Level *seam_level(void)
{
    return g_world ? &g_level : 0;
}

static void put_px(int x, int y, int colour)
{
    if (!g_buf || x < 0 || y < 0 || x >= 320 || y >= 200) {
        return;
    }
    g_buf[y * 320 + x] = (df_u8)colour;
}

static void blit_ix(int dx, int dy, const df_u8 *src, int sw, int sh, int key)
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
            put_px(dx + col, dy + row, colour);
        }
    }
}

static void cover_screen(const df_u8 *src, int w, int h)
{
    if (!g_buf) {
        return;
    }
    if (src && w == 320 && h == 200) {
        mem_copy(g_buf, src, 64000);
        return;
    }
    mem_zero(g_buf, 64000);
    if (src) {
        blit_ix(0, 0, src, w, h, 0);
    }
}

static void lay_map(void)
{
    cover_screen(g_land, g_land_w, g_land_h);
}

/* SPOP.RCI's nine 22 x 22 tiles, the same corners msgbox_render uses. */
static void lay_paper(int x0, int y0, int cols, int rows)
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
            blit_ix(x0 + col * 22, y0 + row * 22, g_spop + piece * 22 * 22, 22, 22, 0);
        }
    }
}

static void lay_button(int frame, int x, int y)
{
    if (!g_buttons || frame < 0) {
        return;
    }
    blit_ix(x, y, g_buttons + frame * 32 * 16, 32, 16, 0);
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
 * then sits at og_box_button_y, which is text_y + lines * 7 + 4.
 */
static void paper_box(char lines[][96], int n, int buttons, int *x0, int *y0, int *cols, int *rows, int *text_y)
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

static void paint_lines(char lines[][96], int n, int centered, int x0, int text_y, int ink)
{
    int i;
    int y;

    y = text_y;
    for (i = 0; i < n; i++) {
        int x;

        x = centered ? 160 - smallfont_width(&g_font, lines[i]) / 2 : x0 + 8;
        smallfont_draw(&g_font, g_buf, 320, 200, x, y, ink, 156, lines[i]);
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
    og_chargen_set_lines(n);
    paper_box(lines, n, buttons, &x0, &y0, &cols, &rows, &text_y);
    lay_map();
    lay_paper(x0, y0, cols, rows);
    paint_lines(lines, n, centered, x0, text_y, 146);
    if (buttons) {
        int by;

        by = og_chargen_choice_y();
        lay_button(left_frame, OG_CHOICE_X0, by);
        lay_button(right_frame, OG_CHOICE_X1, by);
    }
}

/* picklist_update_thumb/picklist_draw, including the original bevel colours. */
static void paint_scrollbar(int top, int count)
{
    int y, h, row, col, ink;
    og_chargen_scroll_thumb(top, count, &y, &h);
    for (row = 0; row < h; row++) {
        for (col = 0; col < 5; col++) {
            ink = col == 0 || row == h - 1 ? 123 : (col == 4 || row == 0 ? 112 : 114);
            put_px(241 + col, y + row, ink);
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
        blit_ix(g_pick.x, g_pick.y, g_pick.pixels, g_pick.width, g_pick.height, 0);
    }
    top = og_chargen_list_top();
    sel = og_chargen_list_sel();
    paint_scrollbar(top, OG_LIST_N);
    shown = OG_LIST_VIS;
    for (i = 0; i < shown; i++) {
        int row;
        const char *name;
        int ink;

        row = top + i;
        if (row >= OG_LIST_N) {
            break;
        }
        if (row == 18) {
            name = "Custom";
        } else {
            const OgClass *klass;

            klass = og_class(row);
            name = klass ? klass->name : "";
        }
        ink = row == sel ? 244 : 146;
        /* picklist_draw: shadow one down-right, then the line at x+1, y starting at 1. */
        smallfont_draw(&g_font, g_buf, 320, 200, OG_LIST_X + 1, OG_LIST_Y + 1 + i * OG_LIST_ROW, ink, 156, name);
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
        cover_screen(g_biog_pic.pixels, g_biog_pic.width, g_biog_pic.height);
    } else {
        lay_map();
    }
    ensure_biog(og_character()->klass == 18 ? og_nearest_class(18) : og_character()->klass);
    if (!g_biog_ok) {
        return;
    }
    q = og_chargen_question();
    if (q < 0 || q >= g_biog.questions) {
        return;
    }
    question = &g_biog.question[q];
    smallfont_draw(&g_font, g_buf, 320, 200, 31, 23, 145, 141, question->line[0]);
    smallfont_draw(&g_font, g_buf, 320, 200, 31, 33, 145, 141, question->line[1]);
    for (i = 0; i < question->answers && i < 8; i++) {
        smallfont_draw(&g_font, g_buf, 320, 200, box_x[i] + 21, box_y[i] + 5, 145, 141, question->answer[i].text);
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
    blit_ix(x, y, frame->pixels, frame->w, frame->h, 1);
}

static void paint_sheet(int which)
{
    if (which >= 0 && which < 6 && g_have_sheet[which]) {
        cover_screen(g_sheet[which].pixels, g_sheet[which].width, g_sheet[which].height);
    } else {
        cover_screen(0, 0, 0);
    }
}

static void text_at(int x, int y, const char *text)
{
    smallfont_draw(&g_font, g_buf, 320, 200, x, y, 145, 141, text);
}

static void number_at(int x, int y, int value, int centered, int sign)
{
    char text[20];

    log_format(text, sizeof(text), sign && value > 0 ? "+%d" : "%d", value);
    if (centered) { x -= smallfont_width(&g_font, text) / 2; }
    text_at(x, y, text);
}

/* generate.c/run_0009169B: fields sit at the same coordinates in the summary. */
static void paint_attributes(int summary)
{
    OgCharacter *who;
    int i;
    int y;

    who = og_character();
    for (i = 0; i < 8; i++) {
        number_at(24, og_chargen_attr_y(i) + 20 - g_font.height, who->attribute[i], 1, 0);
    }
    y = og_chargen_attr_y(og_chargen_attr_selected()) + 1;
    blit_ix(44, y, g_attr_arrows.pixels, g_attr_arrows.width, g_attr_arrows.height, 1);
    number_at(51, y + 14 - g_font.height, og_chargen_attr_pool(), 1, 0);
    if (summary) { return; }
    number_at(83, 22, who->attribute[OG_STR] / 10 - 5, 0, 1);
    number_at(103, 32, who->attribute[OG_STR] * 15 / 10, 0, 0);
    number_at(112, 49, og_magicka(who->attribute[OG_INT], who->class_flags), 0, 0);
    number_at(121, 71, who->attribute[OG_WIL] / 10, 0, 0);
    number_at(97, 93, who->attribute[OG_AGI] / 10 - 5, 0, 1);
    number_at(101, 110, who->attribute[OG_END] / 10 - 5, 0, 1);
    number_at(122, 120, who->attribute[OG_END] / 10 - 5, 0, 1);
}

static void paint_skills(void)
{
    OgCharacter *who;
    int i;
    int skill;
    int y;

    who = og_character();
    for (i = 0; i < 12; i++) {
        skill = og_class_skill(who->klass, i);
        if (skill < 0 || skill >= OG_SKILLS) { continue; }
        y = og_chargen_skill_y(i) + 1;
        text_at(68, y, og_skill_name(skill));
        number_at(192, y, who->skill[skill], 1, 0);
    }
    for (i = 0; i < 3; i++) {
        y = og_chargen_skill_y(og_chargen_skill_selected(i));
        blit_ix(203, y, g_skill_arrows.pixels, g_skill_arrows.width, g_skill_arrows.height, 0);
        number_at(221, y + 9 - g_font.height, 6 - og_chargen_skill_bonus(i), 1, 0);
    }
}

static void paint_custom(void)
{
    const OgClass *c;
    const OgSpecial *s;
    int page, list, kind, param, i, y, top, limit, skill;
    int advance;
    char text[96];

    c = og_class(18);
    if (!c) { lay_map(); return; }
    page = og_chargen_maker_page();
    list = og_chargen_maker_list();
    kind = og_chargen_maker_kind();
    cover_screen(g_custom_screen[0].pixels, 320, 200);
    if (page == 6) {
        blit_ix(39, 5, g_custom_screen[3].pixels, g_custom_screen[3].width, g_custom_screen[3].height, 0);
    } else if (page == 2 || page == 3 || page == 4 || page == 5) {
        blit_ix(0, 0, g_custom_screen[1].pixels, g_custom_screen[1].width, g_custom_screen[1].height, 0);
        if (!list) { blit_ix(0, 0, g_custom_screen[2].pixels, g_custom_screen[2].width, g_custom_screen[2].height, 0); }
    }
    if (page == 0 || page == 7 || page == 1 || page == 4 || page == 5) {
        text_at(94, 5, c->name);
        if (page == 7) { text_at(94 + smallfont_width(&g_font, c->name), 5, "_"); }
        for (i = 0; i < 8; i++) {
            number_at(24, og_chargen_attr_y(i) + 20 - g_font.height, c->attribute[i], 1, 0);
        }
        y = og_chargen_attr_y(og_chargen_attr_selected()) + 1;
        blit_ix(44, y, g_attr_arrows.pixels, g_attr_arrows.width, g_attr_arrows.height, 1);
        number_at(51, y + 14 - g_font.height, og_chargen_attr_pool(), 1, 0);
        for (i = 0; i < 12; i++) {
            text_at(68, og_chargen_skill_y(i) + 1, og_skill_name(og_class_skill(18, i)));
        }
        number_at(286, 55, c->hp_per_level, 1, 0);
        advance = c->advancement;
        y = advance >= 65536 ? 115 - (advance - 65536) * 69 / 262144 : 115 + (65536 - advance) * 69 / 65536;
        if (y < 46) { y = 46; } if (y > 175) { y = 175; }
        blit_ix(219, y, g_dagger.pixels, g_dagger.width, g_dagger.height, 1);
    }
    if (page == 2 || page == 3) {
        for (i = 0; i < og_chargen_special_count(list); i++) {
            kind = og_chargen_special_kind(list, i); param = og_chargen_special_param(list, i);
            s = og_special(list, kind);
            text_at(5, 33 + i * 20, s->name);
            text_at(5, 42 + i * 20, s->choice[param]);
        }
    } else if (page == 6) {
        int total, value, x, yy, h, start;
        total = 0;
        for (i = 0; i < 5; i++) {
            value = og_chargen_maker_reputation(i);
            total += value;
            h = (value < 0 ? -value : value) * 5;
            start = value > 0 ? 81 - h : 82;
            for (yy = start; yy < start + h; yy++) {
                for (x = 42 + i * 33; x <= 46 + i * 33; x++) { put_px(x, yy, value > 0 ? 197 : 246); }
            }
            number_at(58 + i * 33, 149, value, 1, 0);
        }
        number_at(105, 179, -total, 1, 0);
    } else if (page == 1 || page == 4 || page == 5) {
        if (g_have_pick) { blit_ix(g_pick.x, g_pick.y, g_pick.pixels, g_pick.width, g_pick.height, 0); }
        top = og_chargen_maker_top();
        limit = page == 4 ? 11 : (page == 5 ? og_special(list, kind)->count : 0);
        if (page == 1) { while (og_chargen_maker_skill(limit) >= 0) { limit++; } }
        paint_scrollbar(top, limit);
        for (i = 0; i < 9 && top + i < limit; i++) {
            if (page == 1) {
                skill = og_chargen_maker_skill(top + i);
                df_copy(text, og_skill_name(skill), sizeof(text));
            } else if (page == 4) {
                df_copy(text, og_special(list, top + i)->name, sizeof(text));
            } else {
                df_copy(text, og_special(list, kind)->choice[top + i], sizeof(text));
            }
            text_at(86, 63 + i * 8, text);
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
    text = og_question_text(og_chargen_question_number());
    y = -og_chargen_question_scroll();
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
    blit_ix(0, 135, strip, 320, 48, 0);
}

static void paint_reflex(int summary)
{
    int r;
    int x;
    int y;

    r = og_character()->reflexes;
    x = summary ? 246 : 127;
    y = (summary ? 95 : 148) + r * 9;
    if (g_reflex.pixels) {
        blit_ix(x, y, g_reflex.pixels + r * 66 * 9, 66, 9, 0);
    }
    if (!summary) {
        char lines[16][96];
        int n, x0, y0, cols, rows, ty, shift;
        n = take_lines(og_words(307), lines, 16);
        paper_box(lines, n, 0, &x0, &y0, &cols, &rows, &ty);
        /* generate.c anchors kind-4 help at y=125 (D_00195F34).
         * Preserve the selector/OK underneath instead of drawing naked text. */
        shift = 125 - rows * 22 - y0;
        lay_paper(x0, y0 + shift, cols, rows);
        paint_lines(lines, n, 1, x0, ty + shift, 145);
    }
}

/* Snapshot is the actual indexed surface uploaded by publish, useful for host QA. */
const df_u8 *seam_screen(const df_u8 **palette)
{
    if (palette) {
        *palette = g_screen_kind == 2 ? g_chgn_rgb : (g_screen_kind == 1 ? g_art_rgb : g_map_rgb);
    }
    return g_buf;
}

static void selected_hud_face(void)
{
    const CifFrame *frame;
    df_u8 pixels[32 * 32];
    int x;
    int y;
    int sx;
    int sy;
    int id;
    OgCharacter *who;

    who = og_character();
    if (g_face_index == who->face && g_hud_race == who->race && g_hud_sex == who->female) { return; }
    ensure_faces(who->race, who->female);
    frame = g_faces_open ? cif_frame(&g_faceset, who->face) : 0;
    if (!frame) { return; }
    mem_zero(pixels, sizeof(pixels));
    for (y = 0; y < 32; y++) {
        sy = y * frame->h / 32;
        for (x = 0; x < 32; x++) {
            sx = x * frame->w / 32;
            pixels[y * 32 + x] = frame->pixels[sy * frame->w + sx];
        }
    }
    id = og_pic(OG_PIC_FACE);
    if (id) { pic_free(id); }
    og_pic_set(OG_PIC_FACE, pic_load(pixels, 32, 32, g_art_rgb, 1));
    g_face_index = who->face;
    g_hud_race = who->race;
    g_hud_sex = who->female;
}

static int screen_kind(int mode)
{
    if (mode == OG_UI_QUESTIONS) {
        return 2;
    }
    if (mode == OG_UI_MAKER || mode == OG_UI_NAME || mode == OG_UI_FACE || mode == OG_UI_ATTR || mode == OG_UI_SKILL
        || mode == OG_UI_REFLEX || mode == OG_UI_SUMMARY) {
        return 1;
    }
    return 0;
}

static void publish_surface(int kind, int cutout)
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
        og_pic_set(OG_PIC_SCREEN, 0);
        return;
    }
    if (g_comp[kind] && g_comp_cutout[kind] != cutout) { pic_free(g_comp[kind]); g_comp[kind] = 0; }
    g_comp_cutout[kind] = cutout;
    if (!g_comp[kind]) {
        g_comp[kind] = pic_load(g_buf, 320, 200, pal, cutout);
    } else {
        pic_update(g_comp[kind], g_buf);
    }
    g_screen_kind = kind;
    og_pic_set(OG_PIC_SCREEN, g_comp[kind]);
}

static void publish(int kind) { publish_surface(kind,0); }

static void fill_box(int x, int y, int w, int h, int color)
{
    int row, col;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) { put_px(x + col, y + row, color); }
    }
}

static void outline(int x, int y, int w, int h, int color)
{
    fill_box(x, y, w, 1, color);
    fill_box(x, y + h - 1, w, 1, color);
    fill_box(x, y, 1, h, color);
    fill_box(x + w - 1, y, 1, h, color);
}

static void centered_text(int x, int y, const char *text, int ink)
{
    smallfont_draw(&g_font, g_buf, 320, 200, x - smallfont_width(&g_font, text)/2, y, ink, -1, text);
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
    paper_box(lines, n, 0, &x, &y, &cols, &rows, &ty);
    lay_paper(x, y, cols, rows);
    paint_lines(lines, n, 1, x, ty, 146);
}

static void paint_options(void)
{
    static df_u8 pixels[150 * 84];
    const OgConfig *config;
    int i, x, y, id;

    config = og_config();
    cover_screen(0, 0, 0);
    blit_ix(85, 40, g_options_art.pixels, g_options_art.width, g_options_art.height, 0);
    for (i = 0; i < 2; i++) {
        if (og_options_flags() & (1 << i)) { fill_box(i ? 225 : 154, 90, 3, 3, 246); }
    }
    fill_box(91, 64, config->sound_volume * 107 / 100, 3, 246);
    fill_box(91, 72, config->music_volume * 107 / 100, 3, 246);
    fill_box(91, 80, config->detail * 108 / 128, 3, 246);
    for (y = 0; y < 84; y++) {
        for (x = 0; x < 150; x++) { pixels[y * 150 + x] = g_buf[(40 + y) * 320 + 85 + x]; }
    }
    id = og_pic(OG_PIC_OPTIONS);
    if (id) { pic_update(id, pixels); }
    else { og_pic_set(OG_PIC_OPTIONS, pic_load(pixels, 150, 84, g_art_rgb, 0)); }
    og_pic_set(OG_PIC_SCREEN, 0);
    g_screen_kind = 1;
}

static void paint_controls(void)
{
    const OgControlRect *rect;
    char name[32];
    int i, ink;

    cover_screen(g_controls_art.pixels, g_controls_art.width, g_controls_art.height);
    if (og_options_mouse()) {
        blit_ix(g_controls_view.x, g_controls_view.y, g_controls_view.pixels,
                g_controls_view.width, g_controls_view.height, 0);
    }
    for (i = 0; i < OG_ACTS; i++) {
        rect = og_control_rect(i);
        if (i == og_controls_editing()) { df_copy(name, "Press key", sizeof(name)); ink = 246; }
        else { og_control_key_name(i, name, sizeof(name)); ink = og_controls_duplicate(i) ? 244 : 146; }
        centered_text((rect->x0 + rect->x1) / 2, rect->y0 + 2, name, ink);
    }
    if (og_controls_notice()[0]) { panel_notice(og_controls_notice()); }
    publish(1);
}

static void paint_mouse(void)
{
    static df_u8 pixels[188 * 146];
    const OgConfig *config;
    int i, x, y, id;

    config = og_config();
    cover_screen(0, 0, 0);
    blit_ix(g_mouse_art.x, g_mouse_art.y, g_mouse_art.pixels, g_mouse_art.width, g_mouse_art.height, 0);
    fill_box(config->mouse_mode ? 220 : 134, 47, 5, 5, 246);
    for (i = 0; i < config->sensitivity_x; i++) { fill_box(139 + i * 7, 108, 5, 6, 246); }
    for (i = 0; i < config->sensitivity_y; i++) { fill_box(139 + i * 7, 121, 5, 6, 246); }
    for (i = 0; i < config->turn_rate; i++) { fill_box(139 + i * 7, 134, 5, 6, 246); }
    if (config->reverse_y) { outline(72, 147, 76, 15, 246); }
    for (y = 0; y < 146; y++) {
        for (x = 0; x < 188; x++) { pixels[y * 188 + x] = g_buf[(22 + y) * 320 + 66 + x]; }
    }
    id = og_pic(OG_PIC_MOUSE);
    if (id) { pic_update(id, pixels); }
    else { og_pic_set(OG_PIC_MOUSE, pic_load(pixels, 188, 146, g_art_rgb, 0)); }
    og_pic_set(OG_PIC_SCREEN, 0);
    g_screen_kind = 1;
}

static void free_previews(void)
{
    int i;
    for (i = 0; i < 6; i++) {
        if (g_preview[i]) { pic_free(g_preview[i]); g_preview[i] = 0; }
        og_preview_pic_set(i, 0);
    }
    g_preview_generation = 0;
}

static void prepare_previews(void)
{
    const OgSaveInfo *info;
    int i;
    if (g_preview_generation == og_files_generation()) { return; }
    free_previews();
    for (i = 0; i < 6; i++) {
        info = og_files_info(i);
        if (info->status == 1 && info->preview_valid) {
            g_preview[i] = pic_load_565(info->preview, OG_THUMB_W, OG_THUMB_H);
            og_preview_pic_set(i, g_preview[i]);
        }
    }
    g_preview_generation = og_files_generation();
}

static void paint_files(void)
{
    const OgSaveInfo *info;
    char text[64];
    int i, x, y;

    cover_screen(g_files_art.pixels, g_files_art.width, g_files_art.height);
    if (og_files_saving()) {
        blit_ix(g_save_label_art.x, g_save_label_art.y, g_save_label_art.pixels,
                g_save_label_art.width, g_save_label_art.height, 0);
    }
    for (i = 0; i < 6; i++) {
        x = i < 3 ? 80 : 240;
        y = (i % 3) * 65;
        info = og_files_info(i);
        if (info->status == 1) {
            /* Names are clipped to each slot's label strip. */
            df_copy(text, info->label, sizeof(text));
            while (smallfont_width(&g_font, text) > 112) {
                int n;
                n = 0; while (text[n]) { n++; }
                if (!n) { break; }
                text[n-1] = 0;
            }
            centered_text(x, y + 57, text, 145);
            if (!info->preview_valid) { centered_text(x, y + 23, info->character, 145); }
        } else {
            centered_text(x, y + 57, info->status < 0 ? "Damaged save" : "Empty slot", info->status < 0 ? 244 : 146);
        }
        if (i == og_files_selected()) { outline(x - 41, y + 3, 82, 52, 146); }
    }
    if (og_files_error()[0]) { panel_notice(og_files_error()); }
    if (og_files_editing()) {
        lay_paper(28, 66, 12, 4);
        centered_text(160, 82, "Name this saved game:", 146);
        centered_text(160, 101, og_files_name(), 146);
        lay_button(0, OG_FILE_ACCEPT_X, OG_FILE_BUTTON_Y);
        lay_button(1, OG_FILE_REJECT_X, OG_FILE_BUTTON_Y);
    }
    publish(1);
}

/* The controllers own input and records; this adapter paints original assets. */
static void load_possession_art(void)
{
    static const char *name[] = {"INVE00I0.IMG", "INVE01I0.IMG", "INFO00I0.IMG", "BOOK00I0.IMG", "SPBK00I0.IMG"};
    char file[20];
    df_u32 size;
    int i;
    for (i = 0; i < 5; i++) { img_load(name[i], &g_possession_art[i]); }
    g_spell_icons = vfs_read("ICON00I0.IMG", &size);
    if (size < 320u * 64u) { g_spell_icons = 0; }
    g_spell_masks = vfs_read("MASK04I0.IMG", &size);
    if (size < 40u * 80u) { g_spell_masks = 0; }
    for (i = 0; i < 5; i++) {
        log_format(file,sizeof(file),"FONT%04d.FNT",i ? i-1 : 3);
        smallfont_load(&g_book_font[i],file);
    }
    cif_open("INVE16I0.CIF", &g_container_icons);
    for (i = 0; i < 3; i++) {
        log_format(file,sizeof(file),"REST%02dI0.IMG",i); img_load(file,&g_rest_art[i]);
    }
    og_spells_load();
}

/* Every decode for one changed screen dies after composition. A newly read
 * uncapped archive is forgotten before this heap goes, so the file cache can
 * never hold a pointer into freed UI storage. */
static Image *possession_texture(int file, int record)
{
    if (!texture_cached(file) && g_panel_file_count < 64) { g_panel_files[g_panel_file_count++] = file; }
    return texture_load(file, record);
}

static void doll_layer(const df_u8 *pixels, int w, int h, int x, int y, int owner, int color_map)
{
    int row, col, dx, dy, color;
    df_u8 *owners;
    owners = og_doll_owners();
    if (!pixels) { return; }
    for (row = 0; row < h; row++) {
        dy = y + row - 8;
        if (dy < 0 || dy >= 184) { continue; }
        for (col = 0; col < w; col++) {
            dx = x + col - 200;
            color = pixels[row*w+col];
            if (!color || dx < 0 || dx >= 110) { continue; }
            if (owner >= 0 && color == 255) {
                g_doll[dy*110+dx] = g_doll_back[dy*110+dx];
                owners[dy*110+dx] = 255;
            } else {
                g_doll[dy*110+dx] = (df_u8)og_item_ink(color_map,color);
                if (owner >= 0) { owners[dy*110+dx] = (df_u8)owner; }
            }
        }
    }
}

static void doll_img(const char *name)
{
    ImgPicture picture;
    if (img_load(name,&picture)) { doll_layer(picture.pixels,picture.width,picture.height,picture.x,picture.y,-1,-1); }
}

static int doll_item_image(const OgObject *item, int slot)
{
    int image, file;
    image = item->inventory_image;
    if (item->group != 3) { return image; }
    /* paperdoll_add_item changes a temporary copy: gender archive, then hand.
     * Bow archives are shared; one-handed weapons have a second, right-hand
     * record. Keep the saved item and its inventory-list picture unchanged. */
    file = image >> 7;
    if (og_character()->female && file != 432 && file != 433) { image -= 128; }
    if (slot == 19 && !og_item_two_handed(item)) { image++; }
    return image;
}

typedef struct DollLayer { int slot, image, order; } DollLayer;

static void doll_insert(DollLayer *layers, int *count, int slot, int image, int order)
{
    int at;
    at = (*count)++;
    /* Equal layers precede existing entries in paperdoll_add_item. */
    while (at && layers[at-1].order >= order) { layers[at] = layers[at-1]; at--; }
    layers[at].slot = slot; layers[at].image = image; layers[at].order = order;
}

static void paint_doll(int x, int y)
{
    const CareerRace *race;
    const OgObject *item;
    const OgItemDef *def;
    ImgPicture back;
    CifSet faces;
    const CifFrame *face;
    Image *image;
    DollLayer layers[27];
    int i, n, slot, image_id, layer, cloak, px, py, color_map;
    race = career_race(og_character()->race+1);
    if (!race || !img_load(race->background,&back) || back.width < 118 || back.height < 191) { return; }
    for (i = 0; i < 184; i++) { mem_copy(g_doll_back+i*110,back.pixels+(i+7)*back.width+8,110); }
    mem_copy(g_doll,g_doll_back,sizeof(g_doll));
    for (i = 0; i < (int)sizeof(g_doll); i++) { og_doll_owners()[i] = 255; }
    doll_img(og_character()->female ? race->body_female : race->body_male);
    doll_img(og_character()->female ? race->body_female_clothed : race->body_male_clothed);
    if (cif_open(og_character()->female ? race->heads_female : race->heads_male,&faces)) {
        face = cif_frame(&faces,og_character()->face);
        if (face) { doll_layer(face->pixels,face->w,face->h,face->x,face->y,-1,-1); }
        cif_close(&faces);
    }
    n = cloak = 0;
    for (i = 0; i < 27; i++) {
        item = og_object(og_state()->equipped[i]);
        if (!item || i < 12) { continue; }
        def = og_item_def(item->group,item->index);
        if (!def) { continue; }
        layer = def->draw_order;
        if (item->group == 3 && i == 21 && !og_item_two_handed(item)) { layer += 5; }
        doll_insert(layers,&n,i,doll_item_image(item,i),layer);
        if (!cloak && ((item->group == 6 && (item->index == 13 || item->index == 14)) ||
                       (item->group == 12 && (item->index == 9 || item->index == 10)))) {
            /* One cloak's first variant forms the back, below all equipment. */
            image_id = (item->inventory_image & ~127) | og_clothing_first(item->group,item->inventory_image & 127);
            doll_insert(layers,&n,i,image_id,0); cloak = 1;
        }
    }
    for (i = 0; i < n; i++) {
        slot = layers[i].slot;
        item = og_object(og_state()->equipped[slot]);
        image_id = layers[i].image;
        image = possession_texture(image_id >> 7,image_id & 127);
        if (!image || !texture_offset(image_id >> 7,image_id & 127,&px,&py)) { continue; }
        color_map = item->color;
        doll_layer(image->index,image->width,image->height,px,py,slot,color_map);
    }
    blit_ix(x,y,g_doll,110,184,0);
}

static void copy_art_rect(const ImgPicture *art, int x, int y, int w, int h)
{
    int row;
    if (!art->pixels || x < 0 || y < 0 || x+w > art->width || y+h > art->height) { return; }
    for (row = 0; row < h; row++) { mem_copy(g_buf+(y+row)*320+x,art->pixels+(y+row)*art->width+x,w); }
}

static void paint_item_icon(const OgObject *item, int x, int y, int width, int height)
{
    const Image *image;
    int w, h, row, col, color, color_map;
    image = possession_texture(item->inventory_image >> 7,item->inventory_image & 127);
    if (!image) { return; }
    w = image->width; h = image->height;
    og_size_fit(&w,&h,width,height);
    if (!w || !h) { return; }
    color_map = item->color;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            color = image->index[(row*image->height/h)*image->width+col*image->width/w];
            if (color && color != 255) { put_px(x+(width-w)/2+col,y+(height-h)/2+row,og_item_ink(color_map,color)); }
        }
    }
}

static void paint_item_cell(const OgObject *item, int x, int y)
{
    char line[32];
    paint_item_icon(item,x+2,y+2,46,33);
    if (og_item_equipped(item->id) >= 0) { outline(x,y,50,37,145); }
    if (item->count > 1 || item->group == 28) {
        log_format(line,sizeof(line),"%d",item->group == 28 ? item->value : item->count);
        text_at(x+2,y+29,line);
    }
}

static void paint_inventory(void)
{
    const OgPossessions *view;
    const OgObject *item;
    const CifFrame *icon;
    df_u32 rows[OG_OBJECTS];
    static const int tab_x[4] = {0,93,163,255}, tab_w[4] = {92,69,91,65};
    static const int action_y[4] = {36,58,80,103};
    char line[64];
    int side, n, i, at, x;
    view = og_possessions();
    cover_screen(g_possession_art[0].pixels,g_possession_art[0].width,g_possession_art[0].height);
    copy_art_rect(&g_possession_art[1],tab_x[view->tab],0,tab_w[view->tab],10);
    copy_art_rect(&g_possession_art[1],226,action_y[view->action],31,14);
    paint_doll(49,13);
    for (i = 0; i < 12; i++) {
        item = og_object(og_state()->equipped[i]);
        if (item) { paint_item_icon(item,i%2*23,10+i/2*31,22,21); }
    }
    {
        static const int armor_x[7] = {120,70,130,60,55,60,70};
        static const int armor_y[7] = {24,50,50,70,100,120,180};
        for (i = 0; i < 7; i++) { number_at(armor_x[i],armor_y[i],og_inventory_armor(i),0,0); }
    }
    for (side = 0; side < 2; side++) {
        x = side ? 270 : 172;
        n = og_inventory_list(side ? view->remote : og_state()->inventory_id,view->tab,rows,OG_OBJECTS);
        for (i = 0; i < 4; i++) {
            at = view->scroll[side]+i;
            if (at >= n || at >= OG_OBJECTS) { break; }
            item = og_object(rows[at]);
            paint_item_cell(item,x,48+i*38);
        }
        {
            char weight[24];
            og_weight_text(og_inventory_weight(side ? view->remote : og_state()->inventory_id),weight,sizeof(weight));
            log_format(line,sizeof(line),"%s kg",weight);
        }
        centered_text(x+22,36,line,145);
    }
    icon = cif_frame(&g_container_icons,view->remote && !(og_object(view->remote)->thing_key & 0x80000000u) ? 5 : 2);
    if (icon) {
        int w, h, row, col, color;
        w = icon->w; h = icon->h;
        og_size_fit(&w,&h,46,29);
        if (w > 0 && h > 0) {
            for (row = 0; row < h; row++) { for (col = 0; col < w; col++) {
                color = icon->pixels[(row*icon->h/h)*icon->w+col*icon->w/w];
                if (color && color != 255) { put_px(272+(46-w)/2+col,12+(29-h)/2+row,color); }
            } }
        }
    }
    log_format(line,sizeof(line),"%d",og_character()->gold);
    centered_text(240,151,line,145);
}

static int skill_in_career(int skill)
{
    const OgClass *career;
    int i;
    career = &og_state()->career;
    for (i = 0; i < 3; i++) { if (career->primary[i] == skill || career->major[i] == skill) { return 1; } }
    for (i = 0; i < 6; i++) { if (career->minor[i] == skill) { return 1; } }
    return 0;
}

static void paint_skill_list(void)
{
    const OgPossessions *view;
    const OgClass *career;
    const char *name;
    static const char *title[] = {"Primary Skills","Major Skills","Minor Skills","Miscellaneous Skills"};
    int i, skill, n, shown;
    char line[64];
    view = og_possessions(); career = &og_state()->career;
    lay_paper(2,2,9,9);
    centered_text(100,12,title[view->skill_group],145);
    shown = n = 0;
    for (i = 0; i < (view->skill_group == 0 || view->skill_group == 1 ? 3 : view->skill_group == 2 ? 6 : OG_SKILLS); i++) {
        skill = view->skill_group == 0 ? career->primary[i] : view->skill_group == 1 ? career->major[i] :
                view->skill_group == 2 ? career->minor[i] : i;
        if (view->skill_group == 3 && skill_in_career(skill)) { continue; }
        if (n++ < view->skill_scroll || shown >= 16) { continue; }
        name = og_skill_name(skill);
        log_format(line,sizeof(line),"%s: %d%%",name,og_character()->skill[skill]);
        text_at(14,27+shown*9,line); shown++;
    }
}

static void paint_character_sheet(void)
{
    const OgCharacter *who;
    char line[80];
    int i;
    who = og_character();
    cover_screen(g_possession_art[2].pixels,g_possession_art[2].width,g_possession_art[2].height);
    paint_doll(200,8);
    text_at(41,4,who->name); text_at(41,14,og_race_name(who->race)); text_at(46,24,og_state()->career.name);
    number_at(45,34,who->level,0,0); number_at(39,44,who->gold,0,0);
    log_format(line,sizeof(line),"%d/%d",who->fatigue>>6,who->attribute[OG_STR]+who->attribute[OG_END]);
    centered_text(77,54,line,145);
    log_format(line,sizeof(line),"%d/%d",who->health,who->max_health); centered_text(72,64,line,145);
    log_format(line,sizeof(line),"%d/%d",og_inventory_weight(og_state()->inventory_id)/4,og_inventory_capacity()); text_at(91,74,line);
    for (i = 0; i < OG_ATTRS; i++) { number_at(155,17+i*24,who->attribute[i],1,0); }
    if (og_possessions()->skill_group >= 0) { paint_skill_list(); }
}

static void blit_stride(int x, int y, const df_u8 *src, int stride, int w, int h)
{
    int row, col;
    for (row = 0; row < h; row++) { for (col = 0; col < w; col++) { put_px(x+col,y+row,src[row*stride+col]); } }
}

static void paint_spellbook(void)
{
    const OgPossessions *view;
    const OgSpellDef *spell;
    const OgObject *owned;
    df_u32 ids[OG_OBJECTS];
    int count, i, selected, x, y, at, top, height, length;
    char line[48];
    view = og_possessions();
    if (view->underlay == OG_VIEW_INVENTORY) { paint_inventory(); }
    else if (view->underlay == OG_VIEW_SHEET) { paint_character_sheet(); }
    else { cover_screen(0,0,0); }
    blit_ix(g_possession_art[4].x,g_possession_art[4].y,g_possession_art[4].pixels,g_possession_art[4].width,g_possession_art[4].height,0);
    count = og_spell_rows(view->book,ids,OG_OBJECTS);
    selected = view->selected < count ? view->selected : count-1;
    for (i = 0; i < 14 && i+view->scroll[0] < count; i++) {
        at = i+view->scroll[0];
        if (at == selected) { fill_box(27,30+i*9,111,9,150); }
        df_copy(line,og_object(ids[at])->name,sizeof(line));
        for (length = 0; line[length]; length++) { }
        while (length && smallfont_width(&g_font,line) > 109) { line[--length] = 0; }
        text_at(28,30+i*9,line);
    }
    og_spell_scrollbar(&top,&height);
    fill_box(145,45,7,104,114); fill_box(145,top,7,height,146);
    if (selected < 0) { return; }
    spell = og_spell_def(og_object(ids[selected])->index);
    owned = og_object(ids[selected]);
    if (!spell) { return; }
    text_at(148,20,owned->name);
    log_format(line,sizeof(line),"%d/%d",og_character()->magicka,og_character()->max_magicka); text_at(239,20,line);
    if (g_spell_icons && owned->spell_icon < 69) {
        x = (owned->spell_icon%20)*16; y = (owned->spell_icon/20)*16;
        blit_stride(172,32,g_spell_icons+y*320+x,320,16,16);
    }
    if (g_spell_masks && spell->target < 5 && spell->element < 5) {
        blit_stride(205,32,g_spell_masks+spell->target*640,40,24,16);
        blit_stride(246,32,g_spell_masks+spell->element*640+24,40,16,16);
    }
    for (i = 0; i < 3; i++) {
        if (spell->group[i] != 255) {
            centered_text(219,63+i*38,og_spell_effect(spell->group[i],spell->sub[i]),145);
            centered_text(219,75+i*38,og_effect_subtype(spell->group[i],spell->sub[i]),145);
        }
    }
}

static int book_line_width(const SmallFont *font, const char *text)
{
    char glyph[2];
    int width;
    width = 0; glyph[1] = 0;
    for (; *text; text++) {
        glyph[0] = *text;
        width += *text == ' ' ? df_ru16(font->bytes) : smallfont_width(font,glyph);
    }
    return width;
}

static void flush_book_line(char *line, int *used, int *x, int y, int centered, int font, int ink)
{
    char glyph[2];
    int left, i;
    const SmallFont *face;
    face = &g_book_font[font];
    if (!face->bytes) { *used = 0; return; }
    line[*used] = 0;
    left = centered ? 160-book_line_width(face,line)/2 : *x;
    glyph[1] = 0;
    for (i = 0; i < *used; i++) {
        glyph[0] = line[i];
        if (line[i] == ' ') { left += df_ru16(face->bytes); }
        else {
            smallfont_draw(face,g_buf,320,181,left,y,ink,156,glyph);
            left += smallfont_width(face,glyph);
        }
    }
    *used = 0;
}

static void paint_readable_book(void)
{
    const OgBook *book;
    const df_u8 *bytes;
    char line[256];
    int at, used, x, y, font, ink, center, token, height;
    book = og_reading(); bytes = book->page;
    cover_screen(g_possession_art[3].pixels,g_possession_art[3].width,g_possession_art[3].height);
    x = 10; y = 20; font = 4; ink = 145; center = used = 0; height = g_book_font[font].height;
    for (at = 0; at < book->page_bytes && y < 180; ) {
        token = bytes[at++];
        if (token == 246) { break; }
        if (token == 251 && at+2 <= book->page_bytes) { x = df_ru16(bytes+at); at += 2; }
        else if (token == 250 && at < book->page_bytes) { ink = bytes[at++]; }
        else if (token == 249 && at < book->page_bytes) {
            flush_book_line(line,&used,&x,y,center,font,ink);
            font = bytes[at++]; if (font > 4) { font = 4; }
            if (height < g_book_font[font].height) { height = g_book_font[font].height; }
        } else if (token == 253) { center = 1; }
        else if (token == 247) {
            /* Inline picture names are NUL terminated; loading, if supported,
             * stays within this page's checked bound. */
            int start;
            ImgPicture picture;
            start = at;
            while (at < book->page_bytes && bytes[at]) { at++; }
            if (at < book->page_bytes && at-start < 24 && img_load((const char *)bytes+start,&picture)) {
                blit_ix(x,y,picture.pixels,picture.width,picture.height,1);
            }
            if (at < book->page_bytes) { at++; }
        } else if (!token || token == 1) {
            flush_book_line(line,&used,&x,y,center,font,ink);
            if (!token) { x = 10; y += height; height = g_book_font[font].height; }
            center = 0;
        } else if (token < 246 && used < (int)sizeof(line)-1) { line[used++] = (char)token; }
    }
    if (used) { flush_book_line(line,&used,&x,y,center,font,ink); }
    log_format(line,sizeof(line),"%d/%d",og_possessions()->book_page+1,book->pages);
    text_at(11,188,line);
}

static void paint_painting(int picture, const char *caption)
{
    CifSet art;
    const CifFrame *frame;
    char name[16], lines[OG_NOTICE_LINES][96];
    int count, i, y;
    lay_paper(6,1,14,9);
    log_format(name,sizeof(name),"%cPAINT.CIF",'A'+picture/8);
    y = 145;
    if (cif_open(name,&art)) {
        frame = cif_frame(&art,picture%8);
        if (frame) { blit_ix(160-frame->w/2,50,frame->pixels,frame->w,frame->h,0); y = 50+frame->h+7; }
        cif_close(&art);
    }
    count = og_notice_lines(caption,0,lines,OG_NOTICE_LINES);
    if (count > OG_NOTICE_LINES) { count = OG_NOTICE_LINES; }
    for (i = 0; i < count && y < 190; i++, y += 7) { centered_text(160,y,lines[i],146); }
}

static int present_possessions(void)
{
    const OgPossessions *view;
    MemHeap *heap, *was;
    char line[160];
    int i;
    view = og_possessions();
    if (!view->page) { g_possession_page = 0; return 0; }
    if (g_possession_page == view->page && g_possession_revision == view->revision && og_pic(OG_PIC_SCREEN)) { return 1; }
    g_possession_page = view->page; g_possession_revision = view->revision;
    if (!g_buf) { g_buf = (df_u8 *)mem_keep(64000); }
    if (!g_buf) { return 1; }
    heap = mem_heap_new();
    if (!heap) { return 1; }
    was = mem_heap_use(heap); g_panel_file_count = 0;
    if (view->page == OG_VIEW_INVENTORY) { paint_inventory(); }
    else if (view->page == OG_VIEW_SHEET) { paint_character_sheet(); }
    else if (view->page == OG_VIEW_SPELLS) { paint_spellbook(); }
    else { paint_readable_book(); }
    if (view->notice[0] && view->painting) { paint_painting(view->painting-1,view->notice); }
    else if (view->notice[0]) {
        char lines[OG_NOTICE_LINES+1][96];
        int count, shown, x, y, cols, rows, ty;
        count = og_notice_lines(view->notice,view->notice_page*OG_NOTICE_LINES,lines,OG_NOTICE_LINES);
        shown = count-view->notice_page*OG_NOTICE_LINES;
        if (shown > OG_NOTICE_LINES) { shown = OG_NOTICE_LINES; }
        if (count > OG_NOTICE_LINES) {
            log_format(lines[shown++],96,"Page %d/%d  PgUp/PgDn",view->notice_page+1,(count+OG_NOTICE_LINES-1)/OG_NOTICE_LINES);
        }
        paper_box(lines,shown,0,&x,&y,&cols,&rows,&ty);
        lay_paper(x,y,cols,rows); paint_lines(lines,shown,1,x,ty,146);
    }
    if (view->prompt) {
        const char *prompt;
        prompt = view->prompt == OG_PROMPT_GOLD ? "How much gold?" : view->prompt == OG_PROMPT_NAME ? "Name thyself" :
                 view->prompt == OG_PROMPT_SPELL_NAME ? "Enter spell name" :
                 "Enter new page:";
        fill_box(68,65,184,75,45); outline(68,65,184,75,145);
        centered_text(160,75,prompt,145);
        log_format(line,sizeof(line),"%s_",view->input); centered_text(160,96,line,145);
        centered_text(117,121,"Accept",145); centered_text(203,121,"Reject",145);
    }
    for (i = 0; i < g_panel_file_count; i++) { texture_forget(g_panel_files[i]); }
    mem_heap_use(was); mem_heap_delete(heap);
    publish_surface(1,view->page == OG_VIEW_SPELLS && !view->underlay);
    return 1;
}

static int present_panel(void)
{
    unsigned revision;
    int panel;

    panel = og_options_panel();
    if ((panel != OG_PANEL_CONTROLS || !og_controls_mouse()) && og_pic(OG_PIC_MOUSE)) {
        pic_free(og_pic(OG_PIC_MOUSE));
        og_pic_set(OG_PIC_MOUSE, 0);
    }
    if (panel == OG_PANEL_FILES) { prepare_previews(); }
    else if (g_preview_generation) { free_previews(); }
    revision = og_options_revision();
    if (panel == OG_PANEL_CONTROLS) { revision += og_controls_revision(); }
    if (panel == OG_PANEL_FILES) { revision += og_files_revision(); }
    if (g_last_panel == panel && g_panel_revision == revision) { return 1; }
    g_last_panel = panel;
    g_panel_revision = revision;
    if (!g_buf) { g_buf = (df_u8 *)mem_keep(64000); }
    if (!g_buf) { return 1; }
    if (panel == OG_PANEL_OPTIONS) { paint_options(); }
    else if (panel == OG_PANEL_FILES) { paint_files(); }
    else if (og_controls_mouse()) { paint_mouse(); }
    else { paint_controls(); }
    return 1;
}

void seam_present(void)
{
    int mode;
    int kind;
    OgCharacter *who;
    const char *text;
    static int death_present;
    static int rest_present;
    static unsigned rest_revision;

    mode = og_chargen_mode();
    if (mode == OG_UI_WORLD) { selected_hud_face(); update_compass(); }
    if (og_options_up()) { death_present = 0; present_panel(); return; }
    if (og_rest()->open) {
        if (!rest_present || rest_revision != og_rest()->revision) {
            const OgRest *rest;
            const ImgPicture *art;
            char line[48];
            rest = og_rest();
            if (!g_buf) { g_buf = (df_u8 *)mem_keep(64000); }
            if (!g_buf) { return; }
            cover_screen(0,0,0);
            art = &g_rest_art[rest->stage == OG_REST_CHOOSE ? 0 : rest->until_healed ? 1 : 2];
            blit_ix(art->x,art->y,art->pixels,art->width,art->height,0);
            if (rest->stage == OG_REST_HOURS) {
                lay_paper(72,96,8,3);
                centered_text(160,105,rest->loiter ? "Loiter how many hours? (1-3)" : "Rest how many hours? (1-99)",146);
                log_format(line,sizeof(line),"%s_  Enter to accept",rest->input); centered_text(160,121,line,146);
            } else if (rest->stage == OG_REST_RUNNING) {
                log_format(line,sizeof(line),"%d",rest->until_healed ? rest->elapsed : rest->hours-rest->elapsed);
                centered_text(118,62,line,146);
            }
            publish_surface(1,1); rest_revision = rest->revision; rest_present = 1;
        }
        return;
    }
    rest_present = 0;
    if (og_dead()) {
        if (!death_present) {
            if (!g_buf) { g_buf = (df_u8 *)mem_keep(64000); }
            if (g_buf) {
                cover_screen(0,0,0);
                lay_paper(50,45,10,6);
                centered_text(160,64,"Thy life has ended.",146);
                centered_text(160,90,"Load game (L)",146);
                centered_text(160,114,"Return to title (N)",146);
                centered_text(160,138,"Quit (Q)",146);
                publish_surface(1,1); death_present = 1;
            }
        }
        return;
    }
    death_present = 0;
    if (present_possessions()) { return; }
    if (g_preview_generation) { free_previews(); }
    if (mode == OG_UI_WORLD && g_last_panel >= 0) {
        if (g_comp[1]) { pic_free(g_comp[1]); g_comp[1] = 0; }
        if (og_pic(OG_PIC_MOUSE)) { pic_free(og_pic(OG_PIC_MOUSE)); og_pic_set(OG_PIC_MOUSE, 0); }
    }
    g_last_panel = -1;
    if (mode == OG_UI_TITLE || mode == OG_UI_MAP || mode == OG_UI_CLASS_HOW
        || mode == OG_UI_WORLD) {
        og_pic_set(OG_PIC_SCREEN, 0);
        return;
    }
    if (g_present_mode == mode && g_revision == og_chargen_revision() && og_pic(OG_PIC_SCREEN)) {
        return;
    }
    g_present_mode = mode;
    g_revision = og_chargen_revision();
    if (!g_buf) {
        g_buf = (df_u8 *)mem_keep(64000);
    }
    if (!g_buf) {
        return;
    }
    who = og_character();
    if (mode == OG_UI_LOAD) {
        cover_screen(g_files_art.pixels, g_files_art.width, g_files_art.height);
    } else if (mode == OG_UI_RACE) {
        text = og_words(og_chargen_text_id());
        paint_parchment(text, 1, 1, 3, 4);
    } else if (mode == OG_UI_GENDER) {
        text = og_words(2200);
        paint_parchment(text, 1, 1, 6, 7);
    } else if (mode == OG_UI_CLASS_LIST) {
        paint_class_list();
    } else if (mode == OG_UI_CLASS_YES) {
        text = og_words(og_chargen_text_id());
        paint_parchment(text, 1, 1, 3, 4);
    } else if (mode == OG_UI_QUESTIONS) {
        if (g_have_chgn) {
            cover_screen(g_chgn.pixels, g_chgn.width, g_chgn.height);
        } else {
            lay_map();
        }
        paint_question();
    } else if (mode == OG_UI_MAKER) {
        paint_custom();
    } else if (mode == OG_UI_BG_HOW) {
        lay_map();
        if (g_have_butn2) {
            blit_ix(g_butn2.x, g_butn2.y, g_butn2.pixels, g_butn2.width, g_butn2.height, 0);
        }
    } else if (mode == OG_UI_BIOG) {
        paint_biography();
    } else if (mode == OG_UI_REPUTE) {
        char plain[512];
        log_format(plain, sizeof(plain),
            "Your reputation has changed as follows:\nCommoners: %d\nMerchants: %d\nScholars: %d\nNobility: %d\nUnderworld: %d",
            who->reputation[0], who->reputation[1], who->reputation[2], who->reputation[3], who->reputation[4]);
        paint_parchment(plain, 0, 0, 0, 0);
    } else if (mode == OG_UI_NAME) {
        paint_sheet(0);
        text_at(80, 5, who->name);
        text_at(80 + smallfont_width(&g_font, who->name), 5, "_");
    } else if (mode == OG_UI_FACE) {
        paint_sheet(1);
        ensure_faces(who->race, who->female);
        paint_portrait(who->face);
    } else if (mode == OG_UI_ATTR) {
        paint_sheet(2);
        paint_attributes(0);
    } else if (mode == OG_UI_SKILL) {
        paint_sheet(3);
        paint_skills();
    } else if (mode == OG_UI_SUMMARY) {
        paint_sheet(4);
        ensure_faces(who->race, who->female);
        paint_portrait(who->face);
        text_at(80, 5, who->name);
        paint_attributes(1);
        paint_skills();
        paint_reflex(1);
    } else if (mode == OG_UI_REFLEX) {
        paint_sheet(5);
        paint_reflex(0);
    } else {
        lay_map();
    }
    if (og_chargen_notice()[0]) {
        char lines[16][96];
        int n, x, y, cols, rows, ty;
        n = take_lines(og_chargen_notice(), lines, 16);
        paper_box(lines, n, 0, &x, &y, &cols, &rows, &ty);
        lay_paper(x, y, cols, rows);
        paint_lines(lines, n, 1, x, ty, 146);
    }
    kind = screen_kind(mode);
    publish(kind);
}
