#include "game/dagger/play/state.h"
#include "game/dagger/play/items/possessions.h"
#include "game/dagger/play/items/inventory.h"
#include "game/dagger/play/ui/screen.h"
#include "game/dagger/play/magic/magic.h"
#include "game/dagger/play/cheats.h"
#include "game/dagger/play/magic/spellmaker.h"
#include "game/dagger/play/magic/itemmaker.h"
#include "game/dagger/play/quests/quest.h"
#include "game/dagger/tables/magicdefs.h"
#include "game/dagger/tables/itemdefs.h"
#include "game/dagger/tables/careers.h"
#include "game/dagger/play/ui/options.h"
#include "../seam.h"
#include "../sound.h"
#include "screen.h"
#include "callbacks.h"

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/core/mem.h"
#include "engine/ui/pic.h"

#include "game/dagger/formats/faces.h"
#include "game/dagger/tables/holidaydefs.h"
#include "game/dagger/tables/datedefs.h"
#include "game/dagger/play/people/bank.h"
#include "game/dagger/play/people/names.h"
#include "game/dagger/play/people/talk.h"
#include "game/dagger/play/people/guild.h"
#include "game/dagger/play/magic/potion.h"
#include "game/dagger/play/magic/spellshop.h"
#include "game/dagger/play/people/talkwin.h"
#include "game/dagger/formats/cif.h"
#include "game/dagger/formats/img.h"
#include "game/dagger/formats/smallfont.h"
#include "game/dagger/formats/texture.h"
#include "engine/core/prof.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/play/travel/travelmap.h"

#include "game/dagger/play/character/curse.h"
#include "game/dagger/play/play.h"
#include "game/dagger/tables/tables.h"

#include "internal.h"
#include "game/dagger/play/ui/canvas.h"

static char g_talk_name[NAME_MAX];
static df_u8 g_talk_face[FACE_W * FACE_H];
static int g_talk_face_ok;
static ImgPicture g_possession_art[5];
static SmallFont g_book_font[5];
static const df_u8 *g_spell_icons, *g_spell_masks;
static CifSet g_container_icons;
static df_u8 g_doll[110 * 184], g_doll_back[110 * 184];
static int g_panel_files[64], g_panel_file_count;
static int g_possession_page;
static unsigned g_possession_revision;
/* The controllers own input and records; this adapter paints original assets. */
void gfall_screen_load_possessions(void)
{
    static const char *name[] = {"INVE00I0.IMG", "INVE01I0.IMG", "INFO00I0.IMG", "BOOK00I0.IMG", "SPBK00I0.IMG"};
    char file[20];
    df_u32 size;
    int i;
    for (i = 0; i < 5; i++) { img_load(name[i], &g_possession_art[i]); }
    g_spell_icons = vfs_read("ICON00I0.IMG", &size);
    if (size < 320u * 64u) { g_spell_icons = 0; }
    g_spell_masks = vfs_read("MASK04I0.IMG", &size);
    if (size < 40u * 80u) { g_spell_masks = 0; }
    for (i = 0; i < 5; i++) {
        log_format(file,sizeof(file),"FONT%04d.FNT",i ? i-1 : 3);
        smallfont_load(&g_book_font[i],file);
    }
    cif_open("INVE16I0.CIF", &g_container_icons);
    gfall_screen_load_rest();
    spells_load();
    potions_register();
}

/* Every decode for one changed screen dies after composition. A newly read
 * uncapped archive is forgotten before this heap goes, so the file cache can
 * never hold a pointer into freed UI storage. */
static Image *possession_texture_frame(int file, int record, int frame)
{
    if (!texture_cached(file) && g_panel_file_count < 64) { g_panel_files[g_panel_file_count++] = file; }
    return texture_load_frame(file, record, frame);
}

static Image *possession_texture(int file, int record)
{
    return possession_texture_frame(file,record,0);
}

static void doll_layer(const df_u8 *pixels, int w, int h, int x, int y, int owner, int color_map)
{
    int row, col, dx, dy, color;
    df_u8 *owners;
    owners = doll_owners();
    if (!pixels) { return; }
    for (row = 0; row < h; row++) {
        dy = y + row - 8;
        if (dy < 0 || dy >= 184) { continue; }
        for (col = 0; col < w; col++) {
            dx = x + col - 200;
            color = pixels[row*w+col];
            if (!color || dx < 0 || dx >= 110) { continue; }
            if (owner >= 0 && color == 255) {
                g_doll[dy*110+dx] = g_doll_back[dy*110+dx];
                owners[dy*110+dx] = 255;
            } else {
                g_doll[dy*110+dx] = (df_u8)item_ink(color_map,color);
                if (owner >= 0) { owners[dy*110+dx] = (df_u8)owner; }
            }
        }
    }
}

static void doll_img(const char *name)
{
    ImgPicture picture;
    if (img_load(name,&picture)) { doll_layer(picture.pixels,picture.width,picture.height,picture.x,picture.y,-1,-1); }
}

static int doll_item_image(const ObjectInfo *item, int slot)
{
    int image, file;
    image = item->inventory_image;
    if (item->group != 3) { return image; }
    /* paperdoll_add_item changes a temporary copy: gender archive, then hand.
     * Bow archives are shared; one-handed weapons have a second, right-hand
     * record. Keep the saved item and its inventory-list picture unchanged. */
    file = image >> 7;
    if (character_state()->female && file != 432 && file != 433) { image -= 128; }
    if (slot == 19 && !item_two_handed(item)) { image++; }
    return image;
}

typedef struct DollLayer { int slot, image, order; } DollLayer;

static void doll_insert(DollLayer *layers, int *count, int slot, int image, int order)
{
    int at;
    at = (*count)++;
    /* Equal layers precede existing entries in paperdoll_add_item. */
    while (at && layers[at-1].order >= order) { layers[at] = layers[at-1]; at--; }
    layers[at].slot = slot; layers[at].image = image; layers[at].order = order;
}

static void paint_doll(int x, int y)
{
    const CareerRace *race;
    const ObjectInfo *item;
    const ItemDef *def;
    ImgPicture back;
    CifSet faces;
    const CifFrame *face;
    Image *image;
    DollLayer layers[27];
    int i, n, slot, image_id, layer, cloak, px, py, color_map;
    race = career_race(character_state()->race+1);
    /* A beast has its own picture, background and body in one, and wears nothing (docs/curses.md 7.3). */
    if (curse_beast()) {
        if (!img_load(curse_form() == 9 ? "WOLF00I0.IMG" : "BOAR00I0.IMG",&back) || back.width < 118 || back.height < 191) { return; }
        for (i = 0; i < 184; i++) { mem_copy(g_doll+i*110,back.pixels+(i+7)*back.width+8,110); }
        gfall_screen_blit_ix(x,y,g_doll,110,184,0);
        return;
    }
    if (!race || !img_load(race->background,&back) || back.width < 118 || back.height < 191) { return; }
    for (i = 0; i < 184; i++) { mem_copy(g_doll_back+i*110,back.pixels+(i+7)*back.width+8,110); }
    mem_copy(g_doll,g_doll_back,sizeof(g_doll));
    for (i = 0; i < (int)sizeof(g_doll); i++) { doll_owners()[i] = 255; }
    doll_img(character_state()->female ? race->body_female : race->body_male);
    doll_img(character_state()->female ? race->body_female_clothed : race->body_male_clothed);
    if (cif_open(character_state()->female ? race->heads_female : race->heads_male,&faces)) {
        face = cif_frame(&faces,character_state()->face);
        if (face) { doll_layer(face->pixels,face->w,face->h,face->x,face->y,-1,-1); }
        cif_close(&faces);
    }
    n = cloak = 0;
    for (i = 0; i < 27; i++) {
        item = object_at(game_state()->equipped[i]);
        if (!item || i < 12) { continue; }
        def = item_def(item->group,item->index);
        if (!def) { continue; }
        layer = def->draw_order;
        if (item->group == 3 && i == 21 && !item_two_handed(item)) { layer += 5; }
        doll_insert(layers,&n,i,doll_item_image(item,i),layer);
        if (!cloak && ((item->group == 6 && (item->index == 13 || item->index == 14)) ||
                       (item->group == 12 && (item->index == 9 || item->index == 10)))) {
            /* One cloak's first variant forms the back, below all equipment. */
            image_id = (item->inventory_image & ~127) | clothing_first(item->group,item->inventory_image & 127);
            doll_insert(layers,&n,i,image_id,0); cloak = 1;
        }
    }
    for (i = 0; i < n; i++) {
        slot = layers[i].slot;
        item = object_at(game_state()->equipped[slot]);
        image_id = layers[i].image;
        image = possession_texture(image_id >> 7,image_id & 127);
        if (!image || !texture_offset(image_id >> 7,image_id & 127,&px,&py)) { continue; }
        color_map = item->color;
        doll_layer(image->index,image->width,image->height,px,py,slot,color_map);
    }
    gfall_screen_blit_ix(x,y,g_doll,110,184,0);
}

static void copy_art_rect(const ImgPicture *art, int x, int y, int w, int h)
{
    int row;
    if (!art->pixels || x < 0 || y < 0 || x+w > art->width || y+h > art->height) { return; }
    for (row = 0; row < h; row++) { mem_copy(gfall_screen_pixels()+(y+row)*320+x,art->pixels+(y+row)*art->width+x,w); }
}

static void paint_item_icon(const ObjectInfo *item, int x, int y, int width, int height)
{
    const Image *image;
    int w, h, row, col, color, color_map;
    image = possession_texture(item->inventory_image >> 7,item->inventory_image & 127);
    if (!image) { return; }
    w = image->width; h = image->height;
    size_fit(&w,&h,width,height);
    if (!w || !h) { return; }
    color_map = item->color;
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            color = image->index[(row*image->height/h)*image->width+col*image->width/w];
            if (color && color != 255) { gfall_screen_put_px(x+(width-w)/2+col,y+(height-h)/2+row,item_ink(color_map,color)); }
        }
    }
}

/* The coins that turn behind what is for sale (DFU's coinsAnimTextureName: TEXTURE.434 record 6, 0.08 s a frame): the frame showing, and the one the screen was last painted with. */
static int g_coin_now = -1, g_coin_shown = -1;
static int coin_frame_now(void)
{
    int n, mhz;

    if (!possession_texture(434,6)) { return -1; }
    n = texture_frame_count(434,6);
    n = n > 16 ? 16 : n;
    mhz = prof_mhz();
    return n > 1 && mhz > 0 ? (int)((prof_now() / ((df_u32)mhz * 80000u)) % (df_u32)n) : 0;
}

static void paint_coins(int x, int y)
{
    const Image *frame;
    int row, col, color, left, top;

    if (g_coin_now < 0) { return; }
    frame = texture_load_frame(434,6,g_coin_now);
    if (!frame || frame->width > 50 || frame->height > 37) { return; }
    left = x+(50-frame->width)/2; top = y+(37-frame->height)/2;
    for (row = 0; row < frame->height; row++) {
        for (col = 0; col < frame->width; col++) {
            color = frame->index[row*frame->width+col];
            if (color && color != 255) { gfall_screen_put_px(left+col,top+row,color); }
        }
    }
}

/* TEXTURE.380 record 5 has nine 51 x 39 frames. Crop it at the
 * list cell edges and scale it to the smaller accessory slots. */
static int g_enchant_visible, g_enchant_frame, g_enchant_shown = -1;
static const Image *g_enchant_mark;

#ifdef DF_HOST
static int g_enchant_test_frame = -1;
void seam_enchant_test_frame(int frame) { g_enchant_test_frame = frame; }
#endif

static int enchant_mark_frame(void)
{
    int mhz;

#ifdef DF_HOST
    if (g_enchant_test_frame >= 0) { return g_enchant_test_frame; }
#endif
    mhz = prof_mhz();
    return mhz > 0 ? (int)((prof_now() / ((df_u32)mhz * 80000u)) % 9u) : 0;
}

static void paint_enchant_mark(int x, int y, int w, int h, int scale)
{
    const Image *mark;
    int row, col, sx, sy, color;

    if (!g_enchant_visible) {
        g_enchant_mark = possession_texture_frame(380,5,g_enchant_frame);
        g_enchant_visible = 1;
    }
    mark = g_enchant_mark;
    if (!mark) { return; }
    for (row = 0; row < h; row++) {
        sy = scale ? row*mark->height/h : row+(mark->height-h)/2;
        if (sy < 0 || sy >= mark->height) { continue; }
        for (col = 0; col < w; col++) {
            sx = scale ? col*mark->width/w : col+(mark->width-w)/2;
            if (sx < 0 || sx >= mark->width) { continue; }
            color = mark->index[sy*mark->width+sx];
            if (color && color != 255) { gfall_screen_put_px(x+col,y+row,color); }
        }
    }
}

static void paint_item_cell(const ObjectInfo *item, int x, int y)
{
    char line[32];
    if (possessions_state()->trade == MARKET_BUY) { paint_coins(x,y); }
    if (item_enchanted(item->id)) { paint_enchant_mark(x,y,50,37,0); }
    paint_item_icon(item,x+2,y+2,46,33);
    if (item_equipped(item->id) >= 0) { gfall_screen_outline(x,y,50,37,145); }
    if (item->count > 1 || item->group == 28) {
        log_format(line,sizeof(line),"%d",item->group == 28 ? item->value : item->count);
        gfall_screen_text_at(x+2,y+29,line);
    }
}

/* Words filled into lines no wider than width pixels, a word never split; how many lines. */
static int wrap_words(const char *text, char lines[][96], int most, int width)
{
    char test[96];
    int rows, len, word, i;

    rows = 0;
    lines[0][0] = 0;
    while (*text && rows < most) {
        while (*text == ' ') { text++; }
        for (word = 0; text[word] && text[word] != ' '; word++) { }
        len = 0;
        while (lines[rows][len]) { len++; }
        for (i = 0; i < len; i++) { test[i] = lines[rows][i]; }
        if (len) { test[len++] = ' '; }
        for (i = 0; i < word && len < 94; i++) { test[len++] = text[i]; }
        test[len] = 0;
        if (lines[rows][0] && smallfont_width(gfall_screen_font(),test) > width) {
            if (++rows >= most) { rows = most - 1; break; }
            lines[rows][0] = 0;
            continue;
        }
        for (i = 0; i <= len; i++) { lines[rows][i] = test[i]; }
        text += word;
    }
    return lines[rows][0] ? rows + 1 : rows;
}

/* The bank (bank_draw, src/hand/func_0006BA50.c): BANK00I0.IMG at its place, the balance, the gold carried, and a loan with its date. */
static void paint_bank(void)
{
    ImgPicture window;
    const BankAccount *account;
    char line[80];

    gfall_screen_cover_screen(0,0,0);
    if (img_load("BANK00I0.IMG",&window)) { gfall_screen_blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    account = &bank_state()->account[possessions_state()->bank_region];
    log_format(line,sizeof(line),"%d",account->balance); gfall_screen_text_at(197,19,line);
    log_format(line,sizeof(line),"%d",gold_total()); gfall_screen_text_at(203,29,line);
    if (account->loan_due) {
        log_format(line,sizeof(line),"%d",account->loan_owed); gfall_screen_text_at(143,39,line);
        date_text(account->loan_due,line,sizeof(line)); gfall_screen_text_at(119,49,line);
    }
    if (possessions_state()->prompt == PROMPT_AMOUNT) {
        log_format(line,sizeof(line),"%s_",possessions_state()->input); gfall_screen_text_at(157,152,line);
    }
}

static void paint_inventory(void)
{
    const Possessions *view;
    const ObjectInfo *item;
    const CifFrame *icon;
    df_u32 rows[OBJECTS];
    static const int tab_x[4] = {0,93,163,255}, tab_w[4] = {92,69,91,65};
    static const int action_y[4] = {36,58,80,103};
    char line[64];
    int side, n, i, at, x;
    view = possessions_state();
    gfall_screen_cover_screen(g_possession_art[0].pixels,g_possession_art[0].width,g_possession_art[0].height);
    copy_art_rect(&g_possession_art[1],tab_x[view->tab],0,tab_w[view->tab],10);
    if (view->trade) {
        /* inventory_draw: the shop's column (INVE08/10/12), the lit button of the current action, then the doll. */
        ImgPicture column, lit;
        static const int lit_top[4] = {0, 0, 0, 0};
        char file[16];
        int top, act;

        (void)lit_top;
        log_format(file,sizeof(file),"INVE%02dI0.IMG",view->trade*2+6);
        if (img_load(file,&column)) { gfall_screen_blit_ix(column.x,column.y,column.pixels,column.width,column.height,0); }
        log_format(file,sizeof(file),"INVE%02dI0.IMG",view->trade*2+7);
        /* Info is the second button of every column, Select the third (inv_mode_buttons). */
        act = view->action == ITEM_INFO ? 36 : 58;
        if (img_load(file,&lit)) {
            for (top = act; top <= act+13; top++) {
                int col;
                for (col = 226; col <= 256; col++) {
                    int sx, sy;
                    sx = col-lit.x; sy = top-lit.y;
                    if (sx >= 0 && sx < lit.width && sy >= 0 && sy < lit.height) { gfall_screen_put_px(col,top,lit.pixels[sy*lit.width+sx]); }
                }
            }
        }
    } else {
        copy_art_rect(&g_possession_art[1],226,action_y[view->action],31,14);
    }
    paint_doll(49,13);
    if (view->trade) {
        ImgPicture cost;
        char total[16];

        if (img_load("SHOP00I0.IMG",&cost)) { gfall_screen_blit_ix(cost.x,cost.y,cost.pixels,cost.width,cost.height,0); }
        log_format(total,sizeof(total),"%d",view->price);
        gfall_screen_text_at(77,15,total);
        log_format(total,sizeof(total),"%d",character_state()->gold);
        gfall_screen_text_at(107,15,total);
    }
    for (i = 0; i < 12; i++) {
        item = object_at(game_state()->equipped[i]);
        if (item) {
            int left, top;

            left = i%2*23; top = 10+i/2*31;
            if (item_enchanted(item->id)) { paint_enchant_mark(left,top,22,21,1); }
            paint_item_icon(item,left,top,22,21);
        }
    }
    {
        static const int armor_x[7] = {120,70,130,60,55,60,70};
        static const int armor_y[7] = {24,50,50,70,100,120,180};
        for (i = 0; i < 7; i++) { gfall_screen_number_at(armor_x[i],armor_y[i],inventory_armor(i),0,0); }
    }
    for (side = 0; side < 2; side++) {
        x = side ? 270 : 172;
        n = inventory_list(side ? view->remote : game_state()->inventory_id,view->tab,rows,OBJECTS);
        for (i = 0; i < 4; i++) {
            at = view->scroll[side]+i;
            if (at >= n || at >= OBJECTS) { break; }
            item = object_at(rows[at]);
            paint_item_cell(item,x,48+i*38);
        }
        {
            char weight[24];
            weight_text(inventory_weight(side ? view->remote : game_state()->inventory_id),weight,sizeof(weight));
            log_format(line,sizeof(line),"%s kg",weight);
        }
        gfall_screen_centered_text(x+22,36,line,145);
    }
    icon = cif_frame(&g_container_icons,view->remote && !(object_at(view->remote)->thing_key & 0x80000000u) ? 5 : 2);
    if (icon) {
        int w, h, row, col, color;
        w = icon->w; h = icon->h;
        size_fit(&w,&h,46,29);
        if (w > 0 && h > 0) {
            for (row = 0; row < h; row++) { for (col = 0; col < w; col++) {
                color = icon->pixels[(row*icon->h/h)*icon->w+col*icon->w/w];
                if (color && color != 255) { gfall_screen_put_px(272+(46-w)/2+col,12+(29-h)/2+row,color); }
            } }
        }
    }
    if (!view->trade) {
        log_format(line,sizeof(line),"%d",character_state()->gold);
        gfall_screen_centered_text(240,151,line,145);
    }
}

static int skill_in_career(int skill)
{
    const ClassInfo *career;
    int i;
    career = &game_state()->career;
    for (i = 0; i < 3; i++) { if (career->primary[i] == skill || career->major[i] == skill) { return 1; } }
    for (i = 0; i < 6; i++) { if (career->minor[i] == skill) { return 1; } }
    return 0;
}

static void paint_skill_list(void)
{
    const Possessions *view;
    const ClassInfo *career;
    const char *name;
    static const char *title[] = {"Primary Skills","Major Skills","Minor Skills","Miscellaneous Skills"};
    int i, skill, n, shown;
    char line[64];
    view = possessions_state(); career = &game_state()->career;
    gfall_screen_lay_paper(2,2,9,9);
    gfall_screen_centered_text(100,12,title[view->skill_group],145);
    shown = n = 0;
    for (i = 0; i < (view->skill_group == 0 || view->skill_group == 1 ? 3 : view->skill_group == 2 ? 6 : SKILLS); i++) {
        skill = view->skill_group == 0 ? career->primary[i] : view->skill_group == 1 ? career->major[i] :
                view->skill_group == 2 ? career->minor[i] : i;
        if (view->skill_group == 3 && skill_in_career(skill)) { continue; }
        if (n++ < view->skill_scroll || shown >= 16) { continue; }
        name = skill_name(skill);
        log_format(line,sizeof(line),"%s: %d%%",name,character_state()->skill[skill]);
        smallfont_draw(gfall_screen_font(),gfall_screen_pixels(),CANVAS_W,CANVAS_H,14,27+shown*9,
                       progress_raised(skill) ? 146 : 145,-1,line); shown++;
    }
}

static void paint_character_sheet(void)
{
    const CharacterState *who;
    char line[80];
    int i;
    who = character_state();
    gfall_screen_cover_screen(g_possession_art[2].pixels,g_possession_art[2].width,g_possession_art[2].height);
    paint_doll(200,8);
    gfall_screen_text_at(41,4,who->name); gfall_screen_text_at(41,14,race_name(who->race)); gfall_screen_text_at(46,24,game_state()->career.name);
    gfall_screen_number_at(45,34,who->level,0,0); gfall_screen_number_at(39,44,who->gold,0,0);
    log_format(line,sizeof(line),"%d/%d",who->fatigue>>6,who->attribute[ATTR_STR]+who->attribute[ATTR_END]);
    gfall_screen_centered_text(77,54,line,145);
    log_format(line,sizeof(line),"%d/%d",who->health,who->max_health); gfall_screen_centered_text(72,64,line,145);
    log_format(line,sizeof(line),"%d/%d",inventory_weight(game_state()->inventory_id)/4,inventory_capacity()); gfall_screen_text_at(91,74,line);
    for (i = 0; i < ATTRS; i++) { gfall_screen_number_at(155,17+i*24,who->attribute[i],1,0); }
    if (progress_state()->active) {
        i = possessions_state()->selected;
        if (i < 0 || i >= ATTRS) { i = 0; }
        gfall_screen_attribute_arrows(176,5+i*24);
        gfall_screen_number_at(183,5+i*24+14-gfall_screen_font()->height,progress_state()->points,1,0);
    }
    if (possessions_state()->skill_group >= 0) { paint_skill_list(); }
}

static void blit_stride(int x, int y, const df_u8 *src, int stride, int w, int h)
{
    int row, col;
    for (row = 0; row < h; row++) { for (col = 0; col < w; col++) { gfall_screen_put_px(x+col,y+row,src[row*stride+col]); } }
}

static void paint_spellbook(void)
{
    const Possessions *view;
    const SpellDef *spell;
    const ObjectInfo *owned;
    df_u32 ids[OBJECTS];
    int count, i, selected, x, y, at, top, height, length;
    char line[48];
    view = possessions_state();
    if (view->underlay == VIEW_INVENTORY) { paint_inventory(); }
    else if (view->underlay == VIEW_SHEET) { paint_character_sheet(); }
    else { gfall_screen_cover_screen(0,0,0); }
    gfall_screen_blit_ix(g_possession_art[4].x,g_possession_art[4].y,g_possession_art[4].pixels,g_possession_art[4].width,g_possession_art[4].height,0);
    count = spell_rows(view->book,ids,OBJECTS);
    selected = view->selected < count ? view->selected : count-1;
    for (i = 0; i < 14 && i+view->scroll[0] < count; i++) {
        at = i+view->scroll[0];
        if (at == selected) { gfall_screen_fill_box(27,30+i*9,111,9,150); }
        df_copy(line,object_at(ids[at])->name,sizeof(line));
        for (length = 0; line[length]; length++) { }
        while (length && smallfont_width(gfall_screen_font(),line) > 109) { line[--length] = 0; }
        gfall_screen_text_at(28,30+i*9,line);
    }
    spell_scrollbar(&top,&height);
    gfall_screen_fill_box(145,45,7,104,114); gfall_screen_fill_box(145,top,7,height,146);
    if (selected < 0) { return; }
    spell = spell_def(object_at(ids[selected])->index);
    owned = object_at(ids[selected]);
    if (!spell) { return; }
    gfall_screen_text_at(148,20,owned->name);
    log_format(line,sizeof(line),"Cost: %d",magicka_cost(spell_cost(spell,character_state()))); gfall_screen_text_at(155,166,line);
    log_format(line,sizeof(line),"%d/%d",character_state()->magicka,character_state()->max_magicka); gfall_screen_text_at(239,20,line);
    if (g_spell_icons && owned->spell_icon < 69) {
        x = (owned->spell_icon%20)*16; y = (owned->spell_icon/20)*16;
        blit_stride(172,32,g_spell_icons+y*320+x,320,16,16);
    }
    if (g_spell_masks && spell->target < 5 && spell->element < 5) {
        blit_stride(205,32,g_spell_masks+spell->target*640,40,24,16);
        blit_stride(246,32,g_spell_masks+spell->element*640+24,40,16,16);
    }
    for (i = 0; i < 3; i++) {
        if (spell->group[i] != 255) {
            gfall_screen_centered_text(219,63+i*38,spell_effect(spell->group[i],spell->sub[i]),145);
            gfall_screen_centered_text(219,75+i*38,effect_subtype(spell->group[i],spell->sub[i]),145);
        }
    }
}

/* The quests' faces (3.11 of quest-hooks.md): each used slot in order at x = 32 * n + 8, y = 36 of classic's screen, FACE{sex}{bank}I0.CIF's picture (a child's from KIDS00I0.CIF), colour 0 clear; drawn
 * with the world's HUD, so under no menu. The strip is made again when the faces change. */
static int g_face_pic;
static df_u8 g_face_pixels[320*48];
static df_u32 g_face_signature;

static void quest_faces_draw(void)
{
    df_u32 signature;
    int i, n;

    signature = 2166136261u; n = 0;
    for (i = 0; i < QUEST_FACES; i++) {
        const QuestFace *f;

        f = quest_face(i);
        if (f) { signature = (signature ^ (df_u32)(f->face + 1)) * 16777619u; signature = (signature ^ (df_u32)(f->quest_id)) * 16777619u; n++; }
    }
    if (!n || !gfall_screen_have_art_palette()) { return; }
    if (!g_face_pic || signature != g_face_signature) {
        int slot;

        mem_zero(g_face_pixels, sizeof(g_face_pixels));
        slot = 0;
        for (i = 0; i < QUEST_FACES; i++) {
            const QuestFace *f;
            CifSet set;
            const CifFrame *frame;
            char name[16];
            int x, y, record;

            f = quest_face(i);
            if (!f) { continue; }
            if (f->face & 0x10) { log_format(name, sizeof(name), "KIDS00I0.CIF"); record = f->face & 15; }
            else { log_format(name, sizeof(name), "FACE%d%dI0.CIF", (f->face & 0x80) ? 1 : 0, (f->face & 0x40) ? 1 : 0); record = f->face & 15; }
            if (cif_open(name, &set)) {
                frame = cif_frame(&set, record);
                if (frame) {
                    for (y = 0; y < frame->h && y < 48; y++) {
                        for (x = 0; x < frame->w && slot * 32 + 8 + x < 320; x++) { g_face_pixels[y * 320 + slot * 32 + 8 + x] = frame->pixels[y * frame->w + x]; }
                    }
                }
                cif_close(&set);
            }
            slot++;
        }
        if (!g_face_pic) { g_face_pic = pic_load(g_face_pixels, 320, 48, gfall_screen_art_palette(), 1); }
        else { pic_update(g_face_pic, g_face_pixels); }
        g_face_signature = signature;
    }
    if (g_face_pic) { hud_blit_part(g_face_pic, 0, 0, 320, 48, 0, 36, 320, 48); }
}

static int g_effect_pic;
static df_u8 g_effect_pixels[320*64];
static df_u32 g_effect_signature;
void gfall_magic_hud_draw(const MagicState *state)
{
    const Effect *e;
    quest_faces_draw();
    df_u32 signature;
    int i, j, count, duplicate, x, y, px, py, icon, blink, hostile, expiring, columns;
    if (!g_spell_icons) { return; }
    signature = 2166136261u; count = 0;
    blink = (game_state()->ticks/8) & 1;
    for (i = 0; i < EFFECTS; i++) {
        e = &state->effect[i]; if (!e->active || e->target.place || e->target.thing) { continue; }
        signature = (signature ^ (df_u32)(e->spell*257+e->rounds+(e->caster.thing & 255)))*16777619u; count++;
    }
    signature ^= (df_u32)(options_flags() & 1); signature ^= (df_u32)blink;
    if (!count) { return; }
    if (!g_effect_pic || signature != g_effect_signature) {
        mem_zero(g_effect_pixels,sizeof(g_effect_pixels)); count = 0;
        for (i = 0; i < EFFECTS; i++) {
            e = &state->effect[i]; if (!e->active || e->target.place || e->target.thing) { continue; }
            duplicate = 0;
            for (j = 0; j < i; j++) {
                if (state->effect[j].active && !state->effect[j].target.place && !state->effect[j].target.thing &&
                    state->effect[j].spell == e->spell && state->effect[j].caster.thing == e->caster.thing &&
                    state->effect[j].caster.place == e->caster.place) { duplicate = 1; break; }
            }
            if (duplicate || count >= 24) { continue; }
            hostile = e->caster.place || e->caster.thing;
            columns = hostile && (options_flags() & 1) ? 11 : 12;
            /* runspell.c uses D_00185CEC: 16,40,...,280. Hostile
             * spells share their own row above the bar (or at y=177). */
            x = 16+(count%columns)*24; y = hostile ? 48 : count/12*24;
            icon = e->icon >= 200 ? 10 : e->icon; count++;
            /* A lasting spell (a held item's) never blinks. */
            expiring = !e->permanent && e->rounds <= 1;
            for (j = i+1; j < EFFECTS && expiring; j++) {
                if (state->effect[j].active && !state->effect[j].target.place && !state->effect[j].target.thing &&
                    state->effect[j].spell == e->spell && state->effect[j].caster.thing == e->caster.thing &&
                    state->effect[j].caster.place == e->caster.place && state->effect[j].rounds > 1) { expiring = 0; }
            }
            if (icon < 0 || icon >= 69 || (expiring && blink)) { continue; }
            for (py = 0; py < 16; py++) {
                for (px = 0; px < 16; px++) {
                    g_effect_pixels[(y+py)*320+x+px] = g_spell_icons[(icon/20*16+py)*320+icon%20*16+px];
                }
            }
        }
        if (!g_effect_pic) { g_effect_pic = pic_load(g_effect_pixels,320,64,gfall_screen_art_palette(),1); }
        else { pic_update(g_effect_pic,g_effect_pixels); }
        g_effect_signature = signature;
    }
    x = (options_flags() & 1) ? 35 : 0;
    hud_blit_part(g_effect_pic,0,0,320,48,x,16,320,48);
    hud_blit_part(g_effect_pic,0,48,320,16,x,(options_flags() & 1) ? 177 : 132,320,16);
}

static int book_line_width(const SmallFont *font, const char *text)
{
    char glyph[2];
    int width;
    width = 0; glyph[1] = 0;
    for (; *text; text++) {
        glyph[0] = *text;
        width += *text == ' ' ? df_ru16(font->bytes) : smallfont_width(font,glyph);
    }
    return width;
}

static void flush_book_line(char *line, int *used, int *x, int y, int centered, int font, int ink)
{
    char glyph[2];
    int left, i;
    const SmallFont *face;
    face = &g_book_font[font];
    if (!face->bytes) { *used = 0; return; }
    line[*used] = 0;
    left = centered ? 160-book_line_width(face,line)/2 : *x;
    glyph[1] = 0;
    for (i = 0; i < *used; i++) {
        glyph[0] = line[i];
        if (line[i] == ' ') { left += df_ru16(face->bytes); }
        else {
            smallfont_draw(face,gfall_screen_pixels(),320,181,left,y,ink,156,glyph);
            left += smallfont_width(face,glyph);
        }
    }
    *used = 0;
}

static void paint_readable_book(void)
{
    const BookInfo *book;
    const df_u8 *bytes;
    char line[256];
    int at, used, x, y, font, ink, center, token, height;
    book = reading_state(); bytes = book->page;
    gfall_screen_cover_screen(g_possession_art[3].pixels,g_possession_art[3].width,g_possession_art[3].height);
    x = 10; y = 20; font = 4; ink = 145; center = used = 0; height = g_book_font[font].height;
    for (at = 0; at < book->page_bytes && y < 180; ) {
        token = bytes[at++];
        if (token == 246) { break; }
        if (token == 251 && at+2 <= book->page_bytes) { x = df_ru16(bytes+at); at += 2; }
        else if (token == 250 && at < book->page_bytes) { ink = bytes[at++]; }
        else if (token == 249 && at < book->page_bytes) {
            flush_book_line(line,&used,&x,y,center,font,ink);
            font = bytes[at++]; if (font > 4) { font = 4; }
            if (height < g_book_font[font].height) { height = g_book_font[font].height; }
        } else if (token == 253) { center = 1; }
        else if (token == 247) {
            /* Inline picture names are NUL terminated; loading, if supported,
             * stays within this page's checked bound. */
            int start;
            ImgPicture picture;
            start = at;
            while (at < book->page_bytes && bytes[at]) { at++; }
            if (at < book->page_bytes && at-start < 24 && img_load((const char *)bytes+start,&picture)) {
                gfall_screen_blit_ix(x,y,picture.pixels,picture.width,picture.height,1);
            }
            if (at < book->page_bytes) { at++; }
        } else if (!token || token == 1) {
            flush_book_line(line,&used,&x,y,center,font,ink);
            if (!token) { x = 10; y += height; height = g_book_font[font].height; }
            center = 0;
        } else if (token < 246 && used < (int)sizeof(line)-1) { line[used++] = (char)token; }
    }
    if (used) { flush_book_line(line,&used,&x,y,center,font,ink); }
    log_format(line,sizeof(line),"%d/%d",possessions_state()->book_page+1,book->pages);
    gfall_screen_text_at(11,188,line);
}

static void paint_painting(int picture, const char *caption)
{
    CifSet art;
    const CifFrame *frame;
    char name[16], lines[NOTICE_LINES][96];
    int count, i, y;
    gfall_screen_lay_paper(6,1,14,9);
    log_format(name,sizeof(name),"%cPAINT.CIF",'A'+picture/8);
    y = 145;
    if (cif_open(name,&art)) {
        frame = cif_frame(&art,picture%8);
        if (frame) { gfall_screen_blit_ix(160-frame->w/2,50,frame->pixels,frame->w,frame->h,0); y = 50+frame->h+7; }
        cif_close(&art);
    }
    count = notice_lines(caption,0,lines,NOTICE_LINES);
    if (count > NOTICE_LINES) { count = NOTICE_LINES; }
    for (i = 0; i < count && y < 190; i++, y += 7) { gfall_screen_centered_text(160,y,lines[i],146); }
}

/*
 * The talk window (talk_draw, src/lifted/talk.c 282): TALK01I0.IMG is the whole screen, TALK02I0.IMG the four tabs
 * greyed (drawn at 4,26) and TALK03I0.IMG the two lit ways of asking, 107 x 10 each. In "where is" the selected tab is
 * put back from the base picture. The name sits centred at 213,53, the face at 119,65, the tone's button is filled,
 * the list runs from y = 71 in 7-pixel rows, the question is at 125,10 and the log at 190,66 (18 rows).
 */
static void paint_talk(void)
{
    const TalkWin *t;
    ImgPicture base, grey, lit;
    char name[NAME_MAX];
    int i, row, x, y, w;
    const short *b;

    t = talkwin_state();
    gfall_screen_cover_screen(0, 0, 0);
    if (img_load("TALK01I0.IMG", &base)) { gfall_screen_blit_ix(base.x, base.y, base.pixels, base.width, base.height, 0); }
    df_copy(name, g_talk_name, (int)sizeof(name));
    w = smallfont_width(gfall_screen_font(), name);
    smallfont_draw(gfall_screen_font(), gfall_screen_pixels(), CANVAS_W, CANVAS_H, 213 - w / 2, 53, 145, 156, name);
    if (g_talk_face_ok) { gfall_screen_blit_ix(119, 65, g_talk_face, FACE_W, FACE_H, 0); }
    b = talkwin_button(TB_POLITE + t->tone);
    gfall_screen_fill_box(b[0], b[1], b[2] - b[0] + 1, b[3] - b[1] + 1, 244);
    if (img_load("TALK02I0.IMG", &grey)) { gfall_screen_blit_ix(4, 26, grey.pixels, grey.width, grey.height, 0); }
    if (t->mode != 0 && base.width == 320) {
        b = talkwin_button(TB_PLACES + t->tab);
        for (y = b[1]; y <= b[3]; y++) {
            for (x = b[0]; x < b[0] + 107; x++) { gfall_screen_put_px(x, y, base.pixels[y * 320 + x]); }
        }
    }
    if (img_load("TALK03I0.IMG", &lit)) {
        b = talkwin_button(t->mode);
        for (y = b[1]; y <= b[3]; y++) {
            for (x = 0; x < 107; x++) {
                int sy;

                sy = t->mode * 10 + (y - b[1]);
                if (sy < lit.height) { gfall_screen_put_px(b[0] + x, y, lit.pixels[sy * lit.width + x]); }
            }
        }
    }
    /* Clip the scrolling list to the text column, including the glyph shadows. */
    {
        static df_u8 pixels[TW_LIST_WIDTH * TALK_CATEGORIES * 7];
        int rows, height;

        rows = t->mode == 1 && t->tab == 0 && t->categories ? TALK_CATEGORIES : TW_LIST_ROWS;
        height = rows * 7;
        for (y = 0; y < height; y++) {
            mem_copy(pixels + y * TW_LIST_WIDTH, gfall_screen_pixels() + (71 + y) * 320 + 6, TW_LIST_WIDTH);
        }
        for (i = t->top, row = 0; i < t->count && row < rows; i++, row++) {
            TwItem item;
            int ink;

            if (!talkwin_item(i, &item)) { break; }
            if (item.value < 0 && t->mode == 1) { ink = 96; }
            else if ((t->mode == 1 && t->tab == 0 && t->categories ? item.value : i) == t->row) { ink = 244; }
            else { ink = 145; }
            smallfont_draw(gfall_screen_font(), pixels, TW_LIST_WIDTH, height, t->scroll_x, row * 7, ink, 156, item.text);
        }
        for (y = 0; y < height; y++) {
            mem_copy(gfall_screen_pixels() + (71 + y) * 320 + 6, pixels + y * TW_LIST_WIDTH, TW_LIST_WIDTH);
        }
    }
    for (i = 0; i < t->question_count; i++) {
        smallfont_draw(gfall_screen_font(), gfall_screen_pixels(), CANVAS_W, CANVAS_H, 125, 10 + i * 7, 145, 156, t->question[i]);
    }
    for (row = 0; row < TW_ANSWER_ROWS && t->answer_scroll + row < t->answer_count; row++) {
        const char *line;

        i = t->answer_scroll + row;
        line = t->answer[i];
        while (*line == ' ') { line++; }
        smallfont_draw(gfall_screen_font(), gfall_screen_pixels(), CANVAS_W, CANVAS_H, 190, 66 + row * 7, t->asked[i] ? 96 : 145, 156, line);
    }
}

/* A shopkeeper's panel (repair_menu_frame, service_menu_frame): REPR01I0.IMG or GNRC01I0.IMG over the world; the service row's word at 159,70. */
static void paint_service(void)
{
    ImgPicture panel;
    const Possessions *view;

    view = possessions_state();
    gfall_screen_cover_screen(0, 0, 0);
    if (img_load(view->service == SERVICE_REPAIR ? "REPR01I0.IMG" : view->service == SERVICE_TAVERN ? "TVRN00I0.IMG" : view->service == SERVICE_GUILD ? guild_picture(view->service_member) : "GNRC01I0.IMG", &panel)) {
        gfall_screen_blit_ix(panel.x, panel.y, panel.pixels, panel.width, panel.height, 0);
    }
    if (view->service == SERVICE_GUILD) {
        gfall_screen_centered_text(159, 71, view->service_label, 145);
    } else if (view->service != SERVICE_REPAIR && view->service != SERVICE_TAVERN) {
        gfall_screen_centered_text(159, 70, view->service == SERVICE_BANK ? "Banking" : "Sell", 145);
    }
}

/*
 * The potion maker (potionmaker_update, src/hand/func_0008F31F.c; MASK00I0.IMG): the name of what is being made and the gold under the window; the pack's ingredients three
 * across from x 16 and 38 pixels down (the first twelve from the scroll), each drawn at its cell's middle with its name under it; the cauldron's cells two across from x 221.
 */
static void paint_potion_maker(void)
{
    const Possessions *view;
    const Cauldron *c;
    ImgPicture window;
    df_u32 ids[512];
    char line[40];
    int count, i, n, cells;

    view = possessions_state();
    gfall_screen_cover_screen(0,0,0);
    if (img_load("MASK00I0.IMG",&window)) { gfall_screen_blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    gfall_screen_text_at(31,185,"Unnamed potion");
    log_format(line,sizeof(line),"%d",character_state()->gold); gfall_screen_text_at(235,185,line);
    count = ingredient_list(ids,512);
    for (i = view->scroll[0], n = 0; i < count && n < 12; i++, n++) {
        const ObjectInfo *item;
        char name[32];

        item = object_at(ids[i]);
        if (!item) { continue; }
        paint_item_icon(item,n%3*56+28-26,n/3*38+42-18,52,36);
        item_name(item,name,sizeof(name));
        gfall_screen_centered_text(n%3*56+30,n/3*38+58+(n%3 == 1 ? 5 : 0),name,145);
    }
    c = cauldron_state();
    for (i = cells = 0; i < CAULDRON; i++) {
        const ObjectInfo *item;
        char name[32];

        if (!c->item[i]) { continue; }
        item = object_at(c->item[i]);
        if (!item) { continue; }
        paint_item_icon(item,(cells&1)*56+233-26,(cells>>1)*38+42-18,52,36);
        item_name(item,name,sizeof(name));
        gfall_screen_centered_text((cells&1)*56+236,(cells>>1)*40+48,name,145);
        cells++;
    }
}

/* The spell shop (spellshop_update, spellshop_draw_spell): SPBK01I0.IMG, the offered spells in the list, the selected one's icon, element, target and effects, and its price at 97,172. */
/* spellmaker_update (func_00037014): INFO01I0.IMG, each effect's name centred at y 30 + 32 a slot, the points, gold, price and spell points, the name, the chosen target and element and the icon; with a panel open MASK05I0.IMG over it and the effect's numbers in their boxes (func_00038911). */
static void paint_maker(void)
{
    const Maker *maker;
    ImgPicture window, panel;
    char line[64];
    int i, k, type, sub, skill, shown;

    maker = maker_state();
    gfall_screen_cover_screen(0,0,0);
    if (img_load("INFO01I0.IMG",&window)) { gfall_screen_blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    for (i = 0; i < 3; i++) {
        type = maker->spell[i*2]; sub = maker->spell[i*2+1];
        if (type == 255) { continue; }
        if (sub != 255 && effect_sub_name(type,sub)) { log_format(line,sizeof(line),"%s %s",effect_name(type),effect_sub_name(type,sub)); }
        else { log_format(line,sizeof(line),"%s",effect_name(type)); }
        gfall_screen_centered_text(160,30+i*32,line,146);
    }
    log_format(line,sizeof(line),"%d",character_state()->magicka); gfall_screen_text_at(43,149,line);
    log_format(line,sizeof(line),"%d",character_state()->gold); gfall_screen_text_at(40,158,line);
    log_format(line,sizeof(line),"%d",maker_shown()); gfall_screen_text_at(59,167,line);
    log_format(line,sizeof(line),"%d",maker_spell_points()); gfall_screen_text_at(70,176,line);
    gfall_screen_text_at(60,185,(const char *)maker->spell+MAKER_NAME_AT);
    if (g_spell_masks) {
        k = maker->spell[7]; blit_stride(275,114+16*k,g_spell_masks+k*640,40,24,16);
        k = maker->spell[6]; blit_stride(299,114+16*k,g_spell_masks+k*640+24,40,16,16);
    }
    if (g_spell_icons && maker->spell[72] < MAKER_ICONS) {
        k = maker->spell[72]; blit_stride(288,94,g_spell_icons+(k/20)*16*320+(k%20)*16,320,16,16);
    }
    if (!maker->kind) { return; }
    if (img_load("MASK05I0.IMG",&panel)) { gfall_screen_blit_ix(panel.x,panel.y,panel.pixels,panel.width,panel.height,1); }
    skill = character_state()->skill[effect_school(maker->spell[maker->slot*2])];
    shown = (110-skill)*(short)(maker->spell[8+maker->slot*2] | (maker->spell[9+maker->slot*2] << 8))/100;
    log_format(line,sizeof(line),"%d",shown); gfall_screen_text_at(275,119,line);
    {
        static const int left[11] = {64, 104, 160, 64, 104, 160, 64, 104, 144, 184, 240};
        static const int top[11] = {94, 94, 94, 114, 114, 114, 134, 134, 134, 134, 134};

        for (k = 0; k < 11; k++) {
            if ((k < 3 && !(maker->kind & 1)) || (k >= 3 && k < 6 && !(maker->kind & 2)) || (k >= 6 && !(maker->kind & 4))) { continue; }
            gfall_screen_number_at(left[k]+12,top[k]+4,maker_value(k),1,0);
        }
    }
}

/* itemmaker_draw (func_000562A1): ITEM00I0.IMG; the current tab's strip of ITEM01I0.IMG at (175, 9 a tab + 6); the gold, the item's name, the gold cost and the points used of its limit; the item in its box; the left list's four rows; the powers' rows from x 10 and the side effects' from x 108, 7 pixels each, colour 146 and 193 for what a soul gave. */
static void paint_item_maker(void)
{
    const Possessions *view;
    const ItemMaker *maker;
    const ObjectInfo *item;
    ImgPicture window;
    const df_u8 *tabs;
    df_u32 size, ids[OBJECTS];
    ImRow rows[40];
    char line[48];
    int i, k, side, n, count;

    view = possessions_state();
    maker = itemmaker_state();
    gfall_screen_cover_screen(0,0,0);
    if (img_load("ITEM00I0.IMG",&window)) { gfall_screen_blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    tabs = vfs_read("ITEM01I0.IMG",&size);
    if (tabs && size >= 4u*729u && view->im_tab >= 0 && view->im_tab < 4) { blit_stride(175,view->im_tab*9+6,tabs+view->im_tab*729,81,81,9); }
    log_format(line,sizeof(line),"%d",character_state()->gold); gfall_screen_text_at(70,15,line);
    item = maker->item ? object_at(maker->item) : 0;
    if (item) {
        gfall_screen_text_at(51,3,item->name);
        log_format(line,sizeof(line),"%d",itemmaker_gold()); gfall_screen_text_at(63,27,line);
        log_format(line,sizeof(line),"%d/%d",itemmaker_points_used(),itemmaker_capacity(item->id)); gfall_screen_text_at(96,39,line);
        paint_item_icon(item,203,69,46,34);
    }
    count = possessions_im_list(ids,OBJECTS);
    for (i = 0; i < 4 && view->im_scroll+i < count; i++) {
        static const int rowtop[4] = {50, 87, 124, 161};

        item = object_at(ids[view->im_scroll+i]);
        if (item) { paint_item_icon(item,265,rowtop[i]+1,46,33); }
    }
    for (side = 0; side < 2; side++) {
        n = possessions_im_rows(side,rows,40);
        for (k = 0; k < n; k++) {
            if (!rows[k].text[0]) { continue; }
            smallfont_draw(gfall_screen_font(),gfall_screen_pixels(),CANVAS_W,CANVAS_H,side ? 108 : 10,60+k*7,rows[k].soul ? 193 : 146,156,rows[k].text);
        }
    }
}

static void paint_spellshop(void)
{
    const Possessions *view;
    const SpellDef *spell;
    ImgPicture window;
    char line[48];
    int i, at, x, y, length;

    view = possessions_state();
    gfall_screen_cover_screen(0,0,0);
    if (img_load("SPBK01I0.IMG",&window)) { gfall_screen_blit_ix(window.x,window.y,window.pixels,window.width,window.height,0); }
    for (i = 0; i < 14 && i+view->scroll[0] < view->shop_count; i++) {
        at = i+view->scroll[0];
        spell = spell_def(view->shop[at]);
        if (!spell) { continue; }
        if (at == view->selected) { gfall_screen_fill_box(27,30+i*9,111,9,150); }
        df_copy(line,spell->name,sizeof(line));
        for (length = 0; line[length]; length++) { }
        while (length && smallfont_width(gfall_screen_font(),line) > 109) { line[--length] = 0; }
        gfall_screen_text_at(28,30+i*9,line);
    }
    gfall_screen_fill_box(145,45,7,104,114);
    if (view->shop_count > 14) {
        int height, top;

        height = 104*14/view->shop_count; top = 45+(104-height)*view->scroll[0]/(view->shop_count-14);
        gfall_screen_fill_box(145,top,7,height,146);
    }
    spell = spell_def(view->shop[view->selected]);
    if (!spell) { return; }
    gfall_screen_text_at(148,20,spell->name);
    if (g_spell_icons && spell->icon < 69) {
        x = (spell->icon%20)*16; y = (spell->icon/20)*16;
        blit_stride(172,32,g_spell_icons+y*320+x,320,16,16);
    }
    if (g_spell_masks && spell->target < 5 && spell->element < 5) {
        blit_stride(205,32,g_spell_masks+spell->target*640,40,24,16);
        blit_stride(246,32,g_spell_masks+spell->element*640+24,40,16,16);
    }
    for (i = 0; i < 3; i++) {
        if (spell->group[i] != 255) {
            gfall_screen_centered_text(219,63+i*38,spell_effect(spell->group[i],spell->sub[i]),145);
            gfall_screen_centered_text(219,75+i*38,effect_subtype(spell->group[i],spell->sub[i]),145);
        }
    }
    log_format(line,sizeof(line),"%d",spellshop_price(spell,holiday_today(game_state()->minutes,travel_region())));
    gfall_screen_text_at(97,172,line);
}

int gfall_screen_present_possessions(void)
{
    const Possessions *view;
    MemHeap *heap, *was;
    char line[160];
    int i;
    view = possessions_state();
    if (!view->page) { g_possession_page = 0; return 0; }
    g_coin_now = view->page == VIEW_INVENTORY && view->trade == MARKET_BUY ? coin_frame_now() : -1;
    g_enchant_frame = view->page == VIEW_INVENTORY ? enchant_mark_frame() : 0;
    if (g_possession_page == view->page && g_possession_revision == view->revision && g_coin_shown == g_coin_now &&
        (!g_enchant_visible || g_enchant_shown == g_enchant_frame) && play_pic(PIC_SCREEN)) { return 1; }
    g_possession_page = view->page; g_possession_revision = view->revision; g_coin_shown = g_coin_now;
    if (!gfall_screen_prepare_pixels()) { return 1; }
    heap = mem_heap_new();
    if (!heap) { return 1; }
    was = mem_heap_use(heap); g_panel_file_count = 0;
    g_enchant_visible = 0; g_enchant_mark = 0; g_enchant_shown = g_enchant_frame;
    if (view->page == VIEW_INVENTORY) { paint_inventory(); }
    else if (view->page == VIEW_SHEET) { paint_character_sheet(); }
    else if (view->page == VIEW_SPELLS) { paint_spellbook(); }
    else if (view->page == VIEW_MESSAGE) { gfall_screen_cover_screen(0,0,0); }
    else if (view->page == VIEW_BANK) { paint_bank(); }
    else if (view->page == VIEW_TALK) { paint_talk(); }
    else if (view->page == VIEW_SERVICE) { paint_service(); }
    else if (view->page == VIEW_POTION) { paint_potion_maker(); }
    else if (view->page == VIEW_SPELLSHOP) { paint_spellshop(); }
    else if (view->page == VIEW_MAKER) { paint_maker(); }
    else if (view->page == VIEW_ITEMMAKER) { paint_item_maker(); }
    else if (view->page == VIEW_COURT) {
        /* cort01i0.img fills the screen once a court sits; the offer to surrender is over the world. */
        ImgPicture court;
        gfall_screen_cover_screen(0,0,0);
        if (view->court.open && img_load("CORT01I0.IMG",&court)) { gfall_screen_blit_ix(court.x,court.y,court.pixels,court.width,court.height,0); }
    }
    else { paint_readable_book(); }
    if (view->notice[0] && view->painting) { paint_painting(view->painting-1,view->notice); }
    else if (view->notice[0]) {
        char lines[NOTICE_LINES+1][96];
        int count, shown, x, y, cols, rows, ty;
        count = notice_lines(view->notice,view->notice_page*NOTICE_LINES,lines,NOTICE_LINES);
        shown = count-view->notice_page*NOTICE_LINES;
        if (shown > NOTICE_LINES) { shown = NOTICE_LINES; }
        if (count > NOTICE_LINES) {
            log_format(lines[shown++],96,"Page %d/%d  PgUp/PgDn",view->notice_page+1,(count+NOTICE_LINES-1)/NOTICE_LINES);
        }
        gfall_screen_paper_box(lines,shown,0,&x,&y,&cols,&rows,&ty);
        gfall_screen_lay_paper(x,y,cols,rows); gfall_screen_paint_lines(lines,shown,1,x,ty,146);
    }
    if (view->prompt == PROMPT_LIST) {
        /* list_popup_open_strings: the rows in a box, each eight pixels from y 34. */
        int k;

        int shown;

        shown = view->list_count < LIST_SHOWN ? view->list_count : LIST_SHOWN;
        gfall_screen_fill_box(100,26,120,shown*8+14,45); gfall_screen_outline(100,26,120,shown*8+14,145);
        for (k = 0; k < shown; k++) { smallfont_draw(gfall_screen_font(),gfall_screen_pixels(),CANVAS_W,CANVAS_H,106,34+k*8,145,156,view->list[view->list_top+k]); }
    } else if (view->prompt == PROMPT_OFFER || view->prompt == PROMPT_ASK) {
        /* msgbox_yes_no_rsc: the shopkeeper's words over a box, Accept and Reject under them. */
        char wrapped[10][96];
        int rows, k, court;

        /* The court's questions are long: a taller, wider box that still leaves the buttons at 116..134 free. */
        court = view->page == VIEW_COURT;
        rows = wrap_words(view->offer,wrapped,court ? 10 : 6,court ? 208 : 176);
        if (court) { gfall_screen_fill_box(48,8,224,126,45); gfall_screen_outline(48,8,224,126,145); }
        else if (gfall_screen_paper_ready()) {
            /* msgbox_show_string: the words on SPOP's parchment, the Yes and No pictures of BUTTONS.RCI under them (the pointer's regions are the buttons' own). */
            gfall_screen_lay_paper(61,34,9,5);
            gfall_screen_lay_button(3,112,117); gfall_screen_lay_button(4,176,117);
        } else { gfall_screen_fill_box(68,40,184,96,45); gfall_screen_outline(68,40,184,96,145); }
        for (k = 0; k < rows; k++) { gfall_screen_centered_text(160,(court ? 14 : 44)+k*10,wrapped[k],145); }
        if (court || !gfall_screen_paper_ready()) {
            gfall_screen_centered_text(117,121,(view->page == VIEW_COURT || view->page == VIEW_MESSAGE) ? view->yes : "Accept",145);
            gfall_screen_centered_text(203,121,(view->page == VIEW_COURT || view->page == VIEW_MESSAGE) ? view->no : "Reject",145);
        }
    } else if (view->prompt && view->prompt != PROMPT_AMOUNT && view->prompt != PROMPT_LIST) {
        const char *prompt;
        prompt = view->prompt == PROMPT_GOLD ? "How much gold?" : view->prompt == PROMPT_NAME ? "Name thyself" :
                 view->prompt == PROMPT_SPELL_NAME ? "Enter spell name" :
                 view->prompt == PROMPT_ANSWER ? "Your answer:" : view->prompt == PROMPT_NUMBER ? view->offer :
                 "Enter new page:";
        if (gfall_screen_paper_ready()) {
            /* The prompt on the parchment, the typed line under the words, the Yes and No pictures at the bottom. */
            gfall_screen_lay_paper(61,34,9,5);
            gfall_screen_lay_button(3,112,117); gfall_screen_lay_button(4,176,117);
        } else { gfall_screen_fill_box(68,65,184,75,45); gfall_screen_outline(68,65,184,75,145); }
        /* The words are fitted to the parchment's width, a few rows centred where the one line stood. */
        {
            char wrapped[3][96];
            int rows, k, top;

            rows = wrap_words(prompt,wrapped,3,170);
            top = (gfall_screen_paper_ready() ? 56 : 75) - (rows - 1) * 5;
            for (k = 0; k < rows; k++) { gfall_screen_centered_text(160,top + k * 10,wrapped[k],145); }
        }
        log_format(line,sizeof(line),"%s_",view->input); gfall_screen_centered_text(160,gfall_screen_paper_ready() ? 84 : 96,line,145);
        if (!gfall_screen_paper_ready()) { gfall_screen_centered_text(117,121,"Accept",145); gfall_screen_centered_text(203,121,"Reject",145); }
    }
    for (i = 0; i < g_panel_file_count; i++) { texture_forget(g_panel_files[i]); }
    mem_heap_use(was); mem_heap_delete(heap);
    gfall_screen_publish_surface(1,(view->page == VIEW_SPELLS && !view->underlay) || view->page == VIEW_MESSAGE || view->page == VIEW_BANK || view->page == VIEW_TALK || view->page == VIEW_POTION || view->page == VIEW_SPELLSHOP || view->page == VIEW_MAKER || view->page == VIEW_ITEMMAKER || (view->page == VIEW_COURT && !view->court.open));
    return 1;
}

void gfall_screen_talk_name(const char *name)
{
    df_copy(g_talk_name, name, (int)sizeof(g_talk_name));
}

void gfall_screen_talk_face(int archive, int record)
{
    g_talk_face_ok = faces_read(faces_of_flat(archive, record), g_talk_face);
}

void gfall_screen_reset_possessions(void) { g_possession_page = 0; }
