#include "engine/core/log.h"

/*
 * A ring of fixed lines, so logging never allocates. g_next is the slot
 * the next line takes. g_ring_level is each line's level, for
 * log_get_level.
 */

static char g_ring[LOG_LINES][LOG_COLS];
static unsigned char g_ring_level[LOG_LINES];
static int g_next;
static int g_count;
static df_u32 g_serial;
static int g_level = LOG_INFO;
static void (*g_sink)(const char *line);

void log_set_level(int level)
{
    if (level < LOG_ERROR) {
        level = LOG_ERROR;
    }
    if (level > LOG_DEBUG) {
        level = LOG_DEBUG;
    }
    g_level = level;
}

int log_level(void)
{
    return g_level;
}

const char *log_level_name(int level)
{
    switch (level) {
    case LOG_ERROR: return "error";
    case LOG_WARN: return "warn";
    case LOG_INFO: return "info";
    case LOG_DEBUG: return "debug";
    default: return "print";
    }
}

int log_wants(int level)
{
    return level <= g_level;
}

void log_set_sink(void (*fn)(const char *line))
{
    g_sink = fn;
}

static void put(char *dst, int *n, int cap, char ch)
{
    if (*n < cap - 1) {
        dst[*n] = ch;
        *n += 1;
    }
}

/* Digits of u in base, most significant first, into tmp. Returns the count. */
static int digits(char *tmp, unsigned long u, unsigned int base)
{
    static const char hex[] = "0123456789abcdef";
    int k;

    k = 0;
    do {
        tmp[k++] = hex[u % base];
        u /= base;
    } while (u && k < 32);
    return k;
}

/* sign, digits[k-1..0] padded to width, as the flags ask. */
static void put_number(char *dst, int *n, int cap, int neg, const char *tmp, int k,
                       int width, int left, int zero)
{
    int len;
    int pad;

    len = k + (neg ? 1 : 0);
    pad = width > len ? width - len : 0;
    if (!left && !zero) {
        while (pad-- > 0) {
            put(dst, n, cap, ' ');
        }
    }
    if (neg) {
        put(dst, n, cap, '-');
    }
    if (!left && zero) {
        while (pad-- > 0) {
            put(dst, n, cap, '0');
        }
    }
    while (k > 0) {
        put(dst, n, cap, tmp[--k]);
    }
    if (left) {
        while (pad-- > 0) {
            put(dst, n, cap, ' ');
        }
    }
}

/*
 * Fixed point with prec decimals. The scaled value is rounded through
 * an unsigned long, so a C cast runs here: fine, a log line is not in a
 * per-frame loop.
 */
static void put_fixed(char *dst, int *n, int cap, double v, int prec,
                      int width, int left, int zero)
{
    static const unsigned long scale_of[5] = { 1ul, 10ul, 100ul, 1000ul, 10000ul };
    char tmp[40];
    unsigned long scale;
    unsigned long whole;
    unsigned long frac;
    int neg;
    int k;
    int i;

    if (prec < 0) {
        prec = 2;
    }
    if (prec > 4) {
        prec = 4;
    }
    neg = v < 0.0;
    if (neg) {
        v = -v;
    }
    scale = scale_of[prec];
    if (!(v * (double)scale < 4000000000.0)) {
        const char *big = "big";

        while (*big) {
            put(dst, n, cap, *big++);
        }
        return;
    }
    frac = (unsigned long)(v * (double)scale + 0.5);
    /* -0.001 at two decimals prints 0.00, not -0.00. */
    neg = neg && frac != 0ul;
    whole = frac / scale;
    frac %= scale;
    /* tmp holds the digits least significant first: decimals, point, whole. */
    k = 0;
    for (i = 0; i < prec; i++) {
        tmp[k++] = (char)('0' + (int)(frac % 10ul));
        frac /= 10ul;
    }
    if (prec > 0) {
        tmp[k++] = '.';
    }
    k += digits(tmp + k, whole, 10u);
    put_number(dst, n, cap, neg, tmp, k, width, left, zero);
}

int log_vformat(char *dst, int cap, const char *fmt, va_list ap)
{
    int n;

    if (!dst || cap < 1) {
        return 0;
    }
    n = 0;
    if (!fmt) {
        fmt = "";
    }
    while (*fmt) {
        int left;
        int zero;
        int width;
        int prec;
        char tmp[40];
        int k;

        if (*fmt != '%') {
            put(dst, &n, cap, *fmt++);
            continue;
        }
        fmt++;
        left = 0;
        zero = 0;
        for (;;) {
            if (*fmt == '-') {
                left = 1;
            } else if (*fmt == '0') {
                zero = 1;
            } else {
                break;
            }
            fmt++;
        }
        width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                prec = prec * 10 + (*fmt - '0');
                fmt++;
            }
        }
        if (*fmt == 'l') {
            fmt++;
        }
        switch (*fmt) {
        case 'd':
        case 'i': {
            long v;
            unsigned long u;

            v = va_arg(ap, int);
            u = v < 0 ? (unsigned long)(-(v + 1)) + 1ul : (unsigned long)v;
            k = digits(tmp, u, 10u);
            put_number(dst, &n, cap, v < 0, tmp, k, width, left, zero);
            break;
        }
        case 'u':
            k = digits(tmp, (unsigned long)va_arg(ap, unsigned int), 10u);
            put_number(dst, &n, cap, 0, tmp, k, width, left, zero);
            break;
        case 'x':
            k = digits(tmp, (unsigned long)va_arg(ap, unsigned int), 16u);
            put_number(dst, &n, cap, 0, tmp, k, width, left, zero);
            break;
        case 'c':
            tmp[0] = (char)va_arg(ap, int);
            put_number(dst, &n, cap, 0, tmp, 1, width, left, 0);
            break;
        case 's': {
            const char *s;
            int len;
            int pad;

            s = va_arg(ap, const char *);
            if (!s) {
                s = "(null)";
            }
            len = 0;
            while (s[len] && (prec < 0 || len < prec)) {
                len++;
            }
            pad = width > len ? width - len : 0;
            if (!left) {
                while (pad-- > 0) {
                    put(dst, &n, cap, ' ');
                }
            }
            for (k = 0; k < len; k++) {
                put(dst, &n, cap, s[k]);
            }
            if (left) {
                while (pad-- > 0) {
                    put(dst, &n, cap, ' ');
                }
            }
            break;
        }
        case 'f':
            put_fixed(dst, &n, cap, va_arg(ap, double), prec, width, left, zero);
            break;
        case '%':
            put(dst, &n, cap, '%');
            break;
        case 0:
            fmt--;
            break;
        default:
            put(dst, &n, cap, '%');
            put(dst, &n, cap, *fmt);
            break;
        }
        fmt++;
    }
    dst[n] = 0;
    return n;
}

int log_format(char *dst, int cap, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = log_vformat(dst, cap, fmt, ap);
    va_end(ap);
    return n;
}

static const char *prefix_of(int level)
{
    if (level == LOG_ERROR) {
        return "error: ";
    }
    if (level == LOG_WARN) {
        return "warning: ";
    }
    return "";
}

/* Takes the next ring slot and writes the prefix. Returns its length. */
static char *slot_begin(int level, int *n)
{
    char *line;
    const char *pre;

    line = g_ring[g_next];
    g_ring_level[g_next] = (unsigned char)level;
    pre = prefix_of(level);
    *n = 0;
    while (pre[*n]) {
        line[*n] = pre[*n];
        *n += 1;
    }
    line[*n] = 0;
    return line;
}

static void slot_end(char *line)
{
    int i;

    /* A tab or a stray newline would draw as a box: keep the line flat. */
    for (i = 0; line[i]; i++) {
        if (line[i] == '\t' || line[i] == '\n' || line[i] == '\r') {
            line[i] = ' ';
        }
    }
    g_next++;
    if (g_next >= LOG_LINES) {
        g_next = 0;
    }
    if (g_count < LOG_LINES) {
        g_count++;
    }
    g_serial++;
    if (g_sink) {
        g_sink(line);
    }
}

void log_text(int level, const char *s)
{
    char *line;
    int n;

    if (!log_wants(level)) {
        return;
    }
    if (!s) {
        s = "";
    }
    line = slot_begin(level, &n);
    df_copy(line + n, s, LOG_COLS - n);
    slot_end(line);
}

void log_print(int level, const char *fmt, ...)
{
    va_list ap;
    char *line;
    int n;

    if (!log_wants(level)) {
        return;
    }
    line = slot_begin(level, &n);
    va_start(ap, fmt);
    log_vformat(line + n, LOG_COLS - n, fmt, ap);
    va_end(ap);
    slot_end(line);
}

int log_count(void)
{
    return g_count;
}

static int slot_of(int index)
{
    int i;

    i = g_next - 1 - index;
    while (i < 0) {
        i += LOG_LINES;
    }
    return i;
}

const char *log_get(int index)
{
    if (index < 0 || index >= g_count) {
        return "";
    }
    return g_ring[slot_of(index)];
}

int log_get_level(int index)
{
    if (index < 0 || index >= g_count) {
        return LOG_PRINT;
    }
    return g_ring_level[slot_of(index)];
}

df_u32 log_serial(void)
{
    return g_serial;
}

void log_clear(void)
{
    g_count = 0;
    g_next = 0;
}
