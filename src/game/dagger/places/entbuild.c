#include "game/dagger/places/entbuild.h"
#include "game/dagger/places/doors.h"
#include "engine/scene/ent.h"

/* See entbuild.h. */

static EntityBuildHook g_change;
static EntityAddHook g_add;
static const EntBuilder *g_builder;

void entbuild_set_hooks(EntityBuildHook change, EntityAddHook add)
{
    g_change = change;
    g_add = add;
}

void entbuild_begin(const EntBuilder *builder)
{
    g_builder = builder;
}

void entbuild_end(void)
{
    g_builder = 0;
}

int entbuild_slot(int file, int image, int *slot, int *w, int *h)
{
    return g_builder && g_builder->slot && g_builder->slot(file, image, slot, w, h);
}

int entbuild_marker_nth(int record, int n, float *x, float *y, float *z)
{
    return g_builder && g_builder->marker && g_builder->marker(record, n, x, y, z);
}

void entbuild_make_person(Entity *pe, int slot, int w, int h, int tag, int file, int image, float x, float y, float z)
{
    entity_init(pe);
    pe->pose[ENTITY_IDLE].slot[0] = (df_u16)slot;
    pe->pose[ENTITY_IDLE].w[0] = (df_u16)w;
    pe->pose[ENTITY_IDLE].h[0] = (df_u16)h;
    pe->x = x; pe->y = y; pe->z = z;
    pe->radius = PEOPLE_RADIUS;
    pe->live = 1;
    pe->flags = ENT_USABLE | ENT_GHOST;
    pe->residence = ENT_HOME_PLACE;
    pe->owner = (df_u32)tag;
    pe->use.kind = USE_TALK;
    pe->use.reach = (df_u16)TALK_REACH;
    pe->use.what = tag & 0x7fff;
    pe->use.tag = (file << 7) + image;
    pe->use.label = "person";
}

int entbuild_person(Entity *out, int file, int image, int tag, float x, float y, float z)
{
    int slot, w, h;

    if (!entbuild_slot(file, image, &slot, &w, &h)) {
        return 0;
    }
    entbuild_make_person(out, slot, w, h, tag, file, image, x, y, z);
    return 1;
}

int entbuild_change(const EntBuild *b, Entity *e)
{
    return g_change ? g_change(b, e) : 1;
}

int entbuild_add(const EntBuild *b, Entity *out, int cap)
{
    return g_add ? g_add(b, out, cap) : 0;
}

int entbuild_have_hooks(void)
{
    return g_change != 0 || g_add != 0;
}
