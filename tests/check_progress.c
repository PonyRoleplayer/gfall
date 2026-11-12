#include "check_progress.h"
#include "check_util.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/save.h"
#include "engine/core/mem.h"

static GameState g_before;

void check_progress(void)
{
    ProgressState *progress;
    PlayIn input;
    int skill, i, value, points, health;

    g_before = *game_state();
    for (skill = 0; skill < SKILLS; skill++) {
        character_state()->skill[skill] = 25; state_clear_skill_uses(skill);
        for (i = 0; i < 2000; i++) { skill_use(skill); }
    }
    character_state()->level = 1; character_state()->reflexes = 2;
    progress_begin(); health = character_state()->max_health;
    check_expect(progress_needed(50,1,65536,13) == 42 &&
                 progress_needed(50,2,65536,13) == 44,
                 "original skill thresholds include skill factor, class multiplier and 1.04 to the level");
    state_pass_minutes(360); progress_rest();
    check_expect(character_state()->skill[13] == 25,"rest does not test skill advancement at exactly six hours");
    state_pass_minutes(1); progress_rest(); progress = progress_state();
    check_expect(character_state()->skill[13] == 26 && !game_state()->skill_uses[13] && progress_raised(13),
                 "rest after six hours raises a trained skill once, consumes its uses and records the change");
    check_expect(character_state()->level == 2 && character_state()->max_health > health &&
                 progress->active && progress->points >= 4 && progress->points <= 6,
                 "the original career skill sum triggers one level, health gain and 4..6 attribute points");
    points = progress->points; value = game_state()->base_attribute[ATTR_STR];
    check_expect(!progress_adjust(ATTR_STR,-1),"a level-up cannot lower an attribute below its opening value");
    check_expect(progress_adjust(ATTR_STR,1) && progress_adjust(ATTR_STR,-1) &&
                 progress->points == points && game_state()->base_attribute[ATTR_STR] == value,
                 "level-up points can be allocated and refunded within their original floor");
    check_expect(state_capture_world() && save_write(6) && save_read(6) &&
                 progress_state()->active && progress_state()->points == points,
                 "unfinished level allocation survives a game save and reload");
    possessions_open(VIEW_SHEET,0); possessions_close();
    check_expect(possessions_state()->page == VIEW_SHEET && possessions_state()->notice[0],
                 "the character sheet refuses to discard unspent level points");
    /* Dismiss the warning, then spend through the actual sheet's spinner. */
    mem_zero(&input,sizeof(input)); input.ch = 13; possessions_frame(&input);
    input.ch = 0; input.on_picture = 1; input.mouse_pressed = 1; input.cx = 180; input.cy = 6;
    for (i = 0; i < points; i++) { possessions_frame(&input); }
    check_expect(!progress_state()->points && game_state()->base_attribute[ATTR_STR] == value+points,
                 "the sheet spinner spends the saved points on the chosen attribute");
    possessions_close();
    check_expect(!possessions_state()->page && !progress_state()->active,"a completed level-up returns to play");
    state_restore(&g_before); state_restore_world();
}
