#include "internal.h"
#include "engine/core/log.h"

static void (*g_answer)(const char *);
/* Messages waiting behind the one shown (a service that has several things to say). */
static char g_queue[4][sizeof(possessions_view.notice)];
static int g_queued;

/* A dialog over the world: the message page already up and between questions is used again (an answer's follow-up), else one is opened. */
static int dialog_ready(void)
{
    if ((possessions_view.page == VIEW_MESSAGE || possessions_view.page == VIEW_POTION || possessions_view.page == VIEW_SPELLSHOP || possessions_view.page == VIEW_MAKER || possessions_view.page == VIEW_ITEMMAKER) && !possessions_view.prompt) { return 1; }
    return !possessions_view.page && possessions_open(VIEW_MESSAGE,0);
}

static void (*g_ask_cb)(int yes);
static void (*g_number_cb)(int amount);
static void (*g_list_cb)(int row);

int possessions_list(const char names[][LIST_NAME], int count, void (*pick)(int row))
{
    int i;

    if (!names || count < 1 || count > LIST_MOST || !pick || !dialog_ready()) { return 0; }
    for (i = 0; i < count; i++) { df_copy(possessions_view.list[i],names[i],LIST_NAME); }
    possessions_view.list_count = count; possessions_view.list_top = 0; possessions_view.prompt = PROMPT_LIST; g_list_cb = pick; possessions_view.revision++;
    return 1;
}

int possessions_ask(const char *words, void (*answer)(int yes))
{
    if (!words || !*words || !answer || !dialog_ready()) { return 0; }
    df_copy(possessions_view.offer,words,sizeof(possessions_view.offer));
    possessions_view.offer_page = 0;
    df_copy(possessions_view.yes,"Yes",sizeof(possessions_view.yes)); df_copy(possessions_view.no,"No",sizeof(possessions_view.no));
    possessions_view.prompt = PROMPT_ASK; g_ask_cb = answer; possessions_view.revision++;
    return 1;
}

int possessions_number(const char *words, void (*answer)(int amount))
{
    return possessions_number_from(words, 0, answer);
}

int possessions_number_from(const char *words, int initial, void (*answer)(int amount))
{
    if (!words || !*words || !answer || !dialog_ready()) { return 0; }
    df_copy(possessions_view.offer,words,sizeof(possessions_view.offer)); possessions_view.input[0] = 0;
    if (initial > 0) { log_format(possessions_view.input,sizeof(possessions_view.input),"%d",initial); }
    possessions_view.prompt = PROMPT_NUMBER; g_number_cb = answer; possessions_view.revision++;
    return 1;
}

int possessions_message(const char *text, void (*answer)(const char *))
{
    /* A message said while another is up waits its turn (up to four); said over an answered question it is shown at once. */
    /* Over a window that is up (the potion maker, the spell shop), the words are shown over it. */
    if (text && *text && (possessions_view.page == VIEW_POTION || possessions_view.page == VIEW_SPELLSHOP) && !possessions_view.prompt && !possessions_view.notice[0]) {
        df_copy(possessions_view.notice,text,sizeof(possessions_view.notice)); possessions_view.notice_page = 0; possessions_view.revision++; return 1;
    }
    if (text && *text && possessions_view.page == VIEW_MESSAGE && !possessions_view.prompt) {
        if (!possessions_view.notice[0]) { df_copy(possessions_view.notice,text,sizeof(possessions_view.notice)); possessions_view.notice_page = 0; possessions_view.revision++; return 1; }
        if (g_queued < 4) { df_copy(g_queue[g_queued++],text,sizeof(g_queue[0])); return 1; }
        return 0;
    }
    if (!text || !*text || possessions_view.page || !possessions_open(VIEW_MESSAGE,0)) { return 0; }
    df_copy(possessions_view.notice,text,sizeof(possessions_view.notice)); g_answer = answer;
    if (answer) { possessions_view.prompt = PROMPT_ANSWER; }
    possessions_view.revision++;
    return 1;
}

static void submit_answer(int accepted)
{
    char text[32];
    void (*callback)(const char *);
    df_copy(text,possessions_view.input,sizeof(text)); callback = g_answer; g_answer = 0;
    possessions_close();
    if (callback) { callback(accepted ? text : 0); }
}

/* The parchment's text width in pixels, measured by the app's font; none until it is set (then 40 letters a line). */
static int (*g_measure)(const char *text);
void possessions_set_measure(int (*measure)(const char *text)) { g_measure = measure; }
#define NOTICE_PIXELS 250

/* How much of the line at text, length letters long, fits the parchment: all of it, else up to its last space that fits (a word longer than the box is cut where it stops fitting). */
static int fit(const char *text, int length)
{
    char probe[96];
    int n, k, best, limit;

    if (!g_measure) {
        if (length <= 40) { return length; }
        for (n = 40; n > 0; n--) { if (text[n] == ' ') { return n; } }
        return 40;
    }
    best = 0; limit = length > 95 ? 95 : length;
    for (n = 1; n <= limit; n++) {
        for (k = 0; k < n; k++) { probe[k] = text[k]; }
        probe[n] = 0;
        if (g_measure(probe) > NOTICE_PIXELS) { break; }
        best = n;
    }
    if (best == length) { return length; }
    if (text[best] == ' ') { return best; }
    for (n = best; n > 0; n--) { if (text[n] == ' ') { return n; } }
    return best > 0 ? best : 1;
}

int notice_lines(const char *text, int first, char out[][96], int capacity)
{
    int count, length, n, split;
    count = 0;
    while (*text) {
        while (*text == '\n' || *text == ' ') { text++; }
        if (!*text) { break; }
        for (length = 0; text[length] && text[length] != '\n' && length < 95; length++) { }
        split = fit(text,length);
        if (out && count >= first && count-first < capacity) {
            for (n = 0; n < split; n++) { out[count-first][n] = text[n]; }
            while (n && out[count-first][n-1] == ' ') { n--; }
            out[count-first][n] = 0;
        }
        text += split; count++;
    }
    return count;
}

static void prompt_accept(void)
{
    int value, i;
    if (possessions_view.prompt == PROMPT_ANSWER) { submit_answer(1); return; }
    if (possessions_view.prompt == PROMPT_AMOUNT) {
        int amount, k;
        amount = 0;
        for (k = 0; possessions_view.input[k] && amount < 100000000; k++) { amount = amount * 10 + possessions_view.input[k] - '0'; }
        possessions_view.prompt = 0; possessions_view.input[0] = 0;
        possessions_bank_run(possessions_view.bank_op, amount, 0);
        return;
    }
    if (possessions_view.prompt == PROMPT_NUMBER) {
        int amount, k;
        void (*cb)(int);

        amount = 0;
        for (k = 0; possessions_view.input[k] && amount < 100000000; k++) { amount = amount * 10 + possessions_view.input[k] - '0'; }
        cb = g_number_cb; g_number_cb = 0;
        possessions_view.prompt = 0; possessions_view.offer[0] = 0; possessions_view.input[0] = 0;
        if (cb) { cb(amount); }
        return;
    }
    if (possessions_view.prompt == PROMPT_ASK) {
        if (possessions_view.page == VIEW_COURT) { possessions_court_answer(1); return; }
        if (possessions_view.page == VIEW_MESSAGE && g_ask_cb) {
            void (*cb)(int);

            cb = g_ask_cb; g_ask_cb = 0;
            possessions_view.prompt = 0; possessions_view.offer[0] = 0;
            cb(1);
            return;
        }
        possessions_view.prompt = 0; possessions_view.offer[0] = 0;
        possessions_bank_run(possessions_view.bank_op, possessions_view.bank_amount, 1);
        return;
    }
    if (possessions_view.prompt == PROMPT_OFFER) { possessions_trade_answer(1); return; }
    value = 0;
    for (i = 0; possessions_view.input[i] && value < 10000000; i++) { value = value * 10 + possessions_view.input[i] - '0'; }
    if (possessions_view.prompt == PROMPT_NAME) {
        if (!possessions_view.input[0]) { return; }
        df_copy(character_state()->name, possessions_view.input, NAME_LEN);
    } else if (possessions_view.prompt == PROMPT_SPELL_NAME && possessions_view.page == VIEW_ITEMMAKER) {
        possessions_itemmaker_name();
    } else if (possessions_view.prompt == PROMPT_SPELL_NAME && possessions_view.page == VIEW_MAKER) {
        possessions_spellmaker_name();
    } else if (possessions_view.prompt == PROMPT_SPELL_NAME) {
        possessions_spellbook_name();
    } else if (possessions_view.prompt == PROMPT_GOLD) {
        if (!possessions_drop_gold(value)) { return; }
    } else if (possessions_view.prompt == PROMPT_PAGE) { possessions_book_page(value-1); }
    possessions_view.prompt = 0; possessions_view.input[0] = 0;
}

/* The no, Escape, or the Reject button: whatever is asked is answered no. */
static void prompt_reject(void)
{
    if (possessions_view.prompt == PROMPT_ANSWER) { submit_answer(0); return; }
    if (possessions_view.prompt == PROMPT_OFFER) { possessions_trade_answer(0); }
    if (possessions_view.prompt == PROMPT_ASK && possessions_view.page == VIEW_COURT) { possessions_court_answer(0); return; }
    if (possessions_view.prompt == PROMPT_ASK && possessions_view.page == VIEW_MESSAGE && g_ask_cb) {
        void (*cb)(int);

        cb = g_ask_cb; g_ask_cb = 0;
        possessions_view.offer[0] = 0; possessions_view.prompt = 0;
        cb(0);
        return;
    }
    if (possessions_view.prompt == PROMPT_LIST) {
        void (*cb)(int);

        cb = g_list_cb; g_list_cb = 0;
        possessions_view.prompt = 0; possessions_view.list_count = 0;
        if (cb) { cb(-1); }
        return;
    }
    if (possessions_view.prompt == PROMPT_NUMBER) { g_number_cb = 0; }
    possessions_view.offer[0] = 0; possessions_view.prompt = 0;
}

void possessions_prompt_frame(const PlayIn *in, int click)
{
    int n;

    if (possessions_view.prompt == PROMPT_ASK && possessions_view.page == VIEW_MESSAGE && g_ask_cb) {
        int last;
        int left = click && possessions_hit(in->cx,in->cy,84,116,151,134);
        int right = click && possessions_hit(in->cx,in->cy,169,116,236,134);

        if (!left && !right && in->ch != 13 && in->ch != 27 && in->key != KEY_PRIOR && in->key != KEY_NEXT) { return; }
        last = (notice_lines(possessions_view.offer,0,0,0)-1)/ASK_LINES;
        if (in->ch == 27) { prompt_reject(); }
        else if (in->key == KEY_PRIOR || (right && possessions_view.offer_page < last)) {
            if (possessions_view.offer_page > 0) { possessions_view.offer_page--; }
        } else if (in->key == KEY_NEXT) {
            if (possessions_view.offer_page < last) { possessions_view.offer_page++; }
        } else if (in->ch == 13 || left) {
            if (possessions_view.offer_page < last) { possessions_view.offer_page++; }
            else { prompt_accept(); }
        } else if (right) { prompt_reject(); }
        return;
    }
    if (possessions_view.prompt == PROMPT_LIST) {
        /* The rows are 8 pixels from y 34; a click on one picks it, Escape none. */
        int shown;

        shown = possessions_view.list_count < LIST_SHOWN ? possessions_view.list_count : LIST_SHOWN;
        if (in->ch == 27) { prompt_reject(); }
        else if (in->key == KEY_DOWN || in->key == KEY_NEXT) { if (possessions_view.list_top+shown < possessions_view.list_count) { possessions_view.list_top += in->key == KEY_NEXT ? shown : 1; if (possessions_view.list_top+shown > possessions_view.list_count) { possessions_view.list_top = possessions_view.list_count-shown; } possessions_view.revision++; } }
        else if (in->key == KEY_UP || in->key == KEY_PRIOR) { if (possessions_view.list_top) { possessions_view.list_top -= in->key == KEY_PRIOR ? shown : 1; if (possessions_view.list_top < 0) { possessions_view.list_top = 0; } possessions_view.revision++; } }
        else if (click && in->cx > 100 && in->cx < 220 && in->cy >= 34 && (in->cy-34)/8 < shown) {
            void (*cb)(int);
            int row;

            row = possessions_view.list_top+(in->cy-34)/8;
            cb = g_list_cb; g_list_cb = 0;
            possessions_view.prompt = 0; possessions_view.list_count = 0;
            if (cb) { cb(row); }
        }
    }
    else if (in->ch == 27) { prompt_reject(); }
    else if (in->ch == 13 || (click && possessions_hit(in->cx,in->cy,84,116,151,134))) { prompt_accept(); }
    else if (click && possessions_hit(in->cx,in->cy,169,116,236,134)) { prompt_reject(); }
    else {
        for (n = 0; possessions_view.input[n]; n++) { }
        if (in->ch == 8 && n) { possessions_view.input[n-1] = 0; }
        else if (in->ch >= 32 && in->ch <= 126 && n < (int)sizeof(possessions_view.input)-1 &&
                 (possessions_view.prompt == PROMPT_NAME || (possessions_view.prompt == PROMPT_ANSWER && n < 16) ||
                  (possessions_view.prompt == PROMPT_SPELL_NAME && n < 24) ||
                  (possessions_view.prompt != PROMPT_SPELL_NAME && possessions_view.prompt != PROMPT_ANSWER && possessions_view.prompt != PROMPT_OFFER && possessions_view.prompt != PROMPT_ASK && in->ch >= '0' && in->ch <= '9' && n < 10))) {
            possessions_view.input[n] = (char)in->ch; possessions_view.input[n+1] = 0;
        }
    }
}

void possessions_notice_frame(const PlayIn *in, int click)
{
    int pages;
    pages = (notice_lines(possessions_view.notice,0,0,0)+NOTICE_LINES-1)/NOTICE_LINES;
    if (in->key == KEY_PRIOR) { if (possessions_view.notice_page) { possessions_view.notice_page--; } }
    else if (in->key == KEY_NEXT) { if (possessions_view.notice_page+1 < pages) { possessions_view.notice_page++; } }
    else if (in->ch == 27 || (in->mouse_pressed & 2)) { possessions_view.notice[0] = 0; }
    else if (in->ch == 13 || in->ch == ' ' || click) {
        if (possessions_view.notice_page+1 < pages) { possessions_view.notice_page++; } else { possessions_view.notice[0] = 0; }
    }
    if (!possessions_view.notice[0]) { possessions_inventory_notice_closed(); }
    if (!possessions_view.notice[0] && possessions_view.page == VIEW_COURT) { possessions_court_notice_closed(); }
}

void possessions_dialog_clear(void)
{
    g_answer = 0;
    g_queued = 0;
}

void possessions_message_frame(void)
{
    if (possessions_view.page == VIEW_MESSAGE && !possessions_view.notice[0] && !possessions_view.prompt && g_queued) {
        int k;

        df_copy(possessions_view.notice,g_queue[0],sizeof(possessions_view.notice)); possessions_view.notice_page = 0;
        for (k = 1; k < g_queued; k++) { df_copy(g_queue[k-1],g_queue[k],sizeof(g_queue[0])); }
        g_queued--; possessions_view.revision++;
    }
    if (possessions_view.page == VIEW_MESSAGE && !possessions_view.notice[0] && !possessions_view.prompt) { possessions_close(); }
}
