#include "engine/core/df.h"
#include "game/dagger/tables/progressdefs.h"
#include "game/dagger/tables/progressdata.h"
int skill_advance_factor(int skill) { return skill >= 0 && skill < 35 ? g_skill_advance[skill] : 1; }
