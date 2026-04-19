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
  --warmup 3
```

The comparison tool emits per-engine CSV rows with:

- parse time
- first-frame latency
- steady-state average frame time
- resident RSS after parse, first frame, and steady-state loop
- first-frame pixel signature for coarse validation

If ThorVG thread count needs to be controlled explicitly, pass `--threads <n>`
to the comparison script and it will forward that value to `thorvgbench`.

For a corpus-wide audit that summarizes load failures, coarse render
signature mismatches, and the largest rlottie losses against ThorVG:

```sh
python3 benchmarks/audit_thorvg_corpus.py \
  --asset-dir ../thorvg.example/res/lottie \
  --iterations 10 \
  --warmup 2 \
  --size 360x360
```

This wrapper still uses `compare_lottie_engines.py` underneath, but it adds a
direct triage summary for unsupported assets and the top parse, first-frame,
steady-state, and RSS gaps.

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

Recent correctness fix:

- module-mode image loading now restores nonzero output on
  `image_embedded.json` and closes the previous zero-output regression on
  `32266.json`
