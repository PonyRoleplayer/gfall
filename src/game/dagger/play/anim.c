#include "game/dagger/play/anim.h"
#include "game/dagger/play/state.h"
#include "engine/core/mem.h"

void anim_start(Anim *a, const AnimScript *script, int action)
{
    int entry;
    mem_zero(a,sizeof(*a));
    a->script = script;
    a->action = action;
    if (!script || action < 0 || 6+action*2+2 > script->size) { a->ended = 1; return; }
    entry = df_ru16(script->bytes+6+action*2);
    if (entry == 0x8000) { entry = df_ru16(script->bytes+6); }
    a->pc = entry;
}

int anim_tick(Anim *a)
{
    const df_u8 *s;
    int op, pc, budget, target;
    a->events = 0;
    if (a->ended || a->wait == 255 || !a->script) { return 0; }
    if (a->wait) { a->wait--; return 0; }
    s = a->script->bytes;
    for (budget = 0; budget < 64; budget++) {
        pc = a->pc;
        if (pc < 0 || pc >= a->script->size) { break; }
        op = s[pc];
        if (op >= 128) { a->frame = 255-op; a->pc++; return a->events; }
        if (pc+1+(op == 6 ? 0 : op == 9 || op == 10 || op == 11 ? 1 : op == 7 ? 3 : 2) > a->script->size) { break; }
        switch (op) {
        case 2: a->events |= df_ru16(s+pc+1); a->pc += 3; break;
        case 3:
            a->frame = s[pc+1]; a->wait = s[pc+2] ? s[pc+2] : 255;
            a->pc += 3; return a->events;
        case 4:
            target = df_ru16(s+pc+1);
            if (target >= a->script->size) { a->ended = 1; return a->events; }
            /* Combat actions return to idle/walk in the script itself. */
            if (target <= df_ru16(s+6) || (a->action == 8 && s[target] == 11)) { a->ended = 1; return a->events; }
            a->pc = target; break;
        case 6: a->ended = 1; return a->events;
        case 7:
            a->pc = random_range(0,100) < s[pc+1] ? df_ru16(s+pc+2) : pc+4; break;
        case 9: a->pc += 2; break; /* The engine chooses mirroring by view. */
        case 10: a->pc += 2; break;
        case 11: a->record = s[pc+1]; a->pc += 2; break;
        default: a->ended = 1; return a->events;
        }
    }
    a->ended = 1; /* Malformed scripts cannot hang the frame. */
    return a->events;
}
