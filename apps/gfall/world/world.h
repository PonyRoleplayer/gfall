#ifndef GFALL_APP_WORLD_H
#define GFALL_APP_WORLD_H

void gfall_world_install(void);
int gfall_creature_rate(int archive);
void gfall_release_soul(int kind);
int gfall_summon_ally(int kind);
int gfall_transform(int victim, int kind);
void gfall_transport(void);

#endif
