#include "game/dagger/play/keys.h"
#include "engine/assets/recfile.h"
#include "engine/assets/vfs.h"
#include "engine/core/log.h"
#include "engine/platform/fs.h"
#include "engine/platform/input.h"

/* See keys.h. */

typedef struct KeyBind {
    df_u16 id;
    df_u16 vk;
} KeyBind;

static const RecField g_key_fields[] = {
    REC_FIELD("id", REC_UINT, KeyBind, id),
    REC_FIELD("vk", REC_UINT, KeyBind, vk)
};
static const RecSchema g_key_schema = { "KEYS", "key", g_key_fields, 2, (df_u16)sizeof(KeyBind), 0 };

/*
 * Classic's defaults, as Windows virtual keys (no windows.h here): the
 * left button 0x01, Shift 0x10, Space 0x20, a letter its capital.
 */
static const KeyBind g_defaults[] = {
    { INPUT_ATTACK, 0x01 },
    { INPUT_USE, 'E' },
    { INPUT_RUN, 0x10 },
    { INPUT_JUMP, 0x20 },
    { INPUT_CHARACTER, 'C' },
    { INPUT_INVENTORY, 'I' },
    { INPUT_MAP, 'M' },
    { INPUT_REST, 'R' },
    { INPUT_FORWARD, 'W' },
    { INPUT_BACK, 'S' },
    { INPUT_LEFT, 'A' },
    { INPUT_RIGHT, 'D' },
    { INPUT_SWAP_HANDS, 'H' }
};
#define DEFAULTS ((int)(sizeof(g_defaults) / sizeof(g_defaults[0])))

void keys_defaults(void)
{
    int i;

    for (i = 0; i < DEFAULTS; i++) {
        input_bind(g_defaults[i].id, g_defaults[i].vk);
    }
}

int keys_load(void)
{
    static RecTable table;
    const df_u8 *b;
    df_u32 n;
    df_u32 pos;
    RecChunk c;
    int i;

    keys_defaults();
    b = vfs_read_beside(KEYS_FILE, &n);
    if (!b || !rec_is_file(b, n)) {
        return 0;
    }
    if (!table.schema && !rec_table_init(&table, &g_key_schema, INPUT_ACTIONS)) {
        return 0;
    }
    rec_table_clear(&table);
    pos = 0;
    while (rec_next(b, n, &pos, &c)) {
        if (c.tag[0] == 'K' && c.tag[1] == 'E' && c.tag[2] == 'Y' && c.tag[3] == 'S') {
            rec_merge(&table, &c, 0);
        }
    }
    for (i = 0; i < table.count; i++) {
        const KeyBind *k;

        k = (const KeyBind *)(table.rows + (df_u32)i * sizeof(KeyBind));
        /* Escape is never rebound: a file that says so is ignored there. */
        if (k->id < INPUT_ACTIONS && k->id != INPUT_MENU && k->vk && !input_bind(k->id, k->vk)) {
            log_print(LOG_WARN, "keys: %s's key %d for action %d is refused", KEYS_FILE, (int)k->vk, (int)k->id);
        }
    }
    return 1;
}

static int put_file(void *ctx, const void *src, df_u32 n)
{
    (void)ctx;
    return fs_append(src, n);
}

int keys_save(void)
{
    KeyBind rows[INPUT_ACTIONS];
    RecHead head;
    int n;
    int i;

    n = 0;
    for (i = 0; i < INPUT_ACTIONS; i++) {
        if (i == INPUT_MENU) {
            continue;
        }
        rows[n].id = (df_u16)i;
        rows[n].vk = (df_u16)input_binding(i);
        n++;
    }
    head.id = 0;
    head.role = REC_HEAD_SELF;
    head.size = 1;
    df_copy(head.name, "KEYS.DAT", REC_NAME);
    if (!fs_make_dir("DATA") || !fs_create(KEYS_FILE) || !rec_put_begin(put_file, 0)
        || !rec_put_rows(put_file, 0, &rec_head_schema, &head, 1, 0, 0)
        || !rec_put_rows(put_file, 0, &g_key_schema, rows, n, 0, 0)) {
        fs_abort();
        log_text(LOG_WARN, "keys: DATA\\KEYS.DAT could not be written");
        return 0;
    }
    fs_finish();
    return 1;
}
