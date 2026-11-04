# Pentium MMX 233 engine performance audit

Source snapshot: `main`, `a066e04`, inspected 2026-11-01. Target: P55C Pentium MMX 233, 64 MB RAM, Voodoo 1 through Glide 2, AWE64, Windows 98.

This is a source audit and a prioritized suggestions list. **No build, compiler, test, benchmark, implementation change, commit, merge or push was performed.** The new MMX kernels and their authored tests remain unvalidated as recorded in the existing documentation. Every proposed speedup below is unmeasured. Confidence describes the evidence for redundant work or a possible bottleneck, not a promised frame-rate increase. Links name the file; function names are the durable references, since line numbers move.

> **Since this snapshot** (`f024e74`, 2026-11-03): the three texture MMX kernels' parity checks run in hostcheck and pass on an MMX host; the far ground's rectangles are one MMX batch with one `EMMS` per tile (item 11's per-rectangle overhead); `g_rgba`, `g_box_pal` and the two MOVQ constant tables are eight-byte aligned; and `pe_check` refuses every post-P55C instruction named under "Tempting approaches", item 4. Their speed on the 233 is still unmeasured, so item 0 stands first.

The best next work is to establish a target baseline, remove repeated per-face mathematics, then reduce repeated geometry and query work. More MMX is a secondary choice. Streaming stalls deserve their own ranking: reducing a worst-case work unit can improve play more than making a warm inner loop faster.

## What the P55C changes about the analysis

- This processor has separate 16 KB instruction/data caches, four-way associativity and 32-byte cache lines. A larger scratch structure can trade fewer calculations for more cache traffic. Keep hot working sets compact; 64 MB system RAM does not make the L1 large. These are P55C properties, not the earlier Pentium's 8 KB caches. [Intel Pentium Processor Family Developer's Manual, §§2.5–2.5.1](https://www.ardent-tool.com/CPU/docs/Intel/Pentium/241428-005.pdf#page=46).
- P55C scheduling is not P6 scheduling. Address-generation dependencies and U/V pairing matter. In particular, immediately using a newly written register as an address can stall; packing/unpacking/shift instructions also compete for the MMX shifter. Extra instructions do not automatically run in the second pipe. P6 partial-register and micro-op advice must not be applied indiscriminately. [Intel MMX Developer's Manual, chapter 3, §§3.2.1 and 3.3.3](https://datasheets.chipdb.org/Intel/x86/MMX/MMX/DG_CHP3.HTM).
- MMX operates on packed integers and aliases the x87 register file. Use bounded integer batches, empty the MMX state before x87 resumes, and preserve compiler-visible clobbers. `EMMS` is not a way to preserve live floating-point values. [Intel MMX Developer's Manual, §§4.3–4.5](https://datasheets.chipdb.org/Intel/x86/MMX/MMX/DG_CHP4.HTM).
- Geometry remains x87 work unless an explicitly bounded fixed-point representation is justified. Original MMX provides neither SIMD floating point nor the later `PAVGB`, `PSADBW`, `PSHUFW` and SSE-era conveniences. SSE, P6 conditional moves, 3DNow and non-temporal-store strategies are outside this target. The 1997 manual's original instruction inventory is the baseline, not an x86-64 host's instruction set. [Intel manual, §2.4.2](https://www.ardent-tool.com/CPU/docs/Intel/Pentium/241428-005.pdf#page=42).

No instruction-latency number in older repository comments is treated here as an end-to-end measurement. In particular, host time multiplied by a guessed factor is not a target result.

## Existing optimizations to preserve

These are already implemented and are not new suggestions:

- Whole-face polygon submission, exact-fan fallback, planar Glide submission, shared-edge clipping rules and 1/16-pixel snapping.
- Baked model bounds, chunk spans, conservative occlusion cells, sprite occlusion and near/far ground selection.
- Cached texture handles and redundant-bind suppression; cached tint and palette selection; cached image scales and mesh texture-slot keys.
- Render-only x87 precision scope and explicit fast integer-conversion helpers. Do not extend the render precision scope into physics or world-coordinate doubles.
- Budgeted residency with permanent tiny copies, retained mip chains, sprites-first sharpening, fragmentation-aware allocation and bounded wall stealing.
- Chunk loading state machines, texture file pieces, separate decode/finish stages, light cutting at face boundaries and incremental sky changes.
- Swept-body collision with a model grid or streamed spans; retained work buffers; scheduled distant game actors; sound wall-ray budgets.
- Runtime-guarded MMX audio mixing, mip halving, cutout packing and indexed ground-box sums, each with a scalar alternative. `cutout_top` already bypasses an RGBA conversion for a full cutout level alone. The ground archive/record cache already survives stream reloads.
- Word-oriented memory routines, cached CPU detection, sparse kept-world changes and binary-search lookup in `keep.c`.

Evidence: [draw.c](../src/engine/gfx/draw.c), [glide.c](../src/engine/gfx/glide.c), [scene.c](../src/engine/scene/scene.c), [texcache.c](../src/engine/gfx/texcache.c), [texres.c](../src/engine/gfx/texres.c), [mip.c](../src/engine/gfx/mip.c), [keep.c](../src/engine/scene/keep.c). Some older docs still describe features as absent; the current source takes precedence.

## Ranked suggestions

### 0. Establish trustworthy target attribution before choosing the next kernel

**Benefit:** makes frame, hitch and load decisions defensible. **Confidence:** high that current measurements are insufficient to rank actual elapsed costs. **MMX fit:** none.

**Evidence.** [prof.c: `cycles`, `prof_begin`, `prof_frame`](../src/engine/core/prof.c) provides useful coarse TSC stages. [draw_frame](../src/engine/gfx/draw.c) brackets simulation, sound, culling, model submission and other passes. [docs/profile.md](../docs/profile.md) correctly says stage time includes possible FIFO-full waits, then overstates that a large `models` stage establishes CPU-bound geometry. It does not: Glide/driver/card pressure can stall submission inside that stage.

**Suggested next action.** Capture repeatable views with panels hidden, then panels shown separately, under vsync off/on. Compare pass-disable experiments, different visible geometry, and supported resolution changes if available. Keep camera, FOV, weather, population and asset residency fixed. Record overall frame distributions, submission counts, transformed vertices, clip events, uploads and loader-step maxima. Separate cold reads, warm reads, steady movement and a forced stream overrun. Isolate audio mixing from sound rays and gains. Do not infer GPU time by simply subtracting one coarse stage from another.

**Cost/risk.** Small optional counters and a bounded sample ring; avoid logging inside the measured loops. No CPUID serialization around every face or sample. The low 32-bit TSC wraps in roughly 18 seconds at 233 MHz; modular short-frame differences are valid, but very long loads/stalls need a separate 64-bit or wall-clock measurement. Coarse brackets are not instruction latency measurements. Windows scheduling and disk activity also enter elapsed time.

**Acceptance.** Later, when execution is authorized: repeat runs on the actual P55C/board, publish variation and tail latency as well as means, and show an end-to-end improvement beyond noise. Keep these measurements separate from host correctness tests. All later acceptance checks in this report are future work, not checks executed during this audit.

### 1. Move back-face tests into the mesh's frame once per instance

**Benefit:** steady-frame CPU geometry work; potentially also occlusion preparation. **Confidence:** high redundant mathematics, unmeasured elapsed benefit. **Feasibility:** medium, small storage cost. **MMX fit:** poor; this is an algebraic reduction in x87 work.

**Evidence.** [scene.c: `instance_faces`](../src/engine/scene/scene.c) transforms a face normal into world/view space, transforms a point on the face into world/view space, then takes their dot product. [draw_frame's face loop](../src/engine/gfx/draw.c) invokes it repeatedly for one instance. Current instance matrices are constructed from rotations by `set_axes`/`instance_set_rotation`.

**Suggestion.** Compute the camera in local instance coordinates once, then test each local face normal against the local vector from camera to the face. Alternatively precompute the local plane constant when geometry is finalized. This also avoids camera-space work for faces that will be rejected before drawing. Keep the existing handling of degenerate normals, lone triangles and two-sided geometry.

**Cost/risk.** A few floats per current instance and optionally one plane constant per face. The transpose shortcut requires the rotation/orthonormal invariant; use a correct inverse or a fallback if scaling or other transforms are introduced. Near-edge signs can change through reassociation, so preserve a conservative boundary policy.

**Measure/accept.** Count facing tests and their rejected faces. Compare orientations throughout the hold, town and rotating/swinging doors, including edge-on and degenerate faces. No newly missing wall or inconsistent occlusion is acceptable; lower model-stage time must survive a full-frame comparison.

### 2. Compose instance-to-view transforms once per visible instance

**Benefit:** steady frames with visible model geometry. **Confidence:** high repeated two-stage transform. **Feasibility:** medium; bounded scratch, low memory cost. **MMX fit:** none required.

**Evidence.** [draw.c: `view_point`](../src/engine/gfx/draw.c) is already inlined, but each point still applies the instance matrix and translation, then yaw/pitch view arithmetic. [draw_face](../src/engine/gfx/draw.c) repeats this for every accepted face corner; `draw_tris` repeats it for fallback geometry.

**Suggestion.** Compose a view matrix/translation per instance and use one affine transform per point. Share that preparation with item 1 where practical. Benchmark a scalar C version before hand-written x87. Do not confuse this with the already implemented inlining of `view_point` or `project_vert`.

**Cost/risk.** Roughly a dozen floats of current-instance state, not a scene-wide transformed copy. Composition adds setup work, so tiny meshes need measurement. Different association changes floating-point intermediates; preserve the current control-word scope, clipping inequalities and snap behavior.

**Measure/accept.** Compare point count, setup count and elapsed geometry work for large and tiny models. Check seam continuity, movers, near-plane transitions, FOV extremes and the triangle fallback. Do not claim an arithmetic-operation reduction is the same percentage frame-rate gain.

### 3. Add indexed positions and a bounded lazy transform cache

**Benefit:** recurring model and possibly occlusion/window-pass CPU work; reduced duplicate source traffic. **Confidence:** high structural duplication, medium practical payoff after storage costs. **Feasibility:** substantial change. **MMX fit:** poor; the main win is avoiding calculations.

**Evidence.** [Mesh/Tri layout](../src/engine/assets/mesh.h) stores positions and UVs per triangle. Whole-face drawing avoids repeated fan corners within one face, but [draw_face](../src/engine/gfx/draw.c) still transforms shared positions separately across faces. [draw_windows](../src/engine/gfx/draw.c) can revisit geometry. The historical repetition count in `docs/glide.md` is motivation, not a newly measured count for this snapshot.

**Suggestion.** Give finalized/welded mesh positions stable indices, retain UVs at face corners for seams, and lazily cache view positions only when a surviving face needs them. Start with bounded scratch reused for one instance. Cache reciprocal/projection values separately for original, valid in-frustum vertices only; clipping-generated vertices still need their own projection. A per-mesh cache alone is wrong when two instances have different transforms.

**Cost/risk.** Additional indices, stamps and unique positions, plus scratch. Calculate the RAM change from the actual corpus before choosing 16-bit indices or a cap. Replacing the expanded representation can save memory; merely adding another full copy may not. Shader/pass-specific colour and UV data must not be shared accidentally. Scene reloads, origin rebasing, geometry mutation and instance changes invalidate cached results.

**Measure/accept.** Record transformed positions per submitted face corner and resulting L1-sensitive working set. Require existing geometry/welding counts and visual seam tests to remain valid. Accept only if lazy caching beats the simpler composed-transform path on representative rooms and towns, including models with little sharing.

### 4. Share a frame's camera basis and frustum preparation

**Benefit:** small recurring CPU saving with low implementation complexity. **Confidence:** high duplication; likely smaller than items 1–3. **MMX fit:** none.

**Evidence.** [camera_basis](../src/engine/gfx/camera.c) evaluates yaw and pitch sine/cosine. [scene.c: `frustum_planes`](../src/engine/scene/scene.c) and [`view_build`](../src/engine/scene/scene.c) each build their own basis. `scene_visible_models` builds a kept view and another occlusion view; `scene_visible_sprites` prepares the same camera again, while drawing also has a basis.

**Suggestion.** Build a frame view description once and pass/reuse it through model culling, occlusion, sprites and drawing. Keep an uncached entry point for arbitrary cameras used by diagnostic and interaction callers. Prefer explicit lifetime/versioning to a hidden global keyed only by a `Camera *` address.

**Cost/risk.** A small structure; do not enlarge every instance. A camera may change in place, and FOV/far/fog changes matter as well as yaw/pitch. Some calls can occur outside the normal draw sequence.

**Measure/accept.** Count basis construction and trigonometric calls. Verify same-frame multiple cameras, FOV changes, console exits, culling off and empty scenes. No stale frustum or cross-camera occlusion state.

### 5. Reuse clipping classifications instead of repeatedly testing planes

**Benefit:** steady geometry CPU work, particularly partial frustum intersections. **Confidence:** high repeated tests; payoff depends on clipped-face frequency. **MMX fit:** limited; a scalar bitmask is the first step.

**Evidence.** [poly_inside](../src/engine/gfx/draw.c) checks all corner planes for the direct-submit path. On failure, [clip_view](../src/engine/gfx/draw.c) scans planes again, and `clip_plane` calculates distances for cutting. The all-inside fast path is already present and must not be proposed as new.

**Suggestion.** Produce corner outcodes and polygon AND/OR masks during transformation or the first classification. Reject a polygon wholly outside a common plane, accept zero-OR polygons, and restrict clipping to potentially crossed planes. Recompute classifications as needed after cuts; do not blindly reuse original-vertex distances for generated vertices.

**Cost/risk.** Small per-corner masks and perhaps selective distance scratch. Extra always-written data can lose on mostly inside polygons. Preserve inside-first edge interpolation and existing clipping order where required for shared edges.

**Measure/accept.** Count all-inside, common-plane reject and actual clipping cases. Check each plane, corners crossing multiple planes, shared edges, degenerate polygons and the `CLIP_MAX`/`FACE_MAX` fallbacks. No cracks or reduced conservative visibility.

### 6. Reuse scene spatial selection across interaction, rays and collision

**Benefit:** interaction latency and recurring simulation/sound queries in large scenes. **Confidence:** high query duplication; costs vary by query frequency. **MMX fit:** poor until candidate sets are small and layout is suitable.

**Evidence.** [interact.c: `first_wall`](../src/engine/scene/interact.c) scans all model slots before box/ray tests. [scene_line_blocked](../src/engine/scene/scene.c) already uses spans, and [collide_use/collide_gather](../src/engine/scene/collide.c) already use a grid or streamed spans. Sound wall rays are already limited by [ambience.c: `walls`](../src/engine/sound/ambience.c); recommending a ray budget as new would miss existing work.

**Suggestion.** First extend the existing span/cell candidate traversal to activation rays, with nearest-hit ordering where useful. Consider a shared conservative query interface rather than three divergent spatial structures. Preserve broadphase-only selection: activation still needs exact reach and the nearest blocking wall, whereas a sound ray can stop at any blocker.

**Cost/risk.** Reuse current bounds before allocating another scene index. Dynamic movers, dead/empty slots, generation changes, far-ground duplicates and rebasing need explicit treatment. A use ray is event-driven; optimize its actual worst case without adding a passive per-frame ray.

**Measure/accept.** Count bounds/triangles tested for use, sound and AI separately. Validate off-centre activation at multiple FOVs, walls hiding doors, reach edges, passages and doors moving while queried. A faster broadphase must never allow use through a wall.

### 7. Cache collision triangle preparation and repeated floor coefficients selectively

**Benefit:** recurring body/AI CPU work, strongest when many bodies gather from the same meshes. **Confidence:** high repeated narrowphase preparation; medium ranking until actor counts are timed. **MMX fit:** not the first choice.

**Evidence.** [gather_model](../src/engine/scene/collide.c) scans a candidate mesh's triangles, rejects by local extents, transforms survivors and recomputes/normalizes their planes. [floor_under](../src/engine/scene/body.c) calls [`collide_floor`](../src/engine/scene/collide.c) at nine footprint points; floor eligibility and coefficients are recomputed while scanning the same gathered set.

**Suggestion.** Start with inexpensive prepared floor flags/coefficients for the current gathered shape. Then consider local triangle bounds and a small per-mesh triangle hierarchy, or bounded prepared static-instance data for crowded locations. Reuse an enclosing gather only while its query box, scene/mover generation and ellipsoid radii remain valid. Do not simply reuse the last body's scaled set for another body.

**Cost/risk.** Per-triangle acceleration can be large relative to 16 KB L1 and existing mesh data. Plane scaling depends on body shape; moving instances cannot use stale world geometry. The current 1024-triangle cap and dropped-triangle diagnostics remain essential.

**Measure/accept.** Count source triangles visited, transformed triangles, gathers per body, roots/divides and floor scans. Run future stair, shelf, wall-edge, low-ceiling, water, lift and differing-body-size regressions. Treat epsilon and reciprocal changes as numerical changes, not automatically bit-identical optimizations.

### 8. Use active entity sets and spatial bins before vectorizing body pairs

**Benefit:** recurring simulation and entity-render setup with larger streamed populations. **Confidence:** high linear scans; quadratic interaction is workload-dependent. **MMX fit:** low priority compared with fewer candidates.

**Evidence.** [entity_blocked](../src/engine/scene/entity.c) scans every entity for each moving actor. [body.c: `push_off`](../src/engine/scene/body.c) scans the entity array per body. [draw_entities](../src/engine/gfx/draw.c) calls `entity_flat` before distance/frustum rejection while scanning entity slots. Stream capacities include unused ranges.

**Suggestion.** Maintain a compact list of active collidable entities and conservative cell/chunk bins; query neighbouring cells for movement. Separate cheap bounding checks from pose/view selection for drawing when pose-independent conservative bounds are available. Existing game-side distant-update scheduling remains in place.

**Cost/risk.** Indices and membership updates, not copies of entire `Entity` structures. Preserve player/body special cases, vertical overlap, already-overlapping escape, corpses, population changes and deterministic pair processing order. Extra maintenance may not pay for a few monsters.

**Measure/accept.** Report active versus allocated slots and pairs tested at the hold, a crowded town and an empty streamed world. Match collision outcomes and actor ordering at chunk boundaries and after rebasing. Avoid reducing collision/update frequency as a disguised gameplay change.

### 9. Bound the largest streaming work unit and reduce emergency fills

**Benefit:** load and movement hitch tails; likely more visible than small warm-kernel gains. **Confidence:** high budget-boundary limitation. **MMX fit:** only inside a demonstrated expensive integer substep.

**Evidence.** [stream_update](../src/engine/scene/stream.c) checks its time budget after `fill_step`; it cannot preempt a callback. [`fill_under`](../src/engine/scene/stream.c) deliberately finishes a missing underfoot chunk synchronously. Game callbacks include whole terrain/block/far-bake phases in [wild.c: `fill`](../src/game/dagger/places/wild.c). [wres.c](../src/game/dagger/places/wres.c) already separates file pieces, decode and finish, but a complete image's smoothing/sheet or chain remains a unit. [texcache.c: `chain_set`](../src/engine/gfx/texcache.c) builds the whole retained chain.

**Suggestion.** Rank step maxima using `stream_worst`; split only the dominant step into resumable rows/meshes/faces. Give preparation enough movement-aware lead to reduce `fill_under` frequency while retaining its safety fallback. Consider early essential ground/collision readiness separately from decorative completion, only with a precise publication contract. Never remove the underfoot guarantee merely to lower a profile spike.

**Cost/risk.** More state, cancellation paths and partially prepared allocations. A smaller work unit does not make a synchronous disk seek bounded. Keep unfinished pictures/geometry unpublished and honor slot generation changes. Light cutting already yields at face boundaries and sky updates already read/upload incrementally.

**Measure/accept.** Record worst step, stream overrun count and end-to-end high-percentile frames along identical walks/runs, with cold and warm files. Require uninterrupted ground, correct dropped/reloaded chunks, stable memory and no partial image on screen.

### 10. Attack cold palette-search and preparation reuse before another bilinear MMX rewrite

**Benefit:** texture load/rebuild time and streaming hitches, not warm geometry frames. **Confidence:** high cold scalar work; medium whether it dominates. **MMX fit:** possible, but not automatically worthwhile.

**Evidence.** [mip_nearest](../src/engine/gfx/mip.c) already caches answers per 5-bit RGB cell and uses scalar squared-distance tables on misses. The 32768-entry `df_u16` cache is 64 KB. [wres.c: `decode_pic`](../src/game/dagger/places/wres.c) can smooth walls, and `chain_set` prepares chains. The source image, nearest-search cache and chain cache solve different costs. Recent halving, packing and far-ground MMX do not remove palette misses.

**Suggestion.** Instrument miss counts, miss time, palette switches and repeated identical preparation. First retain/reuse valid prepared results across equivalent resource uses within the existing RAM policy. An optional versioned derived cache could store exact prepared output beside the program, keyed by source identity, palette, climate/season, cutout/smoothing policy and algorithm version; original ARENA2 files stay untouched. If search itself dominates, compare an exact alternative search or batched distance kernel with the current compact scalar loop.

**Cost/risk.** More RAM/disk and invalidation complexity. Eagerly filling every inverse cell requires millions of palette comparisons per palette and can worsen startup; do not assume it is free. A SIMD distance path must preserve squared RGB distance, 32-bit totals and lower-index ties; original MMX lacks the convenient later min/reduction instructions. Existing square tables already avoid scalar multiplies.

**Measure/accept.** Byte-identical prepared indices/chains for representative palettes, transparent bleed, glass and all ties; cache invalidation after data/mod/palette changes. Compare cold preparation, repeat entry and RAM/cache churn separately. Keep file I/O, palette search and averaging timings distinct.

### 11. Tune and validate the MMX kernels already present

**Benefit:** load/hitch work for texture kernels; recurring sound work for the mixer. **Confidence:** high specific scheduling opportunities, unmeasured net gains. **Feasibility:** localized, moderate assembly risk.

**Evidence.** [mip.c: `box_sum_mmx`](../src/engine/gfx/mip.c) immediately uses a `MOVZBL` destination as the index of a palette address. This matches the P55C address-generation dependency described by Intel. [`halve_mmx`](../src/engine/gfx/mip.c) and [`pack_mmx`](../src/engine/gfx/mip.c) have independent texel groups but many shifts/unpacks. [mixer.c: `add_mmx`](../src/engine/sound/mixer.c) already batches samples.

**Suggestion.** After correctness/target timing is authorized, inspect emitted 32-bit code and try scheduling enough independent work between index loads and their address use. Interleave independent accumulators/texel operations within register limits. Experiment with internal eight-byte buffer/mask alignment and batch-size thresholds. Measure row-total spill traffic and per-rectangle call/EMMS overhead before making the far-ground batch larger. Its 64-sample threshold is explicitly heuristic today.

**Cost/risk.** More index registers can cause spills on 32-bit x86; unrolling can overfill instruction cache. More shifts cannot bypass a single shifter, and memory-form MMX operations have pairing restrictions. These scheduling constraints follow Intel's P55C rules, not host instruction-count intuition. [Intel MMX guidelines, §§3.2.1 and 3.3.3](https://datasheets.chipdb.org/Intel/x86/MMX/MMX/DG_CHP3.HTM).

**Measure/accept.** Use existing scalar switches independently and together; preserve authored raw-byte/RGB oracles, odd tails, alignment cases, maximum sums and x87-return checks. Preserve `(sum+2)>>2` for mip halving, alpha `>=128` and signed-pack handling for 1555, and a single truncating whole-box division for ground. Unsupported CPUs must remain scalar. Require end-to-end benefit as well as kernel timing; no gain means keep the simpler version.

### 12. Give known UI rectangles a cheaper submission path and amortize text state

**Benefit:** overlay-heavy frames and viewers; less important with the HUD/panels hidden. **Confidence:** high setup duplication, conditional practical priority. **MMX fit:** little; avoid generic geometry work first.

**Evidence.** [font.c: `glyph_quad`](../src/engine/ui/font.c) builds four `DrawVert`s per glyph. [ui_string_alpha](../src/engine/ui/font.c) enters/leaves UI/alpha/tint state per string. [glide_polygon](../src/engine/gfx/glide.c) fills a vertex array, searches for a largest-area starting triangle, rotates/copies it and submits it. These are useful general polygon rules, but screen-aligned glyph rectangles already have a known shape.

**Suggestion.** Add a narrowly scoped rectangle path that retains snapping and known-good planar gradients without generic start-corner selection/copying. Scope UI state over compatible text runs or a panel, preserving string-specific ink/opacity. This is state batching, not a nonexistent modern Glide vertex-buffer facility; individual glyph submissions may remain necessary. Cache static composed labels only when their texture-room cost is justified.

**Cost/risk.** Small scratch; cached panels can consume scarce TMU space and require translation/resolution invalidation. Preserve picture palettes, cutout alpha, point sampling, partial rectangles and state restoration before world rendering.

**Measure/accept.** Compare identical scenes with F1 panels hidden/shown, record glyphs and Glide state changes, and include real GFALL overlays. No change to text metrics, colours, fading, clipping or the next world pass. Do not sell a viewer-only improvement as game geometry performance.

### 13. Avoid repeated sound-gain trigonometry when pan has not changed

**Benefit:** recurring sound CPU setup, independently of PCM mixing. **Confidence:** high repeated gain calculation; medium elapsed importance. **MMX fit:** poor for the existing x87 gain calculation.

**Evidence.** [sound.c: `aim`](../src/engine/sound/sound.c) calculates distance/pan and two trigonometric functions. [`sound_update`](../src/engine/sound/sound.c) calls it for every active voice each update; other gain-setting calls can already have evaluated it. Many flat/weather loops have unchanged pan while volume or wall attenuation changes.

**Suggestion.** Separate pan factors from attenuation and track when listener/voice geometry, pan, volume and muffle state actually change. Reuse the sine/cosine factors for unchanged pan; retain frame-rate gain updates for moving sources/listeners and easing. Begin with exact reuse. An approximate panning lookup is a separate fidelity decision, not a prerequisite.

**Cost/risk.** A few cached values/dirty flags per voice. Handle slot reuse/serials, master volume, listener orientation and device-rate changes. A moving player can invalidate much of the cache; measure that case.

**Measure/accept.** Count `aim`/trig calls and separate setup time from `sound_mix` and wall-ray time. Compare gain values and mixed PCM for stationary/moving scenes, voice stealing, muffling and volume transitions. No stale pan, clicks or altered voice-selection decisions.

### 14. Reduce measured heap/zeroing churn while preserving ownership

**Benefit:** chunk load/drop latency, long-session memory behaviour and preparation locality. **Confidence:** high allocation mechanisms, conditional bottleneck. **MMX fit:** generally low.

**Evidence.** [mem.c Win32 heaps](../src/engine/core/mem.c) create per-owner heaps and allocate with `HEAP_ZERO_MEMORY`. Some users then fill/copy buffers completely. Ground reduction still uses a short-lived source heap in [wild.c: `tile_small`](../src/game/dagger/places/wild.c); picture preparation tracks individual allocations in `wres.c`. Existing scratch reuse and epoch protection are deliberate.

**Suggestion.** Count heap creation, allocations, zeroed bytes and frees per phase. Reuse a bounded decode/source arena where lifetime permits. Consider an explicit uninitialized-allocation API only for buffers proven completely written before publication; keep the default zeroed API. Prefer eliminating allocation and improving contiguous access to adding MMX zero-fill instructions.

**Cost/risk.** Retained arenas trade resident RAM for fewer allocations; cap them against actual 64 MB working set. Do not globally remove zeroing, destroy a shared heap, reuse a pointer across `mem_epoch`, or retain another copy of every source image. Separate host allocator behaviour from Windows 98 heaps.

**Measure/accept.** Compare cold entry and long walks including failed loads, cancellation, re-entry and memory pressure. Existing heap ownership/epoch tests and stable high-water usage remain acceptance requirements. Do not assume the accounting counters equal total Windows heap overhead.

### 15. Cache texture priority calculations only if residency scheduling shows up

**Benefit:** recurring streaming service CPU work; conditional, lower priority. **Confidence:** high repeated candidate work, uncertain dominance. **MMX fit:** weak compared with caching decisions.

**Evidence.** [texres_note](../src/engine/gfx/texres.c) already builds a compact noted list. [`texres_service`](../src/engine/gfx/texres.c) rescans it to choose each upload and recomputes `want_drop`; up to eight uploads and two classes are supported. Eviction/fragmentation selection also traverses resident records. This is not a naive scan of all slots for every note.

**Suggestion.** Cache desired drop/shortage per noted entry for the current service call, or build small priority buckets if those scans are measurable. Invalidate after a successful upload or policy change. Preserve deterministic priority and the allocator's global region evaluation rather than replacing it reflexively with a heap/tree.

**Cost/risk.** A few bytes per noted entry versus more maintenance and branches. Source arrays already scale to 4096 slots; a larger metadata structure can hurt L1. A driver upload often outweighs scheduler arithmetic.

**Measure/accept.** Count candidates visited, scheduler time, uploaded bytes, fragmentation, evictions and detail latency. Retain permanent tiny copies, sprite/wall fairness, bounded steals, allocation boundaries and the fragmentation churn regression. Equal CPU time with poorer sharpness is a regression.

### 16. Batch more audio work only when mixing is the measured sound cost

**Benefit:** recurring multi-voice PCM work. **Confidence:** current per-voice boundaries are clear; expected benefit is lower than a claim of fourfold mixing speed. **MMX fit:** legitimate, but ordering and tails constrain it.

**Evidence.** [sound_mix](../src/engine/sound/sound.c) visits active voices; `mix_voice` enters a batch per unit-rate run and ends it with `EMMS`. Non-unit-rate stepping is scalar in [mixer.c](../src/engine/sound/mixer.c). [audio.c: `put_mix`](../src/engine/sound/audio.c) actually clears a 1024-frame RAM scratch with `__builtin_memset`; optimizing the standalone `mix_clear` helper alone would not change that production path.

**Suggestion.** If PCM mixing dominates, investigate one integer-only batch over multiple eligible voices, preserving scalar fallback and voice/run boundaries. Evaluate pitched mixing separately by actual usage: variable source positions require gathers and complicate SIMD. Keep the small RAM scratch unless device-specific measurements prove direct mixing better; repeated read-modify-write of driver/device memory can undo the saved copy.

**Cost/risk.** More complex loop/stop/loop-wrap handling and perhaps small run descriptors. Saturating addition is not associative: preserve the current voice order rather than summing voices in wider accumulators and clipping once. An all-MMX batch must return with valid x87 state on every path.

**Measure/accept.** Bit-identical PCM including overload saturation, silence, pitch, wraps, short clips and voice stealing; no underrun or changed refill lead. Test DirectSound primary/secondary and WINMM separately later. Keep hardware MIDI on the AWE64 and its existing voice cap.

### 17. Move visibility rejection before avoidable per-light setup

**Benefit:** small steady-frame saving in scenes with many off-screen light pools; investigate after larger geometry work. **Confidence:** high source ordering, unmeasured magnitude. **MMX fit:** none needed.

**Evidence.** [draw_lights](../src/engine/gfx/draw.c) checks that a light has decals, then calculates flicker and some tint/distance setup before `scene_box_visible`. It already rejects distant outdoor lamps and already transforms/clips a decal once for repeated lamp passes. `glide_tint` already suppresses equal-state writes.

**Suggestion.** Reject conservative light bounds before pure shading work/state writes when semantics allow, and keep a compact active-light list if sparse slot scans are measurable. If blended passes dominate instead, investigate an explicit visual-quality option with target captures; simply lowering lamp passes/range changes the result and is not a free optimization.

**Cost/risk.** Small code change and possibly active indices. Preserve flicker's time basis, state for the next visible light, darkness, fade and shadowed decal coverage. Do not repeat the already implemented budgeted decal-cutting work as a new proposal.

**Measure/accept.** Count light-box rejections, actual state writes and decal polygons with view direction varied. Require identical visible flicker/tint and no lost light at frustum/occlusion boundaries. Confirm full-frame impact, especially when fill rather than CPU setup dominates.

## Tempting approaches to avoid or demote

1. **Treating the old MMX suggestions table as a measured roadmap.** Its “Great” ratings are hypotheses. Halving, packing and far-ground sums are now implemented. Heightmap byte averaging is not evidence of a current hot pure-average loop: the active terrain recipe also performs interpolation/noise/mesh work. Identify a real dominant loop first.
2. **“Muffle four voices” as a simple optimization.** Current muffling is gain attenuation/easing, not a per-sample low-pass filter. Adding a filter changes behaviour and creates work. SIMD across a new filter is feature development, not removal of an existing cost.
3. **MMX world transforms or fixed-point culling without a range proof.** Packed integer lanes do not accelerate x87 by themselves. Conversion, precision, overflow and EMMS overhead must be included; large world coordinates and conservative visibility make casual quantization dangerous.
4. **SSE/P6/3DNow recipes, non-temporal stores or branchless conditional-move rewrites.** They violate this target. Integer branch removal using supported instructions can still be considered, but its cache, dependency and correctness costs need evidence. P6 partial-register folklore is not a P55C diagnosis.
5. **A software rasterizer in front of the Voodoo.** The current architecture already delegates raster work. Better CPU batching/culling is a different proposal from duplicating the card's job.
6. **Sorting the entire world every frame, or globally sorting transparent passes.** State changes and fill can be expensive, but full sorting has a CPU cost and changes alpha/overlay order. Start with measured opaque, bounded groups and preserve correctness. The existing cached bind/tint paths already remove some state calls.
7. **Blind loop unrolling, aligning/padding every structure, or replacing every copy with MMX.** Added code/data can exceed small caches. REP word copies already exist. Screenshot expansion, one-off data viewers and unused generic pack/RPG screens do not deserve priority over active game loops without a separate responsiveness problem.
8. **Global fast-math, reciprocal approximations or broader x87 single precision.** Preserve exact integer rounding rules, the render precision boundary and world-coordinate doubles. A change of x87 precision can change intermediate rounding even when C variables are `float`; existing comments claiming only timing changes are not a numerical proof. No such change is made here.
9. **Assuming a worker thread fixes CPU-bound loading on one core.** It cannot add CPU capacity. An I/O-only reader might overlap waits, but the present global heaps/palette/work buffers and Win98/Glide ownership make broad asynchronous preparation a separate high-risk design. Begin with finer work units and measured disk wait; keep device calls on their existing thread.
10. **Removing safe underfoot fills, tiny textures, collision caps or image publication rules to make graphs look faster.** These maintain playable correctness under pressure. Optimizations must reduce work without replacing stalls with missing ground, untextured objects or partially prepared assets.

## Recommended execution order when further work is authorized

1. Establish the target baseline and validate the three recent texture MMX additions using their existing switches and authored checks.
2. Prototype local-camera face rejection and a composed instance-to-view transform independently. They are smaller, lower-memory changes than a mesh-format rewrite.
3. Follow measured pressure: indexed lazy geometry for CPU submission, query acceleration for actor-heavy simulation, or smaller streaming work units for hitch tails.
4. Compare UI rectangle/state work only with real overlay load; compare sound gain reuse before expanding the PCM assembly if setup/rays dominate sound.
5. Tune new MMX gather scheduling and thresholds only against correct scalar output and actual P55C end-to-end results. Retain scalar on kernels/shapes where it wins.

For every future change: keep a reference path, make one mechanism reviewable at a time, record RAM/working-set costs, verify period instruction/import constraints when builds are authorized, run the relevant host and Dagger regressions, and play the same target scenes. This report approves no implementation and records no measured performance gain.
