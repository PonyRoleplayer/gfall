#include "game/dagger/formats/book.h"
#include "engine/core/mem.h"
#include "engine/core/log.h"

#ifdef DF_HOST
#define BOOK_PATH "BOOKS/BOK%05d.TXT"
#else
#define BOOK_PATH "BOOKS\\BOK%05d.TXT"
#endif

void book_close(BookInfo *book)
{
    if (book->file) { fs_close(book->file); }
    mem_zero(book, sizeof(*book));
}

int book_open(BookInfo *book, int id)
{
    char path[32];
    df_u8 header[236], offset[4];
    df_u32 size, start, end;
    int i;
    book_close(book);
    if (id < 0 || id > 99999) { return 0; }
    log_format(path, sizeof(path), BOOK_PATH, id);
    book->file = fs_open(path);
    if (!book->file) { return 0; }
    size = fs_size(book->file);
    if (size < 240 || !fs_read_at(book->file, 0, header, sizeof(header))) { goto bad; }
    book->pages = df_ru16(header + 234);
    if (book->pages < 1 || book->pages > BOOK_PAGES || 236u + 4u * book->pages >= size) { goto bad; }
    /* Title and author are 64-byte fields at offsets 0 and 64. */
    mem_copy(book->title, header, 64);
    mem_copy(book->author, header + 64, 64);
    book->title[64] = book->author[64] = 0;
    start = 236u + 4u * book->pages;
    for (i = 0; i < book->pages; i++) {
        if (!fs_read_at(book->file, 236u + 4u * i, offset, 4)) { goto bad; }
        end = df_ru32(offset);
        if (end < start || end >= size) { goto bad; }
        book->offset[i] = end;
        start = end;
    }
    book->offset[book->pages] = size;
    return 1;
bad:
    book_close(book);
    return 0;
}

int book_read(BookInfo *book, int page)
{
    df_u32 size;
    int i;
    book->page_bytes = 0;
    if (!book->file || page < 0 || page >= book->pages) { return 0; }
    /* Original pages may begin inside continuous text and extend beyond the
     * following page's offset. book_draw_page reads up to 16000 bytes, then
     * stops at token 246; preserve that behavior with an explicit file bound. */
    size = book->offset[book->pages] - book->offset[page];
    if (size > BOOK_PAGE_BYTES) { size = BOOK_PAGE_BYTES; }
    if (!size || !fs_read_at(book->file, book->offset[page], book->page, size)) { return 0; }
    /* Walk token operands too: 246 inside an x-coordinate or picture name
     * is data, not an end marker. A damaged operand cannot escape this page. */
    for (i = 0; i < (int)size; ) {
        int token;
        token = book->page[i++];
        if (token == 246) { book->page_bytes = i; return 1; }
        if (token == 251) { if (i + 2 > (int)size) { return 0; } i += 2; }
        else if (token == 249 || token == 250) { if (i >= (int)size) { return 0; } i++; }
        else if (token == 247) {
            while (i < (int)size && book->page[i]) { i++; }
            if (i >= (int)size) { return 0; }
            i++;
        }
    }
    return 0;
}

int book_title(int id, char *name, int capacity, df_u32 *seed)
{
    FsFile *file;
    char path[32];
    df_u8 title[65];
    int good;
    if (id < 0 || id > 99999) { return 0; }
    log_format(path, sizeof(path), BOOK_PATH,id);
    file = fs_open(path);
    if (!file) { return 0; }
    good = fs_read_at(file,0,title,64);
    fs_close(file);
    if (!good) { return 0; }
    title[64] = 0;
    df_copy(name,(const char *)title,capacity);
    if (seed) { *seed = df_ru32(title); }
    return 1;
}

int book_header(int id, char *title, int title_cap, char *author, int author_cap)
{
    FsFile *file;
    char path[32], field[65];
    df_u8 header[128];
    int good;
    if (id < 0 || id > 99999) { return 0; }
    log_format(path,sizeof(path),BOOK_PATH,id);
    file = fs_open(path);
    if (!file) { return 0; }
    good = fs_read_at(file,0,header,sizeof(header));
    fs_close(file);
    if (!good) { return 0; }
    mem_copy(field,header,64); field[64] = 0; df_copy(title,field,title_cap);
    mem_copy(field,header+64,64); field[64] = 0; df_copy(author,field,author_cap);
    return 1;
}
