#include "game/dagger/play/config.h"
#include "engine/core/df.h"

/* See config.h and docs/game.md (The settings file). */

/*
 * What an action goes by in the file, by its INPUT_* number: the order of input.h. Escape (INPUT_MENU) is the
 * engine's and is not kept. A new action is a new name at its number, and old files lack it and keep its default.
 */
static const char *const g_action_names[INPUT_ACTIONS] = {
    "attack", "use", "run", "jump", "character", "inventory", "map", "rest", 0,
    "forward", "back", "left", "right", "swap_hands", "cursor", "swing", "turn_left", "turn_right",
    "look_up", "look_down", "look_center", "freelook", "travel_map", "transport", "crouch",
    "spellbook", "recast"
};

/*
 * Classic's defaults as Windows virtual keys (no windows.h here): the left button 0x01, Shift 0x10, Space 0x20, a
 * letter its capital. (The table keys.c held; the action ids stay append-only so old files keep their meaning.)
 */
static const struct {
    int action;
    int vk;
} g_default_keys[] = {
    { INPUT_ATTACK, 0x01 }, { INPUT_USE, 'E' }, { INPUT_RUN, 0x10 }, { INPUT_JUMP, 0x20 },
    { INPUT_CHARACTER, 'C' }, { INPUT_INVENTORY, 'I' }, { INPUT_MAP, 'M' }, { INPUT_REST, 'R' },
    { INPUT_FORWARD, 'W' }, { INPUT_BACK, 'S' }, { INPUT_LEFT, 'A' }, { INPUT_RIGHT, 'D' },
    { INPUT_SWAP_HANDS, 'H' },
    /* Free the HUD pointer without pausing. */
    { INPUT_CURSOR, 0x09 },
    /* Cursor mode separates the right-button gesture from physical left-button UI clicks. */
    { INPUT_SWING, 0x02 }, { INPUT_TURN_LEFT, 0x25 }, { INPUT_TURN_RIGHT, 0x27 },
    { INPUT_LOOK_UP, 0x21 }, { INPUT_LOOK_DOWN, 0x22 }, { INPUT_LOOK_CENTER, 0x24 }, { INPUT_FREELOOK, 0x12 },
    /* The travel map's own key, DFU's V: M is the automap. */
    { INPUT_TRAVEL_MAP, 'V' }, { INPUT_TRANSPORT, 'T' },
    /* Crouch, Ctrl (either key) as a first-person game has it: C is the character sheet. */
    { INPUT_CROUCH, 0x11 },
    /* The spellbook and the last spell again, DFU's B and Q. */
    { INPUT_SPELLBOOK, 'B' }, { INPUT_RECAST, 'Q' }
};

int config_default_key(int action)
{
    int i;

    for (i = 0; i < (int)(sizeof(g_default_keys) / sizeof(g_default_keys[0])); i++) {
        if (g_default_keys[i].action == action) {
            return g_default_keys[i].vk;
        }
    }
    return 0;
}

const char *config_action_name(int action)
{
    return action >= 0 && action < INPUT_ACTIONS ? g_action_names[action] : 0;
}

/* ---- Text helpers ---- */

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

/* 1 when text of n bytes is the NUL-terminated word, ignoring case. */
static int same(const char *s, int n, const char *word)
{
    int i;

    for (i = 0; i < n; i++) {
        if (!word[i] || lower((unsigned char)s[i]) != lower((unsigned char)word[i])) {
            return 0;
        }
    }
    return word[n] == 0;
}

static int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/* An integer in lo..hi from s (n bytes); 0 when it is none or out of range. */
static int to_int(const char *s, int n, int lo, int hi, int *out)
{
    long v;
    int neg;
    int i;

    i = 0;
    neg = 0;
    if (i < n && s[i] == '-') {
        neg = 1;
        i++;
    }
    if (i >= n) {
        return 0;
    }
    v = 0;
    for (; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        v = v * 10 + (s[i] - '0');
        if (v > 100000L) {
            return 0;
        }
    }
    v = neg ? -v : v;
    if (v < lo || v > hi) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int to_bool(const char *s, int n, int *out)
{
    if (same(s, n, "on") || same(s, n, "1") || same(s, n, "yes") || same(s, n, "true")) {
        *out = 1;
        return 1;
    }
    if (same(s, n, "off") || same(s, n, "0") || same(s, n, "no") || same(s, n, "false")) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int to_detail(const char *s, int n, int *out)
{
    if (same(s, n, "low")) {
        *out = 0;
    } else if (same(s, n, "medium")) {
        *out = 1;
    } else if (same(s, n, "high")) {
        *out = 2;
    } else {
        return 0;
    }
    return 1;
}

/* "640x480", with spaces about the x allowed; both sides 64..4096. */
static int to_mode(const char *s, int n, int *w, int *h)
{
    int x;
    int a;
    int b;
    int lw;
    int rs;

    for (x = 0; x < n && s[x] != 'x' && s[x] != 'X'; x++) {
    }
    if (x >= n) {
        return 0;
    }
    lw = x;
    while (lw > 0 && is_space(s[lw - 1])) {
        lw--;
    }
    rs = x + 1;
    while (rs < n && is_space(s[rs])) {
        rs++;
    }
    if (!to_int(s, lw, 64, 4096, &a) || !to_int(s + rs, n - rs, 64, 4096, &b)) {
        return 0;
    }
    *w = a;
    *h = b;
    return 1;
}

/* A key by the name the bind screen shows (input_key_name's), or a number of a virtual key. 0 when it is none. */
static int to_key(const char *s, int n)
{
    char name[24];
    int vk;
    int number;
    int k;

    if (to_int(s, n, 1, 254, &number)) {
        return number;
    }
    for (vk = 1; vk < 255; vk++) {
        input_key_name(vk, name, (int)sizeof(name));
        for (k = 0; name[k]; k++) {
        }
        if (k > 0 && k == n && same(s, n, name)) {
            return vk;
        }
    }
    return 0;
}

/* ---- Parse ---- */

/* Sets the field the key names to the value; 1 when both were good. */
static int set_field(Config *c, const char *key, int kn, const char *v, int vn)
{
    int n;
    int w;
    int h;

    if (kn > 4 && same(key, 4, "key.")) {
        int action;

        for (action = 0; action < INPUT_ACTIONS; action++) {
            if (g_action_names[action] && same(key + 4, kn - 4, g_action_names[action])) {
                int vk;

                vk = to_key(v, vn);
                if (!vk) {
                    return 0;
                }
                c->key[action] = vk;
                return 1;
            }
        }
        return 0;
    }
    if (same(key, kn, "mouse")) {
        if (same(v, vn, "cursor")) {
            c->mouse_cursor = 1;
            return 1;
        }
        if (same(v, vn, "view")) {
            c->mouse_cursor = 0;
            return 1;
        }
        return 0;
    }
    if (same(key, kn, "resolution")) {
        if (!to_mode(v, vn, &w, &h)) {
            return 0;
        }
        c->mode_w = w;
        c->mode_h = h;
        return 1;
    }
    if (same(key, kn, "fov")) { return to_int(v, vn, 45, 120, &c->fov); }
    if (same(key, kn, "far_clip")) { return to_int(v, vn, 16, 16384, &c->far_clip); }
    if (same(key, kn, "ambient_light")) { return to_int(v, vn, 0, 255, &c->ambient_light); }
    if (same(key, kn, "light_range")) { return to_int(v, vn, 1, 8, &c->light_range); }
    if (same(key, kn, "flora_density")) { return to_int(v, vn, 0, 100, &c->flora_density); }
    if (same(key, kn, "flora_sight")) { return to_int(v, vn, 0, 16384, &c->flora_sight); }
    if (same(key, kn, "sfx_volume")) { return to_int(v, vn, 0, 100, &c->sfx_volume); }
    if (same(key, kn, "music_volume")) { return to_int(v, vn, 0, 100, &c->music_volume); }
    if (same(key, kn, "texture_detail")) { return to_detail(v, vn, &c->texture_detail); }
    if (same(key, kn, "sprite_detail")) { return to_detail(v, vn, &c->sprite_detail); }
    if (same(key, kn, "fog")) { return to_bool(v, vn, &c->fog); }
    if (same(key, kn, "crisp_sprites")) { return to_bool(v, vn, &c->crisp_sprites); }
    if (same(key, kn, "vsync")) { return to_bool(v, vn, &c->vsync); }
    if (same(key, kn, "dungeon_lights")) { return to_bool(v, vn, &c->dungeon_lights); }
    if (same(key, kn, "light_shadow")) { return to_bool(v, vn, &c->light_shadow); }
    if (same(key, kn, "weather_view")) { return to_bool(v, vn, &c->weather_view); }
    if (same(key, kn, "flora")) { return to_bool(v, vn, &c->flora); }
    if (same(key, kn, "full_hud")) { return to_bool(v, vn, &c->full_hud); }
    if (same(key, kn, "head_bob")) { return to_bool(v, vn, &c->head_bob); }
    if (same(key, kn, "toggle_crouch")) { return to_bool(v, vn, &c->toggle_crouch); }
    (void)n;
    return 0;
}

/*
 * Calls each line's key and value, trimmed: line numbers from 1; a blank or comment line is not passed.
 * sink returns 1 when it used the line.
 */
static int each_line(const char *text, int n, Config *c, int (*sink)(Config *, const char *, int, const char *, int),
                     int *first_bad)
{
    int bad;
    int line;
    int at;

    bad = 0;
    line = 0;
    at = 0;
    if (first_bad) {
        *first_bad = 0;
    }
    while (at < n) {
        int start;
        int end;
        int eq;
        int ks;
        int ke;
        int vs;
        int ve;

        start = at;
        while (at < n && text[at] != '\n') {
            at++;
        }
        end = at;
        if (at < n) {
            at++;
        }
        line++;
        while (start < end && is_space(text[start])) {
            start++;
        }
        while (end > start && is_space(text[end - 1])) {
            end--;
        }
        if (start >= end || text[start] == '#') {
            continue;
        }
        for (eq = start; eq < end && text[eq] != '='; eq++) {
        }
        ks = start;
        ke = eq;
        while (ke > ks && is_space(text[ke - 1])) {
            ke--;
        }
        vs = eq < end ? eq + 1 : end;
        while (vs < end && is_space(text[vs])) {
            vs++;
        }
        ve = end;
        /* A note after the value: a # that follows a space. No key's name holds one. */
        {
            int k;

            for (k = vs; k < ve; k++) {
                if (text[k] == '#' && k > vs && is_space(text[k - 1])) {
                    ve = k;
                    break;
                }
            }
            while (ve > vs && is_space(text[ve - 1])) {
                ve--;
            }
        }
        if (eq >= end || ke <= ks || vs >= ve || !sink(c, text + ks, ke - ks, text + vs, ve - vs)) {
            bad++;
            if (first_bad && !*first_bad) {
                *first_bad = line;
            }
        }
    }
    return bad;
}

int config_parse(const char *text, int n, Config *c, int *first_bad)
{
    return each_line(text, n, c, set_field, first_bad);
}

/* For the peek: only the resolution line is read, and every other line is let by. */
static int peek_field(Config *c, const char *key, int kn, const char *v, int vn)
{
    if (same(key, kn, "resolution")) {
        return to_mode(v, vn, &c->mode_w, &c->mode_h);
    }
    return 1;
}

int config_peek_mode(const char *text, int n, int *w, int *h)
{
    Config c;
    int bad;

    c.mode_w = 0;
    c.mode_h = 0;
    bad = each_line(text, n, &c, peek_field, 0);
    (void)bad;
    if (c.mode_w < 64 || c.mode_h < 64) {
        return 0;
    }
    *w = c.mode_w;
    *h = c.mode_h;
    return 1;
}

/* ---- Format ---- */

typedef struct Out {
    char *s;
    int cap;
    int n;
    int full;
} Out;

static void put(Out *o, const char *s)
{
    while (*s) {
        if (o->n >= o->cap) {
            o->full = 1;
            return;
        }
        o->s[o->n++] = *s++;
    }
}

static void put_int(Out *o, int v)
{
    char digits[12];
    int k;

    if (v < 0) {
        put(o, "-");
        v = -v;
    }
    k = 0;
    do {
        digits[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && k < 11);
    while (k > 0) {
        char one[2];

        one[0] = digits[--k];
        one[1] = 0;
        put(o, one);
    }
}

static void line_int(Out *o, const char *key, int v, const char *note)
{
    put(o, key);
    put(o, "=");
    put_int(o, v);
    if (note) {
        put(o, "    # ");
        put(o, note);
    }
    put(o, "\r\n");
}

static void line_bool(Out *o, const char *key, int v)
{
    put(o, key);
    put(o, v ? "=on\r\n" : "=off\r\n");
}

static void line_detail(Out *o, const char *key, int v)
{
    put(o, key);
    put(o, v <= 0 ? "=low\r\n" : (v == 1 ? "=medium\r\n" : "=high\r\n"));
}

int config_format(const Config *c, char *out, int cap)
{
    Out o;
    char name[24];
    int i;

    o.s = out;
    o.cap = cap;
    o.n = 0;
    o.full = 0;
    put(&o, "# gfall.cfg: GFALL's settings, one key=value a line. The Settings screens write this file when you press\r\n");
    put(&o, "# Apply, and it is read at the start. Edit a value here if you like; a line it cannot use is skipped.\r\n");
    put(&o, "# A comment starts a line with #. on and off may also be written 1 and 0, yes and no.\r\n");
    put(&o, "\r\n# The screen. A resolution shows at the next start: 320x240, 400x300, 512x384, 640x480 (800x600 with 4 MB).\r\n");
    put(&o, "resolution=");
    put_int(&o, c->mode_w);
    put(&o, "x");
    put_int(&o, c->mode_h);
    put(&o, "\r\n");
    line_int(&o, "fov", c->fov, "horizontal degrees, 45 to 120");
    line_int(&o, "far_clip", c->far_clip, "world units, 16 to 16384");
    line_detail(&o, "texture_detail", c->texture_detail);
    line_detail(&o, "sprite_detail", c->sprite_detail);
    line_bool(&o, "fog", c->fog);
    line_bool(&o, "crisp_sprites", c->crisp_sprites);
    line_bool(&o, "vsync", c->vsync);
    line_bool(&o, "dungeon_lights", c->dungeon_lights);
    line_int(&o, "ambient_light", c->ambient_light, "0 to 255");
    line_int(&o, "light_range", c->light_range, "1 to 8");
    line_bool(&o, "light_shadow", c->light_shadow);
    line_bool(&o, "weather_view", c->weather_view);
    line_bool(&o, "flora", c->flora);
    line_int(&o, "flora_density", c->flora_density, "0 to 100");
    line_int(&o, "flora_sight", c->flora_sight, "world units, 0 for each place's own");
    put(&o, "\r\n# Sound, in percent.\r\n");
    line_int(&o, "sfx_volume", c->sfx_volume, 0);
    line_int(&o, "music_volume", c->music_volume, 0);
    put(&o, "\r\n# Play. mouse is cursor (the classic pointer) or view (mouse look).\r\n");
    put(&o, c->mouse_cursor ? "mouse=cursor\r\n" : "mouse=view\r\n");
    line_bool(&o, "full_hud", c->full_hud);
    line_bool(&o, "head_bob", c->head_bob);
    line_bool(&o, "toggle_crouch", c->toggle_crouch);
    put(&o, "\r\n# Keys, by the names the Keybindings screen shows (Escape is the menu and cannot be changed).\r\n");
    for (i = 0; i < INPUT_ACTIONS; i++) {
        if (!g_action_names[i]) {
            continue;
        }
        put(&o, "key.");
        put(&o, g_action_names[i]);
        put(&o, "=");
        input_key_name(c->key[i], name, (int)sizeof(name));
        put(&o, name);
        put(&o, "\r\n");
    }
    return o.full ? -1 : o.n;
}
