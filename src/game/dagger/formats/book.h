#ifndef GFALL_BOOK_H
#define GFALL_BOOK_H

/* BOOKS/BOKnnnnn.TXT: 234-byte header, page count, absolute u32 offsets,
 * then the book_draw_page token stream. A reader owns its file and page
 * buffer, not the world's temporary heap. Read docs/gfall.md. */
#include "engine/core/df.h"
#include "engine/platform/fs.h"
#define BOOK_PAGES 512
#define BOOK_PAGE_BYTES 16000

typedef struct BookInfo {
    FsFile *file;
    int pages;
    char title[65], author[65];
    df_u32 offset[BOOK_PAGES + 1];
    df_u8 page[BOOK_PAGE_BYTES];
    int page_bytes;
} BookInfo;
int book_open(BookInfo *book, int id);
void book_close(BookInfo *book);
int book_read(BookInfo *book, int page);
int book_title(int id, char *name, int capacity, df_u32 *seed);
int book_header(int id, char *title, int title_cap, char *author, int author_cap);
#endif
