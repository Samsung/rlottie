# rlottie 대 ThorVG 경쟁 현황 (한글)

이 문서는 `rlottie`의 현재 구현 스펙, ThorVG 대비 부족 스펙, 우선 구현 대상, 그리고 최신 성능 측정 결과를 한글로 정리한 운영 문서다.

## 측정 기준

| 항목 | 값 |
| --- | --- |
| 비교 엔진 | `build/example/lottiebench` vs `build/example/thorvgbench` |
| 코퍼스 | `benchmarks/thorvg_example_smoke.txt` + backlog 추가 자산(`threads.json`, `32266.json`, `R_QPKIVi.json`, `43391.json`) |
| 해상도 | `360x360` |
| 반복 | `30 iterations`, `3 warmup`, `5 trials` |
| 비교 기준 | median-case, ThorVG `--threads 1` 고정 |
| correctness 판정 | `benchmarks/adjudicate_lottie_frames.py` frame 지정 image adjudication |

## 최신 반영 사항

| 항목 | 현재 상태 |
| --- | --- |
| `ty`-last shape parser | `gr/sh/fl/tr` shape object에서 `ty`가 마지막에 와도 payload를 잃지 않도록 parser fallback 추가 |
| 회귀 fixture | `example/resource/shape_group_ty_last.json` 추가. `100x100` 기준 `nonzero_pixels=1600`으로 정상 렌더 확인 |
| `ADBE 4ColorGradient` | narrow whole-layer bitmap postprocess를 추가했다. 현재 지원 범위는 static point/color/opacity + `Blend=100`, `Jitter=0`, `Blending Mode=1` 기본값 케이스뿐이다. synthetic fixture `layer_effect_4color_gradient_solid.json`의 코너 샘플은 `blue / white / red / green`으로 분리된다 |
| `ADBE 4ColorGradient` 수학 교체 | default-case `4ColorGradient`의 색 필드를 inverse-distance 가중치에서 quad bilinear sampler로 바꿨다. `stroke_dash.json` same-machine baseline 대비 median steady-state는 `0.265 ms -> 0.194 ms`로 줄었지만, ThorVG 대비 frame 0 exact match는 `0.951435 -> 0.951381`, frame 12 exact match는 `0.949159 -> 0.949066`으로 아주 미세하게만 나빠졌다 |
| stroke/effect hot-path profiling | `threads.json`과 `stroke_dash.json`을 위해 내부 profile counter를 `rasterStrokeSetup`, `rasterRender`, `drawRleSolid`, `drawRleGradient`, `drawRleTexture`까지 확장했다. 최신 profile상 `threads`는 `trim`이 아니라 `raster_render`가 절대 다수이고, `stroke_dash`는 `raster_stroke > bitmap_effect > dash_apply` 순으로 비용이 갈린다 |
| static `4ColorGradient` color-map cache | 정적인 `4ColorGradient` point/color 레이어는 bilinear field를 매 프레임 다시 풀지 않고 color map을 캐시해 재사용하도록 바꿨다. `stroke_dash.json` same-machine profile run은 `avg_frame_ms 0.451 -> 0.232`까지 줄었고, hardened median-of-5도 `0.272 ms`까지 내려왔다. 다만 ThorVG `0.126 ms`보다는 아직 느리다 |
| `R_QPKIVi.json` 상태 변화 | blank-output 단계는 이미 벗어났다. 이번 배치에서 single solid-fill shape layer는 layer alpha를 drawable 쪽으로 접고 offscreen을 생략하도록 바꿨다. exact match는 여전히 `0`이지만 full-frame compositing drift는 확실히 줄었다 |
| `world_locations.json` matte 경로 | 기존 `ShapeLayer` alpha offscreen clip tightening 위에, nested child-layer walk가 필요한 source에서 source offscreen을 건너뛰는 recursive direct-alpha matte 경로를 추가했다. `first-frame` 정합성은 그대로 유지된다 |
| static `ShapeLayer` drawable-list 재사용 | `contentStatic`가 참인 `ShapeLayer`는 drawable pointer list 구조가 프레임마다 바뀌지 않는다는 점을 이용해, `preprocessStage()`에서 render-list 재구성을 매 프레임 반복하지 않도록 바꿨다 |
| cached raster clip bounds | `VDrawable::setPath()`에서 path bounds를 한 번 계산해 저장하고, raster 단계에서는 full clip 대신 그 bounds 기반 tight clip을 사용한다. 이번 배치에서는 clip과 padded bounds가 전혀 겹치지 않으면 empty RLE만 만들고 실제 raster task는 건너뛰도록 좁혔다 |
| 최신 성능 판정 | 이번 변경 뒤 representative late-frame dump는 `threads@90`, `text_anim@120`, `11555@160`, `stroke_dash@12`, `confetti@30`, `world_locations@120` 모두 `HEAD` baseline과 exact match `1.0`이었다. 최신 median-of-5 기준 desktop steady-state 열세는 `threads`, `stroke_dash`, outlined text scene 쪽으로 더 좁혀졌고, `world_locations`, `11555`, `confetti`, `textrange`는 rlottie 우세를 유지했다 |
| transform-cache 선행 작업 | `VPainter`에 affine bitmap draw helper를 추가했고, `model::Layer`에는 layer transform과 분리된 `contentStatic` 메타데이터를 넣었다. 여기에 더해 static `ShapeLayer` drawable-list 재사용은 실제 benchmark를 통과해 남겼다. 반면 narrow `ShapeLayer` snapshot cache 프로토타입은 `11555.json`, `threads.json`에서 ThorVG image adjudication을 악화시켜 버렸다 |
| `43391.json` 시도 결과 | `Merge Paths::Mode::Merge`를 boolean union 대신 compound-path rasterization으로 바꿔 chained merge case를 부분 복구했다. frame 0 exact match ratio는 `0.778789`까지 올라왔지만, 여전히 correctness backlog다 |
| JSON file loading | `loadFromFile()`가 iterator 기반 텍스트 읽기 대신 single binary read를 사용하도록 바뀌었다. `page_slide.json`, `32266.json` 같은 parse-heavy 자산에서 실측 이득이 확인됐다 |
| embedded image first-frame 경로 | `loadBitmapIfNeeded()`가 embedded image/data URI를 소비할 때 `mImageData`와 `mImagePath`를 추가 복사하지 않고 move로 넘기도록 바뀌었다. `image_embedded.json`은 first-frame median이 `0.1405 ms -> 0.1230 ms`, `rss_first_frame_kb`는 `3600 -> 3264`로 줄었다. 반면 `32266.json` 같은 giant data URI correctness/parse 자산은 의미 있게 달라지지 않았다 |
| `32266.json` 재판정 | lazy image decode + single-read loader 이후에도 parse는 여전히 ThorVG보다 훨씬 느리다. 최신 median은 `14.192 ms -> 0.796 ms(ThorVG)`라서 giant data URI를 포함한 JSON/base64 payload 자체가 여전히 핵심 병목이다 |
| rejected 실험 | recursive precomp `coverageBounds()`, `11555/confetti/threads`용 translation-only RLE 재사용, world-space snapshot cache, broad `contentStatic` skip은 모두 benchmark/adjudication에서 살아남지 못해 버렸다 |

## 스펙 지원 현황 및 backlog

| 분류 | 스펙/기능 | rlottie 상태 | ThorVG 관찰 상태 | 현재 판단 | 다음 작업 |
| --- | --- | --- | --- | --- | --- |
| 구현 | Skew / Skew Axis | 지원 | 지원 관찰 | fixture 기반 검증 완료 | mixed asset 회귀 검증 확대 |
| 구현 | Merge Paths Fill / Gradient Fill | 지원 | 지원 관찰 | Intersect/Subtract/Exclude와 `Merge` compound-path rasterization까지 동작 | stroke semantics 별도 구현 |
| 구현 | Layer Blend Modes | 지원 | 지원 관찰 | Multiply~Luminosity까지 소프트웨어 경로 구현 | 실자산 image adjudication 확대 |
| 구현 | ADBE Fill / Tint | 좁은 지원 | 지원 관찰 | whole-layer bitmap postprocess | Stroke / stack allowlist 확대 |
| 구현 | 정적 chars 기반 Text | 좁은 지원 | 지원 관찰 | static layer t + fonts/chars -> shape 변환 | animated text / animator 미구현 |
| 구현 | .lottie manifest 경로 선택 | 지원 | 지원 관찰 | archive 선택 호환성 확보 | 브로드 코퍼스 확대 |
| 구현 | fractional size parser | 지원 | 지원 관찰 | text-heavy asset 로드 복구 | 추가 회귀 자산 확대 |
| 구현 | module image loading | 지원 | 지원 관찰 | 32266 zero-output 복구 기여 | correctness drift 추가 수정 |
| 부분 지원 | ADBE 4ColorGradient | 좁은 지원 | 지원 관찰 | 현재는 whole-layer bitmap postprocess 기반의 default-case만 처리한다. static point/color/opacity와 `Blend=100`, `Jitter=0`, `Blending Mode=1`만 허용한다 | true effect semantics, broader stack, animated params 확장 |
| 부분 지원 | Layer Effect Stroke | 미지원 | 지원 관찰 | Fill/Tint 이후 다음 단계 | alpha silhouette 기반 narrow path |
| 부분 지원 | Merge Paths Stroke | 미지원 | 지원 관찰 | fill은 되지만 stroke semantics 부족 | stroke outline 후 boolean 또는 path boolean backend |
| 부분 지원 | Animated text document (t.d.k) | 좁은 지원 | 지원 관찰 | hold-style document switch는 chars-backed 경로에서 지원되지만 text animator/path는 아직 없다 | animated document 범위 확대와 runtime text 경로 분리 |
| 부분 지원 | Text Animator / Range Selector | 미지원 | 지원 관찰 | textrange opacity animator 미구현 | selector subset evaluator |
| 부분 지원 | 전용 Text Renderer / Glyph Cache | 미지원 | 지원 관찰 | 현재는 parser-time lowering만 존재 | Layer::Type::Text 경로 필요 |
| 부분 지원 | Layer Effect Stack | 제한적 | 지원 관찰 | enabled narrow siblings만 허용 | mixed enabled stack allowlist |
| 미지원 | Expressions Subset | 미지원 | 부분 지원 관찰 | balloons_with_string, expressions/11272 backlog | subset evaluator + unsupported diagnostics |
| 미지원 | Soft Mask Feather / Expansion | 미지원 | 지원 관찰 | hard mask 중심 | mask family 확장 |
| 미지원 | Transform-only Snapshot Cache | 미구현 | 구현 존재 | 11555/confetti/threads의 성능 gap 원인 | content/transform dirty 분리 후 local-space cache |
| 미지원 | Matte 재사용 / bounds 전파 완성 | 부분 구현 | 구현 존재 | world_locations의 성능 gap 원인 | inherited bounds / matte reuse 추가 |

## 핵심 판단

| 항목 | 현재 결론 |
| --- | --- |
| `stroke_dash.json` | 정적 title text는 복구됐고, default-case `ADBE 4ColorGradient`도 bilinear sampler + 정적 color-map cache로 돈다. same-machine baseline 대비 steady-state는 크게 줄었지만 ThorVG 대비 frame 12 exact match는 여전히 `0.949066` 수준이고, hardened median-of-5도 `0.272 ms vs 0.126 ms`라 아직 느리다. 즉 성능은 더 좋아졌지만 effect semantics와 broader stack은 아직 미완이다. |
| `textrange.json` | 성능은 이미 ThorVG보다 빠르다. 남은 핵심 gap은 animated `t.d.k` 전체가 아니라 runtime text 경로의 range-selector opacity animator와 layout/baseline 정합성이다. |
| `text_document_switch.json` | narrow hold-style `t.d.k` 회귀 fixture다. chars-backed glyph path만으로 frame switch가 실제로 일어나고 baseline blank-output을 벗어났지만, 이건 text animator/path가 없는 subset에만 해당한다. |
| `text_anim.json` | runtime text가 아니라 outlined shape scene이다. real text 완성의 근거로 쓰면 안 된다. |
| `32266.json` | steady-state보다 correctness + parse 이슈가 더 크다. first-frame exact match ratio는 `0.717` 수준이다. |
| `world_locations.json` | correctness 문제는 사실상 아니다. matte/direct-alpha 경로와 drawable-list 재사용 위에, 이번에는 raster bounds 바깥 drawables를 실제 rasterize하지 않도록 줄여서 current desktop median이 더 내려갔다. 다만 Tizen 실기기에서도 같은 결과가 유지되는지는 아직 검증이 필요하다. |
| `11555/confetti` | reusable subgraph / transform 계열 진단은 여전히 유효하다. `11555`는 여전히 크게 앞서고 `confetti`도 desktop median에서 rlottie 우세다. 이번 low-level raster skip도 두 자산에서 baseline 대비 추가 이득을 보였다. |
| `threads.json` | pure transform bucket으로 보기 어렵다. 최신 hot-path profile 기준 steady 30-frame 누적이 `trim_update_ms=2.17`, `raster_stroke_setup_ms=6.05`, `raster_render_ms=59.75`라서 실제 병목은 trim이 아니라 stroke raster render 자체다. gradient brush 도색은 `draw_rle_gradient_ms=2.31` 수준이라 주원인이 아니다. |
| expression bucket | `10444`는 direct layer transform alias, `10416`은 transform alias + path alias + `loopOut()`, `11272`는 `content(...).innerRadius` 참조, `16447`는 direct alias + wiggle류 effect expression으로 갈린다. 반면 `balloons_with_string.json`은 명시적 `x` expression string이 없어, 실제로는 pseudo-control/rig 계열 correctness backlog에 더 가깝다. |
| transform-cache 프로토타입 | world-space snapshot 재투영만으로는 충분하지 않았다. `11555.json`과 `threads.json`은 baseline보다 빨라졌지만 ThorVG image adjudication에서는 오히려 더 멀어졌다. 현재 남겨둔 건 affine bitmap helper와 `contentStatic` 메타데이터뿐이고, 다음 시도도 pure transform 자산에 한정해야 한다. |
| `R_QPKIVi.json` | `ty`가 마지막인 shape object parser blank는 닫혔다. 이번 배치에서 single solid-fill shape layer alpha를 drawable 쪽으로 접으면서 frame 0 drift가 더 줄었다. exact match는 여전히 `0`이지만 mean abs diff RGB는 `[0.1539, 1.1118, 0.1537] -> [0.0960, 0.9815, 0.0505]`로 개선됐다. |
| `43391.json` | chained `Merge Paths` semantics 수정으로 큰 빈 구멍은 줄었지만 아직 틀린 영역이 남는다. 현재는 추가 merge-path chain semantics와 stroke semantics를 같이 봐야 한다. |

## 리소스별 성능 비교

| 리소스 | 기능군 | rlottie parse (ms) | ThorVG parse (ms) | rlottie frame (ms) | ThorVG frame (ms) | frame 배수 (rlottie/ThorVG) | rlottie RSS (KB) | ThorVG RSS (KB) | 판정 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| world_locations.json | matte/offscreen | 0.245 | 0.417 | 0.061 | 0.219 | 0.28x | 4480 | 5024 | rlottie 우세 |
| 11555.json | transform cache | 0.171 | 0.391 | 0.084 | 1.265 | 0.07x | 4080 | 3648 | rlottie 우세 |
| confetti.json | reusable subgraph | 1.393 | 0.561 | 0.077 | 0.099 | 0.77x | 3856 | 4368 | rlottie 우세 |
| threads.json | trim + stroke raster | 0.105 | 0.429 | 1.903 | 1.837 | 1.04x | 5200 | 3648 | ThorVG 우세, 실제 병목은 trim이 아니라 `raster_render` |
| text_anim.json | outlined text scene | 1.078 | 0.501 | 0.115 | 0.084 | 1.37x | 3936 | 4160 | ThorVG 우세 |
| stroke_dash.json | real text + effect | 0.121 | 0.398 | 0.272 | 0.126 | 2.16x | 4256 | 3392 | ThorVG 우세, bilinear `4ColorGradient`에 정적 color-map cache를 더해도 아직 effect semantics와 broader stack이 남는다 |
| textrange.json | real text animator | 0.113 | 0.338 | 0.009 | 0.022 | 0.39x | 2784 | 3344 | rlottie 우세 |
| textblock.json | outlined shape text | 4.496 | 1.040 | 0.849 | 0.318 | 2.67x | 8592 | 7584 | ThorVG 우세 |
| 32266.json | correctness + parse | 14.192 | 0.796 | 0.174 | 0.392 | 0.44x | 22608 | 15904 | rlottie 우세 |
| layereffect.json | layer effect | 0.174 | 0.389 | 0.115 | 0.174 | 0.66x | 4080 | 3856 | rlottie 우세 |
| abstract_circle.json | basic shape | 0.072 | 0.415 | 0.136 | 0.379 | 0.36x | 3888 | 3952 | rlottie 우세 |
| glow_loading.json | fill/opacity | 0.130 | 0.404 | 0.011 | 0.033 | 0.32x | 3056 | 3184 | rlottie 우세 |
| gradient_sleepy_loader.json | gradient | 0.075 | 0.408 | 0.126 | 0.118 | 1.06x | 3040 | 3264 | ThorVG 우세 |
| masking.json | mask/matte | 0.559 | 0.451 | 0.172 | 0.275 | 0.63x | 4816 | 4336 | rlottie 우세 |
| merging_shapes.json | merge paths | 0.126 | 0.403 | 0.064 | 0.050 | 1.28x | 3104 | 3328 | ThorVG 우세 |
| polystar_anim.json | polystar | 0.052 | 0.387 | 0.154 | 0.117 | 1.32x | 4336 | 3328 | ThorVG 우세 |
| starts_transparent.json | alpha/transparency | 0.897 | 0.495 | 0.083 | 0.102 | 0.82x | 4496 | 4016 | rlottie 우세 |
| windmill.json | repeater | 0.205 | 0.399 | 0.070 | 0.109 | 0.64x | 4128 | 3696 | rlottie 우세 |

## 기능 버킷별 평균 성능 비교

| 기능 버킷 | 대표 자산 | rlottie parse 평균 (ms) | ThorVG parse 평균 (ms) | rlottie frame 평균 (ms) | ThorVG frame 평균 (ms) | frame 배수 | rlottie RSS 평균 (KB) | ThorVG RSS 평균 (KB) | 해석 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| matte/offscreen | world_locations.json | 0.245 | 0.417 | 0.061 | 0.219 | 0.28x | 4480 | 5024 | rlottie 우세 |
| reusable subgraph / transform cache | 11555.json, confetti.json | 0.782 | 0.476 | 0.080 | 0.682 | 0.12x | 3968 | 4008 | rlottie 우세, 다만 이 버킷 진단을 `threads`까지 넓히면 안 된다 |
| trim + stroke raster | threads.json | 0.105 | 0.429 | 1.903 | 1.837 | 1.04x | 5200 | 3648 | 현재 desktop steady-state 열세의 핵심. trim보다 `raster_render`가 본체다 |
| outlined text scene | text_anim.json, textblock.json | 2.787 | 0.771 | 0.482 | 0.201 | 2.40x | 6264 | 5872 | ThorVG 우세 |
| real text / text animator | stroke_dash.json, textrange.json | 0.117 | 0.368 | 0.141 | 0.076 | 1.86x | 3520 | 3368 | ThorVG 우세, `stroke_dash`는 캐시로 빨라졌지만 여전히 effect semantics와 stack coverage가 부족하다 |
| layer effect | layereffect.json | 0.174 | 0.389 | 0.115 | 0.174 | 0.66x | 4080 | 3856 | rlottie 우세 |
| merge paths | merging_shapes.json | 0.126 | 0.403 | 0.064 | 0.050 | 1.28x | 3104 | 3328 | ThorVG 우세 |
| basic vector | abstract_circle.json, windmill.json, glow_loading.json, gradient_sleepy_loader.json, polystar_anim.json | 0.107 | 0.403 | 0.099 | 0.151 | 0.66x | 3689 | 3484 | rlottie 우세 |
| correctness + parse | 32266.json, R_QPKIVi.json, 43391.json | 5.041 | 0.598 | 0.170 | 0.210 | 0.81x | 10112 | 7947 | steady-state는 오히려 rlottie가 앞서지만 parse와 correctness는 여전히 rlottie 열세 |

## 현재 우선순위

| 우선순위 | 대상 | 이유 | 바로 할 작업 |
| --- | --- | --- | --- |
| 1 | `threads.json` | 최신 audit 기준 trim + animated-path + stroke raster 병목이 남아 있다. 이번 low-level raster skip으로도 아직 ThorVG보다 느리다 | trim/stroke geometry reuse seam을 다시 찾고 raster backend 측 재사용 지점을 확인 |
| 2 | `stroke_dash.json` | bilinear `4ColorGradient`로 baseline 대비 성능은 줄였지만 ThorVG보다 아직 느리고 exact match도 더 좋지 않다 | true `4ColorGradient` semantics와 broader effect stack을 분리해서 다시 설계 |
| 3 | expression bucket (`10444`, `10416`, `11272`, `16447`) | full engine 없이도 direct alias / path alias / simple math / loop류 subset을 먼저 닫을 수 있다 | subset evaluator와 unsupported diagnostics를 분리해서 좁게 넣기 |
| 4 | `text_anim.json`, `textblock.json`, `textrange.json` | outlined scene와 runtime text correctness가 아직 섞여 있다 | outlined bucket 최적화와 runtime selector subset을 분리하고, hold-style `t.d.k`에서 animated document subset을 더 넓힌다 |
| 5 | `32266.json`, `R_QPKIVi.json`, `43391.json`, `27746-joypixels-partying-face-emoji-animation.json` | 성능보다 correctness drift가 더 큰 축이다 | image-precomp drift, non-opaque compositing, chained merge semantics, pseudo-control rig를 각각 분리 |

## 주의할 점

- `text_anim.json`, `textblock.json`을 보고 real text 지원이 됐다고 판단하면 안 된다.
- `32266.json`은 steady-state 성능 타깃으로 보면 우선순위를 잘못 잡게 된다.
- `world_locations.json`은 image-level로 이미 거의 맞았고, desktop median에서도 ThorVG보다 빨라졌다. 지금은 active hotspot이라기보다 Tizen 재검증 대상이다.
- `world_locations`와 `11555/confetti/threads`에 대해 speculative한 matte/cache 경로를 억지로 유지하면 오히려 median frame time이 악화된다. 현재 문서에는 benchmark를 통과한 경로만 남긴다.
- narrow `ShapeLayer` snapshot cache는 baseline steady-state를 일부 줄였지만 `11555.json`과 `threads.json`에서 baseline 대비 frame 0 drift가 커졌다. 그래서 코드에는 남기지 않고, affine bitmap draw helper와 `contentStatic` 메타데이터만 선행 작업으로 유지했다.
- 이번 path-bounds raster clip 계열은 representative late-frame dump 여섯 개(`threads@90`, `text_anim@120`, `11555@160`, `stroke_dash@12`, `confetti@30`, `world_locations@120`)에서 모두 baseline exact match를 유지한 채, bounds 밖 drawables는 empty RLE로 조기 종료하도록 더 좁혀 same-machine `HEAD` baseline 대비 `world_locations 0.085 -> 0.080 ms`, `11555 0.190 -> 0.093 ms`, `confetti 0.085 -> 0.081 ms`, `threads 2.072 -> 2.006 ms`, `stroke_dash 0.216 -> 0.196 ms`, `text_anim 0.162 -> 0.122 ms`, `textrange 0.030 -> 0.007 ms`까지 줄였다.
- `stroke_dash.json`은 bilinear `4ColorGradient`에 정적 color-map cache까지 더해 steady-state는 더 줄었지만, frame 12 판정은 여전히 `0.949066` 수준이다. 지금은 “성능 개선은 됐지만 effect semantics는 아직 틀리다”로 보는 게 맞다.
- `threads.json`은 trim 최적화를 더 파는 방향이 틀렸다는 게 계측으로 확인됐다. 현재는 `raster_render`와 stroke geometry reuse를 직접 건드려야 한다.
- `R_QPKIVi.json`은 이제 blank도 아니고 catastrophic miss도 아니다. single solid-fill layer alpha를 inline한 뒤에도 exact match는 `0`이지만, 이건 여전히 전면적인 작은 compositing drift 문제라는 뜻이다.
- `43391.json`은 parser fallback 문제가 아니라 merge-path chain semantics 문제였다. 다만 이번 수정으로도 exact match가 `0.779` 수준이라, 남은 drift를 과소평가하면 안 된다.
