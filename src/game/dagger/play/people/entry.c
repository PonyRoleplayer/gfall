#include "game/dagger/play/people/entry.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/people/hours.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/travel/doors.h"
#include "game/dagger/tables/doordefs.h"
#include "engine/core/mem.h"

/* See entry.h. */

#define TRIED_MOST 32

/* lockpick_skill_tried is kept on the building's door record in the original; here by building. */
static struct { df_u32 building; int skill; } g_tried[TRIED_MOST];
static int g_tried_count;

/* A street door bashed open (flag 16): kept for the visit to the town, as the original keeps it until the town reloads. */
static df_u32 g_bashed[TRIED_MOST];
static int g_bashed_count;

void entry_reset(void)
{
    g_tried_count = 0;
    g_bashed_count = 0;
}

void entry_set_bashed(df_u32 building)
{
    if (!entry_bashed(building) && g_bashed_count < TRIED_MOST) { g_bashed[g_bashed_count++] = building; }
}

int entry_bashed(df_u32 building)
{
    int i;

    for (i = 0; i < g_bashed_count; i++) { if (g_bashed[i] == building) { return 1; } }
    return 0;
}

static int was_tried(df_u32 building, int skill)
{
    int i;

    for (i = 0; i < g_tried_count; i++) {
        if (g_tried[i].building == building && g_tried[i].skill == skill) {
            return 1;
        }
    }
    return 0;
}

static void note_tried(df_u32 building, int skill)
{
    int i;

    for (i = 0; i < g_tried_count; i++) {
        if (g_tried[i].building == building) {
            g_tried[i].skill = skill;
            return;
        }
    }
    if (g_tried_count < TRIED_MOST) {
        g_tried[g_tried_count].building = building;
        g_tried[g_tried_count++].skill = skill;
    }
}

int building_try_enter(const EntryBuilding *b, const EntryPlayer *p, df_u32 now, int witnessed, Entry *out)
{
    HoursPlayer hours;
    int lock, skill, chance;

    mem_zero(out, sizeof(*out));
    if (b->id && b->id == p->house) { out->allow = 1; return 1; }
    if (b->type == BUILDING_ALWAYS_OPEN) { out->allow = 1; return 1; }
    hours.brotherhood = p->brotherhood; hours.thieves = p->thieves; hours.fighters = p->fighters; hours.mages = p->mages;
    if (b->type < BUILDING_HOUSE1 && building_open(b->type, b->faction, b->id, now, p->daytime, &hours)) { out->allow = 1; return 1; }
    if (b->type < BUILDING_HOUSE1 && (b->flags & BUILDING_FLAG_OPEN)) { return 0; }
    if (b->flags & BUILDING_FLAG_OPEN) { out->allow = 1; return 1; }
    if (b->quest_open) { out->allow = 1; return 1; }
    if (p->lock_spell) {
        out->spent_spell = 1;
        out->allow = 1;
        return 1;
    }
    if (b->faction == FACTION_DARK_BROTHERHOOD && p->brotherhood >= 0) { out->allow = 1; return 1; }
    if (b->faction == FACTION_THIEVES_GUILD && p->thieves >= 0) { out->allow = 1; return 1; }
    if (b->type >= BUILDING_HOUSE1 && b->type <= BUILDING_HOUSE4 && p->daytime && (b->id & 65535u) % (unsigned)PERCENT < HOUSE_OPEN_CHANCE) {
        out->text = TEXT_HOUSE_OPEN;
        out->allow = 1;
        return 1;
    }
    lock = b->name_seed % DOOR_LOCK_SPREAD + DOOR_LOCK_MIN;
    if (p->steal) {
        /* lockpick_action_door */
        if (b->access >= ACCESS_OPEN) {
            out->allow = 1;
            return 1;
        }
        skill = character_state()->skill[SKILL_LOCKPICKING];
        if (was_tried(b->id, skill)) {
            return 0;
        }
        if (lock >= LOCK_IMPOSSIBLE) {
            out->line = lock_text(LOCK_FAILED);
            return 0;
        }
        skill_use(SKILL_LOCKPICKING);
        chance = skill - lock * LOCK_PER_LEVEL;
        chance = chance < CHANCE_MIN ? CHANCE_MIN : chance > CHANCE_MAX ? CHANCE_MAX : chance;
        if (random_range(0, PERCENT) <= chance) {
            out->line = lock_text(LOCK_OPEN);
            out->picked = 1;
            guild_count_crime(GUILD_COUNT_THIEF, 1, now);
            if (witnessed || random_range(1, PICK_SEEN_ROLL) < PICK_SEEN_BASE - character_state()->skill[SKILL_STEALTH]) {
                out->crime = 1;
            }
            out->allow = 1;
            return 1;
        }
        note_tried(b->id, skill);
        out->line = lock_text(LOCK_FAILED);
        return 0;
    }
    /* lock_show_difficulty */
    out->line = lock_hint(character_state()->skill[SKILL_LOCKPICKING], lock);
    return 0;
}
