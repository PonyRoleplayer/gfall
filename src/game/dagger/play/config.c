#include "game/dagger/play/config.h"
#include "game/dagger/play/state.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "engine/platform/fs.h"
#include "engine/platform/input.h"
#include "engine/sound/sound.h"
#include "engine/sound/music.h"
#include "engine/gfx/camera.h"
#include <stddef.h>

static OgConfig g_config;
static const char *g_error = "";
static const struct { const char *token, *name; int key; } g_action[OG_ACTS] = {
    {"forward", "Forward", 0x26}, {"back", "Reverse", 0x28},
    {"turn_left", "Turn left", 0x25}, {"slide_left", "Slide left", 0xa0},
    {"turn_right", "Turn right", 0x27}, {"slide_right", "Slide right", 0xa1},
    {"float_up", "Float up", 0x21}, {"float_down", "Float down", 0x22},
    {"jump", "Jump", 'J'}, {"crouch", "Crouch", 'D'},
    {"strafe", "Slide", 0x11}, {"run", "Run", 'P'},
    {"rest", "Rest", 'R'}, {"transport", "Transport", 'T'},
    {"steal", "Steal mode", 0x70}, {"grab", "Grab mode", 0x71},
    {"info", "Info mode", 0x72}, {"talk", "Talk mode", 0x73},
    {"activate", "Activate center", 0x20}, {"cursor", "Activate cursor", 0x0d},
    {"look_up", "Look up", 0x2d}, {"look_down", "Look down", 0x2e},
    {"look_home", "Center view", 0x24}, {"look_around", "Look around", 0x12},
    {"logbook", "Logbook", 'L'}, {"notebook", "Notebook", 'N'},
    {"automap", "Automap", 'M'}, {"travel", "Travel map", 'W'},
    {"cast", "Cast spell", 0x08}, {"recast", "Recast spell", 'Q'},
    {"abort", "Abort spell", 'E'}, {"magic_item", "Use magic item", 'U'},
    {"ready", "Ready weapon", 'A'}, {"swing", "Swing weapon", 2},
    {"switch_hand", "Switch hand", 'S'}, {"status", "Status", 'I'},
    {"sheet", "Character sheet", 0x74}, {"inventory", "Inventory", 0x75}
};

typedef struct Setting { const char *name; size_t offset; int low, high; } Setting;
#define SETTING(name, field, low, high) {name, offsetof(OgConfig, field), low, high}
static const Setting g_setting[] = {
    SETTING("view_flags", flags, 0, 3), SETTING("mouse_mode", mouse_mode, 0, 1),
    SETTING("sound_volume", sound_volume, 0, 100), SETTING("music_volume", music_volume, 0, 100),
    SETTING("detail", detail, 0, 127),
    SETTING("sensitivity_x", sensitivity_x, 1, 15), SETTING("sensitivity_y", sensitivity_y, 1, 15),
    SETTING("turn_rate", turn_rate, 0, 15), SETTING("reverse_y", reverse_y, 0, 1),
    SETTING("width", width, 320, 800), SETTING("height", height, 200, 600),
    SETTING("fov", fov, 45, 120)
};
#undef SETTING

static int *setting_value(OgConfig *config, int index)
{
    return (int *)((char *)config + g_setting[index].offset);
}

const OgConfig *og_config(void)
{
    return &g_config;
}

const char *og_config_error(void)
{
    return g_error;
}

const char *og_action_name(int action)
{
    return action >= 0 && action < OG_ACTS ? g_action[action].name : "";
}

const char *og_action_token(int action)
{
    return action >= 0 && action < OG_ACTS ? g_action[action].token : "";
}

int og_key_allowed(int key)
{
    /* Escape, the console and F10 retain their own UI actions. */
    return key >= 1 && key <= 254 && key != 0x1b && key != 0xc0 && key != 0x79;
}

void og_config_defaults(OgConfig *config)
{
    int i;

    mem_zero(config, sizeof(*config));
    config->sound_volume = config->music_volume = 80;
    config->detail = 127;
    config->sensitivity_x = config->sensitivity_y = 1;
    config->width = 640;
    config->height = 480;
    config->fov = 75;
    for (i = 0; i < OG_ACTS; i++) {
        config->key[i] = g_action[i].key;
    }
}

static int valid(const OgConfig *config)
{
    int i, value;

    for (i = 0; i < (int)(sizeof(g_setting) / sizeof(g_setting[0])); i++) {
        value = *(const int *)((const char *)config + g_setting[i].offset);
        if (value < g_setting[i].low || value > g_setting[i].high) {
            return 0;
        }
    }
    if (!((config->width == 320 && config->height == 240) ||
          (config->width == 400 && config->height == 300) ||
          (config->width == 512 && config->height == 384) ||
          (config->width == 640 && config->height == 480) ||
          (config->width == 800 && config->height == 600))) {
        return 0;
    }
    for (i = 0; i < OG_ACTS; i++) {
        if (!og_key_allowed(config->key[i])) {
            return 0;
        }
    }
    return 1;
}

void og_config_apply(void)
{
    og_state_settings(g_config.flags, g_config.mouse_mode);
    sound_set_volume(g_config.sound_volume);
    music_set_volume(g_config.music_volume);
    camera_set_fov((float)g_config.fov);
}

int og_config_set(const OgConfig *config)
{
    if (!config || !valid(config)) {
        g_error = "Invalid OGFALL settings";
        return 0;
    }
    g_config = *config;
    og_config_apply();
    return 1;
}

static char *trim(char *text)
{
    int n;

    while (*text == ' ' || *text == '\t' || *text == '\r') { text++; }
    n = 0;
    while (text[n]) {
        n++;
    }
    while (n && (text[n-1] == ' ' || text[n-1] == '\t' || text[n-1] == '\r')) { text[--n] = 0; }
    return text;
}

static int number(const char *text, int *out)
{
    unsigned value;

    value = 0;
    if (!*text) {
        return 0;
    }
    while (*text >= '0' && *text <= '9') {
        if (value > 100000u) {
            return 0;
        }
        value = value * 10u + (unsigned)(*text++ - '0');
    }
    if (*text) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int same_word(const char *a, const char *b)
{
    int x, y;
    do {
        x = (unsigned char)*a++;
        y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') { x += 'a' - 'A'; }
        if (y >= 'A' && y <= 'Z') { y += 'a' - 'A'; }
        if (x != y) { return 0; }
    } while (x);
    return 1;
}

static void key_name(int key, char *name, int capacity)
{
    /* Numeric VK input remains accepted for existing settings. Give digit
     * keys an unambiguous name, distinct from the old mouse-button codes. */
    if (key >= '0' && key <= '9') {
        log_format(name, capacity, "Digit %d", key - '0');
    } else if (key >= 0xa0 && key <= 0xa5) {
        static const char *modifier[] = {
            "Left Shift", "Right Shift", "Left Ctrl", "Right Ctrl", "Left Alt", "Right Alt"
        };
        df_copy(name, modifier[key - 0xa0], capacity);
    } else {
        input_key_name(key, name, capacity);
    }
}

static int key_value(const char *text, int *value)
{
    char name[32];
    int key;
    if (number(text, value)) { return 1; }
    for (key = 1; key < 255; key++) {
        key_name(key, name, sizeof(name));
        if (same_word(text, name)) { *value = key; return 1; }
    }
    return 0;
}

static int boolean(const char *text, int *value)
{
    if (same_word(text, "on") || same_word(text, "yes")) { *value = 1; return 1; }
    if (same_word(text, "off") || same_word(text, "no")) { *value = 0; return 1; }
    return number(text, value) && *value <= 1;
}

static int set_named(OgConfig *config, const char *name, char *value)
{
    int v, i;
    char *height;
    if (df_eq(name, "full_screen") || df_eq(name, "head_bob")) {
        if (!boolean(value, &v)) { return 0; }
        i = df_eq(name, "full_screen") ? 1 : 2;
        config->flags = (config->flags & ~i) | (v ? i : 0);
        return 1;
    }
    if (df_eq(name, "mouse")) {
        if (same_word(value, "cursor")) { config->mouse_mode = 0; return 1; }
        if (same_word(value, "view")) { config->mouse_mode = 1; return 1; }
        return 0;
    }
    if (df_eq(name, "resolution")) {
        height = value;
        while (*height && *height != 'x' && *height != 'X') { height++; }
        if (!*height) { return 0; }
        *height++ = 0;
        return number(trim(value), &config->width) && number(trim(height), &config->height);
    }
    if (df_eq(name, "reverse_y")) { return boolean(value, &config->reverse_y); }
    if (name[0] == 'k' && name[1] == 'e' && name[2] == 'y' && name[3] == '.') {
        for (i = 0; i < OG_ACTS; i++) {
            if (df_eq(name + 4, g_action[i].token)) { return key_value(value, &config->key[i]); }
        }
        return 0;
    }
    for (i = 0; i < (int)(sizeof(g_setting) / sizeof(g_setting[0])); i++) {
        if (df_eq(name, g_setting[i].name)) { return number(value, setting_value(config, i)); }
    }
    return 0;
}

int og_config_parse(OgConfig *out, const char *text)
{
    OgConfig candidate;
    char line[128];
    char *name, *value;
    int n;

    if (!out || !text) { return 0; }
    og_config_defaults(&candidate);
    while (*text) {
        n = 0;
        while (*text && *text != '\n') {
            if (n >= (int)sizeof(line)-1) {
                return 0;
            }
            line[n++] = *text++;
        }
        if (*text) {
            text++;
        }
        line[n] = 0;
        name = trim(line);
        if (!*name || *name == '#' || *name == ';') {
            continue;
        }
        value = name;
        while (*value && *value != '=') {
            value++;
        }
        if (!*value) {
            return 0;
        }
        *value++ = 0;
        name = trim(name);
        value = trim(value);
        if (!set_named(&candidate, name, value)) { return 0; }
    }
    if (!valid(&candidate)) {
        return 0;
    }
    *out = candidate;
    return 1;
}

static int append(char *text, int capacity, int *used, const char *line)
{
    while (*line) {
        if (*used >= capacity - 1) { return 0; }
        text[(*used)++] = *line++;
    }
    text[*used] = 0;
    return 1;
}

int og_config_format(const OgConfig *config, char *text, int capacity)
{
    char line[128], name[32];
    int i, used;

    if (!config || !text || capacity < 1 || !valid(config)) { return 0; }
    used = 0;
    text[0] = 0;
#define PUT(s) do { if (!append(text, capacity, &used, s)) { return 0; } } while (0)
    PUT("# OGFALL settings. Edit name=value lines; Options writes this file on Continue.\r\n");
    PUT("# Switches use on/off (yes/no or 1/0 also work).\r\n");
    PUT("\r\n# Screen. Resolution changes take effect at the next start.\r\n");
    log_format(line, sizeof(line), "resolution=%dx%d\r\nfov=%d\r\n", config->width, config->height, config->fov);
    PUT(line);
    PUT(config->flags & 1 ? "full_screen=on\r\n" : "full_screen=off\r\n");
    PUT(config->flags & 2 ? "head_bob=on\r\n" : "head_bob=off\r\n");
    PUT("\r\n# Audio volumes are percentages. Detail is stored only (0..127).\r\n");
#define NUMBER(name, value) do { log_format(line, sizeof(line), name "=%d\r\n", value); PUT(line); } while (0)
    NUMBER("sound_volume", config->sound_volume);
    NUMBER("music_volume", config->music_volume);
    NUMBER("detail", config->detail);
    PUT("\r\n# Mouse: cursor or view; sensitivities 1..15, turn rate 0..15.\r\n");
    PUT(config->mouse_mode ? "mouse=view\r\n" : "mouse=cursor\r\n");
    NUMBER("sensitivity_x", config->sensitivity_x);
    NUMBER("sensitivity_y", config->sensitivity_y);
    NUMBER("turn_rate", config->turn_rate);
#undef NUMBER
    PUT(config->reverse_y ? "reverse_y=on\r\n" : "reverse_y=off\r\n");
    PUT("\r\n# Keys use names: Up, K, Left Shift, Right Button, Digit 2, etc.\r\n");
    PUT("# Escape, F10 and the console key are reserved.\r\n");
    for (i = 0; i < OG_ACTS; i++) {
        key_name(config->key[i], name, sizeof(name));
        log_format(line, sizeof(line), "key.%s=%s\r\n", g_action[i].token, name);
        PUT(line);
    }
#undef PUT
    return used;
}

int og_config_read(void)
{
    FsFile *file;
    OgConfig candidate;
    char text[4096];
    df_u32 size;
    const char *path;

    path = OG_CONFIG_FILE;
    og_config_defaults(&g_config);
    if (!fs_exists(path) && fs_exists(OG_CONFIG_FILE ".OLD")) {
        path = OG_CONFIG_FILE ".OLD";
    }
    if (!fs_exists(path)) {
        og_config_apply();
        g_error = "";
        return 1;
    }
    file = fs_open_beside(path);
    if (!file) {
        g_error = "Cannot open ogfall/settings.cfg";
        return 0;
    }
    size = fs_size(file);
    if (size >= sizeof(text) || !fs_read_at(file, 0, text, size)) {
        fs_close(file);
        g_error = "Cannot read ogfall/settings.cfg";
        return 0;
    }
    fs_close(file);
    text[size] = 0;
    if (!og_config_parse(&candidate, text)) {
        g_error = "Invalid ogfall/settings.cfg; defaults retained";
        og_config_apply();
        return 0;
    }
    g_config = candidate;
    og_config_apply();
    g_error = "";
    return 1;
}

int og_config_write(void)
{
    static char text[4096];
    static char verify[4096];
    OgConfig candidate;
    FsFile *file;
    int size, good;

    size = og_config_format(&g_config, text, sizeof(text));
    if (!size || !fs_make_dirs("ogfall") || !fs_create("ogfall\\settings.tmp")) {
        g_error = "Cannot create ogfall/settings.cfg";
        return 0;
    }
    if (!fs_append(text, (df_u32)size)) {
        fs_abort();
        g_error = "Cannot write ogfall/settings.cfg";
        return 0;
    }
    fs_finish();
    file = fs_open_beside("ogfall\\settings.tmp");
    good = file && fs_size(file) == (df_u32)size && fs_read_at(file, 0, verify, (df_u32)size);
    if (file) {
        fs_close(file);
    }
    verify[size] = 0;
    if (!good || !og_config_parse(&candidate, verify) ||
        !fs_move("ogfall\\settings.tmp", OG_CONFIG_FILE)) {
        fs_delete("ogfall\\settings.tmp");
        g_error = "Cannot verify or replace ogfall/settings.cfg";
        return 0;
    }
    g_error = "";
    return 1;
}
