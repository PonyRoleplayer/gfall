#include "game/dagger/play/ride.h"
#include "game/dagger/play/journey.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/play/sfx.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/formats/sndfile.h"

/* See ride.h. */

static int g_frame;
static float g_time;
/* The loop playing and its clip; seconds to the next whinny. */
static int g_loop;
static int g_loop_clip = -1;
static float g_neigh;

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
    ride_quiet();
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

int ride_hoof_clip(int riding, int slow)
{
    if (riding == RIDE_HORSE) {
        return slow ? SND_HORSE_CLOP : SND_HORSE_CLOP_PACE;
    }
    return riding == RIDE_CART ? SND_HORSE_AND_CART : -1;
}

void ride_sound(int riding, int moving, int slow, float dt)
{
    int clip;
    float volume;

    clip = moving ? ride_hoof_clip(riding, slow) : -1;
    if (clip != g_loop_clip) {
        if (g_loop) {
            sfx_stop(g_loop);
            g_loop = 0;
        }
        g_loop_clip = clip;
        if (clip >= 0) {
            g_loop = sfx_loop(clip, 1.0f);
        }
    }
    /* Slow hooves at half volume (RidingVolumeScale x 0.5). */
    volume = slow ? 0.5f : 1.0f;
    if (g_loop) {
        sfx_gain(g_loop, volume);
    }
    if (riding == RIDE_FOOT) {
        g_neigh = 0.0f;
        return;
    }
    /* Mounted, moving or not: a whinny now and then, the first at once as DFU's neighTime starts at 0. */
    g_neigh -= dt;
    if (g_neigh <= 0.0f) {
        sfx_play(SND_HORSE);
        g_neigh = (float)(2 + combat_roll(38));
    }
}

void ride_quiet(void)
{
    if (g_loop) {
        sfx_stop(g_loop);
        g_loop = 0;
    }
    g_loop_clip = -1;
}

int ride_journey(const Player *p)
{
    if (inventory_has(&p->pack, ITEM_HORSE)) {
        return JOURNEY_HORSE;
    }
    return inventory_has(&p->pack, ITEM_CART) ? JOURNEY_CART : JOURNEY_FOOT;
}
