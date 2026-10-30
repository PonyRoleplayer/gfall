#include "game/dagger/tables/classic.h"
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
 * DFU's ItemTemplates, the weapons (113..131, the arrow last): name,
 * weight in kg, price, hit points, the picture on the ground and in the
 * paper doll's hand, one-handed, blunt.
 */
static const ItemTemplate g_items[] = {
    { 113, "Dagger", 0.50f, 1, 50, 207, 5, 234, 5, 1, 0 },
    { 114, "Tanto", 0.75f, 3, 50, 207, 10, 234, 22, 1, 0 },
    { 115, "Staff", 2.00f, 5, 300, 207, 7, 234, 21, 0, 1 },
    { 116, "Shortsword", 2.50f, 5, 300, 207, 3, 234, 19, 1, 0 },
    { 117, "Wakizashi", 2.00f, 8, 200, 207, 2, 234, 26, 1, 0 },
    { 118, "Broadsword", 5.00f, 10, 600, 207, 2, 234, 2, 1, 0 },
    { 119, "Saber", 3.75f, 12, 700, 207, 2, 234, 17, 1, 0 },
    { 120, "Longsword", 4.50f, 15, 800, 207, 2, 234, 12, 1, 0 },
    { 121, "Katana", 2.50f, 25, 600, 207, 15, 234, 9, 1, 0 },
    { 122, "Claymore", 7.50f, 30, 1400, 207, 2, 234, 4, 0, 0 },
    { 123, "Dai-katana", 3.50f, 50, 800, 207, 2, 234, 7, 0, 0 },
    { 124, "Mace", 4.50f, 10, 800, 207, 6, 234, 14, 1, 1 },
    { 125, "Flail", 7.00f, 15, 1000, 207, 6, 234, 8, 0, 1 },
    { 126, "Warhammer", 7.00f, 20, 1600, 214, 3, 234, 25, 0, 1 },
    { 127, "Battle Axe", 6.00f, 20, 1200, 207, 4, 234, 0, 1, 0 },
    { 128, "War Axe", 7.50f, 20, 800, 207, 4, 234, 24, 0, 0 },
    { 129, "Short Bow", 1.00f, 10, 50, 207, 8, 234, 16, 0, 0 },
    { 130, "Long Bow", 1.50f, 20, 100, 207, 8, 234, 11, 0, 0 },
    { 131, "Arrow", 0.25f, 2, 1, 207, 16, 0, 0, 0, 0 }
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
 * ItemBuilder and GetWeaponMaterialModifier: iron to daedric, the damage
 * a material adds (ten times it to hit), and its weight, value and
 * condition multipliers.
 */
static const Material g_materials[] = {
    { 0, "Iron", -1, 4, 1, 4 },
    { 1, "Steel", 0, 5, 2, 6 },
    { 2, "Silver", 0, 4, 4, 6 },
    { 3, "Elven", 1, 4, 8, 8 },
    { 4, "Dwarven", 2, 3, 16, 12 },
    { 5, "Mithril", 3, 4, 32, 16 },
    { 6, "Adamantium", 3, 4, 64, 20 },
    { 7, "Ebony", 4, 2, 128, 24 },
    { 8, "Orcish", 5, 4, 256, 28 },
    { 9, "Daedric", 6, 5, 512, 32 }
};

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
