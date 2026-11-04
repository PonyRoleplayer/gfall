#include "game/dagger/play/game.h"
#include "game/dagger/play/control.h"
#include "game/dagger/play/bag.h"
#include "game/dagger/play/atlas.h"
#include "game/dagger/play/journey.h"
#include "game/dagger/play/ills.h"
#include "game/dagger/play/rest.h"
#include "game/dagger/play/lodging.h"
#include "game/dagger/play/guild.h"
#include "game/dagger/play/words.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/play/travelmap.h"
#include "game/dagger/play/ride.h"
#include "game/dagger/play/wagon.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/play/dunmap.h"
#include "game/dagger/play/bank.h"
#include "game/dagger/play/court.h"
#include "game/dagger/play/service.h"
#include "game/dagger/play/shop.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/commands.h"
#include "game/dagger/play/create.h"
#include "game/dagger/play/foes.h"
#include "game/dagger/play/watch.h"
#include "game/dagger/play/keys.h"
#include "game/dagger/play/loot.h"
#include "game/dagger/play/menu.h"
#include "game/dagger/play/mouse.h"
#include "game/dagger/play/viewopt.h"
#include "game/dagger/play/overlay.h"
#include "game/dagger/play/hudbar.h"
#include "game/dagger/play/pile.h"
#include "game/dagger/play/player.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/save.h"
#include "game/dagger/play/screen.h"
#include "game/dagger/play/screens.h"
#include "game/dagger/play/weapon.h"
#include "game/dagger/tables/items.h"
#include "game/dagger/places/doors.h"
#include "game/dagger/places/people.h"
#include "game/dagger/places/town.h"
#include "game/dagger/formats/faces.h"
#include "game/dagger/formats/worldmap.h"
#include "game/dagger/tables/commerce.h"
#include "game/dagger/places/dunstream.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "engine/core/prof.h"
#include "engine/gfx/gfx.h"
#include "engine/scene/interact.h"
#include "engine/scene/keep.h"
#include "engine/scene/stream.h"
#include "engine/scene/automap.h"
#include "engine/scene/clock.h"
#include "engine/scene/walker.h"
#include "engine/scene/weather.h"
#include "engine/ui/cmd.h"
#include "engine/ui/hud.h"
#include "engine/ui/pic.h"

/* See game.h. */

static const Level *g_level;
static Player g_player;
static int g_playing;
/* The character and kit the next place starts with: made in creation, or the default. */
static Character g_new_sheet;
static Inventory g_new_pack;
/* A save read, for the next place to start from; and 1 while it waits to. */
static SaveState g_loaded;
static int g_load_waiting;
/* The clock's day when this game began: skills' six hours count game minutes from it. */
static int g_day_zero;
/* The last game minute that tired the player. */
static int g_last_minute;
/* 1 from the death screen until a save is loaded or the slots put away. */
static int g_dead;
/* The camera's frame's, for what the use handler asks about where it stands; the run key held at the use. */
static const Camera *g_cam;
static int g_stealing;
/* The HUD cursor is a play preference, distinct from a paused screen's
 * pointer. Screens temporarily own input; closing one restores this choice.
 * It survives place changes, and a new/loaded character starts in Grab.
 * The saved Cursor/View scheme supplies its initial value. */
static int g_hud_cursor;
static int g_control_scheme;
static ControlState g_control;
static int g_mode;
static int g_sheathed;
/* The door whose lock last beat the player, and their skill then. */
static int g_lock_tried = -1;
static int g_lock_skill;
/* The merchant whose panel is open, for its Talk. */
static int g_talk_faction;
static int g_talk_flat;
/* The clock's day last looked at for loans past due. */
static int g_loans_day;
/* The thing number of a shop's keep row: past any block's things (docs/game.md, What the player changed). */
#define SHOP_KEY_THING 0xFE000000u

static void apply_pointer(int ui_up)
{
    /*
     * A menu or a screen shows the pointer. In play, the HUD cursor
     * (the saved Cursor scheme, or Tab) keeps it; View looks.
     * The crosshair is the view's centre, off while that pointer is up.
     */
    input_set_pointer(ui_up || g_hud_cursor);
    overlay_set_crosshair(g_hud_cursor ? 0 : 1);
}

/* A screen over the stilled world, with the pointer. */
static void screen_over_play(void)
{
    ride_quiet();
    control_block(&g_control);
    weapon_cancel();
    gfx_set_pause(screens_is_open());
    apply_pointer(screens_is_open());
}

static float game_minutes(void);
static int guild_panel(int faction, int npc_faction);
static void temple_cure(void);

/* The travel map, outdoors. */
static void travel_map_over_play(void)
{
    int px;
    int py;

    if (!travel_pixel(g_cam, &px, &py)) {
        interact_say("You cannot travel from here.");
        return;
    }
    screens_open_travel(&g_player, px, py, travel_region());
    screen_over_play();
}

/*
 * The automap key (M): in a town's streets its automap (P10); in the wilds,
 * where there is nothing to map, the travel map; in a dungeon or a building,
 * classic's automap, whose render is the engine's to give. The travel map has
 * its own key (V).
 */
static void map_over_play(void)
{
    int region;
    int index;
    int block;
    int record;
    float x;
    float z;
    int px;
    int py;

    if (travel_pixel(g_cam, &px, &py) && travel_town_here(g_cam, &region, &index, &x, &z, &block, &record)) {
        screens_open_townmap(region, index, x, z, g_player.yaw);
        screen_over_play();
        if (screens_is_open()) {
            return;
        }
    }
    if (travel_pixel(g_cam, &px, &py)) {
        travel_map_over_play();
        return;
    }
    /* Indoors: what the player has seen of this place, the engine's to draw (docs/automap.md). */
    screens_open_dunmap(g_player.body.feet, g_player.yaw);
    screen_over_play();
}

/* Under the screens' picture, before it: the indoor automap's seen geometry, in the hole its chrome leaves. */
static void draw_under(void)
{
    if (screens_dunmap_open() && g_level) {
        gfx_automap(g_level, dunmap_view(), dunmap_ground());
    }
}

/*
 * The travel map's journey made: paid for, the days passed, the player rested
 * if it was cautious, the place known, and on the way there. 0 when there was
 * none to make.
 */
static int journey_made(void)
{
    Journey j;
    int region;
    int index;
    int cautious;
    int day;
    float minute;
    int px;
    int py;
    int type;

    travelmap_chosen(&region, &index, &j, &cautious);
    if (region < 0 || !worldmap_pixel_of(region, index, &px, &py)) {
        return 0;
    }
    /* A vampire does not set out by day (VampirismEffect.CheckFastTravel). */
    {
        int hour;
        int min;

        clock_hm(&hour, &min);
        if (ills_travel_refused(hour >= 6 && hour < 18)) {
            interact_say("Travelling by day, the sun would burn you.");
            return 0;
        }
    }
    g_player.pack.gold -= (df_u32)(j.cost > (int)g_player.pack.gold ? (int)g_player.pack.gold : j.cost);
    journey_arrival(clock_day(), clock_minute(), j.minutes, cautious, &day, &minute);
    clock_set_day(day);
    clock_set_minute(minute);
    /* The road's own wear is the journey's choice: the minutes it took are not tired again one by one. */
    g_last_minute = df_ifloor(game_minutes());
    if (cautious) {
        character_rest_full(&g_player.sheet);
    }
    atlas_learn(worldmap_location_id(px, py));
    type = worldmap_location_type(px, py);
    return travel_go_to_place(region, index, type == WORLDMAP_DUNGEON_LABYRINTH || type == WORLDMAP_DUNGEON_KEEP
                                                 || type == WORLDMAP_DUNGEON_RUIN || type == WORLDMAP_GRAVEYARD
                                                 || type == WORLDMAP_COVEN);
}

/* Today's holiday's flags (HOLIDAY_*); 0 on an ordinary day. */
static int today_holiday_flags(void)
{
    const Holiday *h;
    int year;
    int month;
    int day;

    clock_date(&year, &month, &day);
    h = holiday_on(month, day);
    return h ? h->flags : 0;
}

/* The hour, for a building's opening. */
static int hour_now(void)
{
    int hour;
    int minute;

    clock_hm(&hour, &minute);
    return hour;
}

/*
 * Whether a street door opens, by its building's hours (PlayerActivate). Shut, it is
 * locked, its strength half the building's quality; holding the run key is the
 * player picking it (classic's Steal mode, until the mode buttons come).
 */
static int door_lets_in(const Usable *u)
{
    int type;
    int quality;
    int lock;
    int skill;

    /* Shops are locked all Sun's Rest (DFU's BuildingIsUnlocked), whatever the hour. */
    if (!travel_door_building(u, &type, &quality)
        || (building_open_at(type, hour_now()) && !(building_is_shop(type) && (today_holiday_flags() & HOLIDAY_SHOPS_SHUT)))) {
        return 1;
    }
    if (!g_stealing) {
        interact_say("The door is locked.");
        return 0;
    }
    lock = quality / 2;
    skill = g_player.sheet.skill[SKILL_LOCKPICKING];
    /* A lock beaten once stays beaten until the skill has grown. */
    if (u->tag == g_lock_tried && skill <= g_lock_skill) {
        interact_say("The lock is beyond you.");
        return 0;
    }
    character_tally(&g_player.sheet, SKILL_LOCKPICKING, 1);
    if (combat_roll(100) + 1 >= rules_lock_chance(skill, lock)) {
        g_lock_tried = u->tag;
        g_lock_skill = skill;
        interact_say("You fail to pick the lock.");
        return 0;
    }
    interact_say("The lock clicks open.");
    return 1;
}

/* Who is being talked to, from who they are and where the player stands; a face from their flat. */
static void talk_to(int faction, int flat, unsigned seed)
{
    static char region_name[FACTION_NAME];
    TalkWho who;
    const Faction *province;
    int hit;

    /* A beast gets no answer (LycanthropyEffect.GetSuppressTalk). */
    if (ills_beast()) {
        interact_say("You get no response.");
        return;
    }

    mem_zero(&who, sizeof(who));
    who.faction = faction;
    who.seed = seed;
    who.face = flat ? faces_of_flat(flat >> 7, flat & 0x7f) : FACE_NONE;
    hit = travel_town_here(g_cam, &who.region, &who.index, &who.x, &who.z, &who.block, &who.record);
    who.located = hit;
    if (hit) {
        int count;

        town_buildings(who.region, who.index, &count);
        who.city = town_name();
    }
    province = faction_province_of(travel_region());
    df_copy(region_name, province ? province->name : "", (int)sizeof(region_name));
    who.region_name = region_name;
    screens_open_talk(&g_player, &who);
    screen_over_play();
}

/* The shop this building is, for the screens; 0 when it is none. */
static int shop_here(Shop *shop)
{
    RegionStanding *r;
    int faction;
    unsigned place;

    if (!travel_building(&shop->type, &faction, &shop->quality, &place)) {
        return 0;
    }
    r = standing_region(travel_region());
    shop->prices = r ? r->prices : 1000;
    /* A row of the place's own, past any thing in its blocks. */
    shop->key.place = place;
    shop->key.thing = SHOP_KEY_THING;
    shop->holiday = today_holiday_flags();
    shop->repair_rank = 0;
    /* Shut out of its hours, and all day on Sun's Rest. */
    shop->closed = !building_open_at(shop->type, hour_now()) || (shop->holiday & HOLIDAY_SHOPS_SHUT);
    return building_is_shop(shop->type);
}

/* A shelf used: the shop's goods, by its hours; in a house, nothing to buy. */
static void shelf_used(void)
{
    Shop shop;

    if (!shop_here(&shop)) {
        interact_say("There is nothing here you want.");
        return;
    }
    if (ills_beast()) {
        interact_say("You cannot trade while shapechanged.");
        return;
    }
    screens_open_trade(&g_player, &shop, TRADE_BUY, clock_day());
    screen_over_play();
}

/*
 * Someone used: a merchant in a shop or a bank offers their panel, as StaticNPCClick;
 * anyone else, and anyone in the street, talks.
 */
static void person_used(const Usable *u)
{
    const Faction *f;
    int type;
    int faction;
    int quality;
    unsigned place;

    g_talk_faction = u->what;
    g_talk_flat = u->tag;
    if (ills_beast()) {
        interact_say("You get no response.");
        return;
    }
    /* A guild hall's or a temple's people: its panel, with what this one does (DFU's guild service). */
    if (travel_building(&type, &faction, &quality, &place) && (type == BUILDING_GUILD_HALL || type == BUILDING_TEMPLE)
        && guild_panel(faction, u->what)) {
        return;
    }
    /* A temple with no guild in MAIN.DAT still cures. */
    if (type == BUILDING_TEMPLE) {
        screens_open_service(SERVICE_PANEL_CURE);
        screen_over_play();
        return;
    }
    f = faction_find(u->what);
    if (f && f->sgroup == SOCIAL_MERCHANTS && travel_building(&type, &faction, &quality, &place)) {
        if (type == BUILDING_BANK) {
            screens_open_service(SERVICE_PANEL_BANK);
            screen_over_play();
            return;
        }
        /* The tavern keeper: a room, food and drink, talk (DaggerfallTavernWindow). */
        if (type == BUILDING_TAVERN) {
            screens_open_service(SERVICE_PANEL_TAVERN);
            screen_over_play();
            return;
        }
        if (building_is_shop(type)) {
            screens_open_service(type == BUILDING_ARMORER || type == BUILDING_WEAPONS ? SERVICE_PANEL_REPAIR : SERVICE_PANEL_SELL);
            screen_over_play();
            return;
        }
    }
    talk_to(u->what, u->tag, (unsigned)u->tag * 31u + (unsigned)u->what);
}

/* A townsperson in the view, not a usable: the people of the region answer, with that people's flat. */
static int street_person_used(const Camera *cam)
{
    const Faction *people;
    int female;
    unsigned who;

    if (!people_in_view(cam, TALK_REACH, &female, &who)) {
        return 0;
    }
    people = faction_people_of(travel_region());
    if (!people) {
        return 0;
    }
    talk_to(people->id, ((int)people->flat_archive[female] << 7) + people->flat_record[female], who);
    return 1;
}

/* ---- Guilds (guild.h): a hall's people, joining, ranks and training. ---- */

/* What a guild hall's person does, by their own faction (DFU's GuildNpcServices). */
enum {
    GUILD_SERVICE_NONE,
    GUILD_SERVICE_TRAINING,
    GUILD_SERVICE_QUESTS,
    GUILD_SERVICE_REPAIR,
    GUILD_SERVICE_CURE,
    GUILD_SERVICE_OTHER
};
typedef struct GuildNpc {
    short faction;
    short service;
    const char *label;
} GuildNpc;
static const GuildNpc g_guild_npcs[] = {
    { 61, GUILD_SERVICE_TRAINING, "TRAINING" }, { 849, GUILD_SERVICE_TRAINING, "TRAINING" },
    { 241, GUILD_SERVICE_TRAINING, "TRAINING" }, { 243, GUILD_SERVICE_TRAINING, "TRAINING" },
    { 245, GUILD_SERVICE_TRAINING, "TRAINING" }, { 247, GUILD_SERVICE_TRAINING, "TRAINING" },
    { 249, GUILD_SERVICE_TRAINING, "TRAINING" }, { 250, GUILD_SERVICE_TRAINING, "TRAINING" },
    { 252, GUILD_SERVICE_TRAINING, "TRAINING" }, { 254, GUILD_SERVICE_TRAINING, "TRAINING" },
    { 63, GUILD_SERVICE_QUESTS, "GET QUEST" }, { 851, GUILD_SERVICE_QUESTS, "GET QUEST" },
    { 240, GUILD_SERVICE_QUESTS, "GET QUEST" }, { 846, GUILD_SERVICE_QUESTS, "GET QUEST" },
    { 850, GUILD_SERVICE_REPAIR, "REPAIRS" },
    { 813, GUILD_SERVICE_CURE, "CURE DISEASE" },
    { 810, GUILD_SERVICE_OTHER, "MAKE DONATION" }, { 801, GUILD_SERVICE_OTHER, "IDENTIFY" },
    { 60, GUILD_SERVICE_OTHER, "BUY SPELLS" }, { 496, GUILD_SERVICE_OTHER, "BUY SPELLS" },
    { 64, GUILD_SERVICE_OTHER, "MAKE SPELLS" }, { 497, GUILD_SERVICE_OTHER, "MAKE SPELLS" },
    { 62, GUILD_SERVICE_OTHER, "TELEPORTATION" }, { 66, GUILD_SERVICE_OTHER, "DAEDRA SUMMONING" },
    { 65, GUILD_SERVICE_OTHER, "BUY MAGIC ITEMS" }, { 480, GUILD_SERVICE_OTHER, "BUY MAGIC ITEMS" },
    { 802, GUILD_SERVICE_OTHER, "MAKE MAGIC ITEMS" }, { 481, GUILD_SERVICE_OTHER, "MAKE MAGIC ITEMS" },
    { 845, GUILD_SERVICE_OTHER, "RECEIVE ARMOR" }, { 848, GUILD_SERVICE_OTHER, "RECEIVE HOUSE" }
};

/* The guild whose panel is open, the building's faction, and its person's service. */
static const GuildKind *g_guild;
static int g_guild_faction;
static int g_guild_service;
static const char *g_skill_lines[GUILD_SKILLS];
static char g_skill_text[GUILD_SKILLS][24];

/* A TEXT.RSC record, its macros filled, said on the screen. */
static void say_record(int record, const char *fallback)
{
    static Words words;
    char line[256];

    mem_zero(&words, sizeof(words));
    words.pc = &g_player.sheet;
    words.amount = guild_training_price(guild_rank(g_guild) >= 0, g_player.sheet.level);
    words_use(&words);
    if (record <= 0 || words_say(record, line, (int)sizeof(line)) <= 0) {
        df_copy(line, fallback, (int)sizeof(line));
    }
    interact_say(line);
}

/*
 * A guild's panel for a person of npc_faction in a building of faction: its
 * rank looked at, a temple's member healed, then Join Guild (or not, for a
 * member), Talk and this person's service. 0 when the building is no guild's.
 */
static int guild_panel(int faction, int npc_faction)
{
    const GuildNpc *npc;
    int updated;
    int i;

    g_guild = guild_of_faction(faction);
    if (!g_guild) {
        return 0;
    }
    g_guild_faction = faction;
    npc = 0;
    for (i = 0; i < (int)(sizeof(g_guild_npcs) / sizeof(g_guild_npcs[0])); i++) {
        if (g_guild_npcs[i].faction == npc_faction) {
            npc = &g_guild_npcs[i];
        }
    }
    g_guild_service = npc ? npc->service : GUILD_SERVICE_NONE;
    /* The rank, 28 days on (Guild.UpdateRank on the window's push). */
    updated = guild_update(g_guild, &g_player.sheet, standing_rep(g_guild->id), clock_day());
    if (updated == GUILD_EXPELLED) {
        say_record(668, "You have been expelled.");
    } else if (updated >= 0) {
        char line[64];

        log_format(line, (int)sizeof(line), "You are now %s.", guild_title(g_guild, updated));
        interact_say(line);
    }
    /* A temple's member from its healing rank comes away whole (Temple.FreeHealing). */
    if (guild_heals(faction) && g_player.sheet.health < g_player.sheet.health_max) {
        g_player.sheet.health = g_player.sheet.health_max;
        interact_say("You are healed.");
    }
    screens_open_service(guild_rank(g_guild) >= 0 ? SERVICE_PANEL_GUILD_MEMBER : SERVICE_PANEL_GUILD);
    service_set_label(npc ? npc->label : "");
    screen_over_play();
    return 1;
}

/* Join Guild: in at rank 0 if the name and the skills allow it, TEXT.RSC's words either way. */
static void guild_join_asked(void)
{
    int result;

    if (!g_guild) {
        return;
    }
    result = guild_join(g_guild, &g_player.sheet, standing_rep(g_guild->id), clock_day());
    if (result == GUILD_JOINED) {
        say_record(g_guild->text_welcome, "Welcome to the guild.");
    } else if (result == GUILD_BAD_NAME) {
        say_record(g_guild->text_bad_name, "Your reputation does not allow you to join.");
    } else if (result == GUILD_LOW_SKILL) {
        say_record(g_guild->text_low_skill, "You are not skilled enough to join.");
    } else if (result == GUILD_ALREADY) {
        interact_say("You are already a member.");
    } else if (result == GUILD_OTHER_OF_GROUP) {
        interact_say("You already belong to another such order.");
    }
}

/* The person's service: training's list of skills, a smith's repairs, a cure; what is not built says so. */
static void guild_service_asked(void)
{
    const CareerSkill *s;
    Shop shop;
    int i;

    if (!g_guild) {
        return;
    }
    switch (g_guild_service) {
    case GUILD_SERVICE_TRAINING:
        if (!guild_trains(g_guild)) {
            interact_say("Only members are trained here.");
            return;
        }
        for (i = 0; i < GUILD_SKILLS && g_guild->training[i] != GUILD_NO_SKILL; i++) {
            s = career_skill(g_guild->training[i]);
            log_format(g_skill_text[i], (int)sizeof(g_skill_text[i]), "%s %d", s ? s->name : "?",
                       g_guild->training[i] < SKILL_COUNT ? g_player.sheet.skill[g_guild->training[i]] : 0);
            g_skill_lines[i] = g_skill_text[i];
        }
        /* The panel again, its list of skills over it; the price is said first (TEXT.RSC 8's sum). */
        {
            char line[64];

            log_format(line, (int)sizeof(line), "Training costs %d gold. Which skill?",
                       guild_training_price(guild_rank(g_guild) >= 0, g_player.sheet.level));
            interact_say(line);
        }
        screens_open_service(guild_rank(g_guild) >= 0 ? SERVICE_PANEL_GUILD_MEMBER : SERVICE_PANEL_GUILD);
        service_set_label("TRAINING");
        service_list(g_skill_lines, i);
        return;
    case GUILD_SERVICE_REPAIR:
        if (guild_rank(g_guild) < 0) {
            interact_say("Only members have their things repaired here.");
            return;
        }
        /* A hall is no shop (shop_here says 0), but its quality, the region's prices and its key fill in the same. */
        shop_here(&shop);
        shop.type = BUILDING_ARMORER;
        shop.closed = 0;
        shop.repair_rank = guild_rank(g_guild) + 1;
        screens_open_trade(&g_player, &shop, TRADE_REPAIR, clock_day());
        return;
    case GUILD_SERVICE_CURE:
        temple_cure();
        return;
    case GUILD_SERVICE_QUESTS:
        interact_say("There is no work for you yet.");
        return;
    default:
        interact_say("That service is not offered yet.");
        return;
    }
}

/* A skill picked off training's list: paid, three hours pass, and it is used 10 to 20 times over. */
static void guild_training_picked(int line)
{
    const CareerSkill *s;
    char text[80];
    int skill;
    int price;
    int result;

    if (!g_guild || line < 0 || line >= GUILD_SKILLS || g_guild->training[line] == GUILD_NO_SKILL) {
        return;
    }
    skill = g_guild->training[line];
    price = guild_training_price(guild_rank(g_guild) >= 0, g_player.sheet.level);
    result = guild_train(&g_player, g_guild, skill, price, ills_now(clock_day(), clock_minute()));
    if (result == GUILD_TOO_SOON) {
        say_record(4023, "You must wait before training again.");
    } else if (result == GUILD_TOO_SKILLED) {
        say_record(4022, "You are too skilled to be trained here.");
    } else if (result == GUILD_NOT_ENOUGH_GOLD) {
        interact_say("You do not have enough gold.");
    } else if (result == GUILD_TRAINED) {
        float minute;

        minute = clock_minute() + 180.0f;
        if (minute >= CLOCK_DAY) {
            minute -= CLOCK_DAY;
            clock_set_day(clock_day() + 1);
        }
        clock_set_minute(minute);
        g_last_minute = df_ifloor(game_minutes());
        s = career_skill(skill);
        log_format(text, (int)sizeof(text), "You train in %s for three hours, for %d gold.", s ? s->name : "it", price);
        interact_say(text);
    }
    screen_over_play();
}

/*
 * A temple's cure (DaggerfallGuildServiceCureDisease): every disease and an
 * infection, 250 gold each by the temple's quality and the region's prices,
 * free or half on its holidays, then bargained. Classic asks first; this pays
 * at once, saying what it cost.
 */
static void temple_cure(void)
{
    RegionStanding *r;
    char line[64];
    int type;
    int faction;
    int quality;
    unsigned place;
    int count;
    int cost;

    count = ills_curable();
    if (count == 0) {
        interact_say("You are not diseased.");
        return;
    }
    if (!travel_building(&type, &faction, &quality, &place)) {
        quality = 10;
    }
    r = standing_region(travel_region());
    cost = ills_cure_cost(count, quality, r ? r->prices : 1000, today_holiday_flags());
    /* An Arkay temple's member pays (10 - rank) tenths (Temple.ReducedCureCost). */
    cost = guild_cure_price(faction, cost);
    if (cost > 0) {
        cost = rules_trade_price(cost, quality, g_player.sheet.skill[SKILL_MERCANTILE], g_player.sheet.attribute[ATTR_PERSONALITY], 0);
    }
    if ((int)g_player.pack.gold < cost) {
        log_format(line, (int)sizeof(line), "The cure costs %d gold, more than you have.", cost);
        interact_say(line);
        return;
    }
    g_player.pack.gold -= (df_u32)cost;
    ills_cure(&g_player.sheet);
    if (cost == 0) {
        interact_say("On this holy day, the temple cures you freely.");
    } else {
        log_format(line, (int)sizeof(line), "You are cured, for %d gold.", cost);
        interact_say(line);
    }
}

/*
 * A room for days in this tavern (DaggerfallTavernWindow.RentRoom): 7 gold a
 * day, Heart's Day free, bargained by its quality; added to a room held here.
 * Classic asks before paying; this pays at once, saying the price.
 */
static void tavern_room(int days)
{
    char line[80];
    int type;
    int faction;
    int quality;
    unsigned place;
    int region;
    int index;
    int block;
    int record;
    int year;
    int month;
    int day;
    int cost;
    int result;
    float x;
    float z;

    if (!travel_building(&type, &faction, &quality, &place)
        || !travel_town_here(g_cam, &region, &index, &x, &z, &block, &record)) {
        return;
    }
    clock_date(&year, &month, &day);
    cost = lodging_room_cost(days, month * 30 + day + 1);
    /* A knight's room is free: from rank 4 anywhere, else in the order's own region (KnightlyOrder.FreeTavernRooms). */
    if (guild_free_rooms(travel_region())) {
        interact_say("The room is free for a knight such as you.");
        cost = 0;
    } else if (cost > 0) {
        cost = rules_trade_price(cost, quality, g_player.sheet.skill[SKILL_MERCANTILE], g_player.sheet.attribute[ATTR_PERSONALITY], 0);
    }
    result = lodging_rent(&g_player, region, index, block, record, days, cost, ills_now(clock_day(), clock_minute()));
    if (result == LODGING_TOO_MANY_DAYS) {
        interact_say("You cannot rent a room that far ahead.");
    } else if (result == LODGING_NOT_ENOUGH_GOLD) {
        log_format(line, (int)sizeof(line), "The room costs %d gold, more than you have.", cost);
        interact_say(line);
    } else if (result == LODGING_FULL) {
        interact_say("You hold too many rooms already.");
    } else if (cost == 0) {
        interact_say("On Heart's Day your room is free.");
    } else {
        log_format(line, (int)sizeof(line), "A room for %d day%s, for %d gold. You may rest here.", days, days == 1 ? "" : "s", cost);
        interact_say(line);
    }
}

/* Food or drink off the tavern's menu. */
static void tavern_food(int which)
{
    char line[64];
    int result;

    result = lodging_eat(&g_player, which, ills_now(clock_day(), clock_minute()), today_holiday_flags());
    if (result == LODGING_NOT_HUNGRY) {
        interact_say("You are not hungry.");
    } else if (result == LODGING_NOT_ENOUGH_GOLD) {
        interact_say("You do not have enough gold.");
    } else {
        log_format(line, (int)sizeof(line), "You have the %s.", lodging_food_name(which));
        interact_say(line);
    }
}

/* What the merchant's panel chose. */
static void service_chosen(void)
{
    Shop shop;
    int choice;

    choice = screens_service_choice();
    if (choice == SERVICE_CURE) {
        temple_cure();
    } else if (choice == SERVICE_JOIN) {
        guild_join_asked();
    } else if (choice == SERVICE_GUILD) {
        guild_service_asked();
    } else if (choice == SERVICE_PICKED) {
        guild_training_picked(service_picked());
        return;
    } else if (choice == SERVICE_ROOM) {
        tavern_room(service_days());
    } else if (choice == SERVICE_FOOD) {
        tavern_food(service_food());
    } else if (choice == SERVICE_TALK) {
        talk_to(g_talk_faction, g_talk_flat, (unsigned)g_talk_flat * 31u + (unsigned)g_talk_faction);
    } else if (choice == SERVICE_BANK) {
        screens_open_bank(&g_player, travel_region());
    } else if (shop_here(&shop)) {
        screens_open_trade(&g_player, &shop, choice == SERVICE_REPAIR ? TRADE_REPAIR : TRADE_SELL, clock_day());
    }
    screen_over_play();
}

/*
 * What the use key does that the engine's doors and levers do not: a
 * passage (travel.h), and a person, until there is talk (P9).
 */
static int on_use(const Scene *scene, const Usable *u, int index)
{
    (void)scene;
    (void)index;
    if (u->kind == USE_PASSAGE) {
        const char *why;

        /* A shut building: locked, unless the player is at its lock. */
        if (u->what == DOOR_ENTER && !door_lets_in(u)) {
            return 1;
        }
        /* Going somewhere is carried out after this frame's end (game_frame); a shut way says why. */
        why = travel_use(u);
        if (why) {
            interact_say(why);
        }
        return 1;
    }
    if (u->kind == USE_TALK) {
        if (g_mode == HUDBAR_STEAL) {
            /* P12-a supplies pickpocketing. Never start an ordinary
             * conversation when the player explicitly selected Steal. */
            interact_say("You cannot steal from this person.");
            return 1;
        }
        person_used(u);
        return 1;
    }
    if (u->kind == USE_OTHER && u->what == OTHER_SHELF) {
        shelf_used();
        return 1;
    }
    return 0;
}

/* The player died (a fall, a blow): classic's death screen over the stilled world. */
static void die(void)
{
    interact_say("You have died.");
    menu_open(MENU_DEATH);
    gfx_set_pause(1);
    apply_pointer(1);
}

/* The HUD's face: a vampire's own (VAMP00I0.CIF, a woman's by race from 0, a man's from 8), else the race's. */
static void show_face(void)
{
    const CareerRace *race;

    if (ills_curse() == ILL_VAMPIRE) {
        overlay_set_face("VAMP00I0.CIF", (g_player.sheet.female ? 0 : 8) + g_player.sheet.race - 1);
        return;
    }
    race = career_race(g_player.sheet.race);
    if (race) {
        overlay_set_face(g_player.sheet.female ? race->heads_female : race->heads_male, g_player.sheet.face);
    }
}

/*
 * What a day of afflictions did, said. A vampire's turning is DFU's fake
 * death: two weeks pass and they wake at dusk (the cemetery it carries them
 * to is not built: they wake where they fell).
 */
static void ills_said_day(int said)
{
    if (said & ILLS_DIED) {
        interact_say("Your disease has killed you.");
        die();
        return;
    }
    if (said & ILLS_TURNED) {
        if (ills_curse() == ILL_VAMPIRE) {
            clock_set_day(clock_day() + 14);
            clock_set_minute(19.0f * 60.0f);
            g_last_minute = df_ifloor(game_minutes());
            character_rest_full(&g_player.sheet);
            show_face();
            interact_say("Death is not eternal. You wake two weeks on, at dusk, a vampire.");
        } else {
            interact_say(ills_curse() == ILL_WEREWOLF ? "You have become a werewolf." : "You have become a wereboar.");
        }
        return;
    }
    if (said & ILLS_DREAM) {
        interact_say(ills()->infection == ILL_VAMPIRE ? "You dream of a cold embrace in the dark."
                                                       : "You dream of the moon, and of running through the night.");
    } else if (said & ILLS_PASSED) {
        interact_say("You feel better.");
    } else if (said & ILLS_FELT_BAD) {
        interact_say("You feel somewhat bad.");
    }
}

static int hud_bar(void);

/* The riding pictures, a frame each (MRED00I0.CFA the horse, MRED01I0.CFA the cart), read the first time a mount is ridden. */
static int g_ride_pic[2][RIDE_FRAMES];
/* The picture's height in classic's rows; its width before DFU's 0.8 a pixel across. */
#define RIDE_PICTURE_H 105
#define RIDE_PICTURE_W 106
static int g_ride_frame;

/* A mount's frames onto the card, kept with the game's pictures. 0 when its file does not read. */
static int read_riding(int cart)
{
    static const char *const files[2] = { "MRED00I0.CFA", "MRED01I0.CFA" };
    static df_u8 rgb[768];
    CfaPicture cfa;
    MemHeap *scratch;
    MemHeap *was;
    int f;
    int ok;

    if (g_ride_pic[cart][0]) {
        return 1;
    }
    if (!img_art_palette(rgb)) {
        return 0;
    }
    scratch = mem_heap_new();
    if (!scratch) {
        return 0;
    }
    was = mem_heap_use(scratch);
    ok = cfa_load(files[cart], &cfa) && cfa.frames >= RIDE_FRAMES;
    for (f = 0; ok && f < RIDE_FRAMES; f++) {
        g_ride_pic[cart][f] = pic_load(cfa.pixels + (df_u32)f * (df_u32)cfa.width * (df_u32)cfa.height, cfa.width, cfa.height, rgb, 1);
        ok = g_ride_pic[cart][f] != 0;
    }
    mem_heap_use(was);
    mem_heap_delete(scratch);
    return ok;
}

/*
 * The horse's head or the cart's reins at the screen's foot, over the bar when it
 * is shown: 105 of classic's rows tall and 0.8 of that to a pixel across, as DFU
 * draws it (TransportManager), the frame moving with the player.
 */
static void draw_riding(void)
{
    int cart;
    int bottom;
    int w;

    cart = g_player.riding == RIDE_CART;
    if (!read_riding(cart)) {
        return;
    }
    bottom = hud_bar() ? HUDBAR_TOP : SCREEN_H;
    w = RIDE_PICTURE_W * 4 / 5 * 12 / 10;
    screen_draw(g_ride_pic[cart][g_ride_frame], (SCREEN_W - w) / 2, bottom - RIDE_PICTURE_H, w, RIDE_PICTURE_H);
}

/* The bottom bar takes clicks only while it is drawn. Full screen hides it. */
static int hud_bar(void)
{
    return overlay_panel() && !viewopt_full();
}

/* What the game draws over the frame: the weapon and the HUD in play, then the open menu and its pointer. */
static void draw(void)
{
    /* A composed screen fills the screen: only it and the pointer. */
    if (screens_is_open()) {
        screens_draw();
        return;
    }
    if (g_playing) {
        int pic;
        int left;
        int top;
        int w;
        int h;

        /* Under the HUD, as classic: the compass and the bars over the blade, or the horse's head and the reins. */
        if (g_player.riding) {
            draw_riding();
        } else if (!g_sheathed && weapon_shown(&pic, &left, &top, &w, &h)) {
            screen_draw(pic, left, top, w, h);
        }
        overlay_draw();
        /* Cursor, or Tab's free pointer: over the world. A menu or a screen draws its own.
           Classic Cursor uses the arrow or X for the region; otherwise ARROW.RAW. */
        if (g_hud_cursor && menu_current() == MENU_NONE) {
            int px;
            int py;
            int x;
            int y;

            input_pointer(&px, &py);
            screen_from_pointer(px, py, &x, &y);
            menu_draw_world_pointer(control_classic() ? control_region(x, y, hud_bar() ? HUDBAR_TOP : 200) : -1, x, y);
        }
    }
    menu_draw();
}

/* A menu open over play stills the world and gives the mouse to the pointer; closed, back to play. */
static void menu_over_play(int which)
{
    ride_quiet();
    control_block(&g_control);
    weapon_cancel();
    menu_open(which);
    gfx_set_pause(which != MENU_NONE);
    apply_pointer(which != MENU_NONE);
}

/* The inventory over the stilled world: the player's own, or with what they are searching beside it. */
/*
 * Whether the cart's wagon is there: anywhere outdoors or in a building, in a
 * dungeon only by its way out (DFU's 5 m), where the cart waits.
 */
static int wagon_in_reach(void)
{
    TravelWhere here;
    int origin[2];
    const Usable *exit;
    float dx;
    float dz;

    travel_here(&here, origin);
    if (here.kind != TRAVEL_DUNGEON) {
        return 1;
    }
    exit = travel_nearest_passage(&g_level->scene, DOOR_EXIT, g_player.body.feet[0], g_player.body.feet[2]);
    if (!exit) {
        return 0;
    }
    dx = exit->cx - g_player.body.feet[0];
    dz = exit->cz - g_player.body.feet[2];
    return dx * dx + dz * dz <= WAGON_EXIT_REACH * WAGON_EXIT_REACH;
}

static void bag_over_play(Entity *searched)
{
    if (ills_beast()) {
        interact_say("You cannot use your inventory while shapechanged.");
        return;
    }
    wagon_set_reachable(wagon_in_reach());
    screens_open_bag(&g_player, searched);
    screen_over_play();
}

/* TEXT.RSC's word that camping in a town is not allowed (DFU's cityCampingIllegal). */
#define TEXT_CITY_CAMPING 17

/* 1 when the player stands in a town (a city, a hamlet or a village, not a farm or a lone tavern): DFU's IsPlayerInTown. */
static int in_a_town(void)
{
    int px;
    int py;
    int type;

    if (!travel_pixel(g_cam, &px, &py)) {
        return 0;
    }
    type = worldmap_location_type(px, py);
    return type == WORLDMAP_TOWN_CITY || type == WORLDMAP_TOWN_HAMLET || type == WORLDMAP_TOWN_VILLAGE;
}

/*
 * Where the player may rest (DaggerfallRestWindow.CanRest; UESP's Rest): a
 * town's streets are vagrancy; a town's building wants a room rented there;
 * a dungeon, the wild, a farm and a lone tavern are free.
 */
static int rest_rule_here(int *room_hours)
{
    TravelWhere here;
    int origin[2];
    int region;
    int index;
    int block;
    int record;
    float x;
    float z;

    *room_hours = 0;
    travel_here(&here, origin);
    if (here.kind == TRAVEL_DUNGEON || !in_a_town()) {
        return REST_RULE_FREE;
    }
    if (here.kind == TRAVEL_WORLD) {
        return REST_RULE_VAGRANCY;
    }
    if (!travel_town_here(g_cam, &region, &index, &x, &z, &block, &record)) {
        return REST_RULE_FREE;
    }
    *room_hours = lodging_hours_left(region, index, block, record, ills_now(clock_day(), clock_minute()));
    if (*room_hours > 0) {
        return REST_RULE_ROOM;
    }
    /* A guild that lets its members rest in its hall: the Fighters (not a tavern, which the data marks theirs too). */
    {
        int type;
        int faction;
        int quality;
        unsigned place;

        if (travel_building(&type, &faction, &quality, &place) && type != BUILDING_TAVERN && guild_can_rest_in(faction)) {
            return REST_RULE_FREE;
        }
    }
    return REST_RULE_NO_ROOM;
}

/* Rest over the stilled world, unless a creature awake forbids it (DFU's rest window). */
static void rest_over_play(void)
{
    int room_hours;
    int rule;

    if (foes_hunting()) {
        interact_say("You cannot rest with enemies nearby.");
        return;
    }
    /* An unfed vampire cannot rest (VampirismEffect.CheckStartRest). */
    if (ills_rest_refused(ills_now(clock_day(), clock_minute()))) {
        interact_say("You must feed before you can rest.");
        return;
    }
    screens_open_rest(&g_player);
    /* Two steps: an argument's order of evaluation is not C's to promise. */
    rule = rest_rule_here(&room_hours);
    rest_set_rule(rule, room_hours);
    screen_over_play();
}

/* Game minutes since this game began. */
static float game_minutes(void)
{
    return (float)(clock_day() - g_day_zero) * CLOCK_DAY + clock_minute();
}

/* The game as it stands, into the slot in use, by this name. What is loaded goes into the keep table first. */
static int save_game(const char *name)
{
    SaveState state;

    df_copy(state.name, name, SAVE_NAME);
    travel_keep_now();
    /* The feet as they are, with the stream's origin that frame has: a stream may open on another. */
    travel_here(&state.where, state.origin);
    state.feet[0] = g_player.body.feet[0];
    state.feet[1] = g_player.body.feet[1];
    state.feet[2] = g_player.body.feet[2];
    state.yaw = g_player.yaw;
    state.pitch = g_player.pitch;
    state.view_yaw = g_control.view_yaw;
    state.view_pitch = g_control.view_pitch;
    state.minute = clock_minute();
    state.day = clock_day();
    state.day_zero = g_day_zero;
    state.seed = loot_seed();
    state.sheet = g_player.sheet;
    state.pack = g_player.pack;
    return save_write(&state);
}

/* A slot's save read, to start from at the next place. 1 when there was one. */
static int load_game(int slot)
{
    save_use_slot(slot);
    if (!save_exists() || !save_read(&g_loaded)) {
        return 0;
    }
    g_new_sheet = g_loaded.sheet;
    g_new_pack = g_loaded.pack;
    g_load_waiting = 1;
    g_dead = 0;
    travel_from_save(&g_loaded.where, g_loaded.feet, g_loaded.origin, g_loaded.yaw, g_loaded.pitch);
    return 1;
}

/* A new character's game: what was changed or dropped in the last one is gone. */
static void forget_last_game(void)
{
    court_forget();
    keep_clear();
    pile_new_game();
    loot_new_game(prof_now());
    g_load_waiting = 0;
    travel_start_game();
}

/* The character sheet over the stilled world: a level due is taken there. */
static void sheet_over_play(void)
{
    screens_open_sheet(&g_player);
    screen_over_play();
}

/* A HUD intention is dispatched here, beside its keyboard equivalent, so
 * neither path duplicates a screen's ownership, pause or creature checks.
 * The caller consumes the click even on the bar's gaps: never an attack. */
static void hud_command(int action)
{
    switch (action) {
    case HUDBAR_CHARACTER:
        sheet_over_play();
        break;
    case HUDBAR_OPTIONS:
        menu_over_play(MENU_OPTIONS);
        break;
    case HUDBAR_INVENTORY:
        bag_over_play(0);
        break;
    case HUDBAR_REST:
        rest_over_play();
        break;
    case HUDBAR_MODE:
        g_mode = (g_mode + 1) % HUDBAR_MODES;
        overlay_set_mode(g_mode);
        interact_say(hudbar_mode_name(g_mode));
        break;
    case HUDBAR_SHEATH:
        g_sheathed = !g_sheathed;
        weapon_cancel();
        break;
    case HUDBAR_STATUS:
        {
            char line[96];
            int hour, minute;
            int year, month, day;

            clock_hm(&hour, &minute);
            clock_date(&year, &month, &day);
            log_format(line, (int)sizeof(line), "%s, %d %s %d, %02d:%02d",
                       clock_weekday_name(clock_weekday()), day + 1, clock_month_name(month), year, hour, minute);
            interact_say(line);
        }
        break;
    /* The original art stays visible. Its future features do not silently
     * perform another action; their screens are connected in P10/P12. */
    case HUDBAR_MAP:
        map_over_play();
        break;
    case HUDBAR_SPELLBOOK:
        interact_say("The spellbook is not available yet.");
        break;
    case HUDBAR_MAGIC_ITEM:
        interact_say("No magic item can be used.");
        break;
    case HUDBAR_TRANSPORT:
        {
            int px;
            int py;
            const char *said;

            /* Outdoors only: a horse does not go into a house or down a dungeon. */
            said = ride_toggle(&g_player, travel_pixel(g_cam, &px, &py));
            if (said) {
                interact_say(said);
            }
        }
        break;
    }
}

/* The attack button this frame: a blade's gesture, or a bow's draw if there is an arrow to draw. */
static void attack_input(const ControlFrame *in, float dt)
{
    int pressed;

    pressed = in->attack_pressed;
    if (pressed && weapon_is_bow() && inventory_arrows(&g_player.pack) < 0) {
        interact_say("You have no arrows.");
        return;
    }
    weapon_gesture(pressed, in->attack_held, in->attack_released, in->gesture_dx, in->gesture_dy, dt);
}

/* The bow looses: an arrow gone, flying along the view. */
static void shoot_arrow(const Camera *cam, const float *eye)
{
    Basis basis;
    float dir[3];

    if (!inventory_spend_arrow(&g_player.pack)) {
        return;
    }
    camera_basis(cam, &basis);
    dir[0] = basis.fx * basis.cp;
    dir[1] = basis.sp;
    dir[2] = basis.fz * basis.cp;
    combat_shoot(&g_player.sheet, inventory_hand(&g_player.pack), &g_level->scene, g_player.body.feet, eye, dir);
}

/*
 * The use key: a body or a heap the view rests on is searched; anything
 * else is the engine's to use (doors, levers, passages). With nothing to
 * find it says so and opens nothing.
 */
static void use_key(const Camera *cam)
{
    Entity *searched;

    /* Info is observational: do not queue the engine's use, open a pack,
     * change a lock or walk through a passage while asking what it is. */
    if (g_mode == HUDBAR_INFO) {
        UseHit hit;

        if (interact_aim(&g_level->scene, cam, &hit)) {
            const Usable *u;
            int type, quality;
            const BuildingKind *b;

            u = &g_level->scene.usables[hit.usable];
            b = travel_door_building(u, &type, &quality) ? building_kind(type) : 0;
            interact_say(b ? b->name : (u->label ? u->label : "You see nothing unusual."));
        } else {
            interact_say("You see nothing unusual.");
        }
        return;
    }
    if (g_mode == HUDBAR_TALK) {
        UseHit hit;

        if (street_person_used(cam)) {
            return;
        }
        if (interact_aim(&g_level->scene, cam, &hit) && g_level->scene.usables[hit.usable].kind == USE_TALK) {
            person_used(&g_level->scene.usables[hit.usable]);
        } else {
            interact_say("There is no one to talk to.");
        }
        return;
    }

    searched = loot_find(&g_level->scene, cam);
    if (!searched) {
        /* Someone walking the street, if the view rests on them; else what the engine's ray finds. */
        if (g_mode == HUDBAR_STEAL || !street_person_used(cam)) {
            Basis b;

            /* Use the exact camera supplied by keyboard or pointer picking.
             * The engine must not fall back to the center of the drawn view. */
            camera_basis(cam, &b);
            interact_request_use_from(cam->x, cam->y, cam->z, b.fx * b.cp, b.sp, b.fz * b.cp);
        }
    } else if (!bag_finds_things(searched, g_player.sheet.level)) {
        interact_say("You find nothing.");
    } else {
        bag_over_play(searched);
    }
}

void game_setup(void)
{
    /* FACTION.TXT, read once at the start: no screen's heap is current, and no click waits for it. */
    standing_new_game(0u);
    /* Escape is the game's menu, not a quit; the left button attacks, so use is the game's own key. */
    input_set_escape_quits(0);
    gfx_set_use_key(0);
    keys_load();
    /* The engine's position corner is a viewer's; the game has its compass. */
    hud_set(0);
    /*
     * Outdoors the weather sets how far one sees and the fog (docs/weather.md,
     * the simulated view): a clear day long, a storm short. The game's on;
     * LEVELFALL keeps it off to inspect a place at any distance.
     */
    weather_set_sim(1);
    pic_set_room(GAME_PICTURE_KB);
    overlay_load();
    menu_setup();
    gfx_set_extra(draw);
    gfx_set_under(draw_under);
    commands_setup();
    cmd_var("panel", overlay_panel, overlay_set_panel, 0, 1, "classic's bottom panel of the HUD; panel alone flips it");
    cmd_toggles("panel");
    g_playing = 0;
    menu_open(MENU_TITLE);
    apply_pointer(1);
}

int game_playing(void)
{
    return g_playing;
}

int game_world_shows(void)
{
    /* All settings tabs now share a framed shell over the paused world. */
    return g_playing && !screens_is_open();
}

/* What every arrival sets up: the frame's creatures gone with the last place, the camera on the player. */
static void arrive(const Level *level, Camera *cam, const TravelPose *at)
{
    int px;
    int py;

    const char *said;

    g_level = level;
    /* Into a building or a dungeon on foot: the horse waits outside (TransportManager). */
    if (!travel_pixel(cam, &px, &py)) {
        ride_get_down(&g_player);
    }
    combat_reset();
    foes_reset();
    watch_reset();
    player_place(&g_player, at->x, at->y, at->z, at->yaw, at->pitch);
    if (!g_control_scheme) {
        /* A classic save may be resumed with View selected in settings.
         * Preserve its visible direction, while View movement follows that
         * same direction rather than inheriting classic's side-look offset. */
        player_turn(&g_player, g_control.view_yaw, g_control.view_pitch);
        g_control.view_yaw = g_control.view_pitch = 0.0f;
    }
    camera_set(cam, at->x, at->y, at->z, g_player.yaw, g_player.pitch);
    control_view(&g_control, cam);
    /*
     * The engine's walker gives the walking to the game's player: it is
     * not stepped, and the console's tcl is the game's collision switch
     * (docs/collision.md, A game's player). On by default.
     */
    walker_set_game(&g_player.body);
    walker_set_on(1);
    interact_set_handler(on_use);
    g_playing = 1;
    menu_over_play(MENU_NONE);
    said = travel_said();
    if (said) {
        interact_say(said);
    }
}

float game_eye(void)
{
    return WALK_EYE;
}

void game_enter(const Level *level, Camera *cam, const TravelPose *at)
{
    /* A saved character's first frame must not finish the previous game's
     * drawn bow or queued hit, even when both use the same weapon picture. */
    weapon_cancel();
    g_mode = HUDBAR_GRAB;
    g_sheathed = 0;
    g_control_scheme = control_classic();
    g_hud_cursor = g_control_scheme;
    mouse_set_cursor(g_control_scheme);
    control_reset(&g_control);
    overlay_set_crosshair(!g_control_scheme);
    overlay_set_mode(g_mode);
    /* Who creation made or the save holds, and what they carry (game_frame set these). */
    player_init(&g_player, &g_new_sheet);
    g_player.pack = g_new_pack;
    player_wear_changed(&g_player);
    show_face();
    g_day_zero = clock_day();
    g_last_minute = df_ifloor(game_minutes());
    if (g_load_waiting) {
        /* The save's clock, its seed. Its place was travel_from_save's. */
        g_load_waiting = 0;
        clock_set_minute(g_loaded.minute);
        clock_set_day(g_loaded.day);
        g_day_zero = g_loaded.day_zero;
        g_last_minute = df_ifloor(game_minutes());
        loot_new_game(g_loaded.seed);
        g_control.view_yaw = g_loaded.view_yaw;
        g_control.view_pitch = g_loaded.view_pitch;
    }
    commands_player(&g_player);
    arrive(level, cam, at);
}

void game_arrive(const Level *level, Camera *cam, const TravelPose *at)
{
    arrive(level, cam, at);
}

int game_frame(Camera *cam, const Input *in, float dt)
{
    int asked;
    /* The click that closed a menu is not a swing. */
    int closed;
    ControlFrame controls;

    prof_begin(PROF_PLAY);
    g_cam = cam;
    if (in->reset) {
        /* Engine discontinuities retain release bits for other consumers.
         * Never let focus/capture loss loose a bow or finish a click swing. */
        control_block(&g_control);
        weapon_cancel();
    }
    g_stealing = g_mode == HUDBAR_STEAL || (in->held & INPUT_BIT(INPUT_RUN)) != 0;
    /* A screen before any place: the title or the start menu. */
    if (!g_playing) {
        /* Creation, once Start is chosen: it ends with a character for the first place. */
        if (screens_is_open()) {
            asked = screens_frame(in, dt);
            prof_end(PROF_PLAY);
            if (asked == SCREENS_MADE && create_result(&g_new_sheet, &g_new_pack)) {
                screens_close();
                forget_last_game();
                return GAME_START;
            }
            if (asked == SCREENS_SLOT) {
                return load_game(screens_slot()) ? GAME_START : GAME_GO_ON;
            }
            /* The slots put away, or Escape on the race map: the start menu again. Not a new game. */
            if (asked == SCREENS_CLOSED) {
                menu_open(MENU_MAIN);
            }
            return GAME_GO_ON;
        }
        asked = menu_frame(in);
        if (asked == MENU_LOAD) {
            if (save_any()) {
                menu_open(MENU_NONE);
                screens_open_slots(0);
            } else {
                menu_say("There is no saved game.");
            }
        }
        if (asked == MENU_START) {
            /* Before creation: its answers move reputations. What the map shows is a new game's too. */
            standing_new_game(prof_now());
            atlas_new_game();
            wagon_new_game();
            ills_new_game();
            lodging_new_game();
            guild_new_game();
            bank_set_ship(BANK_SHIP_NONE);
            automap_clear();
            screens_open_create();
            /* Its pictures missing: the default character starts, as before creation was built. */
            if (!screens_is_open()) {
                create_default(&g_new_sheet, &g_new_pack);
                forget_last_game();
                prof_end(PROF_PLAY);
                return GAME_START;
            }
        }
        prof_end(PROF_PLAY);
        return asked == MENU_QUIT ? GAME_QUIT : GAME_GO_ON;
    }
    mem_zero(&controls, sizeof(controls));
    closed = in->reset != 0;
    /* The controls menu can change scheme while paused. Adopt it once,
     * preserving the actual view by folding Alt offsets into body facing. */
    if (g_control_scheme != control_classic()) {
        g_control_scheme = control_classic();
        player_turn(&g_player, g_control.view_yaw, g_control.view_pitch);
        control_reset(&g_control);
        g_hud_cursor = g_control_scheme;
        mouse_set_cursor(g_control_scheme);
        overlay_set_crosshair(!g_hud_cursor);
        input_set_pointer(g_hud_cursor || screens_is_open() || menu_current() != MENU_NONE);
        weapon_cancel();
        closed = 1;
    }
    if (screens_is_open()) {
        closed = 1;
        asked = screens_frame(in, dt);
        /*
         * Time a screen passes (rest, prison, the travel map's countdown) is
         * its own: it already gave back or took what those hours do. Play's
         * minute-by-minute fatigue must not walk them again, or a night's rest
         * leaves the player exhausted (and dead with a creature near).
         */
        g_last_minute = df_ifloor(game_minutes());
        /* A rest asked in a town's streets: vagrancy, and the guards take the player before the court (DFU spawns them). */
        if (!screens_is_open() && rest_vagrant()) {
            char line[160];

            if (textrsc_read(TEXT_CITY_CAMPING, line, (int)sizeof(line)) <= 0) {
                df_copy(line, "Camping within the city walls is illegal.", (int)sizeof(line));
            }
            interact_say(line);
            court_commit(CRIME_VAGRANCY, travel_region());
        }
        if (asked == SCREENS_SLOT && !screens_slot_saving()) {
            prof_end(PROF_PLAY);
            return load_game(screens_slot()) ? GAME_START : GAME_GO_ON;
        }
        if (asked == SCREENS_SERVICE) {
            service_chosen();
            prof_end(PROF_PLAY);
            return GAME_GO_ON;
        }
        if (asked == SCREENS_TO_TRAVEL) {
            travel_map_over_play();
            prof_end(PROF_PLAY);
            return GAME_GO_ON;
        }
        if (asked == SCREENS_JOURNEY) {
            gfx_set_pause(0);
            apply_pointer(0);
            prof_end(PROF_PLAY);
            return journey_made() ? GAME_TRAVEL : GAME_GO_ON;
        }
        if (asked == SCREENS_SEEN) {
            /* Seen stealing: the shop's region's crime, the court next. */
            court_commit(CRIME_THEFT, travel_region());
            screens_open_court(&g_player);
            screen_over_play();
            prof_end(PROF_PLAY);
            return GAME_GO_ON;
        }
        if (asked == SCREENS_COURT_OVER) {
            gfx_set_pause(0);
            apply_pointer(0);
            /* Out in the street, whatever the sentence: the crime was in a building, and the court does not send you back. */
            if (travel_out_to_street()) {
                prof_end(PROF_PLAY);
                return GAME_TRAVEL;
            }
            prof_end(PROF_PLAY);
            return GAME_GO_ON;
        }
        if (asked == SCREENS_SLOT || asked == SCREENS_CLOSED) {
            const char *said;

            /* The dead, having put the slots away, have only the start menu. */
            if (g_dead) {
                g_dead = 0;
                g_playing = 0;
                gfx_set_pause(0);
                apply_pointer(1);
                menu_open(MENU_MAIN);
                prof_end(PROF_PLAY);
                return GAME_GO_ON;
            }
            gfx_set_pause(0);
            apply_pointer(0);
            if (asked == SCREENS_SLOT) {
                save_use_slot(screens_slot());
                interact_say(save_game(screens_slot_name()) ? "Game saved." : "The game could not be saved.");
            }
            said = screens_said();
            if (said) {
                interact_say(said);
            }
        }
    } else if (menu_current() != MENU_NONE) {
        closed = 1;
        asked = menu_frame(in);
        if (asked == MENU_QUIT) {
            prof_end(PROF_PLAY);
            return GAME_QUIT;
        }
        if (asked == MENU_CLOSED) {
            menu_over_play(MENU_NONE);
        } else if (asked == MENU_SAVE) {
            menu_open(MENU_NONE);
            screens_open_slots(1);
        } else if (asked == MENU_LOAD || asked == MENU_DEATH_OVER) {
            /* From the options, or dead: which save, if there is any. */
            g_dead = asked == MENU_DEATH_OVER;
            if (save_any()) {
                menu_open(MENU_NONE);
                apply_pointer(1);
                screens_open_slots(0);
            } else if (asked == MENU_LOAD) {
                menu_say("There is no saved game.");
            } else {
                /* Dead with nothing saved: back to the start menu, the place left for the next start to replace. */
                g_dead = 0;
                g_playing = 0;
                gfx_set_pause(0);
                apply_pointer(1);
                menu_open(MENU_MAIN);
                prof_end(PROF_PLAY);
                return GAME_GO_ON;
            }
        }
    } else if (in->reset) {
        /* First snapshot after capture/focus ownership changed: held keys
         * can appear pressed again, but no play shortcut belongs to it. */
    } else if (in->pressed & INPUT_BIT(INPUT_MENU)) {
        menu_over_play(MENU_OPTIONS);
    } else if ((in->pressed & INPUT_BIT(INPUT_INVENTORY)) && !gfx_paused()) {
        bag_over_play(0);
    } else if ((in->pressed & INPUT_BIT(INPUT_CHARACTER)) && !gfx_paused()) {
        sheet_over_play();
    } else if ((in->pressed & INPUT_BIT(INPUT_MAP)) && !gfx_paused()) {
        map_over_play();
    } else if ((in->pressed & INPUT_BIT(INPUT_TRANSPORT)) && !gfx_paused()) {
        /* The transport key is the HUD's transport button (T, as Daggerfall Unity). */
        hud_command(HUDBAR_TRANSPORT);
    } else if ((in->pressed & INPUT_BIT(INPUT_TRAVEL_MAP)) && !gfx_paused()) {
        travel_map_over_play();
    } else if ((in->pressed & INPUT_BIT(INPUT_REST)) && !gfx_paused()) {
        rest_over_play();
    } else if ((in->pressed & INPUT_BIT(INPUT_SWAP_HANDS)) && !gfx_paused()) {
        if (!inventory_swap_hands(&g_player.pack)) {
            interact_say("A shield keeps your left hand.");
        }
    } else if ((in->pressed & INPUT_BIT(INPUT_CURSOR)) && !gfx_paused()) {
        /* Cancel any pending bow/gesture on entry. Closing the HUD cursor
         * never turns the release of its last click into a random swing. */
        g_hud_cursor = !g_hud_cursor;
        apply_pointer(0);
        weapon_cancel();
        closed = 1;
    } else if (g_hud_cursor && (in->mouse_pressed & INPUT_MOUSE_LEFT) && !gfx_paused()) {
        int px, py, x, y;

        input_pointer(&px, &py);
        screen_from_pointer(px, py, &x, &y);
        if (hud_bar() && hudbar_contains(x, y)) {
            hud_command(hudbar_hit(x, y));
            closed = 1;
        }
    }
    if (closed || menu_current() != MENU_NONE || screens_is_open() || gfx_paused()) {
        control_block(&g_control);
        weapon_cancel();
    } else {
        int px, py, x, y;

        input_pointer(&px, &py);
        screen_from_pointer(px, py, &x, &y);
        control_step(&g_control, in, control_classic(), g_hud_cursor, x, y,
                     hud_bar() ? HUDBAR_TOP : 200, g_sheathed, dt, &controls);
        if (controls.use) {
            Camera aim;

            control_point(cam, px, py, &aim);
            use_key(&aim);
        }
    }
    /* The console stands the world still too (gfx_paused): the player stands with it. */
    if (menu_current() == MENU_NONE && !screens_is_open() && !gfx_paused()) {
        if (!closed && (in->pressed & INPUT_BIT(INPUT_USE))) {
            use_key(cam);
        }
        /* A passage used: the program loads where it leads, between this frame and the next. */
        if (travel_asked()) {
            prof_end(PROF_PLAY);
            return GAME_TRAVEL;
        }
    }
    /* The use key may just have opened the inventory: the world stands from this frame. */
    if (menu_current() == MENU_NONE && !screens_is_open() && !gfx_paused()) {
        /* A loan past its day, looked at once a day; a crime waiting is tried now. */
        if (clock_day() != g_loans_day) {
            g_loans_day = clock_day();
            bank_check_loans(g_loans_day);
            /* A holiday is said when its day begins, or the game is come back to on it. */
            {
                const Holiday *h;
                int year;
                int month;
                int day;
                char line[48];

                clock_date(&year, &month, &day);
                h = holiday_on(month, day);
                if (h) {
                    log_format(line, (int)sizeof(line), "Today is %s.", h->name);
                    interact_say(line);
                }
            }
            /* Each disease's day, an infection's dream or its curse (docs/game.md, Afflictions). */
            ills_said_day(ills_day(&g_player, clock_day()));
        }
        if (court_pending()) {
            screens_open_court(&g_player);
            screen_over_play();
            prof_end(PROF_PLAY);
            return GAME_GO_ON;
        }
        /* Skills look at their uses every six game hours; a level due opens the sheet (PlayerEntity.RaiseSkills). */
        character_raise_skills(&g_player.sheet, game_minutes());
        if (g_player.sheet.level_due) {
            sheet_over_play();
        }
        /* Each game minute that has passed tires the player, and may be too much (docs/game.md, Fatigue). */
        {
            TravelWhere here;
            int origin[2];
            int minute;
            int in_sun;
            int in_holy;
            int full_moon;
            int hour;
            int type;
            int faction;
            int quality;
            unsigned place;

            /* A vampire burns outdoors by day (6 to 18, DFU's IsDay) and in a temple; the moons force a lycanthrope. */
            travel_here(&here, origin);
            clock_hm(&hour, &minute);
            in_sun = here.kind == TRAVEL_WORLD && hour >= 6 && hour < 18;
            in_holy = here.kind == TRAVEL_INSIDE && travel_building(&type, &faction, &quality, &place) && type == BUILDING_TEMPLE;
            {
                int year;
                int month;
                int day;

                clock_date(&year, &month, &day);
                full_moon = ills_full_moon(year, month * 30 + day + 1);
            }
            minute = df_ifloor(game_minutes());
            while (g_last_minute < minute) {
                int how;
                int now;

                g_last_minute++;
                how = player_minute(&g_player, foes_hunting());
                if (how == PLAYER_DIED) {
                    die();
                    break;
                }
                now = g_day_zero * 1440 + g_last_minute;
                how = ills_minute(&g_player, now, in_sun, in_holy, full_moon);
                if (how & ILLS_DIED) {
                    interact_say(in_sun ? "The sun has burnt you to ash." : "You burn to death on holy ground.");
                    die();
                    break;
                }
                if (how & ILLS_FORCED) {
                    interact_say("You dream of the moon.");
                } else if (how & ILLS_BURNT) {
                    interact_say(in_sun ? "The sunlight burns you." : "The holy ground burns you.");
                } else if ((how & ILLS_HUNGER) && now % 30 == 0) {
                    interact_say("You need to hunt the innocent.");
                }
                if (how == PLAYER_COLLAPSED) {
                    interact_say("You collapse from exhaustion, and wake an hour later.");
                    clock_set_minute(clock_minute() + 60.0f);
                }
            }
        }
        /* Under water the breath runs down; none left is the end. */
        if (player_breath(&g_player, dt)) {
            interact_say("You have drowned.");
            die();
        }
        /* The weapon in hand follows the pack (equip, additem, a break); the mouse swings it. */
        player_check_weapon(&g_player);
        weapon_hold(inventory_weapon(&g_player.pack));
        /* The button that closed a screen, held or let go after it, is not a swing. */
        if (!weapon_gate(closed || g_sheathed || (!control_classic() && g_hud_cursor), controls.attack_held)) {
            attack_input(&controls, dt);
        }
        weapon_step(dt, g_player.sheet.attribute[ATTR_SPEED]);
        {
            float tx;
            float ty;
            float tz;
            float tyaw;

            /* A teleport the last frame reached (docs/interact.md): the body goes, the eye where it says. */
            if (interact_teleport(&tx, &ty, &tz, &tyaw)) {
                body_place(&g_player.body, tx, ty - g_player.body.eye, tz);
                g_player.yaw = tyaw;
            }
        }
        /* tcl 1 (walker_on): the body walks and collides; tcl 0: it flies through everything. */
        /* Cursor steering supplies fractions and dt-scaled angles. Physical
         * mouse motion only looks in View mode or under Alt; a right drag in
         * classic Cursor mode belongs exclusively to the weapon gesture. */
        if (controls.center) {
            g_player.pitch = 0.0f;
        }
        player_turn(&g_player, controls.yaw, controls.pitch);
        if (closed) {
            Input move;

            /* A rebound jump may share a menu key or physical UI button.
             * The handoff frame still advances gravity, with no jump edge. */
            move = *in;
            move.pressed = 0;
            player_step_motion(&g_player, &move, &g_level->scene, dt, walker_on(), 0, 0);
        } else {
            player_step_motion(&g_player, in, &g_level->scene, dt, walker_on(), controls.forward, controls.right);
        }
        {
            float speed;
            float height;

            /* A landing past classic's 5 m hurts. */
            if (body_landed(&g_player.body, &speed, &height)) {
                int died;

                /* tgm on: nothing is taken and nothing said. */
                died = player_hurt(&g_player, rules_fall_damage(height / RULES_UNITS_PER_M));
                if (died > 0) {
                    die();
                } else if (died == 0) {
                    interact_say("That hurt.");
                }
            }
        }
        /* A rider's picture steps and the hooves sound while the body moves on the ground (TransportManager). */
        {
            static float last[2];
            float dx;
            float dz;
            float pace;
            int moving;
            int slow;

            dx = g_player.body.feet[0] - last[0];
            dz = g_player.body.feet[2] - last[1];
            last[0] = g_player.body.feet[0];
            last[1] = g_player.body.feet[2];
            pace = dt > 0.0f ? df_sqrt(dx * dx + dz * dz) / dt : 0.0f;
            moving = g_player.riding != RIDE_FOOT && g_player.body.grounded && pace > 1.0f && pace < 50000.0f;
            slow = pace < 0.5f * rules_ride_speed(g_player.sheet.attribute[ATTR_SPEED], g_player.riding == RIDE_CART) * RULES_UNITS_PER_M;
            g_ride_frame = ride_frame(moving, dt);
            ride_sound(g_player.riding, moving, slow, dt);
        }
        player_camera(&g_player, cam);
        control_view(&g_control, cam);
        /* The swing's hit frame strikes what is in reach of the eye; flinches run out. */
        if (weapon_hit()) {
            float eye[3];

            eye[0] = cam->x;
            eye[1] = cam->y;
            eye[2] = cam->z;
            if (weapon_is_bow()) {
                shoot_arrow(cam, eye);
            } else {
                combat_strike(&g_player.sheet, inventory_hand(&g_player.pack), weapon_swing(), &g_level->scene,
                              g_player.body.feet, eye, cam->yaw);
            }
        }
        combat_frame(dt);
        /* A killing's HALT lines, one a second, while the guard closes. */
        watch_frame(dt);
        /* The creatures see, chase and strike (tai off: they stand); a blow may kill. */
        if (foes_frame(&g_player, &g_level->scene, dt)) {
            die();
            player_camera(&g_player, cam);
            control_view(&g_control, cam);
        }
        /* What is round the player in and out, a stream's creatures and people with it (travel.h, docs/streaming.md). */
        {
            float shift_x;
            float shift_z;

            travel_update(cam, travel_radius(gfx_far()), gfx_stream_budget(GAME_STREAM_MS), dt, &shift_x, &shift_z);
            /* A place walked onto in the world is one the map knows now. */
            {
                int px;
                int py;

                if (travel_pixel(cam, &px, &py)) {
                    atlas_learn(worldmap_location_id(px, py));
                }
            }
            /* A stream's origin moved: it moved the scene and the camera, the game moves what it keeps itself. */
            if (shift_x != 0.0f || shift_z != 0.0f) {
                player_shift(&g_player, shift_x, shift_z);
                foes_shift(shift_x, shift_z);
            }
            /* After the people have stepped: a wanted player turns whatever guards are out. */
            watch_pursue(cam, g_player.body.feet);
        }
    }
    overlay_set_yaw(cam->yaw);
    overlay_set_vitals(g_player.sheet.health, g_player.sheet.health_max, g_player.sheet.fatigue,
                       g_player.sheet.fatigue_max, g_player.sheet.magicka, g_player.sheet.magicka_max);
    prof_end(PROF_PLAY);
    return GAME_GO_ON;
}

void game_shutdown(void)
{
    interact_set_handler(0);
    commands_player(0);
    screens_close();
    weapon_free();
    walker_set_game(0);
    gfx_set_extra(0);
    gfx_set_under(0);
    gfx_set_pause(0);
    input_set_pointer(0);
    menu_shutdown();
    overlay_free();
}
