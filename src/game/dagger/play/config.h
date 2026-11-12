#ifndef OGFALL_CONFIG_H
#define OGFALL_CONFIG_H

/* OGFALL's text preferences, independent of saves. Read docs/ogfall.md.
 * Keys are Windows virtual keys; no DOS keyboard codes enter this schema. */
#include "game/dagger/play/steer.h"

#define OG_CONFIG_FILE "ogfall\\settings.cfg"
typedef struct OgConfig {
    int flags, mouse_mode;
    int sound_volume, music_volume, detail;
    int sensitivity_x, sensitivity_y, turn_rate, reverse_y;
    int width, height, fov;
    int key[OG_ACTS];
} OgConfig;

void og_config_defaults(OgConfig *config);
const OgConfig *og_config(void);
/* Validated whole replacement. apply synchronizes engine audio and view flags. */
int og_config_set(const OgConfig *config);
void og_config_apply(void);
int og_config_read(void);
int og_config_write(void);
int og_config_parse(OgConfig *out, const char *text);
int og_config_format(const OgConfig *config, char *text, int capacity);
const char *og_config_error(void);
const char *og_action_name(int action);
const char *og_action_token(int action);
int og_key_allowed(int key);

#endif
