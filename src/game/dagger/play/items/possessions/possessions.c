#include "internal.h"
#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/ui/msg.h"
#include "engine/core/mem.h"
#include "game/dagger/play/magic/itemmaker.h"

Possessions possessions_view;

const Possessions *possessions_state(void) { return &possessions_view; }
void possessions_reset(void)
{
    possessions_dialog_clear();
    possessions_read_close();
    mem_zero(&possessions_view, sizeof(possessions_view));
    possessions_view.skill_group = -1;
    possessions_view.revision++;
}

static int game_mode(int page)
{
    switch (page) {
    case VIEW_SHEET: return GAME_SHEET;
    case VIEW_INVENTORY: return GAME_INVENTORY;
    case VIEW_SPELLS: return GAME_SPELLBOOK;
    default: return GAME_READING;
    }
}

void possessions_close(void)
{
    int mode;
    if (!possessions_view.page) { return; }
    if (possessions_view.page == VIEW_SHEET && !progress_finish()) {
        df_copy(possessions_view.notice,"You have not distributed all your attribute points.",sizeof(possessions_view.notice));
        possessions_view.revision++; return;
    }
    possessions_dialog_clear();
    if (possessions_view.page == VIEW_INVENTORY && possessions_view.trade) { market_close(); possessions_view.trade = 0; possessions_view.price = 0; possessions_view.offer[0] = 0; }
    if (possessions_view.page == VIEW_BOOK) { possessions_read_close(); }
    if (possessions_view.page == VIEW_TALK) { talkwin_close(); }
    state_mode_pop();
    mode = game_state()->mode;
    possessions_view.page = mode == GAME_SHEET ? VIEW_SHEET : mode == GAME_INVENTORY ? VIEW_INVENTORY : VIEW_NONE;
    possessions_view.notice[0] = possessions_view.input[0] = 0;
    possessions_view.prompt = 0;
    possessions_view.skill_group = -1;
    possessions_view.drag = possessions_view.repeat_step = 0;
    possessions_view.notice_page = 0;
    possessions_view.painting = 0;
    possessions_view.revision++;
}

int possessions_open(int page, df_u32 remote)
{
    if (!game_state()->ready || page < VIEW_INVENTORY || page > VIEW_TRANSPORT) { return 0; }
    /* inventory_open: a beast has no pack to open (docs/curses.md 7.3). */
    if (page == VIEW_INVENTORY && curse_beast()) { msg_set("You cannot access the inventory while shapechanged..."); return 0; }
    if (page == VIEW_SPELLS) {
        df_u32 ids[OBJECTS];
        possessions_view.book = inventory_book();
        if (!possessions_view.book || !spell_rows(possessions_view.book, ids, OBJECTS)) {
            if (possessions_view.page) { df_copy(possessions_view.notice, "Your spellbook contains no spells.", sizeof(possessions_view.notice)); possessions_view.revision++; }
            else { msg_set("Your spellbook contains no spells."); }
            return 0;
        }
    }
    if (!state_mode_push(game_mode(page))) { return 0; }
    possessions_view.underlay = possessions_view.page;
    possessions_view.page = page;
    possessions_view.notice[0] = possessions_view.input[0] = 0;
    possessions_view.prompt = 0;
    possessions_view.skill_group = -1;
    possessions_view.skill_scroll = 0;
    possessions_view.selected = 0;
    possessions_view.drag = possessions_view.repeat_step = possessions_view.notice_page = 0;
    possessions_view.painting = 0;
    if (page == VIEW_SPELLS) { possessions_view.scroll[0] = 0; }
    if (page == VIEW_INVENTORY) {
        itemmaker_refresh_values();
        possessions_view.remote = remote;
        possessions_view.scroll[0] = possessions_view.scroll[1] = 0;
        possessions_view.tab = 0;
        possessions_view.action = ITEM_EQUIP;
    }
    /* inventory_open and an ordinary sheet_open are silent. Only the two
     * book screens play OpenBook (DAGGER.SND ID 237) on entry. */
    if (page == VIEW_SPELLS || page == VIEW_BOOK) { play_sound(237); }
    possessions_view.revision++;
    return 1;
}

int possessions_hit(int x, int y, int left, int top, int right, int bottom)
{
    return x > left && x < right && y > top && y < bottom;
}

/* Returns 1 when selecting a spell has handed control back to casting. */
static int page_frame(const PlayIn *in, int click)
{
    int page = possessions_view.page;

    if (in->ch == 27 && page != VIEW_TALK && page != VIEW_MAKER) {
        possessions_close();
        return 0;
    }
    switch (page) {
    case VIEW_INVENTORY: possessions_inventory_frame(in, click); break;
    case VIEW_BANK: possessions_bank_frame(in, click); break;
    case VIEW_SHEET: possessions_sheet_frame(in, click); break;
    case VIEW_SPELLS: return possessions_spellbook_frame(in, click);
    case VIEW_BOOK: possessions_reading_frame(in, click); break;
    case VIEW_SPELLSHOP: possessions_spellshop_frame(in, click); break;
    case VIEW_ITEMMAKER: possessions_itemmaker_frame(in, click); break;
    case VIEW_MAKER: possessions_spellmaker_frame(in, click); break;
    case VIEW_POTION: possessions_alchemy_frame(in, click); break;
    case VIEW_SERVICE: possessions_service_frame(in, click); break;
    case VIEW_TRANSPORT: possessions_transport_frame(in, click); break;
    case VIEW_TALK: possessions_talk_frame(in, click); break;
    default: break;
    }
    return 0;
}

void possessions_frame(const PlayIn *in)
{
    int click;

    if (!possessions_view.page) { return; }
    if (possessions_view.trade) { possessions_trade_quote(); }
    click = (in->mouse_pressed & (possessions_view.page == VIEW_SPELLS ? 3 : 1)) && in->on_picture;
    if (click) { play_sound(possessions_view.page == VIEW_SPELLS ? 205 : 203); }

    /* Modal input takes precedence over the underlying page. */
    if (!possessions_view.notice[0]) { possessions_view.notice_page = 0; }
    if (possessions_view.prompt) { possessions_prompt_frame(in, click); }
    else if (possessions_view.notice[0]) { possessions_notice_frame(in, click); }
    else if (possessions_view.skill_group >= 0) { possessions_skill_frame(in, click); }
    else if (page_frame(in, click)) { return; }
    possessions_message_frame();
    if (click || in->ch || in->key) { possessions_view.revision++; }
}
