#include "game/dagger/play/game.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/commands.h"
#include "game/dagger/play/keys.h"
#include "game/dagger/play/menu.h"
#include "game/dagger/play/overlay.h"
#include "game/dagger/play/player.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/weapon.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/dunstream.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"
#include "engine/gfx/gfx.h"
#include "engine/scene/interact.h"
#include "engine/scene/walker.h"
#include "engine/scene/weather.h"
#include "engine/ui/hud.h"
#include "engine/ui/pic.h"

/* See game.h. */

static const Level *g_level;
static Player g_player;
static int g_playing;
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

/* What the game draws over the frame: the weapon and the HUD in play, then the open menu and its pointer. */
static void draw(void)
{
    if (g_playing) {
        int pic;
        int left;
        int top;
        int w;
        int h;

        /* Under the HUD, as classic: the compass and the bars over the blade. */
        if (weapon_shown(&pic, &left, &top, &w, &h)) {
            screen_draw(pic, left, top, w, h);
        }
        overlay_draw();
    }
    menu_draw();
}

/* A menu open over play stills the world and gives the mouse to the pointer; closed, back to play. */
static void menu_over_play(int which)
{
    menu_open(which);
    gfx_set_pause(which != MENU_NONE);
    input_set_pointer(which != MENU_NONE);
}

void game_setup(void)
{
    /* Escape is the game's menu, not a quit; the left button attacks, so use is the game's own key. */
    input_set_escape_quits(0);
    gfx_set_use_key(0);
    keys_load();
    /* The engine's position corner is a viewer's; the game has its compass. */
    hud_set(0);
    /*
     * Outdoors the weather sets how far one sees and the fog (docs/weather.md,
     * the simulated view): a clear day long, a storm short. The game's on;
     * LEVELFALL keeps it off to inspect a place at any distance.
     */
    weather_set_sim(1);
    pic_set_room(GAME_PICTURE_KB);
    overlay_load();
    menu_setup();
    gfx_set_extra(draw);
    commands_setup();
    g_playing = 0;
    menu_open(MENU_TITLE);
    input_set_pointer(1);
}

int game_playing(void)
{
    return g_playing;
}

void game_enter(const Level *level, Camera *cam, float x, float y, float z, float yaw, float pitch)
{
    g_level = level;
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
    /*
     * Until character creation (P8) gives classic's starting kit: the
     * warrior's long blade, an iron longsword, in hand.
     */
    {
        Item sword;

        if (item_make(&sword, ITEM_LONGSWORD, MATERIAL_IRON)) {
            inventory_equip(&g_player.pack, inventory_add(&g_player.pack, &sword));
        }
    }
    commands_player(&g_player);
    combat_reset();
    player_place(&g_player, x, y, z, yaw, pitch);
    camera_set(cam, x, y, z, yaw, pitch);
    /*
     * The engine's walker gives the walking to the game's player: it is
     * not stepped, and the console's tcl is the game's collision switch
     * (docs/collision.md, A game's player). On by default.
     */
    walker_set_game(&g_player.body);
    walker_set_on(1);
    interact_set_handler(on_use);
    g_playing = 1;
    menu_over_play(MENU_NONE);
}

int game_frame(Camera *cam, const Input *in, float dt)
{
    int asked;
    /* The click that closed a menu is not a swing. */
    int closed;

    prof_begin(PROF_PLAY);
    /* A screen before any place: the title or the start menu. */
    if (!g_playing) {
        asked = menu_frame(in);
        prof_end(PROF_PLAY);
        return asked == MENU_QUIT ? GAME_QUIT : (asked == MENU_START ? GAME_START : GAME_GO_ON);
    }
    closed = 0;
    if (menu_current() != MENU_NONE) {
        closed = 1;
        asked = menu_frame(in);
        if (asked == MENU_QUIT) {
            prof_end(PROF_PLAY);
            return GAME_QUIT;
        }
        if (asked == MENU_CLOSED) {
            menu_over_play(MENU_NONE);
        }
    } else if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        menu_over_play(MENU_OPTIONS);
    }
    /* The console stands the world still too (gfx_paused): the player stands with it. */
    if (menu_current() == MENU_NONE && !gfx_paused()) {
        if (in->pressed & INPUT_BIT(INPUT_USE)) {
            interact_request_use();
        }
        /* The weapon in hand follows the pack (equip, additem); a click swings it, at random for now. */
        weapon_hold(inventory_weapon(&g_player.pack));
        if (!closed && (in->pressed & INPUT_BIT(INPUT_ATTACK))) {
            weapon_attack();
        }
        weapon_step(dt, g_player.sheet.attribute[ATTR_SPEED]);
        {
            float tx;
            float ty;
            float tz;
            float tyaw;

            /* A teleport the last frame reached (docs/interact.md): the body goes, the eye where it says. */
            if (interact_teleport(&tx, &ty, &tz, &tyaw)) {
                body_place(&g_player.body, tx, ty - g_player.body.eye, tz);
                g_player.yaw = tyaw;
            }
        }
        /* tcl 1 (walker_on): the body walks and collides; tcl 0: it flies through everything. */
        player_step(&g_player, in, &g_level->scene, dt, walker_on());
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
        /* The swing's hit frame strikes what is in reach of the eye; flinches run out. */
        if (weapon_hit()) {
            combat_strike(&g_player.sheet, inventory_weapon(&g_player.pack), weapon_swing(), &g_level->scene, cam->x,
                          cam->y, cam->z, cam->yaw, cam->pitch);
        }
        combat_frame(dt);
        /* The blocks round the player in and out, their creatures read with them, the lights cut (docs/streaming.md). */
        dunstream_update(cam, GAME_STREAM_MS);
    }
    overlay_set_yaw(g_player.yaw);
    overlay_set_vitals(g_player.sheet.health, g_player.sheet.health_max, g_player.sheet.fatigue,
                       g_player.sheet.fatigue_max, g_player.sheet.magicka, g_player.sheet.magicka_max);
    prof_end(PROF_PLAY);
    return GAME_GO_ON;
}

void game_shutdown(void)
{
    interact_set_handler(0);
    commands_player(0);
    weapon_free();
    walker_set_game(0);
    gfx_set_extra(0);
    gfx_set_pause(0);
    input_set_pointer(0);
    menu_shutdown();
    overlay_free();
}
