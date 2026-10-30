#include "game/dagger/play/commands.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/inventory.h"
#include "game/dagger/tables/items.h"
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
    cmd_var("tgm", get_tgm, set_tgm, 0, 1, "god mode: nothing hurts the player; tgm alone flips it");
    cmd_toggles("tgm");
    cmd_var("tai", foes_on, foes_set_on, 0, 1, "creatures' AI: off, no one wakes, moves or strikes; tai alone flips it");
    cmd_toggles("tai");
}

void commands_player(Player *p)
{
    g_player = p;
}
