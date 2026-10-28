/*
 * The host has no card. The game's code puts pictures on it through
 * texcache, which talks to Glide and is not built for the host: these
 * stand in, for hostcheck and for dfrec, which link the game's code.
 */
struct Image;

int texcache_stream_set(int slot, const struct Image *img, int cutout, float density)
{
    (void)slot;
    (void)img;
    (void)cutout;
    (void)density;
    return 0;
}

void texcache_stream_drop(int slot)
{
    (void)slot;
}

/* No card on the host: a place's late pictures (a creature's) are only counted. */
static int g_resident_adds;

int texcache_resident_add(int slot, const struct Image *img, int cutout, float density)
{
    (void)slot;
    (void)cutout;
    (void)density;
    g_resident_adds += img != 0;
    return img != 0;
}
