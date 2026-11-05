# Color filters and custom Metal shaders

The **Color & custom shaders** section is independent of the restart-required
[MetalFX Temporal upscaler](TEMPORAL.md). Turning these filters Off restores the
unfiltered picture without changing the saved upscaling preset.

BOTW Editor can apply lightweight native Metal filters to the supported BOTW
1.6.0 Mac runtime. They operate on the final SDR picture after output scaling.
They change color and local sharpness, not the game's lighting, models, textures,
or geometry. The game's own HUD is part of that picture and is filtered too.
The runtime's separate FPS/shader/load overlays keep their original pipeline.

Off selects the original presentation pipeline. Natural adds restrained color
and contrast; Vivid is stronger; Cinematic slightly softens saturation and adds
a small warm tint. Strength blends the selected grade. Sharpness is a separate,
bounded five-tap filter whose samples stay inside the visible game rectangle.
It preserves alpha and clamps its result to the sampled neighborhood.

For a mix of the original BOTW look and a subtle cinematic grade, start with
Cinematic at 40% strength and 12% sharpness. This is a taste setting, not a
performance measurement or a claim of physical lighting changes.

## Local files and acknowledgement

The editor atomically writes this private file:

```text
~/Library/Application Support/BOTWEditor/shaders/settings.json
```

```json
{"schema":1,"revision":1,"preset":"cinematic","strength":0.4,"sharpness":0.12}
```

Presets are `off`, `natural`, `vivid`, `cinematic`, and `custom`. Strength and
sharpness must be finite numbers from 0 to 1; revision is a nonnegative integer
no larger than 9,007,199,254,740,991. Missing or invalid settings keep the original
picture. The presenter checks file metadata at most once per second and compiles
changed shader source once, so changing a custom shader can briefly interrupt
presentation while the Metal compiler works.

`status.json` beside the settings records the game process ID, supported version,
revision, requested/applied preset, values, error text, and a timestamp refreshed
each second while presentation runs. `stage` is `off`, `ready`, `applied`, or
`error`; `applied` follows encoding the main picture draw with the filter.
This is presenter acknowledgement, not a GPU timing or screenshot comparison.
The UI should require a matching revision and a live process/fresh heartbeat.

## Custom color shaders

Place UTF-8 Metal source at `custom.metal` in the same private folder. It must
define this function:

```metal
float3 customGrade(float3 color, float2 uv) {
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    return mix(float3(luminance), color, 1.05);
}
```

`color` is the sampled SDR RGB value, and `uv` is the cropped source texture
coordinate. The function returns RGB; the runtime retains the original alpha,
blends by Strength, replaces nonfinite result channels with the original values,
and clamps output to 0–1. Sharpening runs before the custom color function.
This contract supports color transforms, not depth-aware effects or arbitrary
ReShade FX programs. Source is limited to 64 KB. A missing source file or Metal
compile/pipeline error falls back to the original picture and reports the error.
Correcting the source causes a new compile on the next metadata poll.

## Validation

`bash scripts/test-postfx.sh` builds a small standalone Metal probe. It uses only
synthetic textures and an isolated directory under the ignored build folder;
it never launches BOTW, reads game data, or touches a gameplay profile. Its 325
assertions cover exact identity/crop/alpha, preset results against independent
CPU formulas, sharpening crop bounds, custom Strength blending, compile errors,
nonfinite channels, invalid settings, and unsupported HDR output. The renderer
also passes a BOTW 1.6.0 Objective-C syntax check.

A rebuilt BOTW 1.6.0 runtime was checked live in an isolated gameplay profile on
2026-11-04. A custom grayscale function visibly changed the game window and
received matching applied status. Invalid Metal source produced an error and
the original/Off fallback; corrected source recovered. Cinematic also received
a matching applied acknowledgement. These observations establish those visual
and recovery paths, not frame-time or sustained-load performance.

The ordinary source check script includes this probe. On a CI host without a
Metal device, the probe exits with code 77 and the wrapper reports a clear GPU
skip; it additionally compiles the Metal source offline when that compiler is
available. Local checks remain strict when no device exists. A CI skip does not
establish GPU pixel results; the native Mac run above supplies that evidence.

The existing port's HDR grading and scene-only FXAA/sharpening code is gated to
other game versions. This filter does not enable those paths for BOTW 1.6.0.
Native Metal libraries/pipelines are used directly; see Apple's
[shader library documentation](https://developer.apple.com/documentation/metal/shader-libraries)
and [render pipeline descriptor](https://developer.apple.com/documentation/metal/mtlrenderpipelinedescriptor).
ReShade's [supported graphics APIs](https://reshade.me/?lang=en) do not include
Metal, so Windows ReShade installers are not used here.
