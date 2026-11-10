#include "game/ogfall/tables/itemdefs.h"
#include "game/ogfall/tables/itemdata.h"

const OgItemDef *og_item_def(int group, int index)
{
    if (group < 0 || group >= 29 || index < 0 || index >= og_item_kinds(group)) { return 0; }
    return &g_template[g_groups[group][index]];
}

int og_item_rarity(int group, int index)
{
    if (group < 0 || group >= 29 || index < 0 || index >= og_item_kinds(group)) { return 0; }
    return g_rarity[g_groups[group][index]];
}

int og_item_capacity(int group, int index)
{
    if (group < 0 || group >= 29 || index < 0 || index >= og_item_kinds(group)) { return 0; }
    return g_capacity[g_groups[group][index]];
}

int og_item_kinds(int group)
{
    int n;
    if (group < 0 || group >= 29) { return 0; }
    for (n = 0; g_groups[group][n] != 65535; n++) { }
    return n;
}

int og_item_slot(int group, int index)
{
    static const int armor[] = {18,20,23,15,13,12,26,21,21,21,21};
    static const int jewel[] = {0,6,4,2,8,0,2,-1};
    int slot;
    if (!og_item_def(group, index)) { return -1; }
    if (group == 2) { return armor[index]; }
    if (group == 3) { return index < 18 ? 19 : -1; }
    if (group == 14) { return 10; }
    if (group == 25) { return jewel[index]; }
    if (group != 6 && group != 12) { return -1; }
    slot = group == 6 ? g_male_slot[index] : g_female_slot[index];
    if (slot == 13) { return 14; }
    if (slot == 22) { return 24; }
    return slot == 12 || slot == 17 || slot == 26 ? slot : -1;
}

int og_body_shape(int race) { return race >= 0 && race < 8 ? g_shape[race] : 2; }
int og_start_spell(int klass, int index)
{
    return klass >= 0 && klass < 7 && index >= 0 && index < 6 ? g_start_spell[klass * 6 + index] : 255;
}
int og_loot_group(int category) { return category >= 0 && category < 15 ? g_loot_group[category] : -1; }
const df_u16 *og_loot_table(int index) { return index >= 0 && index < 21 ? g_loot[index] : 0; }
int og_material_share(int index) { return index >= 0 && index < 10 ? g_share[index] : 0; }
int og_material_value(int index) { return index >= 0 && index < 10 ? g_value[index] : 1; }
int og_material_weight(int index) { return index >= 0 && index < 10 ? g_weight[index] : 4; }
int og_material_condition(int index) { return index >= 0 && index < 10 ? g_condition[index] : 4; }
const char *og_material_name(int index)
{
    static const char *name[] = {"Iron", "Steel", "Silver", "Elven", "Dwarven", "Mithril", "Adamantium", "Ebony", "Orcish", "Daedric"};
    return index >= 0 && index < 10 ? name[index] : "";
}
int og_book_id(int index) { return index >= 0 && index < og_book_count() ? g_book[index] : -1; }
int og_book_count(void) { return sizeof(g_book) / sizeof(g_book[0]); }
int og_book_rarity(int index) { return index >= 0 && index < og_book_count() ? g_book_rarity[index] : 0; }
int og_group_tab(int group) { return group >= 0 && group < 29 ? g_group_tab[group] : -1; }
int og_weapon_sound(int index) { return index >= 0 && index < 19 ? g_weapon_sound[index] : 0; }
int og_weapon_damage(int index, int maximum)
{
    return index >= 0 && index < 19 ? (maximum ? g_damage_max[index] : g_damage_min[index]) : 0;
}
const char *og_condition_name(int condition, int maximum)
{
    int percent, i;
    percent = maximum ? condition*100/maximum : 0;
    for (i = 0; i < 7 && percent > g_condition_threshold[i]; i++) { }
    return g_condition_name[i];
}
int og_effect_text(int group, int subtype)
{
    if (group < 0 || group >= 51) { return -1; }
    return 1200 + g_effect_text[group] + (group != 29 && subtype >= 0 && subtype < 12 ? subtype : 0);
}
const char *og_effect_subtype(int group, int subtype)
{
    const char *name;
    name = group >= 0 && group < 51 && subtype >= 0 && subtype < 12 ? g_effect_subtype[group][subtype] : 0;
    return name ? name : "";
}

int og_weapon_proficiency(int index) { return index >= 0 && index < 19 ? g_proficiency[index] : 0; }
const char *og_effect_name(int group) { return group >= 0 && group < 51 ? g_effect_name[group] : ""; }

int og_dungeon_loot(int type) { return type >= 0 && type < 19 ? g_dungeon_loot[type]-1 : 0; }

int og_armor_base(int type) { return type >= 0 && type < 3 ? g_armor_base[type] : 0; }
int og_armor_slot(int part) { return part >= 0 && part < 7 ? g_armor_slot[part] : -1; }
int og_material_hit(int material) { return material >= 0 && material < 10 ? g_material_hit[material] : 0; }
int og_shield_covers(int index, int part)
{
    int i;
    if (index < 7 || index > 10 || part < 0 || part >= 7) { return 0; }
    for (i = 0; i < 4; i++) { if (g_shield_part[(index-7)*4+i] == part) { return 1; } }
    return 0;
}

int og_item_ink(int color, int index)
{
    int first;
    if (color > 0 && color < 16) {
        first = g_dye_ranges[color*2];
        if (index >= first && index < first+16) { return g_dye_ranges[color*2+1]+index-first; }
    } else if (color >= 16 && color < 26 && index >= 112 && index < 128) {
        return g_dye_metal[(color-16)*16+index-112];
    }
    return index;
}
int og_clothing_dye(int special, int roll)
{
    return special ? g_sash_dye[roll & 3] : g_pants_dye[roll & 3];
}
int og_clothing_next(int group, int index, int record)
{
    const df_u8 *style;
    int first, kind;
    if (record < 0 || record >= 111) { return record; }
    style = group == 12 ? g_female_style : g_male_style;
    kind = style[record];
    if (!kind) { return record; }
    if (record+1 < 111 && style[record+1] == kind) { return record+1; }
    first = og_clothing_first(group,record);
    if ((group == 6 && (index == 13 || index == 14)) || (group == 12 && (index == 9 || index == 10))) { first++; }
    return first < 111 ? first : record;
}
int og_clothing_first(int group, int record)
{
    const df_u8 *style;
    if (record < 0 || record >= 111) { return record; }
    style = group == 12 ? g_female_style : g_male_style;
    while (record > 0 && style[record-1] == style[record]) { record--; }
    return record;
}
