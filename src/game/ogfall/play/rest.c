#include "game/ogfall/play/rest.h"
#include "game/ogfall/play/lodging.h"
#include "game/ogfall/play/law.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/dungeon.h"
#include "game/ogfall/play/move.h"
#include "game/ogfall/play/msg.h"
#include "game/ogfall/play/vitals.h"
#include "game/ogfall/play/progress.h"
#include "game/ogfall/play/encounter.h"
#include "engine/core/mem.h"

static OgRest g;
static const Scene *g_scene;
const OgRest *og_rest(void) { return &g; }
void og_rest_reset(void) { mem_zero(&g,sizeof(g)); g_scene = 0; }

static int (*g_site)(OgRestSite *out);

void og_rest_set_site(int (*site)(OgRestSite *out))
{
    g_site = site;
}

/* rest_allowed (src/lifted/rest.c 197): 1 when the player may sleep here; else the reason is said and the screen closes. */
static int may_sleep(void)
{
    OgRestSite site;
    int to_room, why;

    if (!g_site || !g_site(&site)) { return 1; }
    why = og_rest_allowed(&site, og_state()->minutes, &to_room);
    if (why == OG_REST_NO_ROOM) { og_msg_set(og_no_room_line()); }
    else if (why == OG_REST_VAGRANT) { og_msg_set(og_words(17)); }
    /* Vagrancy and trespass are crime 8 (rest_allowed notes it and calls the watch). Moving to a room needs the building's markers. */
    if (why == OG_REST_VAGRANT || why == OG_REST_TRESPASS) { og_law_note(OG_CRIME_VAGRANCY); og_guards_summon(1); }
    return why == OG_REST_OK;
}

void og_rest_close(void)
{
    if (g.open) { og_state_mode_pop(); og_progress_rest(); }
    g.open = 0; g_scene = 0; g.revision++;
}

int og_rest_open(const Scene *scene, const Camera *cam)
{
    if (g.open) { return 1; }
    if (!og_state()->ready || og_character()->health <= 0) { return 0; }
    if (og_dungeon_nearby(cam,1024)) { og_msg_set("There are enemies nearby."); return 0; }
    if (!scene || og_move_in_water() || !og_move_body()->grounded) {
        og_msg_set("You cannot sleep now."); return 0;
    }
    if (!og_state_mode_push(OG_GAME_REST)) { return 0; }
    og_rest_reset(); g.open = 1; g_scene = scene; g.revision++;
    return 1;
}

void og_rest_recover(void)
{
    OgCharacter *who;
    int rate, heal, fatigue;
    who = og_character(); rate = 60;
    if (who->health <= 0) { return; }
    /* Privateer's Hold is indoors: rapid healing in darkness or always. */
    if (og_state()->career.rapid & 6) { rate += 40; }
    rate += who->skill[0];
    heal = who->max_health*rate/1000+(who->attribute[OG_END]-50)/10;
    if (heal < 1) { heal = 1; }
    who->health += heal;
    if (who->health > who->max_health) { who->health = who->max_health; }
    fatigue = (og_state()->base_attribute[OG_STR]+og_state()->base_attribute[OG_END]) << 3;
    who->fatigue = og_fatigue_add(who->fatigue,fatigue,who->attribute[OG_STR],who->attribute[OG_END]);
    if (!(og_state()->career.flags & 8)) {
        who->magicka += who->max_magicka >> 3;
        if (who->magicka > who->max_magicka) { who->magicka = who->max_magicka; }
    }
    og_skill_use(0);
}

static int healed(void)
{
    const OgCharacter *who;
    who = og_character();
    return who->health == who->max_health &&
        who->fatigue == (who->attribute[OG_STR]+who->attribute[OG_END])*64 &&
        ((og_state()->career.flags & 8) || who->magicka == who->max_magicka);
}

void og_rest_frame(const OgIn *in, const Camera *cam)
{
    int click, n, interrupted;
    df_u32 minute;
    if (!g.open || !in) { return; }
    if (in->ch == 27) { og_rest_close(); return; }
    click = (in->mouse_pressed & 1) && in->on_picture;
    if (g.stage == OG_REST_CHOOSE) {
        if (click && in->cy >= 63 && in->cy <= 86 && in->cx >= 87 && in->cx <= 232) {
            og_sound(203);
            if (in->cx <= 183 && !may_sleep()) { og_rest_close(); return; }
            if (in->cx <= 134) { g.stage = OG_REST_HOURS; }
            else if (in->cx <= 183) { g.until_healed = 1; g.stage = OG_REST_RUNNING; }
            else { g.loiter = 1; g.stage = OG_REST_HOURS; }
            g.revision++;
        }
    } else if (g.stage == OG_REST_HOURS) {
        n = g.input[0] ? (g.input[1] ? 2 : 1) : 0;
        if (in->ch >= '0' && in->ch <= '9' && n < 2) { g.input[n] = (char)in->ch; g.input[n+1] = 0; g.revision++; }
        if (in->ch == 8 && n) { g.input[n-1] = 0; g.revision++; }
        if (in->ch == 13) {
            g.hours = 0;
            for (n = 0; g.input[n]; n++) { g.hours = g.hours*10+g.input[n]-'0'; }
            if (g.hours < 1 || (g.loiter && g.hours > 3)) {
                og_msg_set(g.loiter ? "Loiter for 1 to 3 hours." : "Rest for 1 to 99 hours.");
            } else { g.stage = OG_REST_RUNNING; g.revision++; }
        }
    } else {
        if (click && in->cx >= 140 && in->cx <= 179 && in->cy >= 76 && in->cy <= 85) { og_rest_close(); return; }
        if (og_dungeon_nearby(cam,1024)) { og_msg_set("There are enemies nearby."); og_rest_close(); return; }
        {
            /* rest_update: a tavern room whose time has run out wakes the sleeper. */
            OgRestSite site;
            if (g_site && g_site(&site) && site.kind == OG_SITE_INSIDE && site.building_type == 15 &&
                og_room_expired(site.building, og_state()->minutes)) {
                og_msg_set(og_room_expired_line()); og_rest_close(); return;
            }
        }
        g.time += in->dt;
        if (g.time >= (g.loiter ? 1.8f : 0.5f)) {
            g.time = 0; minute = og_state()->minutes; og_state_pass_minutes(60); g.elapsed++;
            if (og_character()->health <= 0) { og_rest_close(); return; }
            interrupted = 0;
            for (n = 1; n <= 60 && !interrupted; n++) {
                interrupted = og_encounter_minute(g_scene,cam,minute+n,g.loiter);
            }
            if (!g.loiter) { og_rest_recover(); }
            if (interrupted) { og_msg_set("Your rest is interrupted by an enemy."); og_rest_close(); return; }
            g.revision++;
            if ((g.until_healed && healed()) || (!g.until_healed && g.elapsed >= g.hours)) {
                og_msg_set(g.loiter ? "You finish loitering." : g.until_healed ? "You are healed." : "You wake up.");
                og_rest_close();
            }
        }
    }
}
