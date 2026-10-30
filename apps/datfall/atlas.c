#include "atlas.h"

#include "game/dagger/formats/climate.h"

const char *atlas_climate_name(int id)
{
    switch (id) {
    case CLIMATE_OCEAN:
        return "Ocean";
    case CLIMATE_DESERT:
        return "Desert";
    case CLIMATE_DESERT2:
        return "Desert";
    case CLIMATE_MOUNTAIN:
        return "Mountain";
    case CLIMATE_RAINFOREST:
        return "Rainforest";
    case CLIMATE_SWAMP:
        return "Swamp";
    case CLIMATE_SUBTROPICAL:
        return "Subtropical";
    case CLIMATE_MOUNTAIN_WOODS:
        return "Mountain woods";
    case CLIMATE_WOODLANDS:
        return "Woodlands";
    case CLIMATE_HAUNTED:
        return "Haunted woodlands";
    default:
        return "Unknown";
    }
}

const char *atlas_type_name(int type)
{
    switch (type) {
    case WORLDMAP_TOWN_CITY:
        return "City";
    case WORLDMAP_TOWN_HAMLET:
        return "Hamlet";
    case WORLDMAP_TOWN_VILLAGE:
        return "Village";
    case WORLDMAP_HOME_FARMS:
        return "Farm";
    case WORLDMAP_DUNGEON_LABYRINTH:
        return "Labyrinth";
    case WORLDMAP_RELIGION_TEMPLE:
        return "Temple";
    case WORLDMAP_TAVERN:
        return "Tavern";
    case WORLDMAP_DUNGEON_KEEP:
        return "Keep";
    case WORLDMAP_HOME_WEALTHY:
        return "Wealthy home";
    case WORLDMAP_RELIGION_CULT:
        return "Cult";
    case WORLDMAP_DUNGEON_RUIN:
        return "Ruin";
    case WORLDMAP_HOME_POOR:
        return "Poor home";
    case WORLDMAP_GRAVEYARD:
        return "Graveyard";
    case WORLDMAP_COVEN:
        return "Coven";
    case WORLDMAP_HOME_YOUR_SHIPS:
        return "Ships";
    default:
        return "";
    }
}

/* BGR for a location type. */
static void type_bgr(int type, int *b, int *g, int *r)
{
    switch (type) {
    case WORLDMAP_TOWN_CITY:
        *b = 255;
        *g = 255;
        *r = 255;
        break;
    case WORLDMAP_TOWN_HAMLET:
        *b = 40;
        *g = 220;
        *r = 255;
        break;
    case WORLDMAP_TOWN_VILLAGE:
        *b = 40;
        *g = 200;
        *r = 80;
        break;
    case WORLDMAP_HOME_FARMS:
        *b = 40;
        *g = 140;
        *r = 180;
        break;
    case WORLDMAP_DUNGEON_LABYRINTH:
        *b = 180;
        *g = 40;
        *r = 180;
        break;
    case WORLDMAP_RELIGION_TEMPLE:
        *b = 80;
        *g = 200;
        *r = 255;
        break;
    case WORLDMAP_TAVERN:
        *b = 20;
        *g = 140;
        *r = 255;
        break;
    case WORLDMAP_DUNGEON_KEEP:
        *b = 40;
        *g = 40;
        *r = 220;
        break;
    case WORLDMAP_HOME_WEALTHY:
        *b = 220;
        *g = 220;
        *r = 80;
        break;
    case WORLDMAP_RELIGION_CULT:
        *b = 180;
        *g = 40;
        *r = 80;
        break;
    case WORLDMAP_DUNGEON_RUIN:
        *b = 140;
        *g = 140;
        *r = 140;
        break;
    case WORLDMAP_HOME_POOR:
        *b = 80;
        *g = 120;
        *r = 160;
        break;
    case WORLDMAP_GRAVEYARD:
        *b = 80;
        *g = 80;
        *r = 80;
        break;
    case WORLDMAP_COVEN:
        *b = 160;
        *g = 40;
        *r = 120;
        break;
    default:
        *b = 220;
        *g = 80;
        *r = 40;
        break;
    }
}

static void climate_bgr(int id, int *b, int *g, int *r)
{
    switch (id) {
    case CLIMATE_DESERT:
    case CLIMATE_DESERT2:
        *b = 80;
        *g = 180;
        *r = 220;
        break;
    case CLIMATE_MOUNTAIN:
        *b = 170;
        *g = 170;
        *r = 170;
        break;
    case CLIMATE_RAINFOREST:
        *b = 30;
        *g = 110;
        *r = 20;
        break;
    case CLIMATE_SWAMP:
        *b = 50;
        *g = 90;
        *r = 50;
        break;
    case CLIMATE_SUBTROPICAL:
        *b = 60;
        *g = 170;
        *r = 80;
        break;
    case CLIMATE_MOUNTAIN_WOODS:
        *b = 70;
        *g = 110;
        *r = 60;
        break;
    case CLIMATE_HAUNTED:
        *b = 90;
        *g = 50;
        *r = 70;
        break;
    case CLIMATE_WOODLANDS:
        *b = 40;
        *g = 130;
        *r = 40;
        break;
    default:
        *b = 160;
        *g = 80;
        *r = 30;
        break;
    }
}

static void put_dot(unsigned char *dib, int x, int y, int type)
{
    int b;
    int g;
    int r;
    unsigned char *p;

    if (x < 0 || y < 0 || x >= WORLDMAP_W || y >= WORLDMAP_H) {
        return;
    }
    type_bgr(type, &b, &g, &r);
    p = dib + y * ATLAS_STRIDE + x * 3;
    p[0] = (unsigned char)b;
    p[1] = (unsigned char)g;
    p[2] = (unsigned char)r;
}

void atlas_build(unsigned char *dib, int climate_on, int dots)
{
    int y;
    int x;

    if (!dib) {
        return;
    }
    for (y = 0; y < WORLDMAP_H; y++) {
        unsigned char *row;

        row = dib + y * ATLAS_STRIDE;
        for (x = 0; x < WORLDMAP_W; x++) {
            int h;
            int s;
            int b;
            int g;
            int r;

            h = worldmap_height(x, y);
            /* The header's sea: height 1 or less. */
            if (h <= 1) {
                b = 160;
                g = 80;
                r = 30;
            } else if (climate_on) {
                s = 70 + h / 2;
                if (s > 220) {
                    s = 220;
                }
                climate_bgr(worldmap_climate(x, y), &b, &g, &r);
                b = b * s / 255;
                g = g * s / 255;
                r = r * s / 255;
            } else {
                s = 24 + h * 3 / 4;
                if (s > 230) {
                    s = 230;
                }
                b = s / 3;
                g = s;
                r = s / 2;
            }
            row[x * 3] = (unsigned char)b;
            row[x * 3 + 1] = (unsigned char)g;
            row[x * 3 + 2] = (unsigned char)r;
        }
    }
    if (!dots) {
        return;
    }
    /* One pass over the location index. pixel_of scans the whole map, so it is not used here. */
    for (y = 0; y < WORLDMAP_H; y++) {
        for (x = 0; x < WORLDMAP_W; x++) {
            int t;

            t = worldmap_location_type(x, y);
            if (t < 0) {
                continue;
            }
            put_dot(dib, x, y, t);
            put_dot(dib, x - 1, y, t);
            put_dot(dib, x + 1, y, t);
            put_dot(dib, x, y - 1, t);
            put_dot(dib, x, y + 1, t);
        }
    }
}
