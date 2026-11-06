#include "game/dagger/play/keys.h"
#include "game/dagger/play/config.h"
#include "game/dagger/play/mouse.h"
#include "game/dagger/play/viewopt.h"
#include "game/dagger/play/control.h"
#include "engine/gfx/camera.h"
#include "engine/gfx/gfx.h"
#include "engine/gfx/glide.h"
#include "engine/gfx/draw.h"
#include "engine/scene/scene.h"
#include "engine/scene/light.h"
#include "engine/scene/weather.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/assets/recfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/platform/fs.h"
#include "engine/platform/input.h"

/* See keys.h. Settings live in gfall.cfg (config.h); DATA\KEYS.DAT is only read, once, by a program that has no gfall.cfg yet. */

typedef struct KeyBind {
    df_u16 id;
    df_u16 vk;
} KeyBind;

static const RecField g_key_fields[] = {
    REC_FIELD("id", REC_UINT, KeyBind, id),
    REC_FIELD("vk", REC_UINT, KeyBind, vk)
};
static const RecSchema g_key_schema = { "KEYS", "key", g_key_fields, 2, (df_u16)sizeof(KeyBind), 0 };


void keys_defaults(void)
{
    int i;

    mouse_defaults();
    control_set_classic(1);
    for (i = 0; i < INPUT_ACTIONS; i++) {
        if (i != INPUT_MENU && config_default_key(i)) {
            input_bind(i, config_default_key(i));
        }
    }
}

/* gfall.cfg's text beside the program, in a buffer that stays (a program has one): 0 when there is none. */
static const char *read_cfg(int *n)
{
    df_u32 size;
    const df_u8 *b;

    b = vfs_read_beside(CONFIG_FILE, &size);
    if (!b) {
        return 0;
    }
    *n = (int)size;
    return (const char *)b;
}

int keys_peek_mode(int *w, int *h)
{
    df_u32 n;
    const df_u8 *b;
    const char *text;
    int len;

    text = read_cfg(&len);
    if (text) {
        if (config_peek_mode(text, len, w, h)) {
            log_print(LOG_INFO, "keys: %s asks for %dx%d", CONFIG_FILE, *w, *h);
            return 1;
        }
        return 0;
    }
    /* No gfall.cfg: an older program's DATA\KEYS.DAT may hold the choice. */
    b = vfs_read_beside(KEYS_FILE, &n);
    return b && viewopt_mode_in(b, n, w, h);
}

/* The settings as they are now, into c: the engine's and the game's, bindings and all. */
static void config_gather(Config *c)
{
    int i;

    for (i = 0; i < INPUT_ACTIONS; i++) {
        c->key[i] = i == INPUT_MENU ? 0 : input_binding(i);
    }
    c->mouse_cursor = control_classic();
    c->fov = df_iround(camera_fov());
    c->far_clip = df_iround(gfx_far());
    c->fog = gfx_fog();
    c->crisp_sprites = draw_sprite_point();
    c->vsync = glide_vsync();
    c->dungeon_lights = light_on();
    c->ambient_light = light_ambient();
    c->light_range = light_range();
    c->light_shadow = light_shadow();
    c->weather_view = weather_sim();
    c->flora = scene_flora();
    c->flora_density = scene_flora_density();
    c->flora_sight = scene_flora_sight();
    c->texture_detail = viewopt_tex_detail();
    c->sprite_detail = viewopt_sprite_detail();
    viewopt_mode(&c->mode_w, &c->mode_h);
    c->sfx_volume = sound_volume();
    c->music_volume = music_volume();
    c->full_hud = !viewopt_full();
    c->head_bob = viewopt_bob();
    c->toggle_crouch = viewopt_crouch_toggle();
}

/* A Config into the live state: what the Settings screens do on Apply, done at the start. */
static void config_apply(const Config *c)
{
    int i;

    for (i = 0; i < INPUT_ACTIONS; i++) {
        /* Escape is never rebound: a file that says so is ignored there. */
        if (i != INPUT_MENU && c->key[i] && c->key[i] != input_binding(i) && !input_bind(i, c->key[i])) {
            log_print(LOG_WARN, "keys: %s's key %d for action %d is refused", CONFIG_FILE, c->key[i], i);
        }
    }
    mouse_set_cursor(c->mouse_cursor);
    control_set_classic(c->mouse_cursor);
    camera_set_fov((float)c->fov);
    gfx_set_far((float)c->far_clip);
    gfx_set_fog(c->fog);
    draw_set_sprite_point(c->crisp_sprites);
    glide_set_vsync(c->vsync);
    light_set_on(c->dungeon_lights);
    light_set_ambient(c->ambient_light);
    gfx_set_light_range(c->light_range);
    gfx_set_light_shadow(c->light_shadow);
    weather_set_sim(c->weather_view);
    scene_set_flora(c->flora);
    scene_set_flora_density(c->flora_density);
    scene_set_flora_sight(c->flora_sight);
    viewopt_set_tex_detail(c->texture_detail);
    viewopt_set_sprite_detail(c->sprite_detail);
    gfx_set_tex_detail(c->texture_detail);
    gfx_set_sprite_detail(c->sprite_detail);
    viewopt_set_mode(c->mode_w, c->mode_h);
    sound_set_volume(c->sfx_volume);
    music_set_volume(c->music_volume);
    viewopt_set_full(!c->full_hud);
    viewopt_set_bob(c->head_bob);
    viewopt_set_crouch_toggle(c->toggle_crouch);
}

/* DATA\KEYS.DAT, the file an older program kept its settings in: read once when there is no gfall.cfg. */
static int legacy_load(void)
{
    static RecTable table;
    const df_u8 *b;
    df_u32 n;
    df_u32 pos;
    RecChunk c;
    int i;

    b = vfs_read_beside(KEYS_FILE, &n);
    if (!b || !rec_is_file(b, n)) {
        return 0;
    }
    mouse_read(b, n);
    control_read_settings(b, n);
    /* One setting, two rows. A file from before both existed keeps the
       row it has; when both are present, CTLS is what classic controls
       follow and MOUS is brought to it. */
    {
        int saw_mous;
        int saw_ctls;
        df_u32 scan;

        saw_mous = 0;
        saw_ctls = 0;
        scan = 0;
        while (rec_next(b, n, &scan, &c)) {
            if (c.tag[0] == 'M' && c.tag[1] == 'O' && c.tag[2] == 'U' && c.tag[3] == 'S') {
                saw_mous = 1;
            }
            if (c.tag[0] == 'C' && c.tag[1] == 'T' && c.tag[2] == 'L' && c.tag[3] == 'S') {
                saw_ctls = 1;
            }
        }
        if (saw_ctls) {
            mouse_set_cursor(control_classic());
        } else if (saw_mous) {
            control_set_classic(mouse_cursor());
        }
    }
    viewopt_read(b, n);
    /* The details chosen, given to the engine before the first place (it starts at High). */
    gfx_set_tex_detail(viewopt_tex_detail());
    gfx_set_sprite_detail(viewopt_sprite_detail());
    if (!table.schema && !rec_table_init(&table, &g_key_schema, INPUT_ACTIONS)) {
        return 0;
    }
    rec_table_clear(&table);
    pos = 0;
    while (rec_next(b, n, &pos, &c)) {
        if (c.tag[0] == 'K' && c.tag[1] == 'E' && c.tag[2] == 'Y' && c.tag[3] == 'S') {
            rec_merge(&table, &c, 0);
        }
    }
    for (i = 0; i < table.count; i++) {
        const KeyBind *k;

        k = (const KeyBind *)(table.rows + (df_u32)i * sizeof(KeyBind));
        /* Escape is never rebound: a file that says so is ignored there. */
        if (k->id < INPUT_ACTIONS && k->id != INPUT_MENU && k->vk && !input_bind(k->id, k->vk)) {
            log_print(LOG_WARN, "keys: %s's key %d for action %d is refused", KEYS_FILE, (int)k->vk, (int)k->id);
        }
    }
    return 1;
}

int keys_load(void)
{
    static Config c;
    const char *text;
    int len;
    int bad;
    int first;

    keys_defaults();
    viewopt_defaults();
    text = read_cfg(&len);
    if (text) {
        /* Whatever the file leaves out stays as the program has it. */
        config_gather(&c);
        bad = config_parse(text, len, &c, &first);
        config_apply(&c);
        if (bad) {
            log_print(LOG_WARN, "keys: %s: %d lines were not used, the first line %d", CONFIG_FILE, bad, first);
        } else {
            log_print(LOG_INFO, "keys: %s read", CONFIG_FILE);
        }
        return 1;
    }
    /* No gfall.cfg: an older program's file, if there is one, and then the first one is written. */
    if (legacy_load()) {
        gfx_set_tex_detail(viewopt_tex_detail());
        gfx_set_sprite_detail(viewopt_sprite_detail());
        log_print(LOG_INFO, "keys: no %s; %s read, %s written", CONFIG_FILE, KEYS_FILE, CONFIG_FILE);
        keys_save();
        return 1;
    }
    log_print(LOG_INFO, "keys: no %s; the defaults, and one written", CONFIG_FILE);
    keys_save();
    return 0;
}

int keys_save(void)
{
    static Config c;
    static char text[CONFIG_MAX];
    int n;

    config_gather(&c);
    n = config_format(&c, text, (int)sizeof(text));
    if (n < 0 || !fs_create(CONFIG_FILE) || !fs_append(text, (df_u32)n)) {
        fs_abort();
        log_print(LOG_WARN, "keys: %s could not be written", CONFIG_FILE);
        return 0;
    }
    fs_finish();
    log_print(LOG_INFO, "keys: %s written, the screen's mode %dx%d", CONFIG_FILE, c.mode_w, c.mode_h);
    return 1;
}
