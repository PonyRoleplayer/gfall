#ifndef GFALL_CONFIG_H
#define GFALL_CONFIG_H

/* GFALL's text preferences, independent of saves. Read docs/gfall.md.
 * Keys are Windows virtual keys; no DOS keyboard codes enter this schema. */
#include "game/dagger/play/steer.h"

#define CONFIG_FILE "gfall\\settings.cfg"
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
