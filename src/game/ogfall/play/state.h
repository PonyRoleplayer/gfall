#ifndef OGFALL_STATE_H
#define OGFALL_STATE_H

/* OGFALL's pointer-free session, original record ownership and Watcom RNG.
 * This is our own schema, not a DOS save ABI. Read docs/ogfall.md. */
#include "game/ogfall/play/standing.h"
#include "game/ogfall/play/bank.h"
#include "game/ogfall/play/lodging.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/guild.h"
#include "engine/core/df.h"
#include "game/ogfall/tables/tables.h"
#include "game/ogfall/play/move.h"
#include "game/ogfall/play/progress.h"
#include "game/ogfall/play/survival.h"
#include "game/ogfall/play/encounter.h"
#include "game/ogfall/play/magicstate.h"
#include "game/ogfall/play/worldstate.h"
#include "engine/scene/keep.h"

#define OG_STATE_VERSION 12
#define OG_STATE5_WIRE_BYTES 348
#define OG_WORLD_ROWS 512
#define OG_THUMB_W 80
#define OG_THUMB_H 50
#define OG_THUMB_PIXELS (OG_THUMB_W * OG_THUMB_H)
#define OG_OBJECTS 256
#define OG_MODES 8
#define OG_WORLD_ROOT 1u
#define OG_PLAYER_ROOT 700u

enum {
    OG_RECORD_WORLD = 1,
    OG_RECORD_ITEM = 2,
    OG_RECORD_ENTITY = 3,
    OG_RECORD_MOVE = 4,
    OG_RECORD_EYE = 5,
    OG_RECORD_SPELL = 9,
    OG_RECORD_QUESTS = 16,
    OG_RECORD_SOUL = 20,
    OG_RECORD_OPTIONS = 23,
    OG_RECORD_LOGBOOK = 24,
    OG_RECORD_BANK = 25,
    OG_RECORD_NONWORLD = 39,
    OG_RECORD_CONTAINER = 52
};

enum { OG_EVENT_GIVE = 1, OG_EVENT_KILL = 2, OG_EVENT_FOUND = 3,
       OG_EVENT_DROP = 5, OG_EVENT_HURT = 21, OG_EVENT_NPC = 28,
       OG_EVENT_GOLD = 71, OG_EVENT_SPELL = 73, OG_EVENT_FACTION = 78,
       /* OGFALL notifications, outside the original quest event range. */
       OG_EVENT_MINUTE = 256, OG_EVENT_PLACE = 257 };
enum { OG_GAME_WORLD = 0, OG_GAME_SHEET = 3, OG_GAME_INVENTORY = 4,
       OG_GAME_SPELLBOOK = 5, OG_GAME_OPTIONS = 7, OG_GAME_READING = 11, OG_GAME_LOG = 14,
       OG_GAME_REST = 16, OG_GAME_TRAVEL = 19 };

typedef struct OgObject {
    df_u32 id, parent;
    int type, index, flags;
    /* Original group/index identity. Weight is in quarter kilograms. */
    int group, material, count, condition;
    int max_condition, weight, value;
    char name[32];
    int inventory_image, dropped_image, armor_type, color, message;
    /* Containers use the persistent key and place-frame position. */
    df_u32 place_key, thing_key;
    float x, y, z;
    /* Spell instances own their editable name (above) and icon. Effects stay
     * in the immutable standard definition identified by index. */
    int spell_icon;
} OgObject;

typedef struct OgEvent {
    int kind;
    df_u32 subject, other;
    int value;
} OgEvent;

typedef struct OgState {
    int version, ready;
    OgCharacter who;
    /* Includes a custom class's restrictions, independent of the editor. */
    OgClass career;
    int base_attribute[OG_ATTRS];
    df_u32 skill_uses[OG_SKILLS];
    df_u32 equipped[27];
    df_u32 random, next_low, minutes;
    df_u32 tick_fraction, ticks;
    float frame_fraction;
    int settings_flags, mouse_mode;
    int mode, depth, mode_stack[OG_MODES];
    int region, location, environment;
    float x, eye_y, z;
    int yaw, pitch;
    df_u32 move_id, eye_id, entity_id, inventory_id;
    OgObject object[OG_OBJECTS];
    int motion_valid;
    OgMotion motion;
    int world_count;
    Keep world[OG_WORLD_ROWS];
    int hud_mode, weapon_ready, cursor_active;
    char save_label[32];
    int preview_valid;
    df_u16 preview[OG_THUMB_PIXELS];
    df_u32 event_serial;
    OgEvent last_event;
    df_u32 stealth_minute;
    OgProgress progress;
    OgSurvival survival;
    OgEncounterState encounters;
    OgMagicState magic;
    OgWorldState world_state;
    OgStanding standing;
    OgBankState bank;
    OgLodging lodging;
    OgLaw law;
    OgGuilds guilds;
} OgState;

OgCharacter *og_character(void);
void og_character_clear(void);
const OgState *og_state(void);
/* The faction and region standing inside the session (standing.h). */
OgStanding *og_standing(void);
/* The regional accounts, the house and the ship (bank.h). */
OgBankState *og_bank(void);
/* The rooms rented and the last meal (lodging.h). */
OgLodging *og_lodging(void);
/* The crime noted and each region's punishment (law.h). */
OgLaw *og_law(void);
const char *og_state_error(void);
/* Reset gameplay, retaining the RNG stream and control settings.
 * begin consumes the completed character from chargen. */
void og_state_clear(void);
int og_state_begin(void);
void og_random_seed(df_u32 seed);
int og_random_range(int low, int high);
const OgObject *og_object(df_u32 id);
/* Editing fields never changes record identity, type or ownership. */
OgObject *og_object_edit(df_u32 id);
int og_state_equip(int slot, df_u32 item);
df_u32 og_object_new(int type, df_u32 parent, int index, unsigned high);
int og_object_reparent(df_u32 id, df_u32 parent);
/* Only leaves may be deleted. Mandatory session records are protected. */
int og_object_delete(df_u32 id);
void og_state_settings(int flags, int mouse_mode);
int og_state_mode_push(int mode);
int og_state_mode_pop(void);
void og_state_set_place(int region, int location, int environment);
void og_state_pose(float x, float eye_y, float z, int yaw, int pitch);
/* The frame caller excludes menus/focus loss; modes also gate the clock. */
void og_state_frame(float seconds);
void og_state_advance_ms(df_u32 milliseconds);
void og_state_pass_minutes(df_u32 minutes);
void og_state_calendar(void);
/* Original epoch: 3E 400, 360-day years; weekday runs across months. */
void og_state_date(int *year, int *month, int *day, int *weekday);
/* One synchronous observer, kept outside saved data. No dropped event queue. */
void og_state_observe(void (*fn)(const OgEvent *event));
void og_state_event(int kind, df_u32 subject, df_u32 other, int value);
void og_skill_use(int skill);
void og_state_stealth_checked(void);
void og_state_clear_skill_uses(int skill);
void og_state_base_attribute(int attribute, int value);
int og_state_valid(const OgState *snapshot);
int og_state_restore(const OgState *snapshot);
/* Capture the engine's keyed changes only at a save boundary. */
int og_state_capture_world(void);
int og_state_restore_world(void);
void og_state_capture_motion(void);
void og_state_ui(int mode, int weapon_ready, int cursor_active);
void og_state_save_label(const char *label);
void og_state_preview(const df_u16 *pixels);

#endif
