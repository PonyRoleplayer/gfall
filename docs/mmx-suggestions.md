# MMX suggestions

These are ideas for later, not work in progress.

MMX is integer only. The float vertex path is 3DNow and is not in the table below.

A software rasterizer is rejected because the Voodoo already fills triangles in parallel with the CPU.

| Idea | Where it runs | Why MMX fits | What you would feel | Worth it |
|---|---|---|---|---|
| Mix 4 samples at once | add_mmx in mixer.c | Four independent 8-bit samples, scale, saturating add | Already in. 16 voices stay cheap next to the frame | Done |
| Clear the mix buffer | The silence before that mix | A wide fill of 16-bit zeros | A little CPU back, enough for a few more voices of the same clips | Some |
| Resample 4 voices side by side | The pitched path, still one sample at a time in add_frame | Four voices do not depend on each other | Footsteps and swings at the right pitch without a hitch | Some |
| Muffle 4 voices side by side | A low-pass in front of the mixer | One voice is a chain. Four voices are four chains | Sounds behind a door go dull. The filter itself is not the MMX win | Some |
| 2x2 mip average | mip.c, every coarser level | Add four texels and shift, on a whole row | Shorter hitches when a picture is rebuilt or a sharper mip is sent | Great |
| Bilinear wall rebuild | texture_smooth | Blend four neighbors in red, green, and blue, then search the palette | The climate and season rebuilds drop from a stall toward a frame or two. The palette search stays scalar. Pentium MMX has no psadbw | Great |
| Pack a sprite to 1555 | The cutout path in mip.c | Widen indexes, test clear texels, pack to 16-bit | Faster uploads of creatures and flats. The bus to the Voodoo can still be the limit | Some |
| Far-ground 8x8 shrink | The horizon bake | Pure average of color blocks, no palette search in the inner step | The stream catches up while you walk toward a new city | Great |
| Average heightmap bytes | Coarser terrain from WOODS.WLD | The heights are bytes. A row average is adds | Faster far terrain. The mesh after that is still float | Some |
| Copy the long uncompressed texture rows | RLE decode, the rows that are already raw | A wide copy. rep movsl is already strong on this chip | Almost nothing unless a profile says the copy is hot | Little |
| Fixed-point box tests, four at a time | The cull of many models | Four independent boxes | Only if culling shows up in prof. The bigger win already named is transforming each corner once | Little, until measured |
| Screenshot expand, index to RGB | shot.c, F12 | Unpack and scale colors | A faster screenshot. Not the frame rate | Little |
| Software triangle fill | Anything that draws pixels the card should draw | MMX can blend pixels. The Voodoo already does it in parallel with the CPU | The frame gets slower | Do not |
