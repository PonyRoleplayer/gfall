# Starling / Pony97

One original G4-inspired pony, modeled as a faceted 1997 display asset. Teal coat, navy mane and tail, violet eyes and a golden star flank mark. No downloaded models or third-party textures are included.

- 664 indexed positions, 1,068 triangles, 50 palette entries.
- One opaque 16 x 16 colour-swatch atlas: 256 indexed texels on the native viewer. No transparency, bones, animation, shaders or per-frame deformation.
- Y up, muzzle toward +Z; outward counterclockwise triangles. Units are arbitrary; height is about 3.36 units.
- This is a static assembled display mesh with intersecting shells, not a watertight print model or a rigged game character.

## View it

Open `viewer.html` directly in a modern browser. It embeds the complete mesh and needs no network, server, package installation or game data. Drag to orbit, scroll to zoom, Space to pause, R to reset. This preview uses WebGL and is not a Glide compatibility test.

For the Windows 98 / 3dfx viewer, see `../../docs/pony97.md`. Its checked-in C data is embedded in PONY97.EXE when you build it; the executable needs the installed Glide 2 driver, not Daggerfall data. No executable has been built as part of this task.

`preview.png`, `preview-other.png` and `preview-side.png` are software renders of the actual mesh, not concept art or hardware screenshots.

## Edit or regenerate

Import `starling.obj` with adjacent `starling.mtl` into a modeller. Each material is one of the same palette swatches used by the native viewer; UVs also point to the corresponding centres in `palette.tga` if you consolidate materials to a single atlas. The multi-material OBJ is an interchange convenience; the native mesh uses only one texture slot.

Run `python3 assets/pony97/generate.py` from the repository to regenerate OBJ, MTL, TGA, JSON, the native model header, and the embedded browser viewer. This authors assets only, not executable code. Source of truth: `generate.py`; hand edits to generated files will be overwritten.

Optionally run `python3 assets/pony97/preview.py` to regenerate the three PNG renders. That workstation script needs NumPy and Pillow; neither is needed to build or run PONY97.EXE.

The triangle count and tiny texture are conservative for one displayed model, but no target performance or Glide runtime result has been measured. Per-face palette shading is static so rotation does not move the light.
