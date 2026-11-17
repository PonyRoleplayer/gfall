#include "screen.h"
#include "callbacks.h"
#include "game/dagger/play/character/chargen.h"
#include "game/dagger/play/items/possessions.h"

void gfall_screen_install_draw(void)
{
    weapon_set_draw(gfall_weapon_draw);
    magic_set_draw(gfall_cast_draw);
    magic_set_hud(gfall_magic_hud_draw);
    possessions_set_measure(gfall_text_width);
}

void gfall_screen_install_background(void)
{
    chargen_background(gfall_background);
}
