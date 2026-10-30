#include "game/dagger/play/court.h"
#include "game/dagger/play/canvas.h"
#include "game/dagger/play/combat.h"
#include "game/dagger/play/rules.h"
#include "game/dagger/play/standing.h"
#include "game/dagger/play/words.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/tables/commerce.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/scene/clock.h"

/* See court.h. */

/* TEXT.RSC's words of the court. */
#define RECORD_PLEA 8050
#define RECORD_GUILTY 8055
#define RECORD_FREE 8062
#define RECORD_BANISHED 8063
#define RECORD_ARGUE 8064

/* A sentence's piece: 40 gold, or three days. Prison ticks a day each 0.3 s of real time. */
#define PIECE_GOLD 40
#define PIECE_DAYS 3
#define PRISON_TICK 0.3f
/* A chance that a low standing means banishment instead: never past this. */
#define BANISH_MOST 75
/* After it all: four hours, in minutes. */
#define RELEASE_MINUTES 240.0f

/* Where the scroll sits, and its buttons under it. */
static const Rect g_scroll_rect = { 40, 20, 240, 110 };
static const Rect g_left_button = { 96, 130, CANVAS_BUTTON_W, CANVAS_BUTTON_H };
static const Rect g_right_button = { 192, 130, CANVAS_BUTTON_W, CANVAS_BUTTON_H };
static const Rect g_only_button = { 144, 130, CANVAS_BUTTON_W, CANVAS_BUTTON_H };
#define TEXT_MARGIN 12
#define WORDS_TEXT 512
#define COUNT_X 110
#define COUNT_Y 180

enum {
    STEP_PLEA,
    STEP_ARGUE,
    STEP_RESULT,
    STEP_PRISON
};

static int g_crime;
static int g_region;

static struct {
    int open;
    Player *player;
    Composer composer;
    const df_u8 *court;
    const df_u8 *prison;
    int step;
    Sentence sentence;
    int days_left;
    float time;
    char words[WORDS_TEXT];
    char penalty[80];
    Words macros;
    int changed;
} g;

void court_commit(int crime, int region)
{
    const CrimeKind *kind;
    RegionStanding *r;
    const Faction *people;

    kind = crime_kind(crime);
    r = standing_region(region);
    if (!kind || !r) {
        return;
    }
    r->legal = (short)(r->legal - kind->loss < -STANDING_MOST ? -STANDING_MOST : r->legal - kind->loss);
    people = faction_people_of(region);
    if (people) {
        standing_change(people->id, -(kind->loss / 2), 0);
    }
    g_crime = crime;
    g_region = region;
}

int court_pending(void)
{
    return g_crime != CRIME_NONE;
}

void court_forget(void)
{
    g_crime = CRIME_NONE;
}

void court_sentence(int crime, int legal, int gold, Sentence *out)
{
    const CrimeKind *kind;
    int amount;
    int pieces;
    int i;

    out->fine = 0;
    out->days = 0;
    out->banish = 0;
    kind = crime_kind(crime);
    if (!kind) {
        return;
    }
    /* A low standing, two rolls: failing both spares banishment. */
    if (legal < 0) {
        int one;
        int two;

        one = -legal > BANISH_MOST ? BANISH_MOST : -legal;
        two = -legal / 2 > BANISH_MOST ? BANISH_MOST : -legal / 2;
        out->banish = combat_roll(100) < one || combat_roll(100) < two;
    }
    amount = rules_sentence(kind->base, kind->per_point, kind->low, kind->high, legal);
    pieces = amount / PIECE_GOLD;
    for (i = 0; i < pieces; i++) {
        if (combat_roll(2)) {
            out->fine += PIECE_GOLD;
        } else {
            out->days += PIECE_DAYS;
        }
    }
    /* What cannot be paid is served, a day for 40 gold. */
    if (out->fine > gold) {
        out->days += (out->fine - gold) / PIECE_GOLD;
        out->fine = gold;
    }
}

/* One of a record's variants as the words on the scroll. */
static void say(int record)
{
    words_use(&g.macros);
    words_say(record, g.words, WORDS_TEXT);
    g.changed = 1;
}

/* A sentence done or answered: its share of the reputation back. */
static void raise_reputation(void)
{
    const CrimeKind *kind;
    RegionStanding *r;
    const Faction *people;
    int back;

    kind = crime_kind(g_crime);
    r = standing_region(g_region);
    if (!kind || !r) {
        return;
    }
    back = kind->loss / 2 - 1;
    if (back > 0) {
        r->legal = (short)(r->legal + back > STANDING_MOST ? STANDING_MOST : r->legal + back);
        people = faction_people_of(g_region);
        if (people) {
            standing_change(people->id, back / 2, 0);
        }
    }
}

/* The fine paid, then either prison or the street. */
static void pay_and_serve(void)
{
    g.player->pack.gold -= (df_u32)(g.sentence.fine > (int)g.player->pack.gold ? (int)g.player->pack.gold : g.sentence.fine);
    raise_reputation();
    if (g.sentence.days > 0) {
        g.macros.days = g.sentence.days;
        say(RECORD_GUILTY);
        g.step = STEP_RESULT;
        return;
    }
    g.words[0] = 0;
    log_format(g.words, WORDS_TEXT, "You pay the fine of %d gold pieces.", g.sentence.fine);
    g.step = STEP_RESULT;
}

/* Banished: a mark on the region for good. */
static void banish(void)
{
    RegionStanding *r;

    r = standing_region(g_region);
    if (r) {
        r->banished = 1;
    }
    say(RECORD_BANISHED);
    g.step = STEP_RESULT;
    g.sentence.days = 0;
}

static void go_free(void)
{
    raise_reputation();
    g.sentence.days = 0;
    say(RECORD_FREE);
    g.step = STEP_RESULT;
}

/* Whether the argument or the lie wins, and what follows. */
static void argue(int skill)
{
    int chance;
    RegionStanding *r;

    r = standing_region(g_region);
    chance = (r ? r->legal : 0) + (g.player->sheet.skill[skill] + g.player->sheet.attribute[ATTR_PERSONALITY]) / 2;
    chance = chance < 5 ? 5 : (chance > 95 ? 95 : chance);
    character_tally(&g.player->sheet, skill, 1);
    if (combat_roll(100) < chance) {
        go_free();
        return;
    }
    if (g.sentence.banish) {
        banish();
        return;
    }
    /* Lost: the court's mood moves the fine. */
    {
        int mood;

        mood = (r ? r->legal : 0) + combat_roll(100) + 1;
        if (mood < 25) {
            g.sentence.fine *= 2;
            if (g.sentence.fine > (int)g.player->pack.gold) {
                g.sentence.days += (g.sentence.fine - (int)g.player->pack.gold) / PIECE_GOLD;
                g.sentence.fine = (int)g.player->pack.gold;
            }
        } else if (mood > 75) {
            g.sentence.fine /= 2;
        }
    }
    pay_and_serve();
}

int court_open(Player *p)
{
    RegionStanding *r;
    const CrimeKind *kind;

    court_close();
    mem_zero(&g, sizeof(g));
    if (!composer_open(&g.composer)) {
        return 0;
    }
    g.open = 1;
    g.player = p;
    g.court = composer_read(&g.composer, "CORT01I0.IMG");
    g.prison = composer_read(&g.composer, "PRIS00I0.IMG");
    if (!g.court || !g.prison) {
        log_text(LOG_WARN, "court: its pictures did not read");
        court_close();
        return 0;
    }
    r = standing_region(g_region);
    kind = crime_kind(g_crime);
    court_sentence(g_crime, r ? r->legal : 0, (int)p->pack.gold, &g.sentence);
    if (g.sentence.banish) {
        df_copy(g.penalty, "banishment", (int)sizeof(g.penalty));
    } else if (g.sentence.days > 0 && g.sentence.fine > 0) {
        log_format(g.penalty, (int)sizeof(g.penalty), "a fine of %d gold pieces and %d days in prison", g.sentence.fine, g.sentence.days);
    } else if (g.sentence.days > 0) {
        log_format(g.penalty, (int)sizeof(g.penalty), "%d days in prison", g.sentence.days);
    } else {
        log_format(g.penalty, (int)sizeof(g.penalty), "a fine of %d gold pieces", g.sentence.fine);
    }
    g.macros.pc = &p->sheet;
    g.macros.crime = kind ? kind->name : "";
    g.macros.penalty = g.penalty;
    g.macros.tone = RULES_TONE_NORMAL;
    /* %cn in the court's words: it speaks for the whole region. */
    g.macros.city = faction_province_of(g_region) ? faction_province_of(g_region)->name : "";
    g.macros.region = g.macros.city;
    g.step = STEP_PLEA;
    say(RECORD_PLEA);
    return 1;
}

void court_close(void)
{
    if (g.open) {
        g.open = 0;
        composer_close(&g.composer);
    }
}

void court_compose(void)
{
    Rect inside;

    canvas_use(&g.composer);
    if (g.step == STEP_PRISON) {
        char line[40];

        canvas_copy_all(g.prison);
        log_format(line, (int)sizeof(line), "Days until freedom: %d", g.days_left);
        canvas_label(COUNT_X, COUNT_Y, line);
        g.changed = 0;
        return;
    }
    canvas_copy_all(g.court);
    canvas_scroll(&g_scroll_rect);
    inside.x = (df_u16)(g_scroll_rect.x + TEXT_MARGIN);
    inside.y = (df_u16)(g_scroll_rect.y + TEXT_MARGIN);
    inside.w = (df_u16)(g_scroll_rect.w - 2 * TEXT_MARGIN);
    inside.h = (df_u16)(g_scroll_rect.h - 2 * TEXT_MARGIN);
    canvas_text(&inside, g.words);
    if (g.step == STEP_PLEA) {
        canvas_button(CANVAS_BUTTON_GUILTY, g_left_button.x, g_left_button.y);
        canvas_button(CANVAS_BUTTON_NOT_GUILTY, g_right_button.x, g_right_button.y);
    } else if (g.step == STEP_ARGUE) {
        canvas_button(CANVAS_BUTTON_DEBATE, g_left_button.x, g_left_button.y);
        canvas_button(CANVAS_BUTTON_LIE, g_right_button.x, g_right_button.y);
    } else {
        canvas_button(CANVAS_BUTTON_OK, g_only_button.x, g_only_button.y);
    }
    g.changed = 0;
}

const unsigned char *court_pixels(void)
{
    return g.composer.screen;
}

int court_changed(void)
{
    int changed;

    changed = g.changed;
    g.changed = 0;
    return changed;
}

/* All done: time passed, the player whole, and the crime off the books. */
static int finish(void)
{
    clock_set_minute(clock_minute() + RELEASE_MINUTES);
    character_rest_full(&g.player->sheet);
    g_crime = CRIME_NONE;
    return COURT_LEAVE;
}

int court_click(int x, int y)
{
    g.changed = 1;
    switch (g.step) {
    case STEP_PLEA:
        if (rect_has(&g_left_button, x, y)) {
            /* Guilty: the mercy of the court, half of either. */
            if (g.sentence.banish) {
                banish();
            } else {
                g.sentence.fine /= 2;
                g.sentence.days /= 2;
                pay_and_serve();
            }
        } else if (rect_has(&g_right_button, x, y)) {
            say(RECORD_ARGUE);
            g.step = STEP_ARGUE;
        }
        break;
    case STEP_ARGUE:
        if (rect_has(&g_left_button, x, y)) {
            argue(SKILL_ETIQUETTE);
        } else if (rect_has(&g_right_button, x, y)) {
            argue(SKILL_STREETWISE);
        }
        break;
    case STEP_RESULT:
        if (rect_has(&g_only_button, x, y)) {
            if (g.sentence.days > 0) {
                g.step = STEP_PRISON;
                g.days_left = g.sentence.days;
                g.time = 0.0f;
                break;
            }
            return finish();
        }
        break;
    default:
        break;
    }
    return COURT_STAY;
}

int court_step(float dt)
{
    if (g.step != STEP_PRISON) {
        return COURT_STAY;
    }
    g.time += dt;
    while (g.time >= PRISON_TICK && g.days_left > 0) {
        g.time -= PRISON_TICK;
        g.days_left--;
        g.changed = 1;
    }
    if (g.days_left > 0) {
        return COURT_STAY;
    }
    clock_set_day(clock_day() + g.sentence.days);
    return finish();
}

const Sentence *court_given(void)
{
    return &g.sentence;
}

const char *court_words(void)
{
    return g.words;
}
