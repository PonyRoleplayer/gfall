# Pictures on the screen

A game's 2D pictures, such as Daggerfall's IMG and CIF screens, the weapon's swing, the compass and the vitals, drawn into a screen rectangle. Read this before editing `src/engine/ui/pic.c`, the picture calls in `gfx.c` (`pictures_prepare`, `g_pic_ops`), `glide_tex_take`, `glide_pic_palette` or `glide_level_palette`.

## The calls

- `pic_load(pixels, w, h, rgb, cutout)` returns an id, or 0 when nothing is free. `pixels` is `w × h` palette indices (each side 1..1024), row 0 at the top. `rgb` is the picture's own 768-byte palette. The engine copies both, so the game may free its own.
- `pic_free(id)` and `pic_size(id, &w, &h)`.
- `pic_draw(id, x, top, w, h)` draws the whole picture into a rectangle `w × h` whose top left is `x, top`.
- `pic_draw_part(id, sx, sy, sw, sh, x, top, w, h, alpha)` draws a part of it, at an opacity of 0..255: a frame of a sheet, a strip of the compass, a fade.
- `pic_stats` gives the pictures loaded, the tiles on the card, their bytes and the room.

Coordinates are `ui_string`'s: screen pixels from the lower left. A Daggerfall screen of 320 × 200 is drawn at 2×, for example `pic_draw(id, 0, 440, 640, 400)`.

Draw from `gfx_set_under` (under the HUD's text) or `gfx_set_extra` (over it). The order of the calls is the order on the screen.

## On the card

- **Opaque pictures** go on the card as P_8, 1 byte a texel, read through their own palette.
  - `pic.c` keeps up to 16 palettes. Pictures with the same colours share one slot.
  - `glide_pic_palette` sends a slot's palette only when another is on TMU0, so a run of pictures under one palette costs one download.
  - `draw_frame` puts the level's palette back once, after `g_extra`, and only if a picture replaced it (`glide_level_palette`).
  - The sky and the window glow track the palette the same way, so a picture after them sends its own again.
- **Cutout pictures** go as ARGB 1555, 2 bytes a texel. Index 0 is clear (0x0000), and the rest have the alpha bit set. They are alpha-tested as sprites and text are. They need no palette.
- **The draw state** is `glide_ui`: point sampled, over the frame with no depth test or write, tint white. An opacity under 255 blends with `glide_opacity`.

### Tiles

The card takes only squares of a power of two, 256 at most (AGENTS.md), so a picture is cut into tiles:
- A region is covered by a grid of the largest power of two that fits its shorter side (256 at most).
- The strip to the right of that grid and the strip under it are covered the same way, in turn.
- A strip thinner than 16 is covered by 16 × 16 tiles, padded past the picture's edge. A cutout's padding is clear, and an opaque one's repeats the edge.

A 320 × 200 screen becomes 29 tiles: 2 of 128, 7 of 64 and 20 of 16 for its last 8 rows. As P_8 that is 66560 bytes (64000 texels), not the 128 KB of one padded 256 square or 160 KB at 16 bits.

Each tile is a quad of its own. Every quad edge is computed from an integer pixel by the same sum, so tiles that meet share their screen edge exactly: there are no gaps or overlaps under any scale. Bilinear filtering would show seams at tile edges, which is why the draw is point sampled.

### The room

`pic_set_room(kb)` sets the card room for pictures, 0 (the default) to 1024 KB. The viewers draw no pictures and leave it at 0, so the place gets all the card.
- **Where it is taken:** at each `gfx_prepare`, after the sky, the light falloff and the font, and before the place's own pictures. Both the whole-place and stream paths do this (`pictures_prepare`, `glide_tex_take`). So the place's budget, or the stream's residency, sees exactly what is left, as it does for the font.
- **Its limits:** the room never crosses a 2 MB edge. A change of size takes effect at the next place.
- **Inside the room:** each tile has its own space, in a first-fit list of free extents. Extents are 8-byte aligned and joined when they meet. Tiles go up with `glide_upload_chain` and leave with `glide_release`.
- **When it is full:** a picture whose tiles do not all fit is kept in memory but not drawn at all, never drawn with a hole, and one warning is logged. It goes on the card at the next place, when the room may be bigger.

### Across places

`pic.c` keeps every tile's texels, in card format, in a heap of its own (`mem_heap_new`), not a place's heap. Freeing a picture frees them. Every `gfx_prepare` empties the card (`glide_tex_reset`), and `pic_begin` then sends every loaded picture again into the new room. A picture loaded before any place waits for the first one. `gfx_shutdown` calls `pic_end`.

The RAM cost is the card cost: 65 KB for a full screen as P_8.

## The 233

- **Uploading:** a full screen is 29 downloads, about 66 KB over PCI, a millisecond or two. Load a screen when it opens, not every frame.
- **Drawing:** a screen is 29 textured quads with a bind each. A weapon frame is a handful. The fill is the Voodoo's: a 640 × 400 screen is 256000 pixels, about a third of the frame's fill at 640 × 480.
- **Budget:** the game sets the room from what its screens and the HUD need at once. Every KB of it comes out of the place's pictures.

## Tests

`check_pictures` (engine) runs on a fake card:
- a 320 × 200 screen in its 29 tiles and 66560 bytes, covering exactly 640 × 400 when drawn at 2×, with every texel coordinate inside 0..256;
- a part landing on its rectangle;
- two pictures under one palette sharing a slot;
- a cutout drawn with no palette, its clear texel 0;
- freed pictures leaving nothing on the card;
- a picture short of room not drawn, then drawn once the next place's room is bigger;
- freed tiles joined so that one 64 KB tile fits where a screen's 29 were.

## Not yet

- A console line or LEVELFALL panel for `pic_stats`.
- Colour tinting of a picture (the HUD's vitals may want it). Today the tint is white.
- Daggerfall's readers for IMG and CIF are the game's (`formats/`). The engine takes plain indices and a palette.
