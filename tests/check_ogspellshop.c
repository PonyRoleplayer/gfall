#include <string.h>
#include "check_ogspellshop.h"
#include "check_util.h"
#include "game/ogfall/play/spellshop.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/magic.h"

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
}
