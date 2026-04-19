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
| `R_QPKIVi.json` 상태 변화 | blank-output 단계는 이미 벗어났다. 최신 frame 0은 full-frame에 아주 작은 색/알파 drift가 퍼지는 형태라, parser blank보다 non-opaque shape-layer 합성 drift 의심이 더 크다 |
| `world_locations.json` matte 경로 | 단순 `Alpha/AlphaInv` matte direct path를 `opaque stroke`까지 넓혀, source offscreen 1장을 더 자주 생략하도록 확장 |
| `world_locations.json` 최신 판정 | first-frame adjudication은 여전히 `exact_match_ratio = 0.999722` 유지. hardened median steady-state는 `0.499 ms -> 0.235 ms(ThorVG)`로 아직 뒤지지만, 이전 기준선보다 개선됐다 |
| `43391.json` 시도 결과 | `el` order-independent fallback 실험은 cold review에서 버림. `43391`의 핵심 문제는 parser 한 건보다 `Merge Paths` 체인 semantics일 가능성이 높다 |
| lazy image decode | embedded image asset은 parse 단계에서 디코딩하지 않고 first bitmap access로 지연. `image_embedded.json` 로컬 기준 parse가 대략 `8.18 ms -> 1.93 ms`까지 줄었다 |
| `32266.json` 재판정 | lazy image decode 이후에도 parse는 `16~17 ms`대로 거의 줄지 않았다. 이 자산의 parse 병목은 이미지 decode보다 giant data URI를 포함한 JSON/base64 payload 자체에 더 가깝다 |

## 스펙 지원 현황 및 backlog

| 분류 | 스펙/기능 | rlottie 상태 | ThorVG 관찰 상태 | 현재 판단 | 다음 작업 |
| --- | --- | --- | --- | --- | --- |
| 구현 | Skew / Skew Axis | 지원 | 지원 관찰 | fixture 기반 검증 완료 | mixed asset 회귀 검증 확대 |
| 구현 | Merge Paths Fill / Gradient Fill | 지원 | 지원 관찰 | Intersect/Subtract/Exclude 동작 | stroke semantics 별도 구현 |
| 구현 | Layer Blend Modes | 지원 | 지원 관찰 | Multiply~Luminosity까지 소프트웨어 경로 구현 | 실자산 image adjudication 확대 |
| 구현 | ADBE Fill / Tint | 좁은 지원 | 지원 관찰 | whole-layer bitmap postprocess | Stroke / stack allowlist 확대 |
| 구현 | 정적 chars 기반 Text | 좁은 지원 | 지원 관찰 | static layer t + fonts/chars -> shape 변환 | animated text / animator 미구현 |
| 구현 | .lottie manifest 경로 선택 | 지원 | 지원 관찰 | archive 선택 호환성 확보 | 브로드 코퍼스 확대 |
| 구현 | fractional size parser | 지원 | 지원 관찰 | text-heavy asset 로드 복구 | 추가 회귀 자산 확대 |
| 구현 | module image loading | 지원 | 지원 관찰 | 32266 zero-output 복구 기여 | correctness drift 추가 수정 |
| 부분 지원 | ADBE 4ColorGradient | 미지원 | 지원 관찰 | stroke_dash의 후보 gap이지만 단독 원인으로 확정되지는 않음 | frame 지정 adjudication 후 효과 범위 재확정 |
| 부분 지원 | Layer Effect Stroke | 미지원 | 지원 관찰 | Fill/Tint 이후 다음 단계 | alpha silhouette 기반 narrow path |
| 부분 지원 | Merge Paths Stroke | 미지원 | 지원 관찰 | fill은 되지만 stroke semantics 부족 | stroke outline 후 boolean 또는 path boolean backend |
| 부분 지원 | Animated text document (t.d.k) | 미지원 | 지원 관찰 | textrange가 대표 gap | document keyframe/glyph regeneration |
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
| `stroke_dash.json` | 정적 title text는 복구됐고 frame 0/12 adjudication도 꽤 가깝다. 남은 차이는 `ADBE 4ColorGradient` 하나로 단정하기보다 broader effect stack과 image-level drift로 보는 편이 더 안전하다. |
| `textrange.json` | 성능은 이미 ThorVG보다 빠르다. 남은 핵심 gap은 animated `t.d.k` document와 range-selector opacity animator다. |
| `text_anim.json` | runtime text가 아니라 outlined shape scene이다. real text 완성의 근거로 쓰면 안 된다. |
| `32266.json` | steady-state보다 correctness + parse 이슈가 더 크다. first-frame exact match ratio는 `0.717` 수준이다. |
| `world_locations.json` | correctness보다 성능 문제다. first-frame은 거의 맞고, 병목은 여전히 matte/offscreen이다. |
| `11555/confetti/threads` | matte보다 transform-only rerasterization이 본질이다. snapshot/cache 경로가 필요하다. |
| `R_QPKIVi.json` | `ty`가 마지막인 shape object parser blank는 닫혔다. 최신 frame 0은 full-frame 전체에 작은 drift가 퍼지는 패턴이라, parser보다 non-opaque shape-layer 합성 경로가 더 의심스럽다. |
| `43391.json` | 단순 parser fallback 하나로 닫히지 않는다. 현재 1순위 의심은 chained `Merge Paths` semantics다. |

## 리소스별 성능 비교

| 리소스 | 기능군 | rlottie parse (ms) | ThorVG parse (ms) | rlottie frame (ms) | ThorVG frame (ms) | frame 배수 (rlottie/ThorVG) | rlottie RSS (KB) | ThorVG RSS (KB) | 판정 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| world_locations.json | matte/offscreen | 0.459 | 0.459 | 0.499 | 0.235 | 2.12x | 10416 | 4944 | ThorVG 우세 |
| 11555.json | transform cache | 0.248 | 0.434 | 1.472 | 1.328 | 1.11x | 4496 | 3680 | ThorVG 우세 |
| confetti.json | transform cache | 2.293 | 0.658 | 0.175 | 0.111 | 1.57x | 4832 | 4352 | ThorVG 우세 |
| threads.json | transform cache | 0.159 | 0.439 | 2.088 | 1.963 | 1.06x | 4848 | 3648 | ThorVG 우세 |
| text_anim.json | outlined text scene | 1.644 | 0.527 | 0.123 | 0.078 | 1.57x | 4848 | 4176 | ThorVG 우세 |
| stroke_dash.json | real text + effect | 0.196 | 0.441 | 0.160 | 0.131 | 1.22x | 3968 | 3392 | ThorVG 우세 |
| textrange.json | real text animator | 0.175 | 0.434 | 0.008 | 0.028 | 0.30x | 2848 | 3344 | rlottie 우세 |
| textblock.json | outlined shape text | 4.496 | 1.040 | 0.849 | 0.318 | 2.67x | 8592 | 7584 | ThorVG 우세 |
| 32266.json | correctness + parse | 15.478 | 0.802 | 0.191 | 0.399 | 0.48x | 20944 | 15952 | rlottie 우세 |
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
| matte/offscreen | world_locations.json, masking.json, starts_transparent.json | 0.638 | 0.468 | 0.251 | 0.204 | 1.23x | 6576 | 4421 | ThorVG 우세 |
| transform cache | 11555.json, confetti.json, threads.json | 0.900 | 0.510 | 1.245 | 1.134 | 1.10x | 4725 | 3893 | ThorVG 우세 |
| outlined text scene | text_anim.json, textblock.json | 3.070 | 0.784 | 0.486 | 0.198 | 2.45x | 6720 | 5880 | ThorVG 우세 |
| real text / text animator | stroke_dash.json, textrange.json | 0.186 | 0.437 | 0.084 | 0.079 | 1.06x | 3408 | 3368 | ThorVG 우세 |
| layer effect | layereffect.json | 0.174 | 0.389 | 0.115 | 0.174 | 0.66x | 4080 | 3856 | rlottie 우세 |
| merge paths | merging_shapes.json | 0.126 | 0.403 | 0.064 | 0.050 | 1.28x | 3104 | 3328 | ThorVG 우세 |
| basic vector | abstract_circle.json, windmill.json, glow_loading.json, gradient_sleepy_loader.json, polystar_anim.json | 0.107 | 0.403 | 0.099 | 0.151 | 0.66x | 3689 | 3484 | rlottie 우세 |
| correctness + parse | 32266.json | 15.478 | 0.802 | 0.191 | 0.399 | 0.48x | 20944 | 15952 | rlottie 우세 |

## 현재 우선순위

| 우선순위 | 대상 | 이유 | 바로 할 작업 |
| --- | --- | --- | --- |
| 1 | `expressions/world_locations.json` | first-frame은 그대로 맞고, matte path를 더 줄여도 아직 steady-state 격차가 가장 큼 | inherited bounds 전파 마무리, matte 재사용, direct-alpha 적용 범위 추가 확대 |
| 2 | `11555.json`, `confetti.json`, `threads.json` | subagent 리뷰 기준으로 `matrix dirty -> shape dirty -> reraster` 체인이 본질이다 | content/transform dirty 분리, local-space snapshot cache, precomp snapshot boundary 설계 |
| 3 | `stroke_dash.json` | 정적 text는 복구됐고 남은 차이는 effect stack 쪽이다 | late-frame adjudication 정교화, `ADBE 4ColorGradient`와 broader stack 분리 |
| 4 | `textrange.json` | 성능이 아니라 animated `t.d.k` document + range-selector opacity correctness gap이 본질이다 | `t.d.k` keyframe support, selector subset evaluator |
| 5 | `32266.json`, `R_QPKIVi.json`, `43391.json` | 성능보다 correctness drift가 더 크다. 다만 원인은 각각 다르다 | `32266`: image/precomp drift, `R_QPKIVi`: non-opaque shape-layer 합성, `43391`: chained merge-path semantics 분리 |

## 주의할 점

- `text_anim.json`, `textblock.json`을 보고 real text 지원이 됐다고 판단하면 안 된다.
- `32266.json`은 steady-state 성능 타깃으로 보면 우선순위를 잘못 잡게 된다.
- `world_locations.json`은 image-level로는 이미 꽤 가깝기 때문에, correctness보다 matte 성능에 집중해야 한다.
- `stroke_dash.json`은 text path를 올린 뒤에도 frame 시간은 아직 ThorVG보다 느리지만, frame 0과 frame 12 판정 모두 비교적 가까워서 effect 단일 원인으로 몰아가면 우선순위를 잘못 잡을 수 있다.
- `R_QPKIVi.json`은 이제 blank는 아니지만, 최신 판정은 “아예 틀린 도형”보다 “전면적인 작은 합성 drift”에 더 가깝다. parser와 compositing을 분리해서 봐야 한다.
- `43391.json`은 `el` 하나를 더 읽는다고 닫히지 않았다. merge-path chain semantics를 바로 보지 않으면 시간을 낭비하게 된다.
