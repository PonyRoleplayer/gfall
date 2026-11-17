#ifndef GFALL_SCREEN_CALLBACKS_H
#define GFALL_SCREEN_CALLBACKS_H

#include "game/dagger/play/combat/weapon.h"
#include "game/dagger/play/magic/magic.h"

void gfall_weapon_draw(const WeaponView *view, float flash);
void gfall_cast_draw(int element, int frame);
void gfall_magic_hud_draw(const MagicState *state);
int gfall_text_width(const char *text);
int gfall_background(int klass, int question, int answer);

#endif
