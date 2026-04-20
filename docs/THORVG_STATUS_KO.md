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
| `R_QPKIVi.json` 상태 변화 | blank-output 단계는 이미 벗어났다. 이번 배치에서 single solid-fill shape layer는 layer alpha를 drawable 쪽으로 접고 offscreen을 생략하도록 바꿨다. exact match는 여전히 `0`이지만 full-frame compositing drift는 확실히 줄었다 |
| `world_locations.json` matte 경로 | 기존 `ShapeLayer` alpha offscreen clip tightening 위에, nested child-layer walk가 필요한 source에서 source offscreen을 건너뛰는 recursive direct-alpha matte 경로를 추가했다. `first-frame` 정합성은 그대로 유지된다 |
| static `ShapeLayer` drawable-list 재사용 | `contentStatic`가 참인 `ShapeLayer`는 drawable pointer list 구조가 프레임마다 바뀌지 않는다는 점을 이용해, `preprocessStage()`에서 render-list 재구성을 매 프레임 반복하지 않도록 바꿨다 |
| 최신 성능 판정 | 같은 머신의 `HEAD` baseline과 비교한 representative late-frame dump는 `world_locations@120`, `11555@160`, `confetti@90`, `threads@90`, `stroke_dash@12`, `textrange@120` 모두 exact match `1.0`이었다. median-of-5 기준 현재 `world_locations`, `11555`, `confetti`, `textrange`는 ThorVG보다 빨라졌고, 남은 성능 열세는 `threads`, `stroke_dash`, outlined text scene 쪽으로 좁혀졌다 |
| transform-cache 선행 작업 | `VPainter`에 affine bitmap draw helper를 추가했고, `model::Layer`에는 layer transform과 분리된 `contentStatic` 메타데이터를 넣었다. 여기에 더해 static `ShapeLayer` drawable-list 재사용은 실제 benchmark를 통과해 남겼다. 반면 narrow `ShapeLayer` snapshot cache 프로토타입은 `11555.json`, `threads.json`에서 ThorVG image adjudication을 악화시켜 버렸다 |
| `43391.json` 시도 결과 | `Merge Paths::Mode::Merge`를 boolean union 대신 compound-path rasterization으로 바꿔 chained merge case를 부분 복구했다. frame 0 exact match ratio는 `0.778789`까지 올라왔지만, 여전히 correctness backlog다 |
| JSON file loading | `loadFromFile()`가 iterator 기반 텍스트 읽기 대신 single binary read를 사용하도록 바뀌었다. `page_slide.json`, `32266.json` 같은 parse-heavy 자산에서 실측 이득이 확인됐다 |
| `32266.json` 재판정 | lazy image decode + single-read loader 이후에도 parse는 여전히 ThorVG보다 훨씬 느리다. 최신 median은 `14.192 ms -> 0.796 ms(ThorVG)`라서 giant data URI를 포함한 JSON/base64 payload 자체가 여전히 핵심 병목이다 |
| rejected 실험 | recursive precomp `coverageBounds()`, `11555/confetti/threads`용 translation-only RLE 재사용, `stroke_dash`용 narrow `ADBE 4ColorGradient`, world-space snapshot cache, broad `contentStatic` skip은 모두 benchmark/adjudication에서 살아남지 못해 버렸다 |

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
| 부분 지원 | ADBE 4ColorGradient | 미지원 | 지원 관찰 | `stroke_dash` 후보 gap으로 남아 있다. 다만 narrow whole-layer 근사는 frame 0/12 모두 악화시켜 버렸다 | true effect semantics 조사 후 다시 설계 |
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
| `world_locations.json` | correctness 문제는 사실상 아니다. direct-alpha matte와 static drawable-list 재사용 뒤에 current desktop median에서는 ThorVG보다 빨라졌다. 다만 Tizen 실기기에서도 같은 결과가 유지되는지는 아직 검증이 필요하다. |
| `11555/confetti/threads` | transform-only rerasterization이 본질이라는 진단은 그대로였고, static drawable-list 재사용이 `11555`와 `confetti`를 실제로 뒤집었다. 현재 남은 transform bucket의 실질적인 성능 열세는 `threads` 하나다. |
| transform-cache 프로토타입 | world-space snapshot 재투영만으로는 충분하지 않았다. `11555.json`과 `threads.json`은 baseline보다 빨라졌지만 ThorVG image adjudication에서는 오히려 더 멀어졌다. 다음 시도는 world-space bitmap reuse가 아니라 local-space cache + 명시적 transform 경계 분리여야 한다. |
| `R_QPKIVi.json` | `ty`가 마지막인 shape object parser blank는 닫혔다. 이번 배치에서 single solid-fill shape layer alpha를 drawable 쪽으로 접으면서 frame 0 drift가 더 줄었다. exact match는 여전히 `0`이지만 mean abs diff RGB는 `[0.1539, 1.1118, 0.1537] -> [0.0960, 0.9815, 0.0505]`로 개선됐다. |
| `43391.json` | chained `Merge Paths` semantics 수정으로 큰 빈 구멍은 줄었지만 아직 틀린 영역이 남는다. 현재는 추가 merge-path chain semantics와 stroke semantics를 같이 봐야 한다. |

## 리소스별 성능 비교

| 리소스 | 기능군 | rlottie parse (ms) | ThorVG parse (ms) | rlottie frame (ms) | ThorVG frame (ms) | frame 배수 (rlottie/ThorVG) | rlottie RSS (KB) | ThorVG RSS (KB) | 판정 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| world_locations.json | matte/offscreen | 0.269 | 0.462 | 0.059 | 0.236 | 0.25x | 4384 | 4864 | rlottie 우세 |
| 11555.json | transform cache | 0.182 | 0.471 | 0.091 | 1.361 | 0.07x | 4176 | 3664 | rlottie 우세 |
| confetti.json | transform cache | 1.512 | 0.663 | 0.089 | 0.113 | 0.79x | 3872 | 4368 | rlottie 우세 |
| threads.json | transform cache | 0.130 | 0.450 | 2.043 | 1.996 | 1.02x | 5440 | 3632 | ThorVG 근소 우세 |
| text_anim.json | outlined text scene | 1.165 | 0.543 | 0.129 | 0.087 | 1.48x | 3936 | 4160 | ThorVG 우세 |
| stroke_dash.json | real text + effect | 0.135 | 0.463 | 0.169 | 0.126 | 1.34x | 3936 | 3392 | ThorVG 우세 |
| textrange.json | real text animator | 0.130 | 0.449 | 0.009 | 0.029 | 0.32x | 2784 | 3360 | rlottie 우세 |
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
| matte/offscreen | world_locations.json | 0.269 | 0.462 | 0.059 | 0.236 | 0.25x | 4384 | 4864 | rlottie 우세 |
| transform cache | 11555.json, confetti.json, threads.json | 0.608 | 0.528 | 0.741 | 1.157 | 0.64x | 4496 | 3888 | rlottie 우세, 단 `threads`는 아직 개별 열세 |
| outlined text scene | text_anim.json, textblock.json | 2.831 | 0.792 | 0.489 | 0.203 | 2.41x | 6264 | 5872 | ThorVG 우세 |
| real text / text animator | stroke_dash.json, textrange.json | 0.133 | 0.456 | 0.089 | 0.078 | 1.15x | 3360 | 3376 | ThorVG 우세, correctness는 rlottie 열세 |
| layer effect | layereffect.json | 0.174 | 0.389 | 0.115 | 0.174 | 0.66x | 4080 | 3856 | rlottie 우세 |
| merge paths | merging_shapes.json | 0.126 | 0.403 | 0.064 | 0.050 | 1.28x | 3104 | 3328 | ThorVG 우세 |
| basic vector | abstract_circle.json, windmill.json, glow_loading.json, gradient_sleepy_loader.json, polystar_anim.json | 0.107 | 0.403 | 0.099 | 0.151 | 0.66x | 3689 | 3484 | rlottie 우세 |
| correctness + parse | 32266.json, R_QPKIVi.json, 43391.json | 5.041 | 0.598 | 0.170 | 0.210 | 0.81x | 10112 | 7947 | steady-state는 오히려 rlottie가 앞서지만 parse와 correctness는 여전히 rlottie 열세 |

## 현재 우선순위

| 우선순위 | 대상 | 이유 | 바로 할 작업 |
| --- | --- | --- | --- |
| 1 | `threads.json` | transform bucket에서 사실상 유일하게 남은 steady-state 열세다 | local-space snapshot cache를 `threads` 기준으로 다시 좁히고 explicit transform boundary를 실구현 |
| 2 | `stroke_dash.json` | 정적 text는 복구됐지만 effect/image-level drift가 남고, narrow `ADBE 4ColorGradient`는 실패했다 | frame 0뿐 아니라 late-frame adjudication 기준으로 true effect semantics를 다시 분해 |
| 3 | `text_anim.json`, `textblock.json` | outlined scene bucket은 여전히 ThorVG보다 크게 느리다 | static drawable-list 재사용과 별개로 path/raster work를 더 줄일 구조 찾기 |
| 4 | `textrange.json` | 성능이 아니라 animated `t.d.k` document + range-selector opacity correctness gap이 본질이다 | `t.d.k` keyframe support, selector subset evaluator |
| 5 | `32266.json`, `R_QPKIVi.json`, `43391.json` | 성능보다 correctness drift가 더 크다. 다만 원인은 각각 다르다 | `32266`: image/precomp drift, `R_QPKIVi`: non-opaque shape-layer 합성, `43391`: chained merge-path semantics 분리 |

## 주의할 점

- `text_anim.json`, `textblock.json`을 보고 real text 지원이 됐다고 판단하면 안 된다.
- `32266.json`은 steady-state 성능 타깃으로 보면 우선순위를 잘못 잡게 된다.
- `world_locations.json`은 image-level로 이미 거의 맞았고, 이번 drawable-list 재사용 뒤에는 desktop median에서도 ThorVG보다 빨라졌다. 지금은 active hotspot이라기보다 Tizen 재검증 대상이다.
- `world_locations`와 `11555/confetti/threads`에 대해 speculative한 matte/cache 경로를 억지로 유지하면 오히려 median frame time이 악화된다. 현재 문서에는 benchmark를 통과한 경로만 남긴다.
- narrow `ShapeLayer` snapshot cache는 baseline steady-state를 일부 줄였지만 `11555.json`과 `threads.json`에서 baseline 대비 frame 0 drift가 커졌다. 그래서 코드에는 남기지 않고, affine bitmap draw helper와 `contentStatic` 메타데이터만 선행 작업으로 유지했다.
- 반대로 이번 static `ShapeLayer` drawable-list 재사용은 representative late-frame dump 여섯 개가 모두 baseline과 exact match였고, 같은 median-of-5 기준으로 `world_locations`, `11555`, `confetti`를 ThorVG 앞까지 밀어 올렸다.
- `stroke_dash.json`은 text path를 올린 뒤에도 frame 시간은 아직 ThorVG보다 느리지만, frame 0과 frame 12 판정 모두 비교적 가까워서 effect 단일 원인으로 몰아가면 우선순위를 잘못 잡을 수 있다.
- `R_QPKIVi.json`은 이제 blank도 아니고 catastrophic miss도 아니다. single solid-fill layer alpha를 inline한 뒤에도 exact match는 `0`이지만, 이건 여전히 전면적인 작은 compositing drift 문제라는 뜻이다.
- `43391.json`은 parser fallback 문제가 아니라 merge-path chain semantics 문제였다. 다만 이번 수정으로도 exact match가 `0.779` 수준이라, 남은 drift를 과소평가하면 안 된다.
