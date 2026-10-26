#include "engine/ui/cmd.h"

#include "engine/core/df.h"
#include "engine/core/log.h"

/*
 * One table holds commands and variables: a variable is an entry with
 * get set. Lookups are a linear scan of under a hundred short names,
 * once per typed line, so there is no hash.
 */

#define CMD_MAX 96
/* Longest name in the table: help pads its columns to this. */
#define CMD_NAME 12

typedef struct CmdEntry {
    const char *name;
    const char *usage;
    const char *help;
    cmd_fn fn;
    int (*get)(void);
    void (*set)(int);
    int lo;
    int hi;
    /* A switch whose bare name flips it (cmd_toggles), rather than printing it. */
    int toggles;
} CmdEntry;

static CmdEntry g_cmd[CMD_MAX];
static int g_ncmd;
static int g_ready;

static char g_line[CMD_LINE];
static int g_len;
static int g_cur;
/* 0 edits the new line. k shows history entry k - 1. */
static int g_hpos;
static char g_saved[CMD_LINE];

static char g_hist[CMD_HISTORY][CMD_LINE];
static int g_hnext;
static int g_hcount;

static void builtins(void);

static void ready(void)
{
    if (!g_ready) {
        g_ready = 1;
        builtins();
    }
}

static int name_len(const char *s)
{
    int n;

    n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static CmdEntry *find(const char *name)
{
    int i;

    for (i = 0; i < g_ncmd; i++) {
        if (df_eq(g_cmd[i].name, name)) {
            return &g_cmd[i];
        }
    }
    return 0;
}

static CmdEntry *add_entry(const char *name)
{
    CmdEntry *e;

    ready();
    if (!name || !name[0] || g_ncmd >= CMD_MAX || find(name)) {
        log_print(LOG_WARN, "console: cannot add %s", name ? name : "");
        return 0;
    }
    e = &g_cmd[g_ncmd++];
    e->name = name;
    e->usage = "";
    e->help = "";
    e->fn = 0;
    e->get = 0;
    e->set = 0;
    e->lo = 0;
    e->hi = 0;
    e->toggles = 0;
    return e;
}

int cmd_add(const char *name, cmd_fn fn, const char *usage, const char *help)
{
    CmdEntry *e;

    if (!fn) {
        return 0;
    }
    e = add_entry(name);
    if (!e) {
        return 0;
    }
    e->fn = fn;
    e->usage = usage ? usage : "";
    e->help = help ? help : "";
    return 1;
}

int cmd_var(const char *name, int (*get)(void), void (*set)(int),
            int lo, int hi, const char *help)
{
    CmdEntry *e;

    if (!get || !set || hi < lo) {
        return 0;
    }
    e = add_entry(name);
    if (!e) {
        return 0;
    }
    e->get = get;
    e->set = set;
    e->lo = lo;
    e->hi = hi;
    e->usage = (lo == 0 && hi == 1) ? "on|off" : "number";
    e->help = help ? help : "";
    return 1;
}

static int is_switch(const CmdEntry *e);

int cmd_toggles(const char *name)
{
    CmdEntry *e;

    e = find(name);
    if (!e || !is_switch(e)) {
        return 0;
    }
    e->toggles = 1;
    return 1;
}

int cmd_parse_int(const char *s, int *out)
{
    int i;
    int neg;
    int v;

    if (!s || !s[0]) {
        return 0;
    }
    i = 0;
    neg = 0;
    if (s[0] == '-' || s[0] == '+') {
        neg = s[0] == '-';
        i++;
    }
    if (s[i] < '0' || s[i] > '9') {
        return 0;
    }
    v = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        /* Well short of 2^31, and far past any value a variable takes. */
        if (v > 99999999) {
            return 0;
        }
        v = v * 10 + (s[i] - '0');
        i++;
    }
    if (s[i]) {
        return 0;
    }
    *out = neg ? -v : v;
    return 1;
}

int cmd_parse_switch(const char *s, int *out)
{
    if (!s) {
        return 0;
    }
    if (df_eq(s, "1") || df_eq(s, "on") || df_eq(s, "yes")) {
        *out = 1;
        return 1;
    }
    if (df_eq(s, "0") || df_eq(s, "off") || df_eq(s, "no")) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int is_switch(const CmdEntry *e)
{
    return e->get && e->lo == 0 && e->hi == 1;
}

static void say_var(const CmdEntry *e)
{
    if (is_switch(e)) {
        log_print(LOG_PRINT, "%s %s", e->name, e->get() ? "on" : "off");
    } else {
        log_print(LOG_PRINT, "%s %d", e->name, e->get());
    }
}

static void say_usage(const CmdEntry *e)
{
    if (e->get && !is_switch(e)) {
        log_print(LOG_PRINT, "usage: %s %d..%d", e->name, e->lo, e->hi);
    } else if (e->usage[0]) {
        log_print(LOG_PRINT, "usage: %s %s", e->name, e->usage);
    } else {
        log_print(LOG_PRINT, "usage: %s", e->name);
    }
}

static void run_var(CmdEntry *e, int argc, char **argv)
{
    int v;

    if (argc < 2) {
        /* A toggling switch flips; any other variable says what it is. */
        if (e->toggles) {
            e->set(e->get() ? 0 : 1);
        }
        say_var(e);
        return;
    }
    if (is_switch(e)) {
        if (df_eq(argv[1], "toggle")) {
            v = e->get() ? 0 : 1;
        } else if (!cmd_parse_switch(argv[1], &v)) {
            say_usage(e);
            return;
        }
    } else if (!cmd_parse_int(argv[1], &v)) {
        say_usage(e);
        return;
    }
    if (v < e->lo) {
        v = e->lo;
    }
    if (v > e->hi) {
        v = e->hi;
    }
    e->set(v);
    say_var(e);
}

/*
 * Splits one command in place. Spaces separate words, double quotes
 * keep spaces, and the quotes themselves are dropped.
 */
static int split(char *s, char **argv)
{
    int argc;

    argc = 0;
    for (;;) {
        char *out;

        while (*s == ' ' || *s == '\t') {
            s++;
        }
        if (!*s || argc >= CMD_ARGS) {
            break;
        }
        argv[argc++] = s;
        out = s;
        while (*s && *s != ' ' && *s != '\t') {
            if (*s == '"') {
                s++;
                while (*s && *s != '"') {
                    *out++ = *s++;
                }
                if (*s == '"') {
                    s++;
                }
                continue;
            }
            *out++ = *s++;
        }
        if (*s) {
            s++;
        }
        *out = 0;
    }
    return argc;
}

static void lower(char *s)
{
    for (; *s; s++) {
        if (*s >= 'A' && *s <= 'Z') {
            *s = (char)(*s - 'A' + 'a');
        }
    }
}

static void run_one(char *text)
{
    char *argv[CMD_ARGS];
    int argc;
    CmdEntry *e;

    argc = split(text, argv);
    if (argc < 1) {
        return;
    }
    lower(argv[0]);
    e = find(argv[0]);
    if (!e) {
        log_print(LOG_PRINT, "unknown command %s, help lists them", argv[0]);
        return;
    }
    if (e->get) {
        run_var(e, argc, argv);
        return;
    }
    e->fn(argc, argv);
}

int cmd_run(const char *text)
{
    char one[CMD_LINE * 2];
    int n;
    int ran;
    int quote;
    int skip;

    ready();
    if (!text) {
        return 0;
    }
    ran = 0;
    n = 0;
    quote = 0;
    skip = 0;
    for (;;) {
        char ch;

        ch = *text;
        if (ch == 0 || ((ch == ';' && !quote) || ch == '\n' || ch == '\r')) {
            int i;

            one[n] = 0;
            i = 0;
            while (one[i] == ' ' || one[i] == '\t') {
                i++;
            }
            if (one[i] && one[i] != '#') {
                run_one(one + i);
                ran++;
            }
            n = 0;
            quote = 0;
            skip = 0;
            if (ch == 0) {
                break;
            }
            text++;
            continue;
        }
        text++;
        if (skip) {
            continue;
        }
        if (ch == '"') {
            quote = !quote;
        }
        if (!quote && ch == '/' && *text == '/') {
            skip = 1;
            continue;
        }
        if (n < (int)sizeof(one) - 1) {
            one[n++] = ch;
        }
    }
    return ran;
}

/* Indices of the table in name order, for help and completion lists. */
static int sorted(int *order)
{
    int i;
    int j;

    for (i = 0; i < g_ncmd; i++) {
        int k;

        k = i;
        for (j = i; j > 0; j--) {
            const char *a;
            const char *b;
            int c;

            a = g_cmd[order[j - 1]].name;
            b = g_cmd[k].name;
            c = 0;
            while (a[c] && a[c] == b[c]) {
                c++;
            }
            if ((unsigned char)a[c] <= (unsigned char)b[c]) {
                break;
            }
            order[j] = order[j - 1];
        }
        order[j] = k;
    }
    return g_ncmd;
}

/* Names in columns, as many as fit in a log line. */
static void list_names(const int *idx, int n)
{
    char line[LOG_COLS];
    int len;
    int i;
    int per;

    per = (LOG_COLS - 8) / (CMD_NAME + 1);
    if (per > 6) {
        per = 6;
    }
    len = 0;
    line[0] = 0;
    for (i = 0; i < n; i++) {
        len += log_format(line + len, LOG_COLS - len, "%-13s", g_cmd[idx[i]].name);
        if ((i + 1) % per == 0 || i + 1 == n) {
            log_text(LOG_PRINT, line);
            len = 0;
            line[0] = 0;
        }
    }
}

static int has_text(const char *hay, const char *needle)
{
    int i;
    int j;

    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j]; j++) {
            char a;
            char b;

            a = hay[i + j];
            b = needle[j];
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
        }
        if (!needle[j]) {
            return 1;
        }
    }
    return 0;
}

static void describe(const CmdEntry *e)
{
    if (e->get) {
        if (is_switch(e)) {
            log_print(LOG_PRINT, "%s on|off, now %s: %s", e->name,
                      e->get() ? "on" : "off", e->help);
        } else {
            log_print(LOG_PRINT, "%s %d..%d, now %d: %s", e->name,
                      e->lo, e->hi, e->get(), e->help);
        }
        return;
    }
    if (e->usage[0]) {
        log_print(LOG_PRINT, "%s %s: %s", e->name, e->usage, e->help);
    } else {
        log_print(LOG_PRINT, "%s: %s", e->name, e->help);
    }
}

static void c_help(int argc, char **argv)
{
    int order[CMD_MAX];
    CmdEntry *e;

    if (argc > 1) {
        lower(argv[1]);
        e = find(argv[1]);
        if (!e) {
            log_print(LOG_PRINT, "no command %s, find %s searches", argv[1], argv[1]);
            return;
        }
        describe(e);
        return;
    }
    list_names(order, sorted(order));
    log_text(LOG_PRINT, "help name explains one. find word searches. Tab completes.");
}

static void c_find(int argc, char **argv)
{
    int order[CMD_MAX];
    int n;
    int i;
    int hits;

    if (argc < 2) {
        log_text(LOG_PRINT, "usage: find word");
        return;
    }
    n = sorted(order);
    hits = 0;
    for (i = 0; i < n; i++) {
        const CmdEntry *e;

        e = &g_cmd[order[i]];
        if (has_text(e->name, argv[1]) || has_text(e->help, argv[1])) {
            describe(e);
            hits++;
        }
    }
    if (!hits) {
        log_print(LOG_PRINT, "nothing mentions %s", argv[1]);
    }
}

static void c_echo(int argc, char **argv)
{
    char line[LOG_COLS];
    int len;
    int i;

    len = 0;
    line[0] = 0;
    for (i = 1; i < argc; i++) {
        len += log_format(line + len, LOG_COLS - len, i > 1 ? " %s" : "%s", argv[i]);
    }
    log_text(LOG_PRINT, line);
}

static void c_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    log_clear();
}

static void c_history(int argc, char **argv)
{
    int i;

    (void)argc;
    (void)argv;
    if (g_hcount == 0) {
        log_text(LOG_PRINT, "no history");
        return;
    }
    for (i = g_hcount - 1; i >= 0; i--) {
        log_print(LOG_PRINT, "%3d  %s", i + 1, cmd_history(i));
    }
}

static void c_toggle(int argc, char **argv)
{
    CmdEntry *e;

    if (argc < 2) {
        log_text(LOG_PRINT, "usage: toggle switch");
        return;
    }
    lower(argv[1]);
    e = find(argv[1]);
    if (!e || !is_switch(e)) {
        log_print(LOG_PRINT, "%s is not an on|off switch", argv[1]);
        return;
    }
    e->set(e->get() ? 0 : 1);
    say_var(e);
}

static void c_log(int argc, char **argv)
{
    int lv;

    if (argc > 1) {
        lower(argv[1]);
        for (lv = LOG_ERROR; lv <= LOG_DEBUG; lv++) {
            if (df_eq(argv[1], log_level_name(lv))) {
                break;
            }
        }
        if (lv > LOG_DEBUG) {
            log_text(LOG_PRINT, "usage: log error|warn|info|debug");
            return;
        }
        log_set_level(lv);
    }
    log_print(LOG_PRINT, "log %s", log_level_name(log_level()));
}

static void builtins(void)
{
    cmd_add("help", c_help, "[name]", "list the commands, or explain one");
    cmd_add("find", c_find, "word", "commands whose name or help has the word");
    cmd_add("echo", c_echo, "text", "print the text");
    cmd_add("clear", c_clear, "", "empty the log");
    cmd_add("history", c_history, "", "the lines typed, oldest first; Up and Down recall them");
    cmd_add("toggle", c_toggle, "switch", "flip an on|off switch");
    cmd_add("log", c_log, "[error|warn|info|debug]",
            "how much the engine logs; debug adds the detail, error only failures");
}

/* --- the typed line ---------------------------------------------------- */

const char *cmd_history(int index)
{
    int i;

    if (index < 0 || index >= g_hcount) {
        return "";
    }
    i = g_hnext - 1 - index;
    while (i < 0) {
        i += CMD_HISTORY;
    }
    return g_hist[i];
}

int cmd_history_count(void)
{
    return g_hcount;
}

static void history_add(const char *s)
{
    if (g_hcount > 0 && df_eq(cmd_history(0), s)) {
        return;
    }
    df_copy(g_hist[g_hnext], s, CMD_LINE);
    g_hnext++;
    if (g_hnext >= CMD_HISTORY) {
        g_hnext = 0;
    }
    if (g_hcount < CMD_HISTORY) {
        g_hcount++;
    }
}

static void line_set(const char *s)
{
    df_copy(g_line, s, CMD_LINE);
    g_len = name_len(g_line);
    g_cur = g_len;
}

const char *cmd_line_text(void)
{
    return g_line;
}

int cmd_line_cursor(void)
{
    return g_cur;
}

void cmd_line_char(int ch)
{
    int i;

    if (ch < 32 || ch > 126 || g_len >= CMD_LINE - 1) {
        return;
    }
    for (i = g_len; i > g_cur; i--) {
        g_line[i] = g_line[i - 1];
    }
    g_line[g_cur] = (char)ch;
    g_len++;
    g_cur++;
    g_line[g_len] = 0;
}

/*
 * Completes the word that ends at the cursor: the command name, or the
 * name after help or toggle. One match is filled in with a space after
 * it. Several are extended to their common start, and when that adds
 * nothing they are listed.
 */
static void complete(void)
{
    int order[CMD_MAX];
    int hit[CMD_MAX];
    char word[CMD_LINE];
    char tail[CMD_LINE];
    int start;
    int wlen;
    int nhit;
    int common;
    int n;
    int i;

    ready();
    start = g_cur;
    while (start > 0 && g_line[start - 1] != ' ') {
        start--;
    }
    if (start > 0) {
        char first[CMD_LINE];
        int k;

        k = 0;
        while (g_line[k] == ' ') {
            k++;
        }
        n = 0;
        while (g_line[k] && g_line[k] != ' ' && n < CMD_LINE - 1) {
            first[n++] = g_line[k++];
        }
        first[n] = 0;
        lower(first);
        /* Only the second word of help and toggle is a command name. */
        while (g_line[k] == ' ') {
            k++;
        }
        if (k != start || (!df_eq(first, "help") && !df_eq(first, "toggle"))) {
            return;
        }
    }
    wlen = g_cur - start;
    for (i = 0; i < wlen; i++) {
        word[i] = g_line[start + i];
    }
    word[wlen] = 0;
    lower(word);
    n = sorted(order);
    nhit = 0;
    for (i = 0; i < n; i++) {
        const char *name;
        int c;

        name = g_cmd[order[i]].name;
        for (c = 0; c < wlen && name[c] == word[c]; c++) {
        }
        if (c == wlen) {
            hit[nhit++] = order[i];
        }
    }
    if (nhit == 0) {
        return;
    }
    common = name_len(g_cmd[hit[0]].name);
    for (i = 1; i < nhit; i++) {
        const char *a;
        const char *b;
        int c;

        a = g_cmd[hit[0]].name;
        b = g_cmd[hit[i]].name;
        for (c = 0; c < common && a[c] == b[c]; c++) {
        }
        common = c;
    }
    if (nhit > 1 && common == wlen) {
        log_print(LOG_PRINT, "> %s", g_line);
        list_names(hit, nhit);
        return;
    }
    df_copy(tail, g_line + g_cur, CMD_LINE);
    n = start;
    for (i = 0; i < common && n < CMD_LINE - 1; i++) {
        g_line[n++] = g_cmd[hit[0]].name[i];
    }
    if (nhit == 1 && n < CMD_LINE - 1 && tail[0] != ' ') {
        g_line[n++] = ' ';
    }
    g_cur = n;
    for (i = 0; tail[i] && n < CMD_LINE - 1; i++) {
        g_line[n++] = tail[i];
    }
    g_line[n] = 0;
    g_len = n;
}

void cmd_line_key(int key)
{
    int i;

    switch (key) {
    case CMD_KEY_LEFT:
        if (g_cur > 0) {
            g_cur--;
        }
        break;
    case CMD_KEY_RIGHT:
        if (g_cur < g_len) {
            g_cur++;
        }
        break;
    case CMD_KEY_HOME:
        g_cur = 0;
        break;
    case CMD_KEY_END:
        g_cur = g_len;
        break;
    case CMD_KEY_BACK:
        if (g_cur > 0) {
            for (i = g_cur - 1; i < g_len; i++) {
                g_line[i] = g_line[i + 1];
            }
            g_cur--;
            g_len--;
        }
        break;
    case CMD_KEY_DELETE:
        if (g_cur < g_len) {
            for (i = g_cur; i < g_len; i++) {
                g_line[i] = g_line[i + 1];
            }
            g_len--;
        }
        break;
    case CMD_KEY_UP:
        if (g_hpos < g_hcount) {
            if (g_hpos == 0) {
                df_copy(g_saved, g_line, CMD_LINE);
            }
            g_hpos++;
            line_set(cmd_history(g_hpos - 1));
        }
        break;
    case CMD_KEY_DOWN:
        if (g_hpos > 0) {
            g_hpos--;
            line_set(g_hpos == 0 ? g_saved : cmd_history(g_hpos - 1));
        }
        break;
    case CMD_KEY_TAB:
        complete();
        break;
    case CMD_KEY_ENTER: {
        char run[CMD_LINE];

        df_copy(run, g_line, CMD_LINE);
        line_set("");
        g_hpos = 0;
        i = 0;
        while (run[i] == ' ') {
            i++;
        }
        if (!run[i]) {
            break;
        }
        history_add(run + i);
        log_print(LOG_PRINT, "> %s", run + i);
        cmd_run(run + i);
        break;
    }
    case CMD_KEY_KILL:
        line_set("");
        g_hpos = 0;
        break;
    default:
        break;
    }
}
