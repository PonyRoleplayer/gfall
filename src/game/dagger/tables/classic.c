#include "game/dagger/tables/classic.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/formats/climate.h"

/*
 * See classic.h and docs/data.md. Classic keeps these tables inside
 * FALL.EXE; Daggerfall Unity carries them in C# (MIT License, Copyright
 * (c) 2009-2023 Daggerfall Workshop, https://github.com/Interkarma/daggerfall-unity):
 * EnemyBasics.Enemies, RandomEncounters.EncounterTables,
 * WeatherTable.json from the Daggerfall Chronicles, p. 47,
 * DaggerfallDateTime's calendar, RaceTemplate, DFCareer and
 * DaggerfallSkills' skills, and ItemTemplates, FormulaHelper, ItemBuilder
 * and WeaponBasics' weapons and materials.
 */

/*
 * EnemyBasics, by mobile id: monsters 0..42, then the classes 128..146,
 * in EnemyKind's field order. A field DFU leaves out is C#'s default, 0
 * (so a class's least metal is iron, 0). Names are DFU's, but 39 (the
 * horse, never placed: its pictures are missing) and 146 (the city watch).
 */
static const EnemyKind g_enemies[] = {
    { 0, "Rat", 0, 4, 2, 1, 0, 0, 0, 0, 0, 0, 0, 255, 255, 401, 1, 0, 115, 116, 117, -1, { 1, 0, 0 }, { 4, 0, 0 }, 9, 16, 1, 6, 0, 2, 0u, "", { 0 } },
    { 1, "Imp", 1, 2, 16, 0, 0, 0, 1, 0, 0, 0, 1, 256, 256, 406, 5, 0, 118, 119, 120, 1, { 2, 0, 0 }, { 15, 0, 0 }, 11, 18, 2, 3, 1, 40, 1000u, "D", { 0 } },
    { 2, "Spriggan", 0, 1, 3, 1, 0, 0, 1, 0, 0, 0, 0, 257, 257, 406, 3, 0, 121, 122, 123, -1, { 1, 1, 1 }, { 8, 8, 10 }, 12, 26, 3, -4, 0, 240, 1000u, "B", { 0 } },
    { 3, "Giant Bat", 1, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 258, 258, 401, 0, 0, 124, 125, 126, -1, { 2, 0, 0 }, { 12, 0, 0 }, 12, 26, 3, 6, 0, 80, 0u, "", { 0 } },
    { 4, "Grizzly Bear", 0, 4, 4, 1, 0, 0, 0, 0, 0, 0, 0, 259, 259, 401, 2, 0, 127, 128, 129, -1, { 1, 1, 1 }, { 8, 8, 10 }, 13, 34, 4, 6, 0, 1000, 0u, "", { 0 } },
    { 5, "Sabertooth Tiger", 0, 4, 5, 1, 0, 0, 0, 0, 0, 0, 0, 260, 260, 401, 3, 0, 130, 131, 132, -1, { 1, 1, 3 }, { 10, 10, 15 }, 13, 34, 4, 6, 0, 1000, 0u, "", { 0 } },
    { 6, "Spider", 0, 4, 6, 1, 0, 0, 0, 0, 0, 0, 0, 261, 261, 401, 4, 0, 133, 134, 135, -1, { 5, 0, 0 }, { 15, 0, 0 }, 13, 34, 4, 5, 0, 400, 0u, "", { 0 } },
    { 7, "Orc", 0, 2, 7, 1, 0, 0, 1, 0, 1, 0, 0, 262, 262, 96, 2, 0, 136, 137, 138, -1, { 1, 0, 0 }, { 6, 0, 0 }, 13, 34, 5, 7, 0, 600, 1000u, "A", { 0 } },
    { 8, "Centaur", 0, 1, 8, 1, 0, 0, 1, 0, 1, 0, 0, 263, 263, 406, 0, 0, 139, 140, 141, -1, { 5, 0, 0 }, { 15, 0, 0 }, 14, 46, 5, 6, 1, 1200, 3000u, "C", { 0 } },
    { 9, "Werewolf", 0, 2, 9, 0, 0, 0, 1, 0, 0, 0, 0, 264, 264, 96, 5, 0, 142, 143, 144, 2, { 1, 1, 2 }, { 10, 10, 12 }, 17, 66, 6, 5, 0, 480, 1000u, "", { 0 } },
    { 10, "Nymph", 0, 1, 10, 1, 0, 0, 1, 0, 0, 0, 0, 265, 265, 406, 2, 0, 145, 146, 147, 2, { 1, 0, 0 }, { 5, 0, 0 }, 15, 50, 6, 0, 1, 200, 10000u, "C", { 0 } },
    { 11, "Slaughterfish", 2, 7, 11, 0, 0, 0, 0, 0, 0, 0, 0, 266, 266, 305, 1, 0, 148, 149, 150, -1, { 2, 0, 0 }, { 12, 0, 0 }, 15, 50, 7, 6, 0, 400, 0u, "", { 0 } },
    { 12, "Orc Sergeant", 0, 2, 7, 1, 0, 0, 1, 0, 1, 0, 0, 267, 267, 96, 2, 0, 151, 152, 153, -1, { 5, 0, 0 }, { 15, 0, 0 }, 15, 50, 7, 5, 1, 600, 1000u, "A", { 0 } },
    { 13, "Harpy", 1, 1, 12, 0, 0, 0, 1, 0, 0, 0, 0, 268, 268, 406, 4, 0, 154, 155, 156, 4, { 5, 0, 0 }, { 15, 0, 0 }, 16, 85, 8, 2, 0, 200, 3000u, "D", { 0 } },
    { 14, "Wereboar", 0, 2, 9, 0, 0, 0, 1, 0, 0, 0, 0, 269, 269, 96, 5, 0, 157, 158, 159, 2, { 2, 2, 5 }, { 12, 12, 15 }, 17, 66, 8, 3, 0, 560, 1000u, "", { 0 } },
    { 15, "Skeletal Warrior", 0, 3, 13, 1, 0, 0, 1, 0, 1, 0, 1, 270, 270, 306, 1, 2, 160, 161, 162, -1, { 5, 0, 0 }, { 15, 0, 0 }, 17, 66, 9, 2, 1, 80, 0u, "H", { 0 } },
    { 16, "Giant", 0, 1, 14, 1, 0, 0, 1, 0, 0, 0, 0, 271, 271, 406, 1, 0, 163, 164, 165, -1, { 10, 0, 0 }, { 30, 0, 0 }, 18, 74, 10, 3, 1, 3000, 3000u, "F", { 0 } },
    { 17, "Zombie", 0, 3, 13, 1, 0, 0, 1, 0, 0, 0, 0, 272, 272, 306, 4, 0, 166, 167, 168, -1, { 15, 0, 0 }, { 50, 0, 0 }, 52, 66, 10, 0, 1, 4000, 0u, "G", { 0 } },
    { 18, "Ghost", 3, 3, 13, 0, 0, 0, 1, 0, 0, 0, 1, 273, 273, 306, 0, 2, 169, 170, 171, 2, { 10, 0, 0 }, { 35, 0, 0 }, 17, 66, 11, 0, 1, 0, 30000u, "I", { 0 } },
    { 19, "Mummy", 0, 3, 13, 1, 0, 0, 1, 0, 0, 0, 1, 274, 274, 306, 5, 2, 172, 173, 174, 2, { 5, 0, 0 }, { 15, 0, 0 }, 17, 66, 11, 2, 1, 300, 10000u, "E", { 0 } },
    { 20, "Giant Scorpion", 0, 4, 15, 0, 0, 0, 0, 0, 0, 0, 0, 275, 275, 401, 5, 0, 175, 176, 177, -1, { 15, 0, 0 }, { 25, 0, 0 }, 18, 74, 12, 0, 0, 600, 0u, "", { 0 } },
    { 21, "Orc Shaman", 0, 2, 7, 1, 0, 0, 1, 0, 1, 0, 0, 276, 276, 96, 2, 0, 178, 179, 138, -1, { 2, 0, 0 }, { 20, 0, 0 }, 18, 74, 13, 7, 3, 400, 3000u, "U", { 0 } },
    { 22, "Gargoyle", 0, 2, 16, 1, 0, 0, 1, 0, 0, 0, 0, 277, 277, 96, 1, 0, 181, 182, 180, 5, { 10, 0, 0 }, { 15, 0, 0 }, 19, 82, 14, 0, 0, 300, 3000u, "", { 0 } },
    { 23, "Wraith", 3, 3, 13, 0, 0, 0, 1, 0, 0, 0, 1, 278, 278, 306, 0, 2, 183, 184, 185, 2, { 20, 0, 0 }, { 45, 0, 0 }, 30, 90, 15, 0, 1, 0, 30000u, "I", { 0 } },
    { 24, "Orc Warlord", 0, 2, 7, 1, 0, 0, 1, 0, 1, 0, 0, 279, 279, 96, 2, 0, 186, 187, 188, -1, { 5, 0, 0 }, { 50, 0, 0 }, 20, 90, 16, 0, 2, 700, 1000u, "T", { 0 } },
    { 25, "Frost Daedra", 0, 5, 17, 1, 0, 0, 1, 0, 1, 0, 1, 280, 280, 400, 3, 0, 189, 190, 191, 5, { 50, 0, 0 }, { 100, 0, 0 }, 25, 130, 17, -5, 0, 800, 50000u, "J", { 0 } },
    { 26, "Fire Daedra", 0, 5, 17, 1, 0, 0, 1, 0, 1, 0, 1, 281, 281, 400, 2, 0, 192, 193, 194, 5, { 15, 0, 0 }, { 50, 0, 0 }, 26, 138, 17, 1, 0, 800, 50000u, "J", { 0 } },
    { 27, "Daedroth", 0, 5, 17, 1, 0, 0, 1, 0, 1, 0, 1, 282, 282, 400, 1, 0, 195, 196, 197, 5, { 15, 0, 0 }, { 50, 0, 0 }, 27, 146, 18, 1, 0, 400, 10000u, "E", { 0 } },
    { 28, "Vampire", 0, 2, 13, 1, 0, 0, 1, 0, 0, 0, 1, 283, 283, 96, 3, 0, 198, 199, 200, 2, { 20, 0, 0 }, { 50, 0, 0 }, 28, 154, 19, -2, 3, 400, 70000u, "Q", { 0 } },
    { 29, "Daedra Seducer", 0, 5, 17, 1, 0, 0, 1, 0, 0, 0, 1, 284, 284, 400, 6, 0, 201, 202, 203, 5, { 15, 0, 0 }, { 50, 0, 0 }, 27, 146, 19, 1, 1, 200, 150000u, "Q", { 0 } },
    { 30, "Vampire Ancient", 0, 2, 13, 1, 0, 0, 1, 0, 0, 0, 1, 285, 285, 96, 3, 0, 204, 205, 206, 5, { 20, 0, 0 }, { 60, 0, 0 }, 30, 170, 20, -5, 3, 400, 100000u, "Q", { 0 } },
    { 31, "Daedra Lord", 0, 5, 17, 1, 0, 0, 1, 0, 1, 0, 1, 286, 286, 400, 4, 0, 207, 208, 209, 5, { 15, 0, 0 }, { 50, 0, 0 }, 35, 210, 20, -10, 0, 1000, 800000u, "S", { 0 } },
    { 32, "Lich", 0, 3, 13, 1, 0, 0, 1, 0, 0, 0, 1, 287, 287, 306, 2, 2, 210, 211, 212, 5, { 70, 0, 0 }, { 100, 0, 0 }, 30, 170, 20, -10, 4, 300, 100000u, "S", { 0 } },
    { 33, "Ancient Lich", 0, 3, 13, 1, 0, 0, 1, 0, 0, 0, 0, 288, 288, 306, 3, 2, 213, 214, 215, 5, { 70, 0, 0 }, { 100, 0, 0 }, 30, 170, 21, -12, 4, 300, 250000u, "S", { 0 } },
    { 34, "Dragonling", 1, 1, 18, 0, 0, 0, 0, 0, 0, 0, 0, 289, 289, 96, 0, 0, 216, 217, 218, -1, { 5, 0, 0 }, { 15, 0, 0 }, 14, 42, 16, 6, 0, 10000, 0u, "", { 0 } },
    { 35, "Fire Atronach", 0, 6, 16, 1, 0, 0, 1, 0, 0, 0, 0, 290, 290, 405, 2, 0, 219, 220, 221, -1, { 5, 0, 0 }, { 15, 0, 0 }, 25, 130, 16, 6, 0, 1000, 30000u, "", { 0 } },
    { 36, "Iron Atronach", 0, 6, 16, 1, 0, 0, 1, 0, 1, 0, 0, 291, 291, 405, 1, 0, 222, 223, 224, -1, { 5, 0, 0 }, { 15, 0, 0 }, 25, 130, 16, 6, 0, 1000, 30000u, "", { 0 } },
    { 37, "Flesh Atronach", 0, 6, 16, 1, 0, 0, 1, 0, 0, 0, 0, 292, 292, 405, 0, 0, 225, 226, 227, -1, { 5, 0, 0 }, { 15, 0, 0 }, 25, 130, 16, 6, 0, 1000, 30000u, "", { 0 } },
    { 38, "Ice Atronach", 0, 6, 16, 1, 0, 0, 1, 0, 1, 0, 0, 293, 293, 405, 3, 0, 228, 229, 230, -1, { 5, 0, 0 }, { 15, 0, 0 }, 25, 130, 16, 6, 0, 1000, 30000u, "", { 0 } },
    { 39, "Horse", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0u, "", { 0 } },
    { 40, "Dragonling", 1, 1, 18, 0, 0, 0, 0, 0, 0, 0, 0, 295, 295, 96, 0, 0, 216, 217, 218, -1, { 5, 0, 0 }, { 15, 0, 0 }, 14, 42, 16, 6, 0, 10000, 500000u, "", { 0 } },
    { 41, "Dreugh", 2, 7, 11, 0, 0, 0, 1, 0, 0, 0, 0, 296, 296, 305, 0, 0, 237, 238, 239, -1, { 5, 0, 0 }, { 15, 0, 0 }, 13, 34, 16, 6, 0, 600, 10000u, "R", { 0 } },
    { 42, "Lamia", 2, 7, 11, 0, 0, 0, 1, 0, 0, 0, 0, 297, 297, 305, 2, 0, 240, 241, 242, -1, { 5, 0, 0 }, { 15, 0, 0 }, 16, 58, 16, 6, 0, 200, 10000u, "R", { 0 } },
    { 128, "Mage", 0, 8, 19, 1, 1, 0, 1, 0, 0, 1, 0, 486, 485, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 3, 0, 0u, "U", { 0 } },
    { 129, "Spellsword", 0, 8, 19, 1, 1, 0, 1, 0, 1, 1, 0, 476, 475, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, 0u, "P", { 0 } },
    { 130, "Battlemage", 0, 8, 19, 1, 1, 1, 1, 0, 1, 1, 0, 490, 489, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 2, 0, 0u, "U", { 0 } },
    { 131, "Sorcerer", 0, 8, 19, 1, 1, 0, 1, 0, 0, 0, 0, 478, 477, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 3, 0, 0u, "U", { 0 } },
    { 132, "Healer", 0, 8, 19, 1, 1, 0, 1, 0, 0, 1, 0, 486, 485, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, 0u, "U", { 0 } },
    { 133, "Nightblade", 0, 8, 20, 1, 1, 1, 1, 0, 1, 1, 0, 490, 489, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, 0u, "U", { 0 } },
    { 134, "Bard", 0, 8, 19, 1, 1, 0, 1, 0, 1, 0, 0, 484, 483, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 2, 0, 0u, "O", { 0 } },
    { 135, "Burglar", 0, 8, 20, 1, 1, 0, 1, 0, 1, 0, 0, 484, 483, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, 0u, "O", { 0 } },
    { 136, "Rogue", 0, 8, 20, 1, 1, 0, 1, 0, 1, 0, 0, 480, 479, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, 0u, "O", { 0 } },
    { 137, "Acrobat", 0, 8, 19, 1, 1, 0, 1, 0, 1, 0, 0, 484, 483, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0u, "O", { 0 } },
    { 138, "Thief", 0, 8, 20, 1, 1, 0, 1, 0, 1, 0, 0, 484, 483, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 2, 0, 0u, "O", { 0 } },
    { 139, "Assassin", 0, 8, 20, 1, 1, 0, 1, 0, 1, 0, 0, 480, 479, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0u, "O", { 0 } },
    { 140, "Monk", 0, 8, 19, 1, 1, 0, 1, 0, 1, 0, 0, 488, 487, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, 0u, "T", { 0 } },
    { 141, "Archer", 0, 8, 19, 1, 1, 0, 1, 1, 1, 0, 0, 482, 481, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0u, "C", { 0 } },
    { 142, "Ranger", 0, 8, 19, 1, 1, 0, 1, 0, 1, 0, 0, 482, 481, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, 0u, "C", { 0 } },
    { 143, "Barbarian", 0, 8, 20, 1, 1, 0, 1, 0, 1, 0, 0, 488, 487, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0u, "T", { 0 } },
    { 144, "Warrior", 0, 8, 19, 1, 1, 0, 1, 0, 1, 0, 0, 488, 487, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0u, "T", { 0 } },
    { 145, "Knight", 0, 8, 19, 1, 1, 0, 1, 0, 1, 0, 0, 488, 487, 380, 1, 0, 243, 244, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, 0u, "T", { 0 } },
    { 146, "City Watch", 4, 8, 21, 1, 0, 0, 1, 0, 1, 0, 0, 399, 399, 380, 1, 0, 243, 456, 245, 0, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0u, "", { 0 } }
};

/*
 * RandomEncounters' tables for dungeons, by DUN_TYPE_*, then the
 * underwater one (ENCOUNTER_UNDERWATER). Twenty kinds each, weakest
 * first; the cemetery uses eleven.
 */
static const Encounter g_encounters[] = {
    /* crypt */
    { 0, 20, { 0, 15, 3, 0, 15, 3, 19, 15, 6, 17, 18, 17, 17, 18, 18, 23, 23, 28, 30, 32 } },
    /* orc stronghold */
    { 1, 20, { 0, 3, 7, 12, 4, 7, 15, 7, 12, 5, 16, 21, 6, 21, 24, 20, 27, 20, 21, 24 } },
    /* human stronghold */
    { 2, 20, { 144, 136, 0, 4, 141, 133, 3, 129, 8, 145, 144, 136, 143, 24, 141, 23, 129, 21, 144, 28 } },
    /* prison */
    { 3, 20, { 3, 0, 134, 15, 135, 6, 133, 6, 143, 138, 37, 138, 139, 17, 133, 36, 23, 18, 17, 135 } },
    /* desecrated temple */
    { 4, 20, { 3, 1, 132, 140, 131, 15, 13, 140, 19, 140, 21, 22, 23, 27, 29, 140, 25, 26, 34, 31 } },
    /* mine */
    { 5, 20, { 0, 3, 4, 6, 15, 6, 133, 7, 12, 16, 138, 16, 144, 38, 36, 26, 138, 28, 133, 30 } },
    /* natural cave */
    { 6, 20, { 0, 3, 4, 5, 4, 6, 7, 9, 143, 13, 14, 16, 20, 21, 18, 144, 140, 143, 34, 32 } },
    /* coven */
    { 7, 20, { 1, 3, 128, 131, 13, 133, 10, 37, 35, 36, 10, 27, 131, 129, 29, 26, 130, 128, 25, 31 } },
    /* vampire haunt */
    { 8, 20, { 0, 0, 3, 6, 9, 133, 15, 15, 18, 19, 23, 18, 28, 23, 28, 28, 30, 28, 30, 30 } },
    /* laboratory */
    { 9, 20, { 1, 128, 131, 130, 17, 128, 131, 37, 130, 38, 35, 36, 22, 27, 131, 32, 128, 131, 32, 33 } },
    /* harpy nest */
    { 10, 20, { 0, 144, 3, 13, 3, 13, 15, 6, 135, 13, 20, 21, 133, 13, 136, 28, 29, 13, 29, 31 } },
    /* ruined castle */
    { 11, 20, { 0, 3, 144, 7, 15, 6, 12, 9, 145, 14, 17, 16, 145, 18, 23, 144, 145, 32, 33, 30 } },
    /* spider nest */
    { 12, 20, { 0, 3, 3, 6, 15, 2, 142, 6, 20, 136, 13, 6, 138, 18, 19, 6, 23, 6, 139, 32 } },
    /* giant stronghold */
    { 13, 20, { 0, 4, 5, 7, 16, 12, 20, 16, 14, 16, 24, 20, 21, 16, 27, 26, 34, 16, 24, 25 } },
    /* dragon's den */
    { 14, 20, { 0, 3, 8, 135, 20, 9, 145, 13, 6, 22, 131, 10, 133, 128, 145, 28, 130, 138, 33, 31 } },
    /* barbarian stronghold */
    { 15, 20, { 144, 143, 0, 3, 144, 8, 136, 141, 9, 143, 141, 14, 6, 144, 28, 136, 143, 30, 131, 33 } },
    /* volcanic caves */
    { 16, 20, { 3, 1, 5, 5, 143, 35, 13, 14, 16, 18, 20, 36, 35, 128, 36, 23, 27, 26, 31, 33 } },
    /* scorpion nest */
    { 17, 20, { 0, 3, 6, 15, 20, 138, 20, 17, 1, 18, 20, 22, 23, 132, 20, 133, 29, 20, 27, 31 } },
    /* cemetery: eleven */
    { 18, 11, { 0, 15, 3, 19, 6, 17, 18, 23, 28, 30, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    /* underwater */
    { 19, 20, { 11, 11, 15, 11, 11, 41, 41, 11, 17, 41, 11, 38, 11, 41, 42, 41, 42, 17, 23, 18 } }
};

/*
 * The Chronicles' odds: six climate groups, four seasons each in
 * SEASON_* order (fall, spring, summer, winter), seven kinds each in the
 * engine's order: sunny, cloudy, overcast, fog, rain, thunder, snow.
 */
enum { G_DESERT, G_MOUNTAINS, G_JUNGLE, G_SWAMP, G_SUBTROPICAL, G_WOODLANDS };

static const df_u8 g_weather[6][4][WEATHER_KINDS] = {
    /* Desert */
    { { 80, 15, 0, 0, 3, 2, 0 }, { 75, 15, 0, 0, 5, 5, 0 },
      { 85, 15, 0, 0, 0, 0, 0 }, { 75, 15, 0, 3, 5, 2, 0 } },
    /* Mountains */
    { { 30, 18, 20, 2, 20, 10, 0 }, { 30, 23, 15, 2, 20, 10, 0 },
      { 45, 25, 15, 0, 10, 5, 0 }, { 18, 20, 25, 2, 0, 0, 35 } },
    /* Jungle (rainforest) */
    { { 20, 20, 20, 0, 25, 15, 0 }, { 20, 15, 10, 3, 37, 15, 0 },
      { 35, 20, 10, 0, 25, 10, 0 }, { 15, 20, 25, 3, 25, 12, 0 } },
    /* Swamp, and the sea */
    { { 15, 15, 15, 20, 20, 15, 0 }, { 10, 10, 20, 20, 25, 15, 0 },
      { 25, 15, 15, 15, 20, 10, 0 }, { 15, 20, 25, 25, 0, 0, 15 } },
    /* Subtropical */
    { { 25, 20, 15, 0, 25, 15, 0 }, { 30, 15, 10, 3, 27, 15, 0 },
      { 40, 15, 10, 0, 20, 15, 0 }, { 20, 20, 20, 5, 25, 10, 0 } },
    /* Woodlands */
    { { 25, 15, 20, 10, 20, 10, 0 }, { 35, 15, 10, 5, 25, 10, 0 },
      { 60, 20, 5, 0, 10, 5, 0 }, { 25, 15, 20, 5, 10, 0, 25 } },
};

static int group(int climate)
{
    switch (climate) {
    case CLIMATE_DESERT:
    case CLIMATE_DESERT2:
        return G_DESERT;
    case CLIMATE_MOUNTAIN:
    case CLIMATE_MOUNTAIN_WOODS:
        return G_MOUNTAINS;
    case CLIMATE_RAINFOREST:
        return G_JUNGLE;
    case CLIMATE_OCEAN:
    case CLIMATE_SWAMP:
        return G_SWAMP;
    case CLIMATE_SUBTROPICAL:
        return G_SUBTROPICAL;
    default:
        return G_WOODLANDS;
    }
}

/* The climate ids WTHR has a record for: every one a map pixel holds. */
static const int g_climates[] = {
    CLIMATE_OCEAN, CLIMATE_DESERT, CLIMATE_DESERT2, CLIMATE_MOUNTAIN, CLIMATE_RAINFOREST,
    CLIMATE_SWAMP, CLIMATE_SUBTROPICAL, CLIMATE_MOUNTAIN_WOODS, CLIMATE_WOODLANDS, CLIMATE_HAUNTED
};

/*
 * DaggerfallDateTime: twelve months of thirty days, their seasons
 * (winter, winter, spring x3, summer x3, fall x3, winter); a week of
 * seven days that starts again each month; a new game on the 1st of
 * Midyear, 3E 405.
 */
static const CalMonth g_months[] = {
    { 0, "Morning Star", 30, CLOCK_WINTER },
    { 1, "Sun's Dawn", 30, CLOCK_WINTER },
    { 2, "First Seed", 30, CLOCK_SPRING },
    { 3, "Rain's Hand", 30, CLOCK_SPRING },
    { 4, "Second Seed", 30, CLOCK_SPRING },
    { 5, "Midyear", 30, CLOCK_SUMMER },
    { 6, "Sun's Height", 30, CLOCK_SUMMER },
    { 7, "Last Seed", 30, CLOCK_SUMMER },
    { 8, "Hearthfire", 30, CLOCK_FALL },
    { 9, "Frostfall", 30, CLOCK_FALL },
    { 10, "Sun's Dusk", 30, CLOCK_FALL },
    { 11, "Evening Star", 30, CLOCK_WINTER }
};

static const CalWeekday g_weekdays[] = {
    { 0, "Sundas" }, { 1, "Morndas" }, { 2, "Tirdas" }, { 3, "Middas" },
    { 4, "Turdas" }, { 5, "Fredas" }, { 6, "Loredas" }
};

static const CalInfo g_calendar = { 0, "3E", 1, 405, 5, 0 };

const CalMonth *classic_months(int *n)
{
    *n = (int)(sizeof(g_months) / sizeof(g_months[0]));
    return g_months;
}

const CalWeekday *classic_weekdays(int *n)
{
    *n = (int)(sizeof(g_weekdays) / sizeof(g_weekdays[0]));
    return g_weekdays;
}

const CalInfo *classic_calendar(void)
{
    return &g_calendar;
}

/*
 * RaceTemplate's eight races, by DFU's ids: TEXT.RSC's description, the
 * clip at their choosing, the paper doll's pictures, and what they resist
 * (Bretons magic, Nords frost) or are immune to (High Elves paralysis).
 * Their other gifts (an Argonian's water breathing, infravision) come
 * with magic.
 */
static const CareerRace g_races[] = {
    { 1, "Breton", 2003, 209, "SCBG00I0.IMG", "BODY00I0.IMG", "BODY00I1.IMG", "BODY10I0.IMG", "BODY10I1.IMG",
      "FACE00I0.CIF", "FACE10I0.CIF", EFFECT_MAGIC, 0, 0, 0, 0 },
    { 2, "Redguard", 2002, 210, "SCBG01I0.IMG", "BODY01I0.IMG", "BODY01I1.IMG", "BODY11I0.IMG", "BODY11I1.IMG",
      "FACE01I0.CIF", "FACE11I0.CIF", 0, 0, 0, 0, 0 },
    { 3, "Nord", 2000, 211, "SCBG02I0.IMG", "BODY02I0.IMG", "BODY02I1.IMG", "BODY12I0.IMG", "BODY12I1.IMG",
      "FACE02I0.CIF", "FACE12I0.CIF", EFFECT_FROST, 0, 0, 0, 0 },
    { 4, "Dark Elf", 2007, 212, "SCBG03I0.IMG", "BODY03I0.IMG", "BODY03I1.IMG", "BODY13I0.IMG", "BODY13I1.IMG",
      "FACE03I0.CIF", "FACE13I0.CIF", 0, 0, 0, 0, 0 },
    { 5, "High Elf", 2006, 213, "SCBG04I0.IMG", "BODY04I0.IMG", "BODY04I1.IMG", "BODY14I0.IMG", "BODY14I1.IMG",
      "FACE04I0.CIF", "FACE14I0.CIF", 0, EFFECT_PARALYSIS, 0, 0, 0 },
    { 6, "Wood Elf", 2005, 214, "SCBG05I0.IMG", "BODY05I0.IMG", "BODY05I1.IMG", "BODY15I0.IMG", "BODY15I1.IMG",
      "FACE05I0.CIF", "FACE15I0.CIF", 0, 0, 0, 0, 0 },
    { 7, "Khajiit", 2001, 215, "SCBG06I0.IMG", "BODY06I0.IMG", "BODY06I1.IMG", "BODY16I0.IMG", "BODY16I1.IMG",
      "FACE06I0.CIF", "FACE16I0.CIF", 0, 0, 0, 0, 0 },
    { 8, "Argonian", 2004, 216, "SCBG07I0.IMG", "BODY07I0.IMG", "BODY07I1.IMG", "BODY17I0.IMG", "BODY17I1.IMG",
      "FACE07I0.CIF", "FACE17I0.CIF", 0, 0, 0, 0, 0 }
};

/*
 * The 35 skills (DFCareer.Skills): each one's governing attribute
 * (DaggerfallSkills.GetPrimaryStat) and how hard it is to advance
 * (GetAdvancementMultiplier).
 */
static const CareerSkill g_skills[] = {
    { 0, "Medical", ATTR_INTELLIGENCE, 12 },
    { 1, "Etiquette", ATTR_PERSONALITY, 1 },
    { 2, "Streetwise", ATTR_PERSONALITY, 1 },
    { 3, "Jumping", ATTR_STRENGTH, 5 },
    { 4, "Orcish", ATTR_INTELLIGENCE, 15 },
    { 5, "Harpy", ATTR_INTELLIGENCE, 15 },
    { 6, "Giantish", ATTR_INTELLIGENCE, 15 },
    { 7, "Dragonish", ATTR_INTELLIGENCE, 15 },
    { 8, "Nymph", ATTR_INTELLIGENCE, 15 },
    { 9, "Daedric", ATTR_INTELLIGENCE, 15 },
    { 10, "Spriggan", ATTR_INTELLIGENCE, 15 },
    { 11, "Centaurian", ATTR_INTELLIGENCE, 15 },
    { 12, "Impish", ATTR_INTELLIGENCE, 15 },
    { 13, "Lockpicking", ATTR_INTELLIGENCE, 2 },
    { 14, "Mercantile", ATTR_PERSONALITY, 1 },
    { 15, "Pickpocket", ATTR_AGILITY, 2 },
    { 16, "Stealth", ATTR_AGILITY, 2 },
    { 17, "Swimming", ATTR_ENDURANCE, 1 },
    { 18, "Climbing", ATTR_STRENGTH, 2 },
    { 19, "Backstabbing", ATTR_AGILITY, 1 },
    { 20, "Dodging", ATTR_SPEED, 4 },
    { 21, "Running", ATTR_SPEED, 50 },
    { 22, "Destruction", ATTR_WILLPOWER, 1 },
    { 23, "Restoration", ATTR_WILLPOWER, 2 },
    { 24, "Illusion", ATTR_WILLPOWER, 1 },
    { 25, "Alteration", ATTR_WILLPOWER, 1 },
    { 26, "Thaumaturgy", ATTR_WILLPOWER, 2 },
    { 27, "Mysticism", ATTR_WILLPOWER, 1 },
    { 28, "Short Blade", ATTR_AGILITY, 2 },
    { 29, "Long Blade", ATTR_AGILITY, 2 },
    { 30, "Hand-to-Hand", ATTR_AGILITY, 2 },
    { 31, "Axe", ATTR_STRENGTH, 2 },
    { 32, "Blunt Weapon", ATTR_STRENGTH, 2 },
    { 33, "Archery", ATTR_AGILITY, 1 },
    { 34, "Critical Strike", ATTR_AGILITY, 8 }
};

const CareerRace *classic_races(int *n)
{
    *n = (int)(sizeof(g_races) / sizeof(g_races[0]));
    return g_races;
}

const CareerSkill *classic_skills(int *n)
{
    *n = (int)(sizeof(g_skills) / sizeof(g_skills[0]));
    return g_skills;
}

/*
 * DFU's ItemTemplates: the small cart and the horse (93, 94), the armour
 * (102..112), the weapons (113..131, the arrow last), the letter of credit
 * (275) and gold (276): name, weight in kg, price, hit points, the
 * picture on the ground and on the paper doll, one-handed, blunt, and the
 * paper doll's draw order.
 *
 * An armour piece's paper doll archive here is the female Argonian's,
 * 245: the wearer's sex and body shape move it (item_doll_picture).
 */
static const ItemTemplate g_items[] = {
    { 93, "Small Cart", 400.0f, 150, 0, 0, 0, 213, 1, 0, 0, 0 },
    { 94, "Horse", 800.0f, 1000, 0, 0, 0, 201, 0, 0, 0, 0 },
    { 102, "Cuirass", 12.5f, 100, 4096, 207, 11, 245, 3, 0, 0, 50 },
    { 103, "Gauntlets", 1.25f, 50, 2048, 207, 13, 245, 8, 0, 0, 70 },
    { 104, "Greaves", 2.5f, 80, 3072, 207, 13, 245, 10, 0, 0, 30 },
    { 105, "Left Pauldron", 2.0f, 60, 2048, 207, 13, 245, 17, 0, 0, 60 },
    { 106, "Right Pauldron", 2.0f, 60, 2048, 207, 13, 245, 22, 0, 0, 60 },
    { 107, "Helm", 2.5f, 80, 2560, 207, 12, 245, 27, 0, 0, 0 },
    { 108, "Boots", 2.0f, 50, 2560, 204, 1, 245, 0, 0, 0, 20 },
    { 109, "Buckler", 2.0f, 10, 512, 207, 10, 245, 33, 0, 0, 110 },
    { 110, "Round Shield", 4.5f, 20, 1280, 207, 10, 245, 34, 0, 0, 110 },
    { 111, "Kite Shield", 7.5f, 30, 2048, 207, 9, 245, 35, 0, 0, 110 },
    { 112, "Tower Shield", 12.5f, 50, 2560, 207, 9, 245, 36, 0, 0, 110 },
    { 113, "Dagger", 0.50f, 1, 50, 207, 5, 234, 5, 1, 0, 100 },
    { 114, "Tanto", 0.75f, 3, 50, 207, 10, 234, 22, 1, 0, 100 },
    { 115, "Staff", 2.00f, 5, 300, 207, 7, 234, 21, 0, 1, 100 },
    { 116, "Shortsword", 2.50f, 5, 300, 207, 3, 234, 19, 1, 0, 100 },
    { 117, "Wakizashi", 2.00f, 8, 200, 207, 2, 234, 26, 1, 0, 100 },
    { 118, "Broadsword", 5.00f, 10, 600, 207, 2, 234, 2, 1, 0, 100 },
    { 119, "Saber", 3.75f, 12, 700, 207, 2, 234, 17, 1, 0, 100 },
    { 120, "Longsword", 4.50f, 15, 800, 207, 2, 234, 12, 1, 0, 100 },
    { 121, "Katana", 2.50f, 25, 600, 207, 15, 234, 9, 1, 0, 100 },
    { 122, "Claymore", 7.50f, 30, 1400, 207, 2, 234, 4, 0, 0, 100 },
    { 123, "Dai-katana", 3.50f, 50, 800, 207, 2, 234, 7, 0, 0, 100 },
    { 124, "Mace", 4.50f, 10, 800, 207, 6, 234, 14, 1, 1, 100 },
    { 125, "Flail", 7.00f, 15, 1000, 207, 6, 234, 8, 0, 1, 100 },
    { 126, "Warhammer", 7.00f, 20, 1600, 214, 3, 234, 25, 0, 1, 100 },
    { 127, "Battle Axe", 6.00f, 20, 1200, 207, 4, 234, 0, 1, 0, 100 },
    { 128, "War Axe", 7.50f, 20, 800, 207, 4, 234, 24, 0, 0, 100 },
    { 129, "Short Bow", 1.00f, 10, 50, 207, 8, 234, 16, 0, 0, 100 },
    { 130, "Long Bow", 1.50f, 20, 100, 207, 8, 234, 11, 0, 0, 100 },
    { 131, "Arrow", 0.25f, 2, 1, 207, 16, 0, 0, 0, 0, 0 },
    { 275, "Letter of Credit", 0.25f, 0, 0, 209, 7, 0, 0, 0, 0, 0 },
    { 276, "Gold Pieces", 0.0025f, 1, 0, 216, 1, 0, 0, 0, 0, 0 },
    /* Ours: a quest's thing to carry, worth nothing to a shop (items.h, ITEM_QUEST). */
    { 999, "Parcel", 0.25f, 0, 0, 209, 7, 0, 0, 0, 0, 0 }
};

/*
 * FormulaHelper.CalculateWeaponMin/MaxDamage, DaggerfallUnityItem's
 * weapon skills (short blade 28, long blade 29, axe 31, blunt 32,
 * archery 33), and WeaponBasics' swing files (WEAPON##.CIF: a dagger 02,
 * a long blade 04, a staff 01, a mace 05, a flail 06, a warhammer 07, an
 * axe 08, a bow 09). The arrow is no weapon of its own.
 */
static const WeaponTemplate g_weapons[] = {
    { 113, 1, 6, 28, 2 },
    { 114, 1, 8, 28, 2 },
    { 115, 1, 8, 32, 1 },
    { 116, 1, 8, 28, 2 },
    { 117, 1, 10, 28, 2 },
    { 118, 1, 12, 29, 4 },
    { 119, 3, 12, 29, 4 },
    { 120, 2, 16, 29, 4 },
    { 121, 3, 16, 29, 4 },
    { 122, 2, 18, 29, 4 },
    { 123, 3, 21, 29, 4 },
    { 124, 1, 12, 32, 5 },
    { 125, 2, 14, 32, 6 },
    { 126, 3, 18, 32, 7 },
    { 127, 2, 12, 31, 8 },
    { 128, 2, 16, 31, 8 },
    { 129, 4, 16, 33, 9 },
    { 130, 4, 18, 33, 9 }
};

/*
 * ItemBuilder and GetWeaponMaterialModifier: iron to daedric, then the
 * two only armour comes in. The damage a material adds (ten times it to
 * hit), its weight, value and condition multipliers, the value's other
 * multiplier, and the armour it gives (GetMaterialArmorValue).
 *
 * Leather's weight_q of 2 is ItemBuilder's halved weight: the same
 * rounding in quarter kilograms. Leather and chain keep the template's
 * condition, and leather its price; chain's price is doubled.
 */
static const Material g_materials[] = {
    { 0, "Iron", -1, 4, 1, 4, 3, 7 },
    { 1, "Steel", 0, 5, 2, 6, 3, 9 },
    { 2, "Silver", 0, 4, 4, 6, 3, 9 },
    { 3, "Elven", 1, 4, 8, 8, 3, 11 },
    { 4, "Dwarven", 2, 3, 16, 12, 3, 13 },
    { 5, "Mithril", 3, 4, 32, 16, 3, 15 },
    { 6, "Adamantium", 3, 4, 64, 20, 3, 15 },
    { 7, "Ebony", 4, 2, 128, 24, 3, 17 },
    { 8, "Orcish", 5, 4, 256, 28, 3, 19 },
    { 9, "Daedric", 6, 5, 512, 32, 3, 21 },
    { 10, "Leather", 0, 2, 1, 4, 1, 3 },
    { 11, "Chain", 0, 4, 1, 4, 2, 6 }
};

/*
 * What armour adds (ItemEquipTable, ItemBuilder.SetVariant,
 * DaggerfallUnityItem's shield rules). The columns: part, shield, shield
 * parts, then the variants in plate, leather and chain.
 */
#define SHIELD_SMALL ((1 << BODY_LEFT_ARM) | (1 << BODY_HANDS))
#define SHIELD_MIDDLE (SHIELD_SMALL | (1 << BODY_LEGS))
#define SHIELD_TOWER (SHIELD_MIDDLE | (1 << BODY_HEAD))
static const ArmorTemplate g_armors[] = {
    { ITEM_CUIRASS, BODY_CHEST, 0, 0, 1, 3, 0, 0, 4, 4 },
    { ITEM_GAUNTLETS, BODY_HANDS, 0, 0, 1, 1, 0, 0, 1, 1 },
    { ITEM_GREAVES, BODY_LEGS, 0, 0, 2, 5, 0, 1, 6, 6 },
    { ITEM_LEFT_PAULDRON, BODY_LEFT_ARM, 0, 0, 1, 3, 0, 0, 4, 4 },
    { ITEM_RIGHT_PAULDRON, BODY_RIGHT_ARM, 0, 0, 1, 3, 0, 0, 4, 4 },
    { ITEM_HELM, BODY_HEAD, 0, 0, 0, 5, 0, 5, 0, 5 },
    { ITEM_BOOTS, BODY_FEET, 0, 0, 1, 2, 0, 0, 1, 2 },
    { ITEM_BUCKLER, BODY_LEFT_ARM, 1, SHIELD_SMALL, 0, 0, 0, 0, 0, 0 },
    { ITEM_ROUND_SHIELD, BODY_LEFT_ARM, 2, SHIELD_MIDDLE, 0, 0, 0, 0, 0, 0 },
    { ITEM_KITE_SHIELD, BODY_LEFT_ARM, 3, SHIELD_MIDDLE, 0, 0, 0, 0, 0, 0 },
    { ITEM_TOWER_SHIELD, BODY_LEFT_ARM, 4, SHIELD_TOWER, 0, 0, 0, 0, 0, 0 }
};

/*
 * LootTables.DefaultLootTables. The columns: letter, least and most gold,
 * armour, weapon, magic, clothing, book, plant 1 and 2, creature 1, 2 and
 * 3, misc 1 and 2, religious.
 */
static const LootChance g_loots[] = {
    { 'A', 1, 10, 5, 5, 2, 4, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 'B', 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0 },
    { 'C', 2, 20, 5, 25, 3, 0, 2, 10, 10, 5, 5, 5, 5, 2, 2 },
    { 'D', 1, 4, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 0, 4 },
    { 'E', 20, 80, 10, 10, 3, 4, 2, 0, 0, 0, 0, 0, 0, 1, 15 },
    { 'F', 4, 30, 50, 50, 1, 0, 0, 2, 2, 5, 5, 5, 2, 3, 0 },
    { 'G', 3, 15, 50, 50, 1, 5, 0, 0, 0, 0, 0, 0, 0, 3, 0 },
    { 'H', 2, 10, 0, 100, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 'I', 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5 },
    { 'J', 50, 150, 5, 5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 'K', 1, 10, 5, 5, 3, 0, 5, 3, 3, 3, 3, 3, 3, 2, 100 },
    { 'L', 1, 20, 50, 50, 1, 75, 0, 0, 0, 3, 3, 3, 3, 5, 3 },
    { 'M', 1, 15, 10, 10, 1, 15, 2, 1, 1, 1, 1, 1, 2, 3, 1 },
    { 'N', 1, 80, 5, 5, 1, 20, 5, 5, 5, 5, 5, 5, 5, 2, 5 },
    { 'O', 5, 20, 10, 15, 2, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0 },
    { 'P', 5, 20, 5, 10, 2, 0, 10, 5, 5, 5, 5, 5, 5, 5, 0 },
    { 'Q', 20, 80, 10, 25, 3, 35, 5, 2, 2, 8, 8, 8, 2, 3, 0 },
    { 'R', 5, 20, 5, 15, 2, 0, 0, 0, 0, 3, 3, 3, 5, 0, 0 },
    { 'S', 50, 125, 10, 10, 3, 0, 5, 5, 5, 5, 5, 5, 15, 5, 0 },
    { 'T', 20, 80, 100, 100, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 'U', 7, 30, 10, 10, 2, 0, 2, 5, 5, 5, 5, 5, 10, 2, 10 }
};

/*
 * LootTables' key for each dungeon type, by DFU's DFRegion.DungeonTypes:
 * crypt, orc and human strongholds, prison, desecrated temple, mine,
 * natural cave, coven, vampire haunt, laboratory, harpy nest, ruined
 * castle, spider nest, giant stronghold, dragon's den, barbarian
 * stronghold, volcanic caves, scorpion nest, cemetery.
 */
static const DungeonLoot g_dungeon_loots[] = {
    { 0, 'K' }, { 1, 'N' }, { 2, 'N' }, { 3, 'N' }, { 4, 'K' }, { 5, 'M' }, { 6, 'M' }, { 7, 'Q' }, { 8, 'K' }, { 9, 'U' },
    { 10, 'D' }, { 11, 'N' }, { 12, 'L' }, { 13, 'F' }, { 14, 'S' }, { 15, 'N' }, { 16, 'M' }, { 17, 'L' }, { 18, 'N' }
};

const DungeonLoot *classic_dungeon_loots(int *n)
{
    *n = (int)(sizeof(g_dungeon_loots) / sizeof(g_dungeon_loots[0]));
    return g_dungeon_loots;
}

const ItemTemplate *classic_items(int *n)
{
    *n = (int)(sizeof(g_items) / sizeof(g_items[0]));
    return g_items;
}

const WeaponTemplate *classic_weapons(int *n)
{
    *n = (int)(sizeof(g_weapons) / sizeof(g_weapons[0]));
    return g_weapons;
}

const ArmorTemplate *classic_armors(int *n)
{
    *n = (int)(sizeof(g_armors) / sizeof(g_armors[0]));
    return g_armors;
}

const LootChance *classic_loots(int *n)
{
    *n = (int)(sizeof(g_loots) / sizeof(g_loots[0]));
    return g_loots;
}

const Material *classic_materials(int *n)
{
    *n = (int)(sizeof(g_materials) / sizeof(g_materials[0]));
    return g_materials;
}

const EnemyKind *classic_enemies(int *n)
{
    *n = (int)(sizeof(g_enemies) / sizeof(g_enemies[0]));
    return g_enemies;
}

const Encounter *classic_encounters(int *n)
{
    *n = (int)(sizeof(g_encounters) / sizeof(g_encounters[0]));
    return g_encounters;
}

int classic_weather_odds(WeatherOdds *out, int cap)
{
    int n;
    int c;
    int s;
    int k;

    n = 0;
    for (c = 0; c < (int)(sizeof(g_climates) / sizeof(g_climates[0])); c++) {
        for (s = 0; s < 4 && n < cap; s++) {
            out[n].id = (df_u16)forecast_id(g_climates[c], s);
            for (k = 0; k < WEATHER_KINDS; k++) {
                out[n].odds[k] = g_weather[group(g_climates[c])][s][k];
            }
            n++;
        }
    }
    return n;
}

/*
 * A block's building types (DFU's DFLocation.BuildingTypes), with PlayerActivate's
 * opening hours and DaggerfallLootDataTables' stock chances for weapons and armour,
 * all the game has items for yet. That table goes by item group: 2 is armour and 3
 * weapons (ItemGroups, as BIOG's IT lines number them), so an armorer's 80 is armour.
 */
static const BuildingKind g_buildings[] = {
    { 0, "Alchemist", 7, 22, 0, 0 },
    { 1, "House for sale", 8, 16, 0, 0 },
    { 2, "Armorer", 9, 19, 20, 80 },
    { 3, "Bank", 8, 15, 0, 0 },
    { 4, "Town", 0, 25, 0, 0 },
    { 5, "Bookseller", 9, 21, 0, 0 },
    { 6, "Clothing store", 10, 19, 0, 0 },
    { 7, "Furniture store", 10, 20, 0, 0 },
    { 8, "Gem store", 9, 18, 0, 0 },
    { 9, "General store", 6, 23, 20, 0 },
    { 10, "Library", 9, 23, 0, 0 },
    { 11, "Guild hall", 11, 23, 0, 0 },
    { 12, "Pawn shop", 9, 20, 10, 10 },
    { 13, "Weapon smith", 9, 20, 70, 30 },
    { 14, "Temple", 0, 25, 0, 0 },
    { 15, "Tavern", 0, 25, 0, 0 },
    { 16, "Palace", 10, 16, 0, 0 },
    { 17, "House", 0, 0, 0, 0 },
    { 18, "House", 6, 18, 0, 0 },
    { 19, "House", 6, 18, 0, 0 },
    { 20, "House", 6, 18, 0, 0 },
    { 21, "House", 6, 18, 0, 0 },
    { 22, "House", 6, 18, 0, 0 },
    { 23, "Town", 6, 18, 0, 0 },
    { 24, "Ship", 0, 25, 0, 0 }
};

const BuildingKind *classic_buildings(int *n)
{
    *n = (int)(sizeof(g_buildings) / sizeof(g_buildings[0]));
    return g_buildings;
}

/* PlayerEntity's reputationLossPerCrime, and DaggerfallCourtWindow's sentence: per point, base, low, high. */
static const CrimeKind g_crimes[] = {
    { 0, "", 0, 0, 0, 0, 0 },
    { 1, "attempted breaking and entering", 10, 5, 300, 50, 1000 },
    { 2, "trespassing", 5, 5, 200, 10, 800 },
    { 3, "breaking and entering", 10, 6, 600, 80, 1200 },
    { 4, "assault", 8, 6, 1000, 10, 1500 },
    { 5, "murder", 20, 10, 10000, 9000, 12000 },
    { 6, "tax evasion", 10, 5, 200, 10, 12000 },
    { 7, "criminal conspiracy", 2, 5, 500, 10, 1500 },
    { 8, "vagrancy", 1, 3, 100, 2, 700 },
    { 9, "smuggling", 2, 8, 500, 10, 1500 },
    { 10, "piracy", 2, 8, 500, 10, 1500 },
    { 11, "high treason", 75, 9, 1200, 160, 2000 },
    { 12, "pickpocketing", 2, 6, 200, 5, 1000 },
    { 13, "theft", 8, 0, 200, 5, 1000 },
    { 14, "treason", 36, 8, 1000, 10, 1500 },
    { 15, "loan default", 10, 0, 100, 4, 700 }
};

const CrimeKind *classic_crimes(int *n)
{
    *n = (int)(sizeof(g_crimes) / sizeof(g_crimes[0]));
    return g_crimes;
}

/*
 * The holidays every city keeps (UESP's Holidays), by day of the year, month x 30
 * + day: Morning Star is month 0. What each does is what the game has the means
 * for: shops shut or at half, weapons at half, a temple's free cure or half its
 * donation, an inn's free room, free drinks.
 */
#define HDAY(month, day) ((month) * 30 + (day))
static const Holiday g_holidays[] = {
    { HDAY(0, 1), "New Life Festival", HOLIDAY_FREE_DRINK },
    { HDAY(0, 15), "South Wind's Prayer", HOLIDAY_TEMPLE_CURE },
    { HDAY(1, 16), "Heart's Day", HOLIDAY_FREE_ROOM },
    { HDAY(2, 7), "First Planting", HOLIDAY_TEMPLE_CURE },
    { HDAY(3, 28), "Jester's Day", 0 },
    { HDAY(4, 7), "Second Planting", HOLIDAY_TEMPLE_CURE },
    { HDAY(5, 16), "Mid Year Celebration", HOLIDAY_TEMPLE_HALF },
    { HDAY(6, 10), "Merchants' Festival", HOLIDAY_SHOPS_HALF },
    { HDAY(6, 20), "Sun's Rest", HOLIDAY_SHOPS_SHUT },
    { HDAY(7, 27), "Harvest's End", HOLIDAY_FOOD_HALF },
    { HDAY(8, 3), "Tales and Tallows", 0 },
    { HDAY(9, 13), "Witches Festival", 0 },
    { HDAY(9, 30), "Emperor's Day", 0 },
    { HDAY(10, 20), "Warriors Festival", HOLIDAY_WEAPONS_HALF },
    { HDAY(11, 15), "North Wind's Prayer", HOLIDAY_TEMPLE_HALF },
    { HDAY(11, 30), "Old Life Festival", 0 }
};
#undef HDAY

const Holiday *classic_holidays(int *n)
{
    *n = (int)(sizeof(g_holidays) / sizeof(g_holidays[0]));
    return g_holidays;
}

/*
 * The diseases (DFU's DiseaseEffect.diseaseDataSources, classic's numbers):
 * each attribute's weight STR INT WIL AGI END PER SPD LUC, then health,
 * fatigue and spell points, the day's damage range, and the days the
 * symptoms last (DISEASE_FOREVER: until cured).
 */
#define F DISEASE_FOREVER
static const Disease g_diseases[] = {
    { DISEASE_WITCHES_POX, "Witches' Pox", { 1, 0, 0, 0, 1, 0, 0, 0 }, 1, 0, 0, 2, 10, F, F },
    { DISEASE_PLAGUE, "Plague", { 1, 0, 1, 1, 1, 1, 1, 1 }, 1, 1, 1, 3, 30, F, F },
    { DISEASE_YELLOW_FEVER, "Yellow Fever", { 0, 0, 1, 0, 1, 0, 0, 0 }, 1, 0, 0, 5, 10, F, F },
    { DISEASE_STOMACH_ROT, "Stomach Rot", { 0, 0, 0, 0, 0, 0, 0, 0 }, 1, 0, 0, 1, 5, F, F },
    { DISEASE_CONSUMPTION, "Consumption", { 1, 0, 1, 1, 0, 0, 0, 0 }, 0, 0, 0, 2, 10, F, F },
    { DISEASE_BRAIN_FEVER, "Brain Fever", { 0, 0, 1, 0, 0, 1, 0, 0 }, 1, 0, 0, 1, 5, F, F },
    { DISEASE_SWAMP_ROT, "Swamp Rot", { 1, 0, 1, 1, 0, 0, 0, 0 }, 0, 0, 0, 2, 10, F, F },
    { DISEASE_CALIRONS_CURSE, "Caliron's Curse", { 1, 0, 0, 1, 0, 0, 1, 0 }, 0, 0, 0, 5, 10, 3, 18 },
    { DISEASE_CHOLERA, "Cholera", { 1, 1, 1, 1, 1, 1, 1, 1 }, 1, 1, 1, 5, 30, F, F },
    { DISEASE_LEPROSY, "Leprosy", { 1, 1, 1, 1, 1, 1, 1, 1 }, 1, 1, 0, 5, 30, F, F },
    { DISEASE_WOUND_ROT, "Wound Rot", { 1, 0, 0, 0, 1, 0, 0, 0 }, 1, 0, 0, 2, 4, F, F },
    { DISEASE_RED_DEATH, "Red Death", { 0, 0, 0, 0, 1, 1, 0, 0 }, 0, 1, 0, 2, 10, F, F },
    { DISEASE_BLOOD_ROT, "Blood Rot", { 0, 0, 1, 0, 0, 1, 0, 0 }, 1, 0, 0, 5, 10, 3, 18 },
    { DISEASE_TYPHOID_FEVER, "Typhoid Fever", { 0, 1, 0, 0, 1, 0, 0, 0 }, 1, 0, 0, 2, 10, F, F },
    { DISEASE_DEMENTIA, "Dementia", { 0, 1, 1, 0, 0, 1, 0, 0 }, 0, 0, 0, 2, 10, F, F },
    { DISEASE_CHRONDIASIS, "Chrondiasis", { 0, 1, 0, 0, 0, 0, 0, 0 }, 0, 0, 1, 5, 10, F, F },
    { DISEASE_WIZARD_FEVER, "Wizard Fever", { 0, 1, 0, 0, 0, 0, 0, 0 }, 0, 0, 1, 2, 4, 3, 18 }
};
#undef F

const Disease *classic_diseases(int *n)
{
    *n = (int)(sizeof(g_diseases) / sizeof(g_diseases[0]));
    return g_diseases;
}

/*
 * The guilds one can join (DFU's FightersGuild, MagesGuild, Temple and
 * KnightlyOrder): their ranks' titles (DFU's Internal_Strings), the skills
 * their ranks ask for and those they train, what they give, the rank a temple
 * heals from (its RankData's healing), and TEXT.RSC's words. A knightly
 * order trains nothing; its free rooms come at rank 4, or in its own region.
 */
/* DFU's DFCareer.Skills, as the lists name them; E ends a list. */
enum {
    K_MEDICAL,
    K_ETIQUETTE,
    K_STREETWISE,
    K_JUMPING,
    K_ORCISH,
    K_HARPY,
    K_GIANTISH,
    K_DRAGONISH,
    K_NYMPH,
    K_DAEDRIC,
    K_SPRIGGAN,
    K_CENTAURIAN,
    K_IMPISH,
    K_LOCKPICKING,
    K_MERCANTILE,
    K_PICKPOCKET,
    K_STEALTH,
    K_SWIMMING,
    K_CLIMBING,
    K_BACKSTABBING,
    K_DODGING,
    K_RUNNING,
    K_DESTRUCTION,
    K_RESTORATION,
    K_ILLUSION,
    K_ALTERATION,
    K_THAUMATURGY,
    K_MYSTICISM,
    K_SHORT_BLADE,
    K_LONG_BLADE,
    K_HAND_TO_HAND,
    K_AXE,
    K_BLUNT,
    K_ARCHERY,
    K_CRITICAL
};
#define E GUILD_NO_SKILL
static const GuildKind g_guilds[] = {
    /* The Fighters Guild */
    { 41, GUILD_GROUP_FIGHTERS, GUILD_GIVES_REST | GUILD_GIVES_CHEAP_REPAIR, E,
      { "Apprentice", "Journeyman", "Swordsman", "Protector", "Defender", "Warder", "Guardian", "Champion", "Warrior", "Master" },
      { K_ARCHERY, K_AXE, K_BLUNT, K_GIANTISH, K_LONG_BLADE, K_ORCISH, K_SHORT_BLADE, E, E, E, E, E },
      { K_ARCHERY, K_AXE, K_BLUNT, K_CRITICAL, K_GIANTISH, K_JUMPING, K_LONG_BLADE, K_ORCISH, K_RUNNING, K_SHORT_BLADE, K_SWIMMING, E },
      681, 679, 680, 684, 686 },
    /* The Mages Guild */
    { 40, GUILD_GROUP_MAGES, 0, E,
      { "Apprentice", "Journeyman", "Evoker", "Conjurer", "Magician", "Enchanter", "Warlock", "Wizard", "Master Wizard", "Archmage" },
      { K_ALTERATION, K_DESTRUCTION, K_ILLUSION, K_MYSTICISM, K_RESTORATION, K_THAUMATURGY, E, E, E, E, E, E },
      { K_ALTERATION, K_DAEDRIC, K_DESTRUCTION, K_DRAGONISH, K_HARPY, K_ILLUSION, K_IMPISH, K_MYSTICISM, K_ORCISH, K_RESTORATION, K_SPRIGGAN, K_THAUMATURGY },
      606, 612, 611, 5293, 5236 },
    /* the temple of Akatosh */
    { 26, GUILD_GROUP_TEMPLE, GUILD_TRAINS_ALL, 1,
      { "Novice", "Initiate", "Acolyte", "Adept", "Curate", "Disciple", "Brother", "Diviner", "Master", "Patriarch" },
      { K_ALTERATION, K_DAEDRIC, K_DESTRUCTION, K_DRAGONISH, K_LONG_BLADE, K_RUNNING, K_STEALTH, E, E, E, E, E },
      { K_ALTERATION, K_ARCHERY, K_DAEDRIC, K_DESTRUCTION, K_DRAGONISH, K_LONG_BLADE, K_RUNNING, K_STEALTH, K_SWIMMING, E, E, E },
      740, 745, 744, 5290, 5245 },
    /* the temple of Arkay */
    { 21, GUILD_GROUP_TEMPLE, GUILD_TRAINS_ALL | GUILD_GIVES_CHEAP_CURE, 0,
      { "Novice", "Initiate", "Acolyte", "Adept", "Curate", "Disciple", "Brother", "Diviner", "Master", "Patriarch" },
      { K_AXE, K_BACKSTABBING, K_DAEDRIC, K_DESTRUCTION, K_MEDICAL, K_RESTORATION, K_SHORT_BLADE, E, E, E, E, E },
      { K_AXE, K_BACKSTABBING, K_CLIMBING, K_CRITICAL, K_DAEDRIC, K_DESTRUCTION, K_MEDICAL, K_RESTORATION, K_SHORT_BLADE, E, E, E },
      740, 745, 744, 5287, 5242 },
    /* the temple of Dibella */
    { 29, GUILD_GROUP_TEMPLE, GUILD_TRAINS_ALL, 2,
      { "Novice", "Initiate", "Acolyte", "Adept", "Curate", "Disciple", "Brother", "Diviner", "Master", "Patriarch" },
      { K_DAEDRIC, K_ETIQUETTE, K_ILLUSION, K_LOCKPICKING, K_LONG_BLADE, K_NYMPH, K_ORCISH, K_RESTORATION, E, E, E, E },
      { K_DAEDRIC, K_ETIQUETTE, K_HARPY, K_ILLUSION, K_LOCKPICKING, K_LONG_BLADE, K_NYMPH, K_ORCISH, K_RESTORATION, K_STREETWISE, E, E },
      740, 745, 744, 5290, 5247 },
    /* the temple of Julianos */
    { 27, GUILD_GROUP_TEMPLE, GUILD_TRAINS_ALL, 2,
      { "Novice", "Initiate", "Acolyte", "Adept", "Curate", "Disciple", "Brother", "Diviner", "Master", "Patriarch" },
      { K_ALTERATION, K_DAEDRIC, K_IMPISH, K_LOCKPICKING, K_MYSTICISM, K_SHORT_BLADE, K_THAUMATURGY, E, E, E, E, E },
      { K_ALTERATION, K_CRITICAL, K_DAEDRIC, K_IMPISH, K_LOCKPICKING, K_MERCANTILE, K_MYSTICISM, K_SHORT_BLADE, K_THAUMATURGY, E, E, E },
      740, 745, 744, 6610, 5246 },
    /* the temple of Kynareth */
    { 35, GUILD_GROUP_TEMPLE, GUILD_TRAINS_ALL, 1,
      { "Novice", "Initiate", "Acolyte", "Adept", "Curate", "Disciple", "Brother", "Diviner", "Master", "Patriarch" },
      { K_ARCHERY, K_CLIMBING, K_DAEDRIC, K_DESTRUCTION, K_DODGING, K_DRAGONISH, K_HARPY, K_ILLUSION, K_JUMPING, K_RUNNING, K_STEALTH, E },
      { K_ARCHERY, K_CLIMBING, K_DAEDRIC, K_DESTRUCTION, K_DODGING, K_DRAGONISH, K_HARPY, K_ILLUSION, K_JUMPING, K_RUNNING, K_STEALTH, E },
      740, 745, 744, 5290, 5249 },
    /* the temple of Mara */
    { 24, GUILD_GROUP_TEMPLE, GUILD_TRAINS_ALL, 1,
      { "Novice", "Initiate", "Acolyte", "Adept", "Curate", "Disciple", "Brother", "Diviner", "Master", "Patriarch" },
      { K_ARCHERY, K_CRITICAL, K_DAEDRIC, K_ETIQUETTE, K_HARPY, K_ILLUSION, K_MEDICAL, K_NYMPH, K_RESTORATION, K_STREETWISE, E, E },
      { K_ARCHERY, K_CRITICAL, K_DAEDRIC, K_ETIQUETTE, K_HARPY, K_ILLUSION, K_MEDICAL, K_NYMPH, K_RESTORATION, K_STREETWISE, E, E },
      740, 745, 744, 5289, 5244 },
    /* the temple of Stendarr */
    { 33, GUILD_GROUP_TEMPLE, GUILD_TRAINS_ALL, 0,
      { "Novice", "Initiate", "Acolyte", "Adept", "Curate", "Disciple", "Brother", "Diviner", "Master", "Patriarch" },
      { K_AXE, K_BLUNT, K_CRITICAL, K_DAEDRIC, K_DODGING, K_MEDICAL, K_RESTORATION, E, E, E, E, E },
      { K_AXE, K_BLUNT, K_CRITICAL, K_DAEDRIC, K_DODGING, K_MEDICAL, K_ORCISH, K_RESTORATION, K_SPRIGGAN, E, E, E },
      740, 745, 744, 5289, 5248 },
    /* the temple of Zenithar */
    { 22, GUILD_GROUP_TEMPLE, GUILD_TRAINS_ALL, 1,
      { "Novice", "Initiate", "Acolyte", "Adept", "Curate", "Disciple", "Brother", "Diviner", "Master", "Patriarch" },
      { K_BLUNT, K_CENTAURIAN, K_DAEDRIC, K_ETIQUETTE, K_GIANTISH, K_HARPY, K_MERCANTILE, K_ORCISH, K_PICKPOCKET, K_SPRIGGAN, K_STREETWISE, K_THAUMATURGY },
      { K_BLUNT, K_CENTAURIAN, K_DAEDRIC, K_ETIQUETTE, K_GIANTISH, K_HARPY, K_MERCANTILE, K_ORCISH, K_PICKPOCKET, K_SPRIGGAN, K_STREETWISE, K_THAUMATURGY },
      740, 745, 744, 5288, 5243 },
    /* the order of the Horn */
    { 411, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Dragon */
    { 368, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Flame */
    { 410, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Hawk */
    { 417, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Owl */
    { 413, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Rose */
    { 409, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Wheel */
    { 415, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Candle */
    { 408, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Raven */
    { 414, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 },
    /* the order of the Scarab */
    { 416, GUILD_GROUP_KNIGHTLY, GUILD_GIVES_FREE_ROOMS, E,
      { "Aspirant", "Squire", "Gallant", "Chevalier", "Keeper", "Knight Brother", "Commander", "Marshall", "Seneschal", "Paladin" },
      { K_ARCHERY, K_CRITICAL, K_DRAGONISH, K_ETIQUETTE, K_GIANTISH, K_LONG_BLADE, K_MEDICAL, E, E, E, E, E },
      { E, E, E, E, E, E, E, E, E, E, E, E },
      752, 751, 750, 5291, 5237 }
};
#undef E

const GuildKind *classic_guilds(int *n)
{
    *n = (int)(sizeof(g_guilds) / sizeof(g_guilds[0]));
    return g_guilds;
}

/*
 * The effects a spell is made of (DFU's EntityEffect properties: classic's group and
 * subgroup, the school, the parameters it supports, the targets and elements it
 * allows, the cost of each parameter as A, B and a flat offset). Classic's
 * dispel spells reach others and the caster (SPELLS.STD's own Holy Touch and
 * Null Magicka do), Jumping is 32 as SPELLS.STD has it.
 */
#define EF(group, sub, gname, sname, school, sup, tg, el, d0, d1, d2, c0, c1, c2, m0, m1, m2) \
    { MAGIC_ID(group, sub), gname, sname, school, sup, tg, el, { d0, d1, d2 }, { c0, c1, c2 }, { m0, m1, m2 } }
static const MagicEffect g_effects[] = {
    EF(0, 255, "Paralyze", "", 25, 3, 30, 31, 28, 100, 0, 28, 100, 0, 0, 0, 0),
    EF(1, 0, "Continuous Damage", "Health", 22, 5, 30, 31, 28, 8, 0, 0, 0, 0, 40, 28, 0),
    EF(1, 1, "Continuous Damage", "Fatigue", 22, 5, 30, 31, 20, 8, 0, 0, 0, 0, 40, 28, 0),
    EF(1, 2, "Continuous Damage", "Spell Points", 22, 5, 30, 31, 40, 8, 0, 0, 0, 0, 40, 28, 0),
    EF(2, 255, "Create Item", "", 27, 1, 1, 16, 60, 120, 0, 0, 0, 0, 0, 0, 0),
    EF(3, 0, "Cure", "Disease", 23, 2, 31, 16, 0, 0, 0, 8, 100, 0, 0, 0, 0),
    EF(3, 1, "Cure", "Poison", 23, 2, 31, 16, 0, 0, 0, 8, 100, 0, 0, 0, 0),
    EF(3, 2, "Cure", "Paralyzation", 23, 2, 31, 16, 0, 0, 0, 20, 140, 0, 0, 0, 0),
    EF(4, 0, "Damage", "Health", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 20, 28, 0),
    EF(4, 1, "Damage", "Fatigue", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 20, 28, 0),
    EF(4, 2, "Damage", "Spell Points", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 20, 28, 0),
    EF(5, 255, "Disintegrate", "", 22, 2, 30, 31, 0, 0, 0, 80, 140, 0, 0, 0, 0),
    EF(6, 0, "Dispel", "Magic", 27, 2, 31, 16, 0, 0, 0, 120, 180, 0, 0, 0, 0),
    EF(6, 1, "Dispel", "Undead", 27, 2, 31, 16, 0, 0, 0, 80, 140, 0, 0, 0, 0),
    EF(6, 2, "Dispel", "Daedra", 27, 2, 31, 16, 0, 0, 0, 120, 180, 0, 0, 0, 0),
    EF(7, 0, "Drain", "Strength", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 8, 100, 116),
    EF(7, 1, "Drain", "Intelligence", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 8, 100, 116),
    EF(7, 2, "Drain", "Willpower", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 8, 100, 116),
    EF(7, 3, "Drain", "Agility", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 8, 100, 116),
    EF(7, 4, "Drain", "Endurance", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 8, 100, 116),
    EF(7, 5, "Drain", "Personality", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 8, 100, 116),
    EF(7, 6, "Drain", "Speed", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 8, 100, 116),
    EF(7, 7, "Drain", "Luck", 22, 4, 30, 31, 0, 0, 0, 0, 0, 0, 8, 100, 116),
    EF(8, 0, "Resist", "Fire", 25, 3, 31, 16, 100, 100, 0, 8, 100, 0, 0, 0, 0),
    EF(8, 1, "Resist", "Cold", 25, 3, 31, 16, 100, 100, 0, 8, 100, 0, 0, 0, 0),
    EF(8, 2, "Resist", "Poison", 25, 3, 31, 16, 100, 100, 0, 8, 100, 0, 0, 0, 0),
    EF(8, 3, "Resist", "Shock", 25, 3, 31, 16, 100, 100, 0, 8, 100, 0, 0, 0, 0),
    EF(8, 4, "Resist", "Magicka", 25, 3, 31, 16, 100, 100, 0, 8, 100, 0, 0, 0, 0),
    EF(9, 0, "Fortify", "Strength", 23, 5, 31, 16, 28, 100, 0, 0, 0, 0, 40, 120, 0),
    EF(9, 1, "Fortify", "Intelligence", 23, 5, 31, 16, 28, 100, 0, 0, 0, 0, 40, 120, 0),
    EF(9, 2, "Fortify", "Willpower", 23, 5, 31, 16, 28, 100, 0, 0, 0, 0, 40, 120, 0),
    EF(9, 3, "Fortify", "Agility", 23, 5, 31, 16, 28, 100, 0, 0, 0, 0, 40, 120, 0),
    EF(9, 4, "Fortify", "Endurance", 23, 5, 31, 16, 28, 100, 0, 0, 0, 0, 40, 120, 0),
    EF(9, 5, "Fortify", "Personality", 23, 5, 31, 16, 28, 100, 0, 0, 0, 0, 40, 120, 0),
    EF(9, 6, "Fortify", "Speed", 23, 5, 31, 16, 28, 100, 0, 0, 0, 0, 40, 120, 0),
    EF(9, 7, "Fortify", "Luck", 23, 5, 31, 16, 28, 100, 0, 0, 0, 0, 40, 120, 0),
    EF(10, 0, "Heal", "Strength", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 40, 28, 0),
    EF(10, 1, "Heal", "Intelligence", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 40, 28, 0),
    EF(10, 2, "Heal", "Willpower", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 40, 28, 0),
    EF(10, 3, "Heal", "Agility", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 40, 28, 0),
    EF(10, 4, "Heal", "Endurance", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 40, 28, 0),
    EF(10, 5, "Heal", "Personality", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 40, 28, 0),
    EF(10, 6, "Heal", "Speed", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 40, 28, 0),
    EF(10, 7, "Heal", "Luck", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 40, 28, 0),
    EF(10, 8, "Heal", "Health", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 20, 28, 0),
    EF(10, 9, "Heal", "Fatigue", 23, 4, 31, 16, 0, 0, 0, 0, 0, 0, 8, 28, 0),
    EF(11, 0, "Transfer", "Strength", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 1, "Transfer", "Intelligence", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 2, "Transfer", "Willpower", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 3, "Transfer", "Agility", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 4, "Transfer", "Endurance", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 5, "Transfer", "Personality", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 6, "Transfer", "Speed", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 7, "Transfer", "Luck", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 8, "Transfer", "Health", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(11, 9, "Transfer", "Fatigue", 22, 4, 30, 16, 0, 0, 0, 0, 0, 0, 60, 100, 40),
    EF(12, 255, "Soul Trap", "", 27, 3, 30, 31, 60, 68, 0, 40, 68, 0, 0, 0, 0),
    EF(13, 0, "Invisibility", "Normal", 24, 1, 31, 16, 40, 120, 0, 0, 0, 0, 0, 0, 0),
    EF(13, 1, "Invisibility", "True", 24, 1, 31, 16, 60, 140, 0, 0, 0, 0, 0, 0, 0),
    EF(14, 255, "Levitate", "", 26, 1, 1, 16, 60, 100, 0, 0, 0, 0, 0, 0, 0),
    EF(15, 255, "Light", "", 24, 1, 1, 16, 8, 40, 0, 0, 0, 0, 0, 0, 0),
    EF(16, 255, "Lock", "", 27, 2, 1, 16, 0, 0, 0, 28, 120, 120, 0, 0, 0),
    EF(17, 255, "Open", "", 27, 2, 1, 16, 0, 0, 0, 20, 100, 0, 0, 0, 0),
    EF(18, 255, "Regenerate", "", 23, 5, 31, 16, 100, 20, 0, 0, 0, 0, 8, 8, 0),
    EF(19, 255, "Silence", "", 27, 3, 31, 31, 20, 100, 0, 20, 100, 0, 0, 0, 0),
    EF(20, 255, "Spell Absorption", "", 23, 3, 31, 16, 28, 140, 0, 28, 140, 0, 0, 0, 0),
    EF(21, 255, "Spell Reflection", "", 26, 3, 31, 16, 28, 140, 0, 28, 140, 0, 0, 0, 0),
    EF(22, 255, "Spell Resistance", "", 26, 3, 31, 16, 20, 100, 0, 20, 100, 0, 0, 0, 0),
    EF(23, 0, "Chameleon", "Normal", 24, 1, 31, 16, 20, 80, 0, 0, 0, 0, 0, 0, 0),
    EF(23, 1, "Chameleon", "True", 24, 1, 31, 16, 40, 120, 0, 0, 0, 0, 0, 0, 0),
    EF(24, 0, "Shadow", "Normal", 24, 1, 31, 16, 20, 80, 0, 0, 0, 0, 0, 0, 0),
    EF(24, 1, "Shadow", "True", 24, 1, 31, 16, 40, 120, 0, 0, 0, 0, 0, 0, 0),
    EF(25, 255, "Slowfall", "", 25, 1, 1, 16, 20, 100, 0, 0, 0, 0, 0, 0, 0),
    EF(26, 255, "Free Action", "", 23, 1, 31, 16, 20, 8, 0, 0, 0, 0, 0, 0, 0),
    EF(28, 255, "Climbing", "", 25, 1, 31, 16, 20, 20, 0, 0, 0, 0, 0, 0, 0),
    EF(29, 255, "Morph Self", "", 24, 0, 1, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    EF(30, 255, "Water Breathing", "", 25, 1, 31, 16, 20, 8, 0, 0, 0, 0, 0, 0, 0),
    EF(31, 255, "Water Walking", "", 26, 1, 31, 16, 20, 8, 0, 0, 0, 0, 0, 0, 0),
    EF(32, 255, "Jumping", "", 25, 1, 31, 16, 20, 8, 0, 0, 0, 0, 0, 0, 0),
    EF(33, 0, "Calm", "Animal", 26, 2, 30, 16, 0, 0, 0, 60, 100, 160, 0, 0, 0),
    EF(33, 1, "Calm", "Undead", 26, 2, 30, 16, 0, 0, 0, 60, 120, 36, 0, 0, 0),
    EF(33, 2, "Calm", "Humanoid", 26, 2, 30, 16, 0, 0, 0, 80, 140, 60, 0, 0, 0),
    EF(34, 255, "Charm", "", 26, 3, 30, 31, 20, 8, 0, 40, 60, 0, 0, 0, 0),
    EF(35, 255, "Shield", "", 25, 5, 31, 16, 28, 8, 0, 0, 0, 0, 80, 60, 0),
    EF(39, 0, "Detect", "Magic", 26, 1, 1, 16, 20, 8, 200, 0, 0, 0, 0, 0, 0),
    EF(39, 1, "Detect", "Enemy", 26, 1, 1, 16, 20, 8, 200, 0, 0, 0, 0, 0, 0),
    EF(39, 2, "Detect", "Treasure", 26, 1, 1, 16, 20, 8, 160, 0, 0, 0, 0, 0, 0),
    EF(40, 255, "Identify", "", 26, 2, 1, 16, 0, 0, 0, 40, 100, 28, 0, 0, 0),
    EF(43, 255, "Teleport", "", 27, 0, 1, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    EF(44, 255, "Comprehend Languages", "", 27, 3, 1, 16, 60, 68, 0, 40, 68, 0, 0, 0, 0),
};
#undef EF

/*
 * The spells creatures cast (DFU's EnemyEntity lists, classic's), SPELLS.STD's numbers: by monster kind, then a
 * class enemy's seven lists by level / 3: frost daedra's, daedroth's, orc shaman's, ancient vampire's, daedra
 * lord's, lich's, ancient lich's.
 */
static const FoeSpell g_foe_spell_rows[] = {
    { 0, 1, 7 },
    { 1, 1, 10 },
    { 2, 1, 29 },
    { 3, 1, 44 },
    { 4, 18, 34 },
    { 5, 21, 6 },
    { 6, 21, 7 },
    { 7, 21, 22 },
    { 8, 21, 25 },
    { 9, 21, 31 },
    { 10, 23, 28 },
    { 11, 23, 31 },
    { 12, 25, 16 },
    { 13, 25, 20 },
    { 14, 26, 14 },
    { 15, 26, 25 },
    { 16, 27, 22 },
    { 17, 27, 23 },
    { 18, 27, 31 },
    { 19, 28, 51 },
    { 20, 29, 52 },
    { 21, 29, 67 },
    { 22, 30, 8 },
    { 23, 30, 50 },
    { 24, 31, 8 },
    { 25, 31, 10 },
    { 26, 31, 14 },
    { 27, 31, 60 },
    { 28, 31, 67 },
    { 29, 32, 8 },
    { 30, 32, 10 },
    { 31, 32, 14 },
    { 32, 32, 34 },
    { 33, 32, 60 },
    { 34, 33, 8 },
    { 35, 33, 10 },
    { 36, 33, 14 },
    { 37, 33, 29 },
    { 38, 33, 31 },
    { 39, 33, 34 },
    { 40, 33, 60 },
    { 41, ESPL_CLASS + 0, 16 },
    { 42, ESPL_CLASS + 0, 20 },
    { 43, ESPL_CLASS + 1, 22 },
    { 44, ESPL_CLASS + 1, 23 },
    { 45, ESPL_CLASS + 1, 31 },
    { 46, ESPL_CLASS + 2, 6 },
    { 47, ESPL_CLASS + 2, 7 },
    { 48, ESPL_CLASS + 2, 22 },
    { 49, ESPL_CLASS + 2, 25 },
    { 50, ESPL_CLASS + 2, 31 },
    { 51, ESPL_CLASS + 3, 8 },
    { 52, ESPL_CLASS + 3, 50 },
    { 53, ESPL_CLASS + 4, 8 },
    { 54, ESPL_CLASS + 4, 10 },
    { 55, ESPL_CLASS + 4, 14 },
    { 56, ESPL_CLASS + 4, 60 },
    { 57, ESPL_CLASS + 4, 67 },
    { 58, ESPL_CLASS + 5, 8 },
    { 59, ESPL_CLASS + 5, 10 },
    { 60, ESPL_CLASS + 5, 14 },
    { 61, ESPL_CLASS + 5, 34 },
    { 62, ESPL_CLASS + 5, 60 },
    { 63, ESPL_CLASS + 6, 8 },
    { 64, ESPL_CLASS + 6, 10 },
    { 65, ESPL_CLASS + 6, 14 },
    { 66, ESPL_CLASS + 6, 29 },
    { 67, ESPL_CLASS + 6, 31 },
    { 68, ESPL_CLASS + 6, 34 },
    { 69, ESPL_CLASS + 6, 60 },
};

const FoeSpell *classic_foe_spells(int *n)
{
    *n = (int)(sizeof(g_foe_spell_rows) / sizeof(g_foe_spell_rows[0]));
    return g_foe_spell_rows;
}

const MagicEffect *classic_effects(int *n)
{
    *n = (int)(sizeof(g_effects) / sizeof(g_effects[0]));
    return g_effects;
}

int classic_effect_known(const MagicEffect *effects, int id)
{
    int i;

    for (i = 0; i < (int)(sizeof(g_effects) / sizeof(g_effects[0])); i++) {
        if (effects[i].id == id) {
            return 1;
        }
    }
    return 0;
}

/*
 * The quests offered (DFU's QuestList-Classic: the 187 of classic's that its
 * guilds and people give, by file name): who offers it, to whom, the least it
 * asks, its flags. The group is FACTION.TXT's ggroup (3 the Dark Brotherhood,
 * 4 the Thieves Guild, 9 a knightly order, 10 the Mages, 11 the Fighters, 17 a
 * temple, 22 the witches, 23 the vampires) or QUEST_SOCIAL and a social group.
 * A temple's own quests are for its god's letter: T Akatosh, A Arkay, D
 * Dibella, J Julianos, K Kynareth, R Mara, S Stendarr, Z Zenithar.
 */
static const QuestKind g_quests[] = {
    { 0, "M0C00Y11", 11, 'N', 0, 0 },
    { 1, "M0C00Y12", 11, 'N', 0, 0 },
    { 2, "M0C00Y13", 11, 'N', 0, 0 },
    { 3, "M0C00Y14", 11, 'N', 0, 0 },
    { 4, "M0B00Y00", 11, 'M', 0, 0 },
    { 5, "M0B00Y06", 11, 'M', 0, 0 },
    { 6, "M0B00Y07", 11, 'M', 0, 0 },
    { 7, "M0B00Y15", 11, 'M', 0, 0 },
    { 8, "M0B00Y16", 11, 'M', 0, 0 },
    { 9, "M0B00Y17", 11, 'M', 0, 0 },
    { 10, "M0B1XY01", 11, 'M', 1, QUEST_ADULT },
    { 11, "M0B11Y18", 11, 'M', 1, QUEST_ONCE },
    { 12, "M0B20Y02", 11, 'M', 2, 0 },
    { 13, "M0B21Y19", 11, 'M', 2, QUEST_ONCE },
    { 14, "M0B30Y03", 11, 'M', 3, 0 },
    { 15, "M0B30Y04", 11, 'M', 3, 0 },
    { 16, "M0B30Y08", 11, 'M', 3, 0 },
    { 17, "M0B40Y05", 11, 'M', 4, 0 },
    { 18, "M0B50Y09", 11, 'M', 5, 0 },
    { 19, "M0B60Y10", 11, 'M', 6, 0 },
    { 20, "N0C00Y10", 10, 'N', 0, 0 },
    { 21, "N0C00Y11", 10, 'N', 0, 0 },
    { 22, "N0C00Y12", 10, 'N', 0, 0 },
    { 23, "N0C00Y13", 10, 'N', 0, 0 },
    { 24, "N0B00Y04", 10, 'M', 0, 0 },
    { 25, "N0B00Y06", 10, 'M', 0, 0 },
    { 26, "N0B00Y08", 10, 'M', 0, 0 },
    { 27, "N0B00Y09", 10, 'M', 0, 0 },
    { 28, "N0B00Y16", 10, 'M', 0, 0 },
    { 29, "N0B00Y17", 10, 'M', 0, 0 },
    { 30, "N0B10Y01", 10, 'M', 1, 0 },
    { 31, "N0B10Y03", 10, 'M', 1, 0 },
    { 32, "N0B11Y18", 10, 'M', 1, QUEST_ONCE },
    { 33, "N0B20Y02", 10, 'M', 2, 0 },
    { 34, "N0B20Y05", 10, 'M', 2, 0 },
    { 35, "N0B21Y14", 10, 'M', 2, QUEST_ONCE },
    { 36, "N0B30Y15", 10, 'M', 3, 0 },
    { 37, "N0B40Y07", 10, 'M', 4, 0 },
    { 38, "C0C00Y10", 17, 'N', 0, 0 },
    { 39, "C0C00Y11", 17, 'N', 0, 0 },
    { 40, "C0C00Y12", 17, 'N', 0, 0 },
    { 41, "C0C00Y13", 17, 'N', 0, 0 },
    { 42, "C0B00Y00", 17, 'M', 0, 0 },
    { 43, "C0B00Y01", 17, 'M', 0, 0 },
    { 44, "C0B00Y02", 17, 'M', 0, 0 },
    { 45, "C0B00Y03", 17, 'M', 0, 0 },
    { 46, "C0B00Y04", 17, 'M', 0, 0 },
    { 47, "C0B00Y14", 17, 'M', 0, 0 },
    { 48, "C0B10Y05", 17, 'M', 1, 0 },
    { 49, "C0B10Y06", 17, 'M', 1, 0 },
    { 50, "C0B10Y07", 17, 'M', 1, 0 },
    { 51, "C0B10Y15", 17, 'M', 1, 0 },
    { 52, "C0B20Y08", 17, 'M', 2, 0 },
    { 53, "C0B3XY09", 17, 'M', 3, QUEST_ADULT },
    { 54, "00B00Y00", 17, 'J', 0, 0 },
    { 55, "D0B00Y00", 17, 'T', 0, 0 },
    { 56, "E0B00Y00", 17, 'A', 0, 0 },
    { 57, "F0B00Y00", 17, 'D', 0, 0 },
    { 58, "G0B00Y00", 17, 'K', 0, 0 },
    { 59, "H0B00Y00", 17, 'R', 0, 0 },
    { 60, "I0B00Y00", 17, 'S', 0, 0 },
    { 61, "J0B00Y00", 17, 'Z', 0, 0 },
    { 62, "O0A0AL00", 4, 'P', 0, 0 },
    { 63, "O0B00Y00", 4, 'M', 0, 0 },
    { 64, "O0B00Y01", 4, 'M', 0, 0 },
    { 65, "O0B00Y11", 4, 'M', 0, 0 },
    { 66, "O0B00Y12", 4, 'M', 0, 0 },
    { 67, "O0B10Y00", 4, 'M', 1, 0 },
    { 68, "O0B10Y03", 4, 'M', 1, 0 },
    { 69, "O0B10Y05", 4, 'M', 1, 0 },
    { 70, "O0B10Y06", 4, 'M', 1, 0 },
    { 71, "O0B10Y07", 4, 'M', 1, 0 },
    { 72, "O0B20Y02", 4, 'M', 2, 0 },
    { 73, "O0B2XY04", 4, 'M', 2, QUEST_ADULT },
    { 74, "O0B2XY08", 4, 'M', 2, QUEST_ADULT },
    { 75, "O0B2XY09", 4, 'M', 2, QUEST_ADULT },
    { 76, "O0B2XY10", 4, 'M', 2, QUEST_ADULT },
    { 77, "L0A01L00", 3, 'P', 0, QUEST_ONCE },
    { 78, "L0B00Y00", 3, 'M', 0, 0 },
    { 79, "L0B00Y01", 3, 'M', 0, 0 },
    { 80, "L0B00Y02", 3, 'M', 0, 0 },
    { 81, "L0B00Y03", 3, 'M', 0, 0 },
    { 82, "L0B10Y01", 3, 'M', 1, 0 },
    { 83, "L0B10Y03", 3, 'M', 1, 0 },
    { 84, "L0B20Y02", 3, 'M', 2, 0 },
    { 85, "L0B30Y03", 3, 'M', 3, 0 },
    { 86, "L0B30Y09", 3, 'M', 3, 0 },
    { 87, "L0B40Y04", 3, 'M', 4, 0 },
    { 88, "L0B50Y11", 3, 'M', 5, 0 },
    { 89, "L0B60Y10", 3, 'M', 6, 0 },
    { 90, "B0C00Y05", 9, 'N', 0, 0 },
    { 91, "B0C00Y06", 9, 'N', 0, 0 },
    { 92, "B0C00Y10", 9, 'N', 0, 0 },
    { 93, "B0C00Y13", 9, 'N', 0, 0 },
    { 94, "B0B00Y00", 9, 'M', 0, 0 },
    { 95, "B0B00Y01", 9, 'M', 0, 0 },
    { 96, "B0B10Y04", 9, 'M', 1, 0 },
    { 97, "B0B20Y07", 9, 'M', 2, 0 },
    { 98, "B0B40Y08", 9, 'M', 4, 0 },
    { 99, "B0B40Y09", 9, 'M', 4, 0 },
    { 100, "B0B50Y11", 9, 'M', 5, 0 },
    { 101, "B0B60Y12", 9, 'M', 6, 0 },
    { 102, "B0B70Y14", 9, 'M', 7, 0 },
    { 103, "B0B70Y16", 9, 'M', 7, 0 },
    { 104, "B0B71Y03", 9, 'M', 7, QUEST_ONCE },
    { 105, "B0B80Y17", 9, 'M', 8, 0 },
    { 106, "B0B81Y02", 9, 'M', 8, QUEST_ONCE },
    { 107, "Q0C00Y01", 22, 'N', 0, 0 },
    { 108, "Q0C00Y03", 22, 'N', 0, 0 },
    { 109, "Q0C00Y04", 22, 'N', 0, 0 },
    { 110, "Q0C00Y06", 22, 'N', 0, 0 },
    { 111, "Q0C00Y07", 22, 'N', 0, 0 },
    { 112, "Q0C00Y08", 22, 'N', 0, 0 },
    { 113, "Q0C0XY02", 22, 'N', 0, QUEST_ADULT },
    { 114, "Q0C10Y00", 22, 'N', 1, 0 },
    { 115, "Q0C20Y02", 22, 'N', 2, 0 },
    { 116, "Q0C4XY04", 22, 'N', 4, QUEST_ADULT },
    { 117, "A0C00Y00", QUEST_SOCIAL + SOCIAL_COMMONERS, 'N', 0, 0 },
    { 118, "A0C00Y06", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, 0 },
    { 119, "A0C00Y07", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, 0 },
    { 120, "A0C00Y08", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, 0 },
    { 121, "A0C00Y10", QUEST_SOCIAL + SOCIAL_COMMONERS, 'N', 0, 0 },
    { 122, "A0C00Y11", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, 0 },
    { 123, "A0C00Y12", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, 0 },
    { 124, "A0C00Y14", QUEST_SOCIAL + SOCIAL_COMMONERS, 'N', 0, 0 },
    { 125, "A0C00Y15", QUEST_SOCIAL + SOCIAL_COMMONERS, 'N', 0, 0 },
    { 126, "A0C00Y16", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, 0 },
    { 127, "A0C00Y17", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, 0 },
    { 128, "A0C01Y01", QUEST_SOCIAL + SOCIAL_COMMONERS, 'N', 0, QUEST_ONCE },
    { 129, "A0C01Y03", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, QUEST_ONCE },
    { 130, "A0C01Y06", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, QUEST_ONCE },
    { 131, "A0C01Y09", QUEST_SOCIAL + SOCIAL_COMMONERS, 'N', 0, QUEST_ONCE },
    { 132, "A0C01Y13", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, QUEST_ONCE },
    { 133, "A0C0XY04", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 0, QUEST_ADULT },
    { 134, "A0C10Y02", QUEST_SOCIAL + SOCIAL_COMMONERS, 'N', 1, 0 },
    { 135, "A0C10Y05", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 1, 0 },
    { 136, "A0C41Y18", QUEST_SOCIAL + SOCIAL_COMMONERS, 'M', 4, QUEST_ONCE },
    { 137, "K0C00Y00", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, 0 },
    { 138, "K0C00Y02", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'N', 0, 0 },
    { 139, "K0C00Y03", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, 0 },
    { 140, "K0C00Y04", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, 0 },
    { 141, "K0C00Y05", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, 0 },
    { 142, "K0C00Y07", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, 0 },
    { 143, "K0C00Y08", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, 0 },
    { 144, "K0C00Y09", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, 0 },
    { 145, "K0C01Y00", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'N', 0, QUEST_ONCE },
    { 146, "K0C01Y10", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, QUEST_ONCE },
    { 147, "K0C0XY01", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 0, QUEST_ADULT },
    { 148, "K0C30Y03", QUEST_SOCIAL + SOCIAL_MERCHANTS, 'M', 3, 0 },
    { 149, "P0A01L00", 23, 'P', 0, QUEST_ONCE },
    { 150, "P0B00L01", 23, 'M', 0, 0 },
    { 151, "P0B00L03", 23, 'M', 0, 0 },
    { 152, "P0B00L04", 23, 'M', 0, 0 },
    { 153, "P0B00L06", 23, 'M', 0, 0 },
    { 154, "P0B01L02", 23, 'M', 0, QUEST_ONCE },
    { 155, "P0B10L07", 23, 'M', 1, 0 },
    { 156, "P0B10L08", 23, 'M', 1, 0 },
    { 157, "P0B10L10", 23, 'M', 1, 0 },
    { 158, "P0B20L09", 23, 'M', 2, 0 },
    { 159, "R0C10Y00", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 160, "R0C10Y01", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 1, 0 },
    { 161, "R0C10Y02", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 162, "R0C10Y04", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 163, "R0C10Y05", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 164, "R0C10Y06", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 165, "R0C10Y08", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 166, "R0C10Y09", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 167, "R0C10Y10", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 168, "R0C10Y11", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 1, 0 },
    { 169, "R0C10Y12", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 1, 0 },
    { 170, "R0C10Y13", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 1, 0 },
    { 171, "R0C10Y14", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 1, 0 },
    { 172, "R0C10Y15", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 173, "R0C10Y17", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 174, "R0C10Y18", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 175, "R0C10Y20", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 176, "R0C10Y21", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, 0 },
    { 177, "R0C11Y03", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, QUEST_ONCE },
    { 178, "R0C11Y16", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, QUEST_ONCE },
    { 179, "R0C11Y19", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 1, QUEST_ONCE },
    { 180, "R0C11Y26", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 1, QUEST_ONCE },
    { 181, "R0C11Y28", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 1, QUEST_ONCE },
    { 182, "R0C20Y07", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 2, 0 },
    { 183, "R0C20Y22", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 2, 0 },
    { 184, "R0C30Y25", QUEST_SOCIAL + SOCIAL_NOBILITY, 'N', 3, 0 },
    { 185, "R0C4XY23", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 4, QUEST_ADULT },
    { 186, "R0C60Y24", QUEST_SOCIAL + SOCIAL_NOBILITY, 'M', 6, 0 },
    /* The main quest's, the tutorial's and the cures': started by the game and by each other, never offered. */
    { 187, "$CUREVAM", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 188, "$CUREWER", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 189, "S0000001", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 190, "S0000002", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 191, "S0000003", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 192, "S0000004", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 193, "S0000005", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 194, "S0000006", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 195, "S0000007", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 196, "S0000008", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 197, "S0000009", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 198, "S0000010", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 199, "S0000011", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 200, "S0000012", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 201, "S0000013", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 202, "S0000015", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 203, "S0000016", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 204, "S0000017", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 205, "S0000018", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 206, "S0000020", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 207, "S0000021", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 208, "S0000022", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 209, "S0000100", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 210, "S0000101", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 211, "S0000102", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 212, "S0000103", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 213, "S0000104", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 214, "S0000106", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 215, "S0000107", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 216, "S0000500", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 217, "S0000977", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 218, "S0000988", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 219, "S0000999", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 220, "_BRISIEN", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE },
    { 221, "_TUTOR__", QUEST_GROUP_MAIN, 'M', 0, QUEST_ONCE }
};

const QuestKind *classic_quests(int *n)
{
    *n = (int)(sizeof(g_quests) / sizeof(g_quests[0]));
    return g_quests;
}
