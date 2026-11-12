#include "game/ogfall/play/state.h"
#include "game/ogfall/play/possessions.h"
#include "game/ogfall/play/activate.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/weapon.h"
#include "game/ogfall/play/screen.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/play/actions.h"
#include "game/ogfall/play/encounter.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/cheats.h"
#include "game/ogfall/play/travel.h"
#include "game/ogfall/play/maps.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/play/ground.h"
#include "game/dagger/places/dunstream.h"
#include "game/ogfall/play/spellmaker.h"
#include "game/ogfall/play/itemmaker.h"
#include "game/ogfall/play/enchant.h"
#include "game/ogfall/play/quest.h"
#include "game/ogfall/tables/magicdefs.h"
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
#include "engine/scene/interact.h"
#include "engine/scene/light.h"
#include "engine/scene/stream.h"

#include "game/dagger/formats/bind.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/formats/faces.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/people.h"
#include "game/ogfall/play/shelf.h"
#include "game/dagger/play/words.h"
#include "game/dagger/places/town.h"
#include "game/ogfall/tables/holidaydefs.h"
#include "game/ogfall/tables/datedefs.h"
#include "game/ogfall/play/bank.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/names.h"
#include "game/ogfall/play/talk.h"
#include "game/ogfall/play/steal.h"
#include "game/ogfall/play/guild.h"
#include "game/ogfall/play/potion.h"
#include "game/ogfall/play/magicitem.h"
#include "game/ogfall/play/spellshop.h"
#include "game/ogfall/play/hours.h"
#include "game/ogfall/play/entry.h"
#include "game/ogfall/play/hud.h"
#include "game/ogfall/play/ailments.h"
#include "game/ogfall/formats/interior.h"
#include "game/ogfall/play/talkwin.h"
#include "game/dagger/play/character.h"
#include "game/ogfall/play/standing.h"
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
#include "engine/core/prof.h"
#include "game/dagger/places/world.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/places/wild.h"
#include "game/dagger/places/wres.h"
#include "game/dagger/formats/worldmap.h"
#include "game/dagger/play/travelmap.h"
#include "game/dagger/play/dunmap.h"
#include "game/dagger/play/townmap.h"
#include "game/dagger/play/marker.h"
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
static int g_place_open;
static Dungeon g_place_data;
static OgWhere g_saved_where;
static Level g_level;
static void weapon_draw(const OgWeaponView *view, float flash);
static int load_cast_art(void);
static void cast_draw(int element, int frame);
static void magic_hud_draw(const OgMagicState *state);
static void magic_lighting(int enabled);
/*
 * A door from the street into a building (building_try_enter): its hours, a guild's own door, a house by day, a lock picked in Steal mode (and the crime of it), else how hard
 * the lock is. Anything else, a dungeon's door, the way out of a building, goes straight to travel_use.
 */
static unsigned talk_building_id(const TownBuilding *t);

static const char *passage_gate(const Usable *u)
{
    const TownBuilding *t;
    Usable in;
    OgEntryBuilding b;
    OgEntryPlayer p;
    OgEntry e;
    OgMembership *m;
    int region, index, minute;

    if (u->kind == USE_PASSAGE && travel_door_in_town(u, &in, &region, &index) && (t = town_building(region, index, u->tag >> 8, u->tag & 0xff)) != 0) {
        mem_zero(&b, sizeof(b));
        b.id = talk_building_id(t);
        b.type = t->type; b.faction = t->faction; b.name_seed = t->name_seed;
        b.quest_open = og_quest_building_open(region, index, t->block, t->record);
        b.access = og_quest_building_access(region, index, t->block, t->record, og_state()->minutes);
        mem_zero(&p, sizeof(p));
        p.house = (df_u32)og_bank()->house;
        m = og_guild_find_kind(0); p.brotherhood = m ? m->rank : -1;
        m = og_guild_find_kind(3); p.thieves = m ? m->rank : -1;
        m = og_guild_find_kind(2); p.fighters = m ? m->rank : -1;
        m = og_guild_find_kind(1); p.mages = m ? m->rank : -1;
        minute = (int)(og_state()->minutes % 1440u);
        p.daytime = minute >= 360 && minute < 1080;
        p.steal = og_hud_mode() == OG_MODE_STEAL;
        p.lock_spell = (og_character()->conditions & 0x40) != 0;
        if (!og_building_try_enter(&b, &p, og_state()->minutes, (og_guards_witnesses() & 3) != 0, &e)) {
            /* click_show_building_info: a shop shut for the hour says when it opens (FALL.EXE's "Store is closed. Open from %d:00 to %d:00."). In Steal mode the lock's own line is what matters. */
            if (b.type < 14 && b.type != 1 && p.steal == 0) {
                static char hours[64];
                int from, to;

                if (og_building_hours(b.type, &from, &to)) {
                    log_format(hours, sizeof(hours), "Store is closed. Open from %d:00 to %d:00.", from, to);
                    return hours;
                }
            }
            return e.line ? e.line : "The door is locked.";
        }
        if (e.spent_spell) { og_character()->conditions &= ~0x40; }
        if (e.crime) { og_law_note(OG_CRIME_ATTEMPTED_BREAKING); og_guards_summon(1); }
        if (e.text) { og_msg_set(og_words(e.text)); }
        else if (e.picked && e.line) { og_msg_set(e.line); }
    }
    return travel_use(u);
}

static void install_travel(void);
static int seam_world_art(void);
static void install_maps(void);
static void present_maps(void);
static int g_maps_mode;
static df_u8 g_maps_rgb[768];
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
#define ENCOUNTER_ART 20
typedef struct EncounterArt { int kind, archive, rate; Entity entity; } EncounterArt;
static EncounterArt g_encounter_art[ENCOUNTER_ART];
static int g_encounter_art_count, g_original_entities;
static Entity *g_game_entities;
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
    if (g_have_art_rgb) {
        df_u8 pixel;
        pixel = 145; og_pic_set(OG_PIC_BREATH,pic_load(&pixel,1,1,g_art_rgb,0));
        pixel = 246; og_pic_set(OG_PIC_BREATH_LOW,pic_load(&pixel,1,1,g_art_rgb,0));
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
            if (frame) { og_pic_set(OG_PIC_OVERLAY+i,pic_load(frame->pixels,frame->w,frame->h,rgb,1)); }
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

/*
 * The faction table, from FACTION.TXT as the dagger reader parses it, into
 * standing.c's plain records (people and law, slice 1). The tree and the allies
 * and enemies keep the file's ids; the file counts regions from 1 and the
 * reader from 0, as the decomp's regions run.
 */
static OgFactionDef g_faction_def[OG_FACTIONS_MAX];

static int load_factions(void)
{
    int i, k, n;

    if (!factionfile_open()) {
        return 0;
    }
    n = faction_count();
    if (n > OG_FACTIONS_MAX) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        const Faction *f;
        OgFactionDef *d;

        f = faction_at(i);
        d = &g_faction_def[i];
        mem_zero(d, sizeof(*d));
        d->id = f->id;
        d->parent = f->parent;
        d->type = f->type;
        d->region = f->region;
        d->rep = f->rep;
        d->power = f->power;
        d->flags = f->flags;
        d->social_group = f->sgroup;
        d->guild_group = f->ggroup;
        for (k = 0; k < OG_FACTION_LINKS && k < FACTION_LINKS; k++) {
            d->ally[k] = f->ally[k];
            d->enemy[k] = f->enemy[k];
        }
        for (k = 0; k < OG_FACTION_NAME - 1 && f->name[k]; k++) {
            d->name[k] = f->name[k];
        }
    }
    return og_factions_define(g_faction_def, n);
}

/*
 * A shop's shelves (people and law, slice 2): a shelf used opens the inventory as a shop over that
 * shelf's own stock. The stock is made the first time a shelf is used in a visit and gone with the
 * visit (building_load_interior clears and restocks at each entry); a visit is one level and one
 * building. The building's type and quality come from the town's blocks (travel_building), the
 * prices from the region's standing, the holiday from the clock.
 */
#define SHELVES_MOST 64
static df_u32 g_shelf_box[SHELVES_MOST];
static const Level *g_shelf_level;
static unsigned g_shelf_place;

static void forget_shelves(void)
{
    int i;

    for (i = 0; i < SHELVES_MOST; i++) {
        if (g_shelf_box[i]) {
            og_shelf_discard(g_shelf_box[i]);
            g_shelf_box[i] = 0;
        }
    }
}

/*
 * The shopkeeper's offer (TEXT.RSC 260..265): one variant picked at random with its macros said by
 * the dagger reader: %a the price, %cn the town. The shop's own name (%cpn) comes from the
 * decomp's building_name, not built yet, so the shop is "This shop" for now.
 */
#ifndef DF_HOST
/*
 * The dagger reader draws its variants with GFALL's dice, which OGFALL does not link: on the
 * target they come from the session's own stream. (The host build links GFALL's combat, and its
 * own.)
 */
int combat_roll(int n)
{
    return n > 0 ? og_random_range(0, n - 1) : 0;
}
#endif

/* The Status key's first box: TEXT.RSC 22 ("You are in %cn. It is %tim on %dat. In the eyes of the law of %crn, you are %ltn.") with the parse.c macros said: the town or the region, the time as %d:%02d, the date, the region and the law's opinion of the player. */
static int status_intro(char *out, int cap)
{
    const char *raw, *town;
    char time[16], date[48], region[32];
    int i, n, minute;

    raw = og_words(22);
    if (!raw || !raw[0]) { return 0; }
    {
        const Faction *province;

        province = faction_province_of(travel_region());
        df_copy(region, province ? province->name : "", (int)sizeof(region));
    }
    town = town_name();
    if (!town || !town[0]) { town = region; }
    minute = (int)(og_state()->minutes % 1440u);
    log_format(time, sizeof(time), minute % 60 < 10 ? "%d:0%d" : "%d:%d", minute / 60, minute % 60);
    og_date_text(og_state()->minutes, date, sizeof(date));
    n = 0;
    for (i = 0; raw[i] && n < cap - 1; ) {
        const char *with;

        with = 0;
        if (raw[i] == '%') {
            if (raw[i + 1] == 'c' && raw[i + 2] == 'n') { with = town; i += 3; }
            else if (raw[i + 1] == 't' && raw[i + 2] == 'i' && raw[i + 3] == 'm') { with = time; i += 4; }
            else if (raw[i + 1] == 'd' && raw[i + 2] == 'a' && raw[i + 3] == 't') { with = date; i += 4; }
            else if (raw[i + 1] == 'c' && raw[i + 2] == 'r' && raw[i + 3] == 'n') { with = region; i += 4; }
            else if (raw[i + 1] == 'l' && raw[i + 2] == 't' && raw[i + 3] == 'n') { with = og_legal_title(og_region_legal(og_state()->region)); i += 4; }
        }
        if (with) {
            int k;

            for (k = 0; with[k] && n < cap - 1; k++) { out[n++] = with[k]; }
        } else {
            out[n++] = raw[i++];
        }
    }
    out[n] = 0;
    return n;
}

/* The world the enchantments' ticks ask about (disease.c player_in_daylight, player_in_temple, the creatures round the player). */
static int ench_daylight(void)
{
    int minute;

    minute = (int)(og_state()->minutes % 1440u);
    return og_world_state()->where.kind == OG_PLACE_WORLD && minute >= 360 && minute < 1080;
}

static int ench_temple(void)
{
    int type, faction, quality;
    unsigned place;

    return travel_building(&type, &faction, &quality, &place) && type == 14;
}

static int ench_category_near(int category, int distance)
{
    Camera cam;

    og_move_camera(&cam);
    return og_dungeon_category_within(&cam, category, distance);
}

static int ench_hurt_nearest(int amount)
{
    Camera cam;

    og_move_camera(&cam);
    return og_dungeon_hurt_nearest(&cam, 128.0f, amount);
}

/* The quests' words: a message of the quest's own QRC (TEXT.RSC's format), or a record of TEXT.RSC itself; shown in a message box. Macros and symbols are the later steps. */
static int quest_text(const char *quest, int message, int in_text_rsc, char *out, int cap)
{
    char file[16];

    if (in_text_rsc) { return textrsc_read(message, out, cap); }
    log_format(file, sizeof(file), "%s.QRC", quest);
    return textrsc_read_in(file, message, out, cap);
}

static void quest_say(const char *text) { og_possessions_message(text, 0); }

/* The TEXT.RSC macros of a quest's text: the player's name and race, the town and the region. */
static int quest_macros(const char *in, char *out, int cap)
{
    static Character pc;
    Words w;
    int i;
    char province[32];
    const Faction *faction;

    for (i = 0; i < (int)sizeof(pc.name) - 1 && og_character()->name[i]; i++) { pc.name[i] = og_character()->name[i]; }
    pc.name[i] = 0;
    pc.race = og_character()->race; pc.female = og_character()->female;
    faction = faction_province_of(travel_region());
    df_copy(province, faction ? faction->name : "", sizeof(province));
    mem_zero(&w, sizeof(w));
    w.pc = &pc; w.city = town_name(); w.region = province;
    words_use(&w);
    words_expand(in, out, cap);
    for (i = 0; out[i]; i++) { }
    return i;
}

/* What the quests ask of the map: the travel list's locations, where the player stands, a region's name, the journey the quests estimate (ships' sea pixels cost 51) and the map's discovery. */
/* The index of the outside the player is in or under, in its region's list (the travel list's index): a dungeon's is the outside it lies under. -1 when in no location. */
static int quest_outside(void)
{
    const OgWhere *where;
    const Dungeon *data;
    int index;

    where = &og_world_state()->where;
    if (where->kind == OG_PLACE_DUNGEON) {
        data = travel_place_data();
        index = data ? dungeon_outside(where->region, data->exterior_id) : -1;
        return index;
    }
    return where->located ? where->index : -1;
}

static void quest_here(int *region, int *index, int *px, int *py)
{
    og_travel_capture();
    *region = travel_region();
    *index = quest_outside();
    *px = og_world_state()->px; *py = og_world_state()->py;
}

/* A location's number (MAPS' LOC_ID, 1 for an outside and its dungeon the next): the region and the outside's index in it, found by reading each region's two lists once and kept. */
static int quest_find_number(int number, int *region, int *index)
{
    static struct { int number, region, index; } cache[16];
    static int cached;
    static DunRef list[2048];
    int r, n, i, k, count;

    for (k = 0; k < cached; k++) { if (cache[k].number == number) { *region = cache[k].region; *index = cache[k].index; return cache[k].index >= 0; } }
    *region = -1; *index = -1;
    for (r = 0; r < DUN_REGIONS && *index < 0; r++) {
        count = exterior_list_region(r, list, 2048);
        if (count > 2048) { count = 2048; }
        for (i = 0; i < count; i++) { if (list[i].location_id == number) { *region = r; *index = i; break; } }
    }
    if (*index < 0) {
        /* A dungeon's number: its record in the dungeon list names the outside it lies under. */
        static DunRef dungeons[4400];

        n = dungeon_list(dungeons, 4400);
        if (n > 4400) { n = 4400; }
        for (i = 0; i < n && *index < 0; i++) {
            if (dungeons[i].location_id == number) {
                k = dungeon_outside(dungeons[i].region, dungeons[i].exterior_id);
                if (k >= 0) { *region = dungeons[i].region; *index = k; }
            }
        }
    }
    if (cached < 16) { cache[cached].number = number; cache[cached].region = *region; cache[cached].index = *index; cached++; }
    return *index >= 0;
}

static int quest_region_name(int region, char *out, int cap)
{
    const Faction *faction;

    faction = faction_province_of(region);
    df_copy(out, faction ? faction->name : "", cap);
    return faction ? 1 : 0;
}

/* quest_place_object: a quest's creature among the dungeon's actors, unseen and near the player. */
static int quest_spawn_foe(int kind, df_u32 thing)
{
    Camera cam;

    og_move_camera(&cam);
    return og_encounter_place(&cam, kind, thing);
}

/* A pile at the player's feet for a quest's item: the container the drop screen would make. */
static df_u32 quest_place_item(void)
{
    return og_ground_container(og_state()->x,
        og_world_state()->where.kind == OG_PLACE_WORLD ? og_move_body()->feet[1] : og_state()->eye_y - 32,
        og_state()->z);
}

static int quest_journey(int sx, int sy, int ex, int ey)
{
    OgJourney journey;

    og_journey_plan(sx, sy, ex, ey, 0, 0, 1, og_journey_transport(), &journey);
    return journey.minutes;
}

/* The place a logbook entry is written in: the town, else the region. */
static const char *quest_place(void)
{
    static char place[40];
    const char *town;
    const Faction *faction;

    town = town_name();
    if (town && town[0]) { df_copy(place, town, sizeof(place)); return place; }
    faction = faction_province_of(travel_region());
    df_copy(place, faction ? faction->name : "", sizeof(place));
    return place;
}

static int offer_say(int text, int price, char *out, int cap)
{
    Words w;

    mem_zero(&w, sizeof(w));
    w.amount = price;
    w.city = town_name();
    w.place = "This shop";
    words_use(&w);
    return words_say(text, out, cap);
}

/*
 * The talk window's world (people and law, slice 6): the town's buildings as the player stands in it, their signs,
 * which way they lie, and the words with their macros said. The window itself is play/talkwin.c; this is only
 * what it asks the game for. A building is named for the sign over it (names.c) and found again by an id made of
 * its block and record.
 */
static Camera g_talk_cam;
static int g_talk_region, g_talk_index;
static float g_talk_x, g_talk_z;
static int g_talk_in_town;
static char g_talk_name[OG_NAME_MAX];
static char g_talk_region_name[32];
static df_u8 g_talk_face[FACE_W * FACE_H];
static int g_talk_face_ok;
static Words g_talk_words;

static unsigned talk_building_id(const TownBuilding *t)
{
    return ((unsigned)(t->block + 1) << 8) | (unsigned)t->record;
}

static const TownBuilding *talk_building(unsigned id)
{
    int count, i;
    const TownBuilding *rows;

    if (!g_talk_in_town) {
        return 0;
    }
    rows = town_buildings(g_talk_region, g_talk_index, &count);
    for (i = 0; i < count; i++) {
        if (talk_building_id(&rows[i]) == id) {
            return &rows[i];
        }
    }
    return 0;
}

/* xn_math_approx_dist2d is not in the tree: the longer side and half the shorter. */
static int talk_distance_to(const TownBuilding *t)
{
    float dx, dz;
    int a, b;

    dx = t->x - g_talk_x;
    dz = t->z - g_talk_z;
    a = (int)(dx < 0 ? -dx : dx);
    b = (int)(dz < 0 ? -dz : dz);
    return a > b ? a + b / 2 : b + a / 2;
}

static int talk_buildings(OgTalkBuilding *out, int cap)
{
    int count, i, n;
    const TownBuilding *rows;

    if (!g_talk_in_town) {
        return 0;
    }
    rows = town_buildings(g_talk_region, g_talk_index, &count);
    for (i = n = 0; i < count && n < cap; i++, n++) {
        out[n].id = talk_building_id(&rows[i]);
        out[n].type = rows[i].type;
        out[n].flags = 0;
        out[n].distance = talk_distance_to(&rows[i]);
    }
    return n;
}

static void talk_building_name(unsigned id, char *out, int cap)
{
    const TownBuilding *t;
    OgBuildingName b;
    const OgFactionDef *f;

    out[0] = 0;
    t = talk_building(id);
    if (!t) {
        return;
    }
    mem_zero(&b, sizeof(b));
    b.type = t->type;
    b.faction = t->faction;
    b.name_seed = t->name_seed;
    b.region_name = g_talk_region_name;
    f = og_faction_def(og_faction_index(t->faction));
    if (f) {
        b.faction_name = f->name;
    }
    og_building_name(&b, out, cap);
}

static int talk_town_has(int key, int by_type)
{
    int count, i;
    const TownBuilding *rows;

    if (!g_talk_in_town) {
        return 0;
    }
    rows = town_buildings(g_talk_region, g_talk_index, &count);
    for (i = 0; i < count; i++) {
        if (by_type ? rows[i].type == key : rows[i].faction == key) {
            return 1;
        }
    }
    return 0;
}

static int talk_distance(unsigned id)
{
    const TownBuilding *t;

    t = talk_building(id);
    return t ? talk_distance_to(t) : 99999;
}

/* Which of the eight ways (east first, counter-clockwise) a building lies, by the slope's three bands (tan 22.5 degrees is about 0.414). */
static int compass_index(float dx, float dz)
{
    int east, north;

    east = dx > 0.0f ? 1 : -1;
    north = dz > 0.0f ? 1 : -1;
    dx = dx < 0.0f ? -dx : dx;
    dz = dz < 0.0f ? -dz : dz;
    if (dz < dx * 0.414f) { return east > 0 ? 0 : 4; }
    if (dx < dz * 0.414f) { return north > 0 ? 2 : 6; }
    if (north > 0) { return east > 0 ? 1 : 3; }
    return east > 0 ? 7 : 5;
}

static char g_pq_name[40], g_pq_place[64];

/* A record said with the window's macros: %key what is asked, %loc the place, %di its way, %hnt the hint already said. */
static int talk_say(int text, const OgTalkWinSay *ctx, char *out, int cap)
{
    static const char *const compass[8] = { "east", "northeast", "north", "northwest", "west", "southwest", "south", "southeast" };
    const TownBuilding *t;

    mem_zero(&g_talk_words, sizeof(g_talk_words));
    g_talk_words.key = ctx->key;
    g_talk_words.loc = ctx->loc ? ctx->loc : ctx->key;
    g_talk_words.hint = ctx->hint;
    g_talk_words.city = g_talk_in_town ? town_name() : "";
    g_talk_words.region = g_talk_region_name;
    g_talk_words.place = "This place";
    g_talk_words.questor = g_pq_name; g_talk_words.questor_place = g_pq_place;
    g_talk_words.tone = ctx->tone;
    t = ctx->building ? talk_building(ctx->building) : 0;
    if (t) {
        g_talk_words.direction = compass[compass_index(t->x - g_talk_x, t->z - g_talk_z)];
    }
    words_use(&g_talk_words);
    return words_say(text, out, cap);
}

static int talk_width(const char *text)
{
    return smallfont_width(&g_font, text);
}

static void talk_expand(const char *in, char *out, int cap)
{
    mem_zero(&g_talk_words, sizeof(g_talk_words));
    g_talk_words.city = g_talk_in_town ? town_name() : "";
    g_talk_words.region = g_talk_region_name;
    words_use(&g_talk_words);
    words_expand(in, out, cap);
}

/*
 * People met (people and law, slice 8), as click_pedestrian, click_npc and npc_click_service have them (src/lifted/click.c).
 * A person in the street: Steal tries their pocket, every other mode talks (info too). A person standing in a
 * building (a usable): info names them; the others, within 256 units, offer a shopkeeper's panel or talk (npc_click_service).
 * Whoever is picked is remembered here until the window or a panel acts on them.
 */
#define PERSON_TRIED_MOST 32
static unsigned g_person_tried[PERSON_TRIED_MOST];
static int g_person_tried_count;

typedef struct Met {
    int faction, archive, record, female;
    unsigned who;
    char name[OG_NAME_MAX];
    Camera cam;
} Met;
static Met g_met;

static int person_tried(unsigned who)
{
    int i;

    for (i = 0; i < g_person_tried_count && i < PERSON_TRIED_MOST; i++) {
        if (g_person_tried[i] == who) {
            return 1;
        }
    }
    return 0;
}

static void talk_say_text(int text, char *out, int cap)
{
    OgTalkWinSay ctx;

    mem_zero(&ctx, sizeof(ctx));
    ctx.tone = 1;
    if (talk_say(text, &ctx, out, cap) < 0) {
        out[0] = 0;
    }
}

static void person_say_text(int text, char *out, int cap)
{
    Words w;

    mem_zero(&w, sizeof(w));
    w.city = town_name();
    w.tone = 1;
    w.place = "This place";
    words_use(&w);
    if (words_say(text, out, cap) < 0) {
        out[0] = 0;
    }
}

/* talk_start for whoever g_met is: the window opens, or the record that says they will not (shown as a message). */
static int quest_news(int faction, char *out, int cap) { return og_quest_news(faction, out, cap); }
static int quest_rumour_for(unsigned npc, char *out, int cap) { return og_quest_rumour_for((int)npc, out, cap); }

static int g_talk_events_done;
static int potential_questor(char *name, int ncap, char *place, int pcap);
static char g_pq_name[40], g_pq_place[64];

static void start_talk(void)
{
    OgTalkWho target;
    OgTalkPlayer player;
    OgTalkWinWorld world;
    static int members[OG_MEMBERSHIPS];
    static char line[400];
    TravelWhere here;
    const Faction *province;
    int text, origin[2], block, record, index, region;
    float x, z;

    /* A quest's person met: its event 28 runs on the talk path, and a quest that takes the click answers in its own words. */
    if (!g_talk_events_done && og_quest_talk((int)g_met.who, g_met.faction)) { return; }
    g_talk_events_done = 0;
    mem_zero(&target, sizeof(target));
    target.npc = g_met.who;
    target.faction = g_met.faction;
    target.person = 1;
    /* The factions the player belongs to, and the rank held in a local temple (guild_local_temple_rank). */
    player.member_count = 0;
    for (index = 0; index < OG_MEMBERSHIPS; index++) {
        if (og_guilds()->m[index].used) {
            members[player.member_count++] = og_guilds()->m[index].faction;
        }
    }
    player.members = members;
    player.temple_rank = og_guild_local_temple_rank(travel_region());
    /* Where the talk is: the town the player stands in (for the places asked after) and the region's name. */
    g_talk_cam = g_met.cam;
    g_talk_in_town = travel_town_here(&g_talk_cam, &region, &index, &x, &z, &block, &record);
    g_talk_region = region;
    g_talk_index = index;
    g_talk_x = x;
    g_talk_z = z;
    province = faction_province_of(travel_region());
    df_copy(g_talk_region_name, province ? province->name : "", (int)sizeof(g_talk_region_name));
    df_copy(g_talk_name, g_met.name, (int)sizeof(g_talk_name));
    travel_here(&here, origin);
    mem_zero(&world, sizeof(world));
    world.buildings = talk_buildings;
    world.building_name = talk_building_name;
    world.town_has = talk_town_has;
    world.distance = talk_distance;
    world.say = talk_say;
    world.news = quest_news;
    world.topic_count = og_quest_topic_count; world.topic_name = og_quest_topic_name; world.topic_answer = og_quest_topic_answer;
    world.rumour_for = quest_rumour_for;
    world.width = talk_width;
    world.outdoors = here.kind != TRAVEL_INSIDE;
    world.region = travel_region();
    /* quest_find_potential_questor (4.2): no work in a dungeon or where no building is marked; else a marked building's person is named, and the building. */
    world.no_work = !potential_questor(g_pq_name, (int)sizeof(g_pq_name), g_pq_place, (int)sizeof(g_pq_place));
    og_talkwin_world(&world);
    g_talk_face_ok = faces_read(faces_of_flat(g_met.archive, g_met.record), g_talk_face);
    if (!og_possessions_talk(&target, &player, 0, &text) && text) {
        talk_say_text(text, line, (int)sizeof(line));
        og_possessions_message(line, 0);
    }
}

static int person_used(const Camera *aim, int mode)
{
    const Faction *people;
    OgNpcName npc;
    int female;
    unsigned who;
    float along;

    if (!people_in_view_at(aim, TALK_REACH, &female, &who, &along)) {
        return 0;
    }
    people = faction_people_of(travel_region());
    if (!people) {
        return 0;
    }
    mem_zero(&npc, sizeof(npc));
    npc.type = 8;
    npc.region = travel_region();
    npc.id = who;
    npc.female = female;
    mem_zero(&g_met, sizeof(g_met));
    og_npc_name(&npc, g_met.name, (int)sizeof(g_met.name));
    g_met.faction = people->id;
    g_met.archive = people->flat_archive[female];
    g_met.record = people->flat_record[female];
    g_met.female = female;
    g_met.who = who;
    g_met.cam = *aim;
    if (mode == OG_MODE_STEAL) {
        OgSteal taken;
        static char line[96];

        /* click_pedestrian: nearer than 128, and the person is marked before the try, so each is tried once, win or lose. */
        if (along > 128.0f || person_tried(who)) {
            return 1;
        }
        if (g_person_tried_count < PERSON_TRIED_MOST) {
            g_person_tried[g_person_tried_count++] = who;
        }
        og_steal_pocket(0, 0, &taken);
        if (taken.text) {
            person_say_text(taken.text, line, (int)sizeof(line));
            interact_say(line);
        } else if (taken.line) {
            interact_say(taken.line);
        }
        return 1;
    }
    start_talk();
    return 1;
}

/*
 * Whether the person met in this building keeps its shop: the block's own people (og_interior_people) carry the flag the
 * dagger layer drops. The person is found by their picture and faction among those of the building the player stands in.
 */
static int interior_shopkeeper(const Camera *cam, int faction, int image)
{
    OgInteriorPerson people[OG_INTERIOR_PEOPLE];
    const unsigned char *bytes;
    unsigned size;
    int region, index, block, record, n, i;
    float x, z;

    if (!travel_town_here(cam, &region, &index, &x, &z, &block, &record) || block < 0) {
        return 0;
    }
    bytes = town_block_bytes(region, index, block, &size);
    n = og_interior_people(bytes, size, record, people, OG_INTERIOR_PEOPLE);
    for (i = 0; i < n; i++) {
        if (people[i].image == image && people[i].faction == faction) {
            return og_interior_shopkeeper(&people[i]);
        }
    }
    return 0;
}

/*
 * A person standing in a building (a usable of USE_TALK: their faction and flat). npc_click_service: within a shop or a
 * bank, a shopkeeper offers a panel; anyone else talks. The shopkeeper's flag (person flags & 8) is read from the block
 * by OGFALL's own reader (interior_shopkeeper). The armorer and the general store test the faction number's bit 8 where
 * the others test the flag, as the decomp has it.
 */
static void hall_enter(int person_faction, int hall_faction);
static int hall_member(int kind);

/* quest_mark_givers (4.1 of quest-hooks.md): a person standing in a building of a type other than 1, 11, 14 and 16 up, of a faction of type 2, 12 or 4 not under the Mages', Fighters', Thieves' or
 * Brotherhood's trees, is a giver for the day by a roll (type 4 at 51%, the others 26%). The original rolls each person from `rand` reseeded with the day; we roll each building from the day and its id,
 * so all its people are marked or none (the Work answer can then name the building). */
static int building_marked(unsigned place, int building_type, const OgFactionDef *f)
{
    int root, guard;
    const OgFactionDef *top;
    unsigned roll;

    if (!f || building_type == 1 || building_type == 11 || building_type == 14 || building_type >= 16 || building_type < 0) { return 0; }
    if (!(f->type == 4 || f->type == 2 || f->type == 12)) { return 0; }
    root = f->id; top = f;
    for (guard = 0; top && top->parent && guard < 8; guard++) { top = og_faction_def(og_faction_index(top->parent)); if (top) { root = top->id; } }
    if (root == 40 || root == 41 || root == 42 || root == 108) { return 0; }
    roll = (unsigned)(og_state()->minutes / 1440u) * 2654435761u ^ place * 40503u;
    roll = (roll >> 8) % 100u;
    return (int)roll <= (f->type == 4 ? 50 : 25);
}

/* The marks a click cleared today ("the person's mark is cleared at once, whether or not a file was found"), by day and building. */
static struct { unsigned day, place; } g_cleared[16];
static int g_cleared_next;

static int mark_cleared(unsigned place)
{
    int i;

    for (i = 0; i < 16; i++) { if (g_cleared[i].place == place && g_cleared[i].day == (unsigned)(og_state()->minutes / 1440u) + 1u) { return 1; } }
    return 0;
}

/* The giver the Work answer points to (%pqn, %pqp): one of the town's marked buildings picked by the day, a name made by the town's naming, and the building's own. 0 away from a town or when none is
 * marked. */
static int potential_questor(char *name, int ncap, char *place, int pcap)
{
    const TownBuilding *rows;
    int count, i, n, pick;
    OgNpcName npc;

    if (!g_talk_in_town || og_world_state()->where.kind == OG_PLACE_DUNGEON) { return 0; }
    rows = town_buildings(g_talk_region, g_talk_index, &count);
    n = 0;
    for (i = 0; rows && i < count; i++) {
        if (building_marked(talk_building_id(&rows[i]), rows[i].type, og_faction_def(og_faction_index(rows[i].faction))) && !mark_cleared(talk_building_id(&rows[i]))) { n++; }
    }
    if (!n) { return 0; }
    pick = (int)(((unsigned)(og_state()->minutes / 1440u) * 2246822519u >> 7) % (unsigned)n);
    for (i = 0; i < count; i++) {
        if (!(building_marked(talk_building_id(&rows[i]), rows[i].type, og_faction_def(og_faction_index(rows[i].faction))) && !mark_cleared(talk_building_id(&rows[i]))) || pick-- != 0) { continue; }
        mem_zero(&npc, sizeof(npc));
        npc.type = 8; npc.region = travel_region(); npc.id = talk_building_id(&rows[i]) * 31u + (unsigned)(og_state()->minutes / 1440u); npc.faction = rows[i].faction;
        og_npc_name(&npc, name, ncap);
        talk_building_name(talk_building_id(&rows[i]), place, pcap);
        return 1;
    }
    return 0;
}

/* Clicking a marked person (after the quest events of the click) offers a quest: R for a type 4 faction other than 407, else A or K, for a non-member at the player's level, the one their id picks.
 * 1 when one was offered. */
static int street_quest(const OgFactionDef *f, int building_type, unsigned place)
{
    char name[9];

    if (!building_marked(place, building_type, f) || mark_cleared(place) || og_quest_person_busy((int)g_met.who)) { return 0; }
    g_cleared[g_cleared_next].day = (unsigned)(og_state()->minutes / 1440u) + 1u; g_cleared[g_cleared_next].place = place; g_cleared_next = (g_cleared_next + 1) & 15;
    if (!og_quest_pick_seeded(f->type == 4 && f->id != 407 ? 'R' : 'A', f->type == 4 && f->id != 407 ? 0 : 'K', 'C', og_character()->level, g_met.who, name)) { return 0; }
    og_quest_set_questor((int)g_met.who, g_met.faction, g_met.name, town_name());
    return og_quest_begin(name, g_met.faction) >= 0;
}

/* A quest marker of the dungeon the player is in (dunstream's), a quest's creature on the floor beside it, and a pile there for its item. */
static int quest_marker_at(int marker, int record, unsigned seed, float *x, float *y, float *z)
{
    return dunstream_active() && dunstream_quest_marker(marker, record, seed, x, y, z);
}

static int quest_spawn_foe_at(int kind, df_u32 thing, float x, float y, float z)
{
    Camera cam;

    og_move_camera(&cam);
    return og_encounter_place_at(&cam, kind, thing, x, y, z);
}

static df_u32 quest_place_item_at(float x, float y, float z)
{
    return og_ground_container(x, y, z);
}

/* A building of a town for a quest's place: of the type asked (17..20 any house, below 0 any) picked by the seed. */
static int quest_pick_building(int region, int index, int type, unsigned seed, int *block, int *record)
{
    const TownBuilding *list;
    int count, i, n, pick;

    list = town_buildings(region, index, &count);
    if (!list) { return 0; }
    n = 0;
    for (i = 0; i < count; i++) {
        if (type < 0 ? (list[i].type < 17 && list[i].type != 1) : (list[i].type == type || (type >= 17 && type <= 20 && list[i].type >= 17 && list[i].type <= 20))) { n++; }
    }
    if (!n) { return 0; }
    pick = (int)(seed % (unsigned)n);
    for (i = 0; i < count; i++) {
        if (type < 0 ? (list[i].type < 17 && list[i].type != 1) : (list[i].type == type || (type >= 17 && type <= 20 && list[i].type >= 17 && list[i].type <= 20))) {
            if (pick-- == 0) { *block = list[i].block; *record = list[i].record; return 1; }
        }
    }
    return 0;
}

static int quest_outdoors(void) { return og_world_state()->where.kind == OG_PLACE_WORLD; }

/* A building of a town for a person's site: of a type, any shop-like one, of a faction, or of a faction of a type. */
static int site_matches(const TownBuilding *b, int mode, int value)
{
    const OgFactionDef *f;

    if (mode == 0) { return value == 11 ? (b->type == 11 && b->faction == 40) : (b->type == value || (value >= 17 && value <= 20 && b->type >= 17 && b->type <= 20)); }
    if (mode == 1) { return b->type < 17; }
    if (mode == 2) { return b->faction == value; }
    f = b->faction ? og_faction_def(og_faction_index(b->faction)) : 0;
    return f && f->type == value;
}

static int quest_pick_site(int region, int index, int mode, int value, unsigned seed, int *block, int *record, int *faction)
{
    const TownBuilding *list;
    int count, i, n, pick;

    list = town_buildings(region, index, &count);
    if (!list) { return 0; }
    n = 0;
    for (i = 0; i < count; i++) { if (site_matches(&list[i], mode, value)) { n++; } }
    if (!n) { return 0; }
    pick = (int)(seed % (unsigned)n);
    for (i = 0; i < count; i++) {
        if (site_matches(&list[i], mode, value) && pick-- == 0) { *block = list[i].block; *record = list[i].record; *faction = list[i].faction; return 1; }
    }
    return 0;
}

/* A quest person's name, as any person of the town is named: by their id, sex and faction. */
static void quest_person_name(int who, int faction, int female, char *out, int cap)
{
    OgNpcName npc;
    const OgFactionDef *f;

    mem_zero(&npc, sizeof(npc));
    f = faction ? og_faction_def(og_faction_index(faction)) : 0;
    npc.type = 8; npc.region = travel_region(); npc.id = (unsigned)who; npc.female = female;
    npc.faction = faction; npc.faction_is_individual = f && f->type == 4; npc.faction_name = f ? f->name : 0;
    og_npc_name(&npc, out, cap);
}

/* The engine's hook for a building's inside (world_set_people_hook): each quest person put in it stands on a quest marker (199.11) of the building, else beside its first person (else its first flat),
 * as their faction's flat for their sex (else the region's people's), the flat marked with the quest and person so a use of them is theirs (talk_used). */
static int quest_people_hook(const Dungeon *town, int block, int record, Flat *flats, int nflat, Flat *out, int cap)
{
    OgQuestHere here[8];
    const Flat *at;
    int n, i, made;
    float mx, my, mz;

    n = og_quest_people_in(town->region, town->index, block, record, here, cap < 8 ? cap : 8);
    if (!n || !nflat) { return 0; }
    at = &flats[0];
    for (i = 0; i < nflat; i++) { if (flats[i].tag & FLAT_PERSON) { at = &flats[i]; break; } }
    made = 0;
    for (i = 0; i < n; i++) {
        const Faction *f;
        df_u16 file, image, tag;

        f = factionfile_open() && here[i].faction ? faction_find(here[i].faction) : 0;
        if (!f || !f->flat_archive[here[i].female]) { f = faction_people_of(town->region); }
        file = (df_u16)(f ? f->flat_archive[here[i].female] : 182);
        image = (df_u16)(f ? f->flat_record[here[i].female] : 0);
        tag = (df_u16)(FLAT_PERSON | OG_QUEST_PERSON_TAG(here[i].slot, here[i].person));
        if (here[i].takeover) {
            /* The NPC site (8.5 of quest-places-and-start.md): one of the building's own people becomes the quest person, where it stands: its picture is the quest person's, its faction kept. The original picks
             * from the MAPS site list (partial, and its NPC numbers need a replay of the town's objects, `dungeon_sites` reads it); we pick by the quest's seed among the people of the building not already
             * taken. */
            int k, count, pick;

            count = 0;
            for (k = 0; k < nflat; k++) { if ((flats[k].tag & FLAT_PERSON) && (flats[k].tag & 0x7fff) < OG_QUEST_PERSON_MARK) { count++; } }
            if (count) {
                pick = (int)(here[i].seed % (unsigned)count);
                for (k = 0; k < nflat; k++) {
                    if (!((flats[k].tag & FLAT_PERSON) && (flats[k].tag & 0x7fff) < OG_QUEST_PERSON_MARK) || pick-- != 0) { continue; }
                    flats[k].file = file; flats[k].image = image; flats[k].tag = tag;
                    break;
                }
                continue;
            }
        }
        mem_zero(&out[made], sizeof(out[made]));
        if (world_building_marker_nth(11, made, &mx, &my, &mz)) { out[made].x = mx; out[made].y = my; out[made].z = mz; }
        else { out[made].x = at->x + 48.0f * (float)(made + 1); out[made].y = at->y; out[made].z = at->z; }
        out[made].file = file; out[made].image = image; out[made].tag = tag;
        made++;
    }
    return made;
}

static int talk_used(const Usable *u, int mode, float dist, const Camera *cam)
{
    const OgFactionDef *f;
    OgNpcName npc;
    int type, faction, quality, flat_archive, flat_record, female, merchant, kind;
    unsigned place;
    int what, qwho, qfaction, qfemale, quest;
    char qname[40];

    /* A quest's person (their flat is marked with the quest and the person): their own faction, id and name. */
    quest = u->what >= OG_QUEST_PERSON_MARK && og_quest_person_of_tag(u->what, &qwho, &qfaction, &qfemale, qname, (int)sizeof(qname));
    what = quest ? (qfaction ? qfaction : 510) : u->what;
    flat_archive = u->tag >> 7;
    flat_record = u->tag & 127;
    female = faces_female(flat_archive, flat_record);
    mem_zero(&g_met, sizeof(g_met));
    g_met.faction = what;
    g_met.archive = flat_archive;
    g_met.record = flat_record;
    g_met.female = female < 0 ? 0 : female;
    g_met.who = quest ? (unsigned)qwho : (unsigned)u->tag * 31u + (unsigned)what;
    g_met.cam = *cam;
    mem_zero(&npc, sizeof(npc));
    npc.type = 8;
    f = og_faction_def(og_faction_index(what));
    npc.faction = what;
    npc.faction_is_individual = f && f->type == 4;
    npc.faction_name = f ? f->name : 0;
    npc.region = travel_region();
    npc.id = g_met.who;
    npc.female = g_met.female;
    og_npc_name(&npc, g_met.name, (int)sizeof(g_met.name));
    if (quest && qname[0]) { df_copy(g_met.name, qname, (int)sizeof(g_met.name)); }
    if (mode == OG_MODE_INFO) {
        interact_say(g_met.name);
        return 1;
    }
    if (dist > 256.0f) {
        return 1;
    }
    kind = 0;
    merchant = interior_shopkeeper(cam, what, u->tag);
    if (travel_building(&type, &faction, &quality, &place)) {
        /* npc_click_service: the Thieves' and the Brotherhood's people serve their members, a stranger of the Brotherhood only talks, halls and temples have their menu. */
        int parent;

        parent = f ? f->parent : 0;
        if ((what == 42 || parent == 42) && hall_member(3)) { hall_enter(what, faction); return 1; }
        if ((what == 108 || parent == 108) && hall_member(0)) { hall_enter(what, faction); return 1; }
        if (what == 108 || parent == 108) { start_talk(); return 1; }
        if (type == 11 || type == 14) { hall_enter(what, faction); return 1; }
        if (type == 15 && merchant) { kind = OG_SERVICE_TAVERN; }
        else if (type == 3 && merchant) { kind = OG_SERVICE_BANK; }
        else if ((type == 2 || type == 9) && (what & 8)) { kind = OG_SERVICE_REPAIR; }
        else if (type == 13 && merchant) { kind = OG_SERVICE_REPAIR; }
        else if ((type == 0 || type == 5 || type == 6 || type == 8 || type == 12) && merchant) { kind = OG_SERVICE_SELL; }
    }
    if (kind && og_possessions_service(kind)) {
        return 1;
    }
    /* The quest events of the click come first (28, 71, 1, 78), then a marked person's offer, then talk. */
    if (og_quest_talk((int)g_met.who, g_met.faction)) { return 1; }
    if (!kind && travel_building(&type, &faction, &quality, &place) && street_quest(f, type, place)) { return 1; }
    g_talk_events_done = 1;
    start_talk();
    return 1;
}

static df_u32 g_counter;

/* The shop this building is, for the prices (what a shelf's too). */
static int shop_here(OgShop *shop)
{
    int type, faction, quality, region;
    unsigned place;

    if (!travel_building(&type, &faction, &quality, &place)) {
        return 0;
    }
    region = travel_region();
    shop->region_adjust = og_region_price(region);
    shop->quality = quality;
    shop->type = type;
    shop->holiday = og_holiday_today(og_state()->minutes, region);
    shop->half = 0;
    shop->scale = 256;
    return 1;
}

static int hall_member(int kind)
{
    return og_guild_find_kind(kind) != 0;
}

/* guild_find_membership_by_bits for the standing rules: the faction of the temple or order the player belongs to, 0 for none. */
static int standing_membership(int bits)
{
    OgMembership *m;

    m = og_guild_find_bits(bits);
    return m ? m->faction : 0;
}

/* What each button of a panel does (repair_menu_talk, repair_menu_sell, shop_open_repair, bank_open, service_menu_sell). */
static void hall_act(int what);
static void inn_act(int what);

static void service_act(int what)
{
    OgShop shop;

    if (og_possessions()->service == OG_SERVICE_TAVERN) {
        inn_act(what);
        return;
    }

    if (og_possessions()->service == OG_SERVICE_GUILD) {
        hall_act(what);
        return;
    }

    if (what == OG_SERVICE_TALK) {
        start_talk();
        return;
    }
    if (what == OG_SERVICE_DO && og_possessions()->service == OG_SERVICE_BANK) {
        og_possessions_bank(travel_region());
        return;
    }
    if (!shop_here(&shop)) {
        return;
    }
    if (!og_object(g_counter)) {
        g_counter = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    }
    if (!g_counter) {
        interact_say("There is no room for another counter.");
        return;
    }
    og_possessions_trade(what == OG_SERVICE_REPAIR_ITEM ? OG_MARKET_REPAIR : OG_MARKET_SELL, &shop, g_counter);
}

/*
 * Guild halls and temples (people and law, slice 9): guild_service_dispatch (src/hand/run_0006DC35.c 191..949). The hall's person offers the
 * menu (GILD0xI0.IMG: Join, Talk, their service, Exit); the service by the guild, the person and the player's rank is guild.c's gating. The
 * services built are a donation (a temple), the Fighters' repairs and the Thieves' fence; the rest answer that they are not built yet.
 * A promotion's message is shown in place of the menu (the original shows it, then the menu: one page at a time here).
 */
static struct { int kind, faction, person; } g_hall;

static void hall_text(int text)
{
    static char line[600];

    person_say_text(text, line, (int)sizeof(line));
    og_possessions_message(line, 0);
}

/* The yes of the offer to join: the membership, and the first rank's words. */
static void hall_join_answer(int yes)
{
    OgGuildStep step;

    if (yes && og_guild_join(g_hall.kind, g_hall.faction, og_state()->minutes, &step)) {
        hall_text(step.text);
    }
}

/* guild_donate's number: money spent, a chance of the hall's reputation going up by one (amount * 2 against the standing). */
static void hall_donate_answer(int amount)
{
    int rep;

    if (amount < 1) {
        return;
    }
    if (!og_gold_can_afford(amount)) {
        hall_text(702);
        return;
    }
    og_gold_spend(amount);
    rep = og_faction_rep(g_hall.faction);
    if (og_random_range(1, 100) <= amount * 2 / ((rep < 0 ? -rep : rep) + 1)) {
        og_faction_add_reputation(g_hall.faction, 1);
    }
    hall_text(703);
}

/*
 * A tavern's innkeeper (tavern_open, src/lifted/tavern.c; TVRN00I0.IMG: Room, Talk, Food and drinks, Goodbye). A room asks for the days (record 5102, or 5100 with
 * the hours left when one is held), refuses over 350 days kept (16), is free to a local knight and on Heart's Day, else is priced at 7 gold a day through the buyer's
 * skills and offered; a yes rents it, or adds the days. Food is a list of eleven, refused within 240 minutes of the last meal. The bed marker's number the room
 * would send the sleeper to is not read (room 0).
 */
/*
 * The beds of a tavern's inside: the editor markers of record 4 ("kind 2", `(image & 31) - 2`) in file order. A rented room is the number of one of them, drawn at random when the
 * room is rented (tavern_rent); sleeping there puts the player on that bed (tavern_go_to_room). A knight's hall uses the building's room number, 0 when none was ever rented.
 */
static int bed_count(void)
{
    float x, y, z;
    int n;

    for (n = 0; world_marker_nth(g_world, 4, n, &x, &y, &z); n++) {
    }
    return n;
}

static void go_to_bed(df_u32 building, int tavern)
{
    float x, y, z;
    int room, k;

    room = 0;
    if (tavern) {
        for (k = 0; k < OG_ROOMS_MOST; k++) {
            if (og_lodging()->room[k].building == building) { room = og_lodging()->room[k].room; }
        }
    }
    if (world_marker_nth(g_world, 4, room, &x, &y, &z)) {
        og_move_place(x, y + 64.0f, z, og_state()->yaw);
    }
}

static unsigned g_inn_building;
static OgRoomOffer g_inn_offer;

static void inn_room_paid(int yes)
{
    int text, n;

    if (!yes) { return; }
    /* tavern_rent: a bed drawn at random from the building's (rand() % how many). */
    n = bed_count();
    if (!og_room_take(g_inn_building, &g_inn_offer, n > 0 ? og_random_range(0, 32767) % n : 0, og_state()->minutes, &text)) {
        if (text) { hall_text(text); }
    }
}

static void inn_days(int days)
{
    OgShop shop;
    int r, knight;
    static char line[400];

    if (!shop_here(&shop)) { return; }
    knight = og_guild_is_local_knight(travel_region());
    r = og_room_offer(g_inn_building, days, &shop, og_character()->skill[14], og_character()->attribute[5], knight, og_state()->minutes, &g_inn_offer);
    if (r == OG_ROOM_NOTHING) { return; }
    if (r == OG_ROOM_REFUSED) { hall_text(g_inn_offer.text); return; }
    if (r == OG_ROOM_FREE) {
        int text;

        og_possessions_message(g_inn_offer.line, 0);
        og_room_take(g_inn_building, &g_inn_offer, bed_count() > 0 ? og_random_range(0, 32767) % bed_count() : 0, og_state()->minutes, &text);
        return;
    }
    offer_say(og_trade_offer_text(1, days * OG_ROOM_DAY_PRICE, g_inn_offer.price), g_inn_offer.price, line, (int)sizeof(line));
    og_possessions_ask(line, inn_room_paid);
}

static void inn_food_picked(int row)
{
    const char *line;
    int text;

    if (row < 0) { return; }
    if (!og_meal(row, og_holiday_today(og_state()->minutes, travel_region()), og_state()->minutes, &line, &text)) {
        if (line) { og_possessions_message(line, 0); }
        else if (text) { hall_text(text); }
    }
}

static void inn_act(int what)
{
    static char words[300];
    static char names[OG_LIST_MOST][OG_LIST_NAME];
    const char *line;
    int text, i, hours;
    int type, faction, quality;
    unsigned place;

    if (what == OG_SERVICE_TALK) { start_talk(); return; }
    if (!travel_building(&type, &faction, &quality, &place)) { return; }
    g_inn_building = place;
    if (what == OG_SERVICE_ROOM) {
        person_say_text(og_room_rented(place, og_state()->minutes) ? 5100 : 5102, words, (int)sizeof(words));
        /* %dwr: the hours left (macro_dwr_room_hours_left). */
        for (i = 0; words[i]; i++) {
            if (words[i] == '%' && words[i+1] == 'd' && words[i+2] == 'w' && words[i+3] == 'r') {
                char tail[300];
                int k;

                hours = 0;
                for (k = 0; k < OG_ROOMS_MOST; k++) {
                    if (og_lodging()->room[k].building == place && og_lodging()->room[k].expires > og_state()->minutes) {
                        hours = (int)((og_lodging()->room[k].expires - og_state()->minutes) / 60u);
                    }
                }
                df_copy(tail, words + i + 4, (int)sizeof(tail));
                log_format(words + i, (int)sizeof(words) - i, "%d", hours);
                for (k = 0; words[i + k]; k++) { }
                df_copy(words + i + k, tail, (int)sizeof(words) - i - k);
                break;
            }
        }
        og_possessions_number(words, inn_days);
    } else if (what == OG_SERVICE_FOOD) {
        if (og_state()->minutes - og_lodging()->last_meal < OG_MEAL_GAP) {
            og_meal(0, 0, og_state()->minutes, &line, &text);
            if (line) { og_possessions_message(line, 0); }
            return;
        }
        for (i = 0; i < OG_FOODS && i < OG_LIST_MOST; i++) { df_copy(names[i], og_food_name(i), OG_LIST_NAME); }
        og_possessions_list((const char (*)[OG_LIST_NAME])names, OG_FOODS, inn_food_picked);
    }
}

/* guild_confirm_price: the price at the hall's shop (base, then the buyer's skills, then the scale), the offer's words, and a yes. */
static int g_confirm_price;
static void (*g_confirm_cb)(int price);

static void hall_confirm_answer(int yes)
{
    void (*cb)(int);
    int price;

    cb = g_confirm_cb; price = g_confirm_price; g_confirm_cb = 0;
    if (yes && cb) { cb(price); }
}

static void hall_confirm(int base, void (*cb)(int price))
{
    OgShop shop;
    OgQuote q;
    static char line[400];
    int total;

    if (!shop_here(&shop)) { return; }
    total = og_trade_base_price(&shop, base);
    q.total = total;
    q.price = og_trade_adjust(&shop, og_character()->skill[14], og_character()->attribute[5], total, 0) * shop.scale / 256;
    g_confirm_price = q.price;
    g_confirm_cb = cb;
    offer_say(og_trade_offer_text(1, q.total, q.price), q.price, line, (int)sizeof(line));
    if (q.price < 1) { cb(q.price); return; }
    og_possessions_ask(line, hall_confirm_answer);
}

/* A price paid: 454 when the gold is not there (a message over the world), else spent. */
static int hall_pay(int price)
{
    if (!og_gold_can_afford(price)) {
        hall_text(454);
        return 0;
    }
    og_gold_spend(price);
    return 1;
}

/* Training: the lesson's price asked (record 8), then a list of the trainer's skills, then the lesson. */
static struct { int price; const unsigned char *skills; } g_lesson;

static void hall_lesson_picked(int row)
{
    int text, minutes;

    if (row < 0 || row >= OG_LIST_MOST) { return; }
    if (og_training_learn(g_lesson.skills[row], g_lesson.price, og_state()->minutes, &text, &minutes)) {
        hall_text(text);
        og_state_pass_minutes((df_u32)minutes);
    } else if (text) {
        hall_text(text);
    }
}

static void hall_lesson_answer(int yes)
{
    static char names[OG_LIST_MOST][OG_LIST_NAME];
    int n;

    if (!yes) { return; }
    if (!og_gold_can_afford(g_lesson.price)) { hall_text(454); return; }
    for (n = 0; g_lesson.skills[n] != 255 && n < OG_LIST_MOST; n++) {
        df_copy(names[n], og_skill_name(g_lesson.skills[n]), OG_LIST_NAME);
    }
    og_possessions_list((const char (*)[OG_LIST_NAME])names, n, hall_lesson_picked);
}

static void hall_training(int person, int member)
{
    static char line[400];
    int text;

    g_lesson.skills = og_training_skills(person);
    if (!g_lesson.skills) { start_talk(); return; }
    if (!og_training_offer(og_state()->minutes, og_character()->level, member, &g_lesson.price, &text)) {
        hall_text(text);
        return;
    }
    offer_say(8, g_lesson.price, line, (int)sizeof(line));
    og_possessions_ask(line, hall_lesson_answer);
}

/* guild_cure_diseases: 250 gold a disease (a temple of Akatosh's members pay less by rank), at the hall's prices, and the diseases cured. */
static void hall_cure_paid(int price)
{
    if (hall_pay(price)) {
        og_ailments_cure(-1, 0);
    }
}

static void hall_cure(int kind, int rank)
{
    int count, price;

    count = og_ailments_disease_count(-1);
    if (!count) { hall_text(30); return; }
    price = count * 250;
    if (kind == 142 && rank >= 0) { price = price * og_guild_price_scale(rank) / 256; }
    hall_confirm(price, hall_cure_paid);
}

/* guild_buy_blessing: one at a time, 100 gold at the hall's prices, its words by the temple. */
static int g_blessing_kind, g_blessing_rank;

static void hall_blessing_paid(int price)
{
    int text;

    if (!hall_pay(price)) { return; }
    if (og_blessing_buy(g_blessing_kind, g_blessing_rank, travel_region(), og_state()->minutes, &text) && text) {
        hall_text(text);
    }
}

static void hall_blessing(int kind, int rank)
{
    if (kind == 142) { return; }
    if (og_guilds()->blessing.active) { hall_text(454); return; }
    g_blessing_kind = kind;
    g_blessing_rank = rank < 0 ? 0 : rank;
    hall_confirm(100, hall_blessing_paid);
}

/* guild_heal: health to the most; if attributes are down, an offer (403) to put them back, the conditions and the ailments gone. */
/* spellshop_buy: four times the cost (half on the holiday 43) through the hall's chain, 454 if short, then the spell is in the book. */
static int g_buying_spell;

static void hall_spell_paid(int price)
{
    if (hall_pay(price)) {
        og_spellshop_add(g_buying_spell);
    }
}

static void hall_spell_buy(int id)
{
    const OgSpellDef *spell;

    spell = og_spell_def(id);
    if (!spell) { return; }
    g_buying_spell = id;
    hall_confirm(og_spellshop_price(spell, og_holiday_today(og_state()->minutes, travel_region())), hall_spell_paid);
}

/* daedra_summon: the day's prince, the price asked (481), the gold spent and the roll; a first success is the prince's quest (not built), a later one an ally (not built either). */
static int g_daedra_prince, g_daedra_price;

static void hall_daedra_answer(int yes)
{
    int kind, result;

    if (!yes) { return; }
    if (!og_gold_can_afford(g_daedra_price)) { hall_text(454); return; }
    og_gold_spend(g_daedra_price);
    result = og_daedra_summon(g_daedra_prince, 0, &kind);
    if (result == OG_DAEDRA_FAILED) { hall_text(484); }
    else if (result == OG_DAEDRA_QUEST) { og_possessions_message("The Daedra Prince has heard you. His quest is not yet built.", 0); }
    else { og_possessions_message("A Daedra answers your call. Summoned creatures are not yet built.", 0); }
}

static void hall_daedra(void)
{
    OgDaedraStep step;
    static char line[400];

    if (!og_daedra_offer(g_hall.faction, og_faction_rep(g_hall.faction), og_state()->minutes, &step)) {
        if (step.text) { hall_text(step.text); } else if (step.line) { og_possessions_message(step.line, 0); }
        return;
    }
    g_daedra_prince = step.prince;
    g_daedra_price = step.price;
    offer_say(step.text, step.price, line, (int)sizeof(line));
    og_possessions_ask(line, hall_daedra_answer);
}

static void hall_run(void);

static void hall_heal_answer(int yes)
{
    int i;

    if (yes) {
        for (i = 0; i < 8; i++) { og_character()->attribute[i] = og_state()->base_attribute[i]; }
        og_character()->conditions = 0;
        og_ailments_cure(-1, 0);
        og_ailments_cure(-1, 1);
    }
    /* The service the heal came with goes on once the question is answered. */
    hall_run();
}

/* 1 when a question is up (the service waits for its answer). */
static int hall_heal(void)
{
    static char line[400];
    int i, lowered;

    og_character()->health = og_character()->max_health;
    lowered = 0;
    for (i = 0; i < 8; i++) {
        if (og_character()->attribute[i] < og_state()->base_attribute[i]) { lowered++; }
    }
    if (!lowered) { hall_text(350); return 0; }
    person_say_text(403, line, (int)sizeof(line));
    return og_possessions_ask(line, hall_heal_answer);
}

/* guild_service_dispatch's Get quest (run_0006DC35.c): the quest letter of the NPC's guild, 'B' for a member and 'C' for anyone else, and the rank (the guild's for the Brotherhood, the
 * Fighters and the Thieves, the player's level for the Mages and the orders; a non-member has none: 7.3 fixed). A quest picked is started and offered; none to offer leaves the NPC talking. */
static void hall_quest(const OgMembership *m)
{
    char name[9], letter;
    int rank;

    letter = 0; rank = m ? m->rank : 0;
    if (g_hall.kind == 0) { letter = 'L'; }
    else if (g_hall.kind == 1) { letter = 'N'; rank = og_character()->level; }
    else if (g_hall.kind == 2) { letter = 'M'; }
    else if (g_hall.kind == 3) { letter = 'O'; }
    else if (g_hall.kind >= OG_KIND_KNIGHT + 4 && g_hall.kind <= OG_KIND_KNIGHT + 13) { letter = 'B'; rank = og_character()->level; }
    else if (g_hall.kind >= OG_KIND_TEMPLE + 14 && g_hall.kind <= OG_KIND_TEMPLE + 21) {
        /* guild_temple_quest: the temple's letter (the temples by name: Akatosh D, Arkay E, Dibella F, Julianos 0, Kynareth G, Mara H, Stendarr I, Zen J) or any C quest, a member's by rank. */
        letter = "DEF0GHIJ"[g_hall.kind - (OG_KIND_TEMPLE + 14)];
        if (!m) { rank = og_character()->level; }
        if (og_quest_pick(letter, 'C', m ? 'B' : 'C', rank, name) && !og_quest_person_busy((int)g_met.who)) {
            og_quest_set_questor((int)g_met.who, g_hall.person, g_met.name, town_name());
            if (og_quest_begin(name, g_hall.person) >= 0) { return; }
        }
        start_talk();
        return;
    }
    /* A person already giving a quest is not asked for another: they talk (their quest's events run there). */
    if (letter && !og_quest_person_busy(g_met.who)) {
        og_quest_set_questor((int)g_met.who, g_hall.person, g_met.name, town_name());
        if (og_quest_pick(letter, 0, m ? 'B' : 'C', rank, name) && og_quest_begin(name, g_hall.person) >= 0) { return; }
    }
    start_talk();
}

static void hall_run(void)
{
    OgMembership *m;
    OgShop shop;
    int service, heal, rank;

    m = og_guild_find_faction(g_hall.faction);
    rank = m ? m->rank : -1;
    service = og_guild_service(g_hall.kind, g_hall.person, rank, &heal);
    switch (service) {
    case OG_SVC_TALK:
        start_talk();
        break;
    case OG_SVC_MEMBERS_ONLY:
        og_possessions_message("My services are reserved for members only.", 0);
        break;
    case OG_SVC_RANK_REFUSED:
        hall_text(OG_TEXT_RANK_REFUSED);
        break;
    case OG_SVC_TRAINING:
        hall_training(g_hall.person, m != 0);
        break;
    case OG_SVC_MAGIC_ITEMS:
    case OG_SVC_POTIONS:
    case OG_SVC_SOULGEMS:
        /* guild_buy_potions: the container emptied, stocked with the hall's quality + 1 bottles, and opened for buying. */
        {
            int type, faction, quality, i;
            unsigned place;

            if (!shop_here(&shop) || !travel_building(&type, &faction, &quality, &place)) { break; }
            if (!og_object(g_counter)) {
                g_counter = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
            }
            if (!g_counter) { break; }
            for (i = 0; i < OG_OBJECTS; i++) {
                const OgObject *o;

                o = &og_state()->object[i];
                if (o->id && o->type == OG_RECORD_ITEM && o->parent == g_counter) { og_object_delete(o->id); }
            }
            if (service == OG_SVC_SOULGEMS) { og_soul_trap_stock(g_counter, quality); }
            else if (service == OG_SVC_MAGIC_ITEMS) {
                /* guild_buy_magic_items: the stock drawn on the stream started at the building's number (the same each visit), put back after; members of rank 4 and over also find soul traps; the festival 38 halves the prices. */
                int saved;

                saved = og_random_range(0, 32767);
                og_random_seed((df_u32)place);
                og_magic_shop_stock(g_counter, quality, m && m->rank >= 4);
                og_random_seed((df_u32)saved);
                if (og_holiday_today(og_state()->minutes, travel_region()) == 38) { shop.scale = 128; }
            }
            else { og_potion_stock(g_counter, quality); }
            og_possessions_trade(OG_MARKET_BUY, &shop, g_counter);
        }
        break;
    case OG_SVC_POTION_MAKER:
        og_possessions_potion();
        break;
    case OG_SVC_SPELL_MAKER:
        og_possessions_spellmaker();
        break;
    case OG_SVC_ITEM_MAKER:
        og_possessions_itemmaker();
        break;
    case OG_SVC_SPELLS:
        {
            int ids[OG_SPELLSHOP_MOST], count, type, faction, quality;
            unsigned place;

            if (!travel_building(&type, &faction, &quality, &place)) { break; }
            count = og_spellshop_list(g_met.who, quality, ids, OG_SPELLSHOP_MOST);
            if (count) { og_possessions_spellshop(ids, count, hall_spell_buy); }
        }
        break;
    case OG_SVC_DAEDRA:
        hall_daedra();
        break;
    case OG_SVC_TELEPORT:
        /* guild_teleport: the world map, from wherever the player stands. */
        og_maps_open(100);
        break;
    case OG_SVC_ARMOR:
        {
            int text;

            og_knight_armor(m, &text);
            if (text) { hall_text(text); }
        }
        break;
    case OG_SVC_HOUSE:
        {
            int text;

            if (og_knight_house_allowed(m, og_bank()->house != 0, &text)) {
                og_possessions_message("The houses of the town are not yet in the order's gift.", 0);
            } else if (text) {
                hall_text(text);
            }
        }
        break;
    case OG_SVC_CURE:
        hall_cure(g_hall.kind, rank);
        break;
    case OG_SVC_BLESSING:
        hall_blessing(g_hall.kind, rank);
        break;
    case OG_SVC_QUEST:
        hall_quest(m);
        break;
    case OG_SVC_NONE:
        break;
    case OG_SVC_DONATE:
        {
            /* guild_donate: msgbox_prompt_number(1000, ...): 1000 is the number first typed in; the words are FALL.EXE's own line. */
            og_possessions_number_from("Donate how much money", 1000, hall_donate_answer);
        }
        break;
    case OG_SVC_REPAIRS:
    case OG_SVC_FENCE:
        if (shop_here(&shop)) {
            /* The Fighters mend at a member's price ((10 - rank) / 10); the Thieves buy at half. */
            shop.scale = service == OG_SVC_REPAIRS ? og_guild_price_scale(rank) : 128;
            if (!og_object(g_counter)) {
                g_counter = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
            }
            if (g_counter) {
                og_possessions_trade(service == OG_SVC_REPAIRS ? OG_MARKET_REPAIR : OG_MARKET_SELL, &shop, g_counter);
            }
        }
        break;
    default:
        og_possessions_message("That service is not built yet.", 0);
        break;
    }
}

/* guild_service_dispatch's service: a member is healed first (the heal's own question, if any, is answered before the service goes on). */
static void hall_service(void)
{
    OgMembership *m;
    int service, heal;

    m = og_guild_find_faction(g_hall.faction);
    service = og_guild_service(g_hall.kind, g_hall.person, m ? m->rank : -1, &heal);
    (void)service;
    if (heal && hall_heal()) {
        return;
    }
    hall_run();
}

static void hall_act(int what)
{
    OgGuildStep step;
    static char line[600];

    if (what == OG_SERVICE_TALK) {
        start_talk();
    } else if (what == OG_SERVICE_JOIN) {
        if (og_guild_offer(g_hall.kind, g_hall.faction, og_faction_rep(g_hall.faction), &step)) {
            person_say_text(step.text, line, (int)sizeof(line));
            if (step.ask) {
                og_possessions_ask(line, hall_join_answer);
            } else {
                og_possessions_message(line, 0);
            }
        }
    } else if (what == OG_SERVICE_DO) {
        hall_service();
    }
}

/* npc_click_service's call into the hall: promotion first, then the menu if the person has a service, else they talk. */
static void hall_enter(int person_faction, int hall_faction)
{
    OgGuildStep step;
    OgMembership *m;
    const char *label;

    g_hall.faction = hall_faction;
    g_hall.kind = og_guild_kind_of_faction(hall_faction);
    g_hall.person = person_faction;
    if (g_hall.kind < 0) {
        start_talk();
        return;
    }
    if (og_guild_promote(g_hall.kind, hall_faction, og_faction_rep(hall_faction), og_state()->minutes, &step) && step.text) {
        hall_text(step.text);
        return;
    }
    label = og_guild_service_label(person_faction);
    if (!label) {
        start_talk();
        return;
    }
    m = og_guild_find_faction(hall_faction);
    og_possessions_guild(m != 0, label);
}


/* click.c 960..985, a house's chest or cupboard (a 418 prop, or a 410 furniture variant that furniture_is_container says holds things): stocked the first time by shop_generate_stock, empty ones do nothing; the player's own house opens freely; anyone else's asks first (TEXT.RSC 37), and a grab may be seen: a pickpocket roll against 108, then theft (crime 13) and the guards. */
static df_u32 g_container_pending;

static void container_answer(int yes)
{
    if (!yes || !g_container_pending) { return; }
    if (og_random_range(0, 255) >= 108 - og_character()->skill[15]) {
        og_law_note(OG_CRIME_THEFT);
        og_guards_summon(1);
    }
    if (!og_possessions_open(OG_VIEW_INVENTORY, g_container_pending)) { interact_say("You cannot take anything now."); }
}

static int container_used(int index)
{
    const Usable *u;
    OgObject *box;
    df_u32 id;
    int type, faction, quality;
    unsigned place;

    u = &seam_level()->scene.usables[index];
    if (!travel_building(&type, &faction, &quality, &place)) {
        interact_say("There is nothing here you want.");
        return 1;
    }
    id = og_loot_container(og_object(OG_WORLD_ROOT)->place_key, (df_u32)u->tag, u->cx, u->cy, u->cz, -1);
    box = id ? og_object_edit(id) : 0;
    if (!box) { interact_say("There is no room for another container."); return 1; }
    if (!(box->flags & 2)) {
        box->flags |= 2;
        og_shelf_stock(id, (int)((df_u32)u->hd % 100u), quality, type, u->model);
    }
    if (!og_inventory_rows(id, -1, 0, 0)) {
        interact_say("There is nothing here you want.");
        return 1;
    }
    if ((df_u32)og_bank()->house == place) {
        if (!og_possessions_open(OG_VIEW_INVENTORY, id)) { interact_say("You cannot take anything now."); }
        return 1;
    }
    g_container_pending = id;
    if (!og_possessions_ask(og_words(37), container_answer)) { interact_say("You cannot take anything now."); }
    return 1;
}

static int shelf_used(int index, int variant)
{
    OgShop shop;
    int type, faction, quality, region;
    unsigned place;

    if (!travel_building(&type, &faction, &quality, &place)) {
        interact_say("There is nothing here you want.");
        return 1;
    }
    if (g_shelf_level != seam_level() || g_shelf_place != place) {
        forget_shelves();
        g_shelf_level = seam_level();
        g_shelf_place = place;
    }
    if (index < 0 || index >= SHELVES_MOST) {
        return 1;
    }
    if (!g_shelf_box[index]) {
        g_shelf_box[index] = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
        if (!g_shelf_box[index]) {
            interact_say("There is no room for another shelf.");
            return 1;
        }
        og_shelf_stock(g_shelf_box[index], variant, quality, type, index);
        /* shelf_open_stock: an alchemist's shelf, a quarter of the time, also holds a recipe for a potion; each shelf of a general store, when first opened, a horse and a cart (collected_count == 0). */
        if (type == 0 && og_random_range(1, 100) < 25) { og_item_create(g_shelf_box[index], 27, 4, 0); }
        if (type == 9) {
            og_item_create(g_shelf_box[index], 23, 1, -1);
            og_item_create(g_shelf_box[index], 23, 0, -1);
        }
    }
    region = travel_region();
    shop.region_adjust = og_region_price(region);
    shop.quality = quality;
    shop.type = type;
    shop.holiday = og_holiday_today(og_state()->minutes, region);
    shop.half = 0;
    shop.scale = 256;
    {
        /* shelf_open_stock: a shop that is open sells from its shelves; one that is shut lets them be opened as a plain container, the goods free to take. */
        OgHoursPlayer hours;
        OgMembership *m;

        m = og_guild_find_kind(0); hours.brotherhood = m ? m->rank : -1;
        m = og_guild_find_kind(3); hours.thieves = m ? m->rank : -1;
        m = og_guild_find_kind(2); hours.fighters = m ? m->rank : -1;
        m = og_guild_find_kind(1); hours.mages = m ? m->rank : -1;
        if (!og_building_open(type, faction, place, og_state()->minutes, 0, &hours)) {
            if (!og_possessions_open(OG_VIEW_INVENTORY, g_shelf_box[index])) {
                interact_say("You cannot take anything now.");
            }
            return 1;
        }
    }
    if (!og_possessions_trade(OG_MARKET_BUY, &shop, g_shelf_box[index])) {
        interact_say("You cannot trade now.");
    }
    return 1;
}

/*
 * Where the player may sleep (rest_allowed): the dungeon, the wilds, a town's streets (where the watch
 * minds a sleeper) or inside a building. The outdoor location's kind (a keep, a ruin, a graveyard are no
 * town and let a sleeper be) is not carried by the dagger's location records, so every town is a town.
 */
static const Camera *g_site_camera;

static int rest_site(OgRestSite *site)
{
    TravelWhere here;
    int origin[2], region, index, block, record, type, faction, quality;
    unsigned place;
    float x, z;

    mem_zero(site, sizeof(*site));
    travel_here(&here, origin);
    if (here.kind == TRAVEL_DUNGEON) {
        site->kind = OG_SITE_DUNGEON;
    } else if (here.kind == TRAVEL_INSIDE && travel_building(&type, &faction, &quality, &place)) {
        site->kind = OG_SITE_INSIDE;
        site->building_type = type;
        site->building_faction = faction;
        site->building = place;
        site->own_house = og_bank()->house;
        site->knight_member = og_guild_is_local_knight(travel_region());
    } else if (g_site_camera && travel_town_here(g_site_camera, &region, &index, &x, &z, &block, &record)) {
        site->kind = OG_SITE_TOWN;
    } else {
        site->kind = OG_SITE_WILD;
    }
    return 1;
}

/*
 * The law's world (people and law, slice 5): the guards the dungeon AI knows (the watch is kind 146), no
 * place for new ones in a town yet (the streets' creature art is not prepared for them), the court's words
 * said by the dagger reader, and where a sentence leaves the player. A court in a building puts the player out
 * in the street; a street stays where it is (no marker to move to).
 */
static int guards_present(void) { return og_dungeon_guards(); }
/*
 * The street's people and the watch (people and law, the town's creatures): the dagger layer's people pool stands for the original's pedestrians. A guard is a person of archive 399; the watch
 * is those guards (and, when called, more made near the player) taken into OGFALL's own creatures (kind 146, the original's stats and mind) and woken knowing where the player is. A swing's hit
 * kills whoever is within 90 units and 200 of the view (the original's pedestrian pass), the victim itself a witness; a killing seen is murder (crime 5) and brings the guards. Neither
 * the pictures nor the walking of the people are OGFALL's.
 */
static int creature_rate(int archive);
#define STREET_GUARD_KIND 146
#define STREET_GUARD_ARCHIVE 399

/* collide_line_of_sight: a ray from the person's chest to the player's eye that no wall stops. */
static int street_sees(const Entity *e, const float *eye)
{
    UseHit hit;
    float dx, dy, dz, length;

    dx = eye[0] - e->x; dy = eye[1] - (e->y + 40.0f); dz = eye[2] - e->z;
    length = df_sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 1.0f) { return 1; }
    dx /= length; dy /= length; dz /= length;
    interact_cast(&g_level.scene, e->x, e->y + 40.0f, e->z, dx, dy, dz, length, &hit);
    return hit.wall >= length;
}

/* people_check_witnesses: bit 0 another person, bit 1 a guard, with the player in sight; nothing in a dungeon. */
static int street_witnesses(void)
{
    Camera cam;
    float eye[3];
    int i, bits, bit;

    if (og_world_state()->where.kind == OG_PLACE_DUNGEON) { return 0; }
    og_move_camera(&cam);
    eye[0] = cam.x; eye[1] = cam.y; eye[2] = cam.z;
    bits = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;
        float dx, dz;

        e = people_at(i);
        /* A guard of the street is a person too, and the one whose sight brings the watch (people_check_witnesses: bit 1). */
        if (!e || !(people_is_citizen(e) || people_is_guard(e))) { continue; }
        dx = e->x - cam.x; dz = e->z - cam.z;
        if (dx * dx + dz * dz > 2048.0f * 2048.0f) { continue; }
        /* Each look is a wall cast over the whole street: one for a bit not yet set, and none once both are. */
        bit = og_dungeon_index(e) >= 0 || people_is_guard(e) ? 2 : 1;
        if (bits & bit) { continue; }
        if (street_sees(e, eye)) { bits |= bit; if (bits == 3) { break; } }
    }
    return bits;
}

/* One of the pool's guards taken into the creatures and woken: a creature of the watch (kind 146) on the person's entity. */
static int street_enlist(int index)
{
    Entity *e;
    int foe;

    e = people_at(index);
    if (!e || !people_is_guard(e)) { return 0; }
    foe = og_dungeon_index(e);
    if (foe < 0) {
        people_hold(index);
        /* A guard of the watch in pursuit is not talked to (the street's talk refuses the hostile). */
        people_set_hostile(e, 1);
        foe = og_dungeon_add(e, STREET_GUARD_KIND, 0, creature_rate(STREET_GUARD_ARCHIVE));
        if (foe < 0) { return 0; }
        og_dungeon_loaded(foe);
    }
    og_dungeon_wake(foe);
    return 1;
}

/* guard_spawn(0): one guard near the player, out of sight where a cell allows it. */
static int street_spawn(void)
{
    Camera cam;
    int index;

    og_move_camera(&cam);
    index = people_spawn_guard(&cam);
    return index >= 0 && street_enlist(index);
}

/* guards_summon's people: every guard comes; unless guards_only, a quarter of those behind the player (more than 600 of 2048 round from where they face) turn out to be guards too. */
static int street_convert(int guards_only)
{
    Camera cam;
    int i, n;

    og_move_camera(&cam);
    n = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;

        e = people_at(i);
        /* people_is_citizen is false for a guard: the guards walking the street are the first the watch is made of. */
        if (!e || !(people_is_citizen(e) || people_is_guard(e))) { continue; }
        if (people_is_guard(e)) {
            n += street_enlist(i);
            continue;
        }
        if (guards_only) { continue; }
        {
            float dx, dz, diff;

            dx = e->x - cam.x; dz = e->z - cam.z;
            diff = df_absf(df_atan2(dx, dz) - cam.yaw);
            while (diff > DF_TAU) { diff -= DF_TAU; }
            if (diff > DF_TAU / 2) { diff = DF_TAU - diff; }
            if (diff < 600.0f * DF_TAU / 2048.0f) { continue; }
        }
        if (og_random_range(0, 32767) & 3) { continue; }
        /* The original makes a guard of the person where they stand; the pictures are not the person's, so a guard is made near and the person goes. */
        if (street_spawn()) {
            people_fell(e);
            n++;
        }
    }
    return n;
}

/* damage_spawn_splash on a person struck: a splash of blood in the people pool for a second, taken down by street_blood_tick. */
#define STREET_BLOODS 4
static Entity *g_street_blood[STREET_BLOODS];
static float g_street_blood_left[STREET_BLOODS];
static void street_blood(const Entity *e)
{
    int i;

    for (i = 0; i < STREET_BLOODS; i++) {
        if (g_street_blood_left[i] > 0.0f) { continue; }
        g_street_blood[i] = people_blood(e->x, e->y + 40.0f, e->z, 0);
        g_street_blood_left[i] = g_street_blood[i] ? 1.0f : 0.0f;
        return;
    }
}
static void street_blood_tick(float seconds)
{
    int i;

    for (i = 0; i < STREET_BLOODS; i++) {
        if (g_street_blood_left[i] <= 0.0f) { continue; }
        g_street_blood_left[i] -= seconds;
        if (g_street_blood_left[i] <= 0.0f && g_street_blood[i]) { g_street_blood[i]->live = 0; g_street_blood[i] = 0; }
    }
}

/* The pedestrian pass of a swing (func_000733AD.c 96..110). */
static int street_hit(const Camera *cam)
{
    int i, struck, bits;

    if (og_world_state()->where.kind == OG_PLACE_DUNGEON) { return 0; }
    struck = 0;
    for (i = 0; i < PEOPLE_MAX; i++) {
        Entity *e;
        float dx, dz, distance, dy;

        e = people_at(i);
        if (!e || !people_is_citizen(e) || og_dungeon_index(e) >= 0) { continue; }
        dx = e->x - cam->x; dz = e->z - cam->z; dy = og_move_body()->feet[1] - e->y;
        distance = df_sqrt(dx * dx + dz * dz + dy * dy);
        if (distance > 90.0f) { continue; }
        if (distance > 10.0f && df_cos(df_atan2(dx, dz) - cam->yaw) <= df_cos(200.0f * DF_TAU / 2048.0f)) { continue; }
        /* pedestrian_killed: the victim is still among the people, so they have seen it; murder, and the guards. */
        bits = street_witnesses();
        if (bits) {
            og_law_note(OG_CRIME_MURDER);
            og_guards_summon(1);
        }
        street_blood(e);
        og_sound_at(17,e->x,e->y,e->z);
        people_fell(e);
        og_guild_count_crime(6, 5, og_state()->minutes);
        struck++;
    }
    return struck;
}

/* A creature of the watch killed: the body is laid, the guard's corpse. */
static void street_died(int kind, Entity *e)
{
    if (kind == STREET_GUARD_KIND && people_index(e) >= 0) {
        people_lay_corpse(e);
        /* The body's things lie on the ground there, a pile the use key opens (the street has no corpse usables). */
        {
            df_u32 pile;

            /* The watch has no loot table (class creatures carry their gear): the pile is made empty and the guard's gear made into it. */
            pile = og_ground_pile(e->x,e->y,e->z,og_dungeon_loot_table(og_dungeon_index(e)));
            og_dungeon_gear_to(og_dungeon_index(e),pile);
        }
    }
}

/* court_remove_creatures: the watch is sent away. */
static void street_dismiss(void)
{
    int i;

    for (i = 0; i < og_dungeon_count(); i++) {
        Entity *e;

        if (og_dungeon_kind(i) != STREET_GUARD_KIND) { continue; }
        e = og_dungeon_entity(i);
        og_dungeon_remove(i);
        if (e && people_index(e) >= 0) { people_fell(e); }
    }
}

static int guards_count(void) { return og_dungeon_count(); }
static int guards_dungeon(void) { return og_world_state()->where.kind == OG_PLACE_DUNGEON; }

static int court_say(int text, const OgCourt *court, char *out, int cap)
{
    static Character pc;
    static char penalty[96], prison[48];
    Words w;
    int i;

    for (i = 0; i < (int)sizeof(pc.name) - 1 && og_character()->name[i]; i++) { pc.name[i] = og_character()->name[i]; }
    pc.name[i] = 0;
    mem_zero(&w, sizeof(w));
    w.pc = &pc;
    w.city = town_name();
    w.crime = og_crime_name(court->crime + 1);
    if (court->severity == 0) { df_copy(penalty, "Banishment", sizeof(penalty)); }
    else if (court->severity == 1) { df_copy(penalty, "Execution", sizeof(penalty)); }
    else { log_format(penalty, sizeof(penalty), "%d gold pieces in fines and %d days in prison", court->fine, court->days); }
    w.penalty = penalty;
    w.days = court->days;
    w.more_days = court->days;
    /* The prison a death sentence names (record 8100's variants: "The Cage of Slarpen", "The Hall of Yegon"). */
    df_copy(prison, "the prison", sizeof(prison));
    words_use(&w);
    words_say(8100, prison, sizeof(prison));
    w.prison = prison;
    words_use(&w);
    return words_say(text, out, cap);
}

static void court_release(int where)
{
    /* The street (8) and the walls' edge (4) and an acquittal's (12) are all out of the building the case was heard in. */
    (void)where;
    travel_out_to_street();
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
    og_magic_set_draw(cast_draw);
    og_magic_set_hud(magic_hud_draw);
    og_set_save_capture(seam_capture_world);
    og_set_words(textrsc_read);
    og_set_status_intro(status_intro);
    og_quest_set_text(quest_text); og_quest_set_say(quest_say); og_quest_set_ask(og_possessions_ask);
    og_quest_set_macros(quest_macros); og_quest_set_place(quest_place);
    {
        static const OgQuestWorld quest_world = { og_travel_count, og_travel_location, quest_here, quest_region_name, quest_journey, og_travel_discover, quest_spawn_foe, og_teleport, quest_place_item, quest_find_number, quest_marker_at, quest_spawn_foe_at, quest_place_item_at, quest_pick_building, quest_pick_site, quest_person_name, quest_outdoors };

        og_quest_set_world(&quest_world);
    }
    {
        static const OgEnchantWorld enchant_world = { ench_daylight, ench_temple, ench_category_near, ench_hurt_nearest };

        og_enchant_set_world(&enchant_world);
    }
    og_set_word_variants(textrsc_variant);
    if (!maindat_start()) {
        why(maindat_error());
        return 0;
    }
    dagger_bind_loaders();
    install_travel();
    og_activate_set_shelf(OTHER_SHELF, shelf_used);
    og_activate_set_container(OTHER_CONTAINER, container_used);
    og_activate_set_global(OTHER_GLOBAL, og_quest_global_set);
    og_possessions_set_letter(og_quest_item_text);
    og_activate_set_person(person_used);
    world_set_people_hook(quest_people_hook);
    og_activate_set_talk_use(talk_used);
    og_possessions_set_service(service_act);
    og_standing_set_membership(standing_membership);
    og_names_set_expand(talk_expand);
    og_rest_set_site(rest_site);
    og_rest_set_room(go_to_bed);
    og_dungeon_set_street(street_hit, street_died);
    {
        static const OgGuardWorld world = { guards_present, guards_count, street_spawn, street_witnesses, guards_dungeon, street_dismiss, street_convert };

        og_guards_set_world(&world);
    }
    og_possessions_set_court(court_say, court_release);
    og_possessions_set_offer_words(offer_say);
    install_maps();
    load_classes();
    load_class_questions();
    if (!load_factions()) {
        why("FACTION.TXT did not read");
        return 0;
    }
    {
        /* NAMEGEN.DAT's pieces name people and shops (people and law, slice 8); without them names are empty. */
        df_u32 names_size;
        const df_u8 *names_bytes;

        names_bytes = vfs_read("NAMEGEN.DAT", &names_size);
        og_names_set_data(names_bytes, names_bytes ? names_size : 0);
        names_bytes = vfs_read("MAGIC.DEF", &names_size);
        og_magic_defs_set(names_bytes, names_bytes ? names_size : 0);
    }
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
    if (!load_cast_art()) { why("Cannot read the spell casting pictures"); return 0; }
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
    hold = g_place_data;
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

static CifFrame g_cast_art[5][6];
static df_u8 *g_cast_pixels;
static int g_cast_pic, g_cast_element = -1, g_cast_frame = -1, g_cast_bar;

static int load_cast_art(void)
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
    if (palette) { *palette = g_art_rgb; }
    return g_cast_pixels;
}

static void cast_draw(int element, int frame)
{
    const CifFrame *f;
    int x, y, px, py, bar;
    df_u8 pixel;
    if (element < 0 || element >= 5 || frame < 0 || frame >= 6) { return; }
    if (!g_cast_pixels) { g_cast_pixels = (df_u8 *)mem_keep(64000); }
    if (!g_cast_pixels) { return; }
    bar = !(og_options_flags() & 1); f = &g_cast_art[element][frame];
    if (!g_cast_pic || element != g_cast_element || frame != g_cast_frame || bar != g_cast_bar) {
        mem_zero(g_cast_pixels,64000);
        for (y = 0; y < f->h; y++) {
            py = f->y+y-(bar ? 46 : 0); if (py < 0 || py >= 200) { continue; }
            for (x = 0; x < f->w; x++) {
                px = f->x+x; if (px < 0 || px >= 320) { continue; }
                pixel = f->pixels[y*f->w+x];
                g_cast_pixels[py*320+px] = pixel; g_cast_pixels[py*320+319-px] = pixel;
            }
        }
        if (g_weapon_pic) { pic_free(g_weapon_pic); g_weapon_pic = 0; g_weapon_frame = -1; }
        if (!g_cast_pic) { g_cast_pic = pic_load(g_cast_pixels,320,200,g_art_rgb,1); }
        else { pic_update(g_cast_pic,g_cast_pixels); }
        g_cast_element = element; g_cast_frame = frame; g_cast_bar = bar;
    }
    og_hud_blit(g_cast_pic,0,0,320,200);
}

static void weapon_draw(const OgWeaponView *view, float flash)
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

static int encounter_art(int kind, int archive, Entity *out, int *rate)
{
    int i;
    for (i = 0; i < g_encounter_art_count; i++) {
        if (g_encounter_art[i].kind == kind && g_encounter_art[i].archive == archive) {
            *out = g_encounter_art[i].entity; *rate = g_encounter_art[i].rate; return 1;
        }
    }
    return 0;
}

static int prepare_encounter_art(int kind, int archive, Image **images, df_u8 *cutout, int *slots)
{
    static const int first[ENTITY_POSES] = {0,15,5,10,20,25,0};
    EncounterArt *art;
    const Entity *existing;
    EntityPose *pose;
    Image *image;
    int i, p, view, file, record, slot;

    for (i = 0; i < g_encounter_art_count; i++) {
        if (g_encounter_art[i].kind == kind && g_encounter_art[i].archive == archive) { return 1; }
    }
    if (g_encounter_art_count == ENCOUNTER_ART) { return 0; }
    art = &g_encounter_art[g_encounter_art_count++];
    mem_zero(art,sizeof(*art)); art->kind = kind; art->archive = archive; art->rate = creature_rate(archive);
    entity_init(&art->entity);
    for (i = 0; i < world_entity_count(g_world); i++) {
        if (world_entity_kind(g_world,i,0) != kind || world_entity_archive(g_world,i) != archive) { continue; }
        existing = world_entity(g_world,i);
        for (p = 0; p < ENTITY_POSES; p++) { art->entity.pose[p] = existing->pose[p]; }
        break;
    }
    /* Reuse decoded poses without activating a distant placed creature.
     * Missing walk/idle art gets its own slots: the world's reserved slots
     * still belong to its lazy loader. Prepare everything before gfx_prepare. */
    for (p = 0; p < ENTITY_POSES; p++) {
        pose = &art->entity.pose[p]; pose->rate = p == ENTITY_IDLE ? 5 : 10;
        for (view = 0; view < (p == ENTITY_DEAD ? 1 : 5); view++) {
            if (pose->w[view]) { continue; }
            file = archive; record = first[p]+view;
            if (p == ENTITY_DEAD) { og_foe_corpse(kind,&file,&record); }
            if (p == ENTITY_IDLE && archive == 483 && view == 1) { record = 11; }
            image = texture_load(file,record);
            if (!image) { continue; }
            if (image->frames <= 1 || !texture_sheet(image,file,record)) { texture_pad(image); }
            slot = (*slots)++; images[slot] = image; cutout[slot] = 1;
            pose->slot[view] = (df_u16)slot;
            pose->w[view] = (df_u16)texture_extent(image->width,image->xscale);
            pose->h[view] = (df_u16)texture_extent(image->height,image->yscale);
        }
    }
    if (kind == 0) { art->entity.pose[ENTITY_IDLE].right = 14; }
    if (kind == 18 || kind == 23) { art->entity.pose[ENTITY_WALK].right = 4; }
    return art->entity.pose[ENTITY_WALK].w[0] > 0;
}

static int hold_encounter_art(Image **images, df_u8 *cutout, int *slots)
{
    OgEncounterState *state;
    const OgEncounter *saved;
    int i, kind, archive, variants;

    g_encounter_art_count = 0; state = og_encounter_state();
    for (i = 0; i < 6; i++) {
        kind = state->choices[i]; archive = kind < 43 ? 255+kind : kind == 146 ? 399 : 432+og_foe_texture_kind(kind);
        variants = kind >= 128 && kind < 146 ? 2 : 1;
        if (!prepare_encounter_art(kind,archive,images,cutout,slots)) { return 0; }
        if (variants == 2 && !prepare_encounter_art(kind,archive+1,images,cutout,slots)) { return 0; }
    }
    for (i = 0; i < OG_ENCOUNTERS; i++) {
        saved = &state->actor[i];
        if (saved->key.thing && saved->key.place == state->place &&
            !prepare_encounter_art(saved->kind,saved->archive,images,cutout,slots)) { return 0; }
    }
    g_original_entities = g_level.scene.nentities;
    g_game_entities = (Entity *)mem_alloc((g_original_entities+g_encounter_art_count+1)*sizeof(Entity));
    if (!g_game_entities) { return 0; }
    mem_copy(g_game_entities,g_level.scene.entities,g_original_entities*sizeof(Entity));
    /* Invisible reference entities let the generic renderer budget the art
     * for future spawns as creature resources, rather than shrinking walls. */
    for (i = 0; i < g_encounter_art_count; i++) { g_game_entities[g_original_entities+i] = g_encounter_art[i].entity; }
    g_level.scene.entities = g_game_entities;
    g_level.scene.nentities = g_original_entities+g_encounter_art_count;
    og_dungeon_rebind(g_game_entities,g_original_entities);
    mem_copy(images,g_world_images,g_world_image_count*sizeof(Image *));
    return 1;
}

/* Future missile pictures are budgeted as entity art at place opening. */
static int hold_magic_art(Image **images, df_u8 *cutout, int *slots)
{
    int element, pose, slot, ids[5][2];
    Image *image;
    Entity *reference;
    reference = &g_game_entities[g_level.scene.nentities++]; entity_init(reference);
    for (element = 0; element < 5; element++) {
        for (pose = 0; pose < 2; pose++) {
            image = texture_load(375+element,pose); if (!image) { return 0; }
            if (image->frames <= 1 || !texture_sheet(image,375+element,pose)) { texture_pad(image); }
            slot = (*slots)++; images[slot] = image; cutout[slot] = 1; ids[element][pose] = slot;
            reference->pose[pose].slot[element] = (df_u16)slot;
            reference->pose[pose].w[element] = reference->pose[pose].h[element] = 64;
        }
    }
    image = texture_load(210,3); if (!image) { return 0; }
    if (image->frames <= 1 || !texture_sheet(image,210,3)) { texture_pad(image); }
    slot = (*slots)++; images[slot] = image; cutout[slot] = 1;
    reference->pose[2].slot[0] = (df_u16)slot; reference->pose[2].w[0] = reference->pose[2].h[0] = 64;
    og_magic_light_sprite(slot,texture_extent(image->width,image->xscale),texture_extent(image->height,image->yscale));
    og_magic_slots(ids); return 1;
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

/* An emptied container stays in the state with no rows; a pile or corpse whose container is that is gone from the view and from the use key. */
static int container_emptied(df_u32 thing)
{
    const OgObject *o;
    int i;

    for (i = 0; i < OG_OBJECTS; i++) {
        o = &og_state()->object[i];
        if (o->id && o->type == OG_RECORD_CONTAINER && o->parent == OG_WORLD_ROOT &&
            o->place_key == og_object(OG_WORLD_ROOT)->place_key && o->thing_key == thing) {
            return !og_inventory_rows(o->id,-1,0,0);
        }
    }
    return 0;
}

static void hold_dropped_piles(void)
{
    const OgObject *container;
    Flat *flat;
    int i, n, uses, changed;
    if (!g_world || !g_hold_flats || g_hold_pile_serial == og_state()->event_serial) { return; }
    g_hold_pile_serial = og_state()->event_serial;
    n = g_hold_flat_base; uses = g_hold_use_base; changed = 0;
    for (i = 0; i < g_hold_use_base; i++) {
        Usable *u;

        u = &g_hold_usables[i];
        if (u->kind != USE_OTHER || u->what < OG_USE_TREASURE || u->what >= OG_USE_TREASURE+22 || u->sprite < 0 || u->sprite >= g_hold_flat_base) { continue; }
        if (g_hold_flats[u->sprite].w > 0 && container_emptied((df_u32)u->tag)) {
            g_hold_flats[u->sprite].w = g_hold_flats[u->sprite].h = 0;
            u->reach = 0; u->hw = u->hh = u->hd = 0;
            changed = 1;
        }
    }
    for (i = 0; i < og_dungeon_count(); i++) {
        const Entity *e;
        Flat corpse;
        e = og_dungeon_entity(i);
        if (!e->dead || !e->live || container_emptied(e->key.thing)) { continue; }
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
    hold = g_place_data;
    og_object_edit(OG_WORLD_ROOT)->place_key = travel_place_key();
    if (og_world_state()->where.kind == OG_PLACE_DUNGEON) {
        og_encounter_prepare(og_object(OG_WORLD_ROOT)->place_key,hold.dungeon_type);
    }
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
    if (!hold_encounter_art(images,cutout,&slot) || !hold_magic_art(images,cutout,&slot)) { return 0; }
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
    for (b = 0; og_world_state()->where.kind == OG_PLACE_DUNGEON && b < hold.nblocks && count < g_hold_capacity-OG_OBJECTS; b++) {
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

static int hold_actions(void)
{
    static OgAction actions[OG_ACTIONS];
    static int next[OG_ACTIONS];
    const ModelAct *raw;
    OgAction *out;
    int i, j, count, block;

    count = 0;
    for (i = 0; i < world_model_count(g_world); i++) {
        raw = world_model_action(g_world,i,&block);
        if (!raw || !raw->flags) { continue; }
        if (count == OG_ACTIONS) { why("Too many dungeon actions"); return 0; }
        out = &actions[count]; mem_zero(out,sizeof(*out));
        out->model = i; out->mover = out->next = -1;
        out->key.place = og_object(OG_WORLD_ROOT)->place_key;
        out->key.thing = ((df_u32)block << 24) | raw->pos;
        out->trigger = raw->trigger & 15; out->kind = raw->flags;
        out->axis = raw->axis; out->duration = raw->duration;
        out->magnitude = raw->magnitude; out->sound = raw->sound;
        next[count++] = raw->next;
        for (j = 0; j < g_level.scene.nmovers; j++) {
            if (g_level.scene.movers[j].model == i) { out->mover = j; out->key = g_level.scene.movers[j].key; break; }
        }
    }
    for (i = 0; i < count; i++) {
        if (next[i] < 0) { continue; }
        for (j = 0; j < count; j++) {
            if ((actions[i].key.thing >> 24) == (actions[j].key.thing >> 24) &&
                (actions[j].key.thing & 0xffffffu) == (df_u32)next[i]) { actions[i].next = j; break; }
        }
    }
    return og_actions_open(&g_level.scene,actions,count);
}

int seam_open_place(int restoring, int changing, float *x, float *y, float *z)
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
    if (changing && g_place_open) {
        Camera from;
        og_move_camera(&from); travel_came_from(&from);
        if (!seam_capture_world()) { why(og_state_error()); return 0; }
        mem_zero(og_magic_state()->missile,sizeof(og_magic_state()->missile));
    }
    seam_close_hold();
    if (!og_state_restore_world()) {
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
    if (restoring) {
        TravelWhere where;
        const OgWhere *saved;
        int origin[2];
        float feet[3];
        g_saved_where = og_world_state()->where; saved = &g_saved_where; mem_zero(&where,sizeof(where));
        where.kind = saved->kind; where.located = saved->located; where.region = saved->region; where.index = saved->index;
        where.block = saved->block; where.record = saved->record; where.has_back = saved->has_back;
        where.back_x = saved->back_x; where.back_z = saved->back_z; where.back_yaw = saved->back_yaw*(DF_TAU/2048);
        origin[0] = saved->origin_x; origin[1] = saved->origin_z;
        feet[0] = og_state()->x; feet[1] = og_state()->eye_y-WALK_EYE; feet[2] = og_state()->z;
        travel_from_save(&where,feet,origin,og_state()->yaw*(DF_TAU/2048),og_state()->pitch*(DF_TAU/2048));
    } else if (!changing) { travel_start_game(); }
    travel_set_dungeon_streaming(0);
    g_place_open = travel_open(&g_level,1);
    g_world = travel_whole_world();
    {
        int i;
        for (i = 0; i < g_entity_keep_count; i++) { keep_put(&g_entity_keeps[i]); }
    }
    if (!g_place_open) { why(travel_error()); return 0; }
    og_world_restore(); og_travel_capture();
    if (!g_world) {
        if (!seam_world_art()) { why("Cannot prepare world gameplay pictures"); return 0; }
        if (x) { *x = 0; } if (y) { *y = WALK_EYE; } if (z) { *z = 0; }
        og_actions_open(&g_level.scene,0,0);
        return 1;
    }
    g_place_data = *travel_place_data();
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
    if (!hold_actions()) { return 0; }
    if (og_world_state()->where.kind == OG_PLACE_DUNGEON && !og_encounter_open(&g_level.scene,encounter_art)) {
        why("Cannot restore the dungeon encounters"); return 0;
    }
    /* A dungeon loaded: the foes of the quests placed in it are made (func_0004C759 links them as a location loads). */
    if (og_world_state()->where.kind == OG_PLACE_DUNGEON) { og_quest_location_loaded(og_world_state()->where.region, quest_outside()); }
    world_start(g_world, &sx, &sy, &sz, &yaw, &pitch);
    if (!restoring && !changing && og_world_state()->where.kind == OG_PLACE_DUNGEON) { marker6(&sx,&sy,&sz); }
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

int seam_open_hold(float *x, float *y, float *z) { return seam_open_place(0,0,x,y,z); }
void seam_land(Camera *camera, int restoring, int changing)
{
    TravelPose at;
    OgMotion motion;
    int ox, oz, sx, sz;
    travel_land(&g_level,camera,WALK_EYE,&at);
    if (restoring && og_world_state()->where.kind == OG_PLACE_WORLD) {
        stream_origin(&ox,&oz);
        og_magic_shift((ox-g_saved_where.origin_x)*STREAM_CHUNK,(oz-g_saved_where.origin_z)*STREAM_CHUNK);
    }
    if (!restoring && !changing && g_world && og_world_state()->where.kind == OG_PLACE_DUNGEON) { marker6(&at.x,&at.y,&at.z); }
    if (restoring && og_state()->motion_valid) {
        motion = og_state()->motion; stream_origin(&ox,&oz);
        sx = g_saved_where.origin_x-ox; sz = g_saved_where.origin_z-oz;
        if (og_world_state()->where.kind == OG_PLACE_WORLD) {
            motion.feet[0] += sx*STREAM_CHUNK; motion.safe[0] += sx*STREAM_CHUNK;
            motion.feet[2] += sz*STREAM_CHUNK; motion.safe[2] += sz*STREAM_CHUNK;
        }
        og_move_restore(&motion);
    } else {
        og_move_init(); og_move_place(at.x,at.y,at.z,df_iround(at.yaw*2048/DF_TAU));
        og_move_capture(&motion); motion.pitch = df_iround(at.pitch*2048/DF_TAU); og_move_restore(&motion);
    }
    og_move_camera(camera);
    og_state_pose(camera->x,og_move_body()->feet[1]+og_move_body()->eye_now,camera->z,motion.yaw,motion.pitch);
    og_travel_capture();
    {
        const char *said;
        said = travel_said(); if (said) { og_msg_set(said); }
    }
}

void seam_close_hold(void)
{
    float x, y, z, yaw;

    if (!g_place_open) { return; }
    og_maps_close();
    og_ground_close();
    magic_lighting(0);
    og_magic_close();
    og_encounter_close(); g_game_entities = 0; g_original_entities = g_encounter_art_count = 0;
    og_actions_close();
    /* No queued arrival may carry a position into a newly loaded place. */
    interact_teleport(&x,&y,&z,&yaw);
    interact_set_body(0);
    og_dungeon_close();
    g_arrow_mesh = 0; g_arrow_models = 0; g_arrow_bounds = 0; g_arrow_spans = 0;
    og_weapon_cancel();
    cif_close(&g_weapon_art); g_weapon_file = -1;
    if (g_weapon_pic) { pic_free(g_weapon_pic); g_weapon_pic = 0; }
    g_weapon_frame = -1;
    if (g_cast_pic) { pic_free(g_cast_pic); g_cast_pic = 0; }
    sound_stop_all();
    texture_cache_clear();
    g_world = 0; g_place_open = 0;
    g_hold_flats = 0; g_hold_usables = 0;
    travel_drop();
    mem_zero(&g_level, sizeof(g_level));
}

int seam_capture_world(void)
{
    if (!g_place_open) { return 0; }
    og_world_capture();
    og_actions_capture();
    og_encounter_capture();
    travel_keep_now();
    keep_save(&g_level.scene,0,g_level.scene.nmovers,g_level.scene.entities,g_level.scene.nentities,0,0,0);
    return og_state_capture_world();
}

void seam_creatures(const Camera *cam)
{
    og_ground_frame(cam);
    if (!g_world) {
        return;
    }
    hold_dropped_piles();
    magic_lighting((og_character()->conditions & 16) != 0);
    hold_arrows(cam);
    world_creatures_step(g_world, cam, 2048.0f, 2.0f);
    {
        const Entity *loaded;
        Entity *game;
        int i;
        for (i = 0; g_game_entities && i < g_original_entities; i++) {
            loaded = world_entity(g_world,i); game = &g_game_entities[i];
            if (!game->live && loaded->live) {
                game->pose[ENTITY_WALK] = loaded->pose[ENTITY_WALK]; game->pose[ENTITY_IDLE] = loaded->pose[ENTITY_IDLE];
                game->y = loaded->y; game->live = 1;
            }
        }
    }
    {
        int i;
        for (i = 0; i < og_dungeon_count(); i++) {
            if (og_dungeon_loaded(i) && og_dungeon_entity(i)->dead) { g_hold_pile_serial--; }
        }
    }
    hold_dropped_piles();
    /* The world fills reserved creature picture slots as they stream in. */
    if (g_world_images) { mem_copy((void *)g_level.images,g_world_images,g_world_image_count*sizeof(Image *)); }
}

Level *seam_level(void)
{
    return g_place_open ? &g_level : 0;
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
            blit_ix(x0 + col * 22, y0 + row * 22, g_spop + piece * 22 * 22, 22, 22, 1);
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
        *palette = og_maps_up() ? g_maps_rgb : g_screen_kind == 2 ? g_chgn_rgb : (g_screen_kind == 1 ? g_art_rgb : g_map_rgb);
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
    og_potions_register();
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

/* The coins that turn behind what is for sale (DFU's coinsAnimTextureName: TEXTURE.434 record 6, 0.08 s a frame): the frame showing, and the one the screen was last painted with. */
static int g_coin_now = -1, g_coin_shown = -1;
static int coin_frame_now(void)
{
    int n, mhz;

    if (!possession_texture(434,6)) { return -1; }
    n = texture_frame_count(434,6);
    n = n > 16 ? 16 : n;
    mhz = prof_mhz();
    return n > 1 && mhz > 0 ? (int)((prof_now() / ((df_u32)mhz * 80000u)) % (df_u32)n) : 0;
}

static void paint_coins(int x, int y)
{
    const Image *frame;
    int row, col, color, left, top;

    if (g_coin_now < 0) { return; }
    frame = texture_load_frame(434,6,g_coin_now);
    if (!frame || frame->width > 50 || frame->height > 37) { return; }
    left = x+(50-frame->width)/2; top = y+(37-frame->height)/2;
    for (row = 0; row < frame->height; row++) {
        for (col = 0; col < frame->width; col++) {
            color = frame->index[row*frame->width+col];
            if (color && color != 255) { put_px(left+col,top+row,color); }
        }
    }
}

/* inv_draw_item_cell: an enchanted item has TEXTURE.380 record 5 centred in its cell, under the picture. */
static void paint_enchant_mark(int x, int y)
{
    const Image *mark;
    int row, col, color, left, top;

    mark = possession_texture(380,5);
    if (!mark || mark->width > 50 || mark->height > 37) { return; }
    left = x+(50-mark->width)/2; top = y+(37-mark->height)/2;
    for (row = 0; row < mark->height; row++) {
        for (col = 0; col < mark->width; col++) {
            color = mark->index[row*mark->width+col];
            if (color && color != 255) { put_px(left+col,top+row,color); }
        }
    }
}

static void paint_item_cell(const OgObject *item, int x, int y)
{
    char line[32];
    if (og_item_enchanted(item->id)) { paint_enchant_mark(x,y); }
    if (og_possessions()->trade == OG_MARKET_BUY) { paint_coins(x,y); }
    paint_item_icon(item,x+2,y+2,46,33);
    if (og_item_equipped(item->id) >= 0) { outline(x,y,50,37,145); }
    if (item->count > 1 || item->group == 28) {
        log_format(line,sizeof(line),"%d",item->group == 28 ? item->value : item->count);
        text_at(x+2,y+29,line);
    }
}

/* Words filled into lines no wider than width pixels, a word never split; how many lines. */
static int wrap_words(const char *text, char lines[][96], int most, int width)
{
    char test[96];
    int rows, len, word, i;

    rows = 0;
    lines[0][0] = 0;
    while (*text && rows < most) {
        while (*text == ' ') { text++; }
        for (word = 0; text[word] && text[word] != ' '; word++) { }
        len = 0;
        while (lines[rows][len]) { len++; }
        for (i = 0; i < len; i++) { test[i] = lines[rows][i]; }
        if (len) { test[len++] = ' '; }
        for (i = 0; i < word && len < 94; i++) { test[len++] = text[i]; }
        test[len] = 0;
        if (lines[rows][0] && smallfont_width(&g_font,test) > width) {
            if (++rows >= most) { rows = most - 1; break; }
            lines[rows][0] = 0;
            continue;
        }
        for (i = 0; i <= len; i++) { lines[rows][i] = test[i]; }
        text += word;
    }
    return lines[rows][0] ? rows + 1 : rows;
}

/* The bank (bank_draw, src/hand/func_0006BA50.c): BANK00I0.IMG at its place, the balance, the gold carried, and a loan with its date. */
static void paint_bank(void)
{
    ImgPicture window;
    const OgBankAccount *account;
    char line[80];

    cover_screen(0,0,0);
    if (img_load("BANK00I0.IMG",&window)) { blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    account = &og_bank()->account[og_possessions()->bank_region];
    log_format(line,sizeof(line),"%d",account->balance); text_at(197,19,line);
    log_format(line,sizeof(line),"%d",og_gold_total()); text_at(203,29,line);
    if (account->loan_due) {
        log_format(line,sizeof(line),"%d",account->loan_owed); text_at(143,39,line);
        og_date_text(account->loan_due,line,sizeof(line)); text_at(119,49,line);
    }
    if (og_possessions()->prompt == OG_PROMPT_AMOUNT) {
        log_format(line,sizeof(line),"%s_",og_possessions()->input); text_at(157,152,line);
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
    if (view->trade) {
        /* inventory_draw: the shop's column (INVE08/10/12), the lit button of the current action, then the doll. */
        ImgPicture column, lit;
        static const int lit_top[4] = {0, 0, 0, 0};
        char file[16];
        int top, act;

        (void)lit_top;
        log_format(file,sizeof(file),"INVE%02dI0.IMG",view->trade*2+6);
        if (img_load(file,&column)) { blit_ix(column.x,column.y,column.pixels,column.width,column.height,0); }
        log_format(file,sizeof(file),"INVE%02dI0.IMG",view->trade*2+7);
        /* Info is the second button of every column, Select the third (inv_mode_buttons). */
        act = view->action == OG_ITEM_INFO ? 36 : 58;
        if (img_load(file,&lit)) {
            for (top = act; top <= act+13; top++) {
                int col;
                for (col = 226; col <= 256; col++) {
                    int sx, sy;
                    sx = col-lit.x; sy = top-lit.y;
                    if (sx >= 0 && sx < lit.width && sy >= 0 && sy < lit.height) { put_px(col,top,lit.pixels[sy*lit.width+sx]); }
                }
            }
        }
    } else {
        copy_art_rect(&g_possession_art[1],226,action_y[view->action],31,14);
    }
    paint_doll(49,13);
    if (view->trade) {
        ImgPicture cost;
        char total[16];

        if (img_load("SHOP00I0.IMG",&cost)) { blit_ix(cost.x,cost.y,cost.pixels,cost.width,cost.height,0); }
        log_format(total,sizeof(total),"%d",view->price);
        text_at(77,15,total);
        log_format(total,sizeof(total),"%d",og_character()->gold);
        text_at(107,15,total);
    }
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
    if (!view->trade) {
        log_format(line,sizeof(line),"%d",og_character()->gold);
        centered_text(240,151,line,145);
    }
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
        smallfont_draw(&g_font,g_buf,320,200,14,27+shown*9,
                       og_progress_raised(skill) ? 146 : 145,-1,line); shown++;
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
    if (og_progress()->active) {
        i = og_possessions()->selected;
        if (i < 0 || i >= OG_ATTRS) { i = 0; }
        blit_ix(176,5+i*24,g_attr_arrows.pixels,g_attr_arrows.width,g_attr_arrows.height,1);
        number_at(183,5+i*24+14-g_font.height,og_progress()->points,1,0);
    }
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
    log_format(line,sizeof(line),"Cost: %d",og_magicka_cost(og_spell_cost(spell,og_character()))); text_at(155,166,line);
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

/* The original moving Light spell is a candle ahead of the player.
 * The current static-decal renderer uses an ambient lift for its light. */
static void magic_lighting(int enabled)
{
    static int active, base, applied;
    int current;
    current = light_ambient();
    if (active && current != applied) { base = current; }
    if (enabled) {
        if (!active) { base = current; }
        applied = base+64; if (applied > 255) { applied = 255; }
        light_set_ambient(applied); active = 1;
    } else if (active) { light_set_ambient(base); active = 0; }
}

/* The quests' faces (3.11 of quest-hooks.md): each used slot in order at x = 32 * n + 8, y = 36 of classic's screen, FACE{sex}{bank}I0.CIF's picture (a child's from KIDS00I0.CIF), colour 0 clear; drawn
 * with the world's HUD, so under no menu. The strip is made again when the faces change. */
static int g_face_pic;
static df_u8 g_face_pixels[320*48];
static df_u32 g_face_signature;

static void quest_faces_draw(void)
{
    df_u32 signature;
    int i, n;

    signature = 2166136261u; n = 0;
    for (i = 0; i < OG_QUEST_FACES; i++) {
        const OgQuestFace *f;

        f = og_quest_face(i);
        if (f) { signature = (signature ^ (df_u32)(f->face + 1)) * 16777619u; signature = (signature ^ (df_u32)(f->quest_id)) * 16777619u; n++; }
    }
    if (!n || !g_have_art_rgb) { return; }
    if (!g_face_pic || signature != g_face_signature) {
        int slot;

        mem_zero(g_face_pixels, sizeof(g_face_pixels));
        slot = 0;
        for (i = 0; i < OG_QUEST_FACES; i++) {
            const OgQuestFace *f;
            CifSet set;
            const CifFrame *frame;
            char name[16];
            int x, y, record;

            f = og_quest_face(i);
            if (!f) { continue; }
            if (f->face & 0x10) { log_format(name, sizeof(name), "KIDS00I0.CIF"); record = f->face & 15; }
            else { log_format(name, sizeof(name), "FACE%d%dI0.CIF", (f->face & 0x80) ? 1 : 0, (f->face & 0x40) ? 1 : 0); record = f->face & 15; }
            if (cif_open(name, &set)) {
                frame = cif_frame(&set, record);
                if (frame) {
                    for (y = 0; y < frame->h && y < 48; y++) {
                        for (x = 0; x < frame->w && slot * 32 + 8 + x < 320; x++) { g_face_pixels[y * 320 + slot * 32 + 8 + x] = frame->pixels[y * frame->w + x]; }
                    }
                }
                cif_close(&set);
            }
            slot++;
        }
        if (!g_face_pic) { g_face_pic = pic_load(g_face_pixels, 320, 48, g_art_rgb, 1); }
        else { pic_update(g_face_pic, g_face_pixels); }
        g_face_signature = signature;
    }
    if (g_face_pic) { og_hud_blit_part(g_face_pic, 0, 0, 320, 48, 0, 36, 320, 48); }
}

static int g_effect_pic;
static df_u8 g_effect_pixels[320*64];
static df_u32 g_effect_signature;
static void magic_hud_draw(const OgMagicState *state)
{
    const OgEffect *e;
    quest_faces_draw();
    df_u32 signature;
    int i, j, count, duplicate, x, y, px, py, icon, blink, hostile, expiring, columns;
    if (!g_spell_icons) { return; }
    signature = 2166136261u; count = 0;
    blink = (og_state()->ticks/8) & 1;
    for (i = 0; i < OG_EFFECTS; i++) {
        e = &state->effect[i]; if (!e->active || e->target.place || e->target.thing) { continue; }
        signature = (signature ^ (df_u32)(e->spell*257+e->rounds+(e->caster.thing & 255)))*16777619u; count++;
    }
    signature ^= (df_u32)(og_options_flags() & 1); signature ^= (df_u32)blink;
    if (!count) { return; }
    if (!g_effect_pic || signature != g_effect_signature) {
        mem_zero(g_effect_pixels,sizeof(g_effect_pixels)); count = 0;
        for (i = 0; i < OG_EFFECTS; i++) {
            e = &state->effect[i]; if (!e->active || e->target.place || e->target.thing) { continue; }
            duplicate = 0;
            for (j = 0; j < i; j++) {
                if (state->effect[j].active && !state->effect[j].target.place && !state->effect[j].target.thing &&
                    state->effect[j].spell == e->spell && state->effect[j].caster.thing == e->caster.thing &&
                    state->effect[j].caster.place == e->caster.place) { duplicate = 1; break; }
            }
            if (duplicate || count >= 24) { continue; }
            hostile = e->caster.place || e->caster.thing;
            columns = hostile && (og_options_flags() & 1) ? 11 : 12;
            /* runspell.c uses D_00185CEC: 16,40,...,280. Hostile
             * spells share their own row above the bar (or at y=177). */
            x = 16+(count%columns)*24; y = hostile ? 48 : count/12*24;
            icon = e->icon >= 200 ? 10 : e->icon; count++;
            /* A lasting spell (a held item's) never blinks. */
            expiring = !e->permanent && e->rounds <= 1;
            for (j = i+1; j < OG_EFFECTS && expiring; j++) {
                if (state->effect[j].active && !state->effect[j].target.place && !state->effect[j].target.thing &&
                    state->effect[j].spell == e->spell && state->effect[j].caster.thing == e->caster.thing &&
                    state->effect[j].caster.place == e->caster.place && state->effect[j].rounds > 1) { expiring = 0; }
            }
            if (icon < 0 || icon >= 69 || (expiring && blink)) { continue; }
            for (py = 0; py < 16; py++) {
                for (px = 0; px < 16; px++) {
                    g_effect_pixels[(y+py)*320+x+px] = g_spell_icons[(icon/20*16+py)*320+icon%20*16+px];
                }
            }
        }
        if (!g_effect_pic) { g_effect_pic = pic_load(g_effect_pixels,320,64,g_art_rgb,1); }
        else { pic_update(g_effect_pic,g_effect_pixels); }
        g_effect_signature = signature;
    }
    x = (og_options_flags() & 1) ? 35 : 0;
    og_hud_blit_part(g_effect_pic,0,0,320,48,x,16,320,48);
    og_hud_blit_part(g_effect_pic,0,48,320,16,x,(og_options_flags() & 1) ? 177 : 132,320,16);
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

/*
 * The talk window (talk_draw, src/lifted/talk.c 282): TALK01I0.IMG is the whole screen, TALK02I0.IMG the four tabs
 * greyed (drawn at 4,26) and TALK03I0.IMG the two lit ways of asking, 107 x 10 each. In "where is" the selected tab is
 * put back from the base picture. The name sits centred at 213,53, the face at 119,65, the tone's button is filled,
 * the list runs from y = 71 in 7-pixel rows, the question is at 125,10 and the log at 190,66 (18 rows).
 */
static void paint_talk(void)
{
    const OgTalkWin *t;
    ImgPicture base, grey, lit;
    char name[OG_NAME_MAX];
    int i, row, x, y, w;
    const short *b;

    t = og_talkwin();
    cover_screen(0, 0, 0);
    if (img_load("TALK01I0.IMG", &base)) { blit_ix(base.x, base.y, base.pixels, base.width, base.height, 0); }
    df_copy(name, g_talk_name, (int)sizeof(name));
    w = smallfont_width(&g_font, name);
    smallfont_draw(&g_font, g_buf, 320, 200, 213 - w / 2, 53, 145, 156, name);
    if (g_talk_face_ok) { blit_ix(119, 65, g_talk_face, FACE_W, FACE_H, 0); }
    b = og_talkwin_button(OG_TB_POLITE + t->tone);
    fill_box(b[0], b[1], b[2] - b[0] + 1, b[3] - b[1] + 1, 244);
    if (img_load("TALK02I0.IMG", &grey)) { blit_ix(4, 26, grey.pixels, grey.width, grey.height, 0); }
    if (t->mode != 0 && base.width == 320) {
        b = og_talkwin_button(OG_TB_PLACES + t->tab);
        for (y = b[1]; y <= b[3]; y++) {
            for (x = b[0]; x < b[0] + 107; x++) { put_px(x, y, base.pixels[y * 320 + x]); }
        }
    }
    if (img_load("TALK03I0.IMG", &lit)) {
        b = og_talkwin_button(t->mode);
        for (y = b[1]; y <= b[3]; y++) {
            for (x = 0; x < 107; x++) {
                int sy;

                sy = t->mode * 10 + (y - b[1]);
                if (sy < lit.height) { put_px(b[0] + x, y, lit.pixels[sy * lit.width + x]); }
            }
        }
    }
    /* The list: the way back and the title in the dull colour, the selected row bright. */
    for (i = t->top, row = 0; i < t->count && row < OG_TW_LIST_ROWS; i++, row++) {
        char text[OG_TW_ITEM_TEXT];
        int ink;

        df_copy(text, t->item[i].text, (int)sizeof(text));
        while (text[0] && smallfont_width(&g_font, text) > 94) {
            int n;

            for (n = 0; text[n]; n++) { }
            text[n - 1] = 0;
        }
        if (t->item[i].value < 0 && t->mode == 1) { ink = 96; }
        else if ((t->mode == 1 && t->tab == 0 && t->categories) ? t->item[i].value == t->row : i == t->row) { ink = 244; }
        else { ink = 145; }
        smallfont_draw(&g_font, g_buf, 320, 200, 6, 71 + row * 7, ink, 156, text);
    }
    for (i = 0; i < t->question_count; i++) {
        smallfont_draw(&g_font, g_buf, 320, 200, 125, 10 + i * 7, 145, 156, t->question[i]);
    }
    for (row = 0; row < OG_TW_ANSWER_ROWS && t->answer_scroll + row < t->answer_count; row++) {
        const char *line;

        i = t->answer_scroll + row;
        line = t->answer[i];
        while (*line == ' ') { line++; }
        smallfont_draw(&g_font, g_buf, 320, 200, 190, 66 + row * 7, t->asked[i] ? 96 : 145, 156, line);
    }
}

/* A shopkeeper's panel (repair_menu_frame, service_menu_frame): REPR01I0.IMG or GNRC01I0.IMG over the world; the service row's word at 159,70. */
static void paint_service(void)
{
    ImgPicture panel;
    const OgPossessions *view;

    view = og_possessions();
    cover_screen(0, 0, 0);
    if (img_load(view->service == OG_SERVICE_REPAIR ? "REPR01I0.IMG" : view->service == OG_SERVICE_TAVERN ? "TVRN00I0.IMG" : view->service == OG_SERVICE_GUILD ? og_guild_picture(view->service_member) : "GNRC01I0.IMG", &panel)) {
        blit_ix(panel.x, panel.y, panel.pixels, panel.width, panel.height, 0);
    }
    if (view->service == OG_SERVICE_GUILD) {
        centered_text(159, 71, view->service_label, 145);
    } else if (view->service != OG_SERVICE_REPAIR && view->service != OG_SERVICE_TAVERN) {
        centered_text(159, 70, view->service == OG_SERVICE_BANK ? "Banking" : "Sell", 145);
    }
}

/*
 * The potion maker (potionmaker_update, src/hand/func_0008F31F.c; MASK00I0.IMG): the name of what is being made and the gold under the window; the pack's ingredients three
 * across from x 16 and 38 pixels down (the first twelve from the scroll), each drawn at its cell's middle with its name under it; the cauldron's cells two across from x 221.
 */
static void paint_potion_maker(void)
{
    const OgPossessions *view;
    const OgCauldron *c;
    ImgPicture window;
    df_u32 ids[512];
    char line[40];
    int count, i, n, cells;

    view = og_possessions();
    cover_screen(0,0,0);
    if (img_load("MASK00I0.IMG",&window)) { blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    text_at(31,185,"Unnamed potion");
    log_format(line,sizeof(line),"%d",og_character()->gold); text_at(235,185,line);
    count = og_ingredients(ids,512);
    for (i = view->scroll[0], n = 0; i < count && n < 12; i++, n++) {
        const OgObject *item;
        char name[32];

        item = og_object(ids[i]);
        if (!item) { continue; }
        paint_item_icon(item,n%3*56+28-26,n/3*38+42-18,52,36);
        og_item_name(item,name,sizeof(name));
        centered_text(n%3*56+30,n/3*38+58+(n%3 == 1 ? 5 : 0),name,145);
    }
    c = og_cauldron();
    for (i = cells = 0; i < OG_CAULDRON; i++) {
        const OgObject *item;
        char name[32];

        if (!c->item[i]) { continue; }
        item = og_object(c->item[i]);
        if (!item) { continue; }
        paint_item_icon(item,(cells&1)*56+233-26,(cells>>1)*38+42-18,52,36);
        og_item_name(item,name,sizeof(name));
        centered_text((cells&1)*56+236,(cells>>1)*40+48,name,145);
        cells++;
    }
}

/* The spell shop (spellshop_update, spellshop_draw_spell): SPBK01I0.IMG, the offered spells in the list, the selected one's icon, element, target and effects, and its price at 97,172. */
/* spellmaker_update (func_00037014): INFO01I0.IMG, each effect's name centred at y 30 + 32 a slot, the points, gold, price and spell points, the name, the chosen target and element and the icon; with a panel open MASK05I0.IMG over it and the effect's numbers in their boxes (func_00038911). */
static void paint_maker(void)
{
    const OgMaker *maker;
    ImgPicture window, panel;
    char line[64];
    int i, k, type, sub, skill, shown;

    maker = og_maker();
    cover_screen(0,0,0);
    if (img_load("INFO01I0.IMG",&window)) { blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    for (i = 0; i < 3; i++) {
        type = maker->spell[i*2]; sub = maker->spell[i*2+1];
        if (type == 255) { continue; }
        if (sub != 255 && og_effect_sub_name(type,sub)) { log_format(line,sizeof(line),"%s %s",og_effect_name(type),og_effect_sub_name(type,sub)); }
        else { log_format(line,sizeof(line),"%s",og_effect_name(type)); }
        centered_text(160,30+i*32,line,146);
    }
    log_format(line,sizeof(line),"%d",og_character()->magicka); text_at(43,149,line);
    log_format(line,sizeof(line),"%d",og_character()->gold); text_at(40,158,line);
    log_format(line,sizeof(line),"%d",og_maker_shown()); text_at(59,167,line);
    log_format(line,sizeof(line),"%d",og_maker_spell_points()); text_at(70,176,line);
    text_at(60,185,(const char *)maker->spell+OG_MAKER_NAME_AT);
    if (g_spell_masks) {
        k = maker->spell[7]; blit_stride(275,114+16*k,g_spell_masks+k*640,40,24,16);
        k = maker->spell[6]; blit_stride(299,114+16*k,g_spell_masks+k*640+24,40,16,16);
    }
    if (g_spell_icons && maker->spell[72] < OG_MAKER_ICONS) {
        k = maker->spell[72]; blit_stride(288,94,g_spell_icons+(k/20)*16*320+(k%20)*16,320,16,16);
    }
    if (!maker->kind) { return; }
    if (img_load("MASK05I0.IMG",&panel)) { blit_ix(panel.x,panel.y,panel.pixels,panel.width,panel.height,1); }
    skill = og_character()->skill[og_effect_school(maker->spell[maker->slot*2])];
    shown = (110-skill)*(short)(maker->spell[8+maker->slot*2] | (maker->spell[9+maker->slot*2] << 8))/100;
    log_format(line,sizeof(line),"%d",shown); text_at(275,119,line);
    {
        static const int left[11] = {64, 104, 160, 64, 104, 160, 64, 104, 144, 184, 240};
        static const int top[11] = {94, 94, 94, 114, 114, 114, 134, 134, 134, 134, 134};

        for (k = 0; k < 11; k++) {
            if ((k < 3 && !(maker->kind & 1)) || (k >= 3 && k < 6 && !(maker->kind & 2)) || (k >= 6 && !(maker->kind & 4))) { continue; }
            number_at(left[k]+12,top[k]+4,og_maker_value(k),1,0);
        }
    }
}

/* itemmaker_draw (func_000562A1): ITEM00I0.IMG; the current tab's strip of ITEM01I0.IMG at (175, 9 a tab + 6); the gold, the item's name, the gold cost and the points used of its limit; the item in its box; the left list's four rows; the powers' rows from x 10 and the side effects' from x 108, 7 pixels each, colour 146 and 193 for what a soul gave. */
static void paint_item_maker(void)
{
    const OgPossessions *view;
    const OgItemMaker *maker;
    const OgObject *item;
    ImgPicture window;
    const df_u8 *tabs;
    df_u32 size, ids[OG_OBJECTS];
    OgImRow rows[40];
    char line[48];
    int i, k, side, n, count;

    view = og_possessions();
    maker = og_itemmaker();
    cover_screen(0,0,0);
    if (img_load("ITEM00I0.IMG",&window)) { blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    tabs = vfs_read("ITEM01I0.IMG",&size);
    if (tabs && size >= 4u*729u && view->im_tab >= 0 && view->im_tab < 4) { blit_stride(175,view->im_tab*9+6,tabs+view->im_tab*729,81,81,9); }
    log_format(line,sizeof(line),"%d",og_character()->gold); text_at(70,15,line);
    item = maker->item ? og_object(maker->item) : 0;
    if (item) {
        text_at(51,3,item->name);
        log_format(line,sizeof(line),"%d",og_itemmaker_gold()); text_at(63,27,line);
        log_format(line,sizeof(line),"%d/%d",og_itemmaker_points_used(),og_itemmaker_capacity(item->id)); text_at(96,39,line);
        paint_item_icon(item,203,69,46,34);
    }
    count = og_possessions_im_list(ids,OG_OBJECTS);
    for (i = 0; i < 4 && view->im_scroll+i < count; i++) {
        static const int rowtop[4] = {50, 87, 124, 161};

        item = og_object(ids[view->im_scroll+i]);
        if (item) { paint_item_icon(item,265,rowtop[i]+1,46,33); }
    }
    for (side = 0; side < 2; side++) {
        n = og_possessions_im_rows(side,rows,40);
        for (k = 0; k < n; k++) {
            if (!rows[k].text[0]) { continue; }
            smallfont_draw(&g_font,g_buf,320,200,side ? 108 : 10,60+k*7,rows[k].soul ? 193 : 146,156,rows[k].text);
        }
    }
}

static void paint_spellshop(void)
{
    const OgPossessions *view;
    const OgSpellDef *spell;
    ImgPicture window;
    char line[48];
    int i, at, x, y, length;

    view = og_possessions();
    cover_screen(0,0,0);
    if (img_load("SPBK01I0.IMG",&window)) { blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    for (i = 0; i < 14 && i+view->scroll[0] < view->shop_count; i++) {
        at = i+view->scroll[0];
        spell = og_spell_def(view->shop[at]);
        if (!spell) { continue; }
        if (at == view->selected) { fill_box(27,30+i*9,111,9,150); }
        df_copy(line,spell->name,sizeof(line));
        for (length = 0; line[length]; length++) { }
        while (length && smallfont_width(&g_font,line) > 109) { line[--length] = 0; }
        text_at(28,30+i*9,line);
    }
    fill_box(145,45,7,104,114);
    if (view->shop_count > 14) {
        int height, top;

        height = 104*14/view->shop_count; top = 45+(104-height)*view->scroll[0]/(view->shop_count-14);
        fill_box(145,top,7,height,146);
    }
    spell = og_spell_def(view->shop[view->selected]);
    if (!spell) { return; }
    text_at(148,20,spell->name);
    if (g_spell_icons && spell->icon < 69) {
        x = (spell->icon%20)*16; y = (spell->icon/20)*16;
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
    log_format(line,sizeof(line),"%d",og_spellshop_price(spell,og_holiday_today(og_state()->minutes,travel_region())));
    text_at(97,172,line);
}

static int present_possessions(void)
{
    const OgPossessions *view;
    MemHeap *heap, *was;
    char line[160];
    int i;
    view = og_possessions();
    if (!view->page) { g_possession_page = 0; return 0; }
    g_coin_now = view->page == OG_VIEW_INVENTORY && view->trade == OG_MARKET_BUY ? coin_frame_now() : -1;
    if (g_possession_page == view->page && g_possession_revision == view->revision && g_coin_shown == g_coin_now && og_pic(OG_PIC_SCREEN)) { return 1; }
    g_possession_page = view->page; g_possession_revision = view->revision; g_coin_shown = g_coin_now;
    if (!g_buf) { g_buf = (df_u8 *)mem_keep(64000); }
    if (!g_buf) { return 1; }
    heap = mem_heap_new();
    if (!heap) { return 1; }
    was = mem_heap_use(heap); g_panel_file_count = 0;
    if (view->page == OG_VIEW_INVENTORY) { paint_inventory(); }
    else if (view->page == OG_VIEW_SHEET) { paint_character_sheet(); }
    else if (view->page == OG_VIEW_SPELLS) { paint_spellbook(); }
    else if (view->page == OG_VIEW_MESSAGE) { cover_screen(0,0,0); }
    else if (view->page == OG_VIEW_BANK) { paint_bank(); }
    else if (view->page == OG_VIEW_TALK) { paint_talk(); }
    else if (view->page == OG_VIEW_SERVICE) { paint_service(); }
    else if (view->page == OG_VIEW_POTION) { paint_potion_maker(); }
    else if (view->page == OG_VIEW_SPELLSHOP) { paint_spellshop(); }
    else if (view->page == OG_VIEW_MAKER) { paint_maker(); }
    else if (view->page == OG_VIEW_ITEMMAKER) { paint_item_maker(); }
    else if (view->page == OG_VIEW_COURT) {
        /* cort01i0.img fills the screen once a court sits; the offer to surrender is over the world. */
        ImgPicture court;
        cover_screen(0,0,0);
        if (view->court.open && img_load("CORT01I0.IMG",&court)) { blit_ix(court.x,court.y,court.pixels,court.width,court.height,0); }
    }
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
    if (view->prompt == OG_PROMPT_LIST) {
        /* list_popup_open_strings: the rows in a box, each eight pixels from y 34. */
        int k;

        int shown;

        shown = view->list_count < OG_LIST_SHOWN ? view->list_count : OG_LIST_SHOWN;
        fill_box(100,26,120,shown*8+14,45); outline(100,26,120,shown*8+14,145);
        for (k = 0; k < shown; k++) { smallfont_draw(&g_font,g_buf,320,200,106,34+k*8,145,156,view->list[view->list_top+k]); }
    } else if (view->prompt == OG_PROMPT_OFFER || view->prompt == OG_PROMPT_ASK) {
        /* msgbox_yes_no_rsc: the shopkeeper's words over a box, Accept and Reject under them. */
        char wrapped[10][96];
        int rows, k, court;

        /* The court's questions are long: a taller, wider box that still leaves the buttons at 116..134 free. */
        court = view->page == OG_VIEW_COURT;
        rows = wrap_words(view->offer,wrapped,court ? 10 : 6,court ? 208 : 176);
        if (court) { fill_box(48,8,224,126,45); outline(48,8,224,126,145); }
        else if (g_spop && g_buttons && view->page != OG_VIEW_MESSAGE) {
            /* msgbox_show_string: the words on SPOP's parchment, the Yes and No pictures of BUTTONS.RCI under them (the pointer's regions are the buttons' own). */
            lay_paper(61,34,9,5);
            lay_button(3,112,117); lay_button(4,176,117);
        } else { fill_box(68,40,184,96,45); outline(68,40,184,96,145); }
        for (k = 0; k < rows; k++) { centered_text(160,(court ? 14 : 44)+k*10,wrapped[k],145); }
        if (court || view->page == OG_VIEW_MESSAGE || !g_spop || !g_buttons) {
            centered_text(117,121,(view->page == OG_VIEW_COURT || view->page == OG_VIEW_MESSAGE) ? view->yes : "Accept",145);
            centered_text(203,121,(view->page == OG_VIEW_COURT || view->page == OG_VIEW_MESSAGE) ? view->no : "Reject",145);
        }
    } else if (view->prompt && view->prompt != OG_PROMPT_AMOUNT && view->prompt != OG_PROMPT_LIST) {
        const char *prompt;
        prompt = view->prompt == OG_PROMPT_GOLD ? "How much gold?" : view->prompt == OG_PROMPT_NAME ? "Name thyself" :
                 view->prompt == OG_PROMPT_SPELL_NAME ? "Enter spell name" :
                 view->prompt == OG_PROMPT_ANSWER ? "Your answer:" : view->prompt == OG_PROMPT_NUMBER ? view->offer :
                 "Enter new page:";
        if (g_spop && g_buttons) {
            /* The prompt on the parchment, the typed line under the words, the Yes and No pictures at the bottom. */
            lay_paper(61,34,9,5);
            lay_button(3,112,117); lay_button(4,176,117);
        } else { fill_box(68,65,184,75,45); outline(68,65,184,75,145); }
        centered_text(160,g_spop && g_buttons ? 56 : 75,prompt,145);
        log_format(line,sizeof(line),"%s_",view->input); centered_text(160,g_spop && g_buttons ? 84 : 96,line,145);
        if (!g_spop || !g_buttons) { centered_text(117,121,"Accept",145); centered_text(203,121,"Reject",145); }
    }
    for (i = 0; i < g_panel_file_count; i++) { texture_forget(g_panel_files[i]); }
    mem_heap_use(was); mem_heap_delete(heap);
    publish_surface(1,(view->page == OG_VIEW_SPELLS && !view->underlay) || view->page == OG_VIEW_MESSAGE || view->page == OG_VIEW_BANK || view->page == OG_VIEW_TALK || view->page == OG_VIEW_POTION || view->page == OG_VIEW_SPELLSHOP || view->page == OG_VIEW_MAKER || view->page == OG_VIEW_ITEMMAKER || (view->page == OG_VIEW_COURT && !view->court.open));
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

    if (og_maps_up()) { present_maps(); return; }
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

/* Map records and placement remain behind this adapter. OGFALL supplies its
 * own journey rules, object state and UI; the shared layer builds geometry. */
static int location_catalogue(void)
{
    return worldmap_open() ? worldmap_locations() : 0;
}
static int location_record(int ordinal, OgLocation *out)
{
    if (!worldmap_location_info(ordinal,&out->px,&out->py,&out->region,&out->index,&out->type,&out->known)) { return 0; }
    df_copy(out->name,worldmap_location_name(ordinal),(int)sizeof(out->name)); return 1;
}
static int go_to_location(int region, int index)
{
    int px, py, type;
    if (!worldmap_open() || !worldmap_pixel_of(region,index,&px,&py)) { return 0; }
    type = worldmap_location_type(px,py);
    return travel_go_to_place(region,index,type == 4 || type == 7 || type == 10 || type == 12 || type == 13);
}
static int classic_angle(float radians)
{
    int angle;
    angle = df_iround(radians*2048/DF_TAU); return angle & 2047;
}
static void current_place(OgWhere *out, int *px, int *py)
{
    TravelWhere here;
    Camera camera;
    const Dungeon *data;
    int origin[2], exterior, region, index;
    if (!g_place_open) { return; }
    travel_here(&here,origin); og_move_camera(&camera);
    out->kind = here.kind; out->located = here.located; out->region = here.region; out->index = here.index;
    out->block = here.block; out->record = here.record; out->has_back = here.has_back;
    out->back_x = (int)here.back_x; out->back_z = (int)here.back_z; out->back_yaw = classic_angle(here.back_yaw);
    out->origin_x = origin[0]; out->origin_z = origin[1];
    if (here.kind == TRAVEL_WORLD) {
        if (!travel_pixel(&camera,px,py)) { return; }
        if (worldmap_location(*px,*py,&region,&index)) {
            out->region = region; out->index = index; out->located = 1;
            og_travel_discover(worldmap_location_id(*px,*py));
        }
    } else {
        data = travel_place_data(); exterior = here.index;
        if (here.kind == TRAVEL_DUNGEON) { exterior = dungeon_outside(here.region,data->exterior_id); }
        if (exterior >= 0) { worldmap_pixel_of(here.region,exterior,px,py); }
    }
    if (og_state()->region != out->region || og_state()->location != out->index || og_state()->environment != here.kind+1) {
        og_state_set_place(out->region,out->index,here.kind+1);
    }
}
static int return_to_anchor(const OgWhere *saved, float x, float eye_y, float z, int yaw, int pitch)
{
    TravelWhere where;
    int origin[2];
    float feet[3];
    mem_zero(&where,sizeof(where));
    where.kind = saved->kind; where.located = saved->located; where.region = saved->region; where.index = saved->index;
    where.block = saved->block; where.record = saved->record; where.has_back = saved->has_back;
    where.back_x = saved->back_x; where.back_z = saved->back_z; where.back_yaw = saved->back_yaw*(DF_TAU/2048);
    origin[0] = saved->origin_x; origin[1] = saved->origin_z;
    feet[0] = x; feet[1] = eye_y-WALK_EYE; feet[2] = z;
    if (where.kind == TRAVEL_WORLD) {
        /* Open the stream at the anchor, not at the old building's doorstep. */
        where.has_back = 1;
        where.back_x = (double)x+(double)origin[0]*STREAM_CHUNK;
        where.back_z = (double)z+(double)origin[1]*STREAM_CHUNK;
    }
    travel_to_pose(&where,feet,origin,yaw*(DF_TAU/2048),pitch*(DF_TAU/2048)); return 1;
}
static void install_travel(void)
{
    OgTravelHooks hooks;
    mem_zero(&hooks,sizeof(hooks));
    hooks.count = location_catalogue; hooks.location = location_record; hooks.go = go_to_location;
    hooks.asked = travel_asked; hooks.passage = passage_gate; hooks.here = current_place;
    hooks.climate = worldmap_climate; hooks.return_to = return_to_anchor; og_travel_install(&hooks);
}
static int seam_world_art(void)
{
    int element, frame, slots[5][2], slot;
    const Image *image;
    for (element = 0; element < 5; element++) {
        for (frame = 0; frame < 2; frame++) {
            slot = wres_slot(375+element,frame,WRES_SPRITE);
            if (slot < 0 || !wres_acquire(slot)) { return 0; }
            slots[element][frame] = slot;
        }
    }
    og_magic_slots(slots); slot = wres_slot(210,3,WRES_SPRITE);
    if (slot < 0 || !wres_acquire(slot)) { return 0; }
    image = wres_image(slot);
    og_magic_light_sprite(slot,texture_extent(image->width,image->xscale),texture_extent(image->height,image->yscale));
    slot = wres_slot(216,0,WRES_SPRITE);
    if (slot < 0 || !wres_acquire(slot)) { return 0; }
    og_ground_open(&g_level.scene,slot);
    og_object_edit(OG_WORLD_ROOT)->place_key = 0x80000000u;
    mem_zero(g_street_blood,sizeof(g_street_blood)); mem_zero(g_street_blood_left,sizeof(g_street_blood_left));
    og_dungeon_open_street(&g_level.scene); og_dungeon_blood(-1,-1);
    return 1;
}
int seam_next_world(void) { return travel_next_is_world(); }
void seam_world_step(Camera *camera, float seconds)
{
    g_site_camera = camera;
    street_blood_tick(seconds);
    float dx, dz;
    if (!g_place_open || !g_level.streamed) { return; }
    travel_update(camera,1,2,seconds,&dx,&dz);
    if (dx || dz) {
        og_move_shift(dx,dz); og_dungeon_shift(dx,dz);
        og_magic_shift(dx,dz);
    }
    og_travel_capture();
}
int seam_world_needs_reload(void) { return g_place_open && travel_must_reload(); }
void seam_world_reload(Camera *camera) { if (seam_world_needs_reload()) { travel_reload(camera); } }

/* Classic map presenters accept OGFALL data callbacks. They never receive an
 * OGFALL character disguised as a GFALL player. */
static int g_map_pic, g_map_marker, g_map_marker_step = -1;
static int g_map_x, g_map_y, g_map_drag;
static float g_map_repeat;
static unsigned map_gold(void) { return (unsigned)og_character()->gold; }
static const char *map_warning(void) { return og_journey_warning() ? og_words(1010) : 0; }
static void map_question(const char *place, char *out, int capacity)
{
    const char *text;
    int n, i;
    text = og_words(31); n = 0;
    while (*text && n+1 < capacity) {
        if (text[0] == '%' && text[1] == 't' && text[2] == 'c' && text[3] == 'n') {
            for (i = 0; place[i] && n+1 < capacity; i++) { out[n++] = place[i]; }
            text += 4;
        } else { out[n++] = *text++; }
    }
    out[n] = 0;
}
static void map_journey(int sx, int sy, int ex, int ey, int reckless, int inns, int ship, Journey *out)
{
    OgJourney trip;
    og_journey_plan(sx,sy,ex,ey,reckless,!inns,ship,og_journey_transport(),&trip);
    mem_zero(out,sizeof(*out)); out->minutes = trip.minutes; out->hours = trip.hours;
    out->days = (trip.minutes+1439)/1440; out->sea_pixels = trip.sea_pixels; out->cost = trip.cost;
}
static void close_maps(void)
{
    dunmap_close(); townmap_close(); travelmap_close();
    if (g_map_pic) { pic_free(g_map_pic); g_map_pic = 0; }
    if (g_map_marker) { pic_free(g_map_marker); g_map_marker = 0; }
    g_map_marker_step = -1; g_maps_mode = g_map_drag = 0;
    og_pic_set(OG_PIC_SCREEN,0);
}
/* The map opened to teleport (the guild's mode 100): a place chosen is arrived at, not travelled to. */
static int g_teleport;
static int open_maps(int trip)
{
    TravelMapRules rules;
    Camera camera;
    int region, index, block, record, i;
    float x, z;
    og_travel_capture(); og_move_camera(&camera);
    g_map_x = g_map_y = -1; g_map_drag = 0; g_map_repeat = 0;
    g_teleport = trip == 100;
    if (trip && !g_teleport && og_world_state()->where.kind != OG_PLACE_WORLD) { return 0; }
    if (!trip && og_world_state()->where.kind != OG_PLACE_WORLD) {
        if (!dunmap_open(og_move_body()->feet,camera.yaw)) { return 0; }
        g_maps_mode = 1;
    } else if (!trip && travel_town_here(&camera,&region,&index,&x,&z,&block,&record) && townmap_open(region,index,x,z,camera.yaw)) {
        g_maps_mode = 2;
    } else {
        rules.gold = map_gold; rules.known = og_travel_known; rules.plan = map_journey;
        rules.question = map_question; rules.warning = map_warning;
        if (!travelmap_open_rules(&rules,og_world_state()->px,og_world_state()->py,og_state()->region)) { return 0; }
        g_maps_mode = 3;
    }
    for (i = 0; i < 3; i++) { if (g_comp[i]) { pic_free(g_comp[i]); g_comp[i] = 0; } }
    og_pic_set(OG_PIC_SCREEN,0); return 1;
}
static int frame_maps(const OgIn *input)
{
    int result, dx, dy, pressed, repeat;
    result = 0; pressed = input->mouse_pressed & 3;
    if (input->reset) { g_map_drag = 0; g_map_x = g_map_y = -1; return 0; }
    if (g_maps_mode != 3 && input->pressed[OG_AUTOMAP]) { return 1; }
    if (g_maps_mode == 2 && input->pressed[OG_TRAVEL]) { close_maps(); return !open_maps(1); }
    if (input->key) {
        result = g_maps_mode == 1 ? dunmap_key(input->key) : g_maps_mode == 2 ? townmap_key(input->key) : travelmap_key(input->key);
    }
    if (g_maps_mode == 3) {
        if (input->on_picture) { travelmap_point(input->cx,input->cy); }
        if (pressed == 1 && input->on_picture) { result = travelmap_click(input->cx,input->cy); og_sound(203); }
        if (travelmap_step(input->dt) == TRAVELMAP_GO) {
            int region, index, cautious, reckless, camp, ship;
            Journey journey;
            travelmap_chosen(&region,&index,&journey,&cautious); travelmap_options(&reckless,&camp,&ship);
            if (g_teleport ? og_teleport(region,index) : og_journey_begin(region,index,reckless,camp,ship)) { return 1; }
        }
        return result == TRAVELMAP_LEAVE;
    }
    if (result == TOWNMAP_TRAVEL && g_maps_mode == 2) { close_maps(); return !open_maps(1); }
    if (result) { return 1; }
    if (input->wheel) { if (g_maps_mode == 1) { dunmap_wheel(input->wheel); } else { townmap_wheel(input->wheel); } }
    if (g_maps_mode == 2 && input->on_picture) { townmap_point(input->cx,input->cy); }
    repeat = g_maps_mode == 1 ? dunmap_repeats(input->cx,input->cy) : townmap_repeats(input->cx,input->cy);
    if (pressed && input->on_picture) {
        if (pressed == 1) {
            result = g_maps_mode == 1 ? dunmap_click(input->cx,input->cy) : townmap_click(input->cx,input->cy);
            g_map_drag = (g_maps_mode == 1 ? dunmap_on_map(input->cx,input->cy) : townmap_on_map(input->cx,input->cy)) ? 1 : 0;
        } else if (g_maps_mode == 1 && dunmap_on_map(input->cx,input->cy)) { g_map_drag = 2; }
        og_sound(203); g_map_repeat = 0;
    }
    if (!(input->mouse_held & 3) || !input->on_picture) { g_map_drag = 0; }
    if (g_map_drag && g_map_x >= 0 && !pressed) {
        dx = input->cx-g_map_x; dy = input->cy-g_map_y;
        if (g_maps_mode == 1) { if (g_map_drag == 2) { dunmap_turn_by(dx); } else { dunmap_drag(dx,dy); } }
        else { townmap_drag(dx,dy); }
    }
    if (repeat && (input->mouse_held & 1) && input->on_picture) {
        g_map_repeat += input->dt;
        while (g_map_repeat >= 1.0f/18.2065f) {
            g_map_repeat -= 1.0f/18.2065f;
            result = g_maps_mode == 1 ? dunmap_click(input->cx,input->cy) : townmap_click(input->cx,input->cy);
        }
    }
    g_map_x = input->cx; g_map_y = input->cy;
    return result != 0;
}
const AutomapView *seam_automap_view(void)
{
    static AutomapView view;
    int x, top, w, h;
    if (!og_maps_up() || g_maps_mode != 1) { return 0; }
    view = *dunmap_view(); og_screen_rect(df_view_w(),df_view_h(),&x,&top,&w,&h);
    view.x0 = x+view.x0*w/df_view_w(); view.x1 = x+view.x1*w/df_view_w();
    view.y0 = top-h+view.y0*h/df_view_h(); view.y1 = top-h+view.y1*h/df_view_h();
    view.scale *= (float)w/df_view_w(); return &view;
}
static void map_overlay(void)
{
    const AutomapView *view;
    float sx, sy, angle, ax, ay;
    int x, top, w, h, step, visible;
    df_u8 pixels[MARKER_SIDE*MARKER_SIDE];
    visible = 0; angle = 0;
    if (g_maps_mode == 1) {
        view = seam_automap_view();
        visible = automap_to_screen(view,og_move_body()->feet[0],og_move_body()->feet[1],og_move_body()->feet[2],&sx,&sy);
        if (visible && automap_to_screen(view,og_move_body()->feet[0]+df_sin(og_move_yaw()*DF_TAU/2048)*64,
             og_move_body()->feet[1],og_move_body()->feet[2]+df_cos(og_move_yaw()*DF_TAU/2048)*64,&ax,&ay)) { angle = df_atan2(ax-sx,ay-sy); }
    } else if (g_maps_mode == 2) {
        visible = townmap_marker(&sx,&sy,&angle);
        og_screen_rect(df_view_w(),df_view_h(),&x,&top,&w,&h);
        sx = x+sx*w/df_view_w(); sy = top-h+sy*h/df_view_h();
    }
    if (!visible) { return; }
    step = marker_step(angle);
    if (!g_map_marker || step != g_map_marker_step) {
        marker_compose(pixels,step);
        if (!g_map_marker) { g_map_marker = pic_load(pixels,MARKER_SIDE,MARKER_SIDE,g_art_rgb,1); }
        else { pic_update(g_map_marker,pixels); }
        g_map_marker_step = step;
    }
    w = df_view_w()*8/320; pic_draw(g_map_marker,df_iround(sx)-w/2,df_iround(sy)+w/2,w,w);
}
static void install_maps(void)
{
    OgMapsHooks hooks;
    hooks.open = open_maps; hooks.close = close_maps; hooks.frame = frame_maps; hooks.overlay = map_overlay;
    og_maps_install(&hooks);
}
static void present_maps(void)
{
    const df_u8 *pixels;
    int changed;
    if (g_maps_mode == 1) { changed = dunmap_changed(); if (changed || !g_map_pic) { dunmap_compose(); } pixels = dunmap_pixels(); }
    else if (g_maps_mode == 2) { changed = townmap_changed(); if (changed || !g_map_pic) { townmap_compose(); } pixels = townmap_pixels(); }
    else { changed = travelmap_changed(); if (changed || !g_map_pic) { travelmap_compose(); } pixels = travelmap_pixels(); }
    if (!changed && g_map_pic) { return; }
    if (g_maps_mode == 3) { travelmap_palette(g_maps_rgb); } else { mem_copy(g_maps_rgb,g_art_rgb,768); }
    if (!g_map_pic) { g_map_pic = pic_load(pixels,320,200,g_maps_rgb,g_maps_mode == 1); }
    else { pic_update(g_map_pic,pixels); }
    og_pic_set(OG_PIC_SCREEN,g_map_pic);
    if (!g_buf) { g_buf = (df_u8 *)mem_keep(64000); }
    if (g_buf) { mem_copy(g_buf,pixels,64000); } g_screen_kind = 1;
}

void seam_hall_enter(int person_faction, int hall_faction)
{
    hall_enter(person_faction, hall_faction);
}
