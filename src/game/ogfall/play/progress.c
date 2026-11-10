#include "game/ogfall/play/progress.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/play.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/tables/progressdefs.h"
#include "engine/core/log.h"

int og_progress_sum(const OgCharacter *who, const OgClass *career)
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

int og_progress_needed(int value, int level, int multiplier, int skill)
{
    double needed, power;
    int i;
    power = 1;
    for (i = 0; i < level; i++) { power *= 1.04; }
    needed = og_skill_advance_factor(skill)*(double)value;
    needed = multiplier*needed/65536.0;
    needed = power*needed*2.0/5.0+1;
    return needed >= 2147483647.0 ? 2147483647 : (int)needed;
}

void og_progress_begin(void)
{
    OgProgress *progress;
    progress = og_progress(); progress->checked = og_state()->minutes;
    progress->base_sum = og_progress_sum(og_character(),&og_state()->career);
}

int og_progress_raised(int skill)
{
    if (skill < 0 || skill >= OG_SKILLS) { return 0; }
    return skill < 32 ? (og_progress()->raised_lo & (1u << skill)) != 0 :
                       (og_progress()->raised_hi & (1u << (skill-32))) != 0;
}

void og_progress_rest(void)
{
    OgProgress *progress;
    OgCharacter *who;
    const OgClass *career;
    double uses;
    int skill, i, mastered, level, health;
    char message[96];

    progress = og_progress(); who = og_character(); career = &og_state()->career;
    if (progress->active || og_state()->minutes-progress->checked <= 360) { return; }
    progress->checked = og_state()->minutes; progress->raised_lo = progress->raised_hi = 0;
    mastered = 0;
    for (i = 0; i < 3; i++) {
        if (who->skill[career->primary[i]] == 100 || who->skill[career->major[i]] == 100) { mastered = 1; }
    }
    for (skill = 0; skill < OG_SKILLS; skill++) {
        uses = og_state()->skill_uses[skill]*(1.0-(who->reflexes-2)/8.0);
        if (uses < og_progress_needed(who->skill[skill],who->level,career->advancement,skill)) { continue; }
        og_state_clear_skill_uses(skill);
        if ((who->skill[skill] >= 95 && mastered) || who->skill[skill] >= 100) { continue; }
        who->skill[skill]++;
        if (skill < 32) { progress->raised_lo |= 1u << skill; }
        else { progress->raised_hi |= 1u << (skill-32); }
        log_format(message,sizeof(message),"Your %s skill has improved.",og_skill_name(skill)); og_msg_set(message);
    }
    level = (og_progress_sum(who,career)-progress->base_sum+28)/15;
    if (level < 1) { level = 1; }
    if (who->level == level) { return; }
    if (who->level > level) { who->level--; return; }
    who->level++;
    health = og_random_range(career->hp_per_level/2,career->hp_per_level)+who->attribute[OG_END]/10-5;
    who->max_health += health > 0 ? health : 1;
    progress->points = og_random_range(4,6); progress->active = 1;
    for (i = 0; i < OG_ATTRS; i++) { progress->floor[i] = og_state()->base_attribute[i]; }
    og_sound(364);
}

int og_progress_adjust(int attribute, int direction)
{
    OgProgress *progress;
    int value;
    progress = og_progress();
    if (!progress->active || attribute < 0 || attribute >= OG_ATTRS) { return 0; }
    value = og_state()->base_attribute[attribute];
    if (direction > 0 && progress->points > 0 && value < 100) { direction = 1; }
    else if (direction < 0 && value > progress->floor[attribute]) { direction = -1; }
    else { return 0; }
    og_state_base_attribute(attribute,value+direction);
    og_character()->attribute[attribute] += direction; progress->points -= direction;
    if (attribute == OG_INT) {
        og_character()->max_magicka = og_magicka(og_character()->attribute[OG_INT],og_character()->class_flags);
        if (og_character()->magicka > og_character()->max_magicka) {
            og_character()->magicka = og_character()->max_magicka;
        }
    }
    return 1;
}

int og_progress_finish(void)
{
    int i, total;
    if (!og_progress()->active) {
        og_progress()->raised_lo = og_progress()->raised_hi = 0;
        return 1;
    }
    total = 0;
    for (i = 0; i < OG_ATTRS; i++) { total += og_state()->base_attribute[i]; }
    if (og_progress()->points && total < 800) { return 0; }
    og_progress()->active = og_progress()->points = 0;
    og_progress()->raised_lo = og_progress()->raised_hi = 0;
    return 1;
}
