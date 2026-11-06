# Testing and release evidence

The editor, bridge, and game are separate verification surfaces. A successful compile, valid app signature, or matching request acknowledgement alone does not prove that a requested gameplay change behaves correctly.

## Current 0.1.3 source and synthetic checks

| Surface | Evidence |
| --- | --- |
| Swift model | Inventory/destination and optional live-stat/readiness decoding; explicit inventory blocker and pending-request gates; player-cache and travel-availability gates; north-up projection; process/freshness/feature gates; separate stack/raw-durability bounds and equipped-weapon refusal; identifier and command-injection rejection. |
| UI value conversion | Exact integer-hundredths conversion between displayed durability points and native units; two-decimal-place validation, bounds, and point/unit round trips. |
| Backend memory access | Read/write/readback against test allocations; reject unmapped, read-only, and overflowing spans. |
| Backend protocol | Input thread queues requests without applying edits; bounded queue; reject unavailable gameplay edits; match status/off acknowledgements. |
| Health/stamina bridge | Synthetic pointer chains; current-health target; stamina refill and lock; clear locks when context disappears. |
| Inventory value bridge | Synthetic writes round-trip raw weapon durability 2,000 and 3,000; accept durability bounds 1–100,000,000 and stack bounds 1–999; reject out-of-range values, equipped weapons, unsupported types, and unavailable native APIs without writing. |
| Armor metadata | Accept the game's `-1` no-dye sentinel and the verified dye range for armor types only; retain strict positive weapon durability and stack bounds. Validation failure no longer invents a menu condition. |
| Master Sword grants | Refuse internal `Weapon_Sword_056`/`071`/`072` forms before native acquisition; require quantity one for canonical `070`; report duplicate ownership explicitly. |
| Master Sword levels | Four exact additive-power/full-power pairs; owned, unique and unequipped sword gates; typed flag permissions; both-store readback; synthetic failure rollback. Beam, durability, recharge and quest fields are outside the transaction. |
| Pouch capacity | Category/token/range checks, increase-only validation against both stores, native flag readback, synthetic rollback and preservation of unrelated flags. |
| In-game map access | Argument-free command, independent capability, and a two-flag transaction for the verified PortableUnit booleans. Synthetic preflight/readback/rollback does not prove the actual map screen opens. |
| Shrine discovery and runes | Mixed Bool/S32 activation, preservation of each store's positive discovery value, nine rune/Slate flags, exact `Location_Dungeon001` metadata/opcode exception, whole-batch readback and rollback. |
| Rune selection request | Fifteen native guard instructions match the local NX160 ELF; TryLockMutex/UnlockMutex import ABI and the download UI's requested-selection write were verified. Fault-injected sanitizer checks cover absent/valid selections, busy/invalid refusal, write/readback failures, owned rollback and preservation of competing changes. This does not prove the selector opens in the live game. |
| Separate shrine completion | Selected/all-base-shrine Clear transitions plus counter delta only for newly cleared shrines; refuse store disagreement and overflow; restore failed batches. Fixtures exclude Spirit Orbs, Enter, quests, chests and rewards. |
| Horse movement | 16,113 independent ASan/UBSan checks cover the 104-cell scope, original-based scaling, exact Off restoration, resource replacement, competing changes, failed writes/readbacks and rollback. Synthetic fixtures contain no extracted game data. |
| Map labels | Eight standard-library fixtures cover original placements, deduplication, settlement selection, kinds, numeric/visibility bounds and digest behavior. A separate private local extraction produced 557 labels: 8 regions, 24 areas, 34 settlements and 491 landmarks. |
| Upscaling model | 90 assertions cover strict boot presets, bounded/atomic preference I/O, fresh live-process evidence, real larger dimensions, GPU completion/error guards, and matching boot token/new-PID confirmation after restart. |
| Temporal input guards | The release source passes 249 ASan/UBSan-backed configuration, cache, geometry, UV/cull, host-snapshot and GPU-write-order assertions. Passing these fixtures is separate from final live stability. |
| MetalFX GPU probe | RGB10A2 and FP16 inputs at 4/3×, 1.5× and 2× completed four GPU frames per combination, 24 total, with finite/color/alpha sampling checks. |
| Graphics model | Validate preset/strength/sharpness/source bounds and require a matching revision, live presenter process, fresh status, and an encoded applied/off result before confirming a look. |
| Metal pixels | 325 GPU assertions use synthetic textures for exact Off identity, crop/alpha, preset formulas, bounded sharpening, custom strength blending, nonfinite channels, invalid settings, compilation failure, and original-image fallback. |
| World metadata | Known game-name CRCs; direct and proxy flag traversal; separate S32/F32 arrays; reject invalid types/proxy signatures/permissions; base-shrine bounds. |
| Installer | 32 fixtures cover patch allowlists, atomic fresh/legacy/0.1.2 upgrade selection, header ownership/conflict checks, backups, version/process refusal, source-change detection and private-cache/label verification and reuse. The rune-selection revision adds a ninth managed header; its final integration rerun is pending. |

These checks use controlled test data. They establish validation and protocol behavior, not live-game outcomes.

Run the reproducible checks on an Apple silicon Mac with Xcode's command-line tools:

```sh
./scripts/test.sh
```

This runs the Swift model/UI/graphics/upscaling checks, the native C fixtures in `tests/runtime_checks.c`, horse and temporal sanitizer fixtures, real MetalFX and color-filter GPU probes, map-label fixtures and installer fixtures. The core C fixture compiles with `-Wall -Wextra -Werror` and currently passes **29,636 assertions**. It allocates synthetic pointer chains, clears game-control environment variables, and never opens BOTW or reads a gameplay profile. The separate horse suite passes **16,113 assertions** with ASan/UBSan. Build artifacts stay under the ignored `build/` folder.

To run only the native guard checks:

```sh
bash scripts/test-runtime.sh
```

Run the separate synthetic horse suite with `bash scripts/test-horse.sh`.

The Metal probe requires an actual GPU for pixel evidence. Local checks fail if no device is available; CI may report an explicit GPU skip when its host has no Metal device. An offline shader compile on such a host is a separate check. See [Shaders](SHADERS.md) for the contract and test scope.

## 0.1.3 live verification — 2026-11-05

The rebuilt BOTW 1.6.0 runtime was exercised using an isolated profile copy on
the local M4 Mac. Initial readbacks were Master Sword level 0 / normal attack 30 /
full power false, weapon/bow/shield capacity 9/5/4, and map access false. Native
readback and visible game behavior are distinguished below.

The original normal profile's 23 files matched their baseline after the first
installation and test startup. The player then explicitly chose to keep the
copy and its items. That copy was promoted byte-for-byte to the normal profile,
with the original 23-file profile retained as a backup. Follow-up tests use a
fresh copy of the kept profile and a new normal-profile baseline. Preservation
checks now protect that chosen baseline; they do not claim the original
progress was retained unchanged.

| Operation | Observed outcome |
| --- | --- |
| Canonical/internal sword grants | The profile already owned usable `Weapon_Sword_070`. Duplicate `070` was refused with the explicit owned-sword reason. Internal forms `056`, `071` and `072` were refused with the non-pouch explanation. This run does not claim a fresh canonical-sword grant. |
| Awakening level transactions | A level 1 request while the sword was equipped refused. After equipping another weapon in BOTW, levels 1/2/3 were acknowledged with readbacks 40/50/60 and full-power false/false/true. Returning 3→0 read back 30 / full-power false. |
| Visible sword attack | A reopened in-game pouch visibly showed 60. Captures for 40/50 showed gameplay rather than the attack panel, so they do not verify those displayed values. Visible 30/40/50 checks remain **pending**; all four native readbacks were verified above. Situational awakening remains separate. |
| Pouch capacity | Weapon/bow/shield capacity increased 9→20, 5→14 and 4→20 with successful native readbacks. Granting 10 swords, 6 bows and 5 shields produced 13 weapons / 8 bows / 5 shields, exceeding every original category limit. A decrease 20→9 refused and weapon capacity remained 20. |
| In-game map | Before the change, Tab left gameplay visible. `map_access` was acknowledged and read back true; pressing Tab afterward opened the actual BOTW map screen, which was visually inspected. |
| Rune selection and use | The nine ownership/Slate flags acknowledged successfully, but the selector still failed to open during a proper on-foot test. Source tracing found the separate native selection request. **Pending:** verify its requested/current/saved readbacks, visible selector and actual use after the new helper is installed. The default sequence is hold 3, choose, release, then F. |
| Revised shrine discovery | **Pending.** Confirm Open and Location readbacks, reopen the actual in-game map and inspect the marker/travel state, including the exact Dungeon001 exception. Earlier Open-only checks do not establish discovery. |
| Separate shrine clearing | **Pending.** Verify selected/all clear states, counter increments only for newly cleared targets, repeated-command idempotence, and unchanged reward/quest fields. |
| Horse speed | **Pending.** Live resource availability, 1.00/1.50/2.00× application and readback, visible ordinary-horse movement, exact Off restoration and restart reset. Synthetic table checks do not prove movement. |
| New editor controls and labels | **Pending.** Inspect the sword panel, capacity sheet, separate activation/clear actions, horse control and zoom-dependent map labels with fresh live state and unsupported-runtime states. Private previews containing extracted artwork remain outside public assets. |
| Save/reload persistence | **Pending.** An applied live edit does not establish persistence across a game save and reload. |
| Final artifact and profile preservation | An earlier local source build completed its checks and produced an app and archive. **Pending:** final build/artifact/signature readback, comparison against the new kept-profile baseline and follow-up-copy cleanup. The deliberately kept profile/items must remain. |

The expected release asset is `BOTW.Editor.zip` under `v0.1.3`. A local editor
build does not install the runtime bridge, establish the live outcomes above,
or prove that the matching GitHub release/workflow has completed.

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

The 0.1.0 source and build checks passed on GitHub Actions: [editor build](https://github.com/christianfurr/botw-editor/actions/runs/41937056576). The 0.1.2 final live replay and local artifact checks passed. The 0.1.3 live/artifact rows above must be completed separately; its matching GitHub Actions result belongs with the final release evidence.
