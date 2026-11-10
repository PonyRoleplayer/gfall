#include "game/ogfall/play/describe.h"
#include "game/ogfall/play/inventory.h"
#include "game/ogfall/play/spellbook.h"
#include "game/ogfall/play/play.h"
#include "game/ogfall/formats/lore.h"
#include "game/ogfall/tables/itemdefs.h"
#include "game/ogfall/tables/combatdefs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

typedef struct Description {
    const OgObject *item;
    const OgSpellDef *spell;
    int effect;
    char title[65], author[65];
    char painting[4][128], artist[40], names[4][40];
} Description;

int og_item_info_record(df_u32 id)
{
    const OgObject *item;
    item = og_object(id);
    if (!item || item->type != OG_RECORD_ITEM) { return -1; }
    if (item->group == 2) { return 1000; }
    if (item->group == 3) { return item->index == 18 ? 1011 : 1001; }
    if (item->group == 7) { return 1009; }
    if (item->group == 27 && item->index == 1) { return 1004; }
    if (item->group == 27 && item->index == 2) { return 1007; }
    return 1003;
}

/* Longest macro first where names share a prefix. Each result is copied before
 * reading another TEXT.RSC record, so the reader's cache cannot change it. */
static int expand_macro(const char *src, const Description *d, char *out, int cap)
{
    static const char *names[] = {"%mat","%arm","%wep","%mod","%wdm","%qua","%wth","%mit","%it","%kg","%bt","%ba","%hs",
        "%bdr","%adr","%cld","%bch","%ach","%clc","%1bm","%2bm","%1am","%2am","%clm",
        "%sub","%adj","%pp1","%pp2","%an","%fn2","%mn2","%fn","%mn"};
    const OgObject *item;
    int i, n, value, low, high, weight;
    item = d->item;
    for (i = 0; i < (int)(sizeof(names)/sizeof(names[0])); i++) {
        for (n = 0; names[i][n] && names[i][n] == src[n]; n++) { }
        if (!names[i][n]) { break; }
    }
    if (i == (int)(sizeof(names)/sizeof(names[0]))) { return 0; }
    if (i >= 29) { df_copy(out,d->names[i-29],cap); return n; }
    if (i >= 24) { df_copy(out,i == 28 ? d->artist : d->painting[i-24],cap); return n; }
    if (i >= 13) {
        const OgSpellDef *s;
        s = d->spell;
        if (!s) { return 0; }
        value = i < 16 ? s->duration[d->effect][i-13] : i < 19 ? s->chance[d->effect][i-16] : s->magnitude[d->effect][i-19];
        log_format(out,cap,"%d",value);
        return n;
    }
    if (!item) { return 0; }
    switch (i) {
    case 0:
        df_copy(out,item->group == 2 && (item->index >= 7 || item->index == 5) ? "" :
                item->group == 2 && item->armor_type != 2 ? (item->armor_type ? "Chain" : "Leather") :
                og_material_name(item->material),cap);
        break;
    case 1: case 2: case 7: case 8: df_copy(out,item->name,cap); break;
    case 3:
        value = item->index >= 7 ? item->index-6 :
                (item->armor_type == 2 ? 45+og_material_hit(item->material) : og_armor_base(item->armor_type))/10;
        log_format(out,cap,"%s%d",value >= 0 ? "+" : "",value);
        break;
    case 4:
        value = og_material_hit(item->material)/5;
        low = og_weapon_damage(item->index,0)+value; high = og_weapon_damage(item->index,1)+value;
        log_format(out,cap,"%d - %d",low < 0 ? 0 : low,high < 0 ? 0 : high);
        break;
    case 5: df_copy(out,og_condition_name(item->condition,item->max_condition),cap); break;
    case 6: log_format(out,cap,"%d",item->value); break;
    case 9:
        weight = (item->group == 28 ? item->value/100 : item->group == 23 ? 0 : item->weight*item->count) + og_inventory_weight(item->id);
        og_weight_text(weight,out,cap);
        break;
    case 10: df_copy(out,d->title,cap); break;
    case 11: df_copy(out,d->author,cap); break;
    case 12:
        df_copy(out,"Nothing",cap);
        for (i = 0; i < OG_OBJECTS; i++) {
            const OgObject *soul;
            const OgFoeDef *kind;
            soul = &og_state()->object[i];
            if (!soul->id || soul->type != OG_RECORD_SOUL || soul->parent != item->id) { continue; }
            kind = og_foe_def(soul->index); if (kind) { df_copy(out,kind->name,cap); }
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

void og_item_info(df_u32 id, char *out, int capacity)
{
    Description d;
    int record;
    if (capacity < 1) { return; }
    out[0] = 0;
    record = og_item_info_record(id);
    if (record < 0) { return; }
    mem_zero(&d,sizeof(d)); d.item = og_object(id);
    if (d.item->group == 7) { og_book_header(d.item->message,d.title,sizeof(d.title),d.author,sizeof(d.author)); }
    expand(og_words(record),&d,out,capacity);
    if (!out[0]) { og_item_name(d.item,out,capacity); }
}

void og_spell_help_def(const OgSpellDef *def, int effect, char *out, int capacity)
{
    Description d;
    int record;
    if (capacity < 1) { return; }
    out[0] = 0;
    if (!def || effect < 0 || effect >= 3) { return; }
    mem_zero(&d,sizeof(d)); d.spell = def; d.effect = effect;
    record = og_effect_text(d.spell->group[effect],d.spell->sub[effect]);
    if (record >= 0) { expand(og_words(record),&d,out,capacity); }
}

void og_spell_help(df_u32 id, int effect, char *out, int capacity)
{
    const OgObject *spell;
    if (capacity < 1) { return; }
    out[0] = 0;
    spell = og_object(id);
    if (!spell || spell->type != OG_RECORD_SPELL) { return; }
    og_spell_help_def(og_spell_def(spell->index),effect,out,capacity);
}

/* item_info_painting and names.c use Watcom's generator. Keep inspection's
 * seed local: repeated Info gives the same picture/title without reseeding
 * the world simulation or retaining the original shared scratch globals. */
static unsigned lore_random(df_u32 *seed)
{
    *seed = *seed*1103515245u+12345u;
    return (*seed >> 16) & 32767;
}
static void append(char *out, int cap, const char *text)
{
    int length;
    for (length = 0; out[length]; length++) { }
    df_copy(out+length,text,cap-length);
}
static void name_part(OgNameParts *parts, int set, df_u32 *seed, char *out, int cap)
{
    char piece[11];
    if (og_name_piece(parts,set,lore_random(seed),piece)) { append(out,cap,piece); }
}
static void person_name(df_u32 *seed, int bank, int female, char out[40])
{
    OgNameParts parts;
    char surname[30];
    int first;
    out[0] = surname[0] = 0;
    if (!og_name_open(&parts,bank)) { return; }
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
    og_name_close(&parts);
}
static void artist_name(df_u32 *seed, char out[40])
{
    int female, bank;
    /* macro_an evaluates gender before bank in the original instructions. */
    female = lore_random(seed) & 1; bank = lore_random(seed) & 7;
    person_name(seed,bank,female,out);
}
int og_painting_info(df_u32 id, char *out, int capacity)
{
    Description d;
    df_u8 choices[40];
    df_u32 seed, name_seed, named;
    char raw[128];
    static const int offsets[4] = {123,9543,0,3457};
    int picture, i, count, pick, variant;
    if (capacity < 1) { return 0; }
    out[0] = 0;
    mem_zero(&d,sizeof(d)); d.item = og_object(id);
    if (!d.item || d.item->type != OG_RECORD_ITEM || d.item->group != 13) { return 0; }
    seed = (df_u16)d.item->message;
    picture = lore_random(&seed)%180;
    if (!og_painting_read(picture,choices)) { return 0; }
    for (i = 0; i < 4; i++) {
        for (count = 0; count < 10 && choices[i*10+count] != 255; count++) { }
        if (!count) { return 0; }
        pick = count == 1 ? 0 : lore_random(&seed)%count;
        df_copy(d.painting[i],og_words(6100+i*100+choices[i*10+pick]),sizeof(d.painting[i]));
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
    expand(og_words_variant(250,variant),&d,out,capacity);
    return picture+1;
}
