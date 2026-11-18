#include "game/dagger/play/watcom.h"
#include "game/dagger/play/items/describe.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/magic/spellbook.h"
#include "game/dagger/play/play.h"
#include "game/dagger/formats/lore.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/combatdefs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* item_info (inven.c:920..968): the TEXT.RSC records that describe an item. */
#define TEXT_INFO_ARMOR 1000
#define TEXT_INFO_WEAPON 1001
#define TEXT_INFO_OTHER 1003
#define TEXT_INFO_SOUL_TRAP 1004
#define TEXT_INFO_CREDIT 1007
#define TEXT_INFO_BOOK 1009
#define TEXT_INFO_ARROW 1011
/* The description macros' order in expand_macro: the first MACRO_SPELL_FIRST are item numbers, then the spell's duration, chance and magnitude (three each), then the painting words and the artist, then the names. */
#define MACRO_SPELL_FIRST 13
#define MACRO_CHANCE_FIRST 16
#define MACRO_MAGNITUDE_FIRST 19
#define MACRO_PAINTING_FIRST 24
#define MACRO_ARTIST 28
#define MACRO_NAMES_FIRST 29
/* A plate piece's armour is PLATE_ARMOR_BASE plus the material's, shown in tenths; a weapon's material hit counts a fifth. */
#define PLATE_ARMOR_BASE 45
#define ARMOR_SHOWN_DIV 10
#define MATERIAL_DAMAGE_DIV 5

typedef struct Description {
    const ObjectInfo *item;
    const SpellDef *spell;
    int effect;
    char title[65], author[65];
    char painting[4][128], artist[40], names[4][40];
} Description;

int item_info_record(df_u32 id)
{
    const ObjectInfo *item;
    item = object_at(id);
    if (!item || item->type != RECORD_ITEM) { return -1; }
    if (item->group == ITEM_GROUP_ARMOR) { return TEXT_INFO_ARMOR; }
    if (item->group == ITEM_GROUP_WEAPON) { return item->index == WEAPON_ARROW ? TEXT_INFO_ARROW : TEXT_INFO_WEAPON; }
    if (item->group == ITEM_GROUP_BOOK) { return TEXT_INFO_BOOK; }
    if (item->group == ITEM_GROUP_MISC && item->index == MISC_SOUL_TRAP) { return TEXT_INFO_SOUL_TRAP; }
    if (item->group == ITEM_GROUP_MISC && item->index == MISC_LETTER_OF_CREDIT) { return TEXT_INFO_CREDIT; }
    return TEXT_INFO_OTHER;
}

/* Longest macro first where names share a prefix. Each result is copied before
 * reading another TEXT.RSC record, so the reader's cache cannot change it. */
static int expand_macro(const char *src, const Description *d, char *out, int cap)
{
    static const char *names[] = {"%mat","%arm","%wep","%mod","%wdm","%qua","%wth","%mit","%it","%kg","%bt","%ba","%hs",
        "%bdr","%adr","%cld","%bch","%ach","%clc","%1bm","%2bm","%1am","%2am","%clm",
        "%sub","%adj","%pp1","%pp2","%an","%fn2","%mn2","%fn","%mn"};
    const ObjectInfo *item;
    int i, n, value, low, high, weight;
    item = d->item;
    for (i = 0; i < (int)(sizeof(names)/sizeof(names[0])); i++) {
        for (n = 0; names[i][n] && names[i][n] == src[n]; n++) { }
        if (!names[i][n]) { break; }
    }
    if (i == (int)(sizeof(names)/sizeof(names[0]))) { return 0; }
    if (i >= MACRO_NAMES_FIRST) { df_copy(out,d->names[i-MACRO_NAMES_FIRST],cap); return n; }
    if (i >= MACRO_PAINTING_FIRST) { df_copy(out,i == MACRO_ARTIST ? d->artist : d->painting[i-MACRO_PAINTING_FIRST],cap); return n; }
    if (i >= MACRO_SPELL_FIRST) {
        const SpellDef *s;
        s = d->spell;
        if (!s) { return 0; }
        value = i < MACRO_CHANCE_FIRST ? s->duration[d->effect][i-MACRO_SPELL_FIRST] : i < MACRO_MAGNITUDE_FIRST ? s->chance[d->effect][i-MACRO_CHANCE_FIRST] : s->magnitude[d->effect][i-MACRO_MAGNITUDE_FIRST];
        log_format(out,cap,"%d",value);
        return n;
    }
    if (!item) { return 0; }
    switch (i) {
    case 0:
        df_copy(out,item->group == ITEM_GROUP_ARMOR && (item->index >= ARMOR_SHIELD_FIRST || item->index == ARMOR_HELM) ? "" :
                item->group == ITEM_GROUP_ARMOR && item->armor_type != ARMOR_PLATE ? (item->armor_type != ARMOR_LEATHER ? "Chain" : "Leather") :
                material_name(item->material),cap);
        break;
    case 1: case 2: case 7: case 8: df_copy(out,item->name,cap); break;
    case 3:
        value = item->index >= ARMOR_SHIELD_FIRST ? item->index-SHIELD_INDEX_BASE :
                (item->armor_type == ARMOR_PLATE ? PLATE_ARMOR_BASE+material_hit(item->material) : armor_base(item->armor_type))/ARMOR_SHOWN_DIV;
        log_format(out,cap,"%s%d",value >= 0 ? "+" : "",value);
        break;
    case 4:
        value = material_hit(item->material)/MATERIAL_DAMAGE_DIV;
        low = weapon_damage(item->index,0)+value; high = weapon_damage(item->index,1)+value;
        log_format(out,cap,"%d - %d",low < 0 ? 0 : low,high < 0 ? 0 : high);
        break;
    case 5: df_copy(out,condition_name(item->condition,item->max_condition),cap); break;
    case 6: log_format(out,cap,"%d",item->value); break;
    case 9:
        weight = (item->group == ITEM_GROUP_CURRENCY ? item->value/GOLD_PER_WEIGHT : item->group == ITEM_GROUP_TRANSPORT ? 0 : item->weight*item->count) + inventory_weight(item->id);
        weight_text(weight,out,cap);
        break;
    case 10: df_copy(out,d->title,cap); break;
    case 11: df_copy(out,d->author,cap); break;
    case 12:
        df_copy(out,"Nothing",cap);
        for (i = 0; i < OBJECTS; i++) {
            const ObjectInfo *soul;
            const FoeDef *kind;
            soul = &game_state()->object[i];
            if (!soul->id || soul->type != RECORD_SOUL || soul->parent != item->id) { continue; }
            kind = foe_def(soul->index); if (kind) { df_copy(out,kind->name,cap); }
            break;
        }
        break;
    }
    return n;
}

static void expand(const char *source, const Description *d, char *out, int cap)
{
    char value[160];
    int used, n, i;
    if (cap < 1) { return; }
    used = 0;
    while (*source && used < cap-1) {
        n = *source == '%' ? expand_macro(source,d,value,sizeof(value)) : 0;
        if (n) {
            source += n;
            for (i = 0; value[i] && used < cap-1; i++) { out[used++] = value[i]; }
        } else { out[used++] = *source++; }
    }
    out[used] = 0;
}

void item_info(df_u32 id, char *out, int capacity)
{
    Description d;
    int record;
    if (capacity < 1) { return; }
    out[0] = 0;
    record = item_info_record(id);
    if (record < 0) { return; }
    mem_zero(&d,sizeof(d)); d.item = object_at(id);
    if (d.item->group == ITEM_GROUP_BOOK) { book_header(d.item->message,d.title,sizeof(d.title),d.author,sizeof(d.author)); }
    expand(words_state(record),&d,out,capacity);
    if (!out[0]) { item_name(d.item,out,capacity); }
}

void spell_help_def(const SpellDef *def, int effect, char *out, int capacity)
{
    Description d;
    int record;
    if (capacity < 1) { return; }
    out[0] = 0;
    if (!def || effect < 0 || effect >= 3) { return; }
    mem_zero(&d,sizeof(d)); d.spell = def; d.effect = effect;
    record = effect_text(d.spell->group[effect],d.spell->sub[effect]);
    if (record >= 0) { expand(words_state(record),&d,out,capacity); }
}

void spell_help(df_u32 id, int effect, char *out, int capacity)
{
    const ObjectInfo *spell;
    if (capacity < 1) { return; }
    out[0] = 0;
    spell = object_at(id);
    if (!spell || spell->type != RECORD_SPELL) { return; }
    spell_help_def(spell_def(spell->index),effect,out,capacity);
}

/* item_info_painting and names.c use Watcom's generator. Keep inspection's
 * seed local: repeated Info gives the same picture/title without reseeding
 * the world simulation or retaining the original shared scratch globals. */
static unsigned lore_random(df_u32 *seed)
{
    return watcom_next(seed);
}
static void append(char *out, int cap, const char *text)
{
    int length;
    for (length = 0; out[length]; length++) { }
    df_copy(out+length,text,cap-length);
}
static void name_part(NameParts *parts, int set, df_u32 *seed, char *out, int cap)
{
    char piece[11];
    if (name_piece(parts,set,lore_random(seed),piece)) { append(out,cap,piece); }
}
static void person_name(df_u32 *seed, int bank, int female, char out[40])
{
    NameParts parts;
    char surname[30];
    int first;
    out[0] = surname[0] = 0;
    if (!name_open(&parts,bank)) { return; }
    if (bank == 1) {
        name_part(&parts,0,seed,out,30); name_part(&parts,1,seed,out,30); name_part(&parts,2,seed,out,30);
        if (!female && lore_random(seed)%100 < 75) { name_part(&parts,3,seed,out,30); }
        if (female) { name_part(&parts,4,seed,out,30); }
    } else {
        first = female ? 2 : 0;
        name_part(&parts,first,seed,out,30); name_part(&parts,first+1,seed,out,30);
        first = bank == 2 ? 0 : 4;
        name_part(&parts,first,seed,surname,30); name_part(&parts,first+1,seed,surname,30);
        if (bank == 2) { append(surname,30,"sen"); }
        if (*surname) { append(out,40," "); append(out,40,surname); }
    }
    name_close(&parts);
}
static void artist_name(df_u32 *seed, char out[40])
{
    int female, bank;
    /* macro_an evaluates gender before bank in the original instructions. */
    female = lore_random(seed) & 1; bank = lore_random(seed) & 7;
    person_name(seed,bank,female,out);
}
int painting_info(df_u32 id, char *out, int capacity)
{
    Description d;
    df_u8 choices[40];
    df_u32 seed, name_seed, named;
    char raw[128];
    static const int offsets[4] = {123,9543,0,3457};
    int picture, i, count, pick, variant;
    if (capacity < 1) { return 0; }
    out[0] = 0;
    mem_zero(&d,sizeof(d)); d.item = object_at(id);
    if (!d.item || d.item->type != RECORD_ITEM || d.item->group != ITEM_GROUP_PAINTING) { return 0; }
    seed = (df_u16)d.item->message;
    picture = lore_random(&seed)%180;
    if (!painting_read(picture,choices)) { return 0; }
    for (i = 0; i < 4; i++) {
        for (count = 0; count < 10 && choices[i*10+count] != 255; count++) { }
        if (!count) { return 0; }
        pick = count == 1 ? 0 : lore_random(&seed)%count;
        df_copy(d.painting[i],words_state(6100+i*100+choices[i*10+pick]),sizeof(d.painting[i]));
    }
    variant = lore_random(&seed)%9;
    artist_name(&seed,d.artist);
    /* Painting subjects and prefixes contain the normal name macros too.
     * Their separate seeded generators must not alter the world's RNG. */
    name_seed = lore_random(&seed);
    for (i = 0; i < 4; i++) {
        named = name_seed+offsets[i];
        pick = lore_random(&named) & 7;
        person_name(&named,pick,!(i & 1),d.names[i]);
    }
    for (i = 0; i < 4; i++) {
        df_copy(raw,d.painting[i],sizeof(raw));
        expand(raw,&d,d.painting[i],sizeof(d.painting[i]));
    }
    expand(words_variant(250,variant),&d,out,capacity);
    return picture+1;
}
