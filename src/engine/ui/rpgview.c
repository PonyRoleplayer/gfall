#include "engine/ui/rpgview.h"
#include "engine/core/log.h"

/* See rpgview.h. */

void rpgview_open(RpgView *v, RpgSheet *sheet, const RpgRules *rules)
{
    v->sheet = sheet;
    v->rules = rules;
    v->cursor = 0;
}

int rpgview_command(RpgView *v, int command)
{
    switch (command) {
    case RPGVIEW_UP:
        if (v->cursor > 0) {
            v->cursor--;
            return 1;
        }
        return 0;
    case RPGVIEW_DOWN:
        if (v->cursor + 1 < v->rules->nattrs) {
            v->cursor++;
            return 1;
        }
        return 0;
    case RPGVIEW_RAISE:
        return rpg_raise(v->sheet, v->rules, v->cursor);
    case RPGVIEW_LOWER:
        return rpg_lower(v->sheet, v->rules, v->cursor);
    case RPGVIEW_CONFIRM:
        return rpg_commit(v->sheet, v->rules);
    default:
        return 0;
    }
}

int rpgview_lines(const RpgView *v)
{
    return 1 + v->rules->nattrs + v->rules->nvitals;
}

int rpgview_line(const RpgView *v, int i, char *out, int cap)
{
    const RpgRules *r;
    const RpgSheet *s;
    int a;
    int k;

    r = v->rules;
    s = v->sheet;
    if (i == 0) {
        log_format(out, cap, "Level %d  XP %d/%d  Points %d", s->level, s->xp, rpg_xp_next(r, s->level), s->points);
        return 1;
    }
    a = i - 1;
    if (a >= 0 && a < r->nattrs) {
        log_format(out, cap, "%c%s %d", a == v->cursor ? '>' : ' ', r->attr_name[a], s->attr[a]);
        return 1;
    }
    k = a - r->nattrs;
    if (k >= 0 && k < r->nvitals) {
        log_format(out, cap, " %s %d/%d", r->vital[k].name, s->vital[k], rpg_vital_max(s, r, k));
        return 1;
    }
    return 0;
}

void rpgview_draw(const RpgView *v, int x, int top, int line_h, RpgViewText text)
{
    char line[RPGVIEW_LINE];
    int i;

    /* The screen's y counts up: each line lower than the last. */
    for (i = 0; rpgview_line(v, i, line, (int)sizeof(line)); i++) {
        text(x, top - i * line_h, line);
    }
}
