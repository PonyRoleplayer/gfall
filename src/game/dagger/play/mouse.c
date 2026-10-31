#include "game/dagger/play/mouse.h"

/* See mouse.h. */

typedef struct MouseRow {
    df_u16 id;
    df_u8 cursor;
} MouseRow;

static const MouseRow g_default_row = { 0, 1 };
static const RecField g_fields[] = {
    REC_FIELD("id", REC_UINT, MouseRow, id),
    REC_FIELD("cursor", REC_UINT, MouseRow, cursor)
};
static const RecSchema g_schema = { "MOUS", "mouse", g_fields, 2, (df_u16)sizeof(MouseRow), &g_default_row };

/* 1 cursor, 0 view. Classic starts on the pointer. */
static int g_cursor = 1;

void mouse_defaults(void)
{
    g_cursor = 1;
}

int mouse_cursor(void)
{
    return g_cursor;
}

void mouse_set_cursor(int on)
{
    g_cursor = on ? 1 : 0;
}

int mouse_pointer(int ui_up)
{
    return ui_up || g_cursor;
}

void mouse_read(const df_u8 *bytes, df_u32 n)
{
    static RecTable table;
    RecChunk chunk;
    df_u32 pos;
    const MouseRow *row;

    if (!bytes || !rec_is_file(bytes, n)) {
        return;
    }
    if (!table.schema && !rec_table_init(&table, &g_schema, 1)) {
        return;
    }
    rec_table_clear(&table);
    pos = 0;
    while (rec_next(bytes, n, &pos, &chunk)) {
        if (chunk.tag[0] == 'M' && chunk.tag[1] == 'O' && chunk.tag[2] == 'U' && chunk.tag[3] == 'S') {
            rec_merge(&table, &chunk, 0);
        }
    }
    row = (const MouseRow *)rec_row(&table, 0);
    /* A missing row leaves the caller's default. Anything but 0 or 1 is ignored. */
    if (row && row->cursor <= 1) {
        g_cursor = row->cursor;
    }
}

int mouse_write(RecPut put, void *ctx)
{
    MouseRow row;

    row.id = 0;
    row.cursor = (df_u8)g_cursor;
    return rec_put_rows(put, ctx, &g_schema, &row, 1, 0, 0);
}
