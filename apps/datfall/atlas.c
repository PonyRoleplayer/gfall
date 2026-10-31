#include "atlas.h"

#include "game/dagger/formats/climate.h"

static int fold_c(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static int has_q(const char *hay, const char *q)
{
    int i;
    int j;

    if (!q || !q[0]) {
        return 1;
    }
    if (!hay) {
        return 0;
    }
    for (i = 0; hay[i]; i++) {
        for (j = 0; q[j]; j++) {
            if (!hay[i + j] || fold_c(hay[i + j]) != fold_c(q[j])) {
                break;
            }
        }
        if (!q[j]) {
            return 1;
        }
    }
    return 0;
}

static void put_u(char *dst, int n)
{
    char tmp[16];
    int i;
    int k;
    unsigned u;

    if (n < 0) {
        n = 0;
    }
    u = (unsigned)n;
    if (u == 0) {
        dst[0] = '0';
        dst[1] = 0;
        return;
    }
    i = 0;
    while (u && i < 15) {
        tmp[i++] = (char)('0' + (u % 10u));
        u /= 10u;
    }
    for (k = 0; k < i; k++) {
        dst[k] = tmp[i - 1 - k];
    }
    dst[i] = 0;
}

int atlas_place_hits(const DunRef *duns, int ndun, const DunRef *towns, int ntown,
                     const char *q, int *hit_i, unsigned char *hit_out, int cap)
{
    int n;
    int i;
    char reg[16];

    if (!q) {
        q = "";
    }
    if (cap < 0) {
        cap = 0;
    }
    n = 0;
    for (i = 0; duns && i < ndun; i++) {
        put_u(reg, duns[i].region);
        if (!has_q(duns[i].name, q) && !has_q(reg, q)) {
            continue;
        }
        if (n < cap && hit_i && hit_out) {
            hit_i[n] = i;
            hit_out[n] = 0;
        }
        n++;
        if (hit_i && n >= cap) {
            return n;
        }
    }
    for (i = 0; towns && i < ntown; i++) {
        put_u(reg, towns[i].region);
        if (!has_q(towns[i].name, q) && !has_q(reg, q)) {
            continue;
        }
        if (n < cap && hit_i && hit_out) {
            hit_i[n] = i;
            hit_out[n] = 1;
        }
        n++;
        if (hit_i && n >= cap) {
            return n;
        }
    }
    return n;
}

static void copy_two(char *dst, int cap, const char *a, const char *b)
{
    int n;
    int i;

    if (!dst || cap < 1) {
        return;
    }
    dst[0] = 0;
    n = 0;
    for (i = 0; a && a[i] && n < cap - 1; i++) {
        dst[n++] = a[i];
    }
    for (i = 0; b && b[i] && n < cap - 1; i++) {
        dst[n++] = b[i];
    }
    dst[n] = 0;
}

int atlas_other_line(const DunRef *duns, int ndun, const DunRef *towns, int ntown,
                     int region, int outdoor, int location_id, int exterior_id,
                     char *dst, int cap, int *other_index)
{
    int idx;
    int i;
    const DunRef *hit;

    if (dst && cap > 0) {
        dst[0] = 0;
    }
    if (other_index) {
        *other_index = -1;
    }
    hit = 0;
    if (outdoor) {
        idx = dungeon_under(region, location_id);
        if (idx < 0) {
            return 0;
        }
        for (i = 0; duns && i < ndun; i++) {
            if (duns[i].region == region && duns[i].index == idx) {
                hit = &duns[i];
                break;
            }
        }
        if (!hit) {
            return 0;
        }
        copy_two(dst, cap, "Below: ", hit->name);
    } else {
        idx = dungeon_outside(region, exterior_id);
        if (idx < 0) {
            return 0;
        }
        for (i = 0; towns && i < ntown; i++) {
            if (towns[i].region == region && towns[i].index == idx) {
                hit = &towns[i];
                break;
            }
        }
        if (!hit) {
            return 0;
        }
        copy_two(dst, cap, "Above: ", hit->name);
    }
    if (other_index) {
        *other_index = idx;
    }
    return 1;
}

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
