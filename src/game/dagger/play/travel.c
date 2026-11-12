#include "game/dagger/play/travel.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/progress.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/play/possessions.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"
#include "engine/ui/cmd.h"

static TravelHooks g_hooks;
void travel_install(const TravelHooks *hooks) { g_hooks = *hooks; }
int travel_count(void) { return g_hooks.count ? g_hooks.count() : 0; }
int travel_location(int ordinal, LocationInfo *out) { return g_hooks.location && g_hooks.location(ordinal,out); }
int travel_climate(int px, int py) { return g_hooks.climate ? g_hooks.climate(px,py) : 0; }
int play_travel_asked(void) { return g_hooks.asked && g_hooks.asked(); }
int travel_go(int region, int index)
{
    return game_state()->ready && character_state()->health > 0 && g_hooks.go && g_hooks.go(region,index);
}
int travel_at(const PlaceWhere *where)
{
    const PlaceWhere *here;
    travel_capture(); here = &world_where()->where;
    return here->kind == where->kind && here->located == where->located && here->region == where->region &&
        here->index == where->index && here->block == where->block && here->record == where->record &&
        here->origin_x == where->origin_x && here->origin_z == where->origin_z;
}
int travel_return(const PlaceWhere *where, float x, float eye_y, float z, int yaw, int pitch)
{
    return where->located && g_hooks.return_to && g_hooks.return_to(where,x,eye_y,z,yaw,pitch);
}
int travel_passage(const Usable *usable)
{
    const char *error;
    if (!g_hooks.passage || usable->kind != USE_PASSAGE) { return 0; }
    error = g_hooks.passage(usable); if (error) { msg_set(error); }
    return 1;
}
int travel_known(int ordinal)
{
    LocationInfo location;
    return ordinal >= 0 && ordinal < KNOWN_BYTES*8 &&
        (((world_where()->known[ordinal/8] >> (ordinal&7)) & 1) ||
         (travel_location(ordinal,&location) && location.known));
}
void travel_discover(int ordinal)
{
    if (ordinal >= 0 && ordinal < KNOWN_BYTES*8) { world_where()->known[ordinal/8] |= 1 << (ordinal&7); }
}
void travel_capture(void)
{
    WorldState *world;
    world = world_where();
    if (g_hooks.here) { g_hooks.here(&world->where,&world->px,&world->py); }
}
static int where_valid(const PlaceWhere *where)
{
    return where->kind >= 0 && where->kind <= 2 && where->located >= 0 && where->located <= 1 &&
        where->region >= 0 && where->region < 62 && where->index >= 0 && where->index < 4096 &&
        where->block >= 0 && where->block < 64 && where->record >= 0 && where->record < 256 &&
        where->has_back >= 0 && where->has_back <= 1 &&
        where->back_x >= 0 && where->back_x <= 32768000 && where->back_z >= 0 && where->back_z <= 16384000 &&
        where->back_yaw >= 0 && where->back_yaw < 2048 && where->origin_x >= -16384 && where->origin_x <= 16384 &&
        where->origin_z >= -16384 && where->origin_z <= 16384;
}
int world_valid(const WorldState *world)
{
    int i;
    if (!where_valid(&world->where) || !where_valid(&world->recall_where) ||
        world->px < 0 || world->px >= 1000 || world->py < 0 || world->py >= 500 ||
        world->map_count < 0 || world->map_count > MAP_ROWS) { return 0; }
    for (i = 0; i < world->map_count; i++) {
        const AutomapRow *row;
        row = &world->map[i];
        if (!row->place || row->x < -100000000 || row->x > 100000000 || row->y < -100000000 ||
            row->y > 100000000 || row->z < -100000000 || row->z > 100000000) { return 0; }
    }
    return 1;
}
void world_capture(void)
{
    WorldState *world;
    int i;
    world = world_where(); travel_capture();
    world->map_count = automap_count(); if (world->map_count > MAP_ROWS) { world->map_count = MAP_ROWS; }
    for (i = 0; i < world->map_count; i++) { world->map[i] = *automap_at(i); }
}
void world_restore(void)
{
    int i;
    automap_clear();
    for (i = 0; i < world_where()->map_count; i++) { automap_put(&world_where()->map[i]); }
}
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c+32 : c; }
static int matches(const char *name, const char *query, int exact)
{
    int i, j;
    for (i = 0; name[i] || !i; i++) {
        for (j = 0; query[j] && lower(name[i+j]) == lower(query[j]); j++) { }
        if (!query[j] && (!exact || !name[i+j])) { return 1; }
        if (exact) { return 0; }
    }
    return 0;
}
static void goto_command(int argc, char **argv)
{
    LocationInfo location, chosen;
    char query[96];
    int i, j, used, n, region, index, exact;
    if (!game_state()->ready || game_state()->mode != GAME_WORLD || character_state()->health <= 0) {
        log_text(LOG_PRINT,"Close the game screens before using goto."); return;
    }
    if (argc == 3 && cmd_parse_int(argv[1],&region) && cmd_parse_int(argv[2],&index)) {
        if (!travel_go(region,index)) { log_text(LOG_PRINT,"No map location has those numbers."); }
        return;
    }
    if (argc < 2) { log_text(LOG_PRINT,"goto <place name> or goto <region> <location>; ambiguous names list their numbers."); return; }
    used = 0;
    for (i = 1; i < argc; i++) {
        if (used && used < 95) { query[used++] = ' '; }
        for (j = 0; argv[i][j] && used < 95; j++) { query[used++] = argv[i][j]; }
    }
    query[used] = 0; n = 0; mem_zero(&chosen,sizeof(chosen));
    for (exact = 1; exact >= 0 && !n; exact--) {
        for (i = 0; i < travel_count(); i++) {
            if (travel_location(i,&location) && matches(location.name,query,exact)) {
                if (!n) { chosen = location; } n++;
            }
        }
        if (n == 1) {
            if (travel_go(chosen.region,chosen.index)) { log_print(LOG_PRINT,"Going to %s.",chosen.name); }
            else { log_text(LOG_PRINT,"Cannot change place now."); }
            return;
        }
        if (n > 1) {
            j = 0;
            for (i = 0; i < travel_count() && j < 12; i++) {
                if (travel_location(i,&location) && matches(location.name,query,exact)) {
                    log_print(LOG_PRINT,"%d %d: %s",location.region,location.index,location.name); j++;
                }
            }
            log_print(LOG_PRINT,"%d matches; use region and location numbers.",n); return;
        }
    }
    log_text(LOG_PRINT,"No map location matches that name.");
}
static void where_command(int argc, char **argv)
{
    (void)argc; (void)argv; travel_capture();
    log_print(LOG_PRINT,"Place %d, region %d location %d; map pixel %d,%d, origin %d,%d.",
        world_where()->where.kind,game_state()->region,game_state()->location,world_where()->px,
        world_where()->py,world_where()->where.origin_x,world_where()->where.origin_z);
}
/* bank [region]: the bank of a region's account (the current one without a number), to try the bank before bankers talk. */
static void bank_command(int argc, char **argv)
{
    int region;

    region = game_state()->region;
    if (argc > 1 && !cmd_parse_int(argv[1], &region)) { log_text(LOG_PRINT, "bank [region 0..61]"); return; }
    if (!game_state()->ready || !possessions_bank(region)) { log_text(LOG_PRINT, "The bank cannot open now."); }
}

void travel_commands(void)
{
    cmd_add("goto",goto_command,"place | region index","Go to any town, tavern or other map location (no journey cost/time).");
    cmd_add("where",where_command,"","Show the current place and world-map pixel.");
    cmd_add("bank",bank_command,"[region]","Open a region's bank, to try it before bankers talk.");
}
