# Original slate frame

Generated with the built-in image_gen tool on 2026-11-04 for BOTW Editor. This is an original generated UI bezel; it is not extracted Nintendo artwork. No game assets, screenshots, logos, or exact Nintendo symbols are embedded.

## Visual research

- Nintendo, Sheikah Slate feature illustration: https://zelda.nintendo.com/breath-of-the-wild/assets/img/features/sheikah_2x.png
- Nintendo, Breath of the Wild features: https://zelda.nintendo.com/features/
- Zelda Wiki, physical slate reference: https://zeldawiki.wiki/wiki/Sheikah_Slate

References were used to study dark stone, warm raised relief, flowing channels, cyan light, and amber accents. The reference PNG in this work folder is research-only and must not be packaged in the public repository.

## Final prompt

Use case: stylized-concept.
Asset type: production UI skin texture, a transparent 4:3 landscape tablet bezel for a native macOS game editor.
Primary request: create an ORIGINAL ancient-tech tablet frame inspired by the tactile Sheikah Slate materials and visual language in Breath of the Wild: weathered dark umber volcanic stone, aged warm sandstone/bronze raised carved curls and flowing geometric channels, thin luminous cyan-blue circuit inlays with tiny amber-orange energy nodes, irregular hand-carved ornamental corners. It should feel like a real ancient artifact rendered with the beautiful painterly, softly cel-shaded 3D materials of BOTW, not generic flat sci-fi lines.
Composition: straight-on orthographic, perfectly flat front-facing, no perspective tilt, one complete landscape rectangular frame, centered and edge-to-edge with only a tiny transparent margin. Canvas approximately 1536 by 1152. Outer silhouette gently rounded but slightly irregular/chipped. Thin functional bezel around all four edges, roughly 60 pixels thick at this image size, richer relief at the corners up to 90 pixels. The center is a LARGE fully transparent rectangular cutout taking about 92% of the width and 89% of the height: real alpha transparency, no screen, no glass, no image or texture inside the central opening. Outside the frame is also real alpha transparency. Straight middle sections along each edge must be continuous enough to stretch as a nine-slice UI image.
Details: asymmetrical carved curls, shallow pits, worn lighter edges and recessed dark seams; restrained cyan glow embedded in a few channels; small amber-orange triangular or circular light details at corner joints. Keep the eye-catching relief on the outer corners and perimeter, leaving every part of the center opening empty and transparent for app controls.
Lighting: restrained soft top-left lighting and local cavity shadows that belong to the frame; no cast shadow outside.
Text: absolutely no text, letters, glyph words, numbers, logos, or watermark. No exact Nintendo symbols, no Triforce, no copied eye emblem, no characters, hands, background scenery, props, screen content, buttons or existing UI.
Output: a single clean production-ready PNG-style cutout asset with true transparent center and background. Preserve the complete thin outer frame with all edges visible.

## Integration

slate-frame.png is 1448 by 1086 pixels, RGBA. Center alpha is 0 and exterior corner alpha is 0. Use it as a noninteractive perimeter overlay, preserving enough content inset for the corners. The straight border band is approximately 80–110 source pixels; corner relief reaches about 250 source pixels. At a near-4:3 window, an ordinary resizable overlay preserves the proportions closely. Keep controls and text in native SwiftUI; do not bake them into the raster frame.
