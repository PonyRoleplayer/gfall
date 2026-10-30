#include "game/dagger/formats/biog.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"

/* See biog.h. */

/* Moves past spaces and tabs. */
static const char *skip_blanks(const char *s, const char *end)
{
    while (s < end && (*s == ' ' || *s == '\t')) {
        s++;
    }
    return s;
}

/* A number at *s, signed or not, spaces allowed after the sign; 0 and s unmoved when none. */
static int read_number(const char **s, const char *end)
{
    const char *p;
    int sign;
    int value;

    p = skip_blanks(*s, end);
    sign = 1;
    if (p < end && (*p == '+' || *p == '-')) {
        sign = *p == '-' ? -1 : 1;
        p = skip_blanks(p + 1, end);
    }
    value = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
    }
    *s = p;
    return sign * value;
}

/* A line's text, from s to its end, into out, without its trailing blanks. */
static void copy_text(char *out, const char *s, const char *end)
{
    int n;

    n = 0;
    while (s < end && n < BIOG_LINE - 1) {
        out[n++] = *s == '\t' ? ' ' : *s;
        s++;
    }
    while (n > 0 && out[n - 1] == ' ') {
        n--;
    }
    out[n] = 0;
}

/* One line under an answer: what it does, if it is something the game can do. */
static void read_effect(BiogAnswer *answer, const char *s, const char *end)
{
    BiogEffect *e;

    if (answer->effects >= BIOG_EFFECTS) {
        return;
    }
    e = &answer->effect[answer->effects];
    if (*s >= '0' && *s <= '9') {
        e->kind = BIOG_SKILL;
        e->a = (short)read_number(&s, end);
        e->b = (short)read_number(&s, end);
        answer->effects++;
    } else if (end - s > 2 && s[0] == 'G' && s[1] == 'P') {
        s += 2;
        e->kind = BIOG_GOLD;
        e->a = (short)read_number(&s, end);
        answer->effects++;
    } else if (end - s > 2 && s[0] == 'I' && s[1] == 'T') {
        s += 2;
        e->kind = BIOG_ITEM;
        e->a = (short)read_number(&s, end);
        e->b = (short)read_number(&s, end);
        e->c = (short)read_number(&s, end);
        answer->effects++;
    }
}

/* 1 when the line starts a question: a number and a full stop. */
static int starts_question(const char *s, const char *end)
{
    const char *p;

    p = s;
    while (p < end && *p >= '0' && *p <= '9') {
        p++;
    }
    return p > s && p < end && *p == '.';
}

/* 1 when the line starts an answer: one letter and a full stop. */
static int starts_answer(const char *s, const char *end)
{
    return end - s >= 2 && s[0] >= 'a' && s[0] <= 'z' && s[1] == '.';
}

int biog_load(int klass, Biog *out)
{
    char name[16];
    const char *text;
    const char *at;
    const char *stop;
    BiogQuestion *question;
    BiogAnswer *answer;
    df_u32 size;

    mem_zero(out, sizeof(*out));
    log_format(name, (int)sizeof(name), "BIOG%02dT0.TXT", klass);
    text = (const char *)vfs_read(name, &size);
    if (!text) {
        return 0;
    }
    question = 0;
    answer = 0;
    at = text;
    stop = text + size;
    while (at < stop) {
        const char *line;
        const char *end;

        end = at;
        while (end < stop && *end != '\n' && *end != '\r') {
            end++;
        }
        line = skip_blanks(at, end);
        if (line == end) {
            answer = 0;
        } else if (starts_question(line, end) && out->questions < BIOG_QUESTIONS) {
            question = &out->question[out->questions++];
            answer = 0;
            while (*line != '.') {
                line++;
            }
            copy_text(question->line[0], skip_blanks(line + 1, end), end);
        } else if (question && starts_answer(line, end) && question->answers < BIOG_ANSWERS) {
            answer = &question->answer[question->answers++];
            copy_text(answer->text, skip_blanks(line + 2, end), end);
        } else if (answer) {
            read_effect(answer, line, end);
        } else if (question && !question->answers && !question->line[1][0]) {
            copy_text(question->line[1], line, end);
        }
        at = end;
        while (at < stop && (*at == '\n' || *at == '\r')) {
            at++;
        }
    }
    return out->questions > 0;
}
