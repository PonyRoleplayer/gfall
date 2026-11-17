#include "internal.h"

static BookInfo g_reading;
const BookInfo *reading_state(void) { return &g_reading; }
void possessions_book_page(int page)
{
    if (page < 0 || page >= g_reading.pages || page == possessions_view.book_page) { return; }
    if (book_read(&g_reading,page)) { possessions_view.book_page = page; play_sound(205); }
    else { df_copy(possessions_view.notice, "This page is damaged.",sizeof(possessions_view.notice)); }
}

void possessions_read_close(void)
{
    book_close(&g_reading);
}

int possessions_read_open(int record)
{
    if (!book_open(&g_reading, record) || !book_read(&g_reading, 0)) {
        book_close(&g_reading);
        df_copy(possessions_view.notice, "This book could not be read.", sizeof(possessions_view.notice)); possessions_view.revision++; return 0;
    }
    if (!possessions_open(VIEW_BOOK, 0)) { book_close(&g_reading); return 0; }
    possessions_view.book_page = 0;
    return 1;
}

void possessions_reading_frame(const PlayIn *in, int click)
{
    if (in->key == KEY_PRIOR || in->key == KEY_LEFT) { possessions_book_page(possessions_view.book_page-1); }
    else if (in->key == KEY_NEXT || in->key == KEY_RIGHT) { possessions_book_page(possessions_view.book_page+1); }
    else if (click) {
        if (possessions_hit(in->cx,in->cy,181,188,193,196)) { possessions_book_page(possessions_view.book_page-1); }
        else if (possessions_hit(in->cx,in->cy,209,188,222,196)) { possessions_book_page(possessions_view.book_page+1); }
        else if (possessions_hit(in->cx,in->cy,240,187,269,196)) { possessions_view.prompt = PROMPT_PAGE; possessions_view.input[0] = 0; }
        else if (possessions_hit(in->cx,in->cy,278,187,307,196)) { possessions_close(); }
    }
}
