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
- `.lottie` loading improved to select animation JSON from manifest-aware archive paths.
- Large-row `VRle` merge path no longer depends on a fixed 1024-byte scratch buffer.
- `ShapeLayer` now avoids one obvious repeated drawable-list rebuild path.
- `Skew` / `Skew Axis` parsing and transform composition now work on targeted
  group-transform, repeater, and 3D layer fixtures.
- Layer `Blend Modes` now have fixture-backed software-render support for
  `Multiply`, `Screen`, `Overlay`, `Darken`, `Lighten`, `Color Dodge`,
  `Color Burn`, `Hard Light`, `Soft Light`, `Difference`, and `Exclusion`.
- Offscreen blend composition now supports logical draw regions, so blend
  layers can render into tight temporary surfaces instead of full clip-sized
  buffers.
- `thorvgbench` and `compare_lottie_engines.py` now provide direct
  `rlottie` vs `thorvg` comparison on shared JSON corpora, including steady
  time, first-frame time, parse time, resident RSS snapshots, and first-frame
  pixel signatures.
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
- Current benchmark is not representative enough to decide whether we are beating ThorVG.
- The support table in `README.md` is stale relative to the implementation.

## Critical Review Summary

### Strengths

- Core shape rendering path is already competent for shape, trim, repeater, masks, mattes, gradients, images, precomps, and markers.
- Parser is already in-situ and arena-based, so raw JSON tokenization is not the first-order bottleneck.
- The codebase already has CPU-oriented software rasterization and SIMD entry points, which is useful for Tizen-first tuning.

### Major Feature Gaps

- `Text` is effectively unsupported.
- `Merge Paths` now has fixture-backed fill/gradient-fill support for boolean
  modes, and static merge RLE no longer gets recomputed every frame, but stroke
  semantics still fall back to path concatenation and need a real path-boolean
  backend to close the gap fully.
- `Skew` and `Skew Axis` now have fixture-backed coverage on base transforms,
  repeaters, and 3D layers, but they still need broader mixed-asset regression
  coverage before the gap is fully closed.
- Layer `Blend Modes` now cover `Multiply`, `Screen`, `Overlay`, `Darken`,
  `Lighten`, `Color Dodge`, `Color Burn`, `Hard Light`, `Soft Light`,
  `Difference`, and `Exclusion` on targeted fixtures, but hue/saturation/color
  family modes and mixed-asset coverage remain open.
- `Layer Effects` are missing.
- Soft-mask features such as `Feather` and `Expansion` are missing.
- `.lottie` support was fragile and still needs broader archive coverage.

### Major Architecture and Performance Problems

- Full-tree frame evaluation remains too eager.
- Offscreen composition is too expensive because alpha, matte, and complex content render through large temporary surfaces.
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
4. `Layer Effects` subset: `Fill`, `Tint`, `Stroke`.
5. Soft-mask features: `Expansion`, `Feather`.
6. broader mixed-asset skew regression coverage.

### Group B: Render Performance

Owner responsibilities:

- Reduce frame time on CPU raster workloads.
- Shrink offscreen rendering costs.
- Simplify runtime evaluation and invalidation behavior.

Primary targets:

1. Tight-bounds offscreen composition for alpha and matte paths.
2. Better frame-to-frame dirty propagation.
3. Property evaluation acceleration with segment cursors or indexed search.
4. Repeater lazy materialization.
5. Scheduler simplification or unification.
6. More useful cache behavior for surfaces, paths, and repeated frame work.

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

1. Replace clip-sized offscreen surfaces with tight content bounds where possible.
2. Reduce repeated drawable-list creation and repeated path work.
3. Accelerate property lookup for animated channels.
4. Improve repeater evaluation cost.
5. Eliminate nested or fragmented scheduler overhead where it hurts.
6. Harden `VRle` ops for large rows and pathological masks.

Completion criteria:

- Measurable improvement on the CPU benchmark corpus.
- No correctness regressions on masks, mattes, alpha, and gradients.

### Phase 2: High-Value Spec Expansion

Purpose:
Close the most damaging compatibility gaps in descending ROI order.

Priority items:

1. `Merge Paths` stroke semantics and mixed-asset regression coverage
2. broader layer `Blend Modes` coverage beyond the current mode set, with
   hue/saturation/color family modes next
3. `Text` glyph-first pipeline
4. selected `Layer Effects`
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
