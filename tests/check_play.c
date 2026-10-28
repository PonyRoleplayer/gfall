/*
 * The game's tests (src/game/dagger/play): classic's rules as sums, and
 * the player on foot in Privateer's Hold. Runs after check_dagger, with
 * ARENA2 mounted and MAIN.DAT merged. Does not open Glide.
 */

#include "check_util.h"

#include <stdio.h>
#include <string.h>

#include "engine/core/df.h"
#include "engine/scene/walker.h"
#include "game/dagger/places/world.h"
#include "game/dagger/play/player.h"
#include "game/dagger/play/rules.h"

/* Classic's speeds and jump, from Daggerfall Unity's formulas worked by hand. */
static void check_rules(void)
{
    check_expect(check_near(rules_walk_speed(50), 175.0f / 39.5f, 0.001f)
                     && check_near(rules_walk_speed(100), 250.0f / 39.5f, 0.001f)
                     && check_near(rules_walk_speed(10), 125.0f / 39.5f, 0.001f),
                 "walking is not (speed + 150 - drag) / 39.5");
    check_expect(check_near(rules_run_speed(50, 20), 200.0f / 39.5f * 1.45f, 0.001f)
                     && check_near(rules_run_speed(50, 100), 200.0f / 39.5f * 1.85f, 0.001f),
                 "running is not (speed + 150) / 39.5 times 1.35 and running over 200");
    check_expect(check_near(rules_jump_speed(20), 4.95f, 0.001f) && check_near(rules_gravity(), 20.0f, 0.001f)
                     && check_near(rules_jump_height(0), 4.5f * 4.5f / 40.0f, 0.001f)
                     && check_near(rules_jump_height(100), 6.75f * 6.75f / 40.0f, 0.001f),
                 "a jump does not rise as DFU's 4.5 m/s under 20 m/s^2");
}

/* n frames of 1/60 s with one input; returns how far the feet went across the ground. */
static float walk(Player *p, const Scene *scene, const Input *in, int n)
{
    float x0;
    float z0;
    float dx;
    float dz;
    int i;

    x0 = p->body.feet[0];
    z0 = p->body.feet[2];
    for (i = 0; i < n; i++) {
        player_step(p, in, scene, 1.0f / 60.0f);
    }
    dx = p->body.feet[0] - x0;
    dz = p->body.feet[2] - z0;
    return df_sqrt(dx * dx + dz * dz);
}

/*
 * The player in the hold, from its start marker: it settles on the
 * floor; a quarter second's walk goes classic's walking distance, a run
 * farther by classic's ratio; both keys at once no farther than one;
 * the mouse turns it; a jump rises classic's height and lands.
 */
static void check_hold(void)
{
    World *world;
    Level level;
    Player p;
    Input in;
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float walked;
    float ran;
    float both;
    float floor_y;
    float top;
    float want;
    float speed;
    float height;
    int landed;
    int i;

    world = world_load();
    check_expect(world != 0, "play: Privateer's Hold did not load");
    if (!world) {
        return;
    }
    world_as_level(world, &level);
    world_start(world, &x, &y, &z, &yaw, &pitch);
    player_init(&p);
    player_place(&p, x, y, z, yaw, pitch);
    memset(&in, 0, sizeof(in));
    walk(&p, &level.scene, &in, 60);
    check_expect(p.body.grounded, "play: the player does not stand on the hold's floor");
    floor_y = p.body.feet[1];
    body_landed(&p.body, &speed, &height);

    /* Forward along the start's facing, then back, then a run the same way. */
    in.move_f = 1;
    walked = walk(&p, &level.scene, &in, 15);
    in.move_f = -1;
    walk(&p, &level.scene, &in, 15);
    in.move_f = 1;
    in.held = INPUT_BIT(INPUT_RUN);
    ran = walk(&p, &level.scene, &in, 15);
    in.held = 0;
    in.move_f = -1;
    walk(&p, &level.scene, &in, 15);
    want = rules_walk_speed(PLAYER_SPEED) * RULES_UNITS_PER_M * 0.25f;
    printf("play: a quarter second walks %.1f (classic %.1f), runs %.1f\n", (double)walked, (double)want,
           (double)ran);
    check_expect(check_near(walked, want, want * 0.05f), "play: the walk does not go classic's distance");
    check_expect(check_near(ran / walked, rules_run_speed(PLAYER_SPEED, PLAYER_RUNNING) / rules_walk_speed(PLAYER_SPEED),
                            0.05f),
                 "play: the run is not classic's ratio to the walk");
    in.move_f = 1;
    in.move_r = 1;
    both = walk(&p, &level.scene, &in, 15);
    check_expect(both <= walked * 1.02f, "play: forward and sideways at once is faster than one");
    in.move_f = -1;
    in.move_r = -1;
    walk(&p, &level.scene, &in, 15);

    /* The mouse: 100 counts right turn it 0.3 radians. */
    memset(&in, 0, sizeof(in));
    yaw = p.yaw;
    in.look_dx = 100;
    player_step(&p, &in, &level.scene, 1.0f / 60.0f);
    check_expect(check_near(df_wrap_tau(p.yaw - yaw), 0.3f, 0.001f), "play: the mouse does not turn the player");

    /* A jump: up classic's height, then down on the floor with a landing. */
    memset(&in, 0, sizeof(in));
    in.pressed = INPUT_BIT(INPUT_JUMP);
    player_step(&p, &in, &level.scene, 1.0f / 60.0f);
    in.pressed = 0;
    top = p.body.feet[1];
    landed = 0;
    for (i = 0; i < 120 && !landed; i++) {
        player_step(&p, &in, &level.scene, 1.0f / 60.0f);
        if (p.body.feet[1] > top) {
            top = p.body.feet[1];
        }
        landed = body_landed(&p.body, &speed, &height);
    }
    want = rules_jump_height(PLAYER_JUMPING) * RULES_UNITS_PER_M;
    printf("play: a jump rises %.1f (classic %.1f) and lands in %d frames (classic %.0f)\n",
           (double)(top - floor_y), (double)want, i,
           (double)(2.0f * rules_jump_speed(PLAYER_JUMPING) / rules_gravity() * 60.0f));
    check_expect(check_near(top - floor_y, want, 2.0f), "play: the jump does not rise classic's height");
    check_expect(check_near((float)i, 2.0f * rules_jump_speed(PLAYER_JUMPING) / rules_gravity() * 60.0f, 3.0f),
                 "play: the jump is not as long in the air as classic's");
    check_expect(landed && p.body.grounded && check_near(p.body.feet[1], floor_y, 1.0f),
                 "play: the jump does not land on the floor it left");
}

void check_play(void)
{
    check_rules();
    check_hold();
}
