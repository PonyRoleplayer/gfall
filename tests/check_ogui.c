#include "check_ogui.h"
#include "check_util.h"
#include "engine/core/mem.h"
#include "engine/assets/level.h"
#include "engine/platform/fs.h"
#include "engine/platform/input.h"
#include "engine/scene/interact.h"
#include "engine/scene/collide.h"
#include "engine/scene/walker.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/ui/pic.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/formats/texture.h"
#include "game/dagger/play/chargen.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/controls.h"
#include "game/dagger/play/files.h"
#include "game/dagger/play/move.h"
#include "game/dagger/play/options.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/kit.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/describe.h"
#include "game/dagger/play/activate.h"
#include "game/dagger/play/hud.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/weapon.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/dungeon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int seam_open_hold(float *x, float *y, float *z);
void seam_close_hold(void);
void seam_present(void);
void seam_creatures(const Camera *camera);
Level *seam_level(void);
const df_u8 *seam_screen(const df_u8 **palette);
void seam_title(void);
static OgState g_before, g_saved;
static Camera g_camera;
void check_ogfall_draw_reset(void);
int check_ogfall_draw_count(void);
static int g_last_quads;

static int frame(OgIn *input)
{
    return og_frame(input, &g_camera, &seam_level()->scene);
}

static int key(int ch, int vk)
{
    OgIn input;
    mem_zero(&input, sizeof(input));
    input.ch = ch;
    input.key = vk;
    return frame(&input);
}

static void click(int x, int y)
{
    OgIn input;
    mem_zero(&input, sizeof(input));
    input.cx = x; input.cy = y; input.on_picture = 1;
    input.mouse_held = input.mouse_pressed = 1;
    frame(&input);
    input.mouse_held = input.mouse_pressed = 0;
    frame(&input);
}

static void screenshot(const char *name)
{
    const df_u8 *pixels, *palette;
    char path[96];
    FILE *file;
    int i, pictures, tiles;
    df_u32 used, room;

    seam_present();
    check_ogfall_draw_reset();
    og_draw();
    g_last_quads = check_ogfall_draw_count();
    pic_stats(&pictures, &tiles, &used, &room);
    check_expect(used <= room && used > 0 && tiles > 0, "modal pictures fit the Voodoo picture budget");
    pixels = seam_screen(&palette);
    snprintf(path, sizeof(path), "build/ogfall-%s.ppm", name);
    file = fopen(path, "wb");
    check_expect(file && pixels && palette, "modal screenshot is available");
    if (!file || !pixels || !palette) { if (file) { fclose(file); } return; }
    fprintf(file, "P6\n320 200\n255\n");
    for (i = 0; i < 64000; i++) { fwrite(palette + pixels[i] * 3, 1, 3, file); }
    fclose(file);
}

static void check_key_bank(void)
{
    InputKeys bank;
    mem_zero(&bank, sizeof(bank));
    bank.count = 40;
    input_keys_fold(&bank, 37, 1, 1, 0);
    check_expect(bank.pressed[37] && bank.released[37] && !bank.held[37],
                 "a finished tap is retained beyond the engine's 32-bit action mask");
    input_keys_fold(&bank, 37, 0, 0, 1);
    input_keys_fold(&bank, 37, 1, 0, 1);
    check_expect(bank.held[37] && !bank.pressed[37], "a repeated key-down does not repeat a custom action press");
    input_keys_release(&bank);
    check_expect(!bank.held[37] && bank.released[37], "focus suppression releases a game-owned action");
    input_keys_release(&bank);
    check_expect(!bank.released[37], "focus suppression releases it only once");
}

static void check_sound_ids(void)
{
    /* Positions independently checked in DAGGER.SND's numbered directory. */
    static const int ids[4] = {203,205,237,224};
    static const int positions[4] = {360,362,384,378};
    df_i16 expected[2048], actual[2048];
    int i, volume, on;
    volume = sound_volume(); on = sound_on();
    sound_set_on(1); sound_set_volume(100); sound_set_mix_rate(11025);
    for (i = 0; i < 4; i++) {
        sound_stop_all();
        check_expect(sound_play(sndfile_clip(positions[i]),1.0f,0.0f,0) != 0,
                     "the original UI sound sample can be mixed");
        memset(expected,0,sizeof(expected));
        sound_mix(expected,1024);
        sound_stop_all();
        og_sound(ids[i]);
        memset(actual,0,sizeof(actual));
        sound_mix(actual,1024);
        check_expect(!memcmp(expected,actual,sizeof(expected)),
                     "OGFALL resolves original sound IDs before mixing the actual sample");
    }
    sound_stop_all(); og_sound(999999);
    check_expect(!sound_busy(), "an unknown original sound ID stays silent");
    sound_set_volume(volume); sound_set_on(on);
}

static void weapon_mask(df_u8 *mask, int file, int record, int slot)
{
    const Image *image;
    int x, y, col, row, dx, dy, ink;
    image = texture_load(file,record);
    check_expect(image && texture_offset(file,record,&x,&y), "the original hand-specific weapon artwork loads");
    if (!image || !texture_offset(file,record,&x,&y)) { return; }
    for (row = 0; row < image->height; row++) {
        for (col = 0; col < image->width; col++) {
            dx = x+col-200; dy = y+row-8;
            ink = image->index[row*image->width+col];
            if (ink && dx >= 0 && dx < 110 && dy >= 0 && dy < 184) {
                mask[dy*110+dx] = ink == 255 ? 255 : slot;
            }
        }
    }
}

static void check_weapon_hands(void)
{
    static OgState saved, unrestricted;
    static df_u8 expected[110*184];
    df_u32 right, left, third, claymore, shield;
    int i, female, count[2], images[2];
    saved = *og_state();
    unrestricted = saved;
    unrestricted.career.forbidden_equipment = unrestricted.career.forbidden_materials = 0;
    check_expect(og_state_restore(&unrestricted), "prepare unrestricted hand equipment for the rendering audit");
    for (i = 0; i < 27; i++) { og_state_equip(i,0); }
    right = og_item_create(og_state()->inventory_id,3,7,1);
    left = og_item_create(og_state()->inventory_id,3,7,0);
    third = og_item_create(og_state()->inventory_id,3,7,2);
    claymore = og_item_create(og_state()->inventory_id,3,9,1);
    shield = og_item_create(og_state()->inventory_id,2,7,1);
    check_expect(og_inventory_equip(right) && og_item_equipped(right) == 19 &&
                 og_inventory_equip(left) && og_item_equipped(left) == 21,
                 "two one-handed swords occupy right and left hands separately");
    images[0] = og_object(right)->inventory_image; images[1] = og_object(left)->inventory_image;
    for (female = 0; female < 2; female++) {
        og_character()->female = female;
        og_possessions_open(OG_VIEW_INVENTORY,0);
        seam_present();
        memset(expected,255,sizeof(expected));
        /* Longsword: male TEXTURE.234, female .233; right record 13, left 12.
         * The left-hand layer follows the right, including its pick mask. */
        weapon_mask(expected,female ? 233 : 234,13,19);
        weapon_mask(expected,female ? 233 : 234,12,21);
        check_expect(!memcmp(expected,og_doll_owners(),sizeof(expected)),
                     "both swords draw and pick through their original gender/hand-specific artwork");
        count[0] = count[1] = 0;
        for (i = 0; i < (int)sizeof(expected); i++) {
            if (og_doll_owners()[i] == 19) { count[0]++; }
            if (og_doll_owners()[i] == 21) { count[1]++; }
        }
        check_expect(count[0] > 0 && count[1] > 0, "each equipped sword is visible and independently clickable");
        screenshot(female ? "dual-swords-female" : "dual-swords-male");
        check_expect(og_object(right)->inventory_image == images[0] && og_object(left)->inventory_image == images[1],
                     "paper-doll hand variants do not mutate saved inventory pictures");
        og_possessions_close();
    }
    check_expect(og_inventory_equip(third) && og_item_equipped(third) == 19 && og_item_equipped(left) == 21 &&
                 og_item_equipped(right) < 0, "a third one-handed weapon replaces the right hand");
    check_expect(og_inventory_equip(claymore) && og_item_equipped(claymore) == 19 && !og_state()->equipped[21],
                 "a two-handed weapon clears the left hand");
    check_expect(og_inventory_equip(shield) && og_item_equipped(shield) == 21 && !og_state()->equipped[19],
                 "a shield removes a conflicting two-handed weapon");
    check_expect(og_state_restore(&saved), "restore the surrounding session after the hand audit");
    og_possessions_reset();
}

static void check_preferences(void)
{
    OgConfig config, parsed;
    char text[4096];
    int size, vk;
    FILE *example;

    config = *og_config();
    config.key[OG_FORWARD] = 'K';
    config.flags = 3;
    config.sound_volume = 37;
    config.music_volume = 64;
    config.mouse_mode = 1;
    config.sensitivity_x = 4;
    config.sensitivity_y = 7;
    config.turn_rate = 3;
    config.reverse_y = 1;
    config.width = 512; config.height = 384; config.fov = 90;
    size = og_config_format(&config, text, sizeof(text));
    check_expect(size && og_config_parse(&parsed, text) && !memcmp(&parsed, &config, sizeof(config)),
                 "every preference and all 38 keys round-trip through text");
    check_expect(strstr(text, "full_screen=on\r\nhead_bob=on") &&
                 strstr(text, "resolution=512x384") && strstr(text, "mouse=view") &&
                 strstr(text, "key.forward=K") && strstr(text, "key.slide_left=Left Shift") &&
                 strstr(text, "key.swing=Right Button") && !strstr(text, "view_flags="),
                 "written preferences use readable switches, resolution, scheme and key names");
    check_expect(og_config_parse(&parsed,
                 "# Hand edited settings\r\nfull_screen=YES\nhead_bob=off\nmouse=View\n"
                 "resolution=400x300\nreverse_y=on\nkey.forward=up\nkey.swing==\n"
                 "key.jump=Digit 2\nkey.slide_right=Right Shift\nkey.cast=;\n") &&
                 parsed.flags == 1 && parsed.width == 400 && parsed.height == 300 &&
                 parsed.key[OG_FORWARD] == 38 && parsed.key[OG_SWING_BUTTON] == 0xbb &&
                 parsed.key[OG_JUMP] == '2' && parsed.key[OG_CAST] == 0xba,
                 "hand edits accept names without losing punctuation or digit bindings");
    check_expect(og_config_parse(&parsed, "view_flags=3\nmouse_mode=1\nkey.forward=75\nkey.swing=2\n") &&
                 parsed.flags == 3 && parsed.mouse_mode == 1 && parsed.key[OG_FORWARD] == 'K' &&
                 parsed.key[OG_SWING_BUTTON] == 2, "existing numeric settings preserve user preferences");
    check_expect(!og_config_parse(&parsed, "head_bob=maybe\n") &&
                 !og_config_parse(&parsed, "mouse=flight\n") &&
                 !og_config_parse(&parsed, "key.forward=Banana\n"), "unknown symbolic values are refused");
    check_expect(!og_config_parse(&parsed, "key.forward=27\n"), "Escape cannot replace a gameplay binding");
    check_expect(!og_config_parse(&parsed, "sound_volume=101\n"), "invalid volume is refused");
    check_expect(!og_config_parse(&parsed, "width=799\n"), "unsupported resolution is refused");
    check_expect(!og_config_parse(&parsed, "key.misspelled=80\n"), "unknown settings are reported");
    check_expect(!og_config_format(&config, text, 12), "small config output buffer is refused");
    for (vk = 1; vk <= 254; vk++) {
        if (!og_key_allowed(vk)) { continue; }
        config.key[OG_FORWARD] = vk;
        check_expect(og_config_format(&config,text,sizeof(text)) && og_config_parse(&parsed,text) &&
                     parsed.key[OG_FORWARD] == vk, "every permitted key has an unambiguous readable round trip");
    }
    config.key[OG_FORWARD] = 'K';
    og_config_format(&config,text,sizeof(text));
    example = fopen("build/ogfall-settings.cfg","wb");
    if (example) { fwrite(text,1,strlen(text),example); fclose(example); }
    og_config_set(&config);
    check_expect(og_config_write() && fs_exists("ogfall\\settings.cfg"), "preferences use ogfall/settings.cfg");
    og_config_defaults(&config);
    og_config_set(&config);
    check_expect(og_config_read() && og_config()->key[OG_FORWARD] == 'K' &&
                 sound_volume() == 37 && music_volume() == 64 && og_options_flags() == 3,
                 "disk settings apply to live audio, HUD and key bindings");
    check_expect(fs_move("ogfall\\settings.cfg", "ogfall\\settings.cfg.OLD"), "stage interrupted config replacement");
    check_expect(og_config_read() && og_config()->sensitivity_y == 7, "an interrupted config replacement recovers .OLD");
    fs_delete("ogfall\\settings.cfg.OLD");
    og_config_defaults(&config);
    og_config_set(&config);
}

static void check_options_and_controls(void)
{
    OgIn input;
    OgConfig config;

    key(27, 0x1b);
    check_expect(og_options_up() && og_paused(), "Escape opens the actual Options controller");
    click(145, 65);
    check_expect(sound_volume() > 45 && sound_volume() < 60, "Sound slider changes the engine mixer volume");
    mem_zero(&input, sizeof(input));
    input.cx = 250; input.mouse_held = 1;
    /* Begin a new capture, then drag outside its original narrow rectangle. */
    click(95, 73);
    input.cx = 250; input.cy = 73; input.on_picture = 1;
    input.mouse_pressed = 1;
    input.cx = 100;
    frame(&input);
    input.mouse_pressed = 0; input.cx = 250;
    frame(&input);
    check_expect(music_volume() == 100, "Music slider captures a held drag and clamps at full volume");
    input.mouse_held = 0; frame(&input);
    click(120, 90);
    click(180, 90);
    check_expect(og_options_flags() == 3, "fullscreen and head bobbing flags toggle independently");
    screenshot("options");
    click(120, 105);
    check_expect(og_options_panel() == OG_PANEL_CONTROLS, "Controls opens the real binding screen");
    screenshot("controls");
    click(80, 15);
    check_expect(og_controls_editing() == OG_FORWARD, "the original Forward cell captures a key");
    key(0, 'K');
    check_expect(og_config()->key[OG_FORWARD] == 'K' && og_controls_editing() < 0, "captured key replaces Forward");
    click(80, 26); key(0, 'K');
    check_expect(og_controls_duplicate(OG_FORWARD) && og_controls_duplicate(OG_BACK), "duplicate bindings are highlighted");
    click(280, 194);
    check_expect(og_options_panel() == OG_PANEL_CONTROLS && og_controls_notice()[0], "duplicate bindings cannot be silently committed");
    click(160, 95); /* Dismiss the notice. */
    click(80, 26); key(0, 0x28);
    click(80, 15); key(27, 0x1b);
    check_expect(og_config()->key[OG_FORWARD] == 'K' && og_controls_editing() < 0, "Escape cancels a rebind without changing its key");
    click(120, 194);
    check_expect(og_controls_mouse(), "Mouse opens Cursor/View and sensitivity controls");
    click(200, 60);
    click(175, 110);
    click(73, 152);
    check_expect(og_options_mouse() == 1 && og_config()->sensitivity_x > 1 && og_config()->reverse_y,
                 "mouse scheme, sensitivity and reverse controls change their real settings");
    screenshot("mouse");
    click(200, 152);
    click(200, 194);
    check_expect(og_config()->key[OG_FORWARD] == 0x26, "Defaults restores the original key map");
    click(80, 15); key(0, 'K');
    click(280, 194);
    check_expect(og_options_panel() == OG_PANEL_OPTIONS && fs_exists(OG_CONFIG_FILE), "Continue writes bindings and returns to Options");
    mem_zero(&input, sizeof(input));
    input.cx = 190; input.cy = 105; input.on_picture = 1;
    input.mouse_held = input.mouse_pressed = 1;
    frame(&input);
    g_saved = *og_state();
    input.mouse_pressed = 0; input.cx = 315; input.cy = 5; input.dt = 0.1f;
    frame(&input);
    check_expect(!memcmp(&g_saved, og_state(), sizeof(g_saved)), "holding Continue's click cannot leak into world steering");
    input.mouse_held = 0; frame(&input);
    check_expect(!og_options_up(), "Continue resumes play");
    config = *og_config();
    check_expect(og_config_read() && og_config()->key[OG_FORWARD] == 'K' &&
                 og_config()->reverse_y && og_config()->flags == config.flags,
                 "binding and visual changes persist after reloading settings.cfg");
    og_config_defaults(&config);
    og_config_set(&config);
}

static void check_pointer_shapes(void)
{
    OgIn input;
    int i;

    mem_zero(&input, sizeof(input));
    input.on_picture = 1;
    for (i = 0; i < 9; i++) {
        input.cx = 53 + (i % 3) * 106;
        input.cy = 25 + (i / 3) * 51;
        frame(&input);
        check_expect(og_pointer_region() == i && og_pic(OG_PIC_CURSOR0 + i),
                     "all nine PNTER.CIF cursors preview the pointed movement region");
    }
    input.cy = 170;
    frame(&input);
    check_expect(og_pointer_region() == -1, "the HUD uses the ordinary arrow");
    og_options_set_mouse(1);
    frame(&input);
    check_expect(og_pointer_region() == -1, "View-mode UI uses the ordinary arrow");
    og_options_set_mouse(0);
}

static void check_save_load(const char *root, const char *test_root)
{
    OgIn input;
    Scene *scene;
    KeepKey door_key;
    OgSaveInfo info;
    float x, y, z;
    int i, status, empty_quads;
    pid_t child;
    static df_u16 preview[OG_THUMB_PIXELS];

    scene = &seam_level()->scene;
    for (i = 0; i < scene->nmovers; i++) {
        if (!scene->movers[i].teleport && (scene->movers[i].key.place || scene->movers[i].key.thing)) { break; }
    }
    check_expect(i < scene->nmovers, "the Hold supplies a keyed door for the save test");
    if (i == scene->nmovers) { return; }
    door_key = scene->movers[i].key;
    mover_set_end(&scene->movers[i], 1);
    og_character()->gold = 987;
    for (i = 0; i < OG_THUMB_PIXELS; i++) { preview[i] = (df_u16)(i * 13); }
    og_state_preview(preview);
    key(27, 0x1b);
    click(110, 50);
    check_expect(og_options_panel() == OG_PANEL_FILES && og_files_saving(), "Save opens the actual six-slot browser");
    screenshot("save");
    empty_quads = g_last_quads;
    key(13, 0x0d);
    check_expect(og_files_editing(), "confirming a save opens its name prompt");
    for (i = 0; i < 32; i++) { key(8, 8); }
    {
        const char *name = "Host round trip";
        while (*name) { key(*name, *name); name++; }
    }
    screenshot("save-name");
    key(13, 0x0d);
    check_expect(!og_options_up() && fs_exists("ogfall\\saves\\S0001.SAV"), "saving returns to play and creates the required file");
    check_expect(og_save_info(1, &info) == 1 && info.preview_valid &&
                 !strcmp(info.label, "Host round trip"), "save browser metadata and preview are stored with the session");
    g_saved = *og_state();
    for (i = 2; i <= 6; i++) { check_expect(og_save_write(i), "populate all six save previews"); }
    og_character()->gold = 1;
    og_move_place(100, 200, 300, 1000);
    key(27, 0x1b);
    click(160, 50);
    check_expect(!og_files_saving(), "Load opens the same browser in load mode");
    screenshot("load");
    check_expect(g_last_quads >= empty_quads + 6, "all six RGB previews actually draw within the card budget");
    check_expect(key(13, 0x0d) == OG_FRAME_LOAD, "loading requests reconstruction of the saved place");
    check_expect(og_character()->gold == 987 && !og_paused() && og_state()->depth == 0,
                 "loaded games resume the world rather than reopening a stale modal");
    fs_set_root(root);
    check_expect(seam_open_hold(&x, &y, &z), "a saved Hold rebuilds after its previous heap closes");
    og_move_restore(&og_state()->motion);
    check_expect(check_near(og_move_body()->feet[0], g_saved.motion.feet[0], 0.001f) &&
                 check_near(og_move_body()->feet[2], g_saved.motion.feet[2], 0.001f) &&
                 og_move_yaw() == g_saved.motion.yaw, "the saved player body and heading return");
    scene = &seam_level()->scene;
    for (i = 0; i < scene->nmovers; i++) {
        if (scene->movers[i].key.place == door_key.place && scene->movers[i].key.thing == door_key.thing) { break; }
    }
    check_expect(i < scene->nmovers && mover_at_end(&scene->movers[i]), "an opened door remains open after a place rebuild");
    fs_set_root(test_root);
    child = fork();
    if (child == 0) {
        execl("/proc/self/exe", "ogfall-test", "--ogfall-resume", test_root, (char *)0);
        _exit(127);
    }
    check_expect(child > 0, "start a fresh process for title-menu loading");
    if (child > 0) {
        waitpid(child, &status, 0);
        check_expect(WIFEXITED(status) && WEXITSTATUS(status) == 0, "a fresh process loads the saved game through the title screen");
    }
    /* A controller reset cannot finish a stale slider or move the player. */
    mem_zero(&input, sizeof(input));
    input.reset = 1;
    input.dt = 0.1f;
    input.held[OG_FORWARD] = 1;
    g_saved = *og_state();
    frame(&input);
    check_expect(!memcmp(&g_saved, og_state(), sizeof(g_saved)), "focus reset does not move or advance the session");
}

static void check_inventory_completion(const char *test_root)
{
    static OgState saved, fixture, disk;
    df_u32 stash, loot, book, sword, arrows, incoming, spell, rows[OG_OBJECTS];
    const OgObject *item;
    char text[2048], title[65], author[65];
    OgIn in;
    int i, n, icon, old_gold, status;
    pid_t child;

    saved = fixture = *og_state();
    fixture.career.forbidden_equipment = fixture.career.forbidden_materials = 0;
    fixture.who.attribute[OG_STR] = 100;
    check_expect(og_state_restore(&fixture), "prepare a strong unrestricted inventory test character");
    stash = og_loot_container(og_object(OG_WORLD_ROOT)->place_key,0x7110,0,0,0,-1);
    loot = og_loot_container(og_object(OG_WORLD_ROOT)->place_key,0x7111,0,0,0,-1);
    n = og_inventory_rows(og_state()->inventory_id,-1,rows,OG_OBJECTS);
    for (i = 0; i < n; i++) { check_expect(og_inventory_transfer(rows[i],stash), "isolate the carried pack for original inventory scenarios"); }
    sword = og_item_create(loot,3,0,1);
    og_possessions_open(OG_VIEW_INVENTORY,loot);
    click(290,60);
    check_expect(og_object(sword)->parent == og_state()->inventory_id && og_item_equipped(sword) == 19,
                 "Equip on the right-hand loot column takes and equips the selected weapon");
    check_expect(!og_inventory_list(og_state()->inventory_id,0,rows,OG_OBJECTS) &&
                 og_inventory_rows(og_state()->inventory_id,0,rows,OG_OBJECTS) == 1,
                 "worn weapons leave the pack list but retain ownership and weight");
    og_item_info(sword,text,sizeof(text));
    check_expect(strstr(text,"Steel Dagger") && strstr(text,"points of damage") && strstr(text,"Condition: New") &&
                 strstr(text,"Weight: 0.50 kilograms") && !strchr(text,'%'),
                 "weapon information expands the original material, damage, condition and quarter-kg macros");
    og_object_edit(sword)->condition = og_object(sword)->max_condition*40/100;
    og_item_info(sword,text,sizeof(text));
    check_expect(strstr(text,"Condition: Worn") != 0, "condition wording follows the original nonuniform thresholds");
    og_possessions_close();
    og_inventory_unequip(sword);
    check_expect(og_inventory_list(og_state()->inventory_id,0,rows,OG_OBJECTS) == 1,
                 "unequipping returns the same item to the pack list");

    incoming = og_item_create(loot,13,0,0);
    /* Seed 2 picks painting 8 and prefix 6402, whose text contains %fn2. */
    og_object_edit(incoming)->message = 2;
    og_possessions_open(OG_VIEW_INVENTORY,loot);
    click(200,4); click(240,42); click(290,60);
    check_expect(og_possessions()->painting == 9 && og_possessions()->notice[0] && !strchr(og_possessions()->notice,'%'),
                 "painting Info resolves its saved seed, PAINT.DAT caption and generated artist");
    screenshot("painting-info");
    {
        df_u32 seed;
        int picture;
        char again[2048];
        seed = og_state()->random; picture = og_possessions()->painting;
        check_expect(og_painting_info(incoming,again,sizeof(again)) == picture &&
                     !strcmp(again,og_possessions()->notice) && og_state()->random == seed,
                     "reopening painting Info is stable and does not reseed the world simulation");
    }
    key(27,27); og_possessions_close(); og_object_delete(incoming);

    arrows = og_item_create(og_state()->inventory_id,3,18,0); og_object_edit(arrows)->count = 24;
    incoming = og_item_create(loot,3,18,0); og_object_edit(incoming)->count = 15;
    check_expect(og_inventory_transfer(incoming,og_state()->inventory_id) && !og_object(incoming) && og_object(arrows)->count == 39,
                 "taken arrows merge into the existing carried stack and consume the source only once");
    og_object_edit(arrows)->count = 190;
    incoming = og_item_create(loot,3,18,0); og_object_edit(incoming)->count = 20;
    check_expect(og_inventory_transfer(incoming,og_state()->inventory_id) && og_object(arrows)->count == 199 && !og_object(incoming),
                 "the original arrow merge caps the carried stack at 199, including its overflow behavior");
    {
        df_u32 full[OG_OBJECTS], record;
        int filled;
        incoming = og_item_create(loot,3,18,0); og_object_edit(incoming)->count = 1;
        for (filled = 0; filled < OG_OBJECTS; filled++) {
            record = og_object_new(OG_RECORD_CONTAINER,OG_WORLD_ROOT,0,1);
            if (!record) { break; }
            full[filled] = record;
        }
        check_expect(og_inventory_transfer(incoming,og_state()->inventory_id) && !og_object(incoming) && og_object(arrows)->count == 199,
                     "merging an existing arrow stack succeeds even with every object slot occupied");
        for (i = 0; i < filled; i++) { og_object_delete(full[i]); }
    }
    og_item_info(arrows,text,sizeof(text));
    check_expect(strstr(text,"Arrow") && !strstr(text,"Condition:") && strstr(text,"49.75"),
                 "arrow information uses its own record and the whole stack's weight");
    og_inventory_transfer(arrows,loot);
    check_expect(og_inventory_transfer(arrows,og_state()->inventory_id) && og_object(arrows)->count == 199,
                 "a whole arrow stack can be dropped and recovered without changing its count");

    book = og_item_create(og_state()->inventory_id,27,0,0);
    spell = og_spell_create(book,5);
    for (i = 1; i <= 24; i++) { if (i != 5) { og_spell_create(book,i); } }
    og_possessions_open(OG_VIEW_SPELLS,0);
    n = og_spell_rows(book,rows,OG_OBJECTS);
    check_expect(n > 14, "the scrollbar scenario has more than one page of real spell definitions");
    click(190,22);
    check_expect(og_possessions()->prompt == OG_PROMPT_SPELL_NAME, "the original name strip edits the owned spell");
    for (i = 0; i < 24; i++) { key(8,8); }
    for (i = 0; "My Lantern"[i]; i++) { key("My Lantern"[i],0); }
    key(13,13);
    check_expect(!strcmp(og_object(spell)->name,"My Lantern") && !strcmp(og_spell_def(5)->name,"Light"),
                 "renaming changes one owned spell without changing its standard definition");
    icon = og_object(spell)->spell_icon;
    click(180,40);
    check_expect(og_object(spell)->spell_icon == (icon+1)%69, "left-click cycles the owned icon forward");
    mem_zero(&in,sizeof(in)); in.on_picture = 1; in.cx = 180; in.cy = 40; in.mouse_pressed = in.mouse_held = 2;
    frame(&in); in.mouse_held = in.mouse_pressed = 0; frame(&in);
    check_expect(og_object(spell)->spell_icon == icon, "right-click cycles the owned icon backward");
    click(220,65);
    check_expect(strstr(og_possessions()->notice,"Light") && strstr(og_possessions()->notice,"1 + 4 per 1") &&
                 !strchr(og_possessions()->notice,'%'), "effect help expands the actual spell parameters in TEXT.RSC");
    screenshot("spell-effect-help"); key(27,27);
    key(0,OG_VK_NEXT);
    check_expect(og_possessions()->scroll[0] == n-14, "Page Down reaches the end of a long owned-spell list");
    click(147,32);
    check_expect(og_possessions()->scroll[0] == n-15, "the spellbook up arrow moves one row");
    key(0,OG_VK_PRIOR);
    check_expect(!og_possessions()->scroll[0], "Page Up returns to the beginning");
    screenshot("spellbook-edited");
    click(30,48); /* Select a different row, then move it down. */
    i = og_possessions()->selected;
    incoming = rows[i];
    click(170,175);
    og_spell_rows(book,rows,OG_OBJECTS);
    check_expect(rows[i+1] == incoming && !og_possessions()->selected && !og_possessions()->scroll[0],
                 "Down swaps neighbouring owned spells and rebuilds the list from the first row like the original");
    click(30,48); incoming = rows[2];
    click(40,175);
    check_expect(!og_object(incoming) && !og_possessions()->prompt && og_spell_rows(book,rows,OG_OBJECTS) == n-1,
                 "the original Delete button removes the selected spell immediately and rebuilds the list");
    og_spell_create(book,50); /* Paralysis has a long original help record. */
    i = og_spell_rows(book,rows,OG_OBJECTS);
    key(0,OG_VK_NEXT);
    click(30,32+(i-1-og_possessions()->scroll[0])*9);
    click(220,65);
    check_expect(og_notice_lines(og_possessions()->notice,0,0,0) <= OG_NOTICE_LINES &&
                 strstr(og_possessions()->notice,"unless target is cured") != 0,
                 "the complete original Paralysis description fits the expanded parchment, including its last line");
    key(0,OG_VK_NEXT);
    check_expect(!og_possessions()->notice_page, "Page Down cannot leave the last message page");
    screenshot("spell-effect-help-long");
    key(0,OG_VK_PRIOR);
    check_expect(!og_possessions()->notice_page, "Page Up cannot leave the first help page");
    key(27,27);
    og_possessions_close();

    incoming = og_item_create(loot,7,0,0);
    item = og_object(incoming);
    check_expect(og_item_tab(item) == 2, "books belong to Clothing & Misc by the original group table");
    og_book_header(item->message,title,sizeof(title),author,sizeof(author));
    og_item_info(incoming,text,sizeof(text));
    check_expect(strstr(text,title) && strstr(text,author) && !strchr(text,'%'), "book Info displays the full title and author from its header");
    og_possessions_open(OG_VIEW_INVENTORY,loot);
    click(200,4); click(240,110); click(290,60);
    check_expect(og_possessions()->page == OG_VIEW_BOOK && og_object(incoming)->parent == loot,
                 "Use can read a book in the searched container without taking it");
    og_possessions_close(); og_possessions_close();
    for (i = 0; i < 32; i++) {
        incoming = og_item_create(loot,7,0,0);
        item = og_object(incoming);
        for (n = 0; n < og_book_count() && og_book_id(n) != item->message; n++) { }
        check_expect(n < og_book_count() && og_book_rarity(n) <= 0, "a common book uses its original rarity tier, independent of player level");
        og_object_delete(incoming);
    }
    old_gold = og_character()->gold;
    incoming = og_item_create(loot,28,0,0); og_object_edit(incoming)->value = 100;
    check_expect(og_inventory_transfer(incoming,og_state()->inventory_id) && og_character()->gold == old_gold+100,
                 "gold still credits exactly once through the transfer path");
    disk = *og_state();
    check_expect(og_save_write(4) && og_save_read(4) && !memcmp(&disk,og_state(),sizeof(disk)) &&
                 !strcmp(og_object(spell)->name,"My Lantern") && og_object(spell)->spell_icon == icon,
                 "owned spell names, icons and merged stacks survive an exact disk round trip");
    child = fork();
    if (child == 0) {
        execl("/proc/self/exe","ogfall-test","--ogfall-resume-inventory",test_root,(char *)0);
        _exit(127);
    }
    if (child > 0) {
        waitpid(child,&status,0);
        check_expect(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                     "a fresh process restores edited spell instances and merged arrows through the real Load screen");
    } else { check_expect(0,"start an independent inventory-completion resume process"); }
    check_expect(og_state_restore(&saved), "restore the surrounding playtest after inventory completion checks");
    og_possessions_reset();
}

static void check_visible_pile_click(void)
{
    const Scene *scene;
    const Usable *pile;
    OgMotion saved, aimed;
    float saved_x, saved_y, saved_z;
    int saved_yaw, saved_pitch;
    OgIn in;
    Basis basis;
    UseHit hit;
    float angle, x, z, floor, dx, dy, dz, length, vx, vy, vz, sx, sy, ow, oz;
    int i, side, picked, ready, mouse;
    scene = &seam_level()->scene;
    saved_x = og_state()->x; saved_y = og_state()->eye_y; saved_z = og_state()->z;
    saved_yaw = og_state()->yaw; saved_pitch = og_state()->pitch;
    og_move_capture(&saved); ready = og_hud_weapon_ready(); mouse = og_options_mouse();
    if (!ready) { og_hud_toggle_ready(); }
    og_hud_set_mode(OG_MODE_GRAB); og_options_set_mouse(0);
    picked = 0;
    for (i = 0; i < scene->nusables && !picked; i++) {
        pile = &scene->usables[i];
        if (pile->kind != USE_OTHER || pile->what < OG_USE_TREASURE || pile->what >= OG_USE_TREASURE+22) { continue; }
        for (side = 0; side < 8 && !picked; side++) {
            angle = side*DF_TAU/8; x = pile->cx+96*df_sin(angle); z = pile->cz+96*df_cos(angle);
            collide_use(scene); collide_gather(x-20,pile->cy-128,z-20,x+20,pile->cy+80,z+20);
            if (!collide_floor(x,z,pile->cy+32,pile->cy-128,&floor)) { continue; }
            og_move_init(); og_move_place(x,floor+WALK_EYE,z,0);
            body_step(og_move_body(),scene,0,0,0);
            og_move_capture(&aimed);
            dx = pile->cx-aimed.feet[0]; dy = pile->cy-(aimed.feet[1]+WALK_EYE); dz = pile->cz-aimed.feet[2];
            length = df_sqrt(dx*dx+dy*dy+dz*dz);
            if (length > 125 || length < 1) { continue; }
            aimed.yaw = df_iround(df_atan2(dx,dz)*2048/DF_TAU) & 2047;
            aimed.pitch = df_iround(df_atan2(dy,df_sqrt(dx*dx+dz*dz))*2048/DF_TAU);
            if (aimed.pitch < -256 || aimed.pitch > 256) { continue; }
            og_move_restore(&aimed); og_move_camera(&g_camera);
            if (!interact_aim_from(scene,g_camera.x,g_camera.y,g_camera.z,dx/length,dy/length,dz/length,&hit) || hit.usable != i) { continue; }
            camera_basis(&g_camera,&basis);
            camera_view(&g_camera,&basis,pile->cx,pile->cy,pile->cz,&vx,&vy,&vz);
            if (!camera_project(vx,vy,vz,&sx,&sy,&ow,&oz)) { continue; }
            mem_zero(&in,sizeof(in));
            in.on_picture = og_world_pointer(df_iround(sx),df_iround(sy),df_view_w(),df_view_h(),!(og_options_flags() & 1),&in.cx,&in.cy);
            in.cx -= 6; in.cy -= 6; in.mouse_pressed = in.mouse_held = 1;
            frame(&in);
            check_expect(og_possessions()->page == OG_VIEW_INVENTORY && og_possessions()->remote,
                         "left-clicking the X opens a visible real Hold pile from standing height with a weapon drawn");
            if (og_possessions()->page) { og_possessions_close(); }
            in.mouse_pressed = in.mouse_held = 0; in.mouse_released = 1; frame(&in);
            picked = 1;
        }
    }
    check_expect(picked,"the real-pile pickup test found a reachable pile from outside its hit box");
    og_move_restore(&saved); og_move_camera(&g_camera);
    og_state_pose(saved_x,saved_y,saved_z,saved_yaw,saved_pitch);
    if (!ready) { og_hud_toggle_ready(); }
    og_options_set_mouse(mouse);
}

static void check_possessions(const char *test_root)
{
    static OgState before, saved;
    static OgBook book_file;
    const OgObject *item;
    const Usable *treasure;
    df_u32 rows[OG_OBJECTS], spells[OG_OBJECTS], id, book, container, gold, pile;
    OgIn in;
    Camera camera;
    int i, n, count, old_gold, remaining, slot, status;
    pid_t child;
    char name[80];
    df_u32 seed, main_bytes, temp_bytes, main_after, temp_after;

    before = *og_state();
    {
        OgCharacter archer;
        int bows, arrows;
        archer = before.who;
        archer.klass = 13; archer.gifts = archer.arrows = 0;
        og_kit_give(&archer);
        *og_character() = archer;
        check_expect(og_state_begin(), "Archer starting records build from the original class kit");
        count = og_inventory_rows(og_state()->inventory_id,-1,rows,OG_OBJECTS);
        bows = arrows = 0;
        for (i = 0; i < count; i++) {
            item = og_object(rows[i]);
            if (item->group == 3 && item->index == 17) { bows += item->count; }
            if (item->group == 3 && item->index == 18) { arrows += item->count; }
        }
        check_expect(bows == 1 && arrows == 24 && !og_state()->equipped[19],
                     "Archer receives one long bow and 24 arrows, with weapons initially in the pack");
        check_expect(og_state_restore(&before), "restore the character after the Archer-kit check");
    }
    check_weapon_hands();
    sound_stop_all();
    mem_zero(&in,sizeof(in)); in.pressed[OG_INVENTORY] = 1;
    frame(&in);
    check_expect(og_possessions()->page == OG_VIEW_INVENTORY, "the inventory action opens the actual controller");
    check_expect(!sound_busy(), "opening inventory does not play the book-opening sound");
    seam_present(); og_draw(); screenshot("inventory");
    check_expect(og_paused() && og_wants_pointer() && og_state()->mode == OG_GAME_INVENTORY,
                 "inventory pauses simulation and owns the pointer");
    count = og_inventory_rows(og_state()->inventory_id,-1,rows,OG_OBJECTS);
    check_expect(count >= 3, "starting gifts are actual inventory records");
    check_expect(og_pic(OG_PIC_SCREEN) != 0, "the inventory fits on the card alongside the real HUD");
    mem_stats(&main_bytes,&temp_bytes);
    mem_zero(&in,sizeof(in)); in.dt = 0.1f;
    seed = og_state()->minutes;
    for (i = 0; i < 20; i++) { frame(&in); seam_present(); }
    mem_stats(&main_after,&temp_after);
    check_expect(main_bytes == main_after && temp_bytes == temp_after && seed == og_state()->minutes,
                 "idle inventory frames allocate nothing and advance no game time");
    for (i = 0; i < count; i++) {
        item = og_object(rows[i]);
        check_expect(item->max_condition > 0 && item->inventory_image > 0, "starting items have condition and real pictures");
    }
    click(20,4); /* Weapons tab, then Info. */
    click(240,42); click(190,60);
    check_expect(og_possessions()->notice[0], "Info inspects the clicked item");
    seam_present(); screenshot("item-info"); key(27,27);
    key(27,27);
    check_expect(!og_possessions()->page && !og_state()->depth, "Escape closes inventory and balances the mode stack");

    mem_zero(&in,sizeof(in)); in.pressed[OG_SHEET] = 1;
    sound_stop_all();
    frame(&in); seam_present(); og_draw(); screenshot("character-sheet");
    check_expect(!sound_busy(), "opening the ordinary character sheet is silent");
    check_expect(og_possessions()->page == OG_VIEW_SHEET, "the rebound sheet action opens the real sheet");
    click(50,110); seam_present(); screenshot("primary-skills");
    check_expect(og_possessions()->skill_group == 0, "Primary Skills uses the saved career");
    key(27,27); key(27,27);
    check_expect(!og_possessions()->page, "the skill list closes before its parent sheet");

    container = og_loot_container(og_object(OG_WORLD_ROOT)->place_key,0xabcdefu,12,20,30,-1);
    id = og_item_create(og_state()->inventory_id,3,0,1);
    check_expect(id && og_inventory_equip(id), "a steel dagger can be equipped");
    slot = og_item_equipped(id);
    check_expect(slot == 19 || slot == 21, "weapons use original hand slots");
    og_item_name(og_object(id),name,sizeof(name));
    check_expect(!strcmp(name,"Steel Dagger"), "item names include the original material");
    check_expect(og_inventory_transfer(id,container) && og_item_equipped(id) < 0 && og_object(id)->parent == container,
                 "dropping an equipped weapon clears its hand and preserves its ID");
    check_expect(og_inventory_transfer(id,og_state()->inventory_id), "the same item can be taken back");
    check_expect(og_inventory_equip(id) && og_item_equipped(id) >= 0, "a recovered weapon can be re-equipped");
    old_gold = og_character()->gold;
    gold = og_item_create(container,28,0,0); og_object_edit(gold)->value = 37;
    check_expect(og_inventory_transfer(gold,og_state()->inventory_id) && og_character()->gold == old_gold+37 && !og_object(gold),
                 "taking coins credits gold once and removes the item");
    id = og_item_create(container,2,10,0);
    og_object_edit(id)->weight = og_inventory_capacity()*4+1;
    check_expect(!og_inventory_transfer(id,og_state()->inventory_id) && og_object(id)->parent == container,
                 "an overweight transfer leaves ownership intact");
    og_object_delete(id);

    book = og_inventory_book();
    if (!book) { book = og_item_create(og_state()->inventory_id,27,0,0); }
    n = og_spell_rows(book,spells,OG_OBJECTS);
    if (!n) {
        id = og_spell_create(book,8);
        check_expect(id && og_spell_def(8), "the known test spell is an original SPELLS.STD record");
    }
    check_expect(og_possessions_open(OG_VIEW_INVENTORY,0) && og_possessions_use(book),
                 "Use on the spellbook item opens its owned spells");
    seam_present(); og_draw(); screenshot("spellbook");
    check_expect(og_possessions()->page == OG_VIEW_SPELLS && og_state()->depth == 2,
                 "the spellbook is a nested screen, not a detached spell array");
    {
        const df_u8 *pixels, *palette;
        pixels = seam_screen(&palette);
        check_expect(og_possessions()->underlay == OG_VIEW_INVENTORY && pixels[5*320+5] != 0,
                     "the spellbook retains the inventory behind its overlay");
    }
    key(27,27);
    check_expect(og_possessions()->page == OG_VIEW_INVENTORY, "closing the spellbook returns to inventory");
    key(27,27);
    check_expect(og_inventory_transfer(book,container) && !og_inventory_book(), "the spellbook can be stored with its spells");
    n = og_spell_rows(book,spells,OG_OBJECTS);
    check_expect(n && og_object(spells[0])->parent == book && !og_possessions_open(OG_VIEW_SPELLS,0),
                 "a stored spellbook retains its children but cannot be used from the player");
    check_expect(og_inventory_transfer(book,og_state()->inventory_id), "taking the spellbook restores access");

    id = og_item_create(og_state()->inventory_id,7,0,0);
    og_object_edit(id)->message = 3;
    check_expect(og_possessions_open(OG_VIEW_INVENTORY,0) && og_possessions_use(id), "Use opens the actual readable book");
    check_expect(og_reading()->pages == 3 && !strcmp(og_reading()->title,"The Wild Elves"), "book header and absolute page offsets decode");
    seam_present(); og_draw(); screenshot("book-page1");
    key(0,OG_VK_NEXT); seam_present(); screenshot("book-page2");
    check_expect(og_possessions()->book_page == 1 && og_reading()->page_bytes > 0, "Page Down reads another actual page");
    key(27,27);
    check_expect(og_possessions()->page == OG_VIEW_INVENTORY && !og_reading()->file, "book closes its file and returns to the inventory");
    key(27,27);
    for (i = 0; i < og_book_count(); i++) {
        check_expect(og_book_open(&book_file,og_book_id(i)), "an installed book's bounded directory loads");
        for (n = 0; n < book_file.pages; n++) { check_expect(og_book_read(&book_file,n), "each original book page terminates inside its bounds"); }
        og_book_close(&book_file);
    }

    treasure = 0;
    check_visible_pile_click();
    for (i = 0; i < seam_level()->scene.nusables; i++) {
        const Usable *u;
        u = &seam_level()->scene.usables[i];
        if (u->kind == USE_OTHER && u->what >= OG_USE_TREASURE && u->what < OG_USE_TREASURE+22) { treasure = u; break; }
    }
    check_expect(treasure != 0, "the actual Hold exposes treasure with use geometry");
    if (treasure) {
        camera_set(&camera,treasure->cx,treasure->cy,treasure->cz,0,0);
        /* A ray from inside the use volume cannot miss this treasure or need a wall exception. */
        check_expect(og_activate(&seam_level()->scene,&camera,OG_MODE_GRAB,0,0,1,160,100), "the real activation ray reaches the dungeon treasure");
        check_expect(og_possessions()->page == OG_VIEW_INVENTORY && og_possessions()->remote,
                     "activating treasure opens the inventory's right container");
        container = og_possessions()->remote;
        seed = og_state()->random;
        remaining = og_inventory_rows(container,-1,rows,OG_OBJECTS);
        check_expect(remaining > 0, "the source loot table generates actual dungeon items");
        seam_present(); screenshot("dungeon-loot");
        og_possessions_close();
        check_expect(og_loot_container(og_object(OG_WORLD_ROOT)->place_key,treasure->tag,0,0,0,treasure->what-OG_USE_TREASURE-1) == container &&
                     og_state()->random == seed && og_inventory_rows(container,-1,rows,OG_OBJECTS) == remaining,
                     "reopening a dungeon pile neither rerolls nor duplicates its contents");
    }
    /* Exercise the visible Gold prompt and the actual dropped-pile geometry. */
    old_gold = og_character()->gold;
    og_possessions_open(OG_VIEW_INVENTORY,0);
    click(240,130); key('5','5'); key(13,13);
    pile = og_possessions()->remote;
    check_expect(pile && og_character()->gold == old_gold-5, "Gold prompt deposits real coins in the ground container");
    key(27,27); seam_creatures(&g_camera);
    n = 0;
    for (i = 0; i < seam_level()->scene.nusables; i++) {
        const Usable *u;
        u = &seam_level()->scene.usables[i];
        n += u->kind == USE_OTHER && u->what == OG_USE_TREASURE && (df_u32)u->tag == og_object(pile)->thing_key;
    }
    check_expect(n == 1, "dropped coins create one visible, usable pile in the real Hold");
    og_possessions_open(OG_VIEW_INVENTORY,pile); click(200,4); click(290,60);
    check_expect(og_character()->gold == old_gold && !og_inventory_rows(pile,-1,rows,OG_OBJECTS), "taking the dropped coins restores gold exactly once");
    key(27,27); seam_creatures(&g_camera);
    n = 0;
    for (i = 0; i < seam_level()->scene.nusables; i++) {
        const Usable *u;
        u = &seam_level()->scene.usables[i];
        n += u->kind == USE_OTHER && u->what == OG_USE_TREASURE && (df_u32)u->tag == og_object(pile)->thing_key;
    }
    check_expect(!n, "an emptied dropped pile leaves no phantom use target");
    saved = *og_state();
    check_expect(og_save_write(6), "inventory, equipment and owned spell records save");
    og_state_clear();
    check_expect(og_save_read(6) && !memcmp(&saved,og_state(),sizeof(saved)), "all item fields, containers and equipment survive disk loading");
    child = fork();
    if (child == 0) {
        execl("/proc/self/exe","ogfall-test","--ogfall-resume-items",test_root,(char *)0);
        _exit(127);
    }
    if (child > 0) {
        waitpid(child,&status,0);
        check_expect(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                     "a second process resumes the equipped, looted inventory through the real Load menu");
    } else { check_expect(0,"start an independent inventory resume process"); }
    check_expect(og_state_restore(&before), "restore the surrounding UI test session");
    og_possessions_reset();
}

static void check_drawn_weapon_pickup(void)
{
    Scene scene;
    Usable treasure;
    OgIn input;
    df_u32 sword;
    int x, y, bar;
    og_options_close(); og_possessions_reset();
    if (og_state()->equipped[19]) { og_inventory_unequip(og_state()->equipped[19]); }
    if (og_state()->equipped[21]) { og_inventory_unequip(og_state()->equipped[21]); }
    sword = og_item_create(og_state()->inventory_id,3,5,1);
    check_expect(og_inventory_equip(sword),"equip a melee weapon for the pickup regression");
    og_weapon_reset();
    if (!og_hud_weapon_ready()) { og_hud_toggle_ready(); }
    og_hud_set_mode(OG_MODE_GRAB); og_options_set_mouse(0);
    og_move_init(); og_move_place(0,WALK_EYE,0,0);
    mem_zero(&scene,sizeof(scene)); mem_zero(&treasure,sizeof(treasure));
    treasure.kind = USE_OTHER; treasure.model = treasure.sprite = treasure.mover = -1;
    treasure.nz = 1;
    treasure.cy = WALK_EYE; treasure.cz = 60;
    treasure.hw = treasure.hh = treasure.hd = 16; treasure.reach = 128;
    treasure.what = OG_USE_TREASURE+1; treasure.tag = 0x123abcd;
    scene.usables = &treasure; scene.nusables = 1;
    bar = !(og_options_flags() & 1);
    og_world_pointer(df_view_cx(),df_view_cy(),df_view_w(),df_view_h(),bar,&x,&y);
    mem_zero(&input,sizeof(input)); input.on_picture = 1; input.cx = x-6; input.cy = y-6;
    input.mouse_pressed = input.mouse_held = 2; input.held[OG_SWING_BUTTON] = 1;
    og_frame(&input,&g_camera,&scene);
    input.mouse_pressed = input.mouse_held = 0; input.held[OG_SWING_BUTTON] = 0; input.mouse_released = 2;
    og_frame(&input,&g_camera,&scene);
    check_expect(og_hud_weapon_ready() && og_possessions()->page == OG_VIEW_INVENTORY && og_possessions()->remote,
                 "a stationary right click takes treasure with a melee weapon drawn");
    og_possessions_close(); og_weapon_cancel();
}

static void check_dungeon_teleport(void)
{
    Scene scene;
    Mover portal[2];
    Usable trigger[2];
    Instance model[2];
    Bound bounds[2];
    OgMotion before, motion;
    OgIn input;
    float x, y, z, yaw;
    int i;

    og_move_capture(&before);
    og_options_close(); og_possessions_reset(); og_weapon_cancel();
    og_move_init(); og_move_place(0,WALK_EYE,0,256);
    og_move_capture(&motion);
    motion.crouched = 1; motion.eye = WALK_EYE/2;
    motion.pitch = 64; motion.vx = 100; motion.vy = -200;
    og_move_restore(&motion);
    mem_zero(&scene,sizeof(scene)); mem_zero(portal,sizeof(portal));
    mem_zero(trigger,sizeof(trigger)); mem_zero(model,sizeof(model)); mem_zero(bounds,sizeof(bounds));
    scene.movers = portal; scene.nmovers = 2; scene.usables = trigger; scene.nusables = 2;
    scene.models = model; scene.nmodels = 2; scene.bounds = bounds;
    /* Earlier engine tests can leave a consumed arrival guard. Settle it in
     * an empty scene before entering this fixture's first touch volume. */
    interact_take_use(); interact_teleport(&x,&y,&z,&yaw);
    scene.nusables = 0;
    interact_frame(&scene,&g_camera,0,0);
    scene.nusables = 2;
    for (i = 0; i < 2; i++) {
        trigger[i].kind = USE_MOVER; trigger[i].model = trigger[i].mover = i;
        trigger[i].sprite = -1; trigger[i].touch = 1;
        portal[i].teleport = 1; portal[i].next = -1;
        bounds[i].ex = bounds[i].ez = 20; bounds[i].ey = 20;
    }
    bounds[0].cy = 20;
    bounds[1].cx = portal[0].to_x = 300;
    bounds[1].cy = 260; portal[0].to_y = 240+WALK_EYE;
    bounds[1].cz = portal[0].to_z = 400;
    portal[0].to_yaw = DF_TAU/2; portal[1].to_y = WALK_EYE;
    /* The game's body, rather than a stray viewer camera, touches the pad. */
    camera_set(&g_camera,900,900,900,0,0);
    interact_frame(&scene,&g_camera,0,0);
    mem_zero(&input,sizeof(input)); input.on_picture = 1; input.cx = 160; input.cy = 80;
    og_frame(&input,&g_camera,&scene);
    og_frame(&input,&g_camera,&scene);
    check_expect(og_move_body()->feet[0] == 300 && og_move_body()->feet[1] == 240 &&
                 og_move_body()->feet[2] == 400 && og_move_crouched(),
                 "a touch teleporter moves OGFALL's own crouched body to the target floor");
    og_move_capture(&motion);
    check_expect(motion.yaw == 256 && motion.pitch == 64 && !motion.vx && !motion.vy &&
                 !motion.have_safe && motion.top == 240,
                 "teleport retains original view angles and restarts collision/fall tracking at arrival");
    check_expect(g_camera.x == 300 && g_camera.y == 240+WALK_EYE/2 && g_camera.z == 400 &&
                 og_state()->x == 300 && og_state()->eye_y == g_camera.y,
                 "camera and save pose follow the teleported body immediately");
    interact_frame(&scene,&g_camera,0,0);
    interact_frame(&scene,&g_camera,0,0);
    check_expect(!interact_teleport(&x,&y,&z,&yaw),"a paired arrival pad cannot immediately teleport the player back");
    og_move_restore(&before); og_move_camera(&g_camera);
    og_state_pose(g_camera.x,g_camera.y,g_camera.z,before.yaw,before.pitch);
}

static void check_rest(void)
{
    OgIn in;
    df_u32 minutes;
    int i, health, fatigue, magicka;
    g_saved = *og_state();
    og_dungeon_open(0,0,0,0);
    og_move_body()->grounded = 1;
    og_character()->max_health = 100; og_character()->health = 50;
    og_character()->attribute[OG_END] = 50; og_character()->skill[0] = 40;
    og_character()->fatigue = 0; og_character()->max_magicka = 80; og_character()->magicka = 0;
    minutes = og_state()->minutes;
    click(249,185);
    check_expect(og_rest()->open && og_paused() && og_wants_pointer(),"the HUD campfire opens Rest and pauses dungeon combat");
    screenshot("rest-choice"); click(110,74); key('2','2'); key(13,13);
    check_expect(og_rest()->stage == OG_REST_RUNNING && og_rest()->hours == 2,"timed rest accepts two hours");
    screenshot("rest-running");
    mem_zero(&in,sizeof(in)); in.dt = 0.1f;
    for (i = 0; i < 12 && og_rest()->open; i++) { frame(&in); }
    check_expect(!og_rest()->open && og_state()->minutes == minutes+120 && og_character()->health > 50 &&
                 og_character()->fatigue > 0 && og_character()->magicka == 20,"two hours of rest advance time and recover original hourly vitals");
    og_move_body()->grounded = 1; in.dt = 0; in.pressed[OG_REST] = 1; frame(&in);
    check_expect(og_rest()->open,"the bound Rest key opens the same controller");
    click(205,74); key('3','3'); key(13,13);
    health = og_character()->health; fatigue = og_character()->fatigue; magicka = og_character()->magicka;
    minutes = og_state()->minutes; mem_zero(&in,sizeof(in)); in.dt = 0.1f;
    for (i = 0; i < 60 && og_rest()->open; i++) { frame(&in); }
    check_expect(!og_rest()->open && og_state()->minutes == minutes+180 && og_character()->health == health &&
                 og_character()->fatigue == fatigue && og_character()->magicka == magicka,"three hours of loitering pass time without healing");
    og_move_body()->grounded = 1; mem_zero(&in,sizeof(in)); in.pressed[OG_REST] = 1; frame(&in);
    click(160,74); screenshot("rest-healing");
    mem_zero(&in,sizeof(in)); in.dt = 0.1f;
    for (i = 0; i < 100 && og_rest()->open; i++) { frame(&in); }
    check_expect(!og_rest()->open && og_character()->health == 100 && og_character()->magicka == 80,
                 "rest until healed stops when all recoverable vitals are full");
    og_move_body()->grounded = 1; og_rest_open(&seam_level()->scene,&g_camera); key(27,27);
    check_expect(!og_rest()->open && !og_paused(),"Escape cancels Rest and returns to the dungeon");
    check_expect(og_state_restore(&g_saved),"restore the surrounding session after the Rest test");
}

static void check_spatial_sound(void)
{
    SoundSpot near, far;
    int volume, on;
    volume = sound_volume(); on = sound_on();
    sound_set_on(1); sound_set_volume(100); sound_stop_all(); sound_listen(0,0,0,0);
    og_sound_at(10000,0,0,128); sound_update();
    check_expect(sound_voice_at(0,&near),"a creature's voice is positional");
    sound_stop_all(); og_sound_at(10000,0,0,640); sound_update();
    check_expect(sound_voice_at(0,&far) && far.gain < near.gain*0.5f && far.gain > 0,
                 "the real DAGGER.SND creature clip loses volume with distance");
    sound_stop_all(); og_sound_at(10000,0,0,2048);
    check_expect(sound_busy() == 0,"an out-of-range creature cannot be heard");
    sound_set_volume(volume); sound_set_on(on);
}

static void check_death(void)
{
    OgIn input;
    int health, w, h, i;
    char path[64];
    og_options_close(); og_possessions_reset();
    health = og_character()->health;
    og_character()->health = 0;
    mem_zero(&input,sizeof(input)); frame(&input);
    check_expect(og_dead() && !og_options_up() && og_wants_pointer(),
                 "death offers recovery choices without forcing a load");
    check_expect(!og_save_write(6) && strstr(og_save_error(),"dead"),"the save API refuses a dead character");
    og_options_open_files(1);
    check_expect(!og_options_up(),"the save menu refuses a dead character");
    screenshot("death");
    for (i = 1; i <= 6; i++) {
        snprintf(path,sizeof(path),"ogfall\\saves\\S%04d.SAV",i); fs_delete(path);
    }
    key('l','L');
    check_expect(og_options_panel() == OG_PANEL_FILES,"death can open the load screen");
    for (i = 0; i < 6; i++) { check_expect(!og_files_info(i)->status,"the death recovery test has no saved games"); }
    key(27,27);
    check_expect(!og_options_up() && og_dead(),"cancelled loading returns to death choices");
    check_expect(key('n','N') == OG_FRAME_TITLE && og_chargen_mode() == OG_UI_TITLE && !og_dead(),
                 "a player without saves can return to title and create a new character");
    seam_close_hold(); seam_title(); screenshot("title-after-death");
    check_expect(og_pic(OG_PIC_TITLE) && og_pic(OG_PIC_MAP) && og_pic(OG_PIC_BACK) &&
                 og_pic(OG_PIC_BUTTONS) && og_pic(OG_PIC_YES) && og_pic(OG_PIC_NO) &&
                 og_pic(OG_PIC_MALE) && og_pic(OG_PIC_FEMALE),
                 "death restores all title and creation pictures within the card budget");
    og_character()->health = health;
    w = 200; h = 100; og_size_fit(&w,&h,100,100);
    check_expect(w == 100 && h == 50,"inventory icons preserve their original aspect ratio");
    w = 50; h = 300; og_size_fit(&w,&h,100,100);
    check_expect(w == 16 && h == 100,"inventory icon fitting uses classic's fixed point rounding");
    w = 50; h = 60; og_size_fit(&w,&h,100,100);
    check_expect(w == 50 && h == 60,"inventory icons are never enlarged beyond their native art");
}

void check_ogui(const char *root)
{
    OgConfig preferences;
    char resolved[4096], save_path[64];
    int slot;
    const char *test_root = "build/ogfall-ui-data/ARENA2";

    preferences = *og_config();
    g_before = *og_state();
    og_move_camera(&g_camera);
    fs_set_root("build/ARENA2");
    check_expect(fs_make_dirs("ogfall-ui-data"), "create an isolated UI test directory");
    check_expect(realpath(root, resolved) != 0, "resolve the read-only game data");
    unlink("build/ogfall-ui-data/ARENA2");
    check_expect(symlink(resolved, test_root) == 0, "link game data without copying it into saves");
    fs_set_root(test_root);
    check_sound_ids();
    check_key_bank();
    check_preferences();
    check_options_and_controls();
    check_pointer_shapes();
    check_save_load(root, test_root);
    check_possessions(test_root);
    check_inventory_completion(test_root);
    check_drawn_weapon_pickup();
    check_dungeon_teleport();
    check_spatial_sound();
    check_rest();
    check_death();
    fs_delete("ogfall\\settings.cfg");
    for (slot = 1; slot <= 6; slot++) {
        snprintf(save_path, sizeof(save_path), "ogfall\\saves\\S%04d.SAV", slot);
        fs_delete(save_path);
    }
    fs_remove_dir("ogfall\\saves");
    fs_remove_dir("ogfall");
    fs_set_root("build/ARENA2");
    unlink(test_root);
    fs_remove_dir("ogfall-ui-data");
    fs_set_root(root);
    og_state_restore(&g_before);
    og_config_set(&preferences);
    og_chargen_resume(); og_state_restore_world();
    {
        float x, y, z;
        check_expect(seam_open_hold(&x,&y,&z),"restore the Hold after testing the complete death/title flow");
    }
}
