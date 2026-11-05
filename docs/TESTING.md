# Testing and release evidence

The editor, bridge, and game are separate verification surfaces. A successful compile, valid app signature, or matching request acknowledgement alone does not prove that a requested gameplay change behaves correctly.

## Current 0.1.1 source and synthetic checks

| Surface | Evidence |
| --- | --- |
| Swift model | Inventory/destination and optional live-stat/readiness decoding; explicit inventory blocker and pending-request gates; player-cache and travel-availability gates; north-up projection; process/freshness/feature gates; separate stack/raw-durability bounds and equipped-weapon refusal; identifier and command-injection rejection. |
| UI value conversion | Exact integer-hundredths conversion between displayed durability points and native units; two-decimal-place validation, bounds, and point/unit round trips. |
| Backend memory access | Read/write/readback against test allocations; reject unmapped, read-only, and overflowing spans. |
| Backend protocol | Input thread queues requests without applying edits; bounded queue; reject unavailable gameplay edits; match status/off acknowledgements. |
| Health/stamina bridge | Synthetic pointer chains; current-health target; stamina refill and lock; clear locks when context disappears. |
| Inventory value bridge | Synthetic writes round-trip raw weapon durability 2,000 and 3,000; accept durability bounds 1–100,000,000 and stack bounds 1–999; reject out-of-range values, equipped weapons, unsupported types, and unavailable native APIs without writing. |
| Armor metadata | Accept the game's `-1` no-dye sentinel and the verified dye range for armor types only; retain strict positive weapon durability and stack bounds. Validation failure no longer invents a menu condition. |
| Graphics model | Validate preset/strength/sharpness/source bounds and require a matching revision, live presenter process, fresh status, and an encoded applied/off result before confirming a look. |
| Metal pixels | 325 GPU assertions use synthetic textures for exact Off identity, crop/alpha, preset formulas, bounded sharpening, custom strength blending, nonfinite channels, invalid settings, compilation failure, and original-image fallback. |
| World metadata | Known game-name CRCs; direct and proxy flag traversal; separate S32/F32 arrays; reject invalid types/proxy signatures/permissions; base-shrine bounds. |
| Installer | Nineteen fixtures cover patch detection and path/mode allowlists, exclusive backups, read-only preflight, source ownership, version/active-game refusal, source changes, private-cache verification/reuse, and adding the renderer hook/header to an already integrated 0.1.0 checkout without replacing unrelated edits. |

These checks use controlled test data. They establish validation and protocol behavior, not live-game outcomes.

Run the reproducible checks on an Apple silicon Mac with Xcode's command-line tools:

```sh
./scripts/test.sh
```

This runs the Swift model/UI/graphics checks, the native C fixtures in `tests/runtime_checks.c`, the synthetic Metal probe, and the installer fixtures. The C fixture compiles with `-Wall -Wextra -Werror` and currently passes 224 assertions. It allocates synthetic pointer chains, clears game-control environment variables, and never opens BOTW or reads a gameplay profile. Build artifacts stay under the ignored `build/` folder.

To run only the native guard checks:

```sh
bash scripts/test-runtime.sh
```

The Metal probe requires an actual GPU for pixel evidence. Local checks fail if no device is available; CI may report an explicit GPU skip when its host has no Metal device. An offline shader compile on such a host is a separate check. See [Shaders](SHADERS.md) for the contract and test scope.

## 0.1.1 live verification — 2026-11-04

The rebuilt runtime was exercised in an isolated copy of the local gameplay profile. These observations cover the revised paths; the prior release's checks remain dated below.

| Operation | Observed outcome |
| --- | --- |
| Armor and inventory readiness | A pouch containing `Armor_151_Upper`, type 5 with native dye value `-1`, remained readable. The runtime reported `inventory_ready=true` and listed the armor and sword instead of treating the entire pouch as changing. |
| Item grants with menus open | Apples were granted outside the menu (+2), from the system menu (+3), and while the Materials pouch page was open (+1). Each request matched its acknowledgement and native readback, reaching six apples without a close-menu refusal. |
| Pouch refresh | The open in-game page could retain its cached count after a grant. The success message now distinguishes the completed grant from the optional action of reopening the pouch to refresh that page; closing it is not a grant prerequisite. |
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

The 0.1.0 source and build checks passed on GitHub Actions: [editor build](https://github.com/christianfurr/botw-editor/actions/runs/41937056576). The 0.1.1 release requires its own final build and workflow result.
