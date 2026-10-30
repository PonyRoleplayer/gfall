#include "made.h"

static int parse_i(const char *s, int *out)
{
    int i;
    int n;
    int sign;

    if (!s || !s[0]) {
        return 0;
    }
    i = 0;
    sign = 1;
    if (s[0] == '-') {
        sign = -1;
        i = 1;
    }
    if (s[i] < '0' || s[i] > '9') {
        return 0;
    }
    n = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        n = n * 10 + (s[i] - '0');
        i++;
    }
    *out = sign * n;
    return 1;
}

/* Thousandths, from dat_value's three-decimal float. */
static int parse_milli(const char *s, int *out)
{
    int i;
    int whole;
    int frac;
    int digits;
    int place;

    if (!s || s[0] < '0' || s[0] > '9') {
        return 0;
    }
    i = 0;
    whole = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        whole = whole * 10 + (s[i] - '0');
        i++;
    }
    frac = 0;
    digits = 0;
    place = 100;
    if (s[i] == '.') {
        i++;
        while (s[i] >= '0' && s[i] <= '9' && digits < 3) {
            frac += (s[i] - '0') * place;
            place /= 10;
            digits++;
            i++;
        }
    }
    *out = whole * 1000 + frac;
    return 1;
}

static int field_at(const DatFile *f, int kind, int row, const char *want, char *dst, int cap)
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
    if (!f || kind < 0 || row < 0 || !want) {
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

static int row_id(const DatFile *f, int kind, int row, int *id)
{
    char text[16];

    text[0] = 0;
    if (!dat_value(f, kind, row, 0, 0, text, (int)sizeof(text))) {
        return 0;
    }
    return parse_i(text, id);
}

static int find_row(const DatFile *f, const char *tag, int id, int *kind_out)
{
    int kind;
    int rows;
    int row;
    int got;

    kind = f ? dat_find_tag(f, tag) : -1;
    if (kind < 0) {
        return -1;
    }
    if (kind_out) {
        *kind_out = kind;
    }
    rows = dat_rows(f, kind);
    for (row = 0; row < rows; row++) {
        got = 0;
        if (row_id(f, kind, row, &got) && got == id) {
            return row;
        }
    }
    return -1;
}

int made_weapon_at(const DatFile *f, int index, int *id, char *name, int namecap)
{
    int kind;
    int item_kind;
    int got;
    int item;

    if (id) {
        *id = 0;
    }
    if (name && namecap > 0) {
        name[0] = 0;
    }
    kind = f ? dat_find_tag(f, "WEAP") : -1;
    if (kind < 0 || index < 0 || index >= dat_rows(f, kind)) {
        return 0;
    }
    if (!row_id(f, kind, index, &got)) {
        return 0;
    }
    if (id) {
        *id = got;
    }
    item_kind = -1;
    item = find_row(f, "ITEM", got, &item_kind);
    if (item >= 0) {
        field_at(f, item_kind, item, "name", name, namecap);
    }
    if (name && namecap > 0 && !name[0]) {
        dat_value(f, kind, index, 0, 0, name, namecap);
    }
    return 1;
}

int made_material_at(const DatFile *f, int index, int *id, char *name, int namecap)
{
    int kind;
    int got;

    if (id) {
        *id = 0;
    }
    if (name && namecap > 0) {
        name[0] = 0;
    }
    kind = f ? dat_find_tag(f, "MATL") : -1;
    if (kind < 0 || index < 0 || index >= dat_rows(f, kind)) {
        return 0;
    }
    if (!row_id(f, kind, index, &got)) {
        return 0;
    }
    if (id) {
        *id = got;
    }
    field_at(f, kind, index, "name", name, namecap);
    return 1;
}

int made_cif(const DatFile *f, int weapon_id)
{
    int kind;
    int row;
    int cif;
    char text[16];

    kind = -1;
    row = find_row(f, "WEAP", weapon_id, &kind);
    if (row < 0) {
        return -1;
    }
    text[0] = 0;
    if (!field_at(f, kind, row, "cif", text, (int)sizeof(text)) || !parse_i(text, &cif)) {
        return -1;
    }
    return cif;
}

int made_of(const DatFile *f, int weapon_id, int material_id, MadeItem *out)
{
    int item_kind;
    int mat_kind;
    int item;
    int mat;
    int milli;
    int quarters;
    int weight_q;
    int price;
    int value_q;
    int hits;
    int condition_q;
    int n;
    int q;
    int r;
    char text[32];

    if (!out) {
        return 0;
    }
    out->quarters = 0;
    out->value = 0;
    out->condition = 0;
    out->cif = made_cif(f, weapon_id);
    item_kind = -1;
    mat_kind = -1;
    item = find_row(f, "ITEM", weapon_id, &item_kind);
    mat = find_row(f, "MATL", material_id, &mat_kind);
    if (item < 0 || mat < 0) {
        return 0;
    }
    text[0] = 0;
    if (!field_at(f, item_kind, item, "weight", text, (int)sizeof(text)) || !parse_milli(text, &milli)) {
        return 0;
    }
    /* (int)(kg * 4 + 0.001), kg in thousandths. */
    quarters = (milli * 4 + 1) / 1000;
    text[0] = 0;
    if (!field_at(f, mat_kind, mat, "weight_q", text, (int)sizeof(text)) || !parse_i(text, &weight_q)) {
        return 0;
    }
    n = quarters * weight_q;
    q = n / 4;
    r = n % 4;
    if (r < 0) {
        r = -r;
    }
    if (r > 2 || (r == 2 && (q & 1))) {
        q++;
    }
    out->quarters = q;
    text[0] = 0;
    price = 0;
    value_q = 0;
    if (field_at(f, item_kind, item, "price", text, (int)sizeof(text))) {
        parse_i(text, &price);
    }
    text[0] = 0;
    if (field_at(f, mat_kind, mat, "value_q", text, (int)sizeof(text))) {
        parse_i(text, &value_q);
    }
    out->value = price * 3 * value_q;
    text[0] = 0;
    hits = 0;
    condition_q = 0;
    if (field_at(f, item_kind, item, "hit_points", text, (int)sizeof(text))) {
        parse_i(text, &hits);
    }
    text[0] = 0;
    if (field_at(f, mat_kind, mat, "condition_q", text, (int)sizeof(text))) {
        parse_i(text, &condition_q);
    }
    out->condition = hits * condition_q / 4;
    return 1;
}
