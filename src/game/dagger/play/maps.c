#include "game/dagger/play/maps.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/play/weapon.h"
static OgMapsHooks g_hooks;
static int g_open;
void og_maps_install(const OgMapsHooks *hooks) { g_hooks = *hooks; }
int og_maps_up(void) { return g_open; }
int og_maps_open(int travel)
{
    if (g_open || !og_state()->ready || og_character()->health <= 0 || !g_hooks.open) { return 0; }
    if (!g_hooks.open(travel)) { og_msg_set(travel ? "You must be outside to travel." : "No map is available here."); return 0; }
    if (!og_state_mode_push(OG_GAME_READING)) { if (g_hooks.close) { g_hooks.close(); } return 0; }
    g_open = 1; og_weapon_cancel(); return 1;
}
void og_maps_close(void)
{
    if (!g_open) { return; }
    if (g_hooks.close) { g_hooks.close(); }
    g_open = 0; og_state_mode_pop();
}
void og_maps_frame(const OgIn *input) { if (g_open && g_hooks.frame && g_hooks.frame(input)) { og_maps_close(); } }
void og_maps_overlay(void) { if (g_open && g_hooks.overlay) { g_hooks.overlay(); } }
