#include <string.h>
#include "check_magicitem.h"
#include "check_util.h"
#include "engine/assets/vfs.h"
#include "game/dagger/play/magicitem.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/play/itemmaker.h"
#include "game/dagger/play/progress.h"
#include "game/dagger/play/enchant.h"
#include "game/dagger/play/possessions.h"
#include "game/dagger/play/play.h"
#include "game/dagger/play/magic.h"
#include "game/dagger/play/spellbook.h"
#include "game/dagger/tables/itemdefs.h"

static int count_in(df_u32 container, int group, int index)
{
    int i, n;

    for (i = n = 0; i < OBJECTS; i++) {
        const ObjectInfo *o;

        o = &game_state()->object[i];
        if (o->id && o->parent == container && o->type == RECORD_ITEM && (group < 0 || (o->group == group && o->index == index))) { n++; }
    }
    return n;
}

static int g_souls_given[2], g_souls_count, g_consumed = -1;
static int give_souls(int *ids, int cap) { int i; for (i = 0; i < g_souls_count && i < cap; i++) { ids[i] = g_souls_given[i]; } return g_souls_count; }
static void eat_soul(int creature) { g_consumed = creature; }
static int kind_offered(int side, int kind)
{
    int list[16], n, i;

    n = itemmaker_kinds(side, list, 16);
    for (i = 0; i < n; i++) { if (list[i] == kind) { return 1; } }
    return 0;
}
static int param_offered(int q)
{
    int list[64], n, i;

    n = itemmaker_params(list, 64);
    for (i = 0; i < n; i++) { if (list[i] == q) { return 1; } }
    return 0;
}
static int find_item(int group, const char *name)
{
    int i;

    for (i = 0; i < item_kinds(group); i++) { if (!strcmp(item_def(group, i)->name, name)) { return i; } }
    return -1;
}

/* The item maker's rules (docs/item-making-math.md sections 4 and 6, and the bugs docs/game-itemmaker.md fixes). */
static void itemmaker_checks(void)
{
    static GameState saved;
    df_u32 sword, dagger, daedric, ring, pack;
    int longsword, dag, rg, k, n, list[16], text;
    char said[300];

    pack = game_state()->inventory_id;
    longsword = find_item(3, "Longsword"); dag = find_item(3, "Dagger"); rg = find_item(25, "Ring");
    check_expect(longsword >= 0 && dag >= 0 && rg >= 0, "itemmaker: a longsword, a dagger or a ring is not in the tables");
    if (longsword < 0 || dag < 0 || rg < 0) { return; }
    sword = item_create(pack, 3, longsword, 1);
    dagger = item_create(pack, 3, dag, 0);
    daedric = item_create(pack, 3, longsword, 9);
    ring = item_create(pack, 25, rg, 0);
    check_expect(itemmaker_capacity(sword) == 600 && itemmaker_capacity(dagger) == 150 && itemmaker_capacity(daedric) == 1800 && itemmaker_capacity(ring) == 1800,
                 "itemmaker: a steel longsword is not 600 points, an iron dagger 150, a Daedric longsword 1800 and a ring 1800");
    character_state()->gold = 1000000;
    itemmaker_reset();
    check_expect(itemmaker_begin(0, 4) == -IM_NO_ITEM, "itemmaker: a power was added with no item");
    check_expect(itemmaker_pick(sword) == 0, "itemmaker: the steel longsword was not taken");
    /* Section 6: Potent vs undead, Enhances skill Stealth, Item deteriorates all the time. */
    check_expect(itemmaker_begin(0, 4) == 0 && itemmaker_state()->pending == 0, "itemmaker: Potent vs did not wait for its parameter");
    check_expect(param_offered(0) && param_offered(3) && !param_offered(4), "itemmaker: Potent vs does not offer its four kinds");
    itemmaker_choose(0);
    check_expect(itemmaker_begin(0, 10) == 1, "itemmaker: Enhances skill did not take the next slot");
    itemmaker_choose(16);
    check_expect(itemmaker_begin(1, 1) == 2, "itemmaker: Item deteriorates did not take the third slot");
    itemmaker_choose(0);
    check_expect(itemmaker_points_used() == 800 + 900 - 3000 && itemmaker_gold() == (800 + 900) * 10, "itemmaker: the points or the gold are not the original's worked example");
    /* The bugs fixed: Low damage vs the same kind, Item deteriorates again, the kinds. */
    check_expect(!kind_offered(1, 1), "itemmaker: Item deteriorates is offered again with every parameter taken (7.1 and 7.3)");
    check_expect(itemmaker_begin(1, 5) == 3 && !param_offered(0) && param_offered(1) && param_offered(3), "itemmaker: Low damage vs undead is offered with Potent vs undead on the item (7.1)");
    itemmaker_cancel();
    check_expect(itemmaker_free_slots() == 7, "itemmaker: Escape on the parameters left the half-made slot (7.2)");
    k = itemmaker_enchant(eat_soul);
    check_expect(k == IM_DONE && itemmaker_row(sword) != 0, "itemmaker: the item was not enchanted");
    {
        const ItemEnchant *row;

        row = itemmaker_row(sword);
        check_expect(row && row->type[0] == 4 && row->param[0] == 0 && row->type[1] == 10 && row->param[1] == 16 && row->type[2] == 16 && row->param[2] == 0 && row->type[3] == -1 && row->identified,
                     "itemmaker: the stored enchantments are not (4, 0), (10, 16), (16, 0), nothing");
    }
    check_expect(itemmaker_value(sword) == 1700, "itemmaker: the enchanted value is not 800 + 900");
    itemmaker_text(sword, said, sizeof(said));
    check_expect(strstr(said, "Potent vs undead") && strstr(said, "Enhances skill Stealth") && strstr(said, "Item deteriorates all the time"), "itemmaker: the powers text does not name the enchantments");
    check_expect(strstr(words_state(1016), "%mpw") != 0, "itemmaker: TEXT.RSC 1016 has no %mpw for the item's powers");
    check_expect(itemmaker_pick(sword) == IM_ENCHANTED, "itemmaker: an enchanted item was taken again");
    /* inv_refresh_magic_values: the value is the powers' cost, worked again; and the same lines for a guild's magic item. */
    object_edit(sword)->value = 5;
    itemmaker_refresh_values();
    check_expect(object_at(sword)->value == 1700 && item_enchanted(sword) && !item_enchanted(dagger), "itemmaker: the inventory did not work an enchanted item's value again");
    {
        df_u32 magic;

        magic = magic_item_create(pack, -1);
        check_expect(magic == 0 || (item_enchanted(magic) && item_powers_text(magic, said, sizeof(said)) > 0), "itemmaker: a guild's magic item has no powers text");
    }
    saved = *game_state();
    check_expect(state_restore(&saved) && itemmaker_row(sword) && itemmaker_value(sword) == 1700, "itemmaker: the enchantments were not kept through the state");
    /* Capacity and gold refused. */
    itemmaker_reset();
    check_expect(itemmaker_pick(dagger) == 0, "itemmaker: the iron dagger was not taken");
    itemmaker_begin(0, 3); itemmaker_choose(0);
    check_expect(itemmaker_enchant(0) == IM_NO_POINTS, "itemmaker: Extra spell pts (500) went on a 150 point dagger");
    itemmaker_remove(0);
    itemmaker_begin(0, 0); itemmaker_choose(0);
    check_expect(itemmaker_points_used() == magic_spell_cost(4) && itemmaker_gold() == magic_spell_cost(4) * 10, "itemmaker: Cast when used Levitate does not cost the spell's cost in points and ten times in gold");
    character_state()->gold = 0;
    check_expect(itemmaker_enchant(0) == IM_NO_POINTS || itemmaker_enchant(0) == IM_NO_GOLD, "itemmaker: a spell with no gold went through");
    itemmaker_reset();
    check_expect(itemmaker_enchant(0) == IM_NO_ITEM, "itemmaker: nothing and no item was enchanted");
    /* Reputation and the kinds without parameters. */
    character_state()->gold = 1000000;
    itemmaker_pick(ring);
    itemmaker_begin(0, 14); itemmaker_choose(2);
    check_expect(itemmaker_begin(1, 10) >= 0 && !param_offered(2) && param_offered(1), "itemmaker: Bad rep with offers the group Good rep has");
    itemmaker_cancel();
    itemmaker_begin(0, 11);
    check_expect(!kind_offered(0, 11) && !kind_offered(1, 8) && kind_offered(1, 9), "itemmaker: Feather Weight does not rule out Extra weight and itself");
    itemmaker_begin(0, 8);
    check_expect(!kind_offered(0, 8), "itemmaker: Repairs objects can be added twice (7.7)");
    itemmaker_remove(0);
    check_expect(itemmaker_free_slots() == 8 && kind_offered(0, 14), "itemmaker: a slot removed did not come back");
    /* A soul: the Daedra lord's, 8000 points against it, its powers free, the gold of Potent vs Daedra. */
    itemmaker_reset();
    itemmaker_pick(ring);
    g_souls_given[0] = 31; g_souls_count = 1;
    itemmaker_set_souls(give_souls);
    check_expect(itemmaker_begin(1, 0) == 0 && param_offered(31), "itemmaker: Soul bound did not list the soul");
    check_expect(itemmaker_choose(31) == 0, "itemmaker: the Daedra lord's powers did not fit");
    check_expect(itemmaker_free_slots() == 6 && itemmaker_points_used() == -8000 && itemmaker_gold() == 900 * 10 && !kind_offered(1, 0),
                 "itemmaker: a Daedra lord's soul is not -8000 points with Potent vs Daedra for 9000 gold, and one soul only");
    itemmaker_remove(1);
    check_expect(itemmaker_free_slots() == 6, "itemmaker: a soul-granted slot was removed alone");
    itemmaker_remove(0);
    check_expect(itemmaker_free_slots() == 10, "itemmaker: the soul did not take its powers with it");
    itemmaker_begin(1, 0); itemmaker_choose(31);
    g_consumed = -1;
    check_expect(itemmaker_enchant(eat_soul) == IM_DONE && g_consumed == 31, "itemmaker: the soul bound was not consumed");
    /* Slice 1 of the enchantments in play: a held spell lasting while the ring is worn, Enhances skill's +15 and Absorbs spells' condition. */
    {
        df_u32 r1, r2, r3;
        int slot, held, skill_before, i;

        r1 = item_create(pack, 25, rg, 0); r2 = item_create(pack, 25, rg, 0); r3 = item_create(pack, 25, rg, 0);
        itemmaker_reset(); itemmaker_pick(r1);
        itemmaker_begin(0, 1); itemmaker_choose(0);
        itemmaker_begin(0, 10); itemmaker_choose(16);
        check_expect(itemmaker_enchant(0) == IM_DONE, "enchant: the held ring was not made");
        itemmaker_reset(); itemmaker_pick(r2);
        itemmaker_begin(0, 9);
        check_expect(itemmaker_enchant(0) == IM_DONE, "enchant: the absorbing ring was not made");
        skill_before = character_state()->skill[16];
        character_state()->conditions &= ~0x200;
        itemmaker_reset(); itemmaker_pick(r3);
        itemmaker_begin(0, 9);
        check_expect(itemmaker_enchant(0) == IM_DONE, "enchant: the third ring was not made");
        check_expect(inventory_equip(r1), "enchant: the enchanted ring was not worn");
        slot = item_equipped(r1);
        held = 0;
        for (i = 0; i < EFFECTS; i++) {
            const Effect *e;

            e = &magic_state()->effect[i];
            if (e->active && e->icon == 200 + slot && e->spell == 37 && e->permanent) { held++; }
        }
        check_expect(held > 0, "enchant: Cast when held left no lasting effect with icon 200 + the slot");
        check_expect(character_state()->skill[16] == skill_before + 15, "enchant: Enhances skill did not add 15");
        check_expect(inventory_equip(r2) && (character_state()->conditions & 0x200), "enchant: Absorbs spells did not set its condition");
        check_expect(inventory_equip(r3), "enchant: the second absorbing ring was not worn");
        inventory_unequip(r1);
        held = 0;
        for (i = 0; i < EFFECTS; i++) { const Effect *e; e = &magic_state()->effect[i]; if (e->active && e->icon == 200 + slot) { held++; } }
        check_expect(held == 0 && character_state()->skill[16] == skill_before && (character_state()->conditions & 0x200), "enchant: taking the ring off did not end its spell and skill, or cleared the other ring's absorption");
        inventory_unequip(r2);
        check_expect(character_state()->conditions & 0x200, "enchant: one absorbing ring off cleared the other's absorption");
        inventory_unequip(r3);
        check_expect(!(character_state()->conditions & 0x200), "enchant: absorption stayed with no ring worn");
    }
    /* Slice 5: carrying, weight and reactions. */
    {
        df_u32 belt, feather, heavy, kind_ring, bad_ring;
        int k2, free_row, base, rep0;
        ItemEnchant *row;

        belt = item_create(pack, 25, rg, 0); feather = item_create(pack, 3, longsword, 1); heavy = item_create(pack, 3, dag, 0);
        kind_ring = item_create(pack, 25, rg, 0); bad_ring = item_create(pack, 25, rg, 0);
        {
            df_u32 ids[5];
            int types[5], params[5], n;

            ids[0] = belt; types[0] = 7; params[0] = 0;
            ids[1] = feather; types[1] = 11; params[1] = -1;
            ids[2] = heavy; types[2] = 23; params[2] = -1;
            ids[3] = kind_ring; types[3] = 14; params[3] = 2;
            ids[4] = bad_ring; types[4] = 25; params[4] = 5;
            for (n = 0; n < 5; n++) {
                for (free_row = 0; item_enchant_edit(free_row)->item; free_row++) { }
                row = item_enchant_edit(free_row);
                row->item = ids[n]; row->identified = 1;
                for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
                row->type[0] = types[n]; row->param[0] = params[n];
            }
        }
        base = character_state()->attribute[ATTR_STR] + (character_state()->attribute[ATTR_STR] >> 1);
        inventory_unequip(belt);
        check_expect(inventory_capacity() == base, "carry: the capacity changed with no item worn");
        inventory_equip(belt);
        check_expect(inventory_capacity() == base * 320 / 256, "carry: Increased Weight Allowance did not add a quarter");
        inventory_unequip(belt);
        check_expect(enchant_weight(feather, 40) == 1 && enchant_weight(heavy, 3) == 20 && enchant_weight(heavy, 10) == 40 && enchant_weight(sword, 40) == 40,
                     "weight: Feather Weight is not 1, Extra weight not four times and at least 20, or a plain item changed");
        rep0 = enchant_reaction(2);
        check_expect(rep0 == 10 - 10, "reaction: a carried ring of Good rep with Scholars and one of Bad rep with All did not cancel for Scholars");
        check_expect(enchant_reaction(3) == -10 && enchant_reaction(5) == -10, "reaction: Bad rep with All did not take 10 off for another group");
    }
    /* Slice 4: the hooks of a weapon that hits: Potent vs and Low damage vs by the wielder's level (9.3 fixed), Health leech on a hit, Vampiric when strikes, armour. */
    {
        df_u32 axe, shield;
        int k2, free_row, lvl, hp, cond;
        ItemEnchant *row;

        axe = item_create(pack, 3, longsword, 1); shield = item_create(pack, 25, rg, 0);
        for (free_row = 0; item_enchant_edit(free_row)->item; free_row++) { }
        row = item_enchant_edit(free_row);
        row->item = axe; row->identified = 1;
        for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
        row->type[0] = 4; row->param[0] = 0; row->type[1] = 20; row->param[1] = 1; row->type[2] = 21; row->param[2] = 0; row->type[3] = 6; row->param[3] = 1;
        for (free_row++; item_enchant_edit(free_row)->item; free_row++) { }
        row = item_enchant_edit(free_row);
        row->item = shield; row->identified = 1;
        for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
        row->type[0] = 12; row->param[0] = -1;
        lvl = character_state()->level;
        character_state()->health = character_state()->max_health; hp = character_state()->health;
        check_expect(enchant_hit_bonus(axe, 0) == lvl, "hit: Potent vs undead did not add the wielder's level");
        check_expect(character_state()->health == hp - ((lvl >> 2) + 1), "hit: Health leech did not cost the wielder (level / 4 + 1) on a hit");
        check_expect(enchant_hit_bonus(axe, 1) == -lvl && enchant_hit_bonus(axe, 2) == 0, "hit: Low damage vs Daedra did not take the level off, or another category changed the damage");
        object_edit(axe)->condition = object_edit(axe)->max_condition = 500; cond = 500;
        character_state()->health = character_state()->max_health - 20; hp = character_state()->health;
        enchant_strike(axe, 0, 10);
        check_expect(character_state()->health == hp + 5 && object_at(axe)->condition == cond - (5 / 4 + 1), "strike: Vampiric when strikes did not heal half the damage and wear the weapon");
        state_equip(0, 0);
        check_expect(enchant_armor_modifier() == 0, "armour: a modifier with nothing worn");
        inventory_equip(shield);
        check_expect(enchant_armor_modifier() == -25, "armour: Strengthens armor did not take 25 off");
        inventory_unequip(shield);
    }
    /* Slice 3: using an item: Cast when used on the caster at once and wearing 10, a spell that has to be aimed readied, Health leech whenever used. */
    {
        df_u32 wand, aim_ring, leech;
        int k2, free_row, hp, cond, effects_before, effects_after, i;

        wand = item_create(pack, 25, rg, 0); aim_ring = item_create(pack, 25, rg, 0); leech = item_create(pack, 25, rg, 0);
        for (free_row = 0; item_enchant_edit(free_row)->item; free_row++) { }
        {
            ItemEnchant *row;

            row = item_enchant_edit(free_row);
            row->item = wand; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            row->type[0] = 0; row->param[0] = 4;
            for (free_row++; item_enchant_edit(free_row)->item; free_row++) { }
            row = item_enchant_edit(free_row);
            row->item = aim_ring; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            row->type[0] = 0; row->param[0] = 7;
            for (free_row++; item_enchant_edit(free_row)->item; free_row++) { }
            row = item_enchant_edit(free_row);
            row->item = leech; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            row->type[0] = 21; row->param[0] = 0;
        }
        object_edit(wand)->condition = object_edit(wand)->max_condition = 500; cond = 500;
        effects_before = 0; for (i = 0; i < EFFECTS; i++) { if (magic_state()->effect[i].active) { effects_before++; } }
        check_expect(possessions_use(wand), "use: an item with Cast when used could not be used");
        effects_after = 0; for (i = 0; i < EFFECTS; i++) { if (magic_state()->effect[i].active) { effects_after++; } }
        check_expect(effects_after > effects_before && object_at(wand)->condition == cond - 10, "use: Levitate was not cast on the user or the item not worn by 10");
        object_edit(aim_ring)->condition = object_edit(aim_ring)->max_condition = 500;
        magic_state()->ready = 0;
        check_expect(item_use(aim_ring) == 1 && magic_pending() && object_at(aim_ring)->condition == 490, "use: a spell to be aimed was not readied and worn by 10");
        magic_abort();
        character_state()->health = character_state()->max_health; hp = character_state()->health;
        object_edit(leech)->condition = object_edit(leech)->max_condition = 500;
        check_expect(item_use(leech) == 0 && character_state()->health == hp - ((character_state()->level >> 1) + 1) && object_at(leech)->condition == 498, "use: Health leech did not cost the user health and wear the item by 2");
    }
    /* Slice 2: wear and breaking, Regens health, Item deteriorates, Extra spell pts and Health leech by the tick. */
    {
        df_u32 plain, ench, regen;
        int year, month, day, weekday, season, before_max, h, i, k2;
        static const int seasons[12] = {0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 0, 0};

        plain = item_create(pack, 3, dag, 0);
        object_edit(plain)->condition = 10;
        item_damage(plain, 3);
        check_expect(object_at(plain)->condition == 7, "wear: a dagger did not lose 3 condition");
        item_damage(plain, 7);
        check_expect(object_at(plain) && object_at(plain)->condition == 0, "wear: a plain item did not stay broken in the pack");
        regen = item_create(pack, 25, rg, 0);
        ench = item_create(pack, 25, rg, 0);
        {
            ItemEnchant *row;
            int free_row;

            for (free_row = 0; item_enchant_edit(free_row)->item; free_row++) { }
            row = item_enchant_edit(free_row);
            row->item = regen; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            row->type[0] = 5; row->param[0] = 0; row->type[1] = 16; row->param[1] = 0;
            for (free_row++; item_enchant_edit(free_row)->item; free_row++) { }
            row = item_enchant_edit(free_row);
            row->item = ench; row->identified = 1;
            for (k2 = 0; k2 < 10; k2++) { row->type[k2] = -1; row->param[k2] = -1; }
            state_date(&year, &month, &day, &weekday);
            season = seasons[month % 12];
            row->type[0] = 3; row->param[0] = season;
        }
        object_edit(regen)->condition = object_edit(regen)->max_condition = 500;
        character_state()->health = character_state()->max_health - 20; h = character_state()->health;
        check_expect(inventory_equip(regen), "tick: the regenerating ring was not worn");
        state_pass_minutes(8);
        check_expect(character_state()->health == h + 2 && object_at(regen)->condition <= 500 - 2 && object_at(regen)->condition >= 500 - 3, "tick: eight minutes did not give 2 health and cost 2 condition");
        state_pass_minutes(100);
        check_expect(object_at(regen)->condition < 500 - 1, "tick: a hundred minutes cost nothing");
        i = object_at(regen)->condition;
        state_pass_minutes(150);
        check_expect(object_at(regen)->condition == i, "tick: a gap of 150 minutes was not thrown away");
        inventory_unequip(regen);
        before_max = character_state()->max_magicka;
        inventory_equip(ench);
        enchant_frame();
        check_expect(character_state()->max_magicka == magicka_state(character_state()->attribute[ATTR_INT], character_state()->class_flags) + 75, "tick: Extra spell pts did not add 75 in its season");
        inventory_unequip(ench);
        enchant_frame();
        check_expect(character_state()->max_magicka == magicka_state(character_state()->attribute[ATTR_INT], character_state()->class_flags), "tick: Extra spell pts stayed after the item came off");
        (void)before_max;
        /* The item that deteriorates breaks and is gone; the worn effects with it. */
        object_edit(regen)->condition = 3;
        inventory_equip(regen);
        state_pass_minutes(40);
        check_expect(!object_at(regen) && item_equipped(regen) < 0, "wear: an enchanted item that wore out was not gone and unequipped");
    }
    (void)daedric; (void)text; (void)n; (void)list;
}

void check_magicitem(void)
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
    magic_defs_set(bytes, size);
    check_expect(magic_def_count() == 59, "magicitem: MAGIC.DEF does not hold 59 entries");
    artifacts = 0;
    for (i = 0; i < magic_def_count(); i++) { artifacts += magic_def_artifact(i); }
    check_expect(artifacts == 23 && !strcmp(magic_def_name(0), "The Masque of Clavicus"), "magicitem: 23 artifacts, the first the Masque of Clavicus");
    /* Prices by enchantment alone (worked from enchant_power_costs and enchant_fixed_costs): a life-stealing 1000, leaping 900, magicka 1500. */
    check_expect(magic_def_value(18) == 1000 && magic_def_value(20) == 900 && magic_def_value(22) == 1500 && magic_def_value(0) == 0, "magicitem: an entry's enchantments are not priced by the original's costs");
    /* An item of an entry: the entry's name, its price, kept as which entry it is. */
    container = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
    id = magic_item_create(container, 8);
    def = magic_item_def(id);
    check_expect(id && def >= 0 && !magic_def_artifact(def) && object_at(id)->value == magic_def_value(def) && !strcmp(object_at(id)->name, magic_def_name(def)), "magicitem: an item is not of its entry");
    item_name(object_at(id), name, sizeof(name));
    check_expect(!strstr(name, "%it"), "magicitem: the macro %it was left in an item's name");
    /* Never an artifact in a shop: many items, none of them one. */
    magic = 0;
    for (i = 0; i < 40; i++) {
        id = magic_item_create(container, -1);
        def = magic_item_def(id);
        magic += id && def >= 0 && !magic_def_artifact(def);
    }
    check_expect(magic == 40, "magicitem: a shop item was an artifact or none");
    /* A shop's shelf: quality / 2 + 1 magic items, a spellbook, and with soul traps as many of them. */
    container = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
    magic_shop_stock(container, 10, 0);
    check_expect(count_in(container, 27, 0) == 1 && count_in(container, 27, 1) == 0 && count_in(container, -1, 0) == 7, "magicitem: a quality 10 hall did not stock six magic items and a spellbook");
    container = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
    magic_shop_stock(container, 10, 1);
    check_expect(count_in(container, 27, 1) == 6, "magicitem: a rank 4 member was not offered soul traps");
    itemmaker_checks();
}
