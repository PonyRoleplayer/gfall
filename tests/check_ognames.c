#include <string.h>
#include "check_ognames.h"
#include "check_util.h"
#include "engine/assets/vfs.h"
#include "game/ogfall/play/names.h"
#include "game/ogfall/play/state.h"

static int has_space(const char *s)
{
    return strchr(s, ' ') != 0;
}

void check_ognames(void)
{
    char a[OG_NAME_MAX], b[OG_NAME_MAX];
    OgNpcName npc;
    OgBuildingName bld;
    df_u32 size;
    const df_u8 *bytes;
    int i, bank, spaces, ok, distinct;
    df_u32 before;

    bytes = vfs_read("NAMEGEN.DAT", &size);
    check_expect(bytes != 0, "names: NAMEGEN.DAT does not read");
    if (!bytes) {
        return;
    }
    og_names_set_data(bytes, size);

    /* A name is the same for a seed, differs between seeds, and leaves the session's stream where it was. */
    og_random_seed(4242);
    before = og_state()->random;
    og_name_seeded(0, 0, 1000, a, sizeof(a));
    check_expect(og_state()->random != before, "names: seeding a name did not draw the stream's saved value");
    og_random_seed(4242);
    og_name_seeded(0, 0, 1000, b, sizeof(b));
    check_expect(!strcmp(a, b) && a[0], "names: a seed did not give the same name twice");
    distinct = 0;
    for (i = 0; i < 40; i++) {
        og_name_seeded(0, 0, 2000 + i, b, sizeof(b));
        distinct += strcmp(a, b) != 0;
    }
    check_expect(distinct > 30, "names: seeds gave the same name too often");
    /* name_generate_seeded keeps rand()'s result, seeds, then seeds again with the kept value: the stream goes on from it. */
    og_random_seed(7);
    i = og_random_range(0, 32767);
    og_random_seed(7);
    og_name_seeded(0, 1, 5, a, sizeof(a));
    check_expect(og_state()->random == (df_u32)i, "names: the stream was not put back after a seeded name");

    /* Banks: 1, 8, 9 and 10 have a given name alone; Nords (2) and the rest have a surname; Nord surnames end in "sen". */
    ok = 1;
    for (bank = 0; bank < 11; bank++) {
        spaces = 0;
        for (i = 0; i < 60; i++) {
            og_name_seeded(bank, i & 1, 9000 + i, a, sizeof(a));
            spaces += has_space(a);
            ok = ok && a[0] && strlen(a) < 41;
            if (bank == 2 && !(strlen(a) > 3 && !strcmp(a + strlen(a) - 3, "sen"))) { ok = 0; }
        }
        if (bank == 1 || bank == 8 || bank == 9) {
            ok = ok && spaces == 0;
        } else if (bank != 10) {
            ok = ok && spaces == 60;
        }
    }
    check_expect(ok, "names: a bank gave an empty, long, or wrongly shaped name");

    /* npc_display_name: no name for a thing; an individual faction's own name; a generated one by the region's bank and the person's sex. */
    memset(&npc, 0, sizeof(npc));
    npc.type = 12;
    check_expect(!og_npc_name(&npc, a, sizeof(a)) && !a[0], "names: a thing had a name");
    npc.type = 8; npc.region = 17; npc.id = 31337u; npc.female = 0;
    check_expect(og_npc_name(&npc, a, sizeof(a)) && a[0] && og_region_bank(17) == 0, "names: a person had no name");
    og_npc_name(&npc, b, sizeof(b));
    check_expect(!strcmp(a, b), "names: a person's name changed between looks");
    npc.faction = 7; npc.faction_is_individual = 1; npc.faction_name = "Lord Brisienna";
    og_npc_name(&npc, a, sizeof(a));
    check_expect(!strcmp(a, "Lord Brisienna"), "names: an individual's faction did not name them");

    /* Signs: the seed fixes a shop's name; houses, banks, palaces and walls are the original's own words. */
    memset(&bld, 0, sizeof(bld));
    bld.type = 15; bld.name_seed = 321;
    og_building_name(&bld, a, sizeof(a));
    og_building_name(&bld, b, sizeof(b));
    check_expect(a[0] && !strcmp(a, b), "names: a tavern's sign changed between looks");
    bld.type = 3; bld.region_name = "Sentinel";
    og_building_name(&bld, a, sizeof(a));
    check_expect(!strcmp(a, "The Bank of Sentinel"), "names: a bank's sign");
    bld.type = 16; og_building_name(&bld, a, sizeof(a));
    check_expect(!strcmp(a, "Palace"), "names: the palace's sign");
    bld.type = 1; og_building_name(&bld, a, sizeof(a));
    check_expect(!strcmp(a, "Your house.") || !strcmp(a, "House for sale"), "names: a house's sign");
    bld.for_sale = 1; og_building_name(&bld, a, sizeof(a));
    check_expect(!strcmp(a, "House for sale"), "names: a house for sale's sign");
    bld.type = 20; bld.resident = "Anorak"; og_building_name(&bld, a, sizeof(a));
    check_expect(!strcmp(a, "The Anorak Residence"), "names: a quest residence");
    bld.resident = 0; og_building_name(&bld, a, sizeof(a));
    check_expect(!strcmp(a, "Residence"), "names: a plain residence");
    bld.type = 20; bld.faction = 108; bld.brotherhood = 1; og_building_name(&bld, a, sizeof(a));
    check_expect(!strcmp(a, "The Dark Brotherhood"), "names: the Brotherhood's hall");
}
