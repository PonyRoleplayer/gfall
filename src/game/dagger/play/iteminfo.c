#include "game/dagger/play/iteminfo.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/tables/items.h"
#include "engine/core/log.h"

/* See iteminfo.h. */

#define TEXT_ARMOR 1000
#define TEXT_WEAPON 1001
#define TEXT_MISC 1003
#define TEXT_LETTER 1007
#define TEXT_ARROW 1011
#define TEXT_ARMOR_NO_MATERIAL 1014
#define RECORD_MAX 512

int iteminfo_record(const Item *it)
{
    const ArmorTemplate *a;

    a = armor_template(it->template_id);
    if (a) {
        /* A helm and a shield show no material (classic's, DFU's default). */
        return a->shield || it->template_id == ITEM_HELM ? TEXT_ARMOR_NO_MATERIAL : TEXT_ARMOR;
    }
    if (it->template_id == ITEM_ARROW) {
        return TEXT_ARROW;
    }
    if (weapon_template(it->template_id)) {
        return TEXT_WEAPON;
    }
    return it->template_id == ITEM_LETTER ? TEXT_LETTER : TEXT_MISC;
}

/* The condition as a word, by the fifth of what it was made with it has left. */
static const char *condition_word(const Item *it)
{
    int percent;

    if (!it->condition_max) {
        return "new";
    }
    percent = (int)((df_u32)it->condition * 100u / it->condition_max);
    if (percent >= 100) {
        return "new";
    }
    if (percent >= 80) {
        return "almost new";
    }
    if (percent >= 60) {
        return "slightly used";
    }
    if (percent >= 40) {
        return "used";
    }
    if (percent >= 20) {
        return "worn";
    }
    return percent > 0 ? "battered" : "useless";
}

/* A weight with its trailing zeros dropped: 4.5, 0.25, 12. */
static void weight_text(float kg, char *out, int cap)
{
    int n;

    log_format(out, cap, "%.2f", kg);
    for (n = 0; out[n]; n++) {
    }
    while (n > 1 && out[n - 1] == '0') {
        out[--n] = 0;
    }
    if (n > 1 && out[n - 1] == '.') {
        out[n - 1] = 0;
    }
}

/* One macro's text. 0 when name is none of ours; *len is how much of the source it took. */
static int macro(const char *src, const Item *it, char *out, int cap)
{
    static const char *const names[] = { "%mat", "%arm", "%wep", "%it", "%mod", "%wdm", "%qua", "%kg", "%wth" };
    const ItemTemplate *t;
    const Material *m;
    int k;
    int which;
    int len;

    which = -1;
    len = 0;
    for (k = 0; k < (int)(sizeof(names) / sizeof(names[0])); k++) {
        int n;

        for (n = 0; names[k][n] && src[n] == names[k][n]; n++) {
        }
        if (!names[k][n] && n > len) {
            which = k;
            len = n;
        }
    }
    if (which < 0) {
        return 0;
    }
    t = item_template(it->template_id);
    m = material(it->material);
    switch (which) {
    case 0:
        log_format(out, cap, "%s", m ? m->name : "");
        break;
    case 1:
    case 2:
    case 3:
        log_format(out, cap, "%s", t ? t->name : "");
        break;
    case 4: {
        const ArmorTemplate *a;

        a = armor_template(it->template_id);
        log_format(out, cap, "%d", a ? (a->shield ? a->shield : (m ? m->armor : 0)) : 0);
        break;
    }
    case 5: {
        const WeaponTemplate *w;

        w = weapon_template(it->template_id);
        log_format(out, cap, "%d-%d", w ? w->damage_min + (m ? m->modifier : 0) : 0, w ? w->damage_max + (m ? m->modifier : 0) : 0);
        break;
    }
    case 6:
        log_format(out, cap, "%s", condition_word(it));
        break;
    case 7:
        weight_text(it->weight * (float)(it->count ? it->count : 1), out, cap);
        break;
    default:
        log_format(out, cap, "%u", (unsigned)it->value * (unsigned)(it->count ? it->count : 1));
        break;
    }
    return len;
}

/* A line of the record with its macros filled and its ends trimmed, into out. */
static void expand_line(const char *src, int n, const Item *it, char *out, int cap)
{
    int at;
    int i;

    at = 0;
    for (i = 0; i < n && at < cap - 1;) {
        if (src[i] == '%') {
            char text[48];
            int len;
            int j;

            len = macro(src + i, it, text, (int)sizeof(text));
            if (len > 0) {
                for (j = 0; text[j] && at < cap - 1; j++) {
                    out[at++] = text[j];
                }
                i += len;
                continue;
            }
        }
        out[at++] = src[i++];
    }
    while (at > 0 && out[at - 1] == ' ') {
        at--;
    }
    out[at] = 0;
}

int iteminfo_lines(const Item *it, char *out, int line_chars, int max)
{
    char record[RECORD_MAX];
    int length;
    int count;
    int start;
    int i;

    count = 0;
    length = textrsc_read(iteminfo_record(it), record, (int)sizeof(record));
    if (length <= 0) {
        item_name(it, out + count * line_chars, line_chars);
        count++;
        if (count < max) {
            char kg[24];

            weight_text(it->weight * (float)(it->count ? it->count : 1), kg, (int)sizeof(kg));
            log_format(out + count * line_chars, line_chars, "Weight: %s kilograms", kg);
            count++;
        }
        return count;
    }
    start = 0;
    for (i = 0; i <= length && count < max; i++) {
        if (i == length || record[i] == '\n') {
            if (i > start) {
                char *line;

                line = out + count * line_chars;
                expand_line(record + start, i - start, it, line, line_chars);
                if (line[0]) {
                    /* A thing with no condition (gold, a horse) has no line for it. */
                    if (!it->condition_max && line[0] == 'C' && line[1] == 'o' && line[2] == 'n' && line[3] == 'd') {
                        line[0] = 0;
                    } else {
                        count++;
                    }
                }
            }
            start = i + 1;
        }
    }
    return count;
}
