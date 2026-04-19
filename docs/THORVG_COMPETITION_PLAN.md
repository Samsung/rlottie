# rlottie ThorVG Competition Plan

## Mission

Build `rlottie` into a Tizen-first Lottie engine that:

1. Supports more real-world Lottie features than ThorVG on the target asset corpus.
2. Renders faster than ThorVG on the target Tizen CPU workloads.
3. Stays maintainable enough to keep extending parser, evaluator, and renderer without repeated full rewrites.

This is not a generic "improve rlottie a bit" effort. The explicit end goal is to beat ThorVG in both feature coverage and render performance for the workloads that matter to Samsung and Tizen.

## Final Goal

The project is complete only when all of the following are true:

1. `rlottie` passes a documented support matrix that exceeds ThorVG on the agreed benchmark corpus.
2. `rlottie` beats ThorVG steady-state frame time on target Tizen-class ARM devices for the main workload buckets.
3. `rlottie` beats or matches ThorVG in parse-to-first-frame latency for the same corpus.
4. The feature claims are backed by golden-image or equivalent correctness tests, not README assertions.
5. The architecture is in a state where new Lottie features can be added without entangling parser, evaluator, and raster code further.

## Current Snapshot

### Repository State

- Repository cloned from `Samsung/rlottie`.
- Local build path restored on macOS arm64.
- CMake benchmark target for `lottieperf` added.
- `lottiebench` now supports internal hotspot profiling through `--profile`.
- `thorvgbench` and `compare_lottie_engines.py` provide direct
  `rlottie` vs `thorvg` comparison on shared JSON corpora.
- `.lottie` loading improved to select animation JSON from manifest-aware archive paths.
- Fractional `w/h/sw/sh` values now parse correctly on compositions, assets, and
  layers, which unblocks real text-heavy assets such as `text_anim.json`.
- Large-row `VRle` merge path no longer depends on a fixed 1024-byte scratch buffer.
- `ShapeLayer` now avoids one obvious repeated drawable-list rebuild path.
- `Skew` / `Skew Axis` parsing and transform composition now work on targeted
  group-transform, repeater, and 3D layer fixtures.
- `Merge Paths` now has fixture-backed boolean support for fill and gradient-fill
  cases, including `Intersect`, `Subtract`, and `Exclude`.
- Layer `Blend Modes` now have fixture-backed software-render support for
  `Multiply`, `Screen`, `Overlay`, `Darken`, `Lighten`, `Color Dodge`,
  `Color Burn`, `Hard Light`, `Soft Light`, `Difference`, `Exclusion`,
  `Hue`, `Saturation`, `Color`, and `Luminosity`.
- Layer `Effects` now have a narrow first-pass `ADBE Fill` implementation for
  whole-layer solid and precomp output, with targeted fixtures for both cases.
- Narrow `ADBE Fill` parsing is now hardened against JSON key-order variance,
  disabled-effect ordering, unsupported enabled sibling effects, and missing
  explicit opacity parameters.
- Module-mode image loading now builds the image-loader plugin with `rlottie`
  and probes build-tree and library-relative plugin locations at runtime,
  which restores embedded and file-backed image assets in local builds.
- Offscreen blend composition now supports logical draw regions, so blend
  layers can render into tight temporary surfaces instead of full clip-sized
  buffers.
- Matte composition now uses tight offscreen surfaces, and direct alpha-matte
  cases avoid a full offscreen blend pass when the layer stack qualifies.
- `thorvg_example_smoke.txt` now defines a repeatable smoke subset from
  `thorvg.example/res/lottie` for functional, performance, and memory checks.

### What Was Verified

- `cmake -S . -B build -DLOTTIE_TEST=OFF -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build -j 8`
- `lottieperf` runs through the CMake build.
- Minimal `.lottie` v1/v2 archive loading was verified with local synthetic archives.
- Shared-engine smoke comparison is now runnable against the `thorvg.example`
  corpus through `build/example/lottiebench`,
  `build/example/thorvgbench`, and
  `benchmarks/compare_lottie_engines.py`.

### What Is Still Weak

- Existing automated tests are too shallow to validate support claims.
- The smoke benchmark is useful, but it is still a coarse sample and not yet a
  complete Tizen product corpus.
- The support table in `README.md` is stale relative to the implementation.

## Current Measured Status

### ThorVG Smoke Snapshot

Reference run:

```sh
python3 benchmarks/compare_lottie_engines.py \
  --asset-dir /Users/junsu/Documents/thorvg.example/res/lottie \
  --asset-list benchmarks/thorvg_example_smoke.txt \
  --size 360x360 \
  --iterations 30 \
  --warmup 3
```

Current result on the local comparison host:

- Parse latency: `rlottie 11` wins, `thorvg 5` wins
- First-frame latency: `rlottie 11` wins, `thorvg 5` wins
- Steady-state frame time: `rlottie 7` wins, `thorvg 9` wins
- Steady RSS: `rlottie 12` wins, `thorvg 4` wins

Clear current `rlottie` steady-state wins:

- `glow_loading.json`
- `gradient_sleepy_loader.json`
- `masking.json`
- `starts_transparent.json`
- `textrange.json`
- `windmill.json`

Largest current `rlottie` steady-state losses:

1. `expressions/world_locations.json`: `0.572 ms` vs `0.197 ms`
2. `11555.json`: `1.548 ms` vs `1.173 ms`
3. `confetti.json`: `0.230 ms` vs `0.177 ms`
4. `text_anim.json`: `0.129 ms` vs `0.081 ms`
5. `polystar_anim.json`: `0.157 ms` vs `0.121 ms`
6. `stroke_dash.json`: `0.160 ms` vs `0.140 ms`
7. `merging_shapes.json`: `0.061 ms` vs `0.053 ms`

Near-parity or noise-range assets should not dominate priority decisions.

### ThorVG Full-Corpus Audit Snapshot

Reference audit flow:

```sh
python3 benchmarks/audit_thorvg_corpus.py \
  --asset-dir /Users/junsu/Documents/thorvg.example/res/lottie \
  --iterations 5 \
  --warmup 1 \
  --size 360x360
```

Current full-corpus triage on the local comparison host:

- Total cases: `148`
- Common loaded cases: `146`
- `rlottie` load failures: `0`
- ThorVG load failures: `0`
- Common-case coarse signature mismatches: `119`
- Parse latency: `rlottie 87` wins, `thorvg 59` wins
- First-frame latency: `rlottie 115` wins, `thorvg 31` wins
- Steady-state frame time: `rlottie 50` wins, `thorvg 96` wins
- Steady RSS: `rlottie 127` wins, `thorvg 19` wins

This broad audit changes the interpretation of the current gap:

- `rlottie` is not broadly failing to load the corpus. The current problem is
  output correctness drift and steady-state speed on a concentrated set of
  heavier assets.
- The worst current zero-output mismatches, such as `32266.json` and
  `R_QPKIVi.json`, are both expression-heavy. That does not make expressions a
  short-term project, but it does confirm that some of the broadest visible
  correctness holes now sit behind expression payloads rather than parser load
  failures.
- The biggest full-corpus steady-state losses extend beyond the smoke subset
  and now include `balloons_with_string.json`, `expressions/11272.json`,
  `threads.json`, and `43391.json` in addition to `11555.json`.
- The biggest parse and memory losses also identify separate backlogs:
  `holdanimation.json`, `page_slide.json`, `expressions/16447.json`,
  `starburst.json`, `uk_flag.json`, and `textblock.json` should be treated as
  full-corpus parse/RSS triage cases rather than ignored because they are
  outside the smoke subset.
- A post-`ADBE Fill` rerun with the same audit workflow kept the same broad
  shape: `0` load failures and `119` coarse signature mismatches. Opening
  layer `ef` parsing for the narrow fill subset did not create a broad
  regression outside the new effect fixtures.

### Hotspot Review

`world_locations.json` is still the highest-value performance target.

Current `lottiebench --profile` run shows:

- `render_matte_ms = 17.68 ms` across the 30-frame steady-state loop
- `composition_render_ms = 18.69 ms` total steady render time
- `comp_update_ms = 0.31 ms`
- `shape_update_ms = 0.21 ms`
- `paint_update_ms = 0.15 ms`

That means the current dominant loss is no longer general property evaluation.
It is still matte composition, especially repeated alpha-matte work inside the
same asset.

The broader backlog is no longer matte-only. Follow-up hotspot review shows
`11555.json` and `confetti.json` are now dominated by rerasterization of static
vector content under transform-only motion, while `text_anim.json` is dominated
by coarse non-opaque precomp/offscreen composition. Generic keyframe lookup
work should stay behind matte reuse and transform-cache work.

## Bug Triage

These are the most important correctness bugs that should be treated as
active engineering work rather than vague backlog items:

1. Module-build image loading was a recent zero-output blocker.
   - Status: fixed in local module builds.
   - Repro assets closed by the fix: `image_embedded.json`, `32266.json`
   - Root cause: `LOTTIE_MODULE` builds did not reliably build or locate the
     `rlottie-image-loader` plugin, so image assets fell back to `0x0`
     bitmaps.
   - Verification: `image_embedded.json` now renders nonzero pixels locally,
     and `32266.json` no longer hits the previous zero-output mismatch.
2. Some real assets still diverge for non-expression reasons even after load
   succeeds.
   - Repro assets: `R_QPKIVi.json`, `43391.json`
   - Current diagnosis: likely shape-layer transform or drawable-list
     generation bugs rather than a broad parser failure.
   - Short-term action: reduce them into minimal fixtures and compare bounds,
     update, and raster stages separately.
3. Narrow `ADBE Fill` support had parser correctness bugs and is now only
   narrowly trusted.
   - Fixed now: key-order dependency, disabled-effect ordering, mixed enabled
     sibling-effect acceptance, and implicit-opacity default.
   - Remaining gap: broaden only after mixed real-asset adjudication proves
     the narrow bitmap-postprocess model is sound.

## Critical Review Summary

### Strengths

- Core shape rendering path is already competent for shape, trim, repeater, masks, mattes, gradients, images, precomps, and markers.
- Parser is already in-situ and arena-based, so raw JSON tokenization is not the first-order bottleneck.
- The codebase already has CPU-oriented software rasterization and SIMD entry points, which is useful for Tizen-first tuning.

### Major Feature Gaps

- `Text` support is still structurally incomplete. Some assets such as
  `text_anim.json`, `textblock.json`, and `textrange.json` render today, but
  `textblock.json` is outlined shape content rather than proof of a real
  runtime text pipeline. The parser still does not consume full text payloads
  such as `fonts`, `chars`, and layer `t` data, and the renderer still has no
  dedicated `Layer::Type::Text` execution path.
- `Merge Paths` now has fixture-backed fill/gradient-fill support for boolean
  modes, and static merge RLE no longer gets recomputed every frame, but stroke
  semantics still fall back to path concatenation and need a real path-boolean
  backend to close the gap fully.
- `Skew` and `Skew Axis` now have fixture-backed coverage on base transforms,
  repeaters, and 3D layers, but they still need broader mixed-asset regression
  coverage before the gap is fully closed.
- Layer `Blend Modes` now cover the full standard Lottie layer-blend family
  from `Multiply` through `Luminosity` on targeted fixtures, but mixed-asset
  coverage and image-level adjudication against ThorVG are still open.
- `Layer Effects` are still mostly missing. A narrow `ADBE Fill` subset now
  works on targeted fixtures, but `Tint`, `Stroke`, effect stacks, masked fill,
  feathered fill, and mixed real-asset adjudication are still open.
- Soft-mask features such as `Feather` and `Expansion` are missing, while hard
  mask path modes remain the only fully wired mask family today.
- Expressions remain unsupported engine-wide and should not drive near-term
  competition priorities.
- `.lottie` support was fragile and still needs broader archive coverage.

### Major Architecture and Performance Problems

- Full-tree frame evaluation remains too eager.
- Offscreen composition is still too expensive in matte-heavy assets, even after
  recent tight-surface and direct-alpha improvements.
- Matrix-only animation still tends to rebuild and rerasterize static vector
  content instead of reusing transform-friendly cached results.
- Async scheduling is fragmented and risks nested parallel inefficiency.
- `VRle` boolean paths needed correctness hardening.
- Repeater strategy is structurally expensive.
- Cache layers are too simple and not driven by strong invalidation or workload-specific reuse.

### Competitive Reality

- ThorVG is currently ahead in breadth of Lottie feature support.
- ThorVG is structurally cleaner in parser-to-scene separation.
- `rlottie` can still win on Tizen if we optimize for CPU-only, fixed-size, no-expression, no-heavy-effect workloads first instead of chasing a generic engine design.

## Review Rules

Every non-trivial change must be reviewed against these questions:

1. Does it improve the ThorVG competition target, or is it just local cleanup?
2. Does it reduce frame time, memory traffic, or unsupported asset count in a measurable way?
3. Does it make future features easier to add, or does it deepen parser-renderer coupling?
4. Does it preserve correctness on masks, mattes, alpha, gradients, and path operations?
5. Is the claim backed by a benchmark, a fixture, or a golden output?

No feature should be marked supported without a reproducible fixture.
No performance change should be called an optimization without before/after measurement.

## Implemented Spec Review

The following areas now have direct code or fixture coverage and should be
treated as implemented work rather than open hypotheses:

- `Skew` / `Skew Axis`: verified on base transforms, repeater transforms, and
  3D layer transforms.
- `Merge Paths` fill and gradient-fill boolean modes: verified on
  intersect/subtract/exclude fixtures.
- Layer `Blend Modes`: verified fixtures exist for `Multiply`, `Screen`,
  `Overlay`, `Darken`, `Lighten`, `Color Dodge`, `Color Burn`, `Hard Light`,
  `Soft Light`, `Difference`, `Exclusion`, `Hue`, `Saturation`, `Color`, and
  `Luminosity`.
- `.lottie` manifest-aware archive selection.
- Fractional size parsing for real-world JSON that stores dimensions as floats.
- Tight offscreen composition for blend and matte layers.
- Direct alpha-matte clip-path fast path for qualifying layer stacks.
- Profiling-backed benchmark workflow for internal hotspot attribution.

## Remaining Spec Backlog

These are the highest-value compatibility gaps that still need explicit
implementation work or broader proof:

1. `Merge Paths` stroke semantics with a real path-boolean backend
2. Real text-layer support beyond outlined-shape assets, including parser
   coverage for `fonts`, `chars`, and layer `t` payloads plus a dedicated
   renderer path for `Layer::Type::Text`
3. Broader `Layer Effects` beyond the current narrow `ADBE Fill` subset,
   starting with `Tint` and `Stroke`
4. Soft-mask `Expansion`
5. Soft-mask `Feather`
6. Broader `.lottie` archive coverage beyond the current manifest-path cases
7. Expressions, explicitly treated as a longer-term project rather than a
   short-term ThorVG competition lever
8. Mixed-asset regression coverage so supported fixtures map to real assets

## Active Performance Backlog

Priority is driven by measured ThorVG comparison losses and current hotspot
evidence, not by generic cleanup preferences.

1. `expressions/world_locations.json`
   - Goal: cut matte cost until steady-state is no longer the worst smoke
     loss.
   - Current blocker: repeated matte composition still dominates frame time,
     especially when matte-clipped translucent shape content falls back to the
     two-offscreen path instead of a reusable direct-alpha route.
2. `11555.json`
   - Goal: stop rerasterizing static vector art when the animation is mostly
     matrix-only motion.
   - Current blocker: static shapes and styles still re-enter the path/raster
     pipeline on transform changes instead of taking a transform-cache path.
3. `confetti.json`
   - Goal: make the same transform-cache work pay off on mixed-shape scenes
     that animate transforms without animated path geometry.
4. `text_anim.json`
   - Goal: reduce non-opaque precomp/offscreen cost on outlined text scenes
     without pretending this solves the real text-layer gap.
5. `polystar_anim.json`
   - Goal: inspect star/path evaluation and cached geometry reuse.
6. `stroke_dash.json`
   - Goal: reduce dash recomputation and stroke raster overhead.
7. `merging_shapes.json`
   - Goal: revisit merge-path cost on real assets after stroke semantics work lands.

`textblock.json` is no longer an active smoke loss and should stay secondary to
`text_anim`, because it is outlined-shape content rather than proof of a real
text engine.

The full-corpus audit adds a second ring of performance targets behind the
smoke subset:

- `balloons_with_string.json`
- `expressions/11272.json`
- `threads.json`
- `43391.json`
- `holdanimation.json` for parse/RSS rather than steady-state alone

`masking.json`, `windmill.json`, `glow_loading.json`, and
`gradient_sleepy_loader.json` are no longer top priority performance targets
because they are already competitive or faster than ThorVG in the current smoke run.

## Lagging Feature Buckets And Strategies

This is the current working map of where `rlottie` still trails ThorVG in
spec coverage, correctness, or steady-state performance. The grouping is by
feature family, not by file path, so the work stays extensible instead of
turning into one-off asset hacks.

### Mattes, Masks, And Offscreen Compositing

- Representative assets: `expressions/world_locations.json`, `masking.json`,
  `confetti.json`
- Current failure mode: repeated alpha-matte and matte-layer composition still
  dominates frame time when translucent shape content misses the narrow
  direct-alpha path.
- Improvement strategy:
  1. propagate inherited mask/matte bounds into every offscreen entry point
  2. widen direct-alpha matte coverage
  3. reuse matte-source RLE where the source stack is static
  4. formalize opaque vs non-opaque layer-stack fast paths instead of keeping
     them implicit

### Static Geometry With Animated Transforms

- Representative assets: `11555.json`, `confetti.json`, `threads.json`,
  `textblock.json`
- Current failure mode: static vector content is still re-rasterized under
  transform-only motion instead of taking a transform-cache path.
- Improvement strategy:
  1. split content dirtiness from transform dirtiness
  2. add a retained local-space snapshot/cache for mask-free, repeater-free
     shape layers
  3. teach repeaters and grouped transforms to consume cached geometry before
     re-entering the full path/raster pipeline

### Text Layers, Fonts, And Chars

- Representative assets: `text_anim.json`, `textrange.json`
- Current failure mode: some outlined-shape text assets render, but real text
  payload coverage is still structurally incomplete and the current outlined
  text path still trails ThorVG on steady-state.
- Improvement strategy:
  1. parse `fonts`, `chars`, and layer `t` payloads for a real
     `Layer::Type::Text` path
  2. build a glyph-first renderer with a text glyph cache
  3. shrink text/precomp offscreen bounds so outlined text scenes stop paying
     broad non-opaque surface costs

### Layer Effects

- Representative assets: `expressions/layereffect.json`, `shutup.json`,
  `bell.json`, `pumped_up.json`
- Current failure mode: only a narrow whole-layer `ADBE Fill` subset is
  supported today.
- Improvement strategy:
  1. keep using bitmap postprocess effects first instead of designing a generic
     effect graph up front
  2. land `Tint` and `Stroke` next
  3. only after single-effect fixtures are stable, handle mixed enabled stacks
     with an explicit allowlist and adjudicated output

### Merge Paths Stroke Semantics

- Representative assets: `merging_shapes.json`
- Current failure mode: fill and gradient-fill boolean paths work, but stroke
  semantics still fall back to concatenation instead of a real boolean result.
- Improvement strategy:
  1. add a real path-boolean backend
  2. if that is too invasive short-term, outline strokes first and boolean the
     outlined fill result
  3. re-run real-asset merge cost checks only after semantics are correct

### Shape Pipeline Correctness And Transform Edge Cases

- Representative assets: `R_QPKIVi.json`, `43391.json`
- Current failure mode: load succeeds but rendered output still diverges,
  which points to transform/bounds/drawable-list bugs rather than parser gaps.
- Improvement strategy:
  1. reduce each asset to a minimal fixture
  2. compare layer update, group transform composition, bounds propagation,
     and drawable-list generation independently
  3. fix correctness before optimizing performance on these assets

### Expressions

- Representative assets: `balloons_with_string.json`,
  `expressions/11272.json`, `holdanimation.json`
- Current failure mode: expression payloads still sit outside the supported
  runtime and create large visible mismatches.
- Improvement strategy:
  1. do not build a general expression engine first
  2. identify the smallest recurring expression subset on the Samsung/Tizen
     corpus
  3. add explicit unsupported-expression diagnostics and fallback behavior so
     failures are explainable even before broader support lands

### Parser Throughput And Precomp-Heavy Parse Cost

- Representative assets: `holdanimation.json`, `page_slide.json`,
  `expressions/16447.json`, `starburst.json`, `uk_flag.json`
- Current failure mode: full-corpus parse latency still trails on heavy
  precomp/property payloads even though first-frame latency is broadly strong.
- Improvement strategy:
  1. add common-key fast paths and reserve sizing for high-fanout arrays
  2. skip unsupported blobs more cheaply
  3. cut asset/layer allocation overhead before touching general evaluator code

## Workstream Structure

### Group A: Spec and Compatibility

Owner responsibilities:

- Rebuild the real support matrix from code and fixtures.
- Add missing high-value Lottie features in priority order.
- Keep ThorVG comparison updated feature by feature.

Primary targets:

1. `Text` with glyph-first support.
2. `Merge Paths` stroke semantics and broader real-asset coverage.
3. broader layer `Blend Modes` coverage beyond the current mode set, especially
   hue/saturation/color family modes and mixed-asset regressions.
4. Broader `Layer Effects` subset beyond the current `ADBE Fill` landing:
   `Tint`, `Stroke`, and mixed-stack correctness.
5. Soft-mask features: `Expansion`, `Feather`.
6. broader mixed-asset skew regression coverage.

### Group B: Render Performance

Owner responsibilities:

- Reduce frame time on CPU raster workloads.
- Shrink offscreen rendering costs.
- Simplify runtime evaluation and invalidation behavior.

Primary targets:

1. Repeated alpha-matte and matte-layer cost reduction for `world_locations`.
2. Better frame-to-frame dirty propagation.
3. Transform-cache paths for matrix-only static vector content such as `11555`.
4. Property evaluation acceleration with segment cursors or indexed search.
5. Dash, star, and text-specific geometry reuse for current ThorVG loss assets.
6. Repeater lazy materialization.
7. Scheduler simplification or unification.
8. More useful cache behavior for surfaces, paths, and repeated frame work.

### Group C: Benchmark and Validation

Owner responsibilities:

- Build the corpus and benchmark harness that decides success or failure.
- Prevent README drift by tying support claims to fixtures.

Primary targets:

1. Golden-image regression tests.
2. Feature-bucket corpus.
3. ThorVG comparison harness.
4. Tizen hardware runs with stable measurement procedure.

### Group D: Architecture and Integration

Owner responsibilities:

- Prevent local fixes from turning into permanent structural debt.
- Define the separation between parse model, evaluated animation state, and raster backend.

Primary targets:

1. Introduce cleaner intermediate evaluation boundaries.
2. Prepare retained operation graphs for future path operators and text.
3. Keep build, test, and benchmark entry points reliable across platforms.

## Execution Phases

### Phase 0: Baseline and Truth

Purpose:
Create the benchmark and correctness baseline so the project stops guessing.

Required outputs:

1. Real support matrix document generated from fixtures.
2. Benchmark corpus split into:
   - simple shapes
   - gradients
   - masks and mattes
   - repeaters
   - images
   - text
   - effects
   - mixed real product assets
3. ThorVG comparison procedure documented for the same corpus.
4. Golden-image test harness for supported features.

Completion criteria:

- We can state exactly where rlottie is behind ThorVG, with reproducible evidence.

### Phase 1: Immediate Performance Path

Purpose:
Fix the most obvious render-path inefficiencies before large feature work lands.

Priority items:

1. Reduce repeated matte composition in `world_locations`.
2. Add branch-vs-baseline validation before claiming narrow performance wins.
3. Replace remaining clip-sized offscreen surfaces with tight content bounds where possible.
4. Reduce repeated drawable-list creation and repeated path work.
5. Accelerate property lookup for animated channels.
6. Improve repeater evaluation cost.
7. Eliminate nested or fragmented scheduler overhead where it hurts.
8. Harden `VRle` ops for large rows and pathological masks.

Completion criteria:

- Measurable improvement on the CPU benchmark corpus.
- No correctness regressions on masks, mattes, alpha, and gradients.

### Phase 2: High-Value Spec Expansion

Purpose:
Close the most damaging compatibility gaps in descending ROI order.

Priority items:

1. `Merge Paths` stroke semantics and mixed-asset regression coverage
2. `Text` glyph-first pipeline and broader correctness coverage
3. broader layer `Blend Modes` coverage beyond the current mode set, with
   hue/saturation/color family modes next
4. broader `Layer Effects` beyond the current `ADBE Fill` subset
5. soft-mask `Expansion` and `Feather`
6. broader `.lottie` robustness
7. broaden mixed-asset skew regression coverage

Completion criteria:

- The supported asset corpus expands meaningfully.
- README claims are updated only after fixtures pass.

### Phase 3: ThorVG Overtake

Purpose:
Turn localized improvements into a clear, defensible win.

Priority items:

1. Tune on Tizen-class ARM hardware.
2. Compare parse latency, steady-state frame time, p95/p99 frame time, RSS, temporary surface memory, and CPU usage.
3. Close remaining ThorVG support gaps that matter to Samsung product assets.
4. Prepare a publishable comparison report.

Completion criteria:

- `rlottie` exceeds ThorVG on the agreed matrix and benchmarks.

## Success Metrics

### Performance Metrics

Measure all of the following for both `rlottie` and ThorVG:

- parse-to-first-frame latency
- steady-state ms/frame
- p95 and p99 frame time
- frames per second under fixed workload
- peak RSS
- temporary surface memory
- CPU usage
- power impact on target hardware

### Support Metrics

Track support as fixture-backed categories:

- fully supported
- partially supported
- parsed but rendered incorrectly
- unsupported

README support claims must be derived from this matrix, not handwritten from memory.

## Immediate Next Tasks

The next execution batch should be:

1. Build a fixture-backed support matrix under `test/fixtures` or equivalent.
2. Replace the current `100x100`-biased benchmark with a corpus and multiple resolutions.
3. Implement tight-bounds offscreen composition for matte and alpha-heavy paths.
4. Add property-segment caching for animated channels.
5. Start `Skew` support because it is high-value and lower-cost than `Text` or `Merge Paths`.

## Non-Negotiable Constraints

- Do not declare support without a fixture.
- Do not declare optimization without a benchmark delta.
- Do not regress Tizen-oriented build portability to satisfy desktop-only environments.
- Do not let README drift ahead of tested implementation.

## Definition of Done for This Plan

This plan remains active until `rlottie` has:

1. A tested support matrix that beats ThorVG on the target corpus.
2. A benchmark report showing a Tizen-relevant render-performance win.
3. A code structure that can continue growing without repeating the current parser-evaluator-render coupling problems.
