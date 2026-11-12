#include "game/dagger/play/cheats.h"
#include "game/dagger/play/state.h"
#include "game/dagger/play/vitals.h"
#include "game/dagger/play/magic.h"
#include "engine/ui/cmd.h"

static int g_god;

int god_mode(void) { return g_god; }

void god_protect(void)
{
    CharacterState *who;
    if (!g_god) { return; }
    who = character_state();
    who->health = who->max_health > 0 ? who->max_health : 1;
    who->fatigue = (who->attribute[ATTR_STR]+who->attribute[ATTR_END]) << 6;
    who->magicka = who->max_magicka;
}

void god_set(int enabled)
{
    g_god = enabled != 0;
    god_protect();
}

void cheats_startup(void)
{
    g_god = 0;
    magic_commands();
    if (cmd_var("tgm",god_mode,god_set,0,1,"God mode: certain hits and protected vitals.")) {
        cmd_toggles("tgm");
    }
}

int magicka_cost(int cost) { return g_god || cost < 0 ? 0 : cost; }

int magicka_spend(int cost)
{
    CharacterState *who;
    who = character_state(); cost = magicka_cost(cost);
    if (who->magicka < cost) { return 0; }
    who->magicka -= cost;
    return 1;
}

void fatigue_spend(int amount)
{
    CharacterState *who;
    if (g_god || amount <= 0) { return; }
    who = character_state();
    who->fatigue = fatigue_add(who->fatigue,-amount,who->attribute[ATTR_STR],who->attribute[ATTR_END]);
}
