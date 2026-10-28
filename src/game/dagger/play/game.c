#include "game/dagger/play/game.h"
#include "game/dagger/play/overlay.h"
#include "game/dagger/play/player.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/world.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"
#include "engine/gfx/gfx.h"
#include "engine/scene/interact.h"
#include "engine/ui/hud.h"
#include "engine/ui/pic.h"

/* See game.h. */

/* Milliseconds a frame may spend reading the creatures that come near (docs/entities.md). */
#define CREATURE_BUDGET_MS 2.0f

static struct World *g_world;
static const Level *g_level;
static Player g_player;
static int g_paused;
/* Where the place starts: where a death puts the player back until there is a menu (P3). */
static float g_start[5];

/*
 * What the use key does that the engine's doors and levers do not: a
 * passage, until there is somewhere to go (P6), and a person, until
 * there is talk (P10).
 */
static int on_use(const Scene *scene, const Usable *u, int index)
{
    (void)scene;
    (void)index;
    if (u->kind == USE_PASSAGE) {
        interact_say(u->what == DOOR_EXIT ? "The way out. The world beyond comes later."
                                          : "It leads on. Not yet.");
        return 1;
    }
    if (u->kind == USE_TALK) {
        interact_say("They have nothing to say yet.");
        return 1;
    }
    return 0;
}

void game_setup(void)
{
    /* Escape is the game's menu, not a quit; the left button attacks, so use is the game's own key. */
    input_set_escape_quits(0);
    gfx_set_use_key(0);
    /* The engine's position corner is a viewer's; the game has its compass. */
    hud_set(0);
    pic_set_room(GAME_PICTURE_KB);
    overlay_load();
}

void game_enter(struct World *world, const Level *level, Camera *cam)
{
    float x;
    float y;
    float z;
    float yaw;
    float pitch;

    g_world = world;
    g_level = level;
    world_start(world, &x, &y, &z, &yaw, &pitch);
    g_start[0] = x;
    g_start[1] = y;
    g_start[2] = z;
    g_start[3] = yaw;
    g_start[4] = pitch;
    {
        Character sheet;

        /* Until character creation (P8): a Breton warrior, at the rolls' middles. */
        if (!character_new(&sheet, CHARACTER_START_RACE, CHARACTER_START_CLASS, 0, "Player")) {
            mem_zero(&sheet, sizeof(sheet));
        }
        player_init(&g_player, &sheet);
    }
    player_place(&g_player, x, y, z, yaw, pitch);
    camera_set(cam, x, y, z, yaw, pitch);
    interact_set_handler(on_use);
    gfx_set_extra(overlay_draw);
    g_paused = 0;
    gfx_set_pause(0);
    overlay_set_paused(0);
}

static void set_paused(int on)
{
    g_paused = on;
    gfx_set_pause(on);
    overlay_set_paused(on);
}

int game_frame(Camera *cam, const Input *in, float dt)
{
    prof_begin(PROF_PLAY);
    if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        set_paused(!g_paused);
    }
    /* Paused, Q quits: there is no menu yet. */
    if (g_paused && in->move_y > 0) {
        prof_end(PROF_PLAY);
        return 0;
    }
    /* The console stands the world still too (gfx_paused): the player stands with it. */
    if (!g_paused && !gfx_paused()) {
        if (in->pressed & INPUT_BIT(INPUT_USE)) {
            interact_request_use();
        }
        player_step(&g_player, in, &g_level->scene, dt);
        {
            float speed;
            float height;

            /* A landing past classic's 5 m hurts. */
            if (body_landed(&g_player.body, &speed, &height)) {
                int hurt;

                hurt = rules_fall_damage(height / RULES_UNITS_PER_M);
                if (hurt > 0 && character_hurt(&g_player.sheet, hurt)) {
                    /* Death, until combat brings its own (P4) and the menu a reload (P3): back at the start, whole. */
                    interact_say("You have died.");
                    character_rest_full(&g_player.sheet);
                    player_place(&g_player, g_start[0], g_start[1], g_start[2], g_start[3], g_start[4]);
                } else if (hurt > 0) {
                    interact_say("That hurt.");
                }
            }
        }
        player_camera(&g_player, cam);
        world_creatures_step(g_world, cam, 0.0f, CREATURE_BUDGET_MS);
    }
    overlay_set_yaw(g_player.yaw);
    overlay_set_vitals(g_player.sheet.health, g_player.sheet.health_max, g_player.sheet.fatigue,
                       g_player.sheet.fatigue_max, g_player.sheet.magicka, g_player.sheet.magicka_max);
    prof_end(PROF_PLAY);
    return 1;
}

void game_shutdown(void)
{
    interact_set_handler(0);
    gfx_set_extra(0);
    gfx_set_pause(0);
    overlay_free();
}
