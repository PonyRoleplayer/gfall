#include "game/ogfall/play/cheats.h"
#include "game/ogfall/play/state.h"
#include "game/ogfall/play/vitals.h"
#include "game/ogfall/play/magic.h"
#include "engine/ui/cmd.h"

static int g_god;

int og_god_mode(void) { return g_god; }

void og_god_protect(void)
{
    OgCharacter *who;
    if (!g_god) { return; }
    who = og_character();
    who->health = who->max_health > 0 ? who->max_health : 1;
    who->fatigue = (who->attribute[OG_STR]+who->attribute[OG_END]) << 6;
    who->magicka = who->max_magicka;
}

void og_god_set(int enabled)
{
    g_god = enabled != 0;
    og_god_protect();
}

void og_cheats_startup(void)
{
    g_god = 0;
    og_magic_commands();
    if (cmd_var("tgm",og_god_mode,og_god_set,0,1,"God mode: certain hits and protected vitals.")) {
        cmd_toggles("tgm");
    }
}

int og_magicka_cost(int cost) { return g_god || cost < 0 ? 0 : cost; }

int og_magicka_spend(int cost)
{
    OgCharacter *who;
    who = og_character(); cost = og_magicka_cost(cost);
    if (who->magicka < cost) { return 0; }
    who->magicka -= cost;
    return 1;
}

void og_fatigue_spend(int amount)
{
    OgCharacter *who;
    if (g_god || amount <= 0) { return; }
    who = og_character();
    who->fatigue = og_fatigue_add(who->fatigue,-amount,who->attribute[OG_STR],who->attribute[OG_END]);
}
