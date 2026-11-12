#include "game/dagger/play/progress.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/tables/progressdefs.h"
#include "engine/core/log.h"

int progress_sum(const CharacterState *who, const ClassInfo *career)
{
    int i, sum, low, high, value;
    sum = 0; low = 1000; high = 0;
    for (i = 0; i < 3; i++) {
        sum += who->skill[career->primary[i]];
        value = who->skill[career->major[i]]; sum += value;
        if (value < low) { low = value; }
    }
    for (i = 0; i < 6; i++) { value = who->skill[career->minor[i]]; if (value > high) { high = value; } }
    return sum-low+high;
}

int progress_needed(int value, int level, int multiplier, int skill)
{
    double needed, power;
    int i;
    power = 1;
    for (i = 0; i < level; i++) { power *= 1.04; }
    needed = skill_advance_factor(skill)*(double)value;
    needed = multiplier*needed/65536.0;
    needed = power*needed*2.0/5.0+1;
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
    return skill < 32 ? (progress_state()->raised_lo & (1u << skill)) != 0 :
                       (progress_state()->raised_hi & (1u << (skill-32))) != 0;
}

void progress_rest(void)
{
    ProgressState *progress;
    CharacterState *who;
    const ClassInfo *career;
    double uses;
    int skill, i, mastered, level, health;
    char message[96];

    progress = progress_state(); who = character_state(); career = &game_state()->career;
    if (progress->active || game_state()->minutes-progress->checked <= 360) { return; }
    progress->checked = game_state()->minutes; progress->raised_lo = progress->raised_hi = 0;
    mastered = 0;
    for (i = 0; i < 3; i++) {
        if (who->skill[career->primary[i]] == 100 || who->skill[career->major[i]] == 100) { mastered = 1; }
    }
    for (skill = 0; skill < SKILLS; skill++) {
        uses = game_state()->skill_uses[skill]*(1.0-(who->reflexes-2)/8.0);
        if (uses < progress_needed(who->skill[skill],who->level,career->advancement,skill)) { continue; }
        state_clear_skill_uses(skill);
        if ((who->skill[skill] >= 95 && mastered) || who->skill[skill] >= 100) { continue; }
        who->skill[skill]++;
        if (skill < 32) { progress->raised_lo |= 1u << skill; }
        else { progress->raised_hi |= 1u << (skill-32); }
        log_format(message,sizeof(message),"Your %s skill has improved.",skill_name(skill)); msg_set(message);
    }
    level = (progress_sum(who,career)-progress->base_sum+28)/15;
    if (level < 1) { level = 1; }
    if (who->level == level) { return; }
    if (who->level > level) { who->level--; return; }
    who->level++;
    health = random_range(career->hp_per_level/2,career->hp_per_level)+who->attribute[ATTR_END]/10-5;
    who->max_health += health > 0 ? health : 1;
    progress->points = random_range(4,6); progress->active = 1;
    for (i = 0; i < ATTRS; i++) { progress->floor[i] = game_state()->base_attribute[i]; }
    play_sound(364);
}

int progress_adjust(int attribute, int direction)
{
    ProgressState *progress;
    int value;
    progress = progress_state();
    if (!progress->active || attribute < 0 || attribute >= ATTRS) { return 0; }
    value = game_state()->base_attribute[attribute];
    if (direction > 0 && progress->points > 0 && value < 100) { direction = 1; }
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
    if (progress_state()->points && total < 800) { return 0; }
    progress_state()->active = progress_state()->points = 0;
    progress_state()->raised_lo = progress_state()->raised_hi = 0;
    return 1;
}
