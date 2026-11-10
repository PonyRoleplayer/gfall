#include <string.h>
#include "check_ogmagicitem.h"
#include "check_util.h"
#include "engine/assets/vfs.h"
#include "game/ogfall/play/magicitem.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"

static int count_in(df_u32 container, int group, int index)
{
    int i, n;

    for (i = n = 0; i < OG_OBJECTS; i++) {
        const OgObject *o;

        o = &og_state()->object[i];
        if (o->id && o->parent == container && o->type == OG_RECORD_ITEM && (group < 0 || (o->group == group && o->index == index))) { n++; }
    }
    return n;
}

void check_ogmagicitem(void)
{
    const df_u8 *bytes;
    df_u32 size, container, id;
    int i, artifacts, magic, def;
    char name[40];

    bytes = vfs_read("MAGIC.DEF", &size);
    check_expect(bytes != 0, "magicitem: MAGIC.DEF does not read");
    if (!bytes) {
        return;
    }
    og_magic_defs_set(bytes, size);
    check_expect(og_magic_def_count() == 59, "magicitem: MAGIC.DEF does not hold 59 entries");
    artifacts = 0;
    for (i = 0; i < og_magic_def_count(); i++) { artifacts += og_magic_def_artifact(i); }
    check_expect(artifacts == 23 && !strcmp(og_magic_def_name(0), "The Masque of Clavicus"), "magicitem: 23 artifacts, the first the Masque of Clavicus");
    /* Prices by enchantment alone (worked from enchant_power_costs and enchant_fixed_costs): a life-stealing 1000, leaping 900, magicka 1500. */
    check_expect(og_magic_def_value(18) == 1000 && og_magic_def_value(20) == 900 && og_magic_def_value(22) == 1500 && og_magic_def_value(0) == 0, "magicitem: an entry's enchantments are not priced by the original's costs");
    /* An item of an entry: the entry's name, its price, kept as which entry it is. */
    container = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    id = og_magic_item_create(container, 8);
    def = og_magic_item_def(id);
    check_expect(id && def >= 0 && !og_magic_def_artifact(def) && og_object(id)->value == og_magic_def_value(def) && !strcmp(og_object(id)->name, og_magic_def_name(def)), "magicitem: an item is not of its entry");
    og_item_name(og_object(id), name, sizeof(name));
    check_expect(!strstr(name, "%it"), "magicitem: the macro %it was left in an item's name");
    /* Never an artifact in a shop: many items, none of them one. */
    magic = 0;
    for (i = 0; i < 40; i++) {
        id = og_magic_item_create(container, -1);
        def = og_magic_item_def(id);
        magic += id && def >= 0 && !og_magic_def_artifact(def);
    }
    check_expect(magic == 40, "magicitem: a shop item was an artifact or none");
    /* A shop's shelf: quality / 2 + 1 magic items, a spellbook, and with soul traps as many of them. */
    container = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    og_magic_shop_stock(container, 10, 0);
    check_expect(count_in(container, 27, 0) == 1 && count_in(container, 27, 1) == 0 && count_in(container, -1, 0) == 7, "magicitem: a quality 10 hall did not stock six magic items and a spellbook");
    container = og_object_new(OG_RECORD_CONTAINER, OG_WORLD_ROOT, 0, 1);
    og_magic_shop_stock(container, 10, 1);
    check_expect(count_in(container, 27, 1) == 6, "magicitem: a rank 4 member was not offered soul traps");
}
