#include "game/dagger/play/ride.h"
#include "game/dagger/play/journey.h"
#include "game/dagger/tables/items.h"

/* See ride.h. */

static int g_frame;
static float g_time;

const char *ride_toggle(Player *p, int outdoors)
{
    if (p->riding != RIDE_FOOT) {
        p->riding = RIDE_FOOT;
        return "You get down.";
    }
    if (!outdoors) {
        return "You are travelling on foot.";
    }
    /* The horse first, as TransportManager.ToggleMount. */
    if (inventory_has(&p->pack, ITEM_HORSE)) {
        p->riding = RIDE_HORSE;
        return 0;
    }
    if (inventory_has(&p->pack, ITEM_CART)) {
        p->riding = RIDE_CART;
        return 0;
    }
    return "You have no horse or cart.";
}

void ride_get_down(Player *p)
{
    p->riding = RIDE_FOOT;
    g_frame = 0;
    g_time = 0.0f;
}

int ride_frame(int moving, float dt)
{
    if (!moving) {
        g_frame = 0;
        g_time = 0.0f;
        return 0;
    }
    g_time += dt;
    while (g_time >= RIDE_FRAME_TIME) {
        g_time -= RIDE_FRAME_TIME;
        g_frame = (g_frame + 1) % RIDE_FRAMES;
    }
    return g_frame;
}

int ride_journey(const Player *p)
{
    if (inventory_has(&p->pack, ITEM_HORSE)) {
        return JOURNEY_HORSE;
    }
    return inventory_has(&p->pack, ITEM_CART) ? JOURNEY_CART : JOURNEY_FOOT;
}
