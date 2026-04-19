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

- `rlottie_first_frame.png`
- `thorvg_first_frame.png`
- `diff_first_frame.png`
- `side_by_side.png`
- `metrics.json`

The image-level tool is now the preferred adjudication path for concentrated
correctness gaps such as `32266.json`, `R_QPKIVi.json`, `43391.json`,
`stroke_dash.json`, and `expressions/layereffect.json`.

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
is the active gating signal for heartbeat work:

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

- `expressions/world_locations.json`: `0.476 ms` vs `0.223 ms`
- `11555.json`: `1.514 ms` vs `1.288 ms`
- `confetti.json`: `0.233 ms` vs `0.110 ms`
- `threads.json`: `1.961 ms` vs `1.888 ms`
- `text_anim.json`: `0.125 ms` vs `0.084 ms`
- `stroke_dash.json`: `0.153 ms` vs `0.121 ms`

The same run confirms that `32266.json` is not a performance problem first.
It is parse-heavy and still needs correctness adjudication:

- parse: `15.883 ms` vs `0.789 ms`
- steady-state: `0.167 ms` vs `0.406 ms`

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
`stroke_dash.json` and `expressions/layereffect.json` now render again after
parser hardening, but they still belong in the layer-effects bucket because
the remaining gap is real text, expression controls, and broader effect
coverage rather than pure zero-output failure.

Recent matte work:

- `expressions/world_locations.json` now preprocesses positive matte pairs
  against tighter current-frame source bounds, while skipping layers whose
  mask semantics depend on the full clip rectangle.
- That change moved the median steady-state cost from `0.592 ms` down to
  `0.476 ms`.
- The same change cut `render_matte_ms` in the steady profile from
  `18.67 ms` to `15.95 ms` over the 30-frame loop without changing the
  first-frame adjudication result (`0.999722` exact-match ratio).

Recent correctness fix:

- module-mode image loading now restores nonzero output on
  `image_embedded.json` and closes the previous zero-output regression on
  `32266.json`
