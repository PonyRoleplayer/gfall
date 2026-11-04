# MMX suggestions

These are ideas for later alongside the implemented mixer, optional mip averaging, cutout packing and indexed far-ground box sums. Their parity checks run in hostcheck against independent byte oracles and pass on an MMX host. Their speed on the target is unmeasured: they build mip levels at load and when the stream brings a picture in, so time them by the load line and the stream's worst step with each switch off and on (`docs/profile.md`, `docs/streaming.md`). `pe_check` refuses any instruction a Pentium MMX lacks (`docs/glide.md`).

MMX is integer only. The float vertex path is 3DNow and is not in the table below.

A software rasterizer is rejected because the Voodoo already fills triangles in parallel with the CPU.

| Idea | Where it runs | Why MMX fits | What you would feel | Worth it |
|---|---|---|---|---|
| Mix 4 samples at once | add_mmx in mixer.c | Four independent 8-bit samples, scale, saturating add | Already in. 16 voices stay cheap next to the frame | Done |
| Clear the mix buffer | The silence before that mix | A wide fill of 16-bit zeros | A little CPU back, enough for a few more voices of the same clips | Some |
| Resample 4 voices side by side | The pitched path, still one sample at a time in add_frame | Four voices do not depend on each other | Footsteps and swings at the right pitch without a hitch | Some |
| Muffle 4 voices side by side | A low-pass in front of the mixer | One voice is a chain. Four voices are four chains | Sounds behind a door go dull. The filter itself is not the MMX win | Some |
| 2x2 mip average | mip.c, every coarser level | Two RGBA outputs per iteration; widen, sum four, add 2, shift once | Optional runtime dispatch, `mipmmx 0` for scalar; speedup unmeasured (`docs/glide.md`) | Implemented; parity checked on the host, speed unmeasured |
| Bilinear wall rebuild | texture_smooth | Blend four neighbors in red, green, and blue, then search the palette | The climate and season rebuilds drop from a stall toward a frame or two. The palette search stays scalar. Pentium MMX has no psadbw | Great |
| Pack a sprite to 1555 | mip.c emit cutout path; full-level palette shortcut retained | Four RGBA texels per batch iteration; shift/mask colour and alpha, sign-extend then PACKSSDW | Optional runtime dispatch, `mippackmmx 0` for scalar; speedup unmeasured (`docs/glide.md`) | Implemented; parity checked on the host, speed unmeasured |
| Far-ground 8x8 shrink | tile_small through mip_box_rgb; existing cache retained | Indexed palette gather/add in RGB word lanes, widened per row; exact truncation | `mipboxmmx 0` for scalar; lookup/search stay scalar, speedup unmeasured (`docs/streaming.md`) | Implemented; parity checked on the host, speed unmeasured |
| Average heightmap bytes | Coarser terrain from WOODS.WLD | The heights are bytes. A row average is adds | Faster far terrain. The mesh after that is still float | Some |
| Copy the long uncompressed texture rows | RLE decode, the rows that are already raw | A wide copy. rep movsl is already strong on this chip | Almost nothing unless a profile says the copy is hot | Little |
| Fixed-point box tests, four at a time | The cull of many models | Four independent boxes | Only if culling shows up in prof. The bigger win already named is transforming each corner once | Little, until measured |
| Screenshot expand, index to RGB | shot.c, F12 | Unpack and scale colors | A faster screenshot. Not the frame rate | Little |
| Software triangle fill | Anything that draws pixels the card should draw | MMX can blend pixels. The Voodoo already does it in parallel with the CPU | The frame gets slower | Do not |
