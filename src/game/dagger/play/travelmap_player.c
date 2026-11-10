#include "game/dagger/play/travelmap.h"
#include "game/dagger/play/atlas.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/ride.h"
#include "game/dagger/play/words.h"
#include "engine/core/mem.h"

/* GFALL's rules for the map presenter. Other adapters supply their own. */
static Player *g_player;
static Words g_words;
static unsigned gold(void) { return g_player ? g_player->pack.gold : 0; }
static void plan(int sx, int sy, int ex, int ey, int reckless, int inns, int ship, Journey *out)
{
    journey_plan(sx,sy,ex,ey,reckless,inns,ship,ride_journey(g_player),0,out);
    if (ship && bank_ship() != BANK_SHIP_NONE) { journey_own_ship(out); }
}
static void question(const char *place, char *out, int capacity)
{
    mem_zero(&g_words,sizeof(g_words));
    g_words.pc = g_player ? &g_player->sheet : 0; g_words.place = place;
    words_use(&g_words); words_say(31,out,capacity);
}
int travelmap_open(Player *player, int px, int py, int region)
{
    TravelMapRules rules;
    g_player = player;
    rules.gold = gold; rules.known = atlas_known; rules.plan = plan; rules.question = question; rules.warning = 0;
    return travelmap_open_rules(&rules,px,py,region);
}
