#include "game/dagger/play/rest.h"
#include "game/dagger/play/lodging.h"
#include "game/dagger/play/law.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/dungeon.h"
#include "game/dagger/play/move.h"
#include "game/dagger/play/msg.h"
#include "game/dagger/play/vitals.h"
#include "game/dagger/play/progress.h"
#include "game/dagger/play/encounter.h"
#include "engine/core/mem.h"

static RestState g;
static const Scene *g_scene;
const RestState *rest_state(void) { return &g; }
void rest_reset(void) { mem_zero(&g,sizeof(g)); g_scene = 0; }

static int (*g_site)(RestSite *out);

void rest_set_site(int (*site)(RestSite *out))
{
    g_site = site;
}

static void (*g_room)(df_u32 building, int tavern);

void rest_set_room(void (*go)(df_u32 building, int tavern))
{
    g_room = go;
}

/* rest_allowed (src/lifted/rest.c 197): 1 when the player may sleep here; else the reason is said and the screen closes. */
static int may_sleep(void)
{
    RestSite site;
    int to_room, why;

    if (!g_site || !g_site(&site)) { return 1; }
    why = rest_allowed(&site, game_state()->minutes, &to_room);
    if (why == REST_NO_ROOM) { msg_set(no_room_line()); }
    else if (why == REST_VAGRANT) { msg_set(words_state(17)); }
    /* tavern_go_to_room: a rented room, or a knight's hall, puts the player on its bed. */
    if (to_room && g_room) { g_room(site.building, site.building_type == 15); }
    /* Vagrancy and trespass are crime 8 (rest_allowed notes it and calls the watch). */
    if (why == REST_VAGRANT || why == REST_TRESPASS) { law_note(CRIME_VAGRANCY); guards_summon(1); }
    return why == REST_OK;
}

void rest_close(void)
{
    if (g.open) { state_mode_pop(); progress_rest(); }
    g.open = 0; g_scene = 0; g.revision++;
}

int rest_open(const Scene *scene, const Camera *cam)
{
    if (g.open) { return 1; }
    if (!game_state()->ready || character_state()->health <= 0) { return 0; }
    if (dungeon_nearby(cam,1024)) { msg_set("There are enemies nearby."); return 0; }
    if (!scene || move_in_water() || !move_body()->grounded) {
        msg_set("You cannot sleep now."); return 0;
    }
    if (!state_mode_push(GAME_REST)) { return 0; }
    rest_reset(); g.open = 1; g_scene = scene; g.revision++;
    return 1;
}

void rest_recover(void)
{
    CharacterState *who;
    int rate, heal, fatigue;
    who = character_state(); rate = 60;
    if (who->health <= 0) { return; }
    /* Privateer's Hold is indoors: rapid healing in darkness or always. */
    if (game_state()->career.rapid & 6) { rate += 40; }
    rate += who->skill[0];
    heal = who->max_health*rate/1000+(who->attribute[ATTR_END]-50)/10;
    if (heal < 1) { heal = 1; }
    who->health += heal;
    if (who->health > who->max_health) { who->health = who->max_health; }
    fatigue = (game_state()->base_attribute[ATTR_STR]+game_state()->base_attribute[ATTR_END]) << 3;
    who->fatigue = fatigue_add(who->fatigue,fatigue,who->attribute[ATTR_STR],who->attribute[ATTR_END]);
    if (!(game_state()->career.flags & 8)) {
        who->magicka += who->max_magicka >> 3;
        if (who->magicka > who->max_magicka) { who->magicka = who->max_magicka; }
    }
    skill_use(0);
}

static int healed(void)
{
    const CharacterState *who;
    who = character_state();
    return who->health == who->max_health &&
        who->fatigue == (who->attribute[ATTR_STR]+who->attribute[ATTR_END])*64 &&
        ((game_state()->career.flags & 8) || who->magicka == who->max_magicka);
}

void rest_frame(const PlayIn *in, const Camera *cam)
{
    int click, n, interrupted;
    df_u32 minute;
    if (!g.open || !in) { return; }
    if (in->ch == 27) { rest_close(); return; }
    click = (in->mouse_pressed & 1) && in->on_picture;
    if (g.stage == REST_CHOOSE) {
        if (click && in->cy >= 63 && in->cy <= 86 && in->cx >= 87 && in->cx <= 232) {
            play_sound(203);
            if (in->cx <= 183 && !may_sleep()) { rest_close(); return; }
            if (in->cx <= 134) { g.stage = REST_HOURS; }
            else if (in->cx <= 183) { g.until_healed = 1; g.stage = REST_RUNNING; }
            else { g.loiter = 1; g.stage = REST_HOURS; }
            g.revision++;
        }
    } else if (g.stage == REST_HOURS) {
        n = g.input[0] ? (g.input[1] ? 2 : 1) : 0;
        if (in->ch >= '0' && in->ch <= '9' && n < 2) { g.input[n] = (char)in->ch; g.input[n+1] = 0; g.revision++; }
        if (in->ch == 8 && n) { g.input[n-1] = 0; g.revision++; }
        if (in->ch == 13) {
            g.hours = 0;
            for (n = 0; g.input[n]; n++) { g.hours = g.hours*10+g.input[n]-'0'; }
            if (g.hours < 1 || (g.loiter && g.hours > 3)) {
                msg_set(g.loiter ? "Loiter for 1 to 3 hours." : "Rest for 1 to 99 hours.");
            } else { g.stage = REST_RUNNING; g.revision++; }
        }
    } else {
        if (click && in->cx >= 140 && in->cx <= 179 && in->cy >= 76 && in->cy <= 85) { rest_close(); return; }
        if (dungeon_nearby(cam,1024)) { msg_set("There are enemies nearby."); rest_close(); return; }
        {
            /* rest_update: a tavern room whose time has run out wakes the sleeper. */
            RestSite site;
            if (g_site && g_site(&site) && site.kind == SITE_INSIDE && site.building_type == 15 &&
                room_expired(site.building, game_state()->minutes)) {
                msg_set(room_expired_line()); rest_close(); return;
            }
        }
        g.time += in->dt;
        if (g.time >= (g.loiter ? 1.8f : 0.5f)) {
            g.time = 0; minute = game_state()->minutes; state_pass_minutes(60); g.elapsed++;
            if (character_state()->health <= 0) { rest_close(); return; }
            interrupted = 0;
            for (n = 1; n <= 60 && !interrupted; n++) {
                interrupted = encounter_minute(g_scene,cam,minute+n,g.loiter);
            }
            if (!g.loiter) { rest_recover(); }
            if (interrupted) { msg_set("Your rest is interrupted by an enemy."); rest_close(); return; }
            g.revision++;
            if ((g.until_healed && healed()) || (!g.until_healed && g.elapsed >= g.hours)) {
                msg_set(g.loiter ? "You finish loitering." : g.until_healed ? "You are healed." : "You wake up.");
                rest_close();
            }
        }
    }
}
