#include "game/dagger/play/game.h"
#include "game/dagger/play/bag.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/commands.h"
#include "game/dagger/play/create.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/keys.h"
#include "game/dagger/play/loot.h"
#include "game/dagger/play/menu.h"
#include "game/dagger/play/overlay.h"
#include "game/dagger/play/pile.h"
#include "game/dagger/play/player.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/screens.h"
#include "game/dagger/play/weapon.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/dunstream.h"
#include "engine/core/mem.h"
#include "engine/core/prof.h"
#include "engine/gfx/gfx.h"
#include "engine/scene/interact.h"
#include "engine/scene/keep.h"
#include "engine/scene/stream.h"
#include "engine/scene/clock.h"
#include "engine/scene/walker.h"
#include "engine/scene/weather.h"
#include "engine/ui/cmd.h"
#include "engine/ui/hud.h"
#include "engine/ui/pic.h"

/* See game.h. */

static const Level *g_level;
static Player g_player;
static int g_playing;
/* The character and kit the next place starts with: made in creation, or the default. */
static Character g_new_sheet;
static Inventory g_new_pack;
/* A save read, for the next place to start from; and 1 while it waits to. */
static SaveState g_loaded;
static int g_load_waiting;
/* The clock's day when this game began: skills' six hours count game minutes from it. */
static int g_day_zero;
/* The last game minute that tired the player. */
static int g_last_minute;

/*
 * What the use key does that the engine's doors and levers do not: a
 * passage, until there is somewhere to go (P8), and a person, until
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

/* The player died (a fall, a blow): classic's death screen over the stilled world. */
static void die(void)
{
    interact_say("You have died.");
    menu_open(MENU_DEATH);
    gfx_set_pause(1);
    input_set_pointer(0);
}

/* What the game draws over the frame: the weapon and the HUD in play, then the open menu and its pointer. */
static void draw(void)
{
    /* A composed screen fills the screen: only it and the pointer. */
    if (screens_is_open()) {
        screens_draw();
        return;
    }
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

/* The inventory over the stilled world: the player's own, or with what they are searching beside it. */
static void bag_over_play(Entity *searched)
{
    screens_open_bag(&g_player, searched);
    gfx_set_pause(screens_is_open());
    input_set_pointer(screens_is_open());
}

/* Rest over the stilled world, unless a creature awake forbids it (DFU's rest window). */
static void rest_over_play(void)
{
    if (foes_awake() > 0) {
        interact_say("You cannot rest with enemies nearby.");
        return;
    }
    screens_open_rest(&g_player);
    gfx_set_pause(screens_is_open());
    input_set_pointer(screens_is_open());
}

/* Game minutes since this game began. */
static float game_minutes(void)
{
    return (float)(clock_day() - g_day_zero) * CLOCK_DAY + clock_minute();
}

/* The game as it stands, into the one slot. What is loaded goes into the keep table first. */
static int save_game(void)
{
    SaveState state;
    int cx;
    int cz;

    stream_keep_now();
    stream_origin(&cx, &cz);
    state.feet[0] = g_player.body.feet[0] + (float)cx * DUNSTREAM_BLOCK;
    state.feet[1] = g_player.body.feet[1];
    state.feet[2] = g_player.body.feet[2] + (float)cz * DUNSTREAM_BLOCK;
    state.yaw = g_player.yaw;
    state.pitch = g_player.pitch;
    state.minute = clock_minute();
    state.day = clock_day();
    state.day_zero = g_day_zero;
    state.seed = loot_seed();
    state.sheet = g_player.sheet;
    state.pack = g_player.pack;
    return save_write(&state);
}

/* The save read, to start from at the next place. 1 when there was one. */
static int load_game(void)
{
    if (!save_exists() || !save_read(&g_loaded)) {
        return 0;
    }
    g_new_sheet = g_loaded.sheet;
    g_new_pack = g_loaded.pack;
    g_load_waiting = 1;
    return 1;
}

/* A new character's game: what was changed or dropped in the last one is gone. */
static void forget_last_game(void)
{
    keep_clear();
    pile_new_game();
    loot_new_game(prof_now());
    g_load_waiting = 0;
}

/* The character sheet over the stilled world: a level due is taken there. */
static void sheet_over_play(void)
{
    screens_open_sheet(&g_player);
    gfx_set_pause(screens_is_open());
    input_set_pointer(screens_is_open());
}

/* The attack button this frame: a blade's gesture, or a bow's draw if there is an arrow to draw. */
static void attack_input(const Input *in, float dt)
{
    int pressed;

    pressed = (in->pressed & INPUT_BIT(INPUT_ATTACK)) != 0;
    if (pressed && weapon_is_bow() && inventory_arrows(&g_player.pack) < 0) {
        interact_say("You have no arrows.");
        return;
    }
    weapon_gesture(pressed, (in->held & INPUT_BIT(INPUT_ATTACK)) != 0, (in->released & INPUT_BIT(INPUT_ATTACK)) != 0, in->look_dx,
                   in->look_dy, dt);
}

/* The bow looses: an arrow gone, flying along the view. */
static void shoot_arrow(const Camera *cam, const float *eye)
{
    Basis basis;
    float dir[3];

    if (!inventory_spend_arrow(&g_player.pack)) {
        return;
    }
    camera_basis(cam, &basis);
    dir[0] = basis.fx * basis.cp;
    dir[1] = basis.sp;
    dir[2] = basis.fz * basis.cp;
    combat_shoot(&g_player.sheet, inventory_hand(&g_player.pack), &g_level->scene, g_player.body.feet, eye, dir);
}

/*
 * The use key: a body or a heap the view rests on is searched; anything
 * else is the engine's to use (doors, levers, passages). With nothing to
 * find it says so and opens nothing.
 */
static void use_key(const Camera *cam)
{
    Entity *searched;

    searched = loot_find(&g_level->scene, cam);
    if (!searched) {
        interact_request_use();
    } else if (!bag_finds_things(searched, g_player.sheet.level)) {
        interact_say("You find nothing.");
    } else {
        bag_over_play(searched);
    }
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
    cmd_var("panel", overlay_panel, overlay_set_panel, 0, 1, "classic's bottom panel of the HUD; panel alone flips it");
    cmd_toggles("panel");
    g_playing = 0;
    menu_open(MENU_TITLE);
    input_set_pointer(1);
}

int game_playing(void)
{
    return g_playing;
}

int game_world_shows(void)
{
    return g_playing && !screens_is_open() && menu_current() != MENU_CONTROLS;
}

void game_enter(const Level *level, Camera *cam, float x, float y, float z, float yaw, float pitch)
{
    g_level = level;
    /* Who creation made or the save holds, and what they carry (game_frame set these). */
    player_init(&g_player, &g_new_sheet);
    g_player.pack = g_new_pack;
    player_wear_changed(&g_player);
    {
        const CareerRace *race;

        race = career_race(g_player.sheet.race);
        if (race) {
            overlay_set_face(g_player.sheet.female ? race->heads_female : race->heads_male, g_player.sheet.face);
        }
    }
    g_day_zero = clock_day();
    g_last_minute = df_ifloor(game_minutes());
    if (g_load_waiting) {
        int cx;
        int cz;

        /* The save's place, in this stream's frame; its clock, its seed. */
        g_load_waiting = 0;
        stream_origin(&cx, &cz);
        x = g_loaded.feet[0] - (float)cx * DUNSTREAM_BLOCK;
        y = g_loaded.feet[1] + g_player.body.eye;
        z = g_loaded.feet[2] - (float)cz * DUNSTREAM_BLOCK;
        yaw = g_loaded.yaw;
        pitch = g_loaded.pitch;
        clock_set_minute(g_loaded.minute);
        clock_set_day(g_loaded.day);
        g_day_zero = g_loaded.day_zero;
        g_last_minute = df_ifloor(game_minutes());
        loot_new_game(g_loaded.seed);
    }
    commands_player(&g_player);
    combat_reset();
    foes_reset();
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
        /* Creation, once Start is chosen: it ends with a character for the first place. */
        if (screens_is_open()) {
            asked = screens_frame(in, dt);
            prof_end(PROF_PLAY);
            if (asked == SCREENS_MADE && create_result(&g_new_sheet, &g_new_pack)) {
                screens_close();
                forget_last_game();
                return GAME_START;
            }
            if (asked == SCREENS_SKIPPED && create_default(&g_new_sheet, &g_new_pack)) {
                screens_close();
                forget_last_game();
                return GAME_START;
            }
            return GAME_GO_ON;
        }
        asked = menu_frame(in);
        if (asked == MENU_LOAD) {
            if (load_game()) {
                menu_open(MENU_NONE);
                prof_end(PROF_PLAY);
                return GAME_START;
            }
            menu_say("There is no saved game.");
        }
        if (asked == MENU_START) {
            screens_open_create();
            /* Its pictures missing: the default character starts, as before creation was built. */
            if (!screens_is_open()) {
                create_default(&g_new_sheet, &g_new_pack);
                forget_last_game();
                prof_end(PROF_PLAY);
                return GAME_START;
            }
        }
        prof_end(PROF_PLAY);
        return asked == MENU_QUIT ? GAME_QUIT : GAME_GO_ON;
    }
    closed = 0;
    if (screens_is_open()) {
        closed = 1;
        if (screens_frame(in, dt) == SCREENS_CLOSED) {
            const char *said;

            gfx_set_pause(0);
            input_set_pointer(0);
            said = screens_said();
            if (said) {
                interact_say(said);
            }
        }
    } else if (menu_current() != MENU_NONE) {
        closed = 1;
        asked = menu_frame(in);
        if (asked == MENU_QUIT) {
            prof_end(PROF_PLAY);
            return GAME_QUIT;
        }
        if (asked == MENU_CLOSED) {
            menu_over_play(MENU_NONE);
        } else if (asked == MENU_SAVE) {
            menu_say(save_game() ? "Game saved." : "The game could not be saved.");
        } else if (asked == MENU_LOAD || asked == MENU_DEATH_OVER) {
            /* From the options, or dead: the last save, if there is one. */
            if (load_game()) {
                menu_open(MENU_NONE);
                prof_end(PROF_PLAY);
                return GAME_START;
            }
            if (asked == MENU_LOAD) {
                menu_say("There is no saved game.");
            } else {
                /* Dead with nothing saved: back to the start menu, the place left for the next start to replace. */
                g_playing = 0;
                gfx_set_pause(0);
                input_set_pointer(1);
                menu_open(MENU_MAIN);
                prof_end(PROF_PLAY);
                return GAME_GO_ON;
            }
        }
    } else if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        menu_over_play(MENU_OPTIONS);
    } else if ((in->pressed & INPUT_BIT(INPUT_INVENTORY)) && !gfx_paused()) {
        bag_over_play(0);
    } else if ((in->pressed & INPUT_BIT(INPUT_CHARACTER)) && !gfx_paused()) {
        sheet_over_play();
    } else if ((in->pressed & INPUT_BIT(INPUT_REST)) && !gfx_paused()) {
        rest_over_play();
    }
    /* The console stands the world still too (gfx_paused): the player stands with it. */
    if (menu_current() == MENU_NONE && !screens_is_open() && !gfx_paused()) {
        if (in->pressed & INPUT_BIT(INPUT_USE)) {
            use_key(cam);
        }
    }
    /* The use key may just have opened the inventory: the world stands from this frame. */
    if (menu_current() == MENU_NONE && !screens_is_open() && !gfx_paused()) {
        /* Skills look at their uses every six game hours; a level due opens the sheet (PlayerEntity.RaiseSkills). */
        character_raise_skills(&g_player.sheet, game_minutes());
        if (g_player.sheet.level_due) {
            sheet_over_play();
        }
        /* Each game minute that has passed tires the player, and may be too much (docs/game.md, Fatigue). */
        {
            int minute;

            minute = df_ifloor(game_minutes());
            while (g_last_minute < minute) {
                int how;

                g_last_minute++;
                how = player_minute(&g_player, foes_awake() > 0);
                if (how == PLAYER_DIED) {
                    die();
                    break;
                }
                if (how == PLAYER_COLLAPSED) {
                    interact_say("You collapse from exhaustion, and wake an hour later.");
                    clock_set_minute(clock_minute() + 60.0f);
                }
            }
        }
        /* Under water the breath runs down; none left is the end. */
        if (player_breath(&g_player, dt)) {
            interact_say("You have drowned.");
            die();
        }
        /* The weapon in hand follows the pack (equip, additem, a break); the mouse swings it. */
        player_check_weapon(&g_player);
        weapon_hold(inventory_weapon(&g_player.pack));
        if (!closed) {
            attack_input(in, dt);
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
        {
            Input walking;

            /* A blade's gesture takes the mouse while the button is down; a bow is aimed with it. */
            walking = *in;
            if ((in->held & INPUT_BIT(INPUT_ATTACK)) && !weapon_is_bow()) {
                walking.look_dx = 0;
                walking.look_dy = 0;
            }
            player_step(&g_player, &walking, &g_level->scene, dt, walker_on());
        }
        {
            float speed;
            float height;

            /* A landing past classic's 5 m hurts. */
            if (body_landed(&g_player.body, &speed, &height)) {
                int died;

                /* tgm on: nothing is taken and nothing said. */
                died = player_hurt(&g_player, rules_fall_damage(height / RULES_UNITS_PER_M));
                if (died > 0) {
                    die();
                } else if (died == 0) {
                    interact_say("That hurt.");
                }
            }
        }
        player_camera(&g_player, cam);
        /* The swing's hit frame strikes what is in reach of the eye; flinches run out. */
        if (weapon_hit()) {
            float eye[3];

            eye[0] = cam->x;
            eye[1] = cam->y;
            eye[2] = cam->z;
            if (weapon_is_bow()) {
                shoot_arrow(cam, eye);
            } else {
                combat_strike(&g_player.sheet, inventory_hand(&g_player.pack), weapon_swing(), &g_level->scene,
                              g_player.body.feet, eye, g_player.yaw);
            }
        }
        combat_frame(dt);
        /* The creatures see, chase and strike (tai off: they stand); a blow may kill. */
        if (foes_frame(&g_player, &g_level->scene, dt)) {
            die();
            player_camera(&g_player, cam);
        }
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
    screens_close();
    weapon_free();
    walker_set_game(0);
    gfx_set_extra(0);
    gfx_set_pause(0);
    input_set_pointer(0);
    menu_shutdown();
    overlay_free();
}
