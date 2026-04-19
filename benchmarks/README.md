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
`blendmode_softlight.json`, `blendmode_difference.json`, and
`blendmode_exclusion.json`.

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
