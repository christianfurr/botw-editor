# Testing and release evidence

The editor, bridge, and game are separate verification surfaces. A successful compile, valid app signature, or matching request acknowledgement alone does not prove that a requested gameplay change behaves correctly.

## Current 0.1.2 source and synthetic checks

| Surface | Evidence |
| --- | --- |
| Swift model | Inventory/destination and optional live-stat/readiness decoding; explicit inventory blocker and pending-request gates; player-cache and travel-availability gates; north-up projection; process/freshness/feature gates; separate stack/raw-durability bounds and equipped-weapon refusal; identifier and command-injection rejection. |
| UI value conversion | Exact integer-hundredths conversion between displayed durability points and native units; two-decimal-place validation, bounds, and point/unit round trips. |
| Backend memory access | Read/write/readback against test allocations; reject unmapped, read-only, and overflowing spans. |
| Backend protocol | Input thread queues requests without applying edits; bounded queue; reject unavailable gameplay edits; match status/off acknowledgements. |
| Health/stamina bridge | Synthetic pointer chains; current-health target; stamina refill and lock; clear locks when context disappears. |
| Inventory value bridge | Synthetic writes round-trip raw weapon durability 2,000 and 3,000; accept durability bounds 1–100,000,000 and stack bounds 1–999; reject out-of-range values, equipped weapons, unsupported types, and unavailable native APIs without writing. |
| Armor metadata | Accept the game's `-1` no-dye sentinel and the verified dye range for armor types only; retain strict positive weapon durability and stack bounds. Validation failure no longer invents a menu condition. |
| Upscaling model | 90 assertions cover strict boot presets, bounded/atomic preference I/O, fresh live-process evidence, real larger dimensions, GPU completion/error guards, and matching boot token/new-PID confirmation after restart. |
| Temporal input guards | The release source passes 249 ASan/UBSan-backed configuration, cache, geometry, UV/cull, host-snapshot and GPU-write-order assertions. Passing these fixtures is separate from final live stability. |
| MetalFX GPU probe | RGB10A2 and FP16 inputs at 4/3×, 1.5× and 2× completed four GPU frames per combination, 24 total, with finite/color/alpha sampling checks. |
| Graphics model | Validate preset/strength/sharpness/source bounds and require a matching revision, live presenter process, fresh status, and an encoded applied/off result before confirming a look. |
| Metal pixels | 325 GPU assertions use synthetic textures for exact Off identity, crop/alpha, preset formulas, bounded sharpening, custom strength blending, nonfinite channels, invalid settings, compilation failure, and original-image fallback. |
| World metadata | Known game-name CRCs; direct and proxy flag traversal; separate S32/F32 arrays; reject invalid types/proxy signatures/permissions; base-shrine bounds. |
| Installer | 24 fixtures cover patch allowlists, atomic selection of fresh/compatibility sections, upgrades from 0.1.0/0.1.1, ownership/conflict checks, backups, version/process refusal, source-change detection and private-cache verification/reuse. |

These checks use controlled test data. They establish validation and protocol behavior, not live-game outcomes.

Run the reproducible checks on an Apple silicon Mac with Xcode's command-line tools:

```sh
./scripts/test.sh
```

This runs the Swift model/UI/graphics/upscaling checks, the native C fixtures in `tests/runtime_checks.c`, sanitizer-backed temporal-input fixtures, real MetalFX and color-filter GPU probes, and the installer fixtures. The C fixture compiles with `-Wall -Wextra -Werror` and currently passes 224 assertions. It allocates synthetic pointer chains, clears game-control environment variables, and never opens BOTW or reads a gameplay profile. Build artifacts stay under the ignored `build/` folder.

To run only the native guard checks:

```sh
bash scripts/test-runtime.sh
```

The Metal probe requires an actual GPU for pixel evidence. Local checks fail if no device is available; CI may report an explicit GPU skip when its host has no Metal device. An offline shader compile on such a host is a separate check. See [Shaders](SHADERS.md) for the contract and test scope.

## 0.1.2 live temporal verification — 2026-11-05

These candidate and final-runtime tests used the supported BOTW 1.6.0 runtime on the local M4 Mac
and an isolated gameplay-profile copy. Counters below are separate observations,
not an FPS measurement. The original profile’s 14 files matched their exact
baseline hashes after final verification. The disposable profile and its 10 test `.sav` files were then removed; the normal profile was preserved.

| Operation | Observed outcome |
| --- | --- |
| 1440p reconstruction | Actual 1920 × 1080 scene input produced 2560 × 1440 output with increasing encoded and GPU-completed temporal counters and no reported GPU errors. |
| Saved 4K preset | Using only the saved `quality-2160` preference, a new process produced 2560 × 1440 → 3840 × 2160 output. The initial observation reported 199 encoded / 199 completed, zero GPU errors, zero missing inputs, one history reset and 18 jitter phases. No render/output/temporal environment override supplied these dimensions. |
| 4K stationary candidate | A later observation reported 1,107 encoded / 1,105 GPU-completed frames, zero GPU errors and one history reset. |
| 4K camera/walk candidate | Camera movement and walking reached 1,387 encoded / 1,387 completed, zero GPU errors and one history reset. Limited still inspection showed Link, scenery and HUD; it is not a complete motion-quality evaluation. |
| Menu and resume candidate | The menu suspended the scene path at 1,412 encoded / completed, with zero GPU errors. Resuming reached 1,512 encoded / 1,511 completed, zero GPU errors and two history resets. |
| Installer upgrade | A real installer upgrade preserved backups, rebuilt the supported app in about 60 seconds, and verified the local caches and 135-destination extraction. |
| Final bound-input replay | The final rebuilt source completed 3,789 encoded / 3,788 GPU-completed 2560 × 1440 → 3840 × 2160 frames with zero GPU errors and no missing inputs. Movement retained one initial history reset. The menu suspended reconstruction; resume added exactly one reset, and a further 45 seconds retained two resets with the temporal path active. The run lasted roughly 90 seconds after first activation. Uniform proof follows the actual bound immutable buffer; unresolved original-pool writes still refuse. |

The implementation uses camera-derived motion plus a depth-reactive mask rather
than complete object-motion vectors. The checks establish actual MetalFX GPU
execution at the recorded sizes and the observed transitions. They do not claim
DLSS-equivalent quality, absence of ghosting in all scenes, sustained performance
across every scene, or a measured frame-rate improvement.

## 0.1.1 live verification — 2026-11-04

The rebuilt runtime was exercised in an isolated copy of the local gameplay profile. These observations cover the revised paths; the prior release's checks remain dated below.

| Operation | Observed outcome |
| --- | --- |
| Armor and inventory readiness | A pouch containing `Armor_151_Upper`, type 5 with native dye value `-1`, remained readable. The runtime reported `inventory_ready=true` and listed the armor and sword instead of treating the entire pouch as changing. |
| Item grants with menus open | Apples were granted outside the menu (+2), from the system menu (+3), and while the Materials pouch page was open (+1). Each request matched its acknowledgement and native readback, reaching six apples without a close-menu refusal. |
| Pouch refresh | The open in-game page retained its cached count after a grant. Reopening the Materials page visibly showed six apples. The success message distinguishes a completed grant from this optional display refresh; closing the menu is not a grant prerequisite. |
| Custom shader | A custom grayscale shader produced a visibly grayscale game window with matching presenter `applied` status. |
| Error fallback and recovery | Invalid Metal source reported `stage=error` with the original/Off fallback. Correcting the custom source restored the effect. |
| Built-in shader | Cinematic was acknowledged as applied with the requested revision and numeric values. A restrained 40% strength / 12% sharpness blend remains a taste setting, not a benchmark result. |
| Editor views | Player, Graphics, Give, and World views were rendered from actual read-only state/catalog data with `UI_SNAPSHOT`; the capture process sent no game commands and wrote no shader settings. |

These checks do not measure shader frame time, compare sustained GPU load, or establish that every menu and item combination behaves identically. The filter's GPU math and error paths have separate synthetic checks. The final test-profile cleanup and release build/workflow result must be recorded by the release owner after their completion.

## 0.1.0 live verification — 2026-11-04

Tests used **Switch BOTW 1.6.0**, the supported MacNativeGames revision `b1a49a2`, and an **Apple M4 Mac with 16 GB memory on macOS 27.2**. Gameplay ran in a separate test profile. The table records observed outcomes, not compatibility promises for other versions or Macs.

| Operation | Observed outcome |
| --- | --- |
| Connection and identity | The final runtime distinguished the title state from gameplay. Live edits became available in the gameplay session and matched request IDs and native readbacks. |
| Inventory | Apples changed from 5 to 9 and were removed. A Traveler's Sword was granted through the native pouch API with a value of 2,000 raw units, equivalent to 20 durability points. |
| Weapon durability | In the final runtime, an unequipped Traveler's Sword (`Weapon_Sword_001`) changed from 2,000 to 3,000 and back to 2,000 native units, with exact readbacks. These correspond to 20 → 30 → 20 displayed durability points. |
| Rupees | Edits to 1,234 and 2,345 were acknowledged and read back from the game. |
| Hearts and stamina | The game exposed 120 current health units / 30 maximum hearts and 3,000 stamina units / three wheels. The in-game HUD showed 30 hearts. |
| Shrine/map access | All 120 base-shrine activation flags and all 15 tower-region flags were read back after the world-access operations. Activation was kept separate from shrine completion. |
| Base abilities | The rune and paraglider acquisition flags were set through native GameData calls and read back. This does not claim that every rune interaction was exercised. |
| Full Access | Towers were physically raised, including the Great Plateau tower. The implementation sets the opening tower sequence flag and skips that sequence. Its flag set does not write shrine-clear arrays or story-quest completion flags. |
| Shrine travel | Travel to Oman Au Shrine completed, with arrival and player movement observed. |
| Tower travel | Travel to the raised Great Plateau Tower reached its platform with the player alive. |
| Long-distance travel | Travel to Tah Muhl Shrine completed its scene load; destination coordinates and an alive player were observed. |
| Persistence | After an autosave, relaunching and loading retained 30 hearts, three stamina wheels, and the world-access state. |
| Installer | A real supported installation created app/profile backups, rebuilt the runtime, and extracted the 135-location destination catalog. |

### Scope of the 0.1.0 prerelease checks

The final durability revision completed its native 2,000 → 3,000 → 2,000 live round trip. The UI's displayed-points conversion is covered separately by compiled exact-hundredths checks; the protocol continues to carry validated integer native units.

The initial tests do not establish every item in the catalog, every travel destination, sustained health locking under damage, or sustained stamina locking during depletion. The game continues to enforce its item capacity, duplicate-item, and DLC rules. Outdoor `MainField` travel is the supported route; interior refusal has a compiled guard and is not a claim of interior travel support.

An initial dedicated test profile was removed after verification: eight test `.sav` files were deleted. The final durability follow-up used another separate test profile; its six `.sav` files and the profile were also removed. The normal profile contained no gameplay saves and was not used for test progression. Application/profile backups were created, but a complete backup-restore gameplay cycle has not been claimed by this record.

Keep saves, extracted artwork, memory, raw logs, private paths, and gameplay screenshots out of the public repository. Public screenshots show only Player or Graphics controls, which use original interface graphics and system symbols.

## Release checks

- Run the source build and its model checks against the final sources.
- Run bridge guard/protocol tests against the final runtime headers.
- Run `python3 scripts/test_install_bridge.py`. The test fixtures do not launch or rebuild BOTW.
- Inspect the built editor at standard and reduced window sizes, including disconnected/loading states.
- Verify the app's architecture, deployment target, signature, and bundled catalog/artwork licenses.
- Test the bridge installer against a supported checkout; verify that unsupported versions and dirty/conflicting integration states stop safely.
- Inspect the release archive and tracked tree for game files, keys, firmware, saves, logs, dumps, private paths, and unintended assets.
- Update this document with actual live results and the final release revision before describing provisional features as verified.

The 0.1.0 source and build checks passed on GitHub Actions: [editor build](https://github.com/christianfurr/botw-editor/actions/runs/41937056576). The 0.1.2 final live replay and local artifact checks passed. Its matching GitHub Actions result is recorded with the release.
