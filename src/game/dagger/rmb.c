#include "game/dagger/rmb.h"
#include "game/dagger/bsa.h"
#include "game/dagger/texture.h"
#include "engine/core/mem.h"

/*
 * RMB fixed header is 6776 bytes, then one subrecord per building.
 * A subrecord is an exterior list and an interior list. BlockDataSizes
 * jumps past both, including the padding byte. Misc props follow the
 * subrecords. Model ids are objectId1 * 100 + objectId2. File yaw is
 * the same 2048-unit circle as an RDB, and place_model's negation is
 * instance_set_rotation with the angle flipped. Y in the file is down.
 *
 * A building sits at (X, 0, 4096 - Z) and its model at (X, -Y, Z) in
 * that frame. Props use (X, -Y - 4, Z + 4096). Ground is a 16 by 16
 * grid of 256-unit quads, emitted as 512-unit patches so the cull can
 * drop the ones behind the camera. Tile row 0 is the high-Z side,
 * matching the 4096 - Z flip, so a building and the dirt under it
 * share one axis.
 */

#define FLD_BYTES 6776
#define FLD_SIZES 1603
#define FLD_GROUND 1739
#define RMB_SIDE 4096.0f
#define GROUND_TILE 256.0f
/* Two cells on a side. 512 units is inside the solid fog, so a patch behind the camera drops out. */
#define PATCH_TILES 2
#define PATCH_N 8
#define GROUND_TEX 64.0f
#define PROP_DROP 4.0f
#define ROT_STEP (DF_TAU / 2048.0f)
#define EDITOR_ARCHIVE 199
#define MODEL_BYTES 66
#define FLAT_BYTES 17

static void place_child(Instance *in, float sub_yaw, float model_yaw,
                        float lx, float ly, float lz,
                        float ox, float oy, float oz)
{
    Instance parent;

    parent.x = ox;
    parent.y = oy;
    parent.z = oz;
    instance_set_rotation(&parent, 0.0f, df_wrap_tau(-sub_yaw), 0.0f);
    instance_point(&parent, lx, ly, lz, &in->x, &in->y, &in->z);
    instance_set_rotation(in, 0.0f, df_wrap_tau(-(sub_yaw + model_yaw)), 0.0f);
}

/*
 * Place model id; record is the building it belongs to (-1 the block's
 * own). prop: an interior's clutter (object type 3), whose file height
 * is where its lowest point stands, not negated, as Daggerfall Unity
 * places it (DaggerfallInterior.AddModels). The block's index of the
 * model, or -1.
 */
static int add_model(Block *block, ArchEnt *cache, int *ncache, Bsa *arch,
                     int id, float sub_yaw, float model_yaw,
                     float lx, float ly, float lz,
                     float ox, float oy, float oz, int record, int prop)
{
    Mesh *mesh;
    Instance *in;
    int at;

    if (id <= 0 || block->ninst >= block->inst_cap) {
        return -1;
    }
    mesh = arch_cached(cache, ncache, arch, (df_u32)id);
    if (!mesh) {
        return -1;
    }
    if (prop) {
        mesh_measure(mesh);
        ly = -ly - (mesh->cy - mesh->ey);
    }
    at = block->ninst;
    in = &block->inst[at];
    in->mesh = mesh;
    place_child(in, sub_yaw, model_yaw, lx, ly, lz, ox, oy, oz);
    block->act[at].id = (df_u32)id;
    block->act[at].record = (df_i16)record;
    block->ninst++;
    block->ntris += mesh->ntris;
    return at;
}

static int model_id(const df_u8 *p)
{
    df_i16 id1;
    int id2;

    id1 = df_ri16(p);
    id2 = (int)p[2];
    if (id1 < 0) {
        return 0;
    }
    return (int)id1 * 100 + id2;
}

/* Object type 3 of an interior's models: clutter, a prop. */
#define PROP_TYPE 3

static void read_model(Block *block, ArchEnt *cache, int *ncache, Bsa *arch,
                       const df_u8 *p, float sub_yaw, float ox, float oy, float oz,
                       int record, int inside)
{
    float myaw;

    myaw = (float)df_ri16(p + 52) * ROT_STEP;
    add_model(block, cache, ncache, arch, model_id(p), sub_yaw, myaw,
              (float)df_ri32(p + 36), -(float)df_ri32(p + 40), (float)df_ri32(p + 44),
              ox, oy, oz, record, inside && (int)p[3] == PROP_TYPE);
}

/*
 * A flat at its file point turned by sub_yaw about the building's
 * origin (0 for the block's own). tag goes on it: FLAT_PERSON for the
 * people lists. An editor marker is kept as a Marker when the block
 * has room for them (an interior's), and dropped otherwise.
 */
static void add_flat(Block *block, const df_u8 *p, float sub_yaw, float ox, float oz, df_u16 tag)
{
    df_u16 bits;
    int archive;
    Flat *fl;
    Instance at;

    bits = df_ru16(p + 12);
    archive = (int)(bits >> 7);
    at.mesh = 0;
    place_child(&at, sub_yaw, 0.0f, (float)df_ri32(p), -(float)df_ri32(p + 4),
                (float)df_ri32(p + 8), ox, 0.0f, oz);
    if (archive == EDITOR_ARCHIVE) {
        if (block->nmarker < block->mark_cap) {
            Marker *mk;

            mk = &block->marker[block->nmarker++];
            mk->x = at.x;
            mk->y = at.y;
            mk->z = at.z;
            mk->record = (df_u16)(bits & 0x7f);
        }
        return;
    }
    if (block->nflat >= block->flat_cap) {
        return;
    }
    fl = &block->flat[block->nflat];
    fl->x = at.x;
    fl->y = at.y;
    fl->z = at.z;
    fl->w = 0.0f;
    fl->h = 0.0f;
    fl->file = (df_u16)archive;
    fl->image = (df_u16)(bits & 0x7f);
    fl->slot = 0xffff;
    /* TEXTURE.210: lamps and fires give light. */
    fl->glow = (df_u16)(archive == 210);
    fl->tag = tag;
    fl->sight = 0;
    fl->thin = 0;
    block->nflat++;
}

/* A people record's flat: FLAT_PERSON and its faction (i16 at 14). */
static df_u16 person_tag(const df_u8 *p)
{
    return (df_u16)(FLAT_PERSON | ((df_u16)df_ri16(p + 14) & 0x7fffu));
}

/* Exterior lists only. The interior list is inside the same size span. */
static void read_exterior(Block *block, ArchEnt *cache, int *ncache, Bsa *arch,
                          const df_u8 *rmb, df_u32 end, df_u32 at,
                          float sx, float sub_yaw, float sz, int record)
{
    int n3d;
    int nflat;
    int nsec;
    int npeople;
    int i;
    float oz;

    if (at + 17 > end) {
        return;
    }
    n3d = (int)rmb[at];
    nflat = (int)rmb[at + 1];
    nsec = (int)rmb[at + 2];
    npeople = (int)rmb[at + 3];
    at += 17;
    oz = RMB_SIDE - sz;
    for (i = 0; i < n3d && at + MODEL_BYTES <= end; i++) {
        read_model(block, cache, ncache, arch, rmb + at, sub_yaw, sx, 0.0f, oz, record, 0);
        at += MODEL_BYTES;
    }
    /*
     * A building's outside flats and people sit at their file point from
     * the block, not turned with the building, as they always have here.
     */
    for (i = 0; i < nflat && at + FLAT_BYTES <= end; i++) {
        add_flat(block, rmb + at, 0.0f, sx, oz, 0);
        at += FLAT_BYTES;
    }
    if ((df_u32)nsec > 256 || at + (df_u32)nsec * 16u > end) {
        return;
    }
    at += (df_u32)nsec * 16u;
    for (i = 0; i < npeople && at + FLAT_BYTES <= end; i++) {
        add_flat(block, rmb + at, 0.0f, sx, oz, person_tag(rmb + at));
        at += FLAT_BYTES;
    }
}

/*
 * Bits 0x40 and 0x80 are four orientations, not a mirror.
 * 00 leaves the picture, 01 turns it 90 degrees, 10 turns it 180,
 * 11 is the other 90. Corner 0 is low X low Z, then high X low Z,
 * high X high Z, low X high Z. Row 0 of the picture is v = 0, so the
 * sample is flipped against a v that grows with Z.
 */
static void corner_uv(int corner, int rot, int flip, float *u, float *v)
{
    float lx;
    float lz;
    float su;
    float sv;

    lx = (corner == 1 || corner == 2) ? 1.0f : 0.0f;
    lz = (corner >= 2) ? 1.0f : 0.0f;
    if (rot && flip) {
        su = 1.0f - lz;
        sv = lx;
    } else if (rot) {
        su = lz;
        sv = 1.0f - lx;
    } else if (flip) {
        su = 1.0f - lx;
        sv = 1.0f - lz;
    } else {
        su = lx;
        sv = lz;
    }
    *u = su * GROUND_TEX;
    *v = (1.0f - sv) * GROUND_TEX;
}

static void ground_tri(Tri *tri, float x0, float z0, float x1, float z1, float x2, float z2,
                       int c0, int c1, int c2, int rot, int flip, int file, int image)
{
    tri->x[0] = x0;
    tri->y[0] = 0.0f;
    tri->z[0] = z0;
    tri->x[1] = x1;
    tri->y[1] = 0.0f;
    tri->z[1] = z1;
    tri->x[2] = x2;
    tri->y[2] = 0.0f;
    tri->z[2] = z2;
    corner_uv(c0, rot, flip, &tri->u[0], &tri->v[0]);
    corner_uv(c1, rot, flip, &tri->u[1], &tri->v[1]);
    corner_uv(c2, rot, flip, &tri->u[2], &tri->v[2]);
    tri->tex_file = (df_u16)file;
    tri->tex_image = (df_u16)image;
    tri->slot = 0xffff;
}

static int cell_live(const df_u8 *tiles, int count, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx > 15 || ty > 15) {
        return 0;
    }
    return ((int)tiles[ty * 16 + tx] & 0x3f) < count;
}

static int patch_quads(const df_u8 *tiles, int count, int px, int py)
{
    int n;
    int ty;
    int tx;

    n = 0;
    for (ty = py * PATCH_TILES; ty < py * PATCH_TILES + PATCH_TILES; ty++) {
        for (tx = px * PATCH_TILES; tx < px * PATCH_TILES + PATCH_TILES; tx++) {
            if (cell_live(tiles, count, tx, ty)) {
                n++;
            }
        }
    }
    return n;
}

static int count_patches(const df_u8 *tiles, int count)
{
    int n;
    int py;
    int px;

    if (count < 1) {
        return 0;
    }
    n = 0;
    for (py = 0; py < PATCH_N; py++) {
        for (px = 0; px < PATCH_N; px++) {
            if (patch_quads(tiles, count, px, py) > 0) {
                n++;
            }
        }
    }
    return n;
}

static void add_patch(Block *block, const df_u8 *tiles, int file, int count,
                      int px, int py)
{
    Mesh *mesh;
    Tri *tris;
    Face *face;
    int quads;
    int ty;
    int n;

    quads = patch_quads(tiles, count, px, py);
    if (quads < 1 || block->ninst >= block->inst_cap) {
        return;
    }
    tris = (Tri *)mem_alloc((df_u32)quads * 2u * (df_u32)sizeof(Tri));
    mesh = (Mesh *)mem_alloc(sizeof(Mesh));
    face = (Face *)mem_alloc(sizeof(Face));
    if (!tris || !mesh || !face) {
        return;
    }
    n = 0;
    for (ty = py * PATCH_TILES; ty < py * PATCH_TILES + PATCH_TILES; ty++) {
        int tx;

        for (tx = px * PATCH_TILES; tx < px * PATCH_TILES + PATCH_TILES; tx++) {
            int bits;
            int record;
            float x0;
            float z0;
            float x1;
            float z1;

            if (!cell_live(tiles, count, tx, ty)) {
                continue;
            }
            bits = (int)tiles[ty * 16 + tx];
            record = bits & 0x3f;
            x0 = (float)tx * GROUND_TILE;
            z0 = (float)(15 - ty) * GROUND_TILE;
            x1 = x0 + GROUND_TILE;
            z1 = z0 + GROUND_TILE;
            ground_tri(&tris[n], x0, z0, x1, z0, x1, z1, 0, 1, 2,
                       (bits & 0x40) ? 1 : 0, (bits & 0x80) ? 1 : 0, file, record);
            n++;
            ground_tri(&tris[n], x0, z0, x1, z1, x0, z1, 0, 2, 3,
                       (bits & 0x40) ? 1 : 0, (bits & 0x80) ? 1 : 0, file, record);
            n++;
        }
    }
    if (n < 1) {
        return;
    }
    mesh->id = 0;
    mesh->tris = tris;
    mesh->ntris = n;
    face->nx = 0.0f;
    face->ny = 1.0f;
    face->nz = 0.0f;
    face->first = 0;
    face->ntris = n;
    mesh->faces = face;
    mesh->nfaces = 1;
    mesh->face_cap = 1;
    mesh_measure(mesh);
    block->inst[block->ninst].mesh = mesh;
    block->inst[block->ninst].x = 0.0f;
    block->inst[block->ninst].y = 0.0f;
    block->inst[block->ninst].z = 0.0f;
    instance_set_rotation(&block->inst[block->ninst], 0.0f, 0.0f, 0.0f);
    block->ninst++;
    block->ntris += n;
}

static void add_ground(Block *block, const df_u8 *tiles, int file)
{
    int count;
    int py;
    int px;

    count = texture_record_count(file);
    if (count < 1) {
        return;
    }
    for (py = 0; py < PATCH_N; py++) {
        for (px = 0; px < PATCH_N; px++) {
            add_patch(block, tiles, file, count, px, py);
        }
    }
}

static void tally_exterior(const df_u8 *rmb, df_u32 end, df_u32 at, int *ninst, int *nflat)
{
    int n3d;
    int listed;
    int nsec;
    int npeople;
    int i;

    if (at + 17 > end) {
        return;
    }
    n3d = (int)rmb[at];
    listed = (int)rmb[at + 1];
    nsec = (int)rmb[at + 2];
    npeople = (int)rmb[at + 3];
    at += 17;
    for (i = 0; i < n3d && at + MODEL_BYTES <= end; i++) {
        if (model_id(rmb + at) > 0) {
            (*ninst)++;
        }
        at += MODEL_BYTES;
    }
    for (i = 0; i < listed && at + FLAT_BYTES <= end; i++) {
        if ((int)(df_ru16(rmb + at + 12) >> 7) != EDITOR_ARCHIVE) {
            (*nflat)++;
        }
        at += FLAT_BYTES;
    }
    if (nsec > 256 || at + (df_u32)nsec * 16u > end) {
        return;
    }
    at += (df_u32)nsec * 16u;
    for (i = 0; i < npeople && at + FLAT_BYTES <= end; i++) {
        if ((int)(df_ru16(rmb + at + 12) >> 7) != EDITOR_ARCHIVE) {
            (*nflat)++;
        }
        at += FLAT_BYTES;
    }
}

/* The ARCH3D ids of one subrecord's outside models, as read_exterior walks them. */
static int ids_exterior(const df_u8 *rmb, df_u32 end, df_u32 at, df_u32 *ids, int n, int cap)
{
    int n3d;
    int i;

    if (at + 17 > end) {
        return n;
    }
    n3d = (int)rmb[at];
    at += 17;
    for (i = 0; i < n3d && at + MODEL_BYTES <= end; i++) {
        if (model_id(rmb + at) > 0 && n < cap) {
            ids[n++] = (df_u32)model_id(rmb + at);
        }
        at += MODEL_BYTES;
    }
    return n;
}

int rmb_model_ids(const df_u8 *rmb, df_u32 size, df_u32 *ids, int cap)
{
    df_u32 pos;
    int nsub;
    int n;
    int i;

    if (!rmb || size < (df_u32)FLD_BYTES) {
        return 0;
    }
    nsub = (int)rmb[0];
    if (nsub > 32) {
        return 0;
    }
    n = 0;
    pos = (df_u32)FLD_BYTES;
    for (i = 0; i < nsub; i++) {
        df_i32 span;

        span = df_ri32(rmb + FLD_SIZES + i * 4);
        if (span < 17 || pos + (df_u32)span > size) {
            return n;
        }
        n = ids_exterior(rmb, pos + (df_u32)span, pos, ids, n, cap);
        pos += (df_u32)span;
    }
    for (i = 0; i < (int)rmb[1] && pos + MODEL_BYTES <= size; i++) {
        if (model_id(rmb + pos) > 0 && n < cap) {
            ids[n++] = (df_u32)model_id(rmb + pos);
        }
        pos += MODEL_BYTES;
    }
    return n;
}

Block *rmb_load(const df_u8 *rmb, df_u32 size, Bsa *arch, int ground_file,
                ArchEnt *cache, int *ncache)
{
    Block *block;
    int nsub;
    int i;
    int want_inst;
    int want_flat;
    df_u32 pos;

    if (!rmb || !arch || !cache || !ncache || size < (df_u32)FLD_BYTES) {
        return 0;
    }
    nsub = (int)rmb[0];
    if (nsub > 32) {
        return 0;
    }
    want_inst = 0;
    want_flat = 0;
    if (ground_file > 1) {
        int records;

        records = texture_record_count(ground_file);
        want_inst += count_patches(rmb + FLD_GROUND, records);
    }
    pos = (df_u32)FLD_BYTES;
    for (i = 0; i < nsub; i++) {
        df_i32 span;

        span = df_ri32(rmb + FLD_SIZES + i * 4);
        if (span < 17 || pos + (df_u32)span > size) {
            break;
        }
        tally_exterior(rmb, pos + (df_u32)span, pos, &want_inst, &want_flat);
        pos += (df_u32)span;
    }
    for (i = 0; i < (int)rmb[1] && pos + MODEL_BYTES <= size; i++) {
        if (model_id(rmb + pos) > 0) {
            want_inst++;
        }
        pos += MODEL_BYTES;
    }
    for (i = 0; i < (int)rmb[2] && pos + FLAT_BYTES <= size; i++) {
        if ((int)(df_ru16(rmb + pos + 12) >> 7) != EDITOR_ARCHIVE) {
            want_flat++;
        }
        pos += FLAT_BYTES;
    }
    block = (Block *)mem_alloc(sizeof(Block));
    if (!block) {
        return 0;
    }
    if (!block_reserve(block, want_inst, want_flat, 0)) {
        return 0;
    }
    if (ground_file > 1) {
        add_ground(block, rmb + FLD_GROUND, ground_file);
    }
    pos = (df_u32)FLD_BYTES;
    for (i = 0; i < nsub; i++) {
        df_i32 span;
        const df_u8 *bp;
        float sx;
        float sz;
        float syaw;

        span = df_ri32(rmb + FLD_SIZES + i * 4);
        if (span < 17 || pos + (df_u32)span > size) {
            return block;
        }
        bp = rmb + 3 + i * 20;
        sx = (float)df_ri32(bp + 8);
        sz = (float)df_ri32(bp + 12);
        syaw = (float)df_ri32(bp + 16) * ROT_STEP;
        read_exterior(block, cache, ncache, arch, rmb, pos + (df_u32)span, pos, sx, syaw, sz, i);
        pos += (df_u32)span;
    }
    for (i = 0; i < (int)rmb[1] && pos + MODEL_BYTES <= size; i++) {
        read_model(block, cache, ncache, arch, rmb + pos, 0.0f, 0.0f, -PROP_DROP, RMB_SIDE, -1, 0);
        pos += MODEL_BYTES;
    }
    for (i = 0; i < (int)rmb[2] && pos + FLAT_BYTES <= size; i++) {
        add_flat(block, rmb + pos, 0.0f, 0.0f, RMB_SIDE, 0);
        pos += FLAT_BYTES;
    }
    return block;
}

/*
 * A subrecord's list header: models, flats, section 3, people and doors,
 * then 12 bytes. The doors are 19 bytes each (Daggerfall Unity's
 * RmbBlockDoorRecord): x, y, z (i32), yaw (i16 at 12), an open turn
 * (i16, unused), the door model (byte at 16), two bytes more.
 */
#define DOOR_BYTES 19
/* An interior's hinged doors are models 9000 to 9004, by the record's model byte mod 5. */
#define DOOR_MODEL 9000

/* Where the list after the one at `at` begins: the interior follows the exterior. */
static df_u32 skip_list(const df_u8 *rmb, df_u32 end, df_u32 at)
{
    df_u32 bytes;

    if (at + 17 > end) {
        return end;
    }
    bytes = 17u + (df_u32)rmb[at] * MODEL_BYTES + (df_u32)rmb[at + 1] * FLAT_BYTES
        + (df_u32)rmb[at + 2] * 16u + (df_u32)rmb[at + 3] * FLAT_BYTES
        + (df_u32)rmb[at + 4] * DOOR_BYTES;
    return at + bytes > end ? end : at + bytes;
}

/* Where subrecord record's lists begin and end, from the block's sizes. 0 when it has none. */
static int find_record(const df_u8 *rmb, df_u32 size, int record, df_u32 *start, df_u32 *end)
{
    df_u32 pos;
    int i;

    if (record < 0 || record >= (int)rmb[0] || record >= 32) {
        return 0;
    }
    pos = (df_u32)FLD_BYTES;
    for (i = 0; i <= record; i++) {
        df_i32 span;

        span = df_ri32(rmb + FLD_SIZES + i * 4);
        if (span < 17 || pos + (df_u32)span > size) {
            return 0;
        }
        if (i == record) {
            *start = pos;
            *end = pos + (df_u32)span;
            return 1;
        }
        pos += (df_u32)span;
    }
    return 0;
}

int rmb_building(const df_u8 *rmb, df_u32 size, int record, int *type, int *faction)
{
    const df_u8 *bd;

    if (!rmb || size < (df_u32)FLD_BYTES || record < 0 || record >= 32 || record >= (int)rmb[0]) {
        return 0;
    }
    /* BuildingDataList, 26 bytes each, after the 640 bytes of positions. */
    bd = rmb + 3 + 640 + record * 26;
    *faction = (int)df_ru16(bd + 18);
    *type = (int)bd[24];
    return 1;
}

Block *rmb_load_interior(const df_u8 *rmb, df_u32 size, Bsa *arch, int record,
                         ArchEnt *cache, int *ncache)
{
    Block *block;
    df_u32 start;
    df_u32 end;
    df_u32 at;
    const df_u8 *bp;
    int n3d, nflat, nsec, npeople, ndoor;
    int nmark;
    int nkeep;
    int i;
    float sx, sz, syaw, oz;

    if (!rmb || !arch || !cache || !ncache || size < (df_u32)FLD_BYTES
        || !find_record(rmb, size, record, &start, &end)) {
        return 0;
    }
    at = skip_list(rmb, end, start);
    if (at + 17 > end) {
        return 0;
    }
    n3d = (int)rmb[at];
    nflat = (int)rmb[at + 1];
    nsec = (int)rmb[at + 2];
    npeople = (int)rmb[at + 3];
    ndoor = (int)rmb[at + 4];
    if (n3d < 1) {
        return 0;
    }
    /* Markers are kept apart, so count them out of the flats. */
    nmark = 0;
    {
        df_u32 f;

        f = at + 17u + (df_u32)n3d * MODEL_BYTES;
        for (i = 0; i < nflat && f + FLAT_BYTES <= end; i++) {
            if ((int)(df_ru16(rmb + f + 12) >> 7) == EDITOR_ARCHIVE) {
                nmark++;
            }
            f += FLAT_BYTES;
        }
    }
    nkeep = nflat - nmark + npeople;
    block = (Block *)mem_alloc(sizeof(Block));
    if (!block) {
        return 0;
    }
    mem_zero(block, sizeof(Block));
    if (!block_reserve(block, n3d + ndoor, nkeep, nmark)) {
        return 0;
    }
    /* The building's place in the block, as its outside is placed (read_exterior). */
    bp = rmb + 3 + record * 20;
    sx = (float)df_ri32(bp + 8);
    sz = (float)df_ri32(bp + 12);
    syaw = (float)df_ri32(bp + 16) * ROT_STEP;
    oz = RMB_SIDE - sz;
    at += 17;
    for (i = 0; i < n3d && at + MODEL_BYTES <= end; i++) {
        read_model(block, cache, ncache, arch, rmb + at, syaw, sx, 0.0f, oz, record, 1);
        at += MODEL_BYTES;
    }
    for (i = 0; i < nflat && at + FLAT_BYTES <= end; i++) {
        add_flat(block, rmb + at, syaw, sx, oz, 0);
        at += FLAT_BYTES;
    }
    if (nsec > 256 || at + (df_u32)nsec * 16u > end) {
        return block;
    }
    at += (df_u32)nsec * 16u;
    for (i = 0; i < npeople && at + FLAT_BYTES <= end; i++) {
        add_flat(block, rmb + at, syaw, sx, oz, person_tag(rmb + at));
        at += FLAT_BYTES;
    }
    for (i = 0; i < ndoor && at + DOOR_BYTES <= end; i++) {
        const df_u8 *p;
        int m;

        p = rmb + at;
        m = add_model(block, cache, ncache, arch, DOOR_MODEL + (int)p[16] % 5,
                      syaw, (float)df_ri16(p + 12) * ROT_STEP,
                      (float)df_ri32(p), -(float)df_ri32(p + 4), (float)df_ri32(p + 8),
                      sx, 0.0f, oz, record, 0);
        if (m >= 0) {
            block->act[m].hinged = 1;
        }
        at += DOOR_BYTES;
    }
    return block;
}
