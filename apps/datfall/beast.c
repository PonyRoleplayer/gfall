#include "beast.h"

static int field_text(const DatFile *f, int kind, int row, const char *want, char *dst, int cap)
{
    int n;
    int i;
    int type;
    int size;
    int count;
    char name[32];

    if (dst && cap > 0) {
        dst[0] = 0;
    }
    if (!f || !want || kind < 0 || row < 0) {
        return 0;
    }
    n = dat_fields(f, kind);
    for (i = 0; i < n; i++) {
        name[0] = 0;
        if (!dat_field(f, kind, i, name, (int)sizeof(name), &type, &size, &count)) {
            break;
        }
        if (df_eq(name, want)) {
            return dat_value(f, kind, row, i, 0, dst, cap);
        }
    }
    return 0;
}

static int field_num(const DatFile *f, int kind, int row, const char *want, int *out)
{
    char text[32];
    int i;
    int n;
    int sign;

    if (!field_text(f, kind, row, want, text, (int)sizeof(text)) || !text[0]) {
        return 0;
    }
    i = 0;
    sign = 1;
    if (text[0] == '-') {
        sign = -1;
        i = 1;
    }
    n = 0;
    if (text[i] < '0' || text[i] > '9') {
        return 0;
    }
    while (text[i] >= '0' && text[i] <= '9') {
        n = n * 10 + (text[i] - '0');
        i++;
    }
    *out = sign * n;
    return 1;
}

static void put_pose(BeastPose *p, const char *label, int archive, int record, int views)
{
    int i;

    for (i = 0; i < 15 && label[i]; i++) {
        p->label[i] = label[i];
    }
    p->label[i] = 0;
    p->archive = archive;
    p->record = record;
    p->views = views;
}

int beast_poses(const DatFile *f, int row, BeastPose *out, int cap)
{
    int kind;
    int male;
    int idle;
    int corpse;
    int crec;
    int n;
    int archive;

    if (!f || !out || cap < 1 || row < 0) {
        return 0;
    }
    kind = dat_find_tag(f, "ENEM");
    if (kind < 0 || row >= dat_rows(f, kind)) {
        return 0;
    }
    male = 0;
    if (!field_num(f, kind, row, "male", &male) || male <= 0) {
        archive = -1;
    } else {
        archive = male;
    }
    n = 0;
    if (n < cap) {
        put_pose(&out[n], "Walk", archive, 0, 5);
        n++;
    }
    if (n < cap) {
        put_pose(&out[n], "Attack", archive, 5, 5);
        n++;
    }
    if (n < cap) {
        put_pose(&out[n], "Hurt", archive, 10, 5);
        n++;
    }
    idle = 0;
    if (archive > 0 && field_num(f, kind, row, "has_idle", &idle) && idle) {
        if (n < cap) {
            put_pose(&out[n], "Stand", archive, 15, 5);
            n++;
        }
    }
    corpse = 0;
    crec = 0;
    if (field_num(f, kind, row, "corpse_archive", &corpse) && corpse > 0) {
        if (!field_num(f, kind, row, "corpse_record", &crec) || crec < 0) {
            crec = 0;
        }
        if (n < cap) {
            put_pose(&out[n], "Corpse", corpse, crec, 1);
            n++;
        }
    }
    return n;
}
