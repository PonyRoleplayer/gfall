#ifndef GFALL_CONFIG_H
#define GFALL_CONFIG_H

/* GFALL's text preferences, independent of saves. Read docs/game.md.
 * Keys are Windows virtual keys; no DOS keyboard codes enter this schema. */
#include "game/dagger/play/input/steer.h"

#define CONFIG_FILE "gfall\\settings.cfg"
/* Config.flags: bit 0 full screen, bit 1 head bob (the original's two view switches, which a save keeps), and bit 2, ours: the opening's logo and movies are not played at start (`skip_opening=on`; a key still skips each one when played). */
#define CONFIG_FLAG_SKIP_OPENING 4
#define CONFIG_FLAGS_MOST 7
typedef struct Config {
    int flags, mouse_mode;
    int sound_volume, music_volume, detail;
    int sensitivity_x, sensitivity_y, turn_rate, reverse_y;
    int width, height, fov;
    int key[ACTS];
} Config;

void config_defaults(Config *config);
const Config *game_config(void);
/* Validated whole replacement. apply synchronizes engine audio and view flags. */
int config_set(const Config *config);
void config_apply(void);
int config_read(void);
int config_write(void);
int config_parse(Config *out, const char *text);
int config_format(const Config *config, char *text, int capacity);
const char *config_error(void);
const char *action_name(int action);
const char *action_token(int action);
int key_allowed(int key);

#endif
