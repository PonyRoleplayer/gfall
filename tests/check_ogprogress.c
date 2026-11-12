#include "check_ogprogress.h"
#include "check_util.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/save.h"
#include "engine/core/mem.h"

static OgState g_before;

void check_ogprogress(void)
{
    OgProgress *progress;
    OgIn input;
    int skill, i, value, points, health;

    g_before = *og_state();
    for (skill = 0; skill < OG_SKILLS; skill++) {
        og_character()->skill[skill] = 25; og_state_clear_skill_uses(skill);
        for (i = 0; i < 2000; i++) { og_skill_use(skill); }
    }
    og_character()->level = 1; og_character()->reflexes = 2;
    og_progress_begin(); health = og_character()->max_health;
    check_expect(og_progress_needed(50,1,65536,13) == 42 &&
                 og_progress_needed(50,2,65536,13) == 44,
                 "original skill thresholds include skill factor, class multiplier and 1.04 to the level");
    og_state_pass_minutes(360); og_progress_rest();
    check_expect(og_character()->skill[13] == 25,"rest does not test skill advancement at exactly six hours");
    og_state_pass_minutes(1); og_progress_rest(); progress = og_progress();
    check_expect(og_character()->skill[13] == 26 && !og_state()->skill_uses[13] && og_progress_raised(13),
                 "rest after six hours raises a trained skill once, consumes its uses and records the change");
    check_expect(og_character()->level == 2 && og_character()->max_health > health &&
                 progress->active && progress->points >= 4 && progress->points <= 6,
                 "the original career skill sum triggers one level, health gain and 4..6 attribute points");
    points = progress->points; value = og_state()->base_attribute[OG_STR];
    check_expect(!og_progress_adjust(OG_STR,-1),"a level-up cannot lower an attribute below its opening value");
    check_expect(og_progress_adjust(OG_STR,1) && og_progress_adjust(OG_STR,-1) &&
                 progress->points == points && og_state()->base_attribute[OG_STR] == value,
                 "level-up points can be allocated and refunded within their original floor");
    check_expect(og_state_capture_world() && og_save_write(6) && og_save_read(6) &&
                 og_progress()->active && og_progress()->points == points,
                 "unfinished level allocation survives a game save and reload");
    og_possessions_open(OG_VIEW_SHEET,0); og_possessions_close();
    check_expect(og_possessions()->page == OG_VIEW_SHEET && og_possessions()->notice[0],
                 "the character sheet refuses to discard unspent level points");
    /* Dismiss the warning, then spend through the actual sheet's spinner. */
    mem_zero(&input,sizeof(input)); input.ch = 13; og_possessions_frame(&input);
    input.ch = 0; input.on_picture = 1; input.mouse_pressed = 1; input.cx = 180; input.cy = 6;
    for (i = 0; i < points; i++) { og_possessions_frame(&input); }
    check_expect(!og_progress()->points && og_state()->base_attribute[OG_STR] == value+points,
                 "the sheet spinner spends the saved points on the chosen attribute");
    og_possessions_close();
    check_expect(!og_possessions()->page && !og_progress()->active,"a completed level-up returns to play");
    og_state_restore(&g_before); og_state_restore_world();
}
