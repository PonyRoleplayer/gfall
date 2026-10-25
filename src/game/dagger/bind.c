#include "game/dagger/bind.h"
#include "engine/assets/loader.h"
#include "game/dagger/arch3d.h"
#include "game/dagger/palette.h"
#include "game/dagger/texture.h"
#include "game/dagger/hmi.h"
#include "game/dagger/sndfile.h"
#include "engine/assets/song.h"
#include "engine/sound/music.h"

/*
 * The formats this game knows how to turn into engine objects.
 * Registered once, then world and block loading call them by name.
 */

static int load_col(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    (void)name;
    return palette_load(bytes, size, (Palette *)out);
}

static int load_arch(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    ArchLoad *load;

    (void)name;
    load = (ArchLoad *)out;
    if (!load) {
        return 0;
    }
    load->mesh = arch3d_load(bytes, size, load->id);
    return load->mesh != 0;
}

static int load_texture(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    TexLoad *load;

    (void)name;
    (void)bytes;
    (void)size;
    load = (TexLoad *)out;
    if (!load) {
        return 0;
    }
    load->out = texture_load_frame(load->file, load->image, load->frame);
    return load->out != 0;
}

/* A song of MIDI.BSA by name; the FM tracks when the music plays on FM. out: Song. */
static int load_song(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    int fm;

    fm = music_kind() == MUSIC_FM;
    if (!out) {
        return 0;
    }
    if (bytes) {
        return hmi_parse(bytes, size, fm, (Song *)out);
    }
    return hmi_load(name, fm, (Song *)out);
}

/* A clip of DAGGER.SND by its number, "389". out: const SoundClip *. */
static int load_sound(const char *name, const df_u8 *bytes, df_u32 size, void *out)
{
    int n;
    int i;

    (void)bytes;
    (void)size;
    if (!out || !name || !name[0]) {
        return 0;
    }
    n = 0;
    for (i = 0; name[i]; i++) {
        if (name[i] < '0' || name[i] > '9' || n > 100000) {
            return 0;
        }
        n = n * 10 + (name[i] - '0');
    }
    *(const SoundClip **)out = sndfile_clip(n);
    return *(const SoundClip **)out != 0;
}

static int g_bound;

void dagger_bind_loaders(void)
{
    if (g_bound) {
        return;
    }
    loader_register("col", load_col);
    loader_register("arch3d", load_arch);
    loader_register("texture", load_texture);
    loader_register("song", load_song);
    loader_register("sound", load_sound);
    g_bound = 1;
}
