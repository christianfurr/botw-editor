# MetalFX Temporal reconstruction

BOTW Editor 0.1.2 adds an experimental scene-reconstruction path to the supported
MacNativeGames BOTW 1.6.0 runtime. It uses Apple’s MetalFX temporal scaler and
lower-resolution scene frames. Color grading and custom color shaders remain
separate final-picture controls in [Color & custom shaders](SHADERS.md).

## Choose a preset

Open the editor’s sliders button and select **Upscaling**. Choose a preset,
click **Use on next launch**, then fully quit and reopen BOTW. All five choices
are restart-required; the running game keeps the configuration it read at boot.

| UI preset | Saved token | Scene render size | Reconstructed output |
| --- | --- | --- | --- |
| Off | `off` | Port’s original setting | Port’s original scaling |
| Quality — 1440p | `quality-1440` | 1920 × 1080 | 2560 × 1440 |
| Balanced — 1440p | `balanced-1440` | 1280 × 720 | 2560 × 1440 |
| Quality — 4K | `quality-2160` | 2560 × 1440 | 3840 × 2160 |
| Balanced — 4K | `balanced-2160` | 1920 × 1080 | 3840 × 2160 |

These are this project’s profiles. Output is the reconstructed frame size;
the window or display can present that frame at another size. Hardware/runtime
capability and scene-input validation still control whether reconstruction runs.

## Local preference and honest status

Only the explicit save button atomically writes the canonical preset token to:

```text
~/Library/Application Support/MacNativeGames/BotW/graphics-upscaler.txt
```

Missing settings select Off. Invalid or truncated settings disable the temporal
request rather than accepting a partial token. The scene-resolution adapter and
renderer share the same startup configuration.

The runtime writes its heartbeat and actual result to:

```text
~/Library/Application Support/BOTWEditor/upscaling/status.json
```

Status includes the immutable `saved_preset` used at boot, requested/capable/active
flags, actual input/output dimensions, encoded and GPU-completed counts, GPU
errors, history resets, missing-input counts, motion source and fallback reason.
The editor also accepts a fresh, live status from the older isolated prototype’s
`temporal/status.json`; it prefers the current installed path.

The editor requires a live process, fresh status, supported version, valid larger
output dimensions, completed GPU work and no GPU errors before displaying
**MetalFX Temporal is active**. A selected preset or a successful file write is
insufficient. A newly saved choice remains **Saved — restart BOTW** until a new
process confirms its boot token and actual dimensions. Fallback state displays
the runtime’s reason; it is not presented as active reconstruction.

## Current visual limits

Motion comes from camera reprojection plus a depth-reactive mask. Full per-object
motion vectors are not available through this bridge. Moving objects, foliage,
disocclusion and rapid camera changes can therefore show ghosting or other
reconstruction artifacts. Menu/loading transitions can suspend the scene path;
resuming it rebuilds history. Missing or incompatible scene inputs use the
fallback path.

This is experimental MetalFX integration, not a claim of Nvidia DLSS-equivalent
quality. Completed-frame counters are execution evidence, not a frame-rate or
visual-quality benchmark. The live checks cover the listed observations rather
than every scene or a sustained performance target.

## Verification

The candidate completed actual gameplay reconstruction at 1920 × 1080 →
2560 × 1440 and 2560 × 1440 → 3840 × 2160. The 4K check used only the saved
`quality-2160` preference, with no environment-variable render/output override.
The final rebuilt runtime completed 3,788 GPU temporal frames at 4K with zero
GPU errors. Movement kept the initial history; resuming from the menu added one
reset, with no further resets during the next 45 seconds. This roughly
90-second replay and the earlier candidate checks are recorded in
[Testing](TESTING.md); they do not establish sustained behavior in every scene.

`bash scripts/test-temporal.sh` runs sanitizer-backed configuration/draw/input
fixtures, then a real MetalFX GPU probe. The probe exercises RGB10A2 and FP16
input at 4/3×, 1.5× and 2× scaling: four GPU-completed frames per combination,
24 total, with sampled finite/color/alpha checks. Its synthetic scenes do not
establish moving-object reconstruction quality. A CI host without a capable
Metal device reports a GPU skip; that skip supplies no GPU execution evidence.

Apple’s [Metal 4 temporal-scaler factory](https://developer.apple.com/documentation/metalfx/mtlfxtemporalscalerdescriptor/maketemporalscaler%28device%3Acompiler%3A%29),
[temporal scaler properties](https://developer.apple.com/documentation/metalfx/mtlfxtemporalscalerbase),
and [integration guidance](https://developer.apple.com/videos/play/wwdc2022/10103/)
explain the temporal inputs and history model. This bridge’s implementation and
its camera-derived-motion limitation are described above.
