# Rule numbers still written inline in play/ (inventory for DOCUMENTER)

Generated 2026-11-17 from `main` after the first cleanup (docs/magic-numbers-gap.md). Each line has a numeric literal other than 0, 1, 2 that is not a screen coordinate. Not every line is a rule: loop bounds, array sizes, text-record ids and picture frames are in here too, and DOCUMENTER should mark them as such.

## combat/dungeon.c (146 lines)
51: enum { FOE_AWAKE = 1, FOE_PASSIVE = 2, FOE_ALLIED = 4, FOE_MARKER_ROLLED = 8 };
55: static SpawnId g_splash[8];
56: static float g_splash_time[8];
63: for (k = 0; k < 4; k++) { p[k] = (df_u8)(value >> (k*8)); }
73: row->game[0] = 0xc1;
77: row->game[3] = (df_u8)(f->mind.direction | (f->mind.turns << 4));
78: write32(row->game+4,(df_u32)f->fighter.health);
79: write32(row->game+8,(df_u32)f->fighter.maximum);
80: row->game[12] = (df_u8)f->mind.give_up;
81: row->game[13] = (df_u8)f->mind.stuck;
82: row->game[14] = (df_u8)f->mind.detected;
83: row->game[15] = (df_u8)f->mind.encountered;
101: f->mind.give_up = 200; store_health(f);
122: if (i >= 0 && i < g_count && type >= 0 && type < 60) {
138: dx = e->x-cam->x; dy = e->y+40-cam->y; dz = e->z-cam->z;
142: float from[3], to[3];
143: from[0] = e->x; from[1] = e->y+40; from[2] = e->z;
171: for (i = 0; i < 8; i++) { if (g_splash[i]) { spawn_remove(g_splash[i]); } }
208: f->kind = kind; f->passive = passive; f->rate = rate > 0 ? rate : 100;
212: f->voice_time = 1+(float)(f->entity->key.thing%4000)/1000;
218: skill = f->fighter.level*5+30; if (skill > 100) { skill = 100; }
220: for (j = 0; j < ATTRS; j++) { f->fighter.attribute[j] = f->def ? f->def->attribute[j] : career ? career->attribute[j] : 50; }
221: for (j = 0; j < 7; j++) { f->fighter.armor[j] = f->def ? f->def->armor : 60; }
225: f->fighter.magicka = f->fighter.max_magicka = f->fighter.level*10+100;
226: for (j = 22; j <= 27; j++) { f->fighter.skill[j] = 80; }
230: if (row && row->game[0] == 0xc1) {
231: f->fighter.health = (int)df_ru32(row->game+4); f->fighter.maximum = (int)df_ru32(row->game+8);
236: f->mind.blocked = row->game[2]; f->mind.direction = row->game[3] & 15;
237: f->mind.turns = row->game[3] >> 4;
238: f->mind.give_up = row->game[12]; f->mind.stuck = row->game[13];
239: f->mind.detected = row->game[14]; f->mind.encountered = row->game[15];
243: for (j = 0; j < dice; j++) { health += random_range(1,f->def ? 8 : career ? career->hp_per_level : 10); }
251: body_init(&f->body,12,80,24,60);
252: f->body.radius = f->entity->radius > 0 ? f->entity->radius : 12;
253: f->body.height = 80; f->body.eye = 60; f->body.step = 24; f->body.self = f->entity;
269: passive ? passive[i] : 0,rates ? rates[i] : 100);
292: if (height < 24) { height = 24; }
293: if (height > 128) { height = 128; }
296: f->body.step = minimum(24,height/4);
351: float start, stretch, a[3], b[3], origin[3], at;
357: for (k = 0; k < 3; k++) { a[k] = from[k]+dir[k]*start; b[k] = a[k]+dir[k]*stretch; origin[k] = a[k]; }
372: return feet+(cam->y-feet)*0.5f;
377: float dir[3], length, hit;
380: for (k = 0; k < 3; k++) { dir[k] = to[k]-from[k]; length += dir[k]*dir[k]; }
383: for (k = 0; k < 3; k++) { dir[k] /= length; }
392: for (i = 0; i < 8; i++) {
394: entity_init(&e); e.live = 1; e.x = target->x; e.y = target->y+40; e.z = target->z;
396: e.pose[ENTITY_IDLE].w[0] = e.pose[ENTITY_IDLE].h[0] = 40;
397: e.pose[ENTITY_IDLE].rate = 12;
398: g_splash[i] = spawn_add(scene,&e,0); g_splash_time[i] = 0.3f;
414: f->mind.detected = f->mind.encountered = 1; f->mind.give_up = 200;
416: splash(scene,e,f->kind == 7 || f->kind == 8);
421: sound_at(17,e->x,e->y,e->z);
424: if ((e->key.thing & 0xe0000000u) == QUEST_FOE_KEY) { quest_foe_killed((int)((e->key.thing >> 4) & 0xffffffu)); }
427: if (source == TARGET_PLAYER && f->kind == ENEMY_CITY_WATCH) { guild_count_crime(6, 1, game_state()->minutes); }
431: force = damage*10;
432: absorbed = (damage*2*((force-weight)*256/(force+weight)))/256;
434: if (f->knock < 15) { f->knock = 15; }
437: f->hurt = 0.25f; e->act = ENTITY_HURT; e->hold = 1; e->frame = 0;
524: random_range(0,32767); return random_range(291,299);
537: part = struck_part(random_range(0,19));
542: backstab = !s->bow && df_cos(toward) < df_cos(640*DF_TAU/2048) ? a.skill[SKILL_BACKSTABBING] : 0;
548: sound_at(miss_sound(index,f->kind),f->entity->x,f->entity->y+40,f->entity->z); return;
551: part,s->hit,random_range(1,100));
553: if (!god_mode() && random_range(0,100) > chance) {
554: sound_at(miss_sound(index,f->kind),f->entity->x,f->entity->y+40,f->entity->z);
558: random_range(a.skill[SKILL_HAND_TO_HAND]/10+1,a.skill[SKILL_HAND_TO_HAND]/5+1);
563: if (backstab && random_range(1,100) <= backstab) { damage *= 3; msg_set("Backstab!"); }
565: if (damage && (f->entity->key.thing & 0xe0000000u) == QUEST_FOE_KEY) { quest_foe_hit((int)((f->entity->key.thing >> 4) & 0xffffffu)); }
569: sound_at(random_range(378,382),f->entity->x,f->entity->y+40,f->entity->z); item_wear(s->item,damage);
595: float dx, dz, distance, point[3], from[3], facing;
609: if (distance > 90 || df_absf(e->y-move_body()->feet[1]) > 90) { continue; }
611: if (distance > 10 && facing <= df_cos(200*DF_TAU/2048)) { continue; }
612: point[0] = e->x; point[1] = e->y+40; point[2] = e->z;
620: actions_strike(scene,cam,5);
634: if (o->count < 199) { object_edit(o->id)->count++; }
647: float wall, length, nearest, along, dx, dy, dz, r, closest, centre[3];
652: length = 2400*dt; if (length > 2048-a->distance) { length = 2048-a->distance; }
658: dx = cam->x-a->at[0]; dy = chest(cam)-a->at[1]; dz = cam->z-a->at[2]; r = 30;
661: dx = e->x-a->at[0]; dy = e->y+40-a->at[1]; dz = e->z-a->at[2];
662: r = e->radius > 20 ? e->radius : 20; r += 20;
674: for (k = 0; k < 3; k++) { a->at[k] += a->direction[k]*nearest; }
677: sound_at(7,a->at[0],a->at[1],a->at[2]);
692: part = struck_part(random_range(0,19));
693: chance = attack_chance(&archer->fighter,victim,archer->fighter.skill[SKILL_ARCHERY],16,0,part,0,random_range(1,100));
695: if (random_range(0,100) <= chance) {
696: damage = random_range(weapon_damage(16,0),weapon_damage(16,1));
706: } else if (blocked || a->distance >= 2048) {
712: if (hit.model >= 0) { actions_trigger(scene,hit.model,6); }
725: static void target_point(const Camera *cam, int target, float point[3])
731: point[1] = entity ? entity->y+40 : chest(cam);
744: float from[3], to[3], dx, dy, dz, distance;
748: from[0] = foe->entity->x; from[1] = foe->entity->y+40; from[2] = foe->entity->z;
759: if (distance > 2048) { continue; }
760: score = 30-(int)distance/128;
761: if (dungeon_target(candidate) == TARGET_NONE) { score += 5; }
762: if (score+20 <= best_score) { continue; }
763: if (visible(scene,from,to)) { score += 20; }
774: float from[3], to[3], dx, dy, dz, distance, yaw;
779: from[0] = foe->entity->x; from[1] = foe->entity->y+40; from[2] = foe->entity->z;
782: if (distance > 90*90) { return; }
784: if (distance > 100 && df_cos(yaw-foe->entity->yaw) <= df_cos(200*DF_TAU/2048)) { return; }
794: part = struck_part(random_range(0,19)); total = 0;
798: for (i = 0; i < (index < 0 && foe->def ? 5 : 1); i++) {
799: if (index < 0 && foe->def && (!foe->def->minimum[i] || random_range(0,99) >= 50-(character_state()->reflexes-2)*10)) { continue; }
801: index >= 0 ? material_hit(material) : 0,part,0,random_range(1,100));
803: if (random_range(0,100) > chance || (index >= 0 && material < victim->metal)) { continue; }
806: random_range(foe->fighter.skill[SKILL_HAND_TO_HAND]/10+1,foe->fighter.skill[SKILL_HAND_TO_HAND]/5+1);
830: sound_at(total ? random_range(380,381) : 374,from[0],from[1],from[2]);
878: float dx, dz, distance, from[3], to[3], speed, reach;
889: if (distance > 2048) { return; }
893: if (distance < 1024 && f->voice_time <= 0) {
894: foe_voice(f,f->awake ? (distance < 128 && random_range(0,3) ? 2 : 1) : 0);
895: f->voice_time = (float)random_range(2000,6000)/1000;
903: from[0] = e->x; from[1] = e->y+40; from[2] = e->z;
907: f->retarget = 0.45f+(float)(index & 7)*0.04f;
917: reach = f->target == TARGET_PLAYER ? 90 : 60;
919: angle = df_iround(df_atan2(dx,dz)*(2048/DF_TAU)) & 2047;
920: facing = df_iround(e->yaw*(2048/DF_TAU)) & 2047;
923: f->think = 0.2f;
924: if (sight && (f->target != TARGET_PLAYER || (magic_sees(f->kind) && (ai_angle(facing,angle) <= 512 ||
927: f->mind.give_up = 200; store_health(f);
933: f->mind.give_up -= dt*60;
935: } else if (f->target == TARGET_PLAYER && (!magic_sees(f->kind) || ((!sight || ai_angle(facing,angle) > 512) &&
938: } else { f->mind.detected = 1; f->mind.give_up = 200; }
944: if (f->knock > 40) { f->knock = 40; }
945: speed = f->knock > 40 ? 40 : f->knock;
946: speed = (speed > 25 ? 25 : speed)*dt*60;
949: f->knock -= dt*300;
953: f->anim.time += dt*1000;
959: shot.x = e->x; shot.y = e->y+50; shot.z = e->z;
961: sound_at(6,shot.x,shot.y,shot.z); launch_arrow(&shot,index,0,16,0);
964: pose = f->anim.record == 25 ? ENTITY_SHOOT : f->anim.record == 20 ? ENTITY_CAST : ENTITY_ATTACK;
968: if (f->attack_wait <= 0 && sight && ai_angle(facing,angle) < 128 &&
970: anim_start(&f->anim,f->script,32); f->attack_wait = 0.5f;
973: ((foe_type_flags(f->type) & 32) && distance > 4*reach))) {
974: if (sight && ai_angle(facing,angle) < 128) {
975: anim_start(&f->anim,f->script,distance > 4*reach ? 24 : 8);
978: f->attack_wait = (float)(random_range(1500,3000)-(character_state()->level-10)*50-900)/1000;
979: if (f->attack_wait < 0.1f) { f->attack_wait = 1.5f; }
981: e->yaw = ai_turn(facing,angle,df_iround(dt*60*64))*(DF_TAU/2048);
983: } else if (distance > reach-20) {
991: e->yaw = ai_turn(facing,angle,df_iround(dt*60*64))*(DF_TAU/2048);
1004: if (dt > 0.1f) { dt = 0.1f; }
1008: for (i = 0; i < 8; i++) {
1071: f->mind.give_up = 600;

## combat/ai.c (30 lines)
16: if (distance > 1024) { return 0; }
24: chance = ((character_state()->skill[SKILL_STEALTH]*distance)/1024)*2;
25: return random_range(1,100) > chance;
32: enum { COMPREHEND_LANGUAGES = 44 };
38: chance = (hud_weapon_ready() ? -25 : 10) + who->attribute[ATTR_PER]/5;
39: if (ENEMY_IS_CLASS(kind)) { chance += who->skill[SKILL_ETIQUETTE]/10; }
52: roll = random_range(1,200);
61: return dx < 0 ? 7-octant : octant;
66: return ((to-from+1024) & 2047)-1024;
80: return (from+difference) & 2047;
88: x = body->feet[0]; z = body->feet[2]; angle = yaw*(DF_TAU/2048);
96: return moved > 2*dt*60;
107: angle = df_iround(df_atan2(dx,dz)*(2048/DF_TAU)) & 2047;
108: yaw = df_iround(entity->yaw*(2048/DF_TAU)) & 2047;
109: mind->clock += dt*60; ticks = df_ifloor(mind->clock); mind->clock -= ticks;
112: if (amount > distance-60) { amount = distance > 60 ? distance-60 : 0; }
115: mind->turns = 12;
116: if (difference >= 32) { yaw = ai_turn(yaw,angle,64*ticks); amount = 0; }
118: if (body->grounded && (mind->stuck += ticks) > 6) { mind->blocked |= 1; mind->stuck = 0; }
120: entity->yaw = yaw*(DF_TAU/2048);
126: if (mind->direction == 4) { nav += 1024; }
127: else if (mind->direction == 8) { nav += 1536; }
128: nav &= 2047;
130: if ((mind->blocked & 1) && difference <= 256 && !flying) {
131: quadrant = ((angle+256)/512) & 3;
133: offset = across-df_ifloor(across/64)*64;
136: offset = offset < 8 ? -offset : offset < 32 ? -8 : offset > 56 ? 64-offset : 8;
142: yaw = ai_turn(yaw,nav,64*ticks); entity->yaw = yaw*(DF_TAU/2048);
143: if (ai_angle(yaw,nav) >= 64) { move(body,scene,yaw,flying,target,0,dt); return; }
147: if (mind->direction & 16) { mind->direction = 2; mind->blocked &= ~1; }

## combat/encounter.c (36 lines)
33: roll = random_range(1,100);
34: if (roll <= 80) { low = level-3; high = level+3; }
35: else if (roll <= 95) { low = 0; high = level+1; }
36: else { low = 0; high = level > 5 ? 19 : level+2; }
37: if (low < 0) { low = 0; high = 5; }
38: if (high > 19) { low = 14; high = 19; }
49: saved_seed = random_range(0,32767);
53: for (i = 0; i < 256; i++) {
55: if (i >= 1 && i <= 6) { state->choices[i-1] = kind; }
66: return (df_u32)(kind & 255) | ((df_u32)(archive & 4095) << 8) | ((df_u32)(rate & 4095) << 20);
115: kind = (int)(row->owner & 255u); archive = (int)((row->owner >> 8) & 4095u); rate = (int)((row->owner >> 20) & 4095u);
127: kind = (int)(e->owner & 255u); archive = (int)((e->owner >> 8) & 4095u); rate = (int)((e->owner >> 20) & 4095u);
171: if (dx*dx+dz*dz > 3072.0f*3072.0f) { outdoor_retire(&g_outdoor_actor[i]); }
238: if (at) { want.inner = 0; want.outer = 192; want.climb = 256; }
239: else if (summoned) { want.inner = 96; want.outer = 300; want.climb = 128; }
240: else { want.inner = 384; want.outer = 768; want.climb = 384; }
241: want.radius = 12; want.height = entity.pose[ENTITY_WALK].h[0];
242: if (want.height < 24) { want.height = 24; }
243: if (want.height > 128) { want.height = 128; }
244: want.flags = (at || summoned ? 0 : SPAWN_UNSEEN) | ((foe_flags(kind) & 65) ? 0 : SPAWN_DRY);
259: quest_foe_pacified((int)((thing >> 4) & 0xffffffu))) { dungeon_set_pacified(rate,1); }
266: return place_actor(camera, encounter_state()->choices[random_range(0,5)], 0, 0, 1, 0, TEAM_HOSTILE);
273: float at[3];
293: static const df_u8 base[] = {20,20,23,26,3,29,32,32,35};
318: spot.radius = 12; spot.height = entity.pose[ENTITY_WALK].h[0];
319: if (spot.height < 24) { spot.height = 24; }
320: if (spot.height > 128) { spot.height = 128; }
321: spot.flags = (foe_flags(kind) & 65) ? 0 : SPAWN_DRY;
323: spot.inner = 384; spot.outer = 768; spot.climb = 384;
326: spot.x += 1024*df_sin(camera->yaw); spot.z += 1024*df_cos(camera->yaw);
327: spot.outer = 128; spot.climb = 1536;
353: if (!g_scene || scene != g_scene || !camera || activity == ENCOUNTER_LOITER || minute%12) { return 0; }
355: return activity == ENCOUNTER_REST && random_range(0,35) == 0 && encounter_try(camera);
363: if (table < 0 || random_range(0,night ? 23 : 35) != 0) { return 0; }
364: for (tries = 0; tries < 32; tries++) {
366: if (night || (kind != 18 && kind != 23)) { return outdoor_spawn(camera,kind,settlement); }

## combat/foegear.c (18 lines)
12: if (random_range(1,100) > chance) { return; }
15: if (id) { object_edit(id)->flags |= FOE_SLOT | (slot << 8); }
23: static const int gear_slot[6] = {EQUIP_HEAD,EQUIP_RIGHT_PAULDRON,EQUIP_LEFT_PAULDRON,EQUIP_CUIRASS,EQUIP_GREAVES,EQUIP_BOOTS};
24: static const int armor_index[6] = {5,4,3,0,2,6};
25: if (ENEMY_IS_CLASS(kind)) { tier = 3; }
33: if (tier == 3) { tier = random_range(0,1); }
34: make(container,3,tier ? 9 : 5,tier ? 14 : 7,-1,100,EQUIP_RIGHT_HAND);
36: make(container,2,7,8,-1,50,EQUIP_LEFT_HAND);
41: ((item->flags >> 8) & 31) == 21) { equipped = 1; }
43: if (!equipped) { make(container,3,0,3,2,50,EQUIP_LEFT_HAND); }
45: for (i = 0; i < 6; i++) {
46: make(container,2,armor_index[i],armor_index[i],-1,tier == 0 ? 50 : tier == 1 ? 75 : 90,gear_slot[i]);
54: part = (item->flags >> 8) & 31;
58: shield = object_at(slots[21]);
59: for (part = 0; part < 7; part++) {
60: armor = 100; item = object_at(slots[armor_slot(part)]);
65: if (shield && shield->group == 2 && shield_covers(shield->index,part)) { armor -= (shield->index-6)*5; }
66: fighter->armor[part] = armor > 50 ? 60 : armor < -100 ? -100 : armor;

## combat/weapon.c (19 lines)
21: int weapon_delay(int speed) { int n; n = (115-speed)*3; return n < 1 ? 1 : n; }
22: int bow_delay(int speed) { int n; n = (100-speed)*10+1000; return n < 200 ? 200 : n; }
57: angle = (int)(df_atan2((float)dx,(float)dy)*2048/DF_TAU);
58: return (angle & 2047) >> 7;
96: fatigue_spend(11);
105: static const int kick[] = {0,1,2,3,4,2,1,0};
114: g_view.cif = curse_beast() ? 11 : weapon_cif(item ? item->index : -1);
124: if (!arrow_count() && g_step < 4) {
129: if (g_step == 3) {
131: play_sound(6); g_step = 4; g_time = 0;
133: } else if (g_time >= 1.0f/60) {
135: if (g_step == 5 && arrow_take()) { strike_at(strike); }
136: if (g_step >= 7) {
137: g_reload = (float)bow_delay(character_state()->attribute[ATTR_SPD])/1000;
147: dx = in->mouse_dx*320/df_view_w(); dy = in->mouse_dy*320/df_view_w();
150: if (motion+small/2 < 45) { return; }
158: delay = (float)weapon_delay(character_state()->attribute[ATTR_SPD])/1000;
162: if (g_step == ((!g_item && swing_group(g_sector) == 2) ? 4 : 2)) { strike_at(strike); }
163: if (g_step >= ((!g_item && swing_group(g_sector) == 2) ? 8 : 5)) {

## magic/enchant.c (53 lines)
27: for (k = 0; item_enchantment(game_state()->equipped[slot], k, &type, &param); k++) { if (type == 9) { return 1; } }
46: else if (type == 5) { g_stamp = game_state()->minutes; }
47: else if (type == 9) { character_state()->conditions |= 0x200; }
48: else if (type == 10 && param >= 0 && param < SKILLS) { character_state()->skill[param] += 15; }
60: else if (type == 9 && !others_absorb(slot)) { character_state()->conditions &= ~COND_ABSORPTION; }
61: else if (type == 10 && param >= 0 && param < SKILLS) { character_state()->skill[param] -= 15; }
62: else if (type == 3) { g_rebuild = 1; }
78: char name[48], line[80];
79: int enchanted, k, type, param, souls[10], count;
92: if (type == 15 && param >= 0) { souls[count++] = param; }
116: static const int season[12] = {0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 0, 0};
123: if (param >= 0 && param <= 3) { return season[month % 12] == param ? 75 : 0; }
124: if (param == 4) { return moon0 == 0 || moon1 == 0 ? 75 : 0; }
125: if (param == 5) { return moon0 == 8 || moon0 == 24 || moon1 == 8 || moon1 == 24 ? 75 : 0; }
126: if (param == 6) { return moon0 == 16 || moon1 == 16 ? 75 : 0; }
127: if (param >= 7 && param <= 10) { return g_world && g_world->category_near && g_world->category_near(param - 7, 1024) ? 75 : 0; }
141: if (!item || item->type != RECORD_ITEM || (item->group == ITEM_GROUP_WEAPON && item->index == 17) || item_enchanted(item->id)) { continue; }
156: else if (type == 3) {
158: if (param >= 7 && param <= 10 && extra_points(param) && who->magicka < who->max_magicka) { who->magicka += amount * 5; }
159: } else if (type == 5) {
163: else if (random_range(0, 9) == 0) { item_damage(id, 1); }
164: } else if (type == 6 && param == 0) {
170: } else if (type == 8) {
172: if (random_range(0, 9) == 0) { item_damage(id, 1); }
173: } else if (type == 16) {
176: } else if (type == 17) {
178: } else if (type == 21 && (param == 1 || param == 2)) {
192: if (elapsed > 100) { elapsed = 0; g_stamp = now; }
197: if (((game_state()->career.flags & CLASS_SUN_DAMAGE) && daylight()) || ((game_state()->career.flags & CLASS_HOLY_DAMAGE) && temple())) { damage_player(amount * 12, 0); }
215: if (type == 3) { any = 1; bonus += extra_points(param); }
231: if (item_enchantment(id,0,&type,&param) && type == 26) {
232: if (param == 3) {
233: if (!magic_cast_item(92)) { return 0; }
234: item_damage(id,50);
237: if (param == 4 || param == 8) {
246: kind = param == 4 ? 27 : dungeon_kind(index);
251: item_damage(id,100);
257: if (magic_cast_item(param)) { used++; item_damage(id, 10); }
258: } else if (type == 21 && param == 0) {
272: if (type == 4 && param == category) { bonus += level; }
273: else if (type == 20 && param == category) { bonus -= level; }
274: else if (type == 21 && param == 0) {
297: item_damage(weapon, 10);
298: } else if (type == 6 && param == 1) {
306: item_damage(weapon, healed / 4 + 1);
325: if (!item_enchantment(item,0,&type,&param) || type != 26 || param != 7) { continue; }
339: if (type == 12) { return -25; }
340: if (type == 24) { return 25; }
352: if (type == 7) { return param == 0 ? capacity * 320 / 256 : capacity * 384 / 256; }
363: if (type == 11) { return 1; }
364: if (type == 23) { return weight * 4 < 20 ? 20 : weight * 4; }
381: if (type == 14 && (param == 5 || param == social)) { mod += 10; }
382: else if (type == 25 && (param == 5 || param == social)) { mod -= 10; }

## magic/magic.c (104 lines)
37: if (index == -3) { key.place = MAGIC_EPHEMERAL_PLACE; key.thing = MAGIC_TRAP_THING; }
49: if (key.place == MAGIC_EPHEMERAL_PLACE && key.thing == MAGIC_TRAP_THING) { return -3; }
70: else if (same(magic->effect[i].caster,key)) { magic->effect[i].caster = magic_key(-3); }
76: if (magic->missile[i].live && same(magic->missile[i].caster,key)) { magic->missile[i].caster = magic_key(-3); }
89: else if (ephemeral_key(magic->effect[i].caster)) { magic->effect[i].caster = magic_key(-3); }
101: if (index == -3) {
105: for (i = 0; i < ATTRS; i++) { who->attribute[i] = base[i] = 50; }
106: for (i = 0; i < SKILLS; i++) { who->skill[i] = 50; }
162: if (formula == 1) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]+c[2]*ch[0]+divide(ch[1],ch[2])*c[3]; }
163: else if (formula == 2) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]+c[2]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[3]; }
164: else if (formula == 3) { cost = c[0]*d[0]+divide(d[1],d[2])*c[1]; }
165: else if (formula == 4) { cost = c[0]*ch[0]+divide(ch[1],ch[2])*c[1]; }
166: else if (formula == 5) { cost = c[0]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[1]; }
167: else if (formula == 6) { cost = c[2]*((m[0]+m[1])/2)+((m[2]+m[3])/2)*divide(c[3],m[4])+c[0]*d[0]+divide(d[1]*c[1],d[2]); }
168: else if (formula == 7) { cost = divide((df_i16)(c[0]*((m[0]+m[1])/2)+divide((m[2]+m[3])/2,m[4])*c[1])*d[0],d[1]); }
181: total += ((SPELL_COST_SKILL_CEILING-skill)*effect_cost_raw(s->group[k],c,s->duration[k],s->chance[k],s->magnitude[k]))/100;
200: int base[8], chance, roll, race;
202: if ((who.conditions & resist_flag(element)) && random_range(1,100) <= magic_chance(index,GROUP_ELEMENTAL_RESISTANCE,element)) { return 0; }
205: if (career.weakness & bit) { return 100; }
213: roll = random_range(1,100);
214: if (roll > chance) { return 100; }
224: if (group == 39) { return COND_GROUP39; }
225: if (group == 42) { return COND_GROUP42; }
226: return group >= GROUP_COMPREHEND_LANGUAGES && group <= 46 ? COND_LATE_GROUP(group) : 0;
232: int base[8], index;
236: if (e->group == GROUP_FORTIFY && e->sub < 8) { who.attribute[e->sub] -= e->change; }
264: return random_range(1,100) <= (conditions & COND_SHADOW ? SEE_SHADOW_CHANCE : SEE_CHANCE);
274: if (value->icon < 200 && e->group == value->group && e->sub == value->sub && same(e->target,value->target)) {
275: e->rounds = (e->rounds+value->rounds) & 65535; return 0;
289: if (sub < 8) { who->attribute[sub] += magnitude; if (who->attribute[sub] > base[sub]) { who->attribute[sub] = base[sub]; } }
290: else if (sub == 8) { who->health += magnitude; }
291: else if (sub == 9) { who->fatigue += magnitude*FATIGUE_UNIT; }
292: else if (sub == 10) { who->magicka += magnitude; }
303: while (*answer >= '0' && *answer <= '9' && row < EFFECTS+1) { row = row*10+*answer++-'0'; }
307: chance = g_dispel_chance+(character_state()->level-e->level)*5;
308: if (chance < 5) { chance = 5; } if (chance > 95) { chance = 95; }
309: if ((!e->caster.place && !e->caster.thing) || random_range(1,100) <= chance) {
321: char text[2048], line[96];
326: for (i = 0; i < EFFECTS && used < 1850; i++) {
347: int base[8], other_base[8], group, sub, before, chance;
351: if ((index == -1 ? curse_race() : who.race) == PLAYER_RACE_HIGH_ELF || (index == -1 && (career.immunity & EFFECT_BIT_PARALYSIS)) || (who.conditions & (COND_FREE_ACTION | 
356: if ((group == GROUP_CURE || group == GROUP_DISINTEGRATE || group == GROUP_SILENCE || group == GROUP_CALM || group == GROUP_CHARM) && random_range(1,100) > e->chance) { e-
372: level = e->level; chance = e->chance+(level-victim.level)*5;
373: if (chance < 5) { chance = 5; } if (chance > 95) { chance = 95; }
374: if (random_range(1,100) <= chance) { victim.health = 0; magic_commit(i,&victim,caster); }
380: if (sub < 8) { who.attribute[sub] -= before; if (who.attribute[sub] < 1) { who.attribute[sub] = 1; } }
381: else if (sub == 8) { who.health -= before; }
382: else if (sub == 9) { who.fatigue -= before; if (who.fatigue < 1) { who.fatigue = 1; } }
386: } else if (group == GROUP_FORTIFY && sub < 8) {
388: if (who.attribute[sub] > 100) { who.attribute[sub] = 100; }
419: gem = magic_artifact_worn(9);
422: if (soul_trap_empty(gem) && object_new(RECORD_SOUL,gem,kind,100)) {
434: !((item->group == ITEM_GROUP_MISC && item->index == MISC_SOUL_TRAP) || magic_artifact_power(item->id) == 9) ||
443: if (!star_worn && random_range(0,100) > e->chance) { return; }
444: soul = object_new(RECORD_SOUL,gem,kind,100);
455: int base[8], k, flags, percent, rounds;
458: if (s->icon >= 200) { level = 8; }
461: if ((to.conditions & COND_ABSORPTION) || (career.absorption & 6)) {
462: if (!(to.conditions & COND_ABSORPTION) || random_range(1,100) <= magic_chance(target,GROUP_SPELL_ABSORPTION,-1)) {
467: if ((to.conditions & COND_REFLECTION) && random_range(1,100) <= magic_chance(target,GROUP_SPELL_REFLECTION,-1)) {
470: if ((to.conditions & COND_RESISTANCE) && random_range(1,100) <= magic_chance(target,GROUP_SPELL_RESISTANCE,-1)) { return; }
472: percent = no_resist || s->target == 0 ? 100 : magic_resist(target,s->element,element_bit(s->element),-(level-to.level)*5);
474: for (k = 0; k < 3; k++) {
475: if (s->group[k] == 255) { continue; }
481: value.rounds = (df_u16)(rounds*percent/100);
484: if (flags & 4) {
486: divide(level,s->magnitude[k][4])*random_range(s->magnitude[k][2],s->magnitude[k][3]))*percent/100);
517: held = *s; held.icon = 200 + slot;
518: for (k = 0; k < 3; k++) {
519: if (held.group[k] == 255) { continue; }
534: if (e->active && e->icon == 200 + slot && e->spell == spell && !e->target.place && !e->target.thing) { end(e); }
544: int i, target, caster, base[8];
553: if (e->group == GROUP_REGENERATE) { heal(&who,base,8,e->magnitude); }
565: int i, k, base[8];
576: if (e->group == GROUP_FORTIFY && e->sub < 8) { who.attribute[e->sub] += e->change; }
581: for (k = 0; k < 8; k++) { who.attribute[k] -= a->drained[k]; }
608: if (state->paid < 0 || state->paid > 1000000) { return 0; }
611: !(state->recall.x >= -100000000 && state->recall.x <= 100000000) ||
612: !(state->recall.y >= -100000000 && state->recall.y <= 100000000) ||
613: !(state->recall.z >= -100000000 && state->recall.z <= 100000000) ||
614: state->recall.yaw < 0 || state->recall.yaw >= 2048 || state->recall.pitch < -256 || state->recall.pitch > 256))) { return 0; }
618: if (a->active != 1 || !a->key.place || !a->key.thing || a->magicka < 0 || a->magicka > 1000000 ||
619: a->fatigue < 0 || a->fatigue > 1000000) { return 0; }
626: if (e->active != 1 || e->group < 0 || e->group >= 51 || e->sub < 0 || (e->sub >= 12 && e->sub != 255) ||
627: e->rounds < 0 || e->rounds > 65535 || e->magnitude < 0 || e->magnitude > 65535 ||
628: e->chance < 0 || e->chance > 255 || e->icon < 0 || e->icon > 255 || !spell_def(e->spell) || e->level < 1 || e->level > 1000 ||
629: e->element < 0 || e->element > 4 || e->permanent < 0 || e->permanent > 1 ||
630: e->change < -1000 || e->change > 1000) { return 0; }
635: a->delay < 0 || a->delay > 32767 || a->maximum < 0 || a->maximum > 32767 ||
636: a->left < 0 || a->left > 32767 || a->stage < 0 || a->stage > 2) { return 0; }
637: for (k = 0; k < 11; k++) { if (a->stat[k] < 0 || a->stat[k] > 1) { return 0; } }
638: for (k = 0; k < 8; k++) { if (a->drained[k] < -100000 || a->drained[k] > 100000) { return 0; } }
642: if (m->live != 1 || !spell_def(m->spell) || m->level < 1 || m->level > 1000 ||
643: m->element < 0 || m->element > 4 || m->target < 0 || m->target > 4 ||
644: !(m->x >= -100000000 && m->x <= 100000000) || !(m->y >= -100000000 && m->y <= 100000000) ||
645: !(m->z >= -100000000 && m->z <= 100000000) || !(m->dx >= -1 && m->dx <= 1) ||
646: !(m->dy >= -1 && m->dy <= 1) || !(m->dz >= -1 && m->dz <= 1) || m->burst < 0 || m->burst > 10000) { return 0; }
662: if (a->active && a->stage && !a->target.place && !a->target.thing) { flags |= a->id >= 128 ? 2 : 4; }
668: char text[2048], line[96];
673: for (i = 0; i < EFFECTS && used < 1800; i++) {
679: for (i = 0; i < AILMENTS && used < 1800; i++) {
682: log_format(line,sizeof(line),"%s %d: %s\n",a->id >= 128 ? "Poison" : "Disease",a->id,a->stage ? "active" : "latent");
686: { static char intro[300]; if (status_intro(intro,sizeof(intro)) > 0) { possessions_message(intro,0); } }
687: if (used == 26) { df_copy(text,words_state(18),sizeof(text)); }

## magic/potion.c (57 lines)
24: spell_register(g_recipe[i] + 20, POTION_SPELL + i);
43: return recipe >= 0 && recipe < RECIPES ? (int)(g_recipe[recipe][8] | g_recipe[recipe][9] << 8) : 0;
58: for (n = 0; n < 8 && n < cap && (signed char)g_recipe[recipe][n] != -2; n++) {
60: group[n] = (signed char)g_recipe[recipe][10 + n];
69: char name[32];
131: trap = item_create(container, 27, 1, -1);
135: object_edit(trap)->value = 5000;
136: if (random_range(1, 100) < 25) {
138: kind = random_range(0, 42);
140: kind = random_range(0, 42);
142: if (object_new(RECORD_SOUL, trap, kind, 100)) {
143: object_edit(trap)->value = g_soul_value[kind] + 5000;
163: mem_fill(g_cauldron.key, 254, CAULDRON);
233: g_cauldron.key[cell] = 254;
280: int i, unmatched, value, potency, group[8], index[8], n, k;
281: unsigned char ids[8];
287: n = potion_ingredients(recipe, group, index, 8);
290: for (i = 0; i < 8; i++) {
295: ids[i] = def ? (unsigned char)def->variants : 255;
299: ids[i] = 255;
303: for (i = 0; i < 8; i++) {
304: if (c->key[i] == 254) {
308: for (k = 0; k < 8; k++) {
313: ids[found] = 255;
322: return ((potency - value) >> 1) + 5 >= 1;
352: unsigned char x[8], y[8], z[8];
358: mem_zero(x, 8); mem_zero(y, 8); mem_zero(z, 8);
366: for (k = i; k < 7; k++) { x[k] = x[k + 1]; z[k] = z[k + 1]; y[k] = y[k + 1]; }
372: if (n > 3) {
373: for (i = 3; i < n; i++) { sum += y[i]; }
374: n = 3;
377: case 3:
402: mem_zero(r, 89);
403: for (i = 0; i < 6; i++) { r[i] = 255; }
404: r[6] = 4;
405: r[7] = 0;
410: if (flags & 1) { r[14 + i * 3] = r[15 + i * 3] = r[16 + i * 3] = 1; }
411: if (flags & 2) { r[23 + i * 3] = 70; r[24 + i * 3] = r[25 + i * 3] = 1; }
413: if (flags & 4) { r[32 + i * 5] = 10; r[33 + i * 5] = 1; r[34 + i * 5] = 1; }
415: for (i = 0; g_maker_unknown[i] && i < 24; i++) { r[47 + i] = (df_u8)g_maker_unknown[i]; }
416: r[73] = 255;
425: packed |= ((type[i] + 1) << 4 | (subtype[i] & 15)) << (i * 10);
435: for (i = 0; i < 3; i++) {
436: field = (message >> (i * 10)) & 1023;
437: if (field >> 4) {
438: type[n] = (field >> 4) - 1;
439: subtype[n] = field & 15;
476: int i, type[3], subtype[3], n;
477: char name[32];
507: int type[3], subtype[3], n;
508: df_u8 record[89];
532: id = item_create(container, 27, 4, -1);
546: if (!o || o->type != RECORD_ITEM || o->group != 27 || o->index != 4 || o->message < 1 || o->message > RECIPES) {
570: df_u32 pack[512];
571: int group[8], index[8], n, i, k, count, found;
573: n = potion_ingredients(recipe, group, index, 8);
574: count = ingredient_list(pack, 512);

## magic/itemmaker.c (102 lines)
25: static int item_type(int s) { return g.kind[s] == 1 ? g.type[s] + 15 : g.type[s]; }
31: for (k = 0; k < 5; k++) { g.pair[row][k][0] = g.pair[row][k][1] = 255; }
39: for (s = 0; s < 10; s++) { g.kind[s] = -1; g.param[s] = -1; }
40: for (s = 0; s < 12; s++) { clear_pairs(s); }
59: if (item->material >= 0 && item->material < 10) { points = (points * g_ench_material[item->material]) >> 2; }
91: for (s = 0, n = 0; s < 10; s++) { if (g.kind[s] == -1) { n++; } }
99: for (s = 0; s < 10; s++) { if (g.kind[s] == -1) { return s; } }
107: if (kind < 0 || kind >= (side ? 11 : 15)) { return ""; }
113: if (kind < 0 || kind >= (side ? 11 : 15)) { return 0; }
119: return g_souls ? g_souls(ids, cap < 63 ? cap : 63) : 0;
124: static const int count[3] = {
133: int pk, ids[63];
137: if (pk <= 3) { return spell_count(pk - 1); }
138: if (pk == 4) { return souls_list(ids, 63); }
139: return g_ench_label_count[pk - 100];
147: if (pk >= 1 && pk <= 3) {
153: if (pk == 4) {
159: if (pk >= 100 && param >= 0 && param < g_ench_label_count[pk - 100]) { return g_ench_labels[pk - 100][param]; }
167: for (row = 0; row < 12; row++) {
168: for (k = 0; k < 5; k++) { if (g.pair[row][k][0] == t && g.pair[row][k][1] == q) { return 1; } }
177: for (row = 0; row < 10; row++) {
178: for (k = 0; k < 5; k++) { if (g.pair[row][k][0] == t && g.pair[row][k][1] == 255) { return 1; } }
188: other = t == 25 ? 14 : 25;
189: for (s = 0; s < 10; s++) {
191: if (item_type(s) == t && g.param[s] != 5 && q == 5) { return 1; }
192: if (item_type(s) == t && g.param[s] == 5) { return 1; }
201: int pk, count, q, n, ids[63], t;
205: t = side ? kind + 15 : kind;
207: if (pk == 4) {
208: count = souls_list(ids, 63);
216: if ((t == 14 || t == 25) && rep_excluded(t, q)) { continue; }
225: int kind, t, n, count, s, scratch[40];
229: for (kind = 0; kind < (side ? 11 : 15) && n < cap; kind++) {
230: t = side ? kind + 15 : kind;
232: if (t == 20 && (!item || item->group != ITEM_GROUP_WEAPON)) { continue; }
233: if (t == 18 || t == 19) { continue; }
235: if (t == 25 || t == 14) {
236: for (s = 0; s < 10; s++) { if (g.kind[s] != -1 && item_type(s) == t && g.param[s] == 5) { break; } }
237: if (s < 10) { continue; }
239: if (t == 15) { for (s = 0; s < 10; s++) { if (g.kind[s] == 1 && g.type[s] == 0) { break; } } if (s < 10) { continue; } }
240: if (t == 21) { for (s = 0; s < 10; s++) { if (g.kind[s] == 1 && g.type[s] == 6) { break; } } if (s < 10) { continue; } }
243: count = allowed(side, kind, scratch, 40);
255: g.pair[s][k][0] = (df_u8)(t < 0 ? 255 : t); g.pair[s][k][1] = (df_u8)(p < 0 ? 255 : p);
263: if (kind < 0 || kind >= (side ? 11 : 15)) { return -IM_NO_ITEM; }
273: if (!side && kind == 11) { set_pair(s, 0, 23, 255); set_pair(s, 1, 11, 255); }
274: else if (!side && kind == 12) { set_pair(s, 0, 24, 255); set_pair(s, 1, 12, 255); }
275: else if (!side && kind == 8) { set_pair(s, 0, 8, 255); }
276: else if (!side && kind == 9) { set_pair(s, 0, 9, 255); }
277: else if (side && kind == 8) { set_pair(s, 0, 11, 255); set_pair(s, 1, 23, 255); }
278: else if (side && kind == 9) { set_pair(s, 0, 12, 255); set_pair(s, 1, 24, 255); }
301: if (!row || param < 0 || param >= 5) { return; }
303: set_pair(s, 1, pairs[row * 20 + param * 4], pairs[row * 20 + param * 4 + 1]);
304: set_pair(s, 2, pairs[row * 20 + param * 4 + 2], pairs[row * 20 + param * 4 + 3]);
313: set_pair(s, 0, side ? kind + 15 : kind, param);
315: if (kind == 11) { set_pair(s, 0, 23, 255); set_pair(s, 1, 11, 255); }
316: else if (kind == 12) { set_pair(s, 0, 24, 255); set_pair(s, 1, 12, 255); }
318: if (kind == 8) { set_pair(s, 0, 11, 255); set_pair(s, 1, 23, 255); }
319: else if (kind == 9) { set_pair(s, 0, 12, 255); set_pair(s, 1, 24, 255); }
331: for (r = 0; r < 12 && g_ench_soul_creature[r] != creature; r++) { }
332: if (r == 12) { return 0; }
336: kind = g_ench_soul_rule[r * 4 + k * 2]; arg = g_ench_soul_rule[r * 4 + k * 2 + 1];
337: if (kind == 255) { continue; }
338: if (kind == 128 || kind == 129) {
341: list = kind - 128; row = 10 + list; slot_out = 0;
342: for (j = 0; j < 5 && slot_out < 5; j++) {
343: spell = g_ench_soul_spells[arg * 5 + j];
344: if (spell == 255) { continue; }
351: set_pair(soul_slot, 3 + k, kind, arg);
354: while (want < 5 && g_ench_soul_power[r][want][0] != -1) { want++; }
361: if (type < 15) { g.kind[s] = 0; g.type[s] = type; }
362: else { g.kind[s] = 1; g.type[s] = type - 15; }
369: kind = g_ench_soul_rule[r * 4 + k * 2];
370: if (kind != 255) { set_pair(s, 3 + k, kind, g_ench_soul_rule[r * 4 + k * 2 + 1]); }
389: set_pair(s, 0, 15, 255);
393: set_pair(s, 0, side ? kind + 15 : kind, param);
413: if (slot < 0 || slot > 9 || g.kind[slot] == -1 || g.soul[slot]) { return; }
416: for (s = 0; s < 10; s++) {
421: clear_pairs(10); clear_pairs(11);
425: for (k = 0; k < 10; k++) { if (g.pending == slot) { g.pending = -1; } }
438: if (code >= 1000) { return g_enchant_by_param[code - 1000][param < 0 ? 0 : param < 12 ? param : 11]; }
439: if (code >= 5 && code <= 7) { return param >= 0 && param < spell_count(code - 5) ? magic_spell_cost(g_ench_spells[code - 5][param]) : 0; }
443: if (code == 99) {
447: if (as_points) { v /= 100; }
450: if (code >= 1000) { return g_ench_side_by_param[type][param < 0 ? 0 : param < 6 ? param : 5]; }
459: for (s = 0; s < 10; s++) {
471: for (s = 0; s < 10; s++) {
475: return cost * 10;
485: for (s = 0; s < 10; s++) { if (g.kind[s] != -1 && s != g.pending) { any = 1; } }
503: for (k = 0; k < 10; k++) { row->type[k] = -1; row->param[k] = -1; }
505: for (s = 0, n = 0; s < 10; s++) {
507: row->type[n] = g.kind[s] == 1 ? g.type[s] + 15 : g.type[s];
531: for (k = 0; k < 10; k++) {
532: if (row->type[k] < 0 || row->type[k] >= 15) { continue; }
536: if (code >= 1000) { value += g_enchant_by_param[code - 1000][param < 0 ? 0 : param < 12 ? param : 11]; }
537: else if (code >= 5 && code <= 7) { value += magic_spell_cost(param); }
548: for (k = 0; k < 10; k++) {
554: name = type < 15 ? g_ench_power_name[type] : g_ench_side_name[type - 15];
557: if (type < 3) { const SpellDef *spell; spell = spell_def(param); what = spell ? spell->name : ""; }
558: else if (type == 15) { const FoeDef *foe; foe = param < ENEMY_CLASSIC_KINDS ? foe_def(param) : 0; what = foe ? foe->name : ""; }
559: else { what = itemmaker_param_name(type < 15 ? 0 : 1, type < 15 ? type : type - 15, param); }
581: if (k < 0 || k > 9) { return 0; }
607: for (k = 0; k < 10; k++) { row.type[k] = -1; row.param[k] = -1; }

## magic/magicitem.c (33 lines)
27: if (!bytes || size < 4) {
30: g_count = (int)(bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24);
35: g_defs = bytes + 4;
61: return d && d[32] != 0;
77: dummy.skill[i] = 50;
79: return spell_cost(s, &dummy) * 10;
92: for (k = 0; k < 10; k++) {
93: type = d[35 + k * 2];
94: param = d[36 + k * 2];
95: if (type == 255) {
99: if (type >= 16) {
106: if (code < 100) {
108: value += code >= 5 && code <= 7 ? magic_spell_cost(param) : g_enchant_fixed[code];
110: value += g_enchant_by_param[code - 1000][param < 12 ? param : 11];
120: kind = 3;
122: kind = g_magic_kind0[random_range(0, 6)];
124: kind = g_magic_kind1[random_range(0, 4)];
126: if (kind == 12 || kind == 6) {
127: kind = character_state()->female ? 12 : 6;
163: kind = pick_base_kind(d[33]);
179: item->condition = item->max_condition = (int)(short)(d[0x37] | d[0x38] << 8);
180: item->material = d[0x3d];
200: static const df_u8 picture[] = {12,13,10,8,19,16,25,18,21,2,24,26,0,15,3,9,23,17,7,1,22,20,5};
215: id = item_create(container,d[33],d[34],d[0x3d] <= 9 ? d[0x3d] : -1);
219: item->condition = item->max_condition = df_ru16(d+0x37);
220: item->value = df_ru32(d+0x39);
222: if (ordinal < (int)sizeof(picture)) { item->inventory_image = (432 + character_state()->female)*128 + picture[ordinal]; }
232: if (!d || k < 0 || k > 9 || d[35 + k * 2] == 255) { return 0; }
233: *type = d[35 + k * 2]; *param = d[36 + k * 2] == 255 ? -1 : d[36 + k * 2];
248: (o->group == ITEM_GROUP_CURIO && o->index == 5) || o->message < 1 || o->message > g_count) {
264: return magic_item_unknown(id) ? (int)(((df_u32)object_at(id)->value * 25u) >> 8) : 0;
279: return magic_def_artifact(entry) && magic_def_enchant(entry,0,&type,&param) && type == 26 ? param : -1;
301: return 8700 + ordinal;

## magic/casting.c (57 lines)
21: static int g_slots[5][2];
32: void magic_slots(const int slots[5][2]) { mem_copy(g_slots,slots,sizeof(g_slots)); }
56: if (g_element >= 0 && g_draw) { g_draw(g_element,df_ifloor(g_animation*18.2065f)%6); }
83: radius = (s->icon >= 250 ? 168 : level*4+64)*3;
86: if (i == -1) { body = move_body(); dx = body->feet[0]-x; dy = body->feet[1]+40-y; dz = body->feet[2]-z; }
89: dx = e->x-x; dy = e->y+40-y; dz = e->z-z;
92: if (caster == -1 && i == -1 && s->target == 3) { continue; }
107: entity.pose[ENTITY_IDLE].w[0] = entity.pose[ENTITY_IDLE].h[0] = m->burst ? 128 : 48;
108: entity.pose[ENTITY_IDLE].rate = 12;
112: live->x = m->x; live->y = m->y-24; live->z = m->z;
114: live->pose[ENTITY_IDLE].w[0] = live->pose[ENTITY_IDLE].h[0] = m->burst ? 128 : 48;
134: step_missile(i,aim,110.0f/400.0f);
160: for (k = 0; k < 3; k++) { if (s->group[k] != 255) { skill_use(effect_school(s->group[k])); } }
211: strike = *s; strike.icon = 250;
214: if (s->target >= 3) { apply_area(&strike,-1,character_state()->level,e->x,e->y+40,e->z); }
228: strike = *def; strike.icon = 250;
229: sound_at(cast_sound(strike.element),foe->x,foe->y+40,foe->z);
230: if (strike.target >= 3) {
234: (victim ? victim->y : body->feet[1])+40,victim ? victim->z : body->feet[2]);
243: for (k = 0; k < 3; k++) {
244: if (spell->group[k] != 255) { skill_use(effect_school(spell->group[k])); }
246: temporary = id ? 0 : object_new(RECORD_SPELL,game_state()->inventory_id,spell->id,100);
269: id = object_new(RECORD_SPELL,game_state()->inventory_id,spell,100);
291: if (model >= 0) { actions_trigger(g_scene,model,6); }
292: if (m->target == 4) { apply_area(s,caster,m->level,m->x,m->y,m->z); }
306: m->burst += df_iround(dt*1000);
307: if (m->burst > 600) { m->live = 0; spawn_remove(g_visual[index]); g_visual[index] = 0; }
312: if (dx*dx+dy*dy+dz*dz > 2048*2048) { m->burst = 1; visual(index); return; }
313: distance = 400*dt;
318: e = dungeon_entity(i); radius = 60;
319: if (i == -1) { body = move_body(); dx = body->feet[0]-m->x; dy = body->feet[1]+40-m->y; dz = body->feet[2]-m->z; }
320: else { if (e->dead || !e->live) { continue; } dx = e->x-m->x; dy = e->y+40-m->y; dz = e->z-m->z; }
352: light.pose[ENTITY_IDLE].rate = 12; g_light = spawn_add(scene,&light,0); live = spawn_get(g_light);
355: live->x = camera->x+df_sin(camera->yaw)*192;
356: live->y = camera->y-18; live->z = camera->z+df_cos(camera->yaw)*192;
359: if (g_element >= 0) { g_animation += input->dt; if (g_animation*18.2065f >= 7) { g_element = -1; } }
366: cast = input->mouse_pressed & 3;
368: victim = -2; best = 160; camera_basis(&aim,&basis);
371: dx = e->x-camera->x; dy = e->y+40-camera->y; dz = e->z-camera->z;
374: if (distance < 0 || distance >= best || across > 60*60 ||
375: !line_clear(camera->x,camera->y,camera->z,e->x,e->y+40,e->z)) { continue; }
385: else if (s->target == 3) {
404: int base[8], ids[32], n, i, ranged, reach;
412: y = victim ? victim->y+40 : camera->y-20;
415: reach = victim ? 60 : 90;
416: ranged = distance > 4*reach && distance > 256;
420: for (i = 0; i < 32 && list[i] != 255; i++) {
430: if (ranged ? (s->target == 2 || s->target == 4) : (s->target == 0 || s->target == 1)) { ids[n++] = s->id; }
434: if (ranged && random_range(0,39)) { return 0; }
435: if (!line_clear(e->x,e->y+40,e->z,x,y,z)) { return 0; }
436: aim.x = e->x; aim.y = e->y+50; aim.z = e->z;
438: if (s->target == 2 || s->target == 4) { if (!launch(s,index,who.level,&aim)) { return 0; } }
439: else { sound_at(cast_sound(s->element),e->x,e->y+40,e->z); magic_apply(s->id,index,s->target == 0 ? index : target,who.level,0); }
457: if (s->target == 2 || s->target == 4) {
460: aim.yaw = df_atan2(dx,dz); aim.pitch = df_atan2(body->feet[1]+40-y,distance);
461: launch(s,-3,character_state()->level,&aim);
464: magic_apply(spell,s->target == 0 ? -1 : -3,-1,character_state()->level,0);

## magic/spellmaker.c (55 lines)
19: static int cost_of(int slot) { return (short)(g.spell[8 + slot * 2] | (g.spell[9 + slot * 2] << 8)); }
22: g.spell[8 + slot * 2] = (df_u8)(value & 255); g.spell[9 + slot * 2] = (df_u8)((value >> 8) & 255);
30: for (k = 0; k < 89; k++) { g.spell[k] = 1; }
31: for (k = MAKER_NAME_AT; k < MAKER_NAME_AT + 24; k++) { g.spell[k] = 0; }
32: g.spell[7] = 0; g.spell[6] = 4;
33: for (k = 0; k < 6; k++) { g.spell[k] = 255; }
34: for (k = 8; k < 14; k++) { g.spell[k] = 0; }
43: for (k = 0; k < 3; k++) {
44: if (type_of(k) == 255) { continue; }
57: for (k = 0; k < 3; k++) {
68: for (k = 0; k < 3; k++) { if (type_of(k) == 255) { return k; } }
78: for (i = 0; i < 51 && n < cap; i++) {
91: for (n = 0; n < 12 && effect_sub_name(type, n); n++) { }
103: if (maker_elements() == 0) { g.spell[6] = 4; }
106: if (mode == 0 && g.spell[7] != 0) { g.spell[7] = 0; }
107: if (mode == 1 && g.spell[7] == 0) { g.spell[7]++; }
127: if (slot < 0 || slot > 2 || type_of(slot) == 255) { return; }
130: if (sub == 255) { sub = 0; }
140: g.spell[slot * 2] = g.spell[slot * 2 + 1] = 255;
141: for (k = 0; k < 3; k++) { g.spell[14 + slot * 3 + k] = 1; g.spell[23 + slot * 3 + k] = 1; }
142: for (k = 0; k < 5; k++) { g.spell[32 + slot * 5 + k] = 1; }
146: void maker_select(int slot) { if (slot >= 0 && slot < 3) { g.slot = slot; } }
153: static const int base[11] = {14, 14, 14, 23, 23, 23, 32, 32, 32, 32, 32};
154: static const int step[11] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 3, 4};
155: static const int top[11] = {60, 60, 20, 100, 100, 20, 100, 100, 100, 100, 20};
156: static const int offset[11] = {0, 0, 0, 0, 0, 0, 1, -1, 1, -1, 0};
159: return base[index] + (index < 3 ? g.slot * 3 : index < 6 ? g.slot * 3 : g.slot * 5) + step[index];
166: r = index % 11;
168: return (r < 3 && (g.kind & 1)) || (r < 6 && (g.kind & 2)) || (g.kind & 4);
176: if (index < 0 || index > 10 || !g.kind) { return; }
190: if (index < 0 || index > 10) { return 0; }
196: int last, k, d[3], ch[3], m[5];
200: for (k = 0; k < 3; k++) { if (type_of(k) != 255) { last = k; } }
201: if (last < 0 || type_of(g.slot) == 255) { return; }
203: for (k = 0; k < 3; k++) { d[k] = g.spell[14 + g.slot * 3 + k]; ch[k] = g.spell[23 + g.slot * 3 + k]; }
204: for (k = 0; k < 5; k++) { m[k] = g.spell[32 + g.slot * 5 + k]; }
210: if (element < 0 || element > 4) { return; }
211: if (element == 4 || maker_elements() > 0) { g.spell[6] = (df_u8)element; }
218: if (target < 0 || target > 4) { return; }
220: if (target == 0 ? mode != 1 : mode != 0) { g.spell[7] = (df_u8)target; }
225: g.spell[72] = (df_u8)((g.spell[72] + MAKER_ICONS + step) % MAKER_ICONS);
233: for (; i < 25; i++) { g.spell[MAKER_NAME_AT + i] = 0; }
238: return (cost_of(0) + cost_of(1) + cost_of(2)) * target_factor(g.spell[7]) * 2;
243: return (((cost_of(0) + cost_of(1) + cost_of(2)) * target_factor(g.spell[7])) >> 1) << 2;
252: def.target = g.spell[7]; def.element = g.spell[6];
253: for (k = 0; k < 3; k++) {
255: for (n = 0; n < 3; n++) { def.duration[k][n] = g.spell[14 + k * 3 + n]; def.chance[k][n] = g.spell[23 + k * 3 + n]; }
256: for (n = 0; n < 5; n++) { def.magnitude[k][n] = g.spell[32 + k * 5 + n]; }
268: if (type_of(0) == 255 && type_of(1) == 255 && type_of(2) == 255) { return 0; }
269: if (!g.spell[MAKER_NAME_AT]) { return 1704; }
271: if (!gold_can_afford(price)) { return 1702; }
277: if (count > 128 || slot < 0) { return 1709; }
281: mem_copy(custom->record, g.spell, 89);
284: play_sound(206);
286: return 1705;

## magic/spellshop.c (8 lines)
17: base_chance = quality * 5 + 30;
19: saved = random_range(0, 32767);
23: for (pass = 0; count == 0 && pass < 1000 && standard > 0; pass++) {
30: while (cost > 95) {
34: if (chance < 5) {
35: chance = 5;
37: if (random_range(1, 50) < chance && count < cap) {
51: return holiday == 43 ? price >> 1 : price;

## character/ailments.c (44 lines)
28: int base[8];
33: return no_resist || magic_resist(target,2,bit,0) != 100;
43: for (count = 0; list[count] != 255; count++) { }
50: a->left = def->days == 255 ? 255 : random_range(def->days,def->stage);
51: for (i = 0; i < 11; i++) { a->stat[i] = def->stat[i]; }
64: a->id = id; a->left = random_range(def[2],def[3]); a->delay = random_range(def[0],def[1]);
76: if (a->active && same(a->target,magic_key(target)) && a->id < 100) { count++; }
88: int base[8], i, k;
94: if (!a->active || !same(a->target,magic_key(target)) || (poison ? a->id < 128 : a->id >= 100)) { continue; }
95: for (k = 0; k < 8; k++) {
117: switch (a->id-128) {
118: case 0: who->health -= random_range(2,12); break;
119: case 1: who->health -= 2; drain(a,who,4,1,0); a->stage = 2; break;
120: case 2: who->health -= random_range(1,10); break;
121: case 3:
122: drain(a,who,0,random_range(5,10),0); drain(a,who,3,random_range(1,5),0);
123: drain(a,who,6,random_range(1,5),0); a->stage = 2; break;
124: case 4: who->fatigue -= random_range(10,100); break;
125: case 5: who->health -= random_range(1,30); break;
126: case 6: drain(a,who,2,random_range(1,5),0); who->magicka -= random_range(5,15); a->stage = 2; break;
127: case 7: drain(a,who,2,random_range(5,20),0); drain(a,who,5,random_range(10,20),0); a->stage = 2; break;
128: case 8: who->fatigue -= random_range(10,100); drain(a,who,7,-random_range(4,10),0); a->stage = 2; break;
129: case 9: drain(a,who,1,random_range(10,30),0); drain(a,who,0,-random_range(5,20),0); a->stage = 2; break;
130: case 10: who->fatigue += random_range(5,10)*FATIGUE_UNIT; drain(a,who,2,random_range(1,4),0); a->stage = 2; break;
131: case 11: who->magicka += random_range(5,10); drain(a,who,4,random_range(1,5),0); a->stage = 2; break;
134: else if (a->stage == 2) { a->left = 254; a->id = 0; }
142: if (a->id > 99) { return; }
144: if (a->left == 254) { return; }
145: if (a->left != 255 && --a->left == 0) { a->left = 254; return; }
147: for (i = 0; i < 11; i++) {
149: if (i < 8) { drain(a,who,i,damage,1); }
150: else if (i == 8) { who->health -= damage; }
151: else if (i == 9) { who->fatigue -= damage; }
162: int i, k, index, changed, base[8];
169: if (!(minute%1440)) { daily(a,&who); }
170: if (a->id >= 128) { poison_tick(a,&who); }
171: else if (a->left == 254) {
173: for (k = 0; k < 8; k++) {
191: if (kind == ENEMY_RAT && random_range(0,100) <= 5) { infection = 1; }
192: if (kind == ENEMY_GIANT_BAT && random_range(0,100) <= 2) { infection = 1; }
193: if (kind == ENEMY_MUMMY && random_range(1,100) <= 5) { infection = 1; }
194: if ((kind == ENEMY_VAMPIRE || kind == ENEMY_VAMPIRE_ANCIENT) && !cursed && random_range(1,100) <= 2) { infection = 1; }
196: if (kind == ENEMY_NYMPH) { fatigue_spend(random_range(10,30)*FATIGUE_UNIT); }
197: if ((kind == ENEMY_SPIDER || kind == ENEMY_GIANT_SCORPION) && !magic_chance(-1,0,-1)) { magic_apply(66,creature,-1,0,0); }

## character/survival.c (9 lines)
24: survival->fraction += seconds*18.2065f;
32: if (limit > 50) { limit = 50; }
35: if (survival->ticks-survival->last_tick > 18) {
48: delta = 11;
50: if (random_range(1,100) > character_state()->skill[SKILL_SWIMMING]) { delta = 44; }
53: if (running) { delta = 88; }
54: if (climbing) { delta = 22; }
58: if (dungeon_nearby(&camera,1024)) {
64: state_pass_minutes(60); rest_recover();

## character/rest.c (25 lines)
43: else if (why == REST_VAGRANT) { msg_set(words_state(17)); }
45: if (to_room && g_room) { g_room(site.building, site.building_type == 15); }
66: if (curse_hungry()) { msg_set(words_state(36)); return 0; }
67: if (dungeon_nearby(cam,1024)) { msg_set("There are enemies nearby."); return 0; }
80: who = character_state(); rate = 60;
83: if (game_state()->career.rapid & 6) { rate += 40; }
85: heal = who->max_health*rate/1000+(who->attribute[ATTR_END]-50)/10;
92: who->magicka += who->max_magicka >> 3;
112: if (in->ch == 27) { rest_close(); return; }
115: if (click && in->cy >= 63 && in->cy <= 86 && in->cx >= 87 && in->cx <= 232) {
116: play_sound(203);
117: if (in->cx <= 183 && !may_sleep()) { rest_close(); return; }
118: if (in->cx <= 134) { g.stage = REST_HOURS; }
119: else if (in->cx <= 183) { g.until_healed = 1; g.stage = REST_RUNNING; }
125: if (in->ch >= '0' && in->ch <= '9' && n < 2) { g.input[n] = (char)in->ch; g.input[n+1] = 0; g.revision++; }
126: if (in->ch == 8 && n) { g.input[n-1] = 0; g.revision++; }
127: if (in->ch == 13) {
129: for (n = 0; g.input[n]; n++) { g.hours = g.hours*10+g.input[n]-'0'; }
130: if (g.hours < 1 || (g.loiter && g.hours > 3)) {
135: if (click && in->cx >= 140 && in->cx <= 179 && in->cy >= 76 && in->cy <= 85) { rest_close(); return; }
136: if (dungeon_nearby(cam,1024)) { msg_set("There are enemies nearby."); rest_close(); return; }
140: if (g_site && g_site(&site) && site.kind == SITE_INSIDE && site.building_type == 15 &&
146: if (g.time >= (g.loiter ? 1.8f : 0.5f)) {
147: g.time = 0; minute = game_state()->minutes; state_pass_minutes(60); g.elapsed++;
150: for (n = 1; n <= 60 && !interrupted; n++) {

## character/progress.c (23 lines)
12: sum = 0; low = 1000; high = 0;
13: for (i = 0; i < 3; i++) {
18: for (i = 0; i < 6; i++) { value = who->skill[career->minor[i]]; if (value > high) { high = value; } }
27: for (i = 0; i < level; i++) { power *= 1.04; }
29: needed = multiplier*needed/65536.0;
30: needed = power*needed*2.0/5.0+1;
31: return needed >= 2147483647.0 ? 2147483647 : (int)needed;
44: return skill < 32 ? (progress_state()->raised_lo & (1u << skill)) != 0 :
45: (progress_state()->raised_hi & (1u << (skill-32))) != 0;
55: char message[96];
58: if (progress->active || game_state()->minutes-progress->checked <= 360) { return; }
61: for (i = 0; i < 3; i++) {
62: if (who->skill[career->primary[i]] == 100 || who->skill[career->major[i]] == 100) { mastered = 1; }
65: uses = game_state()->skill_uses[skill]*(1.0-(who->reflexes-2)/8.0);
68: if ((who->skill[skill] >= 95 && mastered) || who->skill[skill] >= 100) { continue; }
70: if (skill < 32) { progress->raised_lo |= 1u << skill; }
71: else { progress->raised_hi |= 1u << (skill-32); }
74: level = (progress_sum(who,career)-progress->base_sum+28)/15;
79: health = random_range(career->hp_per_level/2,career->hp_per_level)+who->attribute[ATTR_END]/10-5;
82: progress->points = random_range(4,6); progress->active = 1;
84: play_sound(364);
102: if (direction > 0 && progress->points > 0 && value < 100) { direction = 1; }
125: if (progress_state()->points && total < 800) { return 0; }

## character/kit.c (16 lines)
11: static const unsigned char g_weapon[19][2] = {
12: {3,1}, {6,0}, {6,0}, {3,1}, {11,0}, {3,0}, {3,0}, {1,1}, {6,0},
13: {3,0}, {3,0}, {7,0}, {2,1}, {254,254}, {14,1}, {13,0}, {5,0}, {7,0}, {7,0}
20: if (n >= 32) { return; }
32: if (klass < 0 || klass >= 19) { return; }
33: gift(who, who->female ? 12 : 6, who->female ? 8 : 10, 0);
34: gift(who, who->female ? 12 : 6, 24, 0);
36: if (weapon == 254) {
37: gift(who, 3, 17, 0);
38: gift(who, 3, 14, 0);
39: gift(who, 3, 18, 0);
41: who->arrows += 24;
42: weapon = 14;
43: } else if (weapon != 255) {
44: gift(who, 3, weapon, g_weapon[klass][1]);
46: gift(who, 27, 0, 0);

## input/move.c (46 lines)
62: if (region < 0 || region == 3 || region == 4 || region == 5) {
66: return base * 255 / 220;
68: return base * 255 / 256;
101: g_yaw = classic_yaw & 0x7ff;
167: g_yaw = (g_yaw + delta) & 0x7ff;
178: if (region == 6) {
179: heading -= 512;
180: } else if (region == 8) {
181: heading += 512;
182: } else if (region == 7) {
183: heading += 1024;
185: heading = (heading + 4096) & 0x7ff;
209: yaw = g_yaw*(DF_TAU/2048);
211: df_sin(yaw),0,df_cos(yaw),g_body.radius+8,&hit);
212: return hit.model >= 0 && hit.wall <= g_body.radius+8;
230: chance = character_state()->skill[SKILL_CLIMBING]+(curse_race() == PLAYER_RACE_KHAJIIT ? 30 : 0);
231: if (!(character_state()->conditions & COND_CLIMBING) && random_range(1,100) > 95 && random_range(1,100) > chance) {
240: !(character_state()->conditions & COND_LEVITATING) && length > 0 && moved < 25 && into >= 251.0f/256) {
242: if (survival->climb_time > 14/18.2065f) {
258: body_set_size(&g_body, WALK_HEIGHT * 0.5f, WALK_EYE * 0.5f);
274: case 6:
277: case 7:
280: case 8:
286: return base * weight / (region == 1 ? 220 : 256);
326: g_yaw = (g_yaw + in->mouse_dx * game_config()->sensitivity_x) & 0x7ff;
328: if (g_pitch > 256) {
329: g_pitch = 256;
331: if (g_pitch < -256) {
332: g_pitch = -256;
338: g_pitch += df_iround((float)look * 160.0f * in->dt);
339: if (g_pitch > 256) { g_pitch = 256; }
340: if (g_pitch < -256) { g_pitch = -256; }
371: if (!g_air && (region == 0 || region == 3)) {
373: (float)(game_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
374: } else if (!g_air && (region == 2 || region == 5)) {
376: (float)(game_config()->turn_rate * 2 + 4) * 0.25f * in->dt));
397: fatigue_spend(11);
405: vertical = survival_state()->climbing ? speed*in->dt/3 : 0;
407: vertical = in->held ? (in->held[FLOAT_UP]-in->held[FLOAT_DOWN])*80*in->dt : 0;
408: if (swimming && g_weight > 250 && !(in->held && in->held[FLOAT_DOWN])) { vertical -= 80*in->dt; }
414: if ((who->conditions & COND_SLOWFALL) && g_body.vy < -60) { g_body.vy = -60; }
449: if (moved > 0.001f) {
450: g_bob_ms += in->dt * 1000.0f;
451: while (g_bob_ms >= 1000.0f) { g_bob_ms -= 1000.0f; }
479: for (i = 0; i < 3; i++) {
506: for (i = 0; i < 3; i++) { g_body.safe[i] = motion->safe[i]; }

## travel/journey.c (11 lines)
19: error = 0; factor = transport == 1 ? 128 : transport == 2 ? 192 : 256;
25: if (climate == 223) { out->sea_pixels++; minutes = ship ? 51 : 255; }
26: else { minutes = ((512-travel_terrain(climate))*((factor*102)/256))/256; }
27: if (camp) { minutes = minutes*300/256; }
33: out->hours = (out->minutes+59)/60;
34: out->cost = camp ? 0 : ((out->hours-out->sea_pixels)/24)*5+5;
35: if (ship && out->sea_pixels) { out->cost += (out->sea_pixels/24+1)*25; }
61: (a->id < 100 || (a->id >= 128 && a->stage))) { return 1; }
108: minute = game_state()->minutes%1440; wait = 0;
109: if (game_state()->career.flags & CLASS_SUN_DAMAGE) { if (minute >= 360 && minute < 1080) { wait = 1090-minute; } }
110: else if (!reckless) { if (minute >= 1080) { wait = 1440-minute+370; } else if (minute < 360) { wait = 370-minute; } }

## travel/doors.c (22 lines)
21: if (lock <= 0 || lock >= 20) { return 0; }
22: chance = skill+(level-lock)*5;
23: return chance < 5 ? 5 : chance > 95 ? 95 : chance;
28: return lock > 0 && lock < 20 ? 20-lock : 0;
52: for (i = 0; i < 4; i++) { previous |= (df_u32)row->game[i+4] << (i*8); }
72: if (!door || rating < 0 || rating > 65535) { return 0; }
75: row->game[2] = (df_u8)rating; row->game[3] = (df_u8)(rating >> 8);
87: row->game[2] = (df_u8)door->lock; row->game[3] = (df_u8)(door->lock >> 8);
114: if (!item->id || item->type != RECORD_ITEM || item->group != ITEM_GROUP_MISC || item->index != 3 ||
139: if (mover_start(scene,mover) && random_range(1,100) <= magic_chance(-1,16,-1)) {
144: if (random_range(1,100) <= magic_chance(-1,16,-1)) {
163: chance = magic_chance(-1,17,-1); magic_remove(-1,17);
164: if (random_range(1,100) <= chance && mover_start(scene,mover)) {
175: if (!chance) { actions_trigger(scene,scene->usables[usable].model,4); }
186: if (random_range(0,100) <= chance) {
189: sound_at(60,door->x0,door->y0,door->z0);
195: for (i = 0; i < 4; i++) { row->game[i+4] = (df_u8)((df_u32)skill >> (i*8)); }
201: if (!door->lock) { dungeon_alert(-1); actions_trigger(scene,scene->usables[usable].model,7); }
202: else { actions_trigger(scene,scene->usables[usable].model,4); }
218: if (dx*dx+dz*dz > 100*100) { return 0; }
229: if (chance && random_range(1,100) <= chance && mover_start(scene,mover)) {
232: sound_at(9,door->x0,door->y0,door->z0);

## travel/actions.c (49 lines)
29: trigger &= 15;
30: if (trigger == 8) { return event == 2 || event == 3 || event == 5 || event == 6; }
31: if (trigger == 9) { return event == 2 || event == 3; }
42: static int is_motion(int kind) { return (kind >= 1 && kind <= 8) || kind == 14; }
75: if (answer_matches(words_state(5656+g_link[index].data.sound),text)) { start_chain(next); }
85: if (a->kind >= 16 && a->kind <= 20 && link->reverse) { link->begun = 1; return; }
87: case 9:
92: magic_trap(a->sound,m ? m->x0 : body->feet[0],m ? m->y0+40 : body->feet[1]+40,
96: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 14:
99: case 10: case 13: case 29: break;
100: case 11:
101: if (!link->begun) { possessions_message(words_state(8600+a->sound),0); }
103: case 12:
106: if (!possessions_message(words_state(5400+a->sound),answer)) {
112: case 15: if (m && !link->begun) { door_lock(m,a->axis); } break;
113: case 16: case 18:
114: if (!link->begun && m && (a->kind == 18 || !m->lock) && !mover_at_end(m)) {
115: if (mover_start(g_scene,a->mover) && a->kind == 18) { door_unlock(m); }
118: case 17: if (m && !link->begun) { door_unlock(m); } break;
119: case 19: case 20:
122: if (a->kind == 20) { door_relock(m); }
125: case 21:
133: case 22: case 23: case 24: case 25:
138: case 28:
143: case 26: case 27:
147: if (a->kind == 26) { poison_apply(-1,random_range(128,139),0); }
148: else { disease_infect(-1,random_range(0,16),0); }
151: case 30:
170: if (link->data.kind == 12) { return; }
196: event = interact_using_touch() ? (action->trigger == 1 ? 1 : 3) : action->trigger == 6 ? 6 : 2;
224: m->secs = (link->data.kind >= 2 && link->data.kind <= 7 ? 50 : link->data.duration)/BIOS_HZ;
228: if (row && row->kind == KEEP_MOVER && row->game[8] == LINK_MARK) {
229: link->active = row->game[9] & 1; link->reverse = (row->game[9] >> 1) & 1;
230: link->begun = (row->game[9] >> 2) & 1;
231: link->elapsed = df_ru16(row->game+10)/BIOS_HZ;
232: link->pulse = df_ru16(row->game+12)/1000.0f;
234: mover_restore_motion(m,df_ru16(row->game+14)/65535.0f,link->reverse ? -1 : 1);
258: if (!(link->data.kind >= 16 && link->data.kind <= 20)) { link->reverse ^= 1; }
274: if (!link->active && !link->reverse && (!row || row->game[8] != LINK_MARK)) { continue; }
278: row->game[8] = LINK_MARK;
279: row->game[9] = (df_u8)(link->active | (link->reverse << 1) | (link->begun << 2));
280: ticks = df_iround(link->elapsed*BIOS_HZ); if (ticks > 65535) { ticks = 65535; }
281: pulse = df_iround(link->pulse*1000); if (pulse < 0) { pulse = 0; }
282: row->game[10] = (df_u8)ticks; row->game[11] = (df_u8)(ticks >> 8);
283: row->game[12] = (df_u8)pulse; row->game[13] = (df_u8)(pulse >> 8);
284: progress = m ? df_iround(mover_progress(m)*65535) : 0;
285: row->game[14] = (df_u8)progress; row->game[15] = (df_u8)(progress >> 8);
295: interact_cast(scene,camera->x,camera->y,camera->z,basis.fx*basis.cp,basis.sp,basis.fz*basis.cp,100,&hit);
296: if (hit.model >= 0 && hit.wall <= 100) { actions_trigger(scene,hit.model,event); }

## people/law.c (30 lines)
17: if (law->punishment[i] > 3) {
43: return crime >= 1 && crime <= 14 ? g_crime_name[crime - 1] : "";
127: f = faction_of_type(region, 15);
138: f = faction_of_type(region, 15);
175: penalty = -legal > 75 ? 75 : -legal;
176: half = (-legal) / 2 > 75 ? 75 : (-legal) / 2;
179: if (rand_range(1, 100) <= half) {
181: } else if (rand_range(1, 100) <= penalty) {
189: } else if (g_crime_fine[crime][3] < fine) {
190: fine = g_crime_fine[crime][3];
193: steps = fine / 40;
196: if (random_range(0, 32767) & 1) {
197: fine += 40;
199: court->days += 3;
204: court->days += (fine - gold) / 40;
209: if ((crime == 4 || crime == 3) && guilds && guilds->brotherhood_rank >= 0 && guilds->brotherhood_rank >= rand_range(0, 19)) {
211: } else if ((crime <= 2 || crime == 11) && guilds && guilds->thieves_rank >= 0 && guilds->thieves_rank >= rand_range(0, 19)) {
284: court->days += (court->fine - gold) / 40 + 1;
304: if (chance < 5) {
305: chance = 5;
306: } else if (chance > 95) {
307: chance = 95;
309: if (rand_range(1, 100) <= chance) {
321: roll = rand_range(1, 100) + legal;
322: if (roll > 75) {
324: } else if (roll < 25) {
382: n = rand_range(2, 5);
398: g_guard_timer = (float)rand_range(5, 10);
442: if (standing < -20 && !surrendered) {
445: if (standing >= -20 && standing <= 0 && (random_range(0, 32767) & 1) && !surrendered) {

## people/talk.c (36 lines)
50: if (d->type == 2 || d->type == 7 || d->type == 9) {
60: if (social < 5) {
64: if (magic_artifact_worn(0)) { talk->disposition += c->attribute[ATTR_PER]/5; }
65: if (talk->disposition < -20) {
78: if (!d || d->type == 15 || d->type == 14) {
85: if (social < 5) {
88: roll = rand_range(0, 15) - 10;
89: if (talk->score >= roll && rep >= 30) {
110: if (speaks->social_group == 4 && witnessed) {
125: id = own->type == 15 || own->type == 14 ? own->id : speaks->id;
126: social = (own->type == 15 || own->type == 14) ? own->social_group : speaks->social_group;
127: talk->style = social < 5 ? social : 1;
141: } else if (talk->disposition < 10) {
143: } else if (talk->disposition < 30) {
159: attitude += rand_range(1, 100) < c->skill[SKILL_ETIQUETTE] ? 10 : -5;
165: attitude += rand_range(1, 100) < c->skill[SKILL_STREETWISE] ? 10 : -5;
173: attitude += c->attribute[ATTR_PER] / 5;
174: roll = rand_range(0, 20);
186: return roll + 30 <= attitude ? 2 : 1;
193: seed = random_range(0, 32767);
194: if (talk->own == 806 || talk->own == 842) {
198: random_seed((df_u32)((talk->npc ^ (talk->npc >> 16)) + (unsigned)kind) + (unsigned)talk->row);
199: knows = rand_range(1, 20) <= g_knows[kind][talk->style] + 10;
215: for (i = 0; i < 13; i++) {
220: return 13;
222: return (building_flags & 8) ? 13 : -1;
284: *text = 1457;
288: if (talk->own != 806 && talk->own != 842) {
297: if (ask->tab == 3) {
299: *text = 8078;
301: *text = 8075 + (talk->attitude == 0 ? 0 : talk->attitude == 1 ? 1 : 2);
310: *text = 28;
321: return regional_known ? 10 : 11;
323: if (outdoors && (distance < 2048 || rand_range(1, 100) <= 25)) {
325: return 7332;
327: return 7333;

## people/steal.c (13 lines)
14: return random_range(0, 32767) % 101;
19: static char line[48];
26: chance += (character_state()->level - victim_level) * 5;
28: if (chance < 5) {
29: chance = 5;
30: } else if (chance > 95) {
31: chance = 95;
41: if (roll101() < 33) {
42: out->text = 8999;
46: out->gold = random_range(0, 32767) % 5 + 1;
52: guild_count_crime(5, 1, game_state()->minutes);
61: chance = character_state()->skill[SKILL_PICKPOCKET] - total / 32 - weight / 4;
76: if (random_range(0, 255) >= 108 - character_state()->skill[SKILL_PICKPOCKET]) {

## people/entry.c (15 lines)
59: if (b->type == 24) { out->allow = 1; return 1; }
61: if (b->type < 17 && building_open(b->type, b->faction, b->id, now, p->daytime, &hours)) { out->allow = 1; return 1; }
62: if (b->type < 17 && (b->flags & 16)) { return 0; }
63: if (b->flags & 16) { out->allow = 1; return 1; }
70: if (b->faction == 108 && p->brotherhood >= 0) { out->allow = 1; return 1; }
71: if (b->faction == 42 && p->thieves >= 0) { out->allow = 1; return 1; }
73: out->text = 256;
77: lock = b->name_seed % 10 + 3;
80: if (b->access >= 10) {
88: if (lock >= 20) {
93: chance = skill - lock * 5;
94: chance = chance < 5 ? 5 : chance > 95 ? 95 : chance;
95: if (random_range(0, 100) <= chance) {
98: guild_count_crime(5, 1, now);
99: if (witnessed || random_range(1, 300) < 100 - character_state()->skill[SKILL_STEALTH]) {

## people/lodging.c (5 lines)
69: out->text = 16;
77: raw = day <= 46 && day + days > 46 ? (days - 1) * ROOM_DAY_PRICE : days * ROOM_DAY_PRICE;
142: if (holiday == 37) {
171: if (site->location_kind == 4 || site->location_kind == 7 || site->location_kind > 9) {
184: if (site->building_faction == 41 && site->knight_member) {

## people/bank.c (4 lines)
30: return inventory_weight(game_state()->inventory_id) + amount / 100 <= inventory_capacity() * 4;
126: fee = amount / 100 + 1;
164: a->loan_owed = amount + (amount * 10) / 100;
216: a->balance += *price - (*price * 15) / 100;

## people/guild.c (137 lines)
23: if (g->m[i].used && (g->m[i].kind < 0 || g->m[i].kind > 255 || g->m[i].faction < 0)) {
27: return g->thieves_count >= 0 && g->thieves_count <= 100 && g->brotherhood_count >= 0 && g->brotherhood_count <= 100;
32: static const short knights[10] = { 368, 408, 409, 410, 411, 413, 414, 415, 416, 417 };
33: static const short temples[8][2] = { {21, 82}, {22, 84}, {24, 28}, {26, 92}, {27, 94}, {29, 98}, {33, 106}, {35, 36} };
37: case 108: return 0;
38: case 42: return 3;
39: case 40: return 1;
40: case 41: return 2;
43: for (i = 0; i < 8; i++) {
45: return KIND_TEMPLE + 14 + i;
48: for (i = 0; i < 10; i++) {
50: return KIND_KNIGHT + 4 + i;
101: for (i = 0; list[i] != 255; i++) {
115: guild = kind & 63;
134: guild = kind & 63;
162: guild = kind & 63;
207: step->text = g_guild_message[kind & 63][3];
218: guild = kind & 63;
233: rank = (m->rank + delta) & 255;
236: if (guild == 3 && (rank == 6 || rank == 8)) {
239: if (guild == 0 && rank < 100) {
242: if (rank > 100) {
243: step->text = 668;
246: if (guild == 3) { guilds_state()->thieves_count = 0; }
251: step->text = 667;
255: step->text = g_guild_message[guild][3 + rank];
286: return m->rank >= 4;
295: if (crime == 6) { curse_innocent_killed(); }
296: if (crime == 5) {
297: if (g->thieves_invite || g->thieves_count == 100) {
301: if (g->thieves_count >= 6) {
306: if (g->brotherhood_invite || g->brotherhood_count == 100) {
310: if (g->brotherhood_count >= 15) {
320: if (g->thieves_count != 100 && g->thieves_invite && g->thieves_invite < now && outdoors) {
321: g->thieves_count = 100;
323: *kind = 3;
326: if (g->brotherhood_count == 100 || !g->brotherhood_invite || g->brotherhood_invite >= now || !outdoors) {
329: g->brotherhood_count = 100;
339: for (i = 0; i < 89; i++) {
352: name[5] = (char)('0' + (member ? 1 : 0));
358: return ((10 - rank) << 8) / 10;
364: short member[3][2];
365: unsigned char which[3];
369: static const TempleRule g_temple[8] = {
370: /* 142 */ { 0, {{453, 1}, {454, 4}, {455, 4}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_SOULGEMS}, 241, 0 },
371: /* 143 */ { 2, {{462, 1}, {463, 6}, {464, 8}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 243, 460 },
372: /* 144 */ { 1, {{468, 2}, {469, 5}, {470, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 245, 466 },
373: /* 145 */ { 1, {{473, 4}, {474, 5}, {475, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 247, 471 },
374: /* 146 */ { 2, {{480, 3}, {481, 5}, {482, 6}}, {SVC_MAGIC_ITEMS, SVC_ITEM_MAKER, SVC_DAEDRA}, 249, 477 },
375: /* 147 */ { 2, {{485, 1}, {487, 5}, {488, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 250, 484 },
376: /* 148 */ { 0, {{490, 2}, {491, 5}, {492, 7}}, {SVC_POTIONS, SVC_POTION_MAKER, SVC_DAEDRA}, 252, 493 },
377: /* 149 */ { 1, {{496, 3}, {497, 6}, {498, 7}}, {SVC_SPELLS, SVC_SPELL_MAKER, SVC_DAEDRA}, 254, 494 }
384: if (person == 240) { return SVC_TEMPLE_QUEST; }
385: if (person == 810) { return SVC_DONATE; }
386: if (person == 813) { return SVC_CURE; }
397: guild = kind & 63;
398: if (kind >= 142 && kind <= 149) {
401: t = &g_temple[kind - 142];
404: for (i = 0; i < 3; i++) {
416: case 839: return SVC_TRAINING;
417: case 841: return rank >= 1 ? SVC_POTIONS : SVC_RANK_REFUSED;
418: case 840: return rank >= 3 ? SVC_POTION_MAKER : SVC_RANK_REFUSED;
419: case 843: return rank >= 5 ? SVC_SOULGEMS : SVC_RANK_REFUSED;
420: case 842: return rank >= 7 ? SVC_SPYMASTER : SVC_RANK_REFUSED;
421: case 807: return SVC_QUEST;
428: case 61: return SVC_TRAINING;
429: case 64: return SVC_SPELL_MAKER;
430: case 65: return rank >= 3 ? SVC_MAGIC_ITEMS : SVC_RANK_REFUSED;
431: case 802: return rank >= 5 ? SVC_ITEM_MAKER : SVC_RANK_REFUSED;
432: case 66: return rank >= 6 ? SVC_DAEDRA : SVC_RANK_REFUSED;
433: case 62: return rank >= 8 ? SVC_TELEPORT : SVC_RANK_REFUSED;
439: case 60: return SVC_SPELLS;
440: case 63: return SVC_QUEST;
441: case 801: return member ? SVC_IDENTIFY : SVC_MEMBERS_ONLY;
447: case 849: return SVC_TRAINING;
448: case 850: return SVC_REPAIRS;
449: case 851: return SVC_QUEST;
453: return person == 851 ? SVC_QUEST : SVC_TALK;
454: case 3:
457: case 803: return SVC_TRAINING;
458: case 804: return SVC_QUEST;
459: case 805: return rank >= 2 ? SVC_FENCE : SVC_RANK_REFUSED;
460: case 806: return rank >= 4 ? SVC_SPYMASTER : SVC_RANK_REFUSED;
465: if (guild >= 4 && guild <= 13) {
467: if (person == 845) { return SVC_ARMOR; }
468: if (person == 848) { return SVC_HOUSE; }
470: return person == 846 ? SVC_QUEST : SVC_MEMBERS_ONLY;
479: for (i = 0; i < 12; i++) {
491: *text = 4023;
494: *price = level * 100;
507: if (character_state()->skill[skill] > 50) {
508: *text = 4022;
513: skill_add_uses(skill, random_range(10, 20) * g_skill_advance[skill]);
514: *text = 5221;
515: *minutes = 180;
522: if (target == 255) {
524: if (region_legal(region) > 100) {
525: region_add_legal(region, 100 - region_legal(region));
527: } else if (target & 128) {
528: character_state()->attribute[target & 127] += amount;
529: if (character_state()->attribute[target & 127] > 100) { character_state()->attribute[target & 127] = 100; }
532: if (character_state()->skill[target] > 100) { character_state()->skill[target] = 100; }
539: if (b->target == 255) {
541: } else if (b->target & 128) {
542: character_state()->attribute[b->target & 127] -= b->amount;
550: static const short target[8] = { 0, 14, 133, 134, 129, 135, 255, 132 };
551: static const short words[8] = { 0, 705, 707, 709, 710, 712, 716, 717 };
555: if (kind == 142 || kind < 142 || kind > 149) {
560: *text = 454;
564: b->target = target[kind - 142];
567: b->end = (df_u32)(rank + 4) * 1440u + now;
568: b->amount = blessing_apply(b->target, rank + 10, region);
569: *text = words[kind - 142];
593: *text = 461;
597: metal = (m->rank - 1) & 255;
598: if (metal > 100) { metal = 2; } else if (metal > 10) { metal = 10; }
599: if (metal > 9) { metal = 9; }
600: id = item_create(game_state()->inventory_id, ITEM_GROUP_ARMOR, random_range(0, 6), metal);
605: *text = 463;
615: if (m->rank != 9) {
616: *text = 460;
629: for (i = 0; i < 16; i++) {
631: if (g_daedra_day[i] == day && i != 3) {
637: step->text = 480;
640: if (hall_faction != 40 && faction_has_enemy(hall_faction, prince)) {
644: price = (100 - hall_reputation) * 1000 + 100000;
646: if (price > 200000) { price = 200000; }
649: step->text = 481;
658: n = weather == 6 ? 15 : 5;
659: if (random_range(0, 32767) % 101 <= n) {
660: prince = 9;
662: chance = 30 + faction_rep(prince);
663: if (g_daedra_weather[prince] == 100 || weather == g_daedra_weather[prince]) {
664: chance += 30;
666: if (random_range(1, 100) > chance) {
670: *kind = g_daedra_kind[random_range(0, 4)];
679: if (kind < 0 || kind > 21 || rank < 0 || rank > 9) { return ""; }

## people/shelf.c (8 lines)
11: if (model_id == 418) {
14: if (model_id != 410) {
18: return variant == 3 || variant == 4 || variant == 7 || variant == 8 || variant == 50 || variant == 51 ||
19: (variant >= 32 && variant <= 38);
38: chance = 100;
47: group = character_state()->female ? 12 : 6;
51: id = item_create(container, ITEM_GROUP_BOOK, (quality + 3) / 5, -1);
64: if (random_range(0, 32767) % 100 > chance) {

## people/market.c (1 lines)
195: play_sound(204);

## people/names.c (37 lines)
14: static df_u32 g_offset[6], g_count[6];
29: return region >= 0 && region < 62 ? g_region_bank[region] : 0;
35: return random_range(0, 32767) % n;
40: return (df_u32)p[0] | (df_u32)p[1] << 8 | (df_u32)p[2] << 16 | (df_u32)p[3] << 24;
69: for (i = 0; i < 10 && at + (df_u32)i < g_size && g_data[at + (df_u32)i]; i++) {
78: char piece[11];
97: if (!female && rand_mod(100) < 75) {
98: part(name, cap, 3, 0);
101: part(name, cap, 4, 0);
109: case 8:
111: if (rand_mod(50) < 25) {
116: case 9:
118: if (rand_mod(50) < 25) {
123: part(name, cap, 3, 0);
126: case 10:
127: if (!female && rand_mod(100) < 25) {
128: part(name, cap, 3, 1);
150: case 1: case 8: case 9: case 10:
158: part(name, cap, 4, 0);
159: part(name, cap, 5, 0);
166: char first[30], last[30];
171: if (!g_data || bank < 0 || bank >= 11 || g_size < (df_u32)(bank + 1) * 48u) {
174: b = g_data + bank * 48;
175: for (i = 0; i < 6; i++) {
176: g_offset[i] = word(b + i * 4);
177: g_count[i] = word(b + 24 + i * 4);
180: given(bank, female, first, 30);
182: for (i = 0; first[i] && i < 39 && i < cap - 1; i++) {
186: family(bank, last, 30);
188: append(out, cap < 41 ? cap : 41, " ");
189: append(out, cap < 41 ? cap : 41, last);
197: saved = random_range(0, 32767);
208: if (npc->type != 8 && npc->type != 53) {
247: char line[96];
266: saved = random_range(0, 32767);
329: if (b->faction == 108 && b->brotherhood) {
331: } else if (b->faction == 42 && b->thieves) {

## people/standing.c (31 lines)
138: if (s->rep[i] < -100 || s->rep[i] > 100) {
143: if (s->price[i] < 250 || s->price[i] > 4000) {
177: *rep = (short)(sum > 100 ? 100 : (sum < -100 ? -100 : sum));
208: if (g_def[f].id == 108) {
214: if (g_parent[f] < 0 && g_def[f].id != 844) {
217: if (g_def[f].id == 844) {
220: m = g_membership ? g_membership(64) : 0;
237: if (g_def[f].id == 240) {
238: m = g_membership ? g_membership(128) : 0;
286: if (s->rep[i] > 100) {
287: s->rep[i] = 100;
288: } else if (s->rep[i] < -100) {
289: s->rep[i] = 100;
351: relation = 8;
369: *score += 15;
372: *score += 10;
375: if ((has_enemy(o, f) || has_enemy(f, o)) && relation > 3) {
376: *score = 20;
377: relation = 3;
381: if (g_enemy[o][i] >= 0 && g_enemy[o][i] == g_enemy[f][j] && relation > 4) {
382: relation = 4;
383: *score += 5;
389: if (g_ally[o][i] >= 0 && g_ally[o][i] == g_ally[f][j] && relation > 5) {
390: relation = 5;
391: *score += 5;
397: if (g_ally[o][i] >= 0 && has_ally(o, g_enemy[f][j]) && relation > 6) {
398: relation = 6;
399: *score -= 5;
405: if (g_enemy[o][i] >= 0 && has_enemy(o, g_ally[f][j]) && relation > 7) {
406: relation = 7;
407: *score -= 5;

## items/inventory.c (71 lines)
17: level = character_state()->level - 10;
18: roll = random_range(0, 255) + (level < 0 ? level * 4 : level * 2);
20: if (roll > 256) { roll = 256; }
21: for (metal = 0; metal < 9 && roll > material_share(metal); metal++) {
30: item->color = metal + 16;
31: item->value *= 3 * material_value(metal);
32: item->weight = item->weight * material_weight(metal) / 4;
33: item->max_condition = item->max_condition * material_condition(metal) / 4;
40: static const int low[7][3] = {{3,7,4},{8,9,9},{10,16,12},{17,21,18},{22,26,23},{27,27,27},{0,1,1}};
41: static const int high[7][3] = {{3,7,6},{8,9,9},{11,16,15},{17,21,20},{22,26,25},{27,27,27},{0,1,1}};
42: return index < 7 ? random_range(low[index][type], high[index][type]) : -1;
53: item->color = 18;
65: if (!def || metal < -1 || metal > 9) { return 0; }
66: id = object_new(RECORD_ITEM, parent, index, 100);
73: if (group == ITEM_GROUP_ARMOR && !character_state()->female) { item->inventory_image += 4 * 128; }
74: item->inventory_image += body_shape(character_state()->race) * 128;
76: if (random_range(0,100) >= 25) {
77: item->color = random_range(1,10);
78: if ((group == ITEM_GROUP_MENS_CLOTHING && (index == 10 || index == 11)) || (group == ITEM_GROUP_WOMENS_CLOTHING && index == 8)) {
79: item->color = clothing_dye(0,random_range(0,32767));
80: } else if (group == ITEM_GROUP_MENS_CLOTHING && index == 4) { item->color = clothing_dye(1,random_range(0,32767)); }
82: if ((group == ITEM_GROUP_MENS_CLOTHING && (index == 13 || index == 14)) || (group == ITEM_GROUP_WOMENS_CLOTHING && (index == 9 || index == 10))) {
89: roll = metal < 0 ? random_range(1, 100) : 100;
90: item->armor_type = roll < 70 ? 0 : (roll < 90 ? 1 : 2);
95: if (image >= 0) { item->inventory_image = (item->inventory_image & ~127) + image; }
98: if (group == ITEM_GROUP_MISC && index == 4) { item->message = random_range(0,32767) % 20 + 1; }
99: if (group == ITEM_GROUP_WEAPON && index == WEAPON_ARROW) { item->count = random_range(1, 20); item->condition = 0; }
116: item->value = 300 + (int)(((seed >> 16) & 0x7fffu) % 501u);
119: if (group == ITEM_GROUP_PAINTING) { item->message = random_range(0,32767); }
137: char tail[64];
142: for (k = 0; out[i+3+k] && k < 63; k++) { tail[k] = out[i+3+k]; }
202: return def && !(def->flags & 4);
231: bits = item->index >= 7 ? 1 << (item->index + 2) : 1 << (item->armor_type + 6);
240: if (item->index == 1 || (item->index >= 5 && item->index != 6)) { return 233; }
241: return item->armor_type+231;
243: if (item->group == ITEM_GROUP_MENS_CLOTHING || item->group == ITEM_GROUP_WOMENS_CLOTHING) { return 234; }
244: return 236;
299: if (item->group == ITEM_GROUP_CURRENCY) { weight += item->value / 100; }
302: if (parent == game_state()->inventory_id) { weight += character_state()->gold / 100; }
316: log_format(out,capacity,"%d%s",quarters/4,fraction[quarters%4]);
332: if (shield && shield->group == ITEM_GROUP_ARMOR && shield_covers(shield->index,part)) { value += (shield->index-6)*5; }
333: if (value > 200) { value = 200; }
334: return value / 5;
353: if (klass == 18) {
355: int skills[12], n;
357: for (i = 0; i < 3; i++) { skills[i] = career->primary[i]; skills[i+3] = career->major[i]; }
358: for (i = 0; i < 6; i++) { skills[i+6] = career->minor[i]; }
359: for (n = 0; n < 12 && !(skills[n] >= SKILL_DESTRUCTION && skills[n] <= SKILL_MYSTICISM); n++) { }
360: if (n == 12) { return; }
363: for (i = 0; i < 6 && (spell = start_spell(klass, i)) != 255; i++) {
380: weight = item->group == ITEM_GROUP_CURRENCY ? item->value / 100 : item->weight * item->count;
382: if (taking && inventory_weight(parent) + weight > inventory_capacity() * 4) {
388: if (gold > 100000000 - character_state()->gold) { g_error = "Too much gold."; return 0; }
392: play_sound(204);
405: if (count > 199) { count = 199; }
432: for (category = 2; category < 15; category++) {
433: chance = table[category] * (category <= 5 ? level : 1);
434: if (random_range(0, 32767) % 100 > chance) { continue; }
436: if (group == 255) { group = character_state()->female ? 12 : 6; }
444: } while (random_range(0, 32767) % 100 <= chance);
478: for (i = 0; i < 32 && !inventory_rows(id,-1,0,0); i++) { roll_loot(id, table); }
496: !item->name[0] || item->name[24] || item->spell_icon < 0 || item->spell_icon >= 69) { return 0; }
499: if (item->name[31] || !item_def(item->group, item->index) || item->count < 1 || item->count > 10000 ||
500: item->material < 0 || item->material > 9 || item->weight < 0 || item->weight > 100000 ||
501: item->value < 0 || item->value > 100000000 || item->max_condition < 0 || item->max_condition > 1000000 ||
503: item->armor_type < 0 || item->armor_type > 2 || item->inventory_image < 0 || item->inventory_image > 65535 ||
504: item->color < 0 || item->color > 31 || item->dropped_image < 0 || item->dropped_image > 65535 ||
505: item->message < 0 || item->message > 99999) { return 0; }
524: if (item->group == ITEM_GROUP_WEAPON && item->index == 17 && item->count > 1) { item->index = 18; }
530: if (item->group == ITEM_GROUP_ARMOR && !state->who.female) { item->inventory_image += 4 * 128; }
531: item->inventory_image += body_shape(state->who.race) * 128;

## items/describe.c (54 lines)
15: char title[65], author[65];
16: char painting[4][128], artist[40], names[4][40];
24: if (item->group == ITEM_GROUP_ARMOR) { return 1000; }
25: if (item->group == ITEM_GROUP_WEAPON) { return item->index == WEAPON_ARROW ? 1011 : 1001; }
26: if (item->group == ITEM_GROUP_BOOK) { return 1009; }
27: if (item->group == ITEM_GROUP_MISC && item->index == MISC_SOUL_TRAP) { return 1004; }
28: if (item->group == ITEM_GROUP_MISC && item->index == MISC_LETTER_OF_CREDIT) { return 1007; }
29: return 1003;
47: if (i >= 29) { df_copy(out,d->names[i-29],cap); return n; }
48: if (i >= 24) { df_copy(out,i == 28 ? d->artist : d->painting[i-24],cap); return n; }
49: if (i >= 13) {
53: value = i < 16 ? s->duration[d->effect][i-13] : i < 19 ? s->chance[d->effect][i-16] : s->magnitude[d->effect][i-19];
60: df_copy(out,item->group == ITEM_GROUP_ARMOR && (item->index >= 7 || item->index == 5) ? "" :
64: case 1: case 2: case 7: case 8: df_copy(out,item->name,cap); break;
65: case 3:
66: value = item->index >= 7 ? item->index-6 :
67: (item->armor_type == 2 ? 45+material_hit(item->material) : armor_base(item->armor_type))/10;
70: case 4:
71: value = material_hit(item->material)/5;
75: case 5: df_copy(out,condition_name(item->condition,item->max_condition),cap); break;
76: case 6: log_format(out,cap,"%d",item->value); break;
77: case 9:
78: weight = (item->group == ITEM_GROUP_CURRENCY ? item->value/100 : item->group == ITEM_GROUP_TRANSPORT ? 0 : item->weight*item->count) + inventory_weight(item->id);
81: case 10: df_copy(out,d->title,cap); break;
82: case 11: df_copy(out,d->author,cap); break;
83: case 12:
100: char value[160];
134: if (!def || effect < 0 || effect >= 3) { return; }
156: return (*seed >> 16) & 32767;
166: char piece[11];
169: static void person_name(df_u32 *seed, int bank, int female, char out[40])
172: char surname[30];
177: name_part(&parts,0,seed,out,30); name_part(&parts,1,seed,out,30); name_part(&parts,2,seed,out,30);
178: if (!female && lore_random(seed)%100 < 75) { name_part(&parts,3,seed,out,30); }
179: if (female) { name_part(&parts,4,seed,out,30); }
182: name_part(&parts,first,seed,out,30); name_part(&parts,first+1,seed,out,30);
183: first = bank == 2 ? 0 : 4;
184: name_part(&parts,first,seed,surname,30); name_part(&parts,first+1,seed,surname,30);
185: if (bank == 2) { append(surname,30,"sen"); }
186: if (*surname) { append(out,40," "); append(out,40,surname); }
190: static void artist_name(df_u32 *seed, char out[40])
194: female = lore_random(seed) & 1; bank = lore_random(seed) & 7;
200: df_u8 choices[40];
202: char raw[128];
203: static const int offsets[4] = {123,9543,0,3457};
210: picture = lore_random(&seed)%180;
212: for (i = 0; i < 4; i++) {
213: for (count = 0; count < 10 && choices[i*10+count] != 255; count++) { }
216: df_copy(d.painting[i],words_state(6100+i*100+choices[i*10+pick]),sizeof(d.painting[i]));
218: variant = lore_random(&seed)%9;
223: for (i = 0; i < 4; i++) {
225: pick = lore_random(&named) & 7;
228: for (i = 0; i < 4; i++) {
232: expand(words_variant(250,variant),&d,out,capacity);

## items/ground.c (14 lines)
27: px = where->origin_x/8; pz = where->origin_z/8;
28: x += (where->origin_x%8)*STREAM_CHUNK; z += (where->origin_z%8)*STREAM_CHUNK;
31: if (px < 0 || px >= 1000 || pz < 0 || pz >= 500) { return 0; }
32: place = 0x80000000u | (df_u32)(pz*1000+px);
34: qx = df_ifloor(x/64); qz = df_ifloor(z/64);
35: thing = 0x80000000u | (((df_u32)qx & 32767u) << 15) | ((df_u32)qz & 32767u);
48: if (pixel >= 500000) { return 0; }
49: *x = container->x+(pixel%1000*8-where->origin_x)*STREAM_CHUNK;
51: *z = container->z+(pixel/1000*8-where->origin_z)*STREAM_CHUNK;
74: for (i = 0; i < GROUND_VISIBLE; i++) { nearest[i] = 320*320; }
90: entity.pose[ENTITY_IDLE].w[0] = entity.pose[ENTITY_IDLE].h[0] = 24;
107: ex = entity->x-x; ey = entity->y+12-y; ez = entity->z-z;
109: if (along < 0 || off > 12*12) { continue; }
110: along -= df_sqrt(12*12-(off > 0 ? off : 0));

## state.c (66 lines)
28: if (attribute >= 0 && attribute < ATTRS && value >= 0 && value <= 100) { g_state.base_attribute[attribute] = value; }
93: g_state.skill_uses[skill] = uses < 0 ? 0 : uses > 20000 ? 20000 : (df_u32)uses;
129: g_state.next_low = 10000;
149: return low + (int)(((g_state.random >> 16) & 0x7fffu) %
226: for (i = 0; i < 53000; i++) {
230: id = ((df_u32)high << 16) | g_state.next_low++;
328: if (!career || who.race < 0 || who.race > 7) {
342: g_state.move_id = object_new(RECORD_MOVE, PLAYER_ROOT, 0, 100);
343: g_state.eye_id = object_new(RECORD_EYE, g_state.move_id, 0, 100);
344: g_state.entity_id = object_new(RECORD_ENTITY, g_state.move_id, 0, 100);
345: for (i = 0; i <= 8; i++) {
347: if (i == 4) {
350: id = object_new(RECORD_CONTAINER, g_state.entity_id, i, 100);
355: object_new(RECORD_QUESTS, g_state.entity_id, 0, 100);
356: object_new(RECORD_OPTIONS, g_state.entity_id, 0, 100);
357: object_new(RECORD_LOGBOOK, g_state.entity_id, 0, 100);
358: object_new(RECORD_BANK, g_state.entity_id, 0, 100);
359: for (i = 0; i < who.gifts && i < 32; i++) {
364: object_edit(id)->count = who.arrows ? who.arrows : 24;
367: if (i == who.gifts-2 && who.klass != 13 && who.gift[i].group == ITEM_GROUP_WEAPON) {
369: object_edit(id)->value = 15;
385: g_state.settings_flags = flags & 3;
391: if (!g_state.ready || mode < 0 || mode > 19 || g_state.depth >= MODES) {
433: g_state.yaw = yaw & 2047;
443: *year = 400 + (int)(days / 360u);
461: c.start_year = 400;
465: clock_set_day(400 * 360 + (int)(g_state.minutes / 1440u));
529: total = g_state.frame_fraction + seconds * 1000.0f;
546: if (who->race < 0 || who->race > 7 || who->female < 0 || who->female > 1 ||
548: who->gifts < 0 || who->gifts > 32 || who->reflexes < 0 || who->reflexes > 4 ||
549: who->gold < 0 || who->gold > 100000000 || who->level < 1 || who->max_health < 1 || who->max_health > 1000000 ||
551: who->max_magicka < 0 || who->max_magicka > 1000000 ||
553: who->fatigue < 0 || who->fatigue > 1000000 || career->name[15]) {
558: if (who->attribute[i] < 0 || who->attribute[i] > 1000 ||
559: state->base_attribute[i] < 0 || state->base_attribute[i] > 1000) {
564: if (who->skill[i] < 0 || who->skill[i] > 1000) {
568: for (i = 0; i < 3; i++) {
574: for (i = 0; i < 6; i++) {
611: !(object->x >= -100000000 && object->x <= 100000000) ||
612: !(object->y >= -100000000 && object->y <= 100000000) ||
613: !(object->z >= -100000000 && object->z <= 100000000)) {
651: state->progress.base_sum < -1500 || state->progress.base_sum > 600 || state->progress.raised_hi > 7) { return 0; }
652: if (state->survival.breath < 0 || state->survival.breath > 80 ||
658: if (state->progress.floor[i] < 0 || state->progress.floor[i] > 100) { return 0; }
665: state->settings_flags < 0 || state->settings_flags > 3 ||
667: state->depth < 0 || state->depth > MODES || state->mode < 0 || state->mode > 19 ||
668: state->yaw < 0 || state->yaw >= 2048 || state->pitch < -256 || state->pitch > 256 ||
669: !(state->x >= -100000000 && state->x <= 100000000) ||
670: !(state->eye_y >= -100000000 && state->eye_y <= 100000000) ||
671: !(state->z >= -100000000 && state->z <= 100000000)) {
675: if (state->mode_stack[i] < 0 || state->mode_stack[i] > 19) {
684: return value >= -100000000 && value <= 100000000;
699: for (i = 0; i < 6; i++) { if (!encounter_kind_valid(state->choices[i])) { return 0; } }
722: kind = (int)(row->owner & 255u); archive = (int)((row->owner >> 8) & 4095u);
735: state->save_label[31] || state->preview_valid < 0 || state->preview_valid > 1 ||
737: state->hud_mode < 0 || state->hud_mode > 3 ||
742: if (motion->yaw < 0 || motion->yaw >= 2048 || motion->pitch < -256 || motion->pitch > 256 ||
743: !(motion->eye > 0 && motion->eye <= 100) ||
744: !(motion->bob_ms >= 0 && motion->bob_ms < 1000) ||
745: !(motion->vy >= -4000 && motion->vy <= 4000) ||
746: !(motion->vx >= -10000 && motion->vx <= 10000) ||
747: !(motion->vz >= -10000 && motion->vz <= 10000) ||
751: motion->water < 0 || motion->water > 1 || motion->weight < 0 || motion->weight > 100000 ||
754: for (i = 0; i < 3; i++) {
813: if (g_state.who.klass == 18) {
814: class_set(18, &g_state.career);
