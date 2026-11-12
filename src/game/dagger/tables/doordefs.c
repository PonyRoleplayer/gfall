#include "game/dagger/tables/doordefs.h"
#include "game/dagger/tables/doordata.h"

const char *og_lock_text(int message)
{
    return message >= 0 && message <= OG_LOCK_OPEN ? g_lock_text[message] : "";
}

const char *og_lock_hint(int skill, int lock)
{
    int chance, index;

    if (lock <= 0) { return og_lock_text(OG_LOCK_UNLOCKED); }
    if (lock >= 20) { return og_lock_text(OG_LOCK_MAGIC); }
    chance = skill-lock*5;
    index = chance < 30 ? 0 : chance < 35 ? 1 : chance < 45 ? 2 : 3+(chance-45)/5;
    /* The original table ends here; boosted skills must not read past it. */
    if (index > 12) { index = 12; }
    return og_lock_text(index);
}
