#ifndef OGFALL_BOOK_H
#define OGFALL_BOOK_H

/* BOOKS/BOKnnnnn.TXT: 234-byte header, page count, absolute u32 offsets,
 * then the book_draw_page token stream. A reader owns its file and page
 * buffer, not the world's temporary heap. Read docs/ogfall.md. */
#include "engine/core/df.h"
#include "engine/platform/fs.h"
#define OG_BOOK_PAGES 512
#define OG_BOOK_PAGE_BYTES 16000

typedef struct OgBook {
    FsFile *file;
    int pages;
    char title[65], author[65];
    df_u32 offset[OG_BOOK_PAGES + 1];
    df_u8 page[OG_BOOK_PAGE_BYTES];
    int page_bytes;
} OgBook;
int og_book_open(OgBook *book, int id);
void og_book_close(OgBook *book);
int og_book_read(OgBook *book, int page);
int og_book_title(int id, char *name, int capacity, df_u32 *seed);
int og_book_header(int id, char *title, int title_cap, char *author, int author_cap);
#endif
