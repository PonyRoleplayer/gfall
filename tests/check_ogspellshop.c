#include <string.h>
#include "check_ogspellshop.h"
#include "check_util.h"
#include "game/ogfall/play/spellshop.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/spellmaker.h"
#include "game/ogfall/tables/magicdefs.h"
#include "game/ogfall/tables/itemdefs.h"

/* The spell maker: the empty spell, the effect list by target, a Damage spell's numbers clamped and paired, the stored cost and price by hand, the buying and its refusals, and the spell kept through a restore. */
static void maker_checks(void)
{
    static OgState saved;
    int type, k, list[51], n, cost, stored, factor, points, skill, before, gold, made;
    const df_u8 *spell;
    df_u32 book, rows[OG_OBJECTS];
    const OgSpellDef *def;

    og_maker_new();
    spell = og_maker()->spell;
    check_expect(spell[0] == 255 && spell[5] == 255 && spell[6] == 4 && spell[7] == 0 && spell[14] == 1 && spell[47] == 0 && og_maker_targets() == 2,
                 "maker: a new spell is not empty, magic and on the caster");
    n = og_maker_effect_list(list, 51);
    check_expect(n > 20, "maker: an empty spell offers few effects");
    type = -1;
    for (k = 0; k < 51; k++) { if (!strcmp(og_effect_name(k), "Damage")) { type = k; } }
    check_expect(type >= 0 && og_maker_pick_effect(type) == 0, "maker: Damage could not be picked");
    if (type < 0) { return; }
    check_expect(og_maker_targets() == og_effect_target_class(type) && (og_maker_targets() != 1 || spell[7] == 1), "maker: the target did not follow the effect");
    check_expect(og_maker_subtype_count(type) == 3 && !og_maker()->kind, "maker: Damage does not ask for a subtype first");
    og_maker_pick_subtype(0);
    check_expect(og_maker()->kind == og_effect_flags(type, 0) && og_maker()->kind != 0, "maker: the subtype did not open its numbers");
    /* The numbers: 1 at least, a limit at most, a high number never under its low one. */
    for (k = 0; k < 11; k++) { check_expect(og_maker_spin_allowed(k) == (((k < 3) && (og_maker()->kind & 1)) || ((k < 6) && (og_maker()->kind & 2)) || (og_maker()->kind & 4)), "maker: a spinner answers as the original's test says"); }
    if (og_maker_spin_allowed(6)) {
        for (k = 0; k < 130; k++) { og_maker_adjust(6, 1); }
        check_expect(og_maker_value(6) == 100 && og_maker_value(7) == 100, "maker: the low magnitude did not stop at 100 and lift the high one with it");
        for (k = 0; k < 130; k++) { og_maker_adjust(7, -1); }
        check_expect(og_maker_value(7) == 1 && og_maker_value(6) == 1, "maker: the high magnitude did not stop at 1 and bring the low one down");
        for (k = 0; k < 15; k++) { og_maker_adjust(10, 1); }
        check_expect(og_maker_value(10) == 16, "maker: the per-level magnitude did not climb");
        for (k = 0; k < 80; k++) { og_maker_adjust(10, 1); }
        check_expect(og_maker_value(10) == 20, "maker: the per-level magnitude passed its limit of 20");
        for (k = 0; k < 7; k++) { og_maker_adjust(7, 1); }
        for (k = 0; k < 4; k++) { og_maker_adjust(6, 1); }
    }
    og_maker_recompute();
    stored = (short)(spell[8] | (spell[9] << 8));
    check_expect(stored > 0, "maker: the effect has no cost stored");
    factor = og_target_factor(spell[7]);
    check_expect(og_maker_price() == stored * factor * 2 && og_maker_shown() == (((stored * factor) >> 1) << 2), "maker: the price or the window's number is not worked as the original has it");
    skill = og_character()->skill[og_effect_school(type)];
    points = (factor * (((110 - skill) * stored) / 100)) >> 1;
    check_expect(og_maker_spell_points() == (points < 5 ? 5 : points), "maker: the spell points are not the skill-lowered cost of the stored number");
    /* Buying: the empty, the unnamed and the poor are refused; a bought spell is in the book, paid for and kept in the state. */
    og_maker_delete(0);
    check_expect(og_maker_buy() == 0, "maker: an empty spell was bought");
    og_maker_pick_effect(type); og_maker_pick_subtype(0);
    for (k = 0; k < 9; k++) { og_maker_adjust(6, 1); og_maker_adjust(7, 1); }
    og_maker_recompute();
    check_expect(og_maker_buy() == 1704, "maker: an unnamed spell was bought");
    og_maker_set_name("Test Bolt");
    og_character()->gold = 0;
    check_expect(og_maker_buy() == 1702, "maker: a spell was bought with no gold");
    book = og_inventory_book();
    if (!book) { return; }
    before = og_spell_rows(book, rows, OG_OBJECTS);
    cost = og_maker_price(); k = spell[7];
    og_character()->gold = cost + 1000; gold = og_character()->gold;
    made = og_maker_buy();
    check_expect(made == 1705 && og_character()->gold == gold - cost && og_spell_rows(book, rows, OG_OBJECTS) == before + 1, "maker: the bought spell was not paid for and added to the book");
    def = og_spell_def(OG_CUSTOM_SPELL_ID);
    check_expect(def && !strcmp(def->name, "Test Bolt") && def->group[0] == type && def->target == k && og_maker()->spell[0] == 255, "maker: the bought spell is not registered, or the maker did not begin again");
    cost = def ? og_spell_cost(def, og_character()) : 0;
    saved = *og_state();
    og_spells_sync_custom(0);
    check_expect(og_spell_def(OG_CUSTOM_SPELL_ID) == 0, "maker: the registry kept a spell of another game");
    check_expect(og_state_restore(&saved), "maker: the state with a made spell was refused");
    check_expect(og_spell_def(OG_CUSTOM_SPELL_ID) != 0, "maker: the made spell was not registered again by the restore");
    check_expect(og_spell_def(OG_CUSTOM_SPELL_ID) && og_spell_cost(og_spell_def(OG_CUSTOM_SPELL_ID), og_character()) == cost, "maker: the made spell cost another number after the restore");
}

void check_ogspellshop(void)
{
    int a[OG_SPELLSHOP_MOST], b[OG_SPELLSHOP_MOST], n, m, i, c, before;
    const OgSpellDef *s;
    df_u32 book;

    book = og_inventory_book();
    check_expect(og_spell_standard_count() == 89, "spellshop: SPELLS.STD does not hold the eighty-nine spells");
    if (!book) {
        return;
    }
    /* The same person offers the same spells, a better hall offers more, and the stream is put back where it was. */
    og_random_seed(99);
    n = og_spellshop_list(1234u, 10, a, OG_SPELLSHOP_MOST);
    check_expect(n > 0, "spellshop: a person offered nothing");
    m = og_spellshop_list(1234u, 10, b, OG_SPELLSHOP_MOST);
    check_expect(n == m && !memcmp(a, b, (size_t)n * sizeof(int)), "spellshop: the same person offered other spells");
    c = og_spellshop_list(1235u, 10, b, OG_SPELLSHOP_MOST);
    check_expect(c > 0 && (c != n || memcmp(a, b, (size_t)n * sizeof(int))), "spellshop: another person offered the same spells");
    n = og_spellshop_list(7u, 1, a, OG_SPELLSHOP_MOST);
    m = og_spellshop_list(7u, 20, b, OG_SPELLSHOP_MOST);
    check_expect(n > 0 && m >= n, "spellshop: a better hall did not offer at least as many spells");
    for (i = 0; i < m; i++) {
        s = og_spell_def(b[i]);
        check_expect(s && s->name[0] && s->name[0] != '!', "spellshop: an unnamed spell was offered");
    }
    s = og_spell_def(b[0]);
    check_expect(og_spellshop_price(s, 0) == og_spell_cost(s, og_character()) * 4 && og_spellshop_price(s, 43) == og_spellshop_price(s, 0) / 2, "spellshop: the price is not four times the cost, half on the holiday 43");
    {
        df_u32 rows[OG_OBJECTS];

        before = og_spell_rows(book, rows, OG_OBJECTS);
        check_expect(og_spellshop_add(b[0]) && og_spell_rows(book, rows, OG_OBJECTS) == before + 1, "spellshop: a bought spell is not in the book");
    }
    maker_checks();
}
