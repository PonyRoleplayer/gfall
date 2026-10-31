#include "paper.h"

#include "game/dagger/formats/cif.h"

/*
 * Field names are the ones the record file prints, which are not the
 * CareerRace member names: the clothed bodies are clothed_male and
 * clothed_female.
 */
static const char *g_field[PAPER_PARTS] = {
    "background",
    "body_male",
    "clothed_male",
    "body_female",
    "clothed_female",
    "heads_male",
    "heads_female"
};

static const char *g_label[PAPER_PARTS] = {
    "Background",
    "Body",
    "Clothed",
    "Female body",
    "Female clothed",
    "Heads",
    "Female heads"
};

const char *paper_field(int part)
{
    if (part < 0 || part >= PAPER_PARTS) {
        return 0;
    }
    return g_field[part];
}

const char *paper_label(int part)
{
    if (part < 0 || part >= PAPER_PARTS) {
        return "";
    }
    return g_label[part];
}

int paper_file(const DatFile *f, int row, int part, char *dst, int cap)
{
    int kind;
    int n;
    int i;
    int type;
    int size;
    int count;
    char name[32];
    const char *want;

    if (dst && cap > 0) {
        dst[0] = 0;
    }
    want = paper_field(part);
    if (!f || !want || row < 0) {
        return 0;
    }
    kind = dat_find_tag(f, "RACE");
    if (kind < 0 || row >= dat_rows(f, kind)) {
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

int paper_heads(const char *name)
{
    CifSet cif;
    int n;

    if (!name || !name[0]) {
        return 0;
    }
    if (!cif_open(name, &cif)) {
        return 0;
    }
    n = cif.n;
    cif_close(&cif);
    return n;
}
