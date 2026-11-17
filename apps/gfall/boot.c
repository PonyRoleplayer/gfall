#include "game/dagger/play/state.h"
#include "game/dagger/tables/buildingtypes.h"
#include "game/dagger/play/combat/dungeon.h"
#include "game/dagger/play/magic/enchant.h"
#include "game/dagger/play/quests/quest.h"
#include "seam.h"
#include "screen/screen.h"
#include "sound.h"

#include "engine/assets/vfs.h"
#include "engine/core/df.h"
#include "engine/core/log.h"
#include "engine/sound/sound.h"
#include "engine/ui/pic.h"

#include "game/dagger/formats/bind.h"
#include "game/dagger/formats/factionfile.h"
#include "game/dagger/places/town.h"
#include "game/dagger/tables/datedefs.h"
#include "game/dagger/play/people/names.h"
#include "game/dagger/play/magic/magicitem.h"
#include "game/dagger/play/people/standing.h"
#include "game/dagger/formats/sndfile.h"
#include "game/dagger/formats/textrsc.h"
#include "game/dagger/places/travel.h"
#include "game/dagger/play/travel/travelmap.h"
#include "game/dagger/tables/maindat.h"

#include "game/dagger/formats/formats.h"
#include "game/dagger/places/place.h"
#include "game/dagger/play/play.h"
#include "game/dagger/tables/tables.h"

#include "buildings/buildings.h"
#include "people/people.h"
#include "quests/hooks.h"
#include "world/world.h"
#include "error.h"

static char g_why[96];
void gfall_error(const char *text)
{
    df_copy(g_why, text, (int)sizeof(g_why));
}

const char *seam_error(void)
{
    return g_why;
}

void gfall_play_sound(int id)
{
    const SoundClip *clip;

    /* FALL.EXE passes DAGGER.SND record IDs, not DFU's directory positions. */
    clip = sndfile_clip(sndfile_by_id((df_u32)id));
    if (clip) {
        sound_play(clip, 1.0f, 0.0f, 0);
    }
}

static void play_sound_at(int id, float x, float y, float z)
{
    const SoundClip *clip;
    clip = sndfile_clip(sndfile_by_id((df_u32)id));
    if (clip) { sound_play_at(clip,x,y,z,128,1024,1,0); }
}

#ifndef DF_HOST
/*
 * Text variants use the session's random stream on the Windows build.
 * Host tests provide this symbol for deterministic rolls.
 */
int combat_roll(int n)
{
    return n > 0 ? random_range(0, n - 1) : 0;
}
#endif

/* The Status key's first box: TEXT.RSC 22 ("You are in %cn. It is %tim on %dat. In the eyes of the law of %crn, you are %ltn.") with the parse.c macros said: the town or the region, the time as %d:%02d, the date, the region and the law's opinion of the player. */
static int seam_status_intro(char *out, int cap)
{
    const char *raw, *town;
    char time[16], date[48], region[32];
    int i, n, minute;

    raw = words_state(22);
    if (!raw || !raw[0]) { return 0; }
    {
        const Faction *province;

        province = faction_province_of(travel_region());
        df_copy(region, province ? province->name : "", (int)sizeof(region));
    }
    town = town_name();
    if (!town || !town[0]) { town = region; }
    minute = (int)(game_state()->minutes % 1440u);
    log_format(time, sizeof(time), minute % 60 < 10 ? "%d:0%d" : "%d:%d", minute / 60, minute % 60);
    date_text(game_state()->minutes, date, sizeof(date));
    n = 0;
    for (i = 0; raw[i] && n < cap - 1; ) {
        const char *with;

        with = 0;
        if (raw[i] == '%') {
            if (raw[i + 1] == 'c' && raw[i + 2] == 'n') { with = town; i += 3; }
            else if (raw[i + 1] == 't' && raw[i + 2] == 'i' && raw[i + 3] == 'm') { with = time; i += 4; }
            else if (raw[i + 1] == 'd' && raw[i + 2] == 'a' && raw[i + 3] == 't') { with = date; i += 4; }
            else if (raw[i + 1] == 'c' && raw[i + 2] == 'r' && raw[i + 3] == 'n') { with = region; i += 4; }
            else if (raw[i + 1] == 'l' && raw[i + 2] == 't' && raw[i + 3] == 'n') { with = legal_title(region_legal(game_state()->region)); i += 4; }
        }
        if (with) {
            int k;

            for (k = 0; with[k] && n < cap - 1; k++) { out[n++] = with[k]; }
        } else {
            out[n++] = raw[i++];
        }
    }
    out[n] = 0;
    return n;
}

/* The world the enchantments' ticks ask about (disease.c player_in_daylight, player_in_temple, the creatures round the player). */
static int ench_daylight(void)
{
    int minute;

    minute = (int)(game_state()->minutes % 1440u);
    return world_where()->where.kind == WHERE_WORLD && minute >= 360 && minute < 1080;
}

static int ench_temple(void)
{
    int type, faction, quality;
    unsigned place;

    return travel_building(&type, &faction, &quality, &place) && type == BUILDING_TEMPLE;
}

static int ench_category_near(int category, int distance)
{
    Camera cam;

    move_camera(&cam);
    return dungeon_category_within(&cam, category, distance);
}

static int ench_hurt_nearest(int amount)
{
    Camera cam;

    move_camera(&cam);
    return dungeon_hurt_nearest(&cam, 128.0f, amount);
}

/* The quests' words: a message of the quest's own QRC (TEXT.RSC's format), or a record of TEXT.RSC itself; shown in a message box. Macros and symbols are the later steps. */
int seam_boot(void)
{
    const char *error;

    g_why[0] = 0;
    gfall_formats_startup();
    gfall_tables_startup();
    gfall_place_startup();
    gfall_play_startup();
    set_sound(gfall_play_sound);
    set_sound_at(play_sound_at);
    gfall_screen_install_draw();
    set_save_capture(seam_capture_world);
    set_words(textrsc_read);
    set_status_intro(seam_status_intro);
    gfall_quests_install_text();
    {
        static const EnchantWorld world = {
            ench_daylight, ench_temple, ench_category_near, ench_hurt_nearest,
            gfall_release_soul, gfall_summon_ally
        };

        enchant_set_world(&world);
    }
    set_word_variants(textrsc_variant);
    if (!maindat_start()) {
        gfall_error(maindat_error());
        return 0;
    }
    dagger_bind_loaders();
    gfall_world_install();
    gfall_quests_install_entities();
    gfall_people_install();
    gfall_building_install();
    gfall_street_install();
    gfall_screen_install_maps();
    gfall_screen_load_classes();
    if (!gfall_people_load_factions()) {
        gfall_error("FACTION.TXT did not read");
        return 0;
    }
    {
        /* NAMEGEN.DAT's pieces name people and shops (people and law, slice 8); without them names are empty. */
        df_u32 names_size;
        const df_u8 *names_bytes;

        names_bytes = vfs_read("NAMEGEN.DAT", &names_size);
        names_set_data(names_bytes, names_bytes ? names_size : 0);
        names_bytes = vfs_read("MAGIC.DEF", &names_size);
        magic_defs_set(names_bytes, names_bytes ? names_size : 0);
    }
    gfall_screen_install_background();
    error = gfall_screen_boot();
    if (error) { gfall_error(error); return 0; }
    return 1;
}
