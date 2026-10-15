#ifndef DF_CLIMATE_H
#define DF_CLIMATE_H

/*
 * Two different numbers. A map pixel stores a climate id (223..232).
 * A TEXTURE file's hundreds digit is a family: desert 0, mountain 100,
 * woodland 300, swamp 400. There is no family 200 (TEXTURE.219 is not
 * a file, and archive 274 is the mummy set). Crypts are family + 19,
 * caves family + 47, doors family + 74, sewers family + 68.
 */

#define CLIMATE_OCEAN 223
#define CLIMATE_DESERT 224
#define CLIMATE_DESERT2 225
#define CLIMATE_MOUNTAIN 226
#define CLIMATE_RAINFOREST 227
#define CLIMATE_SWAMP 228
#define CLIMATE_SUBTROPICAL 229
#define CLIMATE_MOUNTAIN_WOODS 230
#define CLIMATE_WOODLANDS 231
#define CLIMATE_HAUNTED 232

#define TEX_DESERT 0
#define TEX_MOUNTAIN 100
#define TEX_WOODLAND 300
#define TEX_SWAMP 400

#define TEX_SET_CRYPT 19
#define TEX_SET_CAVE 47
#define TEX_SET_DOOR 74
#define TEX_SET_SEWER 68

/* Family for a climate id, or -1 if the id is not one of the ten. */
int climate_family(int climate);

#endif
