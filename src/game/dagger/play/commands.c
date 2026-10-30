#include "game/dagger/play/commands.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/places/town.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/formats/dungeon.h"
#include "game/dagger/formats/factionfile.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "engine/ui/cmd.h"

/* See commands.h. */

static Player *g_player;

/* The player, or 0 with a line saying there is none. */
static Player *player(void)
{
    if (!g_player) {
        log_print(LOG_PRINT, "Not in a game.");
    }
    return g_player;
}

/* Every weapon and armour template's name on a line, then every material's. */
static void list_gear(void)
{
    char line[CMD_LINE * 3];
    RecTable *t;
    int at;
    int i;

    at = 0;
    line[0] = 0;
    t = item_table();
    for (i = 0; i < t->count; i++) {
        const ItemTemplate *it;

        it = (const ItemTemplate *)(t->rows + (df_u32)i * t->schema->row_size);
        if (weapon_template(it->id) || armor_template(it->id)) {
            at += log_format(line + at, (int)sizeof(line) - at, "%s%s", at ? ", " : "Items: ", it->name);
        }
    }
    log_text(LOG_PRINT, line);
    at = 0;
    t = material_table();
    for (i = 0; i < t->count; i++) {
        const Material *m;

        m = (const Material *)(t->rows + (df_u32)i * t->schema->row_size);
        at += log_format(line + at, (int)sizeof(line) - at, "%s%s", at ? ", " : "Materials: ", m->name);
    }
    log_text(LOG_PRINT, line);
}

/* argv[from..to) as one name, a space between. */
static void join(char *out, int cap, char **argv, int from, int to)
{
    int at;
    int i;

    at = 0;
    out[0] = 0;
    for (i = from; i < to; i++) {
        at += log_format(out + at, cap - at, "%s%s", i > from ? " " : "", argv[i]);
    }
}

static void c_additem(int argc, char **argv)
{
    char name[CMD_LINE];
    Player *p;
    Item it;
    int gear;
    int metal;
    int i;

    if (argc < 2) {
        list_gear();
        return;
    }
    p = player();
    if (!p) {
        return;
    }
    /* The material is the last word or the first ("longsword steel", "steel longsword"), or iron. */
    metal = argc > 2 ? item_find_material(argv[argc - 1]) : -1;
    if (metal >= 0) {
        join(name, (int)sizeof(name), argv, 1, argc - 1);
    } else {
        metal = argc > 2 ? item_find_material(argv[1]) : -1;
        join(name, (int)sizeof(name), argv, metal >= 0 ? 2 : 1, argc);
        if (metal < 0) {
            metal = MATERIAL_IRON;
        }
    }
    gear = item_find_gear(name);
    if (gear < 0) {
        log_print(LOG_PRINT, "No item called %s. additem alone lists them.", name);
        return;
    }
    if (!item_make(&it, gear, metal)) {
        log_print(LOG_PRINT, "That does not come in that material.");
        return;
    }
    i = inventory_add(&p->pack, &it);
    if (i < 0) {
        log_print(LOG_PRINT, "The pack is full.");
        return;
    }
    item_name(&it, name, (int)sizeof(name));
    log_print(LOG_PRINT, "%d: %s added. equip %d wears it.", i, name, i);
}

static void c_items(int argc, char **argv)
{
    Player *p;
    int i;

    (void)argc;
    (void)argv;
    p = player();
    if (!p) {
        return;
    }
    if (!p->pack.count) {
        log_print(LOG_PRINT, "The pack is empty.");
        return;
    }
    for (i = 0; i < p->pack.count; i++) {
        const Item *it;
        char name[48];

        it = &p->pack.item[i];
        item_name(it, name, (int)sizeof(name));
        log_print(LOG_PRINT, "%c%d: %s, %.2f kg, %u gold, %u/%u", inventory_worn_at(&p->pack, i) >= 0 ? '*' : ' ', i, name,
                  it->weight, (unsigned)it->value, (unsigned)it->condition, (unsigned)it->condition_max);
    }
    log_print(LOG_PRINT, "%u gold; %.2f kg in all.", (unsigned)p->pack.gold, inventory_weight(&p->pack));
}

static void c_equip(int argc, char **argv)
{
    char name[48];
    Player *p;
    const Item *it;
    const char *s;
    int i;

    p = player();
    if (!p) {
        return;
    }
    if (argc < 2) {
        log_print(LOG_PRINT, "equip n, a number from items, or equip none.");
        return;
    }
    i = -1;
    s = argv[1];
    if (s[0] == 'n' && s[1] == 'o' && s[2] == 'n' && s[3] == 'e' && !s[4]) {
        s += 4;
    } else if (*s >= '0' && *s <= '9') {
        i = 0;
        while (*s >= '0' && *s <= '9') {
            i = i * 10 + (*s++ - '0');
        }
    }
    if (!*s && i >= 0 && i < p->pack.count && character_refuses(&p->sheet, &p->pack.item[i])) {
        log_print(LOG_PRINT, "%s", character_refuses(&p->sheet, &p->pack.item[i]));
        return;
    }
    if (*s || (i < 0 ? !inventory_equip(&p->pack, -1) : !inventory_wear(&p->pack, i))) {
        log_print(LOG_PRINT, "%s is nothing to wear in the pack.", argv[1]);
        return;
    }
    player_wear_changed(p);
    if (i < 0) {
        log_print(LOG_PRINT, "Bare hands.");
        return;
    }
    it = &p->pack.item[i];
    item_name(it, name, (int)sizeof(name));
    log_print(LOG_PRINT, "%s worn.", name);
}

static void c_gold(int argc, char **argv)
{
    Player *p;
    const char *s;
    df_u32 coins;

    p = player();
    if (!p) {
        return;
    }
    coins = 0;
    s = argc > 1 ? argv[1] : "";
    while (*s >= '0' && *s <= '9') {
        coins = coins * 10u + (df_u32)(*s++ - '0');
    }
    p->pack.gold += coins;
    log_print(LOG_PRINT, "%u gold.", (unsigned)p->pack.gold);
}

/* The creatures awake: what each is and is doing (LEVELFALL does not link the game). */
static void c_foes(int argc, char **argv)
{
    Player *p;

    (void)argc;
    (void)argv;
    p = player();
    if (p) {
        foes_report(p->body.feet);
    }
}

/* ---- goto and where. ---- */

/* The most places listed for a name that fits several. */
#define GOTO_LISTED 12
/* More than the map's 15251 places. */
#define GOTO_ROWS 16000

/* A letter in lower case. */
static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* 1 when word is in text, any case; *whole 1 when it is all of it. */
static int name_fits(const char *text, const char *word, int *whole)
{
    int at;
    int k;

    for (at = 0; text[at]; at++) {
        for (k = 0; word[k] && lower(text[at + k]) == lower(word[k]); k++) {
        }
        if (!word[k]) {
            *whole = at == 0 && !text[k];
            return 1;
        }
    }
    *whole = 0;
    return 0;
}

/* The province a region is, by its ruling house's name. */
static const char *region_word(int region)
{
    const Faction *f;

    f = factionfile_open() ? faction_province_of(region) : 0;
    return f ? f->name : "?";
}

/* Asks for the place, and says so: the console has to close before anything moves. */
static void go_to(const char *name, int region, int index)
{
    if (!travel_go_to_town(region, index)) {
        log_print(LOG_PRINT, "%s is not on the map.", name);
        return;
    }
    log_print(LOG_PRINT, "To %s in %s, as the console closes.", name, region_word(region));
}

/*
 * The place named, whole if one is, the largest of those of that name; a
 * part of a name with one fit goes there, with more they are listed. The
 * list is read and dropped each time: nothing for a command used rarely.
 */
static void go_by_name(const char *name)
{
    MemHeap *scratch;
    MemHeap *was;
    DunRef *rows;
    int n;
    int i;
    int best;
    int fits;
    int first;

    scratch = mem_heap_new();
    if (!scratch) {
        return;
    }
    was = mem_heap_use(scratch);
    rows = (DunRef *)mem_alloc((df_u32)GOTO_ROWS * (df_u32)sizeof(DunRef));
    n = rows ? exterior_list(rows, GOTO_ROWS) : 0;
    best = -1;
    fits = 0;
    first = -1;
    for (i = 0; i < n; i++) {
        int whole;

        if (!name_fits(rows[i].name, name, &whole)) {
            continue;
        }
        if (whole && (best < 0 || rows[i].nblocks > rows[best].nblocks)) {
            best = i;
        }
        if (first < 0) {
            first = i;
        }
        fits++;
    }
    if (best < 0 && fits == 1) {
        best = first;
    }
    if (best >= 0) {
        go_to(rows[best].name, rows[best].region, rows[best].index);
    } else if (!fits) {
        log_print(LOG_PRINT, "No place is called %s.", name);
    } else {
        int shown;

        shown = 0;
        for (i = first; i < n && shown < GOTO_LISTED; i++) {
            int whole;

            if (name_fits(rows[i].name, name, &whole)) {
                log_print(LOG_PRINT, "goto %d: %s, %s", rows[i].location_id, rows[i].name, region_word(rows[i].region));
                shown++;
            }
        }
        log_print(LOG_PRINT, "%d places fit.", fits);
    }
    mem_heap_use(was);
    mem_heap_delete(scratch);
}

/* The place whose location number is code: the number LEVELFALL's list has, unique over the map. */
static void go_by_code(int code)
{
    MemHeap *scratch;
    MemHeap *was;
    DunRef *rows;
    int n;
    int i;

    scratch = mem_heap_new();
    if (!scratch) {
        return;
    }
    was = mem_heap_use(scratch);
    rows = (DunRef *)mem_alloc((df_u32)GOTO_ROWS * (df_u32)sizeof(DunRef));
    n = rows ? exterior_list(rows, GOTO_ROWS) : 0;
    for (i = 0; i < n && rows[i].location_id != code; i++) {
    }
    if (i < n) {
        go_to(rows[i].name, rows[i].region, rows[i].index);
    } else {
        log_print(LOG_PRINT, "No place has the code %d.", code);
    }
    mem_heap_use(was);
    mem_heap_delete(scratch);
}

/* 1 when s is all digits. */
static int all_digits(const char *s)
{
    if (!*s) {
        return 0;
    }
    while (*s >= '0' && *s <= '9') {
        s++;
    }
    return !*s;
}

static int read_digits(const char *s)
{
    int v;

    v = 0;
    while (*s) {
        v = v * 10 + (*s++ - '0');
    }
    return v;
}

static void c_goto(int argc, char **argv)
{
    char name[CMD_LINE];
    int at;
    int i;

    if (!player()) {
        return;
    }
    if (argc < 2) {
        log_print(LOG_PRINT, "goto name, goto code, or goto region index. where says where you are.");
        return;
    }
    if (argc == 2 && all_digits(argv[1])) {
        go_by_code(read_digits(argv[1]));
        return;
    }
    if (argc == 3 && all_digits(argv[1]) && all_digits(argv[2])) {
        log_format(name, (int)sizeof(name), "place %s of region %s", argv[2], argv[1]);
        go_to(name, read_digits(argv[1]), read_digits(argv[2]));
        return;
    }
    /* A name of several words comes as several arguments. */
    at = 0;
    for (i = 1; i < argc; i++) {
        at += log_format(name + at, (int)sizeof(name) - at, "%s%s", i > 1 ? " " : "", argv[i]);
    }
    go_by_name(name);
}

static void c_where(int argc, char **argv)
{
    Player *p;
    Camera at;
    TravelWhere here;
    int origin[2];
    int region;
    int index;
    int block;
    int record;
    float x;
    float z;

    (void)argc;
    (void)argv;
    p = player();
    if (!p) {
        return;
    }
    mem_zero(&at, sizeof(at));
    at.x = p->body.feet[0];
    at.z = p->body.feet[2];
    travel_here(&here, origin);
    if (travel_town_here(&at, &region, &index, &x, &z, &block, &record)) {
        Dungeon town;
        int count;

        town_buildings(region, index, &count);
        log_print(LOG_PRINT, "%s%s, %s: goto %d, %d buildings", record >= 0 ? "Inside, in " : "", town_name(), region_word(region),
                  exterior_load_at(region, index, &town) ? town.location_id : 0, count);
    } else if (here.kind == TRAVEL_DUNGEON) {
        log_print(LOG_PRINT, "In a dungeon, %s: place %d of region %d.", region_word(here.region), here.index, here.region);
    } else {
        log_print(LOG_PRINT, "In the wilds, last in %s.", region_word(here.region));
    }
}

static int get_tgm(void)
{
    return g_player ? g_player->god : 0;
}

static void set_tgm(int on)
{
    if (player()) {
        g_player->god = on;
    }
}

void commands_setup(void)
{
    static int done;

    if (done) {
        return;
    }
    done = 1;
    cmd_add("additem", c_additem, "[item] [material]", "a new weapon or piece of armour in the pack; alone, what there is");
    cmd_add("items", c_items, "", "what the player carries, what is worn marked *");
    cmd_add("equip", c_equip, "n|none", "item n of items worn, or bare hands");
    cmd_add("gold", c_gold, "n", "n gold more in the pack");
    cmd_add("goto", c_goto, "name|code|region index", "to a town's streets, by its name, its code (LEVELFALL's list) or its numbers; a part of a name lists what fits");
    cmd_add("where", c_where, "", "the town and region you are in, with its code for goto");
    cmd_add("foes", c_foes, "", "the creatures awake: what each is doing and how far off");
    cmd_var("tgm", get_tgm, set_tgm, 0, 1, "god mode: nothing hurts the player; tgm alone flips it");
    cmd_toggles("tgm");
    cmd_var("tai", foes_on, foes_set_on, 0, 1, "creatures' AI: off, no one wakes, moves or strikes; tai alone flips it");
    cmd_toggles("tai");
}

void commands_player(Player *p)
{
    g_player = p;
}
