#include "game/dagger/play/state.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/travel/maps.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/play/ui/movie.h"
#include "game/dagger/play/ui/msg.h"
#include "../seam.h"
#include "../screen/screen.h"
#include "../sound.h"

#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/ui/pic.h"
#include "engine/scene/interact.h"
#include "engine/scene/weather.h"

#include "game/dagger/places/town.h"
#include "game/dagger/tables/holidaydefs.h"
#include "game/dagger/play/people/bank.h"
#include "game/dagger/play/people/law.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/magic/potion.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/play/magic/spellshop.h"
#include "game/dagger/play/character/ailments.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/places/world.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/play/travel/travelmap.h"

#include "game/dagger/tables/tables.h"

#include "people.h"
#include "callbacks.h"
#include "interaction.h"
#include "../buildings/buildings.h"
#include "../quests/hooks.h"
static df_u32 g_counter;

/* The shop this building is, for the prices (what a shelf's too). */
static int shop_here(ShopState *shop)
{
    int type, faction, quality, region;
    unsigned place;

    if (!travel_building(&type, &faction, &quality, &place)) {
        return 0;
    }
    region = travel_region();
    shop->region_adjust = region_price(region);
    shop->quality = quality;
    shop->type = type;
    shop->holiday = holiday_today(game_state()->minutes, region);
    shop->half = 0;
    shop->scale = 256;
    shop->place = place;
    return 1;
}

/* guild_find_membership_by_bits for the standing rules: the faction of the temple or order the player belongs to, 0 for none. */
int gfall_standing_membership(int bits)
{
    Membership *m;

    m = guild_find_bits(bits);
    return m ? m->faction : 0;
}

/* What each button of a panel does (repair_menu_talk, repair_menu_sell, shop_open_repair, bank_open, service_menu_sell). */
static void hall_act(int what);
static void inn_act(int what);
static void coven_act(int what);

void gfall_service_act(int what)
{
    ShopState shop;

    if (possessions_state()->service == SERVICE_TAVERN) {
        inn_act(what);
        return;
    }

    if (possessions_state()->service == SERVICE_GUILD) {
        hall_act(what);
        return;
    }
    if (possessions_state()->service == SERVICE_COVEN) {
        coven_act(what);
        return;
    }

    if (what == SERVICE_TALK) {
        gfall_start_talk();
        return;
    }
    if (what == SERVICE_DO && possessions_state()->service == SERVICE_BANK) {
        possessions_bank(travel_region());
        return;
    }
    if (!shop_here(&shop)) {
        return;
    }
    if (!object_at(g_counter)) {
        g_counter = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
    }
    if (!g_counter) {
        interact_say("There is no room for another counter.");
        return;
    }
    possessions_trade(what == SERVICE_REPAIR_ITEM ? MARKET_REPAIR : MARKET_SELL, &shop, g_counter);
}

/*
 * Guild halls and temples (people and law, slice 9): guild_service_dispatch (src/hand/run_0006DC35.c 191..949). The hall's person offers the
 * menu (GILD0xI0.IMG: Join, Talk, their service, Exit); the service by the guild, the person and the player's rank is guild.c's gating. The
 * services built are a donation (a temple), the Fighters' repairs and the Thieves' fence; the rest answer that they are not built yet.
 * A promotion's message is shown in place of the menu (the original shows it, then the menu: one page at a time here).
 */
static struct { int kind, faction, person; } g_hall;

static void hall_text(int text)
{
    static char line[600];

    gfall_person_say_text(text, line, (int)sizeof(line));
    possessions_message(line, 0);
}

/* The yes of the offer to join: the membership, and the first rank's words. */
static void hall_join_answer(int yes)
{
    GuildStep step;

    if (yes && guild_join(g_hall.kind, g_hall.faction, game_state()->minutes, &step)) {
        hall_text(step.text);
    }
}

/* guild_donate's number: money spent, a chance of the hall's reputation going up by one (amount * 2 against the standing). */
static void hall_donate_answer(int amount)
{
    int rep;

    if (amount < 1) {
        return;
    }
    if (!gold_can_afford(amount)) {
        hall_text(702);
        return;
    }
    gold_spend(amount);
    rep = faction_rep(g_hall.faction);
    if (random_range(1, 100) <= amount * 2 / ((rep < 0 ? -rep : rep) + 1)) {
        faction_add_reputation(g_hall.faction, 1);
    }
    hall_text(703);
}

/*
 * A tavern's innkeeper (tavern_open, src/lifted/tavern.c; TVRN00I0.IMG: Room, Talk, Food and drinks, Goodbye). A room asks for the days (record 5102, or 5100 with
 * the hours left when one is held), refuses over 350 days kept (16), is free to a local knight and on Heart's Day, else is priced at 7 gold a day through the buyer's
 * skills and offered; a yes rents it, or adds the days. Food is a list of eleven, refused within 240 minutes of the last meal. The bed marker's number the room
 * would send the sleeper to is not read (room 0).
 */
/*
 * The beds of a tavern's inside: the editor markers of record 4 ("kind 2", `(image & 31) - 2`) in file order. A rented room is the number of one of them, drawn at random when the
 * room is rented (tavern_rent); sleeping there puts the player on that bed (tavern_go_to_room). A knight's hall uses the building's room number, 0 when none was ever rented.
 */
static int bed_count(void)
{
    float x, y, z;
    int n;

    for (n = 0; world_marker_nth(travel_whole_world(), 4, n, &x, &y, &z); n++) {
    }
    return n;
}

void gfall_go_to_bed(df_u32 building, int tavern)
{
    float x, y, z;
    int room, k;

    room = 0;
    if (tavern) {
        for (k = 0; k < ROOMS_MOST; k++) {
            if (lodging_state()->room[k].building == building) { room = lodging_state()->room[k].room; }
        }
    }
    if (world_marker_nth(travel_whole_world(), 4, room, &x, &y, &z)) {
        move_place(x, y + 64.0f, z, game_state()->yaw);
    }
}

static unsigned g_inn_building;
static RoomOffer g_inn_offer;

static void inn_room_paid(int yes)
{
    int text, n;

    if (!yes) { return; }
    /* tavern_rent: a bed drawn at random from the building's (rand() % how many). */
    n = bed_count();
    if (!room_take(g_inn_building, &g_inn_offer, n > 0 ? random_range(0, 32767) % n : 0, game_state()->minutes, &text)) {
        if (text) { hall_text(text); }
    }
}

static void inn_days(int days)
{
    ShopState shop;
    int r, knight;
    static char line[400];

    if (!shop_here(&shop)) { return; }
    knight = guild_is_local_knight(travel_region());
    r = room_offer(g_inn_building, days, &shop, character_state()->skill[14], character_state()->attribute[5], knight, game_state()->minutes, &g_inn_offer);
    if (r == ROOM_NOTHING) { return; }
    if (r == ROOM_REFUSED) { hall_text(g_inn_offer.text); return; }
    if (r == ROOM_FREE) {
        int text;

        possessions_message(g_inn_offer.line, 0);
        room_take(g_inn_building, &g_inn_offer, bed_count() > 0 ? random_range(0, 32767) % bed_count() : 0, game_state()->minutes, &text);
        return;
    }
    gfall_offer_say(trade_offer_text(1, days * ROOM_DAY_PRICE, g_inn_offer.price), g_inn_offer.price, line, (int)sizeof(line));
    possessions_ask(line, inn_room_paid);
}

static void inn_food_picked(int row)
{
    const char *line;
    int text;

    if (row < 0) { return; }
    if (!meal_at(row, holiday_today(game_state()->minutes, travel_region()), game_state()->minutes, &line, &text)) {
        if (line) { possessions_message(line, 0); }
        else if (text) { hall_text(text); }
    }
}

static void inn_act(int what)
{
    static char words[300];
    static char names[LIST_MOST][LIST_NAME];
    const char *line;
    int text, i, hours;
    int type, faction, quality;
    unsigned place;

    if (what == SERVICE_TALK) { gfall_start_talk(); return; }
    if (!travel_building(&type, &faction, &quality, &place)) { return; }
    g_inn_building = place;
    if (what == SERVICE_ROOM) {
        gfall_person_say_text(room_rented(place, game_state()->minutes) ? 5100 : 5102, words, (int)sizeof(words));
        /* %dwr: the hours left (macro_dwr_room_hours_left). */
        for (i = 0; words[i]; i++) {
            if (words[i] == '%' && words[i+1] == 'd' && words[i+2] == 'w' && words[i+3] == 'r') {
                char tail[300];
                int k;

                hours = 0;
                for (k = 0; k < ROOMS_MOST; k++) {
                    if (lodging_state()->room[k].building == place && lodging_state()->room[k].expires > game_state()->minutes) {
                        hours = (int)((lodging_state()->room[k].expires - game_state()->minutes) / 60u);
                    }
                }
                df_copy(tail, words + i + 4, (int)sizeof(tail));
                log_format(words + i, (int)sizeof(words) - i, "%d", hours);
                for (k = 0; words[i + k]; k++) { }
                df_copy(words + i + k, tail, (int)sizeof(words) - i - k);
                break;
            }
        }
        possessions_number(words, inn_days);
    } else if (what == SERVICE_FOOD) {
        if (game_state()->minutes - lodging_state()->last_meal < MEAL_GAP) {
            meal_at(0, 0, game_state()->minutes, &line, &text);
            if (line) { possessions_message(line, 0); }
            return;
        }
        for (i = 0; i < FOODS && i < LIST_MOST; i++) { df_copy(names[i], food_name(i), LIST_NAME); }
        possessions_list((const char (*)[LIST_NAME])names, FOODS, inn_food_picked);
    }
}

/* guild_confirm_price: the price at the hall's shop (base, then the buyer's skills, then the scale), the offer's words, and a yes. */
static int g_confirm_price;
static void (*g_confirm_cb)(int price);

static void hall_confirm_answer(int yes)
{
    void (*cb)(int);
    int price;

    cb = g_confirm_cb; price = g_confirm_price; g_confirm_cb = 0;
    if (yes && cb) { cb(price); }
}

static void hall_confirm(int base, void (*cb)(int price))
{
    ShopState shop;
    Quote q;
    static char line[400];
    int total;

    if (!shop_here(&shop)) { return; }
    total = trade_base_price(&shop, base);
    q.total = total;
    q.price = trade_adjust(&shop, character_state()->skill[14], character_state()->attribute[5], total, 0) * shop.scale / 256;
    g_confirm_price = q.price;
    g_confirm_cb = cb;
    gfall_offer_say(trade_offer_text(1, q.total, q.price), q.price, line, (int)sizeof(line));
    if (q.price < 1) { cb(q.price); return; }
    possessions_ask(line, hall_confirm_answer);
}

/* A price paid: 454 when the gold is not there (a message over the world), else spent. */
static int hall_pay(int price)
{
    if (!gold_can_afford(price)) {
        hall_text(454);
        return 0;
    }
    gold_spend(price);
    return 1;
}

/* Training: the lesson's price asked (record 8), then a list of the trainer's skills, then the lesson. */
static struct { int price; const unsigned char *skills; } g_lesson;

static void hall_lesson_picked(int row)
{
    int text, minutes;

    if (row < 0 || row >= LIST_MOST) { return; }
    if (training_learn(g_lesson.skills[row], g_lesson.price, game_state()->minutes, &text, &minutes)) {
        hall_text(text);
        state_pass_minutes((df_u32)minutes);
    } else if (text) {
        hall_text(text);
    }
}

static void hall_lesson_answer(int yes)
{
    static char names[LIST_MOST][LIST_NAME];
    int n;

    if (!yes) { return; }
    if (!gold_can_afford(g_lesson.price)) { hall_text(454); return; }
    for (n = 0; g_lesson.skills[n] != 255 && n < LIST_MOST; n++) {
        df_copy(names[n], skill_name(g_lesson.skills[n]), LIST_NAME);
    }
    possessions_list((const char (*)[LIST_NAME])names, n, hall_lesson_picked);
}

static void hall_training(int person, int member)
{
    static char line[400];
    int text;

    g_lesson.skills = training_skills(person);
    if (!g_lesson.skills) { gfall_start_talk(); return; }
    if (!training_offer(game_state()->minutes, character_state()->level, member, &g_lesson.price, &text)) {
        hall_text(text);
        return;
    }
    gfall_offer_say(8, g_lesson.price, line, (int)sizeof(line));
    possessions_ask(line, hall_lesson_answer);
}

/* guild_cure_diseases: 250 gold a disease (a temple of Akatosh's members pay less by rank), at the hall's prices, and the diseases cured. */
static void hall_cure_paid(int price)
{
    if (hall_pay(price)) {
        ailments_cure(-1, 0);
    }
}

static void hall_cure(int kind, int rank)
{
    int count, price;

    count = ailments_disease_count(-1);
    if (!count) { hall_text(30); return; }
    price = count * 250;
    if (kind == 142 && rank >= 0) { price = price * guild_price_scale(rank) / 256; }
    hall_confirm(price, hall_cure_paid);
}

/* guild_buy_blessing: one at a time, 100 gold at the hall's prices, its words by the temple. */
static int g_blessing_kind, g_blessing_rank;

static void hall_blessing_paid(int price)
{
    int text;

    if (!hall_pay(price)) { return; }
    if (blessing_buy(g_blessing_kind, g_blessing_rank, travel_region(), game_state()->minutes, &text) && text) {
        hall_text(text);
    }
}

static void hall_blessing(int kind, int rank)
{
    if (kind == 142) { return; }
    if (guilds_state()->blessing.active) { hall_text(454); return; }
    g_blessing_kind = kind;
    g_blessing_rank = rank; /* -1: a stranger, who is given the lesser blessing */
    hall_confirm(100, hall_blessing_paid);
}

/* guild_heal: health to the most; if attributes are down, an offer (403) to put them back, the conditions and the ailments gone. */
/* spellshop_buy: four times the cost (half on the holiday 43) through the hall's chain, 454 if short, then the spell is in the book. */
static int g_buying_spell;

static void hall_spell_paid(int price)
{
    if (hall_pay(price)) {
        spellshop_add(g_buying_spell);
    }
}

static void hall_spell_buy(int id)
{
    const SpellDef *spell;

    spell = spell_def(id);
    if (!spell) { return; }
    g_buying_spell = id;
    hall_confirm(spellshop_price(spell, holiday_today(game_state()->minutes, travel_region())), hall_spell_paid);
}

/* daedra_summon: the day's prince, the price asked (481), the gold spent and the roll; a first success starts the prince's quest, a later one is his angry servant, hostile, put
 * beside the player by the frame (daedra_servant_take). The weather byte carries the storm mark: a thunderstorm is a rain marked (4 + WEATHER_STORM_MARK). */
static int g_daedra_prince, g_daedra_price, g_daedra_letter;

static void hall_daedra_answer(int yes)
{
    int kind, result, weather;
    char quest[9];
    static char line[600];
    Weather w;

    if (!yes) { return; }
    if (!gold_can_afford(g_daedra_price)) { hall_text(454); return; }
    gold_spend(g_daedra_price);
    weather_state(&w);
    weather = w.kind == WEATHER_THUNDER ? WEATHER_RAIN | WEATHER_STORM_MARK : w.kind;
    result = daedra_summon(g_daedra_prince, g_daedra_letter, weather, &kind, quest);
    gfall_daedra_say(result == DAEDRA_FAILED ? TEXT_DAEDRA_FAILED : TEXT_DAEDRA_TWICE, 0, g_daedra_prince, line, (int)sizeof(line));
    if (result == DAEDRA_FAILED) { possessions_message(line, 0); }
    else if (result == DAEDRA_QUEST) { quest_begin(quest, g_daedra_prince); }
    /* A second success: the prince's animation says it (text 482) and his servant is put beside the player when it ends. */
    else if (!movie_scene(prince_flc(quest[0]), line, 0, 0)) { possessions_message(line, 0); }
}

static void hall_daedra(void)
{
    DaedraStep step;
    static char line[400];

    if (!daedra_offer(g_hall.faction, faction_rep(g_hall.faction), game_state()->minutes, &step)) {
        if (step.text) { hall_text(step.text); } else if (step.line) { possessions_message(step.line, 0); }
        return;
    }
    g_daedra_prince = step.prince;
    g_daedra_price = step.price;
    g_daedra_letter = step.letter;
    gfall_daedra_say(step.text, step.price, step.prince, line, (int)sizeof(line));
    possessions_ask(line, hall_daedra_answer);
}

/* guild identify uses the classic per-item gold price, waived on its holiday. */
static df_u32 g_identify_items[LIST_MOST], g_identify_item;

static int hall_identify_price(df_u32 id)
{
    if (holiday_today(game_state()->minutes, travel_region()) == 43) { return 0; }
    return magic_item_identify_price(id);
}

static void hall_identify_paid(int yes)
{
    if (!yes || !magic_item_unknown(g_identify_item) ||
        object_at(g_identify_item)->parent != game_state()->inventory_id) { return; }
    if (hall_pay(hall_identify_price(g_identify_item))) {
        char name[64];

        magic_item_identify(g_identify_item);
        item_name(object_at(g_identify_item),name,sizeof(name));
        possessions_message(name,0);
    }
}

static void hall_identify_pick(int row)
{
    char line[80];
    int price;

    if (row < 0 || row >= LIST_MOST) { return; }
    g_identify_item = g_identify_items[row];
    price = hall_identify_price(g_identify_item);
    if (!price) { hall_identify_paid(1); return; }
    log_format(line,sizeof(line),"Identify this item for %d gold?",price);
    possessions_ask(line,hall_identify_paid);
}

static void hall_identify(void)
{
    static char names[LIST_MOST][LIST_NAME];
    const ObjectInfo *item;
    int i, n;

    for (i = n = 0; i < OBJECTS && n < LIST_MOST; i++) {
        item = &game_state()->object[i];
        if (!item->id || item->parent != game_state()->inventory_id || !magic_item_unknown(item->id)) { continue; }
        g_identify_items[n] = item->id;
        item_name(item,names[n++],LIST_NAME);
    }
    if (n) { possessions_list((const char (*)[LIST_NAME])names,n,hall_identify_pick); }
    else { possessions_message("You have no unidentified magical items.",0); }
}

/* The Identify spell (trade_identify_button, inven.c 1494..1527; outside a Mages Guild; docs items-identify-artifacts.md 1.4a). Casting costs nothing net (spfx_identify gives the cast's cost back), so opening the
 * list and closing it is free. Each press of IDENTIFY is a list: its fee of one cast cost up front, then one per item, in magicka alone, whether or not the item is identified (the holiday does not touch it; a
 * guild hall's gold price is the Mages' own service). FALL.EXE has the player drag items into a list; ours takes them one at a time, so each pick is a list of one: the fee, the item's price, one roll of 1..100
 * under the spell's chance, and the original's single box "%d out of %d identified." ("1 out of 1" or "0 out of 1"). Short of one cast cost at the press: "You do not have enough spell points left.", nothing
 * charged. Short for the item after the fee: the item is counted, neither charged nor rolled. Only an item with powers not yet known is offered (FALL.EXE offers any item, its gate has no caller: a fix). */
static int g_spell_identify_chance, g_spell_identify_cost;

static void spell_identify_pick(int row)
{
    CharacterState *me = character_state();
    char line[LIST_NAME];
    int identified;

    if (row < 0 || row >= LIST_MOST || !magic_item_unknown(g_identify_items[row])) { return; }
    if (g_spell_identify_cost > me->magicka) { msg_set(TEXT_IDENTIFY_SHORT); return; }
    me->magicka -= g_spell_identify_cost;
    identified = 0;
    if (g_spell_identify_cost <= me->magicka) {
        me->magicka -= g_spell_identify_cost;
        if (g_spell_identify_chance >= random_range(1, 100)) { magic_item_identify(g_identify_items[row]); identified = 1; }
    }
    log_format(line, sizeof(line), TEXT_IDENTIFY_COUNT, identified, 1);
    possessions_message(line, 0);
}

void gfall_spell_identify(int chance)
{
    static char names[LIST_MOST][LIST_NAME];
    const ObjectInfo *item;
    int i, n;

    g_spell_identify_chance = chance; g_spell_identify_cost = magic_identify_cost();
    for (i = n = 0; i < OBJECTS && n < LIST_MOST; i++) {
        item = &game_state()->object[i];
        if (!item->id || item->parent != game_state()->inventory_id || !magic_item_unknown(item->id)) { continue; }
        g_identify_items[n] = item->id;
        item_name(item,names[n++],LIST_NAME);
    }
    if (!n) { possessions_message("You have no unidentified magical items.",0); return; }
    possessions_list((const char (*)[LIST_NAME])names,n,spell_identify_pick);
}

static void hall_run(void);

static void hall_heal_answer(int yes)
{
    int i;

    if (yes) {
        for (i = 0; i < 8; i++) { character_state()->attribute[i] = game_state()->base_attribute[i]; }
        character_state()->conditions = 0;
        ailments_cure(-1, 0);
        ailments_cure(-1, 1);
    }
    /* The service the heal came with goes on once the question is answered. */
    hall_run();
}

/* 1 when a question is up (the service waits for its answer). */
static int hall_heal(void)
{
    static char line[400];
    int i, lowered;

    character_state()->health = character_state()->max_health;
    lowered = 0;
    for (i = 0; i < 8; i++) {
        if (character_state()->attribute[i] < game_state()->base_attribute[i]) { lowered++; }
    }
    if (!lowered) { hall_text(350); return 0; }
    gfall_person_say_text(403, line, (int)sizeof(line));
    return possessions_ask(line, hall_heal_answer);
}

/* guild_service_dispatch's Get quest (run_0006DC35.c): the quest letter of the NPC's guild, 'B' for a member and 'C' for anyone else, and the rank (the guild's for the Brotherhood, the
 * Fighters and the Thieves, the player's level for the Mages and the orders; a non-member has none: 7.3 fixed). A quest picked is started and offered; none to offer leaves the NPC talking. */
static void hall_quest(const Membership *m)
{
    char name[9], letter;
    int rank;

    letter = 0; rank = m ? m->rank : 0;
    if (g_hall.kind == 0) { letter = 'L'; }
    else if (g_hall.kind == 1) { letter = 'N'; rank = character_state()->level; }
    else if (g_hall.kind == 2) { letter = 'M'; }
    else if (g_hall.kind == 3) { letter = 'O'; }
    else if (g_hall.kind >= KIND_KNIGHT + 4 && g_hall.kind <= KIND_KNIGHT + 13) { letter = 'B'; rank = character_state()->level; }
    else if (g_hall.kind >= KIND_TEMPLE + 14 && g_hall.kind <= KIND_TEMPLE + 21) {
        /* guild_temple_quest: the temple's letter (the temples by name: Akatosh D, Arkay E, Dibella F, Julianos 0, Kynareth G, Mara H, Stendarr I, Zen J) or any C quest, a member's by rank. */
        letter = "DEF0GHIJ"[g_hall.kind - (KIND_TEMPLE + 14)];
        if (!m) { rank = character_state()->level; }
        if (quest_pick(letter, 'C', m ? 'B' : 'C', rank, name) && !quest_person_busy((int)gfall_people_id())) {
            quest_set_questor((int)gfall_people_id(), g_hall.person, gfall_people_name(), town_name());
            if (quest_begin(name, g_hall.person) >= 0) { return; }
        }
        gfall_start_talk();
        return;
    }
    /* A person already giving a quest is not asked for another: they talk (their quest's events run there). */
    if (letter && !quest_person_busy(gfall_people_id())) {
        quest_set_questor((int)gfall_people_id(), g_hall.person, gfall_people_name(), town_name());
        if (quest_pick(letter, 0, m ? 'B' : 'C', rank, name) && quest_begin(name, g_hall.person) >= 0) { return; }
    }
    gfall_start_talk();
}

static void hall_run(void)
{
    Membership *m;
    ShopState shop;
    int service, heal, rank;

    m = guild_find_faction(g_hall.faction);
    rank = m ? m->rank : -1;
    service = guild_service(g_hall.kind, g_hall.person, rank, &heal);
    switch (service) {
    case SVC_TALK:
        gfall_start_talk();
        break;
    case SVC_MEMBERS_ONLY:
        possessions_message("My services are reserved for members only.", 0);
        break;
    case SVC_RANK_REFUSED:
        hall_text(TEXT_RANK_REFUSED);
        break;
    case SVC_TRAINING:
        hall_training(g_hall.person, m != 0);
        break;
    case SVC_MAGIC_ITEMS:
    case SVC_POTIONS:
    case SVC_SOULGEMS:
        /* guild_buy_potions: the container emptied, stocked with the hall's quality + 1 bottles, and opened for buying. */
        {
            int type, faction, quality, i;
            unsigned place;

            if (!shop_here(&shop) || !travel_building(&type, &faction, &quality, &place)) { break; }
            if (!object_at(g_counter)) {
                g_counter = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
            }
            if (!g_counter) { break; }
            for (i = 0; i < OBJECTS; i++) {
                const ObjectInfo *o;

                o = &game_state()->object[i];
                if (o->id && o->type == RECORD_ITEM && o->parent == g_counter) { object_delete(o->id); }
            }
            /* The stock is the building's and the epoch's (docs/gfall-place-state.md, 4): the same on every visit and every reload, new when the epoch turns over; it was drawn from the live stream (fishable), the magic items' from the building's number alone. */
            seed_stream_begin(thing_seed(place, (df_u32)service));
            if (service == SVC_SOULGEMS) { soul_trap_stock(g_counter, quality); }
            else if (service == SVC_MAGIC_ITEMS) {
                /* guild_buy_magic_items: members of rank 4 and over also find soul traps; the festival 38 halves the prices. */
                magic_shop_stock(g_counter, quality, m && m->rank >= 4);
                if (holiday_today(game_state()->minutes, travel_region()) == 38) { shop.scale = 128; }
            }
            else { potion_stock(g_counter, quality); }
            seed_stream_end();
            possessions_trade(MARKET_BUY, &shop, g_counter);
        }
        break;
    case SVC_POTION_MAKER:
        possessions_potion();
        break;
    case SVC_SPELL_MAKER:
        possessions_spellmaker();
        break;
    case SVC_ITEM_MAKER:
        possessions_itemmaker();
        break;
    case SVC_IDENTIFY:
        hall_identify();
        break;
    case SVC_SPELLS:
        {
            int ids[SPELLSHOP_MOST], count, type, faction, quality;
            unsigned place;

            if (!travel_building(&type, &faction, &quality, &place)) { break; }
            count = spellshop_list(gfall_people_id(), quality, ids, SPELLSHOP_MOST);
            if (count) { possessions_spellshop(ids, count, hall_spell_buy); }
        }
        break;
    case SVC_DAEDRA:
        hall_daedra();
        break;
    case SVC_TELEPORT:
        /* guild_teleport: the world map, from wherever the player stands. */
        maps_open(100);
        break;
    case SVC_ARMOR:
        {
            int text;

            knight_armor(m, &text);
            if (text) { hall_text(text); }
        }
        break;
    case SVC_HOUSE:
        {
            int text;

            if (knight_house_allowed(m, bank_state()->house != 0, &text)) {
                if (bank_gift_house()) { hall_text(TEXT_HOUSE_GIVEN); } else { possessions_message("There is no house for sale here.", 0); }
            } else if (text) {
                hall_text(text);
            }
        }
        break;
    case SVC_CURE:
        hall_cure(g_hall.kind, rank);
        break;
    case SVC_BLESSING:
        hall_blessing(g_hall.kind, rank);
        break;
    case SVC_QUEST:
        hall_quest(m);
        break;
    case SVC_NONE:
        break;
    case SVC_DONATE:
        {
            /* guild_donate: msgbox_prompt_number(1000, ...): 1000 is the number first typed in; the words are FALL.EXE's own line. */
            possessions_number_from("Donate how much money", 1000, hall_donate_answer);
        }
        break;
    case SVC_REPAIRS:
    case SVC_FENCE:
        if (shop_here(&shop)) {
            /* The Fighters mend at a member's price ((10 - rank) / 10); the Thieves buy at half. */
            shop.scale = service == SVC_REPAIRS ? guild_price_scale(rank) : 128;
            if (!object_at(g_counter)) {
                g_counter = object_new(RECORD_CONTAINER, WORLD_ROOT, 0, 1);
            }
            if (g_counter) {
                possessions_trade(service == SVC_REPAIRS ? MARKET_REPAIR : MARKET_SELL, &shop, g_counter);
            }
        }
        break;
    default:
        possessions_message("That service is not built yet.", 0);
        break;
    }
}

/* guild_service_dispatch's service: a member is healed first (the heal's own question, if any, is answered before the service goes on). */
static void hall_service(void)
{
    Membership *m;
    int service, heal;

    m = guild_find_faction(g_hall.faction);
    service = guild_service(g_hall.kind, g_hall.person, m ? m->rank : -1, &heal);
    (void)service;
    if (heal && g_hall.kind == GUILD_MAGES) {
        /* run_0006DC35.c:306..309: a Mages Guild member whose class cannot regain magicka and is short of it has it filled; the Mages' hall gives no healing of health. */
        if ((game_state()->career.flags & CLASS_NO_MAGICKA_REGEN) && character_state()->magicka < character_state()->max_magicka) {
            character_state()->magicka = character_state()->max_magicka;
            hall_text(TEXT_MAGICKA_FILLED);
        }
    } else if (heal && hall_heal()) {
        return;
    }
    hall_run();
}

static void hall_act(int what)
{
    GuildStep step;
    static char line[600];

    if (what == SERVICE_TALK) {
        gfall_start_talk();
    } else if (what == SERVICE_JOIN) {
        if (guild_offer(g_hall.kind, g_hall.faction, faction_rep(g_hall.faction), &step)) {
            gfall_person_say_text(step.text, line, (int)sizeof(line));
            if (step.ask) {
                possessions_ask(line, hall_join_answer);
            } else {
                possessions_message(line, 0);
            }
        }
    } else if (what == SERVICE_DO) {
        hall_service();
    }
}

/* npc_click_service's call into the hall: promotion first, then the menu if the person has a service, else they talk. */
void gfall_hall_enter(int person_faction, int hall_faction)
{
    GuildStep step;
    Membership *m;
    const char *label;

    g_hall.faction = hall_faction;
    g_hall.kind = guild_kind_of_faction(hall_faction);
    g_hall.person = person_faction;
    if (g_hall.kind < 0) {
        gfall_start_talk();
        return;
    }
    if (guild_promote(g_hall.kind, hall_faction, faction_rep(hall_faction), game_state()->minutes, &step) && step.text) {
        hall_text(step.text);
        return;
    }
    label = guild_service_label(person_faction);
    if (!label) {
        gfall_start_talk();
        return;
    }
    m = guild_find_faction(hall_faction);
    possessions_guild(m != 0, label);
}

/* The coven's menu (coven_menu_open, coven_menu_frame, click.c 796..: DAED00I0.IMG): Talk, Summon (daedra_summon of this coven: the day's prince for every coven, Hircine at Glenmoril) and Quest (coven_menu_quest:
 * quest_pick_file('Q', 'Q', '0', 'C', the player's level), a quest of the coven for a level, whoever the player is). The covens have no ranks and no membership. */
void gfall_coven_enter(int coven_faction)
{
    g_hall.faction = coven_faction;
    g_hall.person = coven_faction;
    g_hall.kind = -1;
    if (!possessions_service(SERVICE_COVEN)) { gfall_start_talk(); }
}

static void coven_act(int what)
{
    char name[9];

    if (what == SERVICE_TALK) { gfall_start_talk(); return; }
    if (what == SERVICE_SUMMON) { hall_daedra(); return; }
    if (what == SERVICE_QUEST && !quest_person_busy((int)gfall_people_id())) {
        quest_set_questor((int)gfall_people_id(), g_hall.person, gfall_people_name(), town_name());
        if (quest_pick(COVEN_QUEST_LETTER, COVEN_QUEST_LETTER, 'C', character_state()->level, name)) { quest_begin(name, g_hall.person); }
    }
}

void seam_hall_enter(int person_faction, int hall_faction)
{
    gfall_hall_enter(person_faction, hall_faction);
}
