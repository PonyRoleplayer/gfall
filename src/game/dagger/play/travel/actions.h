#ifndef GFALL_ACTIONS_H
#define GFALL_ACTIONS_H

/* Original dungeon links, separate from engine movers. The adapter supplies
 * raw action records and resolved model/mover/next indices. See docs/game.md. */
#include "engine/scene/scene.h"
#include "engine/gfx/camera.h"

/*
 * The RDB link actions (link_step, run_0006480F.c) and the events links_trigger is called with. Actions ACTION_TRANSLATE..ACTION_ROTATE move a model (2..7 along an axis, ACTION_AXIS_TICKS
 * long), ACTION_TELEPORT too; ACTION_DOOR_OPEN..ACTION_DOOR_LOCK are the doors (they do not reverse). A trap casts from TRAP_CAST_HEIGHT above its model; the pulsed actions repeat every
 * ACTION_PULSE_MS. A trigger of 8 answers LINK_EVENT_USE, LINK_EVENT_WALK_INTO, LINK_EVENT_STRIKE and LINK_EVENT_SPELL, of 9 the first two, another only its own event. A swing reaches a model within
 * STRIKE_MODEL_REACH. Texts: a message is TEXT_LINK_MESSAGE + its parameter, a riddle TEXT_RIDDLE with its answer at TEXT_RIDDLE_ANSWER.
 */
enum {
    ACTION_TRANSLATE = 1, ACTION_ROTATE = 8, ACTION_CAST = 9, ACTION_MESSAGE = 11, ACTION_RIDDLE = 12, ACTION_TELEPORT = 14, ACTION_SET_LOCK = 15,
    ACTION_DOOR_OPEN = 16, ACTION_DOOR_UNLOCK = 17, ACTION_DOOR_FORCE = 18, ACTION_DOOR_CLOSE = 19, ACTION_DOOR_LOCK = 20,
    ACTION_HURT = 21, ACTION_HURT_ELEMENT_FIRST = 22, ACTION_HURT_ELEMENT_LAST = 25, ACTION_POISON = 26, ACTION_DISEASE = 27, ACTION_DRAIN_MAGICKA = 28, ACTION_SOUND = 30
};
enum { LINK_EVENT_STAND = 1, LINK_EVENT_USE = 2, LINK_EVENT_WALK_INTO = 3, LINK_EVENT_LOCK_FAIL = 4, LINK_EVENT_STRIKE = 5, LINK_EVENT_SPELL = 6, LINK_EVENT_LOCK_PICKED = 7 };
#define TRIGGER_MASK 15
#define TRIGGER_ANY_USE 8
#define TRIGGER_TOUCH 9
#define TRAP_CAST_HEIGHT 40
#define ACTION_PULSE_MS 1000
#define ACTION_AXIS_TICKS 50
#define STRIKE_MODEL_REACH 100
#define TEXT_LINK_MESSAGE 8600
#define TEXT_RIDDLE 5400
#define TEXT_RIDDLE_ANSWER 5656
#define DISEASES 17
#define POISONS 12

#define ACTIONS 1024
#define USE_ACTION 0x4f4800
typedef struct ActionInfo {
    KeepKey key;
    int model, mover, next;
    int trigger, kind, axis, duration, magnitude, sound;
} ActionInfo;

int actions_open(const Scene *scene, const ActionInfo *actions, int count);
void actions_close(void);
int actions_trigger(const Scene *scene, int model, int event);
int actions_accepts(int trigger, int event);
void actions_step(float seconds);
void actions_capture(void);
void actions_strike(const Scene *scene, const Camera *camera, int event);

#endif
