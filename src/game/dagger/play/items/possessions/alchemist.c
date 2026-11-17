#include "internal.h"
#include "game/dagger/play/magic/potion.h"
#include "game/dagger/play/quests/quest.h"

int possessions_potion(void)
{
    df_u32 ids[512];

    if (possessions_view.page) { return 0; }
    if (!ingredient_list(ids, 512) && !recipes_known(ids, 512)) {
        return possessions_message(words_state(34), 0);
    }
    if (!possessions_open(VIEW_POTION, 0)) { return 0; }
    cauldron_clear();
    possessions_view.scroll[0] = 0;
    return 1;
}

static void potion_say(const char *text)
{
    df_copy(possessions_view.notice, text, sizeof(possessions_view.notice)); possessions_view.notice_page = 0; possessions_view.revision++;
}

/* potion_make: a known recipe's ingredients loaded into the cauldron, or "you do not have the ingredients". */
static void potion_make(int recipe)
{
    if (!recipe_load(recipe)) { potion_say("You do not have the ingredients required."); }
    possessions_view.revision++;
}

static void potion_recipe_picked(int row)
{
    df_u32 ids[LIST_MOST];
    int count;

    if (row < 0) { return; }
    count = recipes_known(ids, LIST_MOST);
    if (row < count) { potion_make(recipe_of(ids[row])); }
}

static void potion_click(int x, int y)
{
    df_u32 ids[512], recipes[LIST_MOST];
    int i, row, col, count, cells[8], n, brewed, potion_id;
    const Cauldron *c;

    /* The five buttons (potionmaker_buttons), strictly inside. */
    if (possessions_hit(x,y,169,26,204,41)) {
        static char names[LIST_MOST][LIST_NAME];

        play_sound(205);
        count = recipes_known(recipes, LIST_MOST);
        if (!count) { potion_say("You have no recipies."); return; }
        if (count == 1) { potion_make(recipe_of(recipes[0])); return; }
        for (i = 0; i < count; i++) { df_copy(names[i], potion_name(recipe_of(recipes[i])), LIST_NAME); }
        possessions_list((const char (*)[LIST_NAME])names, count, potion_recipe_picked);
        return;
    }
    if (possessions_hit(x,y,169,42,204,57)) {
        df_u32 made;

        brewed = brew_state(&n, &made);
        (void)potion_id;
        if (brewed != BREW_NOTHING) { play_sound(208); potion_say("Your potion has been mixed."); }
        return;
    }
    if (possessions_hit(x,y,6,30,15,45)) { if (possessions_view.scroll[0]) { possessions_view.scroll[0] -= 3; } return; }
    if (possessions_hit(x,y,6,156,15,171)) {
        count = ingredient_list(ids, 512);
        if (count - possessions_view.scroll[0] > 12) { possessions_view.scroll[0] += 3; }
        return;
    }
    if (possessions_hit(x,y,290,178,306,202)) { possessions_close(); return; }
    c = cauldron_state();
    /* A cell of the cauldron (two across from x 221, 38 pixels down from y 30): taking it out. The cells shown are the full ones in order. */
    if (x > 221 && x < 304 && y > 30 && y < 171) {
        col = (x - 221) / 56; if ((x - 221) % 56 > 27) { return; }
        row = (y - 30) / 38; if ((y - 30) % 38 > 24) { return; }
        n = 0;
        for (i = 0; i < CAULDRON; i++) { if (c->item[i]) { cells[n++] = i; } }
        i = col + row * 2;
        if (i < n) { cauldron_remove(cells[i]); }
        return;
    }
    /* The ingredients (three across from x 16, 38 pixels down): putting one in. */
    if (x >= 16 && x <= 155 && y >= 30 && y <= 171) {
        col = (x - 16) / 56; if ((x - 16) % 56 > 27) { return; }
        row = (y - 30) / 38; if ((y - 30) % 38 > 27) { return; }
        count = ingredient_list(ids, 512);
        i = col + row * 3 + possessions_view.scroll[0];
        if (i < count && cauldron_count() != CAULDRON) { cauldron_add(ids[i]); }
    }
}

void possessions_alchemy_frame(const PlayIn *in, int click)
{
    if (click) { potion_click(in->cx,in->cy); }
}
