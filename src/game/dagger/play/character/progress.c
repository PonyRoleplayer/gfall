#include "game/dagger/play/character/progress.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/ui/msg.h"
#include "game/dagger/tables/progressdefs.h"
#include "engine/core/log.h"

int progress_sum(const CharacterState *who, const ClassInfo *career)
{
    int i, sum, low, high, value;
    sum = 0; low = SKILL_MOST * 10; high = 0;
    for (i = 0; i < PRIMARY_SKILLS; i++) {
        sum += who->skill[career->primary[i]];
        value = who->skill[career->major[i]]; sum += value;
        if (value < low) { low = value; }
    }
    for (i = 0; i < MINOR_SKILLS; i++) { value = who->skill[career->minor[i]]; if (value > high) { high = value; } }
    return sum-low+high;
}

int progress_needed(int value, int level, int multiplier, int skill)
{
    double needed, power;
    int i;
    power = 1;
    for (i = 0; i < level; i++) { power *= SKILL_LEVEL_GROWTH; }
    needed = skill_advance_factor(skill)*(double)value;
    needed = multiplier*needed/SKILL_USES_DIVISOR;
    needed = power*needed*SKILL_USES_NUMERATOR/SKILL_USES_DENOMINATOR+1;
    return needed >= 2147483647.0 ? 2147483647 : (int)needed;
}

void progress_begin(void)
{
    ProgressState *progress;
    progress = progress_state(); progress->checked = game_state()->minutes;
    progress->base_sum = progress_sum(character_state(),&game_state()->career);
}

int progress_raised(int skill)
{
    if (skill < 0 || skill >= SKILLS) { return 0; }
    return skill < SKILL_BITS_LO ? (progress_state()->raised_lo & (1u << skill)) != 0 :
                       (progress_state()->raised_hi & (1u << (skill-SKILL_BITS_LO))) != 0;
}

static void rest_checks(void)
{
    ProgressState *progress;
    CharacterState *who;
    const ClassInfo *career;
    double uses;
    int skill, i, mastered, level, health;
    char message[96];

    progress = progress_state(); who = character_state(); career = &game_state()->career;
    if (progress->active || game_state()->minutes-progress->checked <= SKILL_CHECK_MINUTES) { return; }
    progress->checked = game_state()->minutes; progress->raised_lo = progress->raised_hi = 0;
    mastered = 0;
    for (i = 0; i < 3; i++) {
        if (who->skill[career->primary[i]] == SKILL_MOST || who->skill[career->major[i]] == SKILL_MOST) { mastered = 1; }
    }
    for (skill = 0; skill < SKILLS; skill++) {
        uses = game_state()->skill_uses[skill]*(1.0-(who->reflexes-REFLEXES_NORMAL)/REFLEXES_STEP_DIV);
        if (uses < progress_needed(who->skill[skill],who->level,career->advancement,skill)) { continue; }
        state_clear_skill_uses(skill);
        if ((who->skill[skill] >= SKILL_CAP_MASTERED && mastered) || who->skill[skill] >= SKILL_MOST) { continue; }
        who->skill[skill]++;
        if (skill < SKILL_BITS_LO) { progress->raised_lo |= 1u << skill; }
        else { progress->raised_hi |= 1u << (skill-SKILL_BITS_LO); }
        log_format(message,sizeof(message),"Your %s skill has improved.",skill_name(skill)); msg_set(message);
    }
    level = (progress_sum(who,career)-progress->base_sum+LEVEL_SUM_ADD)/LEVEL_SUM_DIV;
    if (level < 1) { level = 1; }
    if (who->level == level) { return; }
    if (who->level > level) { who->level--; return; }
    who->level++;
    health = random_range(career->hp_per_level/2,career->hp_per_level)+who->attribute[ATTR_END]/LEVEL_HP_END_DIV-LEVEL_HP_END_BASE;
    who->max_health += health > 0 ? health : 1;
    curse_level_up(health > 0 ? health : 1);
    progress->points = random_range(LEVEL_POINTS_MIN,LEVEL_POINTS_MAX); progress->active = 1;
    for (i = 0; i < ATTRS; i++) { progress->floor[i] = game_state()->base_attribute[i]; }
    play_sound(SND_LEVEL_UP);
}

/* raise_skills takes the curse's skill bonuses off before it looks at the skills and puts them back after (docs/curses.md 10). */
void progress_rest(void)
{
    curse_skill_bonuses(-1);
    rest_checks();
    curse_skill_bonuses(1);
}

int progress_adjust(int attribute, int direction)
{
    ProgressState *progress;
    int value;
    progress = progress_state();
    if (!progress->active || attribute < 0 || attribute >= ATTRS) { return 0; }
    value = game_state()->base_attribute[attribute];
    if (direction > 0 && progress->points > 0 && value < STAT_MOST) { direction = 1; }
    else if (direction < 0 && value > progress->floor[attribute]) { direction = -1; }
    else { return 0; }
    state_base_attribute(attribute,value+direction);
    character_state()->attribute[attribute] += direction; progress->points -= direction;
    if (attribute == ATTR_INT) {
        character_state()->max_magicka = magicka_state(character_state()->attribute[ATTR_INT],character_state()->class_flags);
        if (character_state()->magicka > character_state()->max_magicka) {
            character_state()->magicka = character_state()->max_magicka;
        }
    }
    return 1;
}

int progress_finish(void)
{
    int i, total;
    if (!progress_state()->active) {
        progress_state()->raised_lo = progress_state()->raised_hi = 0;
        return 1;
    }
    total = 0;
    for (i = 0; i < ATTRS; i++) { total += game_state()->base_attribute[i]; }
    if (progress_state()->points && total < LEVEL_ATTRIBUTE_SUM_MOST) { return 0; }
    progress_state()->active = progress_state()->points = 0;
    progress_state()->raised_lo = progress_state()->raised_hi = 0;
    return 1;
}
