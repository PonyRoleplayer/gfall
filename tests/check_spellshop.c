#include <string.h>
#include "check_spellshop.h"
#include "check_util.h"
#include "game/dagger/play/spellshop.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/spellmaker.h"
#include "game/dagger/tables/magicdefs.h"
#include "game/dagger/tables/itemdefs.h"

/* The spell maker: the empty spell, the effect list by target, a Damage spell's numbers clamped and paired, the stored cost and price by hand, the buying and its refusals, and the spell kept through a restore. */
static void maker_checks(void)
{
    static GameState saved;
    int type, k, list[51], n, cost, stored, factor, points, skill, before, gold, made;
    const df_u8 *spell;
    df_u32 book, rows[OBJECTS];
    const SpellDef *def;

    maker_new();
    spell = maker_state()->spell;
    check_expect(spell[0] == 255 && spell[5] == 255 && spell[6] == 4 && spell[7] == 0 && spell[14] == 1 && spell[47] == 0 && maker_targets() == 2,
                 "maker: a new spell is not empty, magic and on the caster");
    n = maker_effect_list(list, 51);
    check_expect(n > 20, "maker: an empty spell offers few effects");
    type = -1;
    for (k = 0; k < 51; k++) { if (!strcmp(effect_name(k), "Damage")) { type = k; } }
    check_expect(type >= 0 && maker_pick_effect(type) == 0, "maker: Damage could not be picked");
    if (type < 0) { return; }
    check_expect(maker_targets() == effect_target_class(type) && (maker_targets() != 1 || spell[7] == 1), "maker: the target did not follow the effect");
    check_expect(maker_subtype_count(type) == 3 && !maker_state()->kind, "maker: Damage does not ask for a subtype first");
    maker_pick_subtype(0);
    check_expect(maker_state()->kind == effect_flags(type, 0) && maker_state()->kind != 0, "maker: the subtype did not open its numbers");
    /* The numbers: 1 at least, a limit at most, a high number never under its low one. */
    for (k = 0; k < 11; k++) { check_expect(maker_spin_allowed(k) == (((k < 3) && (maker_state()->kind & 1)) || ((k < 6) && (maker_state()->kind & 2)) || (maker_state()->kind & 4)), "maker: a spinner answers as the original's test says"); }
    if (maker_spin_allowed(6)) {
        for (k = 0; k < 130; k++) { maker_adjust(6, 1); }
        check_expect(maker_value(6) == 100 && maker_value(7) == 100, "maker: the low magnitude did not stop at 100 and lift the high one with it");
        for (k = 0; k < 130; k++) { maker_adjust(7, -1); }
        check_expect(maker_value(7) == 1 && maker_value(6) == 1, "maker: the high magnitude did not stop at 1 and bring the low one down");
        for (k = 0; k < 15; k++) { maker_adjust(10, 1); }
        check_expect(maker_value(10) == 16, "maker: the per-level magnitude did not climb");
        for (k = 0; k < 80; k++) { maker_adjust(10, 1); }
        check_expect(maker_value(10) == 20, "maker: the per-level magnitude passed its limit of 20");
        for (k = 0; k < 7; k++) { maker_adjust(7, 1); }
        for (k = 0; k < 4; k++) { maker_adjust(6, 1); }
    }
    maker_recompute();
    stored = (short)(spell[8] | (spell[9] << 8));
    check_expect(stored > 0, "maker: the effect has no cost stored");
    factor = target_factor(spell[7]);
    check_expect(maker_price() == stored * factor * 2 && maker_shown() == (((stored * factor) >> 1) << 2), "maker: the price or the window's number is not worked as the original has it");
    skill = character_state()->skill[effect_school(type)];
    points = (factor * (((110 - skill) * stored) / 100)) >> 1;
    check_expect(maker_spell_points() == (points < 5 ? 5 : points), "maker: the spell points are not the skill-lowered cost of the stored number");
    /* Buying: the empty, the unnamed and the poor are refused; a bought spell is in the book, paid for and kept in the state. */
    maker_delete(0);
    check_expect(maker_buy() == 0, "maker: an empty spell was bought");
    maker_pick_effect(type); maker_pick_subtype(0);
    for (k = 0; k < 9; k++) { maker_adjust(6, 1); maker_adjust(7, 1); }
    maker_recompute();
    check_expect(maker_buy() == 1704, "maker: an unnamed spell was bought");
    maker_set_name("Test Bolt");
    character_state()->gold = 0;
    check_expect(maker_buy() == 1702, "maker: a spell was bought with no gold");
    book = inventory_book();
    if (!book) { return; }
    before = spell_rows(book, rows, OBJECTS);
    cost = maker_price(); k = spell[7];
    character_state()->gold = cost + 1000; gold = character_state()->gold;
    made = maker_buy();
    check_expect(made == 1705 && character_state()->gold == gold - cost && spell_rows(book, rows, OBJECTS) == before + 1, "maker: the bought spell was not paid for and added to the book");
    def = spell_def(CUSTOM_SPELL_ID);
    check_expect(def && !strcmp(def->name, "Test Bolt") && def->group[0] == type && def->target == k && maker_state()->spell[0] == 255, "maker: the bought spell is not registered, or the maker did not begin again");
    cost = def ? spell_cost(def, character_state()) : 0;
    saved = *game_state();
    spells_sync_custom(0);
    check_expect(spell_def(CUSTOM_SPELL_ID) == 0, "maker: the registry kept a spell of another game");
    check_expect(state_restore(&saved), "maker: the state with a made spell was refused");
    check_expect(spell_def(CUSTOM_SPELL_ID) != 0, "maker: the made spell was not registered again by the restore");
    check_expect(spell_def(CUSTOM_SPELL_ID) && spell_cost(spell_def(CUSTOM_SPELL_ID), character_state()) == cost, "maker: the made spell cost another number after the restore");
}

void check_spellshop(void)
{
    int a[SPELLSHOP_MOST], b[SPELLSHOP_MOST], n, m, i, c, before;
    const SpellDef *s;
    df_u32 book;

    book = inventory_book();
    check_expect(spell_standard_count() == 89, "spellshop: SPELLS.STD does not hold the eighty-nine spells");
    if (!book) {
        return;
    }
    /* The same person offers the same spells, a better hall offers more, and the stream is put back where it was. */
    random_seed(99);
    n = spellshop_list(1234u, 10, a, SPELLSHOP_MOST);
    check_expect(n > 0, "spellshop: a person offered nothing");
    m = spellshop_list(1234u, 10, b, SPELLSHOP_MOST);
    check_expect(n == m && !memcmp(a, b, (size_t)n * sizeof(int)), "spellshop: the same person offered other spells");
    c = spellshop_list(1235u, 10, b, SPELLSHOP_MOST);
    check_expect(c > 0 && (c != n || memcmp(a, b, (size_t)n * sizeof(int))), "spellshop: another person offered the same spells");
    n = spellshop_list(7u, 1, a, SPELLSHOP_MOST);
    m = spellshop_list(7u, 20, b, SPELLSHOP_MOST);
    check_expect(n > 0 && m >= n, "spellshop: a better hall did not offer at least as many spells");
    for (i = 0; i < m; i++) {
        s = spell_def(b[i]);
        check_expect(s && s->name[0] && s->name[0] != '!', "spellshop: an unnamed spell was offered");
    }
    s = spell_def(b[0]);
    check_expect(spellshop_price(s, 0) == spell_cost(s, character_state()) * 4 && spellshop_price(s, 43) == spellshop_price(s, 0) / 2, "spellshop: the price is not four times the cost, half on the holiday 43");
    {
        df_u32 rows[OBJECTS];

        before = spell_rows(book, rows, OBJECTS);
        check_expect(spellshop_add(b[0]) && spell_rows(book, rows, OBJECTS) == before + 1, "spellshop: a bought spell is not in the book");
    }
    maker_checks();
}
