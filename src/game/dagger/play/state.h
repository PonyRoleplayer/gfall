#ifndef GFALL_STATE_H
#define GFALL_STATE_H

/* GFALL's pointer-free session, original record ownership and Watcom RNG.
 * This is our own schema, not a DOS save ABI. Read docs/game.md. */
#include "game/dagger/play/people/standing.h"
#include "game/dagger/play/people/bank.h"
#include "game/dagger/play/people/lodging.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/people/politics.h"
#include "engine/core/df.h"
#include "game/dagger/tables/tables.h"
#include "game/dagger/play/input/move.h"
#include "game/dagger/play/character/progress.h"
#include "game/dagger/play/character/survival.h"
#include "game/dagger/play/combat/encounter.h"
#include "game/dagger/play/magic/magicstate.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/travel/worldstate.h"
#include "engine/scene/keep.h"

/* The day: MINUTES_PER_DAY minutes, daylight from DAWN_MINUTE to DUSK_MINUTE (06:00 to 18:00). */
#define MINUTES_PER_DAY 1440u
/* The year: DAYS_PER_YEAR days of MINUTES_PER_DAY (tamriel.c). */
#define MINUTES_PER_YEAR 518400u
#define DAWN_MINUTE 360
#define DUSK_MINUTE 1080
#define MINUTES_PER_WEEK 10080u
#define MONTHS 12

#define STATE_VERSION 23
#define STATE5_WIRE_BYTES 348
/* The biography's five lasting modifiers a save holds from schema 19, four bytes each. */
#define BIO_WIRE_BYTES 20
#define THUMB_W 80
#define THUMB_H 50
#define THUMB_PIXELS (THUMB_W * THUMB_H)
#define OBJECTS 256
#define MODES 8
#define WORLD_ROOT 1u
#define PLAYER_ROOT 700u

/* The player's equipment slots (FALL.EXE's EQUIP_*): the index of equipped[]. The pairs are the left and the right. Slots 22 and 25 have no use. */
enum {
    EQUIP_AMULET0, EQUIP_AMULET1, EQUIP_BRACELET0, EQUIP_BRACELET1, EQUIP_RING0, EQUIP_RING1,
    EQUIP_BRACER0, EQUIP_BRACER1, EQUIP_MARK0, EQUIP_MARK1, EQUIP_CRYSTAL0, EQUIP_CRYSTAL1,
    EQUIP_HEAD, EQUIP_RIGHT_PAULDRON, EQUIP_CLOAK0, EQUIP_LEFT_PAULDRON, EQUIP_CLOAK1,
    EQUIP_CHEST_CLOTHES, EQUIP_CUIRASS, EQUIP_RIGHT_HAND, EQUIP_GAUNTLETS, EQUIP_LEFT_HAND,
    EQUIP_SLOT22, EQUIP_GREAVES, EQUIP_LEGS_CLOTHES, EQUIP_SLOT25, EQUIP_BOOTS,
    EQUIP_SLOTS
};

/* The records that are people: a person flat and a walking townsperson (spfx.c, talk.c). */
#define RECORD_PERSON 8
#define RECORD_PEDESTRIAN 53
#include "game/dagger/play/watcom.h"
/* A skill's uses stop here (skill_add_uses). */
#define SKILL_USES_MOST 20000
/* object_new_id: the low word of an id runs OBJECT_ID_FIRST up to OBJECT_ID_END and wraps; the high word is the place. */
#define OBJECT_ID_FIRST 10000u
#define OBJECT_ID_END 63000u
#define OBJECT_ID_SPAN (OBJECT_ID_END - OBJECT_ID_FIRST)
/* init_player_records: the player's containers 0..PLAYER_CONTAINERS-1; number CONTAINER_WAGON exists only when a wagon is owned. */
#define PLAYER_CONTAINERS 9
#define CONTAINER_WAGON 4
/* A new game's clock in minutes (func_0004ED36); game_reset after a death sets 519210, three days earlier. */
#define NEW_GAME_MINUTES 523530u
/* The calendar: year YEAR_FIRST plus days / DAYS_PER_YEAR, twelve months of DAYS_PER_MONTH days, DAYS_PER_WEEK weekdays. */
#define YEAR_FIRST 400
#define DAYS_PER_YEAR 360u
#define DAYS_PER_MONTH 30u
#define DAYS_PER_WEEK 7u

enum {
    RECORD_WORLD = 1,
    RECORD_ITEM = 2,
    RECORD_ENTITY = 3,
    RECORD_MOVE = 4,
    RECORD_EYE = 5,
    RECORD_SPELL = 9,
    RECORD_QUESTS = 16,
    RECORD_SOUL = 20,
    RECORD_OPTIONS = 23,
    RECORD_LOGBOOK = 24,
    RECORD_BANK = 25,
    RECORD_NONWORLD = 39,
    RECORD_CONTAINER = 52
};

enum { EVENT_GIVE = 1, EVENT_KILL = 2, EVENT_FOUND = 3,
       EVENT_DROP = 5, EVENT_HURT = 21, EVENT_NPC = 28,
       EVENT_GOLD = 71, EVENT_SPELL = 73, EVENT_FACTION = 78,
       /* GFALL notifications, outside the original quest event range. */
       EVENT_MINUTE = 256, EVENT_PLACE = 257 };
enum { GAME_WORLD = 0, GAME_SHEET = 3, GAME_INVENTORY = 4,
       GAME_SPELLBOOK = 5, GAME_OPTIONS = 7, GAME_READING = 11, GAME_LOG = 14,
       GAME_REST = 16, GAME_TRAVEL = 19 };

typedef struct ObjectInfo {
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
} ObjectInfo;

typedef struct QuestEvent {
    int kind;
    df_u32 subject, other;
    int value;
} QuestEvent;

/* A spell the player made at a spell maker (spellmaker_buy copies the 89-byte record into the book's spell object): kept here, registered under id CUSTOM_SPELL_ID + its slot. */
#define CUSTOM_SPELLS 32
#define CUSTOM_SPELL_ID 3000
typedef struct CustomSpell { int used; df_u8 record[89]; } CustomSpell;

/* What an item made at the item maker carries (itemmaker_enchant: ten pairs of a type, 0..14 a power and 15..25 a side effect, and a parameter; the spell id for the first three powers). Kept by the item's id; a row whose item is gone is free. identified is the original's item flag 32. */
#define ENCHANTED_ITEMS 64
typedef struct ItemEnchant { df_u32 item; int identified; int type[10], param[10]; } ItemEnchant;

/* A dungeon the player has come into (the place state, below, explains the row). */
#define PLACE_ROWS 64
typedef struct PlaceRow { df_u32 place, seed_epoch, last_epoch; int roll_level; } PlaceRow;

typedef struct GameState {
    int version, ready;
    CharacterState who;
    /* Includes a custom class's restrictions, independent of the editor. */
    ClassInfo career;
    int base_attribute[ATTRS];
    df_u32 skill_uses[SKILLS];
    df_u32 equipped[EQUIP_SLOTS];
    df_u32 random, next_low, minutes;
    /* Made at a new game and saved (schema 23): every place's rolls derive from it (docs/gfall-place-state.md), so a pile is the same on every save and load. */
    df_u32 world_seed;
    /* The places in use and the epoch of the last sweep (schema 23). */
    PlaceRow place[PLACE_ROWS];
    int places;
    df_u32 sweep_epoch;
    df_u32 tick_fraction, ticks;
    float frame_fraction;
    int settings_flags, mouse_mode;
    int mode, depth, mode_stack[MODES];
    int region, location, environment;
    float x, eye_y, z;
    int yaw, pitch;
    df_u32 move_id, eye_id, entity_id, inventory_id;
    ObjectInfo object[OBJECTS];
    int motion_valid;
    MotionState motion;
    /* How many keep rows the world holds (docs/game.md, Saves): the rows themselves live in the engine's keep and are streamed to and from a save, never copied here, so a long campaign has no row limit. */
    int world_count;
    int hud_mode, weapon_ready, cursor_active;
    char save_label[32];
    int preview_valid;
    df_u16 preview[THUMB_PIXELS];
    df_u32 event_serial;
    QuestEvent last_event;
    df_u32 stealth_minute;
    ProgressState progress;
    Survival survival;
    EncounterState encounters;
    MagicState magic;
    WorldState world_state;
    StandingState standing;
    BankState bank;
    LodgingState lodging;
    LawState law;
    GuildsState guilds;
    /* The politics, region flags and news (schema 20). */
    PoliticsState politics;
    /* Vampirism and lycanthropy (schema 18): the infection, the form, the bonuses and the saved class. */
    CurseState curse;
    CustomSpell custom_spell[CUSTOM_SPELLS];
    ItemEnchant enchant[ENCHANTED_ITEMS];
    /* The game minute of the player's last kill (character +0x1FD, set by damage_creature_death): Health leech's clock. */
    df_u32 last_kill;
    /* The quests as quest.c packs them (schema 16): their length in a save; the bytes are staged (state_stage_quest), not held here. */
    int quest_len;
} GameState;

CharacterState *character_state(void);
/* The live class, which a curse changes and its cure puts back (curse.c). */
ClassInfo *career_edit(void);
/* A slot of the player's own spells, to fill (spellmaker.c). */
CustomSpell *custom_spell_edit(int slot);
/* A row of the enchanted items (itemmaker.c). */
ItemEnchant *item_enchant_edit(int slot);
void character_clear(void);
const GameState *game_state(void);
/* The faction and region standing inside the session (standing.h). */
StandingState *standing_state(void);
/* The regional accounts, the house and the ship (bank.h). */
BankState *bank_state(void);
/* The rooms rented and the last meal (lodging.h). */
LodgingState *lodging_state(void);
/* The crime noted and each region's punishment (law.h). */
LawState *law_state(void);
const char *state_error(void);
/* Reset gameplay, retaining the RNG stream and control settings.
 * begin consumes the completed character from chargen. */
void state_clear(void);
int state_begin(void);
void random_seed(df_u32 seed);
/*
 * Place state (docs/gfall-place-state.md): what a place holds is derived from saved numbers, never from the live stream, so saving, opening a pile and reloading always opens the same pile.
 * place_seed = mix(world_seed, place key, epoch); thing_seed = mix(place_seed, thing key). The epoch is the game day / PLACE_EPOCH_DAYS: when it moves on, a place's seed changes and its contents with it.
 * A place left unvisited for more than PLACE_GRACE_EPOCHS epochs is swept (the user chose one epoch of grace: a place survives 3 to 6 days after the player leaves; 0 is the plain rule).
 */
#define PLACE_EPOCH_DAYS 3
#define PLACE_GRACE_EPOCHS 1
/* Murmur3's finaliser constants and the golden ratio that keeps a zero word from mixing to zero: integer only, the same on the host and the 233. */
#define SEED_FMIX_1 0x85EBCA6Bu
#define SEED_FMIX_2 0xC2B2AE35u
#define SEED_GOLDEN 0x9E3779B9u
/*
 * A dungeon the player has entered has a row (PLACE_ROWS at most; made the first time anything asks for its seed): the epoch its seeds come from (frozen, so what was rolled stays what it was while the place is kept), the
 * epoch it was last in use, and the player's level at the first entry (what its piles and creatures are rolled at). One sweep, when the epoch turns over, drops the rows, keep rows and piles of every place
 * left for more than PLACE_GRACE_EPOCHS epochs, never the one the player is in. A place that is not a dungeon has no row: its seed uses the current epoch.
 */
/* The player left this place: it is stamped as last in use now. */
void place_leave(df_u32 place);
/* The level a place's piles and creatures are rolled at: its row's, else the player's. */
int place_roll_level(df_u32 place);
/* Runs the sweep if the epoch has turned over since the last one (the day tick, a load and a place's entry call it). The number of places swept. */
int place_sweep(void);
df_u32 seed_mix(df_u32 a, df_u32 b);
df_u32 place_epoch(void);
df_u32 place_seed(df_u32 place);
df_u32 thing_seed(df_u32 place, df_u32 thing);
/* Runs the random stream on seed until seed_stream_end puts the game's own back exactly (no draw is spent); they nest. */
void seed_stream_begin(df_u32 seed);
void seed_stream_end(void);
/* Gives the dungeons' creature tables the place seed (state_begin and state_restore do it). */
void place_seeds_install(void);
int random_range(int low, int high);
const ObjectInfo *object_at(df_u32 id);
/* Editing fields never changes record identity, type or ownership. */
ObjectInfo *object_edit(df_u32 id);
int state_equip(int slot, df_u32 item);
df_u32 object_new(int type, df_u32 parent, int index, unsigned high);
int object_reparent(df_u32 id, df_u32 parent);
/* Only leaves may be deleted. Mandatory session records are protected. */
int object_delete(df_u32 id);
void state_settings(int flags, int mouse_mode);
int state_mode_push(int mode);
int state_mode_pop(void);
void state_set_place(int region, int location, int environment);
void state_pose(float x, float eye_y, float z, int yaw, int pitch);
/* The frame caller excludes menus/focus loss; modes also gate the clock. */
void state_frame(float seconds);
void state_advance_ms(df_u32 milliseconds);
void state_pass_minutes(df_u32 minutes);
void state_calendar(void);
/* Original epoch: 3E 400, 360-day years; weekday runs across months. */
void state_date(int *year, int *month, int *day, int *weekday);
/* One synchronous observer, kept outside saved data. No dropped event queue. */
void state_observe(void (*fn)(const QuestEvent *event));
void state_event(int kind, df_u32 subject, df_u32 other, int value);
void skill_use(int skill);
/* skill_add_uses: n uses, never below 0 or past 20000. */
void skill_add_uses(int skill, int n);
void state_stealth_checked(void);
void state_clear_skill_uses(int skill);
void state_base_attribute(int attribute, int value);
int state_valid(const GameState *snapshot);
int state_restore(const GameState *snapshot);
/* Capture the engine's keyed changes only at a save boundary: the count of keep rows (the rows stay in the keep and are written from it). */
int state_capture_world(void);
/* The keep takes the rows a read save staged, or is emptied after a new game or a clear; with neither it is left as it is. */
int state_restore_world(void);
/*
 * What a save read holds until the world is rebuilt (docs/game.md, Saves): the keep rows, the automap rows and the quests' bytes, in a heap of their own, sized by the save (no limit
 * but memory). A failed stage returns 0 and leaves nothing staged. state_stage_drop frees all three; a restore consumes its own.
 */
Keep *state_stage_keep(int count);
AutomapRow *state_stage_map(int count);
df_u8 *state_stage_quest(int len);
void state_stage_drop(void);
/* The staged automap rows and how many, taken by world_restore (travel.c) and then dropped. */
const AutomapRow *state_staged_map(int *count);
void state_stage_map_done(void);
/* A keep row of a save that is consistent (the checks a restored row must pass). */
int state_keep_row_valid(const Keep *row);
void state_capture_motion(void);
void state_ui(int mode, int weapon_ready, int cursor_active);
void state_save_label(const char *label);
void state_preview(const df_u16 *pixels);

#endif
