#include <string.h>
#include "check_ogmagicitem.h"
#include "check_util.h"
#include "engine/assets/vfs.h"
#include "game/ogfall/play/magicitem.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/itemmaker.h"
#include "game/ogfall/play/progress.h"
#include "game/ogfall/play/enchant.h"
#include "game/ogfall/play/play.h"
#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/tables/itemdefs.h"

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

static int g_souls_given[2], g_souls_count, g_consumed = -1;
static int give_souls(int *ids, int cap) { int i; for (i = 0; i < g_souls_count && i < cap; i++) { ids[i] = g_souls_given[i]; } return g_souls_count; }
static void eat_soul(int creature) { g_consumed = creature; }
static int kind_offered(int side, int kind)
{
    int list[16], n, i;

    n = og_itemmaker_kinds(side, list, 16);
    for (i = 0; i < n; i++) { if (list[i] == kind) { return 1; } }
    return 0;
}
static int param_offered(int q)
{
    int list[64], n, i;

    n = og_itemmaker_params(list, 64);
    for (i = 0; i < n; i++) { if (list[i] == q) { return 1; } }
    return 0;
}
static int find_item(int group, const char *name)
{
    int i;

    for (i = 0; i < og_item_kinds(group); i++) { if (!strcmp(og_item_def(group, i)->name, name)) { return i; } }
    return -1;
}

/* The item maker's rules (docs/item-making-math.md sections 4 and 6, and the bugs docs/ogfall-itemmaker.md fixes). */
static void itemmaker_checks(void)
{
    static OgState saved;
    df_u32 sword, dagger, daedric, ring, pack;
    int longsword, dag, rg, k, n, list[16], text;
    char said[300];

    pack = og_state()->inventory_id;
    longsword = find_item(3, "Longsword"); dag = find_item(3, "Dagger"); rg = find_item(25, "Ring");
    check_expect(longsword >= 0 && dag >= 0 && rg >= 0, "itemmaker: a longsword, a dagger or a ring is not in the tables");
    if (longsword < 0 || dag < 0 || rg < 0) { return; }
    sword = og_item_create(pack, 3, longsword, 1);
    dagger = og_item_create(pack, 3, dag, 0);
    daedric = og_item_create(pack, 3, longsword, 9);
    ring = og_item_create(pack, 25, rg, 0);
    check_expect(og_itemmaker_capacity(sword) == 600 && og_itemmaker_capacity(dagger) == 150 && og_itemmaker_capacity(daedric) == 1800 && og_itemmaker_capacity(ring) == 1800,
                 "itemmaker: a steel longsword is not 600 points, an iron dagger 150, a Daedric longsword 1800 and a ring 1800");
    og_character()->gold = 1000000;
    og_itemmaker_reset();
    check_expect(og_itemmaker_begin(0, 4) == -OG_IM_NO_ITEM, "itemmaker: a power was added with no item");
    check_expect(og_itemmaker_pick(sword) == 0, "itemmaker: the steel longsword was not taken");
    /* Section 6: Potent vs undead, Enhances skill Stealth, Item deteriorates all the time. */
    check_expect(og_itemmaker_begin(0, 4) == 0 && og_itemmaker()->pending == 0, "itemmaker: Potent vs did not wait for its parameter");
    check_expect(param_offered(0) && param_offered(3) && !param_offered(4), "itemmaker: Potent vs does not offer its four kinds");
    og_itemmaker_choose(0);
    check_expect(og_itemmaker_begin(0, 10) == 1, "itemmaker: Enhances skill did not take the next slot");
    og_itemmaker_choose(16);
    check_expect(og_itemmaker_begin(1, 1) == 2, "itemmaker: Item deteriorates did not take the third slot");
    og_itemmaker_choose(0);
    check_expect(og_itemmaker_points_used() == 800 + 900 - 3000 && og_itemmaker_gold() == (800 + 900) * 10, "itemmaker: the points or the gold are not the original's worked example");
    /* The bugs fixed: Low damage vs the same kind, Item deteriorates again, the kinds. */
    check_expect(!kind_offered(1, 1), "itemmaker: Item deteriorates is offered again with every parameter taken (7.1 and 7.3)");
    check_expect(og_itemmaker_begin(1, 5) == 3 && !param_offered(0) && param_offered(1) && param_offered(3), "itemmaker: Low damage vs undead is offered with Potent vs undead on the item (7.1)");
    og_itemmaker_cancel();
    check_expect(og_itemmaker_free_slots() == 7, "itemmaker: Escape on the parameters left the half-made slot (7.2)");
    k = og_itemmaker_enchant(eat_soul);
    check_expect(k == OG_IM_DONE && og_itemmaker_row(sword) != 0, "itemmaker: the item was not enchanted");
    {
        const OgItemEnchant *row;

        row = og_itemmaker_row(sword);
        check_expect(row && row->type[0] == 4 && row->param[0] == 0 && row->type[1] == 10 && row->param[1] == 16 && row->type[2] == 16 && row->param[2] == 0 && row->type[3] == -1 && row->identified,
                     "itemmaker: the stored enchantments are not (4, 0), (10, 16), (16, 0), nothing");
    }
    check_expect(og_itemmaker_value(sword) == 1700, "itemmaker: the enchanted value is not 800 + 900");
    og_itemmaker_text(sword, said, sizeof(said));
    check_expect(strstr(said, "Potent vs undead") && strstr(said, "Enhances skill Stealth") && strstr(said, "Item deteriorates all the time"), "itemmaker: the powers text does not name the enchantments");
    check_expect(strstr(og_words(1016), "%mpw") != 0, "itemmaker: TEXT.RSC 1016 has no %mpw for the item's powers");
    check_expect(og_itemmaker_pick(sword) == OG_IM_ENCHANTED, "itemmaker: an enchanted item was taken again");
    /* inv_refresh_magic_values: the value is the powers' cost, worked again; and the same lines for a guild's magic item. */
    og_object_edit(sword)->value = 5;
    og_itemmaker_refresh_values();
    check_expect(og_object(sword)->value == 1700 && og_item_enchanted(sword) && !og_item_enchanted(dagger), "itemmaker: the inventory did not work an enchanted item's value again");
    {
        df_u32 magic;

        magic = og_magic_item_create(pack, -1);
        check_expect(magic == 0 || (og_item_enchanted(magic) && og_item_powers_text(magic, said, sizeof(said)) > 0), "itemmaker: a guild's magic item has no powers text");
    }
    saved = *og_state();
    check_expect(og_state_restore(&saved) && og_itemmaker_row(sword) && og_itemmaker_value(sword) == 1700, "itemmaker: the enchantments were not kept through the state");
    /* Capacity and gold refused. */
    og_itemmaker_reset();
    check_expect(og_itemmaker_pick(dagger) == 0, "itemmaker: the iron dagger was not taken");
    og_itemmaker_begin(0, 3); og_itemmaker_choose(0);
    check_expect(og_itemmaker_enchant(0) == OG_IM_NO_POINTS, "itemmaker: Extra spell pts (500) went on a 150 point dagger");
    og_itemmaker_remove(0);
    og_itemmaker_begin(0, 0); og_itemmaker_choose(0);
    check_expect(og_itemmaker_points_used() == og_magic_spell_cost(4) && og_itemmaker_gold() == og_magic_spell_cost(4) * 10, "itemmaker: Cast when used Levitate does not cost the spell's cost in points and ten times in gold");
    og_character()->gold = 0;
    check_expect(og_itemmaker_enchant(0) == OG_IM_NO_POINTS || og_itemmaker_enchant(0) == OG_IM_NO_GOLD, "itemmaker: a spell with no gold went through");
    og_itemmaker_reset();
    check_expect(og_itemmaker_enchant(0) == OG_IM_NO_ITEM, "itemmaker: nothing and no item was enchanted");
    /* Reputation and the kinds without parameters. */
    og_character()->gold = 1000000;
    og_itemmaker_pick(ring);
    og_itemmaker_begin(0, 14); og_itemmaker_choose(2);
    check_expect(og_itemmaker_begin(1, 10) >= 0 && !param_offered(2) && param_offered(1), "itemmaker: Bad rep with offers the group Good rep has");
    og_itemmaker_cancel();
    og_itemmaker_begin(0, 11);
    check_expect(!kind_offered(0, 11) && !kind_offered(1, 8) && kind_offered(1, 9), "itemmaker: Feather Weight does not rule out Extra weight and itself");
    og_itemmaker_begin(0, 8);
    check_expect(!kind_offered(0, 8), "itemmaker: Repairs objects can be added twice (7.7)");
    og_itemmaker_remove(0);
    check_expect(og_itemmaker_free_slots() == 8 && kind_offered(0, 14), "itemmaker: a slot removed did not come back");
    /* A soul: the Daedra lord's, 8000 points against it, its powers free, the gold of Potent vs Daedra. */
    og_itemmaker_reset();
    og_itemmaker_pick(ring);
    g_souls_given[0] = 31; g_souls_count = 1;
    og_itemmaker_set_souls(give_souls);
    check_expect(og_itemmaker_begin(1, 0) == 0 && param_offered(31), "itemmaker: Soul bound did not list the soul");
    check_expect(og_itemmaker_choose(31) == 0, "itemmaker: the Daedra lord's powers did not fit");
    check_expect(og_itemmaker_free_slots() == 6 && og_itemmaker_points_used() == -8000 && og_itemmaker_gold() == 900 * 10 && !kind_offered(1, 0),
                 "itemmaker: a Daedra lord's soul is not -8000 points with Potent vs Daedra for 9000 gold, and one soul only");
    og_itemmaker_remove(1);
    check_expect(og_itemmaker_free_slots() == 6, "itemmaker: a soul-granted slot was removed alone");
    og_itemmaker_remove(0);
    check_expect(og_itemmaker_free_slots() == 10, "itemmaker: the soul did not take its powers with it");
    og_itemmaker_begin(1, 0); og_itemmaker_choose(31);
    g_consumed = -1;
    check_expect(og_itemmaker_enchant(eat_soul) == OG_IM_DONE && g_consumed == 31, "itemmaker: the soul bound was not consumed");
    /* Slice 1 of the enchantments in play: a held spell lasting while the ring is worn, Enhances skill's +15 and Absorbs spells' condition. */
    {
        df_u32 r1, r2, r3;
        int slot, held, skill_before, i;

        r1 = og_item_create(pack, 25, rg, 0); r2 = og_item_create(pack, 25, rg, 0); r3 = og_item_create(pack, 25, rg, 0);
        og_itemmaker_reset(); og_itemmaker_pick(r1);
        og_itemmaker_begin(0, 1); og_itemmaker_choose(0);
        og_itemmaker_begin(0, 10); og_itemmaker_choose(16);
        check_expect(og_itemmaker_enchant(0) == OG_IM_DONE, "enchant: the held ring was not made");
        og_itemmaker_reset(); og_itemmaker_pick(r2);
        og_itemmaker_begin(0, 9);
        check_expect(og_itemmaker_enchant(0) == OG_IM_DONE, "enchant: the absorbing ring was not made");
        skill_before = og_character()->skill[16];
        og_character()->conditions &= ~0x200;
        og_itemmaker_reset(); og_itemmaker_pick(r3);
        og_itemmaker_begin(0, 9);
        check_expect(og_itemmaker_enchant(0) == OG_IM_DONE, "enchant: the third ring was not made");
        check_expect(og_inventory_equip(r1), "enchant: the enchanted ring was not worn");
        slot = og_item_equipped(r1);
        held = 0;
        for (i = 0; i < OG_EFFECTS; i++) {
            const OgEffect *e;

            e = &og_magic_state()->effect[i];
            if (e->active && e->icon == 200 + slot && e->spell == 37 && e->permanent) { held++; }
        }
        check_expect(held > 0, "enchant: Cast when held left no lasting effect with icon 200 + the slot");
        check_expect(og_character()->skill[16] == skill_before + 15, "enchant: Enhances skill did not add 15");
        check_expect(og_inventory_equip(r2) && (og_character()->conditions & 0x200), "enchant: Absorbs spells did not set its condition");
        check_expect(og_inventory_equip(r3), "enchant: the second absorbing ring was not worn");
        og_inventory_unequip(r1);
        held = 0;
        for (i = 0; i < OG_EFFECTS; i++) { const OgEffect *e; e = &og_magic_state()->effect[i]; if (e->active && e->icon == 200 + slot) { held++; } }
        check_expect(held == 0 && og_character()->skill[16] == skill_before && (og_character()->conditions & 0x200), "enchant: taking the ring off did not end its spell and skill, or cleared the other ring's absorption");
        og_inventory_unequip(r2);
        check_expect(og_character()->conditions & 0x200, "enchant: one absorbing ring off cleared the other's absorption");
        og_inventory_unequip(r3);
        check_expect(!(og_character()->conditions & 0x200), "enchant: absorption stayed with no ring worn");
    }
    /* Slice 3: using an item: Cast when used on the caster at once and wearing 10, a spell that has to be aimed readied, Health leech whenever used. */
    {
        df_u32 wand, aim_ring, leech;
        int k2, free_row, hp, cond, effects_before, effects_after, i;

        wand = og_item_create(pack, 25, rg, 0); aim_ring = og_item_create(pack, 25, rg, 0); leech = og_item_create(pack, 25, rg, 0);
        for (free_row = 0; og_item_enchant_edit(free_row)->item; free_row++) { }
        {
            OgItemEnchant *row;

            row = og_item_enchant_edit(free_row);
            row->item = wand; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            row->type[0] = 0; row->param[0] = 4;
            for (free_row++; og_item_enchant_edit(free_row)->item; free_row++) { }
            row = og_item_enchant_edit(free_row);
            row->item = aim_ring; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            row->type[0] = 0; row->param[0] = 7;
            for (free_row++; og_item_enchant_edit(free_row)->item; free_row++) { }
            row = og_item_enchant_edit(free_row);
            row->item = leech; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            row->type[0] = 21; row->param[0] = 0;
        }
        og_object_edit(wand)->condition = og_object_edit(wand)->max_condition = 500; cond = 500;
        effects_before = 0; for (i = 0; i < OG_EFFECTS; i++) { if (og_magic_state()->effect[i].active) { effects_before++; } }
        check_expect(og_possessions_use(wand), "use: an item with Cast when used could not be used");
        effects_after = 0; for (i = 0; i < OG_EFFECTS; i++) { if (og_magic_state()->effect[i].active) { effects_after++; } }
        check_expect(effects_after > effects_before && og_object(wand)->condition == cond - 10, "use: Levitate was not cast on the user or the item not worn by 10");
        og_object_edit(aim_ring)->condition = og_object_edit(aim_ring)->max_condition = 500;
        og_magic_state()->ready = 0;
        check_expect(og_item_use(aim_ring) == 1 && og_magic_pending() && og_object(aim_ring)->condition == 490, "use: a spell to be aimed was not readied and worn by 10");
        og_magic_abort();
        og_character()->health = og_character()->max_health; hp = og_character()->health;
        og_object_edit(leech)->condition = og_object_edit(leech)->max_condition = 500;
        check_expect(og_item_use(leech) == 0 && og_character()->health == hp - ((og_character()->level >> 1) + 1) && og_object(leech)->condition == 498, "use: Health leech did not cost the user health and wear the item by 2");
    }
    /* Slice 2: wear and breaking, Regens health, Item deteriorates, Extra spell pts and Health leech by the tick. */
    {
        df_u32 plain, ench, regen;
        int year, month, day, weekday, season, before_max, h, i, k2;
        static const int seasons[12] = {0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 0, 0};

        plain = og_item_create(pack, 3, dag, 0);
        og_object_edit(plain)->condition = 10;
        og_item_damage(plain, 3);
        check_expect(og_object(plain)->condition == 7, "wear: a dagger did not lose 3 condition");
        og_item_damage(plain, 7);
        check_expect(og_object(plain) && og_object(plain)->condition == 0, "wear: a plain item did not stay broken in the pack");
        regen = og_item_create(pack, 25, rg, 0);
        ench = og_item_create(pack, 25, rg, 0);
        {
            OgItemEnchant *row;
            int free_row;

            for (free_row = 0; og_item_enchant_edit(free_row)->item; free_row++) { }
            row = og_item_enchant_edit(free_row);
            row->item = regen; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            row->type[0] = 5; row->param[0] = 0; row->type[1] = 16; row->param[1] = 0;
            for (free_row++; og_item_enchant_edit(free_row)->item; free_row++) { }
            row = og_item_enchant_edit(free_row);
            row->item = ench; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            og_state_date(&year, &month, &day, &weekday);
            season = seasons[month % 12];
            row->type[0] = 3; row->param[0] = season;
        }
        og_object_edit(regen)->condition = og_object_edit(regen)->max_condition = 500;
        og_character()->health = og_character()->max_health - 20; h = og_character()->health;
        check_expect(og_inventory_equip(regen), "tick: the regenerating ring was not worn");
        og_state_pass_minutes(8);
        check_expect(og_character()->health == h + 2 && og_object(regen)->condition <= 500 - 2 && og_object(regen)->condition >= 500 - 3, "tick: eight minutes did not give 2 health and cost 2 condition");
        og_state_pass_minutes(100);
        check_expect(og_object(regen)->condition < 500 - 1, "tick: a hundred minutes cost nothing");
        i = og_object(regen)->condition;
        og_state_pass_minutes(150);
        check_expect(og_object(regen)->condition == i, "tick: a gap of 150 minutes was not thrown away");
        og_inventory_unequip(regen);
        before_max = og_character()->max_magicka;
        og_inventory_equip(ench);
        og_enchant_frame();
        check_expect(og_character()->max_magicka == og_magicka(og_character()->attribute[OG_INT], og_character()->class_flags) + 75, "tick: Extra spell pts did not add 75 in its season");
        og_inventory_unequip(ench);
        og_enchant_frame();
        check_expect(og_character()->max_magicka == og_magicka(og_character()->attribute[OG_INT], og_character()->class_flags), "tick: Extra spell pts stayed after the item came off");
        (void)before_max;
        /* The item that deteriorates breaks and is gone; the worn effects with it. */
        og_object_edit(regen)->condition = 3;
        og_inventory_equip(regen);
        og_state_pass_minutes(40);
        check_expect(!og_object(regen) && og_item_equipped(regen) < 0, "wear: an enchanted item that wore out was not gone and unequipped");
    }
    (void)daedric; (void)text; (void)n; (void)list;
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
    itemmaker_checks();
}
