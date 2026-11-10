#include "game/ogfall/play/magic.h"
#include "game/ogfall/play/ailments.h"
#include "game/ogfall/play/inventory.h"
#include "engine/ui/cmd.h"
#include "engine/core/log.h"
#include "engine/core/df.h"

static df_u32 find(int definition)
{
    const OgObject *spell;
    df_u32 book;
    int i;
    book = og_inventory_book();
    for (i = 0; i < OG_OBJECTS; i++) {
        spell = &og_state()->object[i];
        if (spell->id && spell->type == OG_RECORD_SPELL && spell->parent == book && spell->index == definition) { return spell->id; }
    }
    return 0;
}
static void learn(int argc, char **argv)
{
    int id;
    df_u32 book;
    if (!og_state()->ready || argc != 2 || !cmd_parse_int(argv[1],&id) || !og_spell_def(id)) {
        log_text(LOG_PRINT,"learn <SPELLS.STD id>, for example learn 7 (Wizard's Fire)"); return;
    }
    if (find(id)) { log_text(LOG_PRINT,"That spell is already in your book."); return; }
    book = og_inventory_book();
    if (!book) { book = og_item_create(og_state()->inventory_id,27,0,0); }
    if (!book || !og_spell_create(book,id)) { log_text(LOG_PRINT,"Cannot add the spell."); return; }
    log_print(LOG_PRINT,"Learned %s.",og_spell_def(id)->name);
}
static void cast(int argc, char **argv)
{
    int id;
    if (!og_state()->ready || argc != 2 || !cmd_parse_int(argv[1],&id) || !find(id)) {
        log_text(LOG_PRINT,"cast <known SPELLS.STD id>; use learn to add it first."); return;
    }
    log_text(LOG_PRINT,og_magic_ready(find(id)) ? "Spell readied/cast." : "Cannot cast that spell now.");
}
static void infect(int argc, char **argv)
{
    int id, success;
    if (!og_state()->ready || argc != 2 || !cmd_parse_int(argv[1],&id)) {
        log_text(LOG_PRINT,"poison 128..139 or disease 0..16; original level-one and immunity rules still apply."); return;
    }
    success = argv[0][0] == 'p' ? og_poison_apply(-1,id,1) : og_disease_infect(-1,id,1);
    log_text(LOG_PRINT,success ? "Ailment applied (onset is delayed)." : "Infection refused.");
}
void og_magic_commands(void)
{
    cmd_add("learn",learn,"id","Add a standard spell to OGFALL's owned spellbook.");
    cmd_add("cast",cast,"id","Ready/cast a known standard spell with its normal cost.");
    cmd_add("poison",infect,"128..139","Apply an original poison for testing, skipping the resistance roll.");
    cmd_add("disease",infect,"0..16","Apply an original disease for testing, skipping the resistance roll.");
}
