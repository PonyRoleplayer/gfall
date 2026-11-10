#include "game/ogfall/play/entry.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/hours.h"
#include "game/ogfall/play/guild.h"
#include "game/ogfall/play/doors.h"
#include "game/ogfall/tables/doordefs.h"
#include "engine/core/mem.h"

/* See entry.h. */

#define SKILL_LOCKPICKING 13
#define SKILL_STEALTH 16
#define TRIED_MOST 32

/* lockpick_skill_tried is kept on the building's door record in the original; here by building. */
static struct { df_u32 building; int skill; } g_tried[TRIED_MOST];
static int g_tried_count;

void og_entry_reset(void)
{
    g_tried_count = 0;
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

int og_building_try_enter(const OgEntryBuilding *b, const OgEntryPlayer *p, df_u32 now, int witnessed, OgEntry *out)
{
    OgHoursPlayer hours;
    int lock, skill, chance;

    mem_zero(out, sizeof(*out));
    if (b->id && b->id == p->house) { out->allow = 1; return 1; }
    if (b->type == 24) { out->allow = 1; return 1; }
    hours.brotherhood = p->brotherhood; hours.thieves = p->thieves; hours.fighters = p->fighters; hours.mages = p->mages;
    if (b->type < 17 && og_building_open(b->type, b->faction, b->id, now, p->daytime, &hours)) { out->allow = 1; return 1; }
    if (b->type < 17 && (b->flags & 16)) { return 0; }
    if (b->flags & 16) { out->allow = 1; return 1; }
    if (p->lock_spell) {
        out->spent_spell = 1;
        out->allow = 1;
        return 1;
    }
    if (b->faction == 108 && p->brotherhood >= 0) { out->allow = 1; return 1; }
    if (b->faction == 42 && p->thieves >= 0) { out->allow = 1; return 1; }
    if (b->type >= 17 && b->type <= 20 && p->daytime && (b->id & 65535u) % 100u < 50u) {
        out->text = 256;
        out->allow = 1;
        return 1;
    }
    lock = b->name_seed % 10 + 3;
    if (p->steal) {
        /* lockpick_action_door */
        if (b->access >= 10) {
            out->allow = 1;
            return 1;
        }
        skill = og_character()->skill[SKILL_LOCKPICKING];
        if (was_tried(b->id, skill)) {
            return 0;
        }
        if (lock >= 20) {
            out->line = og_lock_text(OG_LOCK_FAILED);
            return 0;
        }
        og_skill_use(SKILL_LOCKPICKING);
        chance = skill - lock * 5;
        chance = chance < 5 ? 5 : chance > 95 ? 95 : chance;
        if (og_random_range(0, 100) <= chance) {
            out->line = og_lock_text(OG_LOCK_OPEN);
            out->picked = 1;
            og_guild_count_crime(5, 1, now);
            if (witnessed || og_random_range(1, 300) < 100 - og_character()->skill[SKILL_STEALTH]) {
                out->crime = 1;
            }
            out->allow = 1;
            return 1;
        }
        note_tried(b->id, skill);
        out->line = og_lock_text(OG_LOCK_FAILED);
        return 0;
    }
    /* lock_show_difficulty */
    out->line = og_lock_hint(og_character()->skill[SKILL_LOCKPICKING], lock);
    return 0;
}
