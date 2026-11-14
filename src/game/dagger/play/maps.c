#include "game/dagger/play/maps.h"
#include "game/dagger/play/curse.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/play/weapon.h"
static MapsHooks g_hooks;
static int g_open;
void maps_install(const MapsHooks *hooks) { g_hooks = *hooks; }
int maps_up(void) { return g_open; }
int maps_open(int travel)
{
    if (g_open || !game_state()->ready || character_state()->health <= 0 || !g_hooks.open) { return 0; }
    /* travel_map_open: a vampire, or a class hurt by sunlight, cannot start a fast travel by day (a teleport is not one). */
    if (travel && travel != 100 && (curse_form() == 8 || (game_state()->career.flags & 16)) && game_state()->minutes % 1440u >= 360u && game_state()->minutes % 1440u < 1080u) {
        msg_set("You cannot initiate fast travel during the day."); return 0;
    }
    if (!g_hooks.open(travel)) { msg_set(travel ? "You must be outside to travel." : "No map is available here."); return 0; }
    if (!state_mode_push(GAME_READING)) { if (g_hooks.close) { g_hooks.close(); } return 0; }
    g_open = 1; weapon_cancel(); return 1;
}
void maps_close(void)
{
    if (!g_open) { return; }
    if (g_hooks.close) { g_hooks.close(); }
    g_open = 0; state_mode_pop();
}
void maps_frame(const PlayIn *input) { if (g_open && g_hooks.frame && g_hooks.frame(input)) { maps_close(); } }
void maps_overlay(void) { if (g_open && g_hooks.overlay) { g_hooks.overlay(); } }
