#include "game/dagger/play/words.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/tables/careers.h"
#include "engine/core/df.h"
#include "engine/core/log.h"

/* See words.h. */

/* Records that name records: how deep a macro's text may go, and how long any line can be. */
#define DEPTH_MOST 3
#define LINE_MOST 512
/* The greeting, the follow-up and "friend or stranger", each three records by tone. */
#define RECORD_GREETING 7215
#define RECORD_FOLLOW_UP 7218
#define RECORD_NAME_FOR 7221
/* A race's oaths: its own record, in the order of g_oath_of_race. */
#define RECORD_OATHS 201

/* Which of the oath records each race (RACE_*) swears by. */
static const signed char g_oath_of_race[9] = { 3, 3, 2, 0, 7, 6, 5, 1, 4 };

static const Words *g_words;

static int text_length(const char *s)
{
    int n;

    n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static void expand(const char *src, char *out, int cap, int depth);

void words_use(const Words *w)
{
    g_words = w;
}

/* A record chosen and filled in, as a macro's text. */
static void say_into(int record, char *out, int cap, int depth)
{
    char line[LINE_MOST];
    int n;

    n = textrsc_variants(record);
    out[0] = 0;
    if (n > 0 && textrsc_variant(record, combat_roll(n), line, (int)sizeof(line)) >= 0) {
        expand(line, out, cap, depth + 1);
    }
}

/* How the person speaks of the player: Sir or Lady. */
static const char *honorific(const Words *w)
{
    return w->pc && w->pc->female ? "Lady" : "Sir";
}

/* The race's name; "stranger" for no character. */
static const char *race_name(const Words *w)
{
    const CareerRace *r;

    r = w->pc ? career_race(w->pc->race) : 0;
    return r ? r->name : "stranger";
}

/* The character's name up to its first space. */
static void first_name(const Words *w, char *out, int cap)
{
    int i;

    out[0] = 0;
    if (!w->pc) {
        return;
    }
    for (i = 0; i < cap - 1 && w->pc->name[i] && w->pc->name[i] != ' '; i++) {
        out[i] = w->pc->name[i];
    }
    out[i] = 0;
}

/* What macro says, in out: its name without the %, the letters read. */
static void say_macro(const char *macro, char *out, int cap, int depth)
{
    const Words *w;
    const char *text;
    int race;

    w = g_words;
    out[0] = 0;
    if (!w || depth >= DEPTH_MOST) {
        return;
    }
    text = 0;
    if (df_eq(macro, "pcn") || df_eq(macro, "pc")) {
        text = w->pc ? w->pc->name : "";
    } else if (df_eq(macro, "pcf") || df_eq(macro, "pcnf")) {
        first_name(w, out, cap);
        return;
    } else if (df_eq(macro, "ra")) {
        text = race_name(w);
    } else if (df_eq(macro, "hnr") || df_eq(macro, "pct") || df_eq(macro, "t")) {
        text = honorific(w);
    } else if (df_eq(macro, "oth")) {
        race = w->pc ? w->pc->race : 0;
        say_into(RECORD_OATHS + g_oath_of_race[race >= 0 && race < 9 ? race : 0], out, cap, depth);
        return;
    } else if (df_eq(macro, "1com")) {
        say_into(RECORD_GREETING + w->tone, out, cap, depth);
        return;
    } else if (df_eq(macro, "2com")) {
        say_into(RECORD_FOLLOW_UP + w->tone, out, cap, depth);
        return;
    } else if (df_eq(macro, "n")) {
        /* A stranger they do not like is named as one; else by name. */
        if (w->reaction <= 0) {
            say_into(RECORD_NAME_FOR + w->tone, out, cap, depth);
        } else {
            first_name(w, out, cap);
        }
        return;
    } else if (df_eq(macro, "key")) {
        text = w->key;
    } else if (df_eq(macro, "loc")) {
        text = w->loc;
    } else if (df_eq(macro, "di")) {
        text = w->direction;
    } else if (df_eq(macro, "hnt") || df_eq(macro, "hnt2")) {
        text = w->hint;
    } else if (df_eq(macro, "cn")) {
        text = w->city;
    } else if (df_eq(macro, "rn") || df_eq(macro, "reg")) {
        text = w->region;
    } else if (df_eq(macro, "cpn")) {
        text = w->place;
    } else if (df_eq(macro, "cri")) {
        text = w->crime;
    } else if (df_eq(macro, "pen")) {
        text = w->penalty;
    } else if (macro[0] == 'f') {
        /* %fnpc, %fpc and the factions round them: the one faction this person answers for, for now. */
        text = w->faction;
    } else if (df_eq(macro, "a")) {
        log_format(out, cap, "%d", w->amount);
        return;
    } else if (df_eq(macro, "dip")) {
        log_format(out, cap, "%d", w->days);
        return;
    }
    if (text) {
        df_copy(out, text, cap);
    }
}

static void expand(const char *src, char *out, int cap, int depth)
{
    int n;

    n = 0;
    while (*src && n < cap - 1) {
        if (*src == '%' && ((src[1] >= 'a' && src[1] <= 'z') || (src[1] >= '0' && src[1] <= '9'))) {
            char macro[8];
            char said[LINE_MOST];
            int k;
            int i;

            src++;
            k = 0;
            while (((*src >= 'a' && *src <= 'z') || (*src >= '0' && *src <= '9')) && k < (int)sizeof(macro) - 1) {
                macro[k++] = *src++;
            }
            macro[k] = 0;
            say_macro(macro, said, (int)sizeof(said), depth);
            for (i = 0; said[i] && n < cap - 1; i++) {
                out[n++] = said[i];
            }
        } else {
            out[n++] = *src++;
        }
    }
    out[n] = 0;
}

void words_expand(const char *src, char *out, int cap)
{
    expand(src, out, cap, 0);
}

/* 1 when say_macro has an answer for macro. */
static int macro_known(const char *macro)
{
    static const char *const known[] = {
        "pcn", "pc", "pcf", "pcnf", "ra", "hnr", "pct", "t", "oth", "1com", "2com", "n", "key", "loc",
        "di", "hnt", "hnt2", "cn", "rn", "reg", "cpn", "cri", "pen", "a", "dip", "fnpc", "fpc"
    };
    int i;

    for (i = 0; i < (int)(sizeof(known) / sizeof(known[0])); i++) {
        if (df_eq(macro, known[i])) {
            return 1;
        }
    }
    return 0;
}

int words_say_plain(int record, char *out, int cap)
{
    char line[LINE_MOST];
    const char *s;
    int n;

    n = textrsc_variants(record);
    out[0] = 0;
    if (n < 1 || textrsc_variant(record, combat_roll(n), line, (int)sizeof(line)) < 0) {
        return -1;
    }
    for (s = line; *s; s++) {
        if (*s == '%') {
            char macro[8];
            int k;

            k = 0;
            s++;
            while (((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')) && k < (int)sizeof(macro) - 1) {
                macro[k++] = *s++;
            }
            macro[k] = 0;
            /* A person's name for %n needs a stranger to name: not in a rumour. */
            if (!macro_known(macro) || df_eq(macro, "n")) {
                return -1;
            }
            s--;
        }
    }
    expand(line, out, cap, 0);
    return text_length(out);
}

int words_say(int record, char *out, int cap)
{
    if (textrsc_variants(record) < 1) {
        out[0] = 0;
        return -1;
    }
    say_into(record, out, cap, 0);
    return text_length(out);
}
