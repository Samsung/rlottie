# Benchmarks

This directory holds the benchmark corpus and workflow used to compare `rlottie`
against ThorVG.

## Current Intent

The immediate goal is to stop relying on the old `example/lottieperf.cpp`
benchmark, which is biased toward a fixed `100x100` render size and a broad,
undifferentiated asset pool.

The new benchmark flow should track:

- parse-to-first-frame latency
- steady-state render time
- multiple resolutions
- explicit asset buckets
- the same corpus across `rlottie` and ThorVG

## Corpus File

`tizen_cpu_corpus.txt` is the starter corpus for CPU-oriented Tizen workloads.
Each line is relative to `example/resource/`.

Targeted support fixtures can live beside the corpus without joining the main
CPU benchmark set immediately. Current transform fixtures include
`skew_rect_x.json`, `skew_rect_y.json`, `skew_repeater_x.json`, and
`skew_rect_3d_y.json`. Current path-operator fixtures include
`merge_paths_intersect_fill.json`, `merge_paths_subtract_fill.json`,
`merge_paths_exclude_fill.json`, and `merge_paths_intersect_gfill.json`.
Current blend-mode fixtures include `blendmode_multiply.json`,
`blendmode_screen.json`, `blendmode_overlay.json`, `blendmode_darken.json`,
`blendmode_lighten.json`, `blendmode_color_dodge.json`,
`blendmode_color_burn.json`, `blendmode_hardlight.json`,
`blendmode_softlight.json`, `blendmode_difference.json`,
`blendmode_exclusion.json`, `blendmode_hue.json`,
`blendmode_saturation.json`, `blendmode_color.json`, and
`blendmode_luminosity.json`. Current layer-effect fixtures include
`layer_effect_fill_solid.json`, `layer_effect_fill_precomp.json`,
`layer_effect_tint_solid.json`, and `layer_effect_tint_precomp.json`.

## Example Usage

```sh
build/example/lottiebench --asset-list benchmarks/tizen_cpu_corpus.txt --size 240x240 --size 360x360
build/example/lottiebench --asset mask.json --size 1080x1080 --async
build/example/lottiebench --asset /abs/path/world_locations.json --size 360x360 --iterations 30 --warmup 3 --profile
```

ThorVG comparison should use the same asset paths, sizes, warmup, and iteration counts.
`--profile` is diagnostic-only: it prints internal rlottie hotspot timing to
`stderr`, and the timing probes materially perturb steady-state frame time, so
do not use it when recording comparison numbers.

## ThorVG Example Comparison

`thorvg_example_smoke.txt` is a representative subset of
`../thorvg.example/res/lottie/` for repeatable smoke comparisons across:

- general shapes
- gradients
- masks and mattes
- merge paths
- dash stroke
- text
- expression-heavy assets

Use the built-in comparison harness to benchmark `rlottie` and `thorvg`
against the same asset set, render size, and loop counts:

```sh
python3 benchmarks/compare_lottie_engines.py \
  --asset-dir ../thorvg.example/res/lottie \
  --asset-list benchmarks/thorvg_example_smoke.txt \
  --size 360x360 \
  --iterations 30 \
  --warmup 3 \
  --trials 5 \
  --threads 1
```

The comparison tool emits per-engine CSV rows with:

- parse time
- first-frame latency
- steady-state average frame time
- resident RSS after parse, first frame, and steady-state loop
- first-frame pixel signature for coarse validation
- trial count
- median, min, and max for the main timing and RSS metrics

The benchmark validity rules are now:

- always pin ThorVG thread count explicitly instead of relying on host-core defaults
- use multiple trials instead of a single run
- alternate engine execution order between trials to reduce warm-cache and thermal bias
- use median-case results for ranking, not one noisy sample
- use image-level adjudication for top mismatches instead of coarse signatures alone

The comparison script now defaults to `--threads 1` and `--trials 3` so CPU
comparisons do not silently depend on the workstation's hardware concurrency.

For a corpus-wide audit that summarizes load failures, coarse render
signature mismatches, and the largest rlottie losses against ThorVG:

```sh
python3 benchmarks/audit_thorvg_corpus.py \
  --asset-dir ../thorvg.example/res/lottie \
  --iterations 10 \
  --warmup 2 \
  --trials 3 \
  --threads 1 \
  --size 360x360
```

This wrapper still uses `compare_lottie_engines.py` underneath, but it adds a
direct triage summary for unsupported assets and the top parse, first-frame,
steady-state, and RSS gaps.

For top mismatches where a coarse signature is not enough, dump and compare the
first rendered frame directly:

```sh
python3 benchmarks/adjudicate_lottie_frames.py \
  --asset ../thorvg.example/res/lottie/32266.json \
  --size 360x360 \
  --output-dir /tmp/rlottie_adjudicate_32266
```

This workflow writes:

- `rlottie_frame_<N>.png`
- `thorvg_frame_<N>.png`
- `diff_frame_<N>.png`
- `side_by_side.png`
- `metrics.json`

The image-level tool is now the preferred adjudication path for concentrated
correctness gaps such as `32266.json`, `R_QPKIVi.json`, `43391.json`,
`stroke_dash.json`, and `expressions/layereffect.json`.

Do not assume frame 0 is enough for effect-heavy assets. The script now accepts
`--frame <index>` so the same asset can be compared at an arbitrary frame:

```sh
python3 benchmarks/adjudicate_lottie_frames.py \
  --asset /abs/path/stroke_dash.json \
  --size 360x360 \
  --frame 12 \
  --output-dir /tmp/stroke_dash_frame12
```

## Local rlottie-vs-rlottie Comparison

When a candidate optimization is noisy or too narrow to trust from a single
ThorVG smoke pass, compare the current build directly against a baseline
`lottiebench` binary from another branch or worktree:

```sh
python3 benchmarks/compare_rlottie_builds.py \
  --current-bin build/example/lottiebench \
  --baseline-bin /Users/junsu/Documents/rlottie-heartbeat/build/example/lottiebench \
  --asset-dir ../thorvg.example/res/lottie \
  --asset-list benchmarks/thorvg_example_smoke.txt \
  --size 360x360 \
  --iterations 60 \
  --warmup 5
```

This is especially useful for validating branch-local changes before deciding
whether they deserve a ThorVG comparison run and a permanent commit.

## Current Priority Signal

The current `360x360`, `30` iteration smoke run is not the final answer, but it
is the active gating signal for optimization work:

- `rlottie` currently leads on parse latency, first-frame latency, and steady RSS
- steady-state frame time is still behind overall
- the largest current steady-state loss is
  `expressions/world_locations.json`, and internal profiling points to
  `render_matte` as the dominant hotspot

Heartbeat-driven work should follow the active backlog in
`docs/THORVG_COMPETITION_PLAN.md` rather than picking speculative work at random.

## Current Priority Snapshot

On the current median-of-5 comparison for the main lagging assets at
`360x360`, `30` iterations, `3` warmup, and `1` ThorVG thread, the main
steady-state losses are:

- `threads.json`: `2.043 ms` vs `1.996 ms`
- `stroke_dash.json`: `0.169 ms` vs `0.126 ms`
- `text_anim.json` and `textblock.json` remain larger steady-state losses in
  the broader corpus and stay in the outlined-scene bucket
- `textrange.json` is already faster in `rlottie`, so it remains a correctness
  target rather than a steady-state target

Representative wins after the latest static drawable-list reuse:

- `expressions/world_locations.json`: `0.059 ms` vs `0.236 ms`
- `11555.json`: `0.091 ms` vs `1.361 ms`
- `confetti.json`: `0.089 ms` vs `0.113 ms`

The same run confirms that `32266.json` is not a performance problem first.
It is parse-heavy and still needs correctness adjudication:

- parse: `14.828 ms` vs `0.841 ms`
- steady-state: `0.191 ms` vs `0.434 ms`

Recent cold-review note:

- recursive precomp `coverageBounds()`, broad `contentStatic` skipping, and a
  translation-only RLE reuse experiment for `11555.json` /
  `confetti.json` / `threads.json` were rejected because they did not survive
  the benchmark workflow
- a narrow whole-layer `ADBE 4ColorGradient` approximation for
  `stroke_dash.json` was also rejected because frame-0 and late-frame
  adjudication both moved farther away from ThorVG
- the current surviving `world_locations.json` optimizations are:
  `ShapeLayer` alpha offscreen clip tightening plus direct-alpha matte support
  for single translucent solid-fill sources, and a recursive direct-alpha
  matte path for source layers whose alpha can be resolved only through a
  nested child-layer walk; the latest path kept first-frame adjudication
  unchanged and beat the same-machine `HEAD` baseline in repeated A/B medians on
  `world_locations`, `11555`, `confetti`, and `threads`
- the latest surviving transform-bucket optimization is static
  `ShapeLayer` drawable-list reuse: if `contentStatic` is true, the pointer
  list built by `renderList()` is reused across frames instead of rebuilt in
  every `preprocessStage()`. Representative late-frame dumps for
  `world_locations@120`, `11555@160`, `confetti@90`, `threads@90`,
  `stroke_dash@12`, and `textrange@120` all stayed exact-match with the
  `HEAD` baseline while median frame time improved materially

## Current Lagging Buckets

The full-corpus audit should now be interpreted by feature bucket rather than
as one flat ranking:

- mattes and offscreen compositing: `expressions/world_locations.json`
- transform-only rerasterization: `11555.json`, `confetti.json`, `threads.json`
- text/runtime text payloads: `text_anim.json`, `textrange.json`
- layer effects beyond narrow `ADBE Fill` / `ADBE Tint`: `stroke_dash.json`,
  `shutup.json`, `expressions/layereffect.json`
- expressions: `balloons_with_string.json`, `expressions/11272.json`

This grouping matters because the fix strategy is different for each family.
Do not treat all full-corpus losses as generic render slowness.
After the latest `ShapeLayer` drawable-list reuse, `world_locations.json`,
`11555.json`, and `confetti.json` are no longer the best desktop steady-state
targets even though they still matter as Tizen verification assets. The
remaining active lag in this subset is `threads.json`.
`stroke_dash.json` and `expressions/layereffect.json` now render again after
parser hardening. `stroke_dash.json` also has a narrow chars-backed static text
path now, and frame-0/frame-12 adjudication is already close. That means the
remaining gap should not be described as "4-Color Gradient only" without
further evidence. The safer statement is broader effect coverage, expression
controls, and image-level output drift.

Recent matte work:

- `expressions/world_locations.json` now has two matte-focused optimizations:
  tighter positive-matte preprocessing clips and a direct alpha-matte path
  that skips the extra source offscreen when the source layer has no
  blend/effect work.
- That direct path now also accepts nested source layers whose child layers
  can collapse into alpha RLEs, which matches the actual precomp source
  structure inside `world_locations.json`.
- On the current median-of-5 comparison, the asset sits around `0.504 ms`
  steady-state against ThorVG's `0.235 ms`. The gap is still large, but the
  recursive direct path survived correctness checks and remains the current
  best matte-specific optimization on this branch.
- The same-machine `HEAD` baseline A/B medians now sit at
  `world_locations 0.580 -> 0.548 ms`, `11555 1.615 -> 1.507 ms`,
  `confetti 0.201 -> 0.196 ms`, and `threads 2.237 -> 2.124 ms` without
  changing the first-frame adjudication result (`0.999722` exact-match
  ratio). Profile runs remain noisy enough that they should be used only as
  hotspot guides, not as ranking inputs.
- A later narrow `ShapeLayer` snapshot-cache prototype did not survive review.
  It improved same-machine baseline medians on some transform-heavy assets, but
  frame-0 adjudication moved `11555.json` and `threads.json` farther away from
  ThorVG, so the cache path was discarded. Only the affine bitmap-draw helper
  and transform-free `contentStatic` metadata were kept as future enablers.

Recent correctness fix:

- module-mode image loading now restores nonzero output on
  `image_embedded.json` and closes the previous zero-output regression on
  `32266.json`
- shape-object parsing now has an order-independent fallback for the common
  `gr` / `sh` / `fl` / `tr` case where `ty` appears last. This closes the
  parser-side blank-output failure on the new
  `example/resource/shape_group_ty_last.json` fixture and moves
  `R_QPKIVi.json` out of the previous zero-pixel state, even though the frame
  still diverges heavily from ThorVG.
