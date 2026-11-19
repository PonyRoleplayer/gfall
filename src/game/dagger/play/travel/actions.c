#include "game/dagger/play/travel/actions.h"
#include "game/dagger/play/character/survival.h"
#include "game/dagger/play/travel/travel.h"
#include "game/dagger/play/combat/combat.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/travel/doors.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/character/ailments.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/interact.h"
#include "engine/scene/keep.h"

#define LINK_MARK 0xa2
typedef struct Link {
    ActionInfo data;
    int active, reverse, begun;
    float elapsed, pulse;
} Link;
static Link g_link[ACTIONS];
static const Scene *g_scene;
static int g_count, g_question = -1, g_deferred = -1;
static void start_chain(int index);

int actions_accepts(int trigger, int event)
{
    trigger &= TRIGGER_MASK;
    if (trigger == TRIGGER_ANY_USE) { return event == LINK_EVENT_USE || event == LINK_EVENT_WALK_INTO || event == LINK_EVENT_STRIKE || event == LINK_EVENT_SPELL; }
    if (trigger == TRIGGER_TOUCH) { return event == LINK_EVENT_USE || event == LINK_EVENT_WALK_INTO; }
    return trigger && trigger == event;
}

static Mover *motion(const Link *link)
{
    int index;
    index = link->data.mover;
    return g_scene && index >= 0 && index < g_scene->nmovers ? &g_scene->movers[index] : 0;
}

static int is_motion(int kind) { return (kind >= ACTION_TRANSLATE && kind <= ACTION_ROTATE) || kind == ACTION_TELEPORT; }
static float duration(const Link *link)
{
    Mover *m;
    m = motion(link);
    return is_motion(link->data.kind) && m ? m->secs : link->data.duration/BIOS_HZ;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c+'a'-'A' : c; }
static int answer_matches(const char *answers, const char *answer)
{
    const char *end;
    int i, size;
    if (!answer || !*answer) { return 0; }
    while (*answers) {
        while (*answers == ' ' || *answers == '\n' || *answers == '\r' || *answers == '"' || *answers == ',') { answers++; }
        end = answers;
        while (*end && *end != ',' && *end != '"' && *end != '\n' && *end != '\r') { end++; }
        size = (int)(end-answers);
        while (size && answers[size-1] == ' ') { size--; }
        for (i = 0; i < size && answer[i] && lower(answer[i]) == lower(answers[i]); i++) { }
        if (i == size && !answer[i]) { return 1; }
        answers = *end ? end+1 : end;
    }
    return 0;
}

/* The dungeon's trespass level (action 99): 0 when none was set. */
static int g_trespass_level;
int actions_trespassing(void) { return g_trespass_level > TRESPASS_LEVEL_FREE; }

static void answer(const char *text)
{
    int next, index;
    index = g_question; next = g_deferred; g_question = g_deferred = -1;
    if (index < 0 || index >= g_count) { return; }
    g_link[index].active = 0;
    if (answer_matches(words_state(5656+g_link[index].data.sound),text)) { start_chain(next); }
    else { interact_say("That is not the answer."); }
}

static void effect(Link *link, float seconds)
{
    const ActionInfo *a;
    Mover *m;
    int damage;
    a = &link->data; m = motion(link);
    if (a->kind >= ACTION_DOOR_OPEN && a->kind <= ACTION_DOOR_LOCK && link->reverse) { link->begun = 1; return; }
    switch (a->kind) {
    case ACTION_CAST:
        link->pulse -= seconds;
        if (link->pulse <= 0) {
            Body *body;
            body = move_body(); link->pulse += 1;
            magic_trap(a->sound,m ? m->x0 : body->feet[0],m ? m->y0+TRAP_CAST_HEIGHT : body->feet[1]+TRAP_CAST_HEIGHT,
                          m ? m->z0 : body->feet[2]);
        }
        break;
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case ACTION_ROTATE: case ACTION_TELEPORT:
        if (!link->begun && m && !mover_busy(g_scene,a->mover)) { mover_start(g_scene,a->mover); }
        break;
    case 10: case 13: case 29: break;
    case ACTION_MESSAGE:
        if (!link->begun) { possessions_message(words_state(TEXT_LINK_MESSAGE+a->sound),0); }
        break;
    case ACTION_RIDDLE:
        if (!link->begun) {
            g_question = (int)(link-g_link); g_deferred = a->next;
            if (!possessions_message(words_state(TEXT_RIDDLE+a->sound),answer)) {
                g_question = g_deferred = -1;
                log_text(LOG_WARN,"GFALL: missing dungeon riddle text");
            }
        }
        break;
    case ACTION_SET_LOCK: if (m && !link->begun) { door_lock(m,a->axis); } break;
    case ACTION_DOOR_OPEN: case ACTION_DOOR_FORCE:
        if (!link->begun && m && (a->kind == ACTION_DOOR_FORCE || !m->lock) && !mover_at_end(m)) {
            if (mover_start(g_scene,a->mover) && a->kind == ACTION_DOOR_FORCE) { door_unlock(m); }
        }
        break;
    case ACTION_DOOR_UNLOCK: if (m && !link->begun) { door_unlock(m); } break;
    case ACTION_DOOR_CLOSE: case ACTION_DOOR_LOCK:
        if (!link->begun && m) {
            if (mover_at_end(m)) { mover_start(g_scene,a->mover); }
            if (a->kind == ACTION_DOOR_LOCK) { door_relock(m); }
        }
        break;
    case ACTION_HURT:
        link->pulse -= seconds;
        if (link->pulse <= 0) {
            link->pulse += 1;
            damage = random_range(a->sound,a->axis)*character_state()->level;
            damage_player(damage ? damage : character_state()->level,-1);
        }
        break;
    case 22: case 23: case 24: case ACTION_HURT_ELEMENT_LAST:
        /* link_hurt_player calls spfx_damage directly, without the spell
         * bundle's resistance roll. The element does not change this sum. */
        damage_player(a->axis*character_state()->level,-1);
        break;
    case ACTION_DRAIN_MAGICKA:
        damage = a->axis ? a->axis : 1;
        if (damage > character_state()->magicka) { damage = character_state()->magicka; }
        magicka_spend(damage);
        break;
    case ACTION_POISON: case ACTION_DISEASE:
        link->pulse -= seconds;
        if (link->pulse <= 0) {
            link->pulse += 1;
            if (a->kind == ACTION_POISON) { poison_apply(-1,random_range(POISON_FIRST,POISON_FIRST+POISONS-1),0); }
            else { disease_infect(-1,random_range(0,DISEASES-1),0); }
        }
        break;
    case ACTION_TRESPASS:
        if (!link->begun) { g_trespass_level = a->axis; }
        if (a->sound && !link->begun && m) { sound_at(a->sound,m->x0,m->y0,m->z0); }
        break;
    case ACTION_SOUND:
        if (a->sound && m) { sound_at(a->sound,m->x0,m->y0,m->z0); }
        break;
    default:
        if (!link->begun) { log_print(LOG_WARN,"GFALL: unimplemented dungeon action %d",a->kind); }
        break;
    }
    link->begun = 1;
}

static void start_chain(int index)
{
    int steps;
    Link *link;
    for (steps = 0; index >= 0 && index < g_count && steps < g_count; steps++) {
        link = &g_link[index];
        if (link->active) { return; }
        link->active = 1; link->begun = 0; link->elapsed = link->pulse = 0;
        effect(link,0);
        if (link->data.kind == ACTION_RIDDLE) { return; }
        index = link->data.next;
    }
}

int actions_trigger(const Scene *scene, int model, int event)
{
    int i, found;
    if (scene != g_scene) { return 0; }
    found = 0;
    for (i = 0; i < g_count; i++) {
        if (g_link[i].data.model == model && actions_accepts(g_link[i].data.trigger,event)) {
            if (!g_link[i].active) { start_chain(i); found = 1; }
        }
    }
    return found;
}

static int use(const Scene *scene, const Usable *usable, int index)
{
    const ActionInfo *action;
    int event;
    (void)index;
    if (travel_passage(usable)) { return 1; }
    if (scene != g_scene || usable->what != USE_ACTION || usable->tag < 0 || usable->tag >= g_count) { return 0; }
    action = &g_link[usable->tag].data;
    event = interact_using_touch() ? (action->trigger == LINK_EVENT_STAND ? LINK_EVENT_STAND : LINK_EVENT_WALK_INTO) : action->trigger == LINK_EVENT_SPELL ? LINK_EVENT_SPELL : LINK_EVENT_USE;
    actions_trigger(scene,action->model,event);
    return 1;
}

void actions_close(void)
{
    g_count = 0; g_scene = 0; g_question = g_deferred = -1; g_trespass_level = 0;
    interact_set_handler(0);
}

int actions_open(const Scene *scene, const ActionInfo *actions, int count)
{
    const Keep *row;
    Mover *m;
    Link *link;
    Usable *usable;
    int i, j;

    actions_close();
    if (!scene || count < 0 || count > ACTIONS) { return 0; }
    g_scene = scene; g_count = count;
    mem_zero(g_link,sizeof(g_link));
    for (i = 0; i < count; i++) {
        link = &g_link[i]; link->data = actions[i]; m = motion(link);
        if (m) {
            m->next = -1;
            if (!m->door && is_motion(link->data.kind)) {
                m->secs = (link->data.kind >= 2 && link->data.kind <= 7 ? 50 : link->data.duration)/BIOS_HZ;
            }
        }
        row = keep_find(link->data.key);
        if (row && row->kind == KEEP_MOVER && row->game[8] == LINK_MARK) {
            link->active = row->game[9] & 1; link->reverse = (row->game[9] >> 1) & 1;
            link->begun = (row->game[9] >> 2) & 1;
            link->elapsed = df_ru16(row->game+10)/BIOS_HZ;
            link->pulse = df_ru16(row->game+12)/1000.0f;
            if (link->active && is_motion(link->data.kind) && m && !m->teleport) {
                mover_restore_motion(m,df_ru16(row->game+14)/65535.0f,link->reverse ? -1 : 1);
            }
        }
        for (j = 0; j < scene->nusables; j++) {
            usable = &((Usable *)scene->usables)[j];
            if (usable->model == link->data.model && (!m || !m->door)) {
                usable->kind = USE_OTHER; usable->what = USE_ACTION; usable->tag = i;
            }
        }
    }
    interact_set_handler(use);
    return 1;
}

void actions_step(float seconds)
{
    Link *link;
    int i;
    if (seconds <= 0 || !g_scene || g_question >= 0) { return; }
    for (i = 0; i < g_count && character_state()->health > 0; i++) {
        link = &g_link[i];
        if (!link->active) { continue; }
        if (link->elapsed >= duration(link)) {
            link->active = 0;
            if (!(link->data.kind >= 16 && link->data.kind <= 20)) { link->reverse ^= 1; }
            continue;
        }
        effect(link,seconds); link->elapsed += seconds;
    }
}

void actions_capture(void)
{
    Keep *row;
    Link *link;
    Mover *m;
    int i, ticks, pulse, progress;
    for (i = 0; i < g_count; i++) {
        link = &g_link[i]; m = motion(link);
        row = keep_find(link->data.key);
        if (!link->active && !link->reverse && (!row || row->game[8] != LINK_MARK)) { continue; }
        row = keep_get(link->data.key,KEEP_MOVER);
        if (!row) { continue; }
        if (m) { row->lock = m->lock; row->at_end = (df_u8)(!m->teleport && mover_at_end(m)); }
        row->game[8] = LINK_MARK;
        row->game[9] = (df_u8)(link->active | (link->reverse << 1) | (link->begun << 2));
        ticks = df_iround(link->elapsed*BIOS_HZ); if (ticks > 65535) { ticks = 65535; }
        pulse = df_iround(link->pulse*1000); if (pulse < 0) { pulse = 0; }
        row->game[10] = (df_u8)ticks; row->game[11] = (df_u8)(ticks >> 8);
        row->game[12] = (df_u8)pulse; row->game[13] = (df_u8)(pulse >> 8);
        progress = m ? df_iround(mover_progress(m)*65535) : 0;
        row->game[14] = (df_u8)progress; row->game[15] = (df_u8)(progress >> 8);
    }
}

void actions_strike(const Scene *scene, const Camera *camera, int event)
{
    UseHit hit;
    Basis basis;
    if (scene != g_scene || !camera) { return; }
    camera_basis(camera,&basis);
    interact_cast(scene,camera->x,camera->y,camera->z,basis.fx*basis.cp,basis.sp,basis.fz*basis.cp,STRIKE_MODEL_REACH,&hit);
    if (hit.model >= 0 && hit.wall <= STRIKE_MODEL_REACH) { actions_trigger(scene,hit.model,event); }
}
