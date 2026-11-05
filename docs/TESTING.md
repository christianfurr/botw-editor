# Testing and release evidence

The editor, bridge, and game are separate verification surfaces. A successful compile, valid app signature, or matching request acknowledgement alone does not prove that a requested gameplay change behaves correctly.

## Compiled checks completed during development

| Surface | Evidence |
| --- | --- |
| Swift model | Inventory/destination and optional live-stat decoding; player-cache and travel-availability gates; north-up/single-marker projection; process/freshness/feature gates; separate stack/raw-durability bounds and equipped-weapon refusal; identifier and command-injection rejection. |
| UI value conversion | Exact integer-hundredths conversion between displayed durability points and native units; two-decimal-place validation, bounds, and point/unit round trips. |
| Backend memory access | Read/write/readback against test allocations; reject unmapped, read-only, and overflowing spans. |
| Backend protocol | Input thread queues requests without applying edits; bounded queue; reject unavailable gameplay edits; match status/off acknowledgements. |
| Health/stamina bridge | Synthetic pointer chains; current-health target; stamina refill and lock; clear locks when context disappears. |
| Inventory value bridge | Synthetic writes round-trip raw weapon durability 2,000 and 3,000; accept durability bounds 1–100,000,000 and stack bounds 1–999; reject out-of-range values, equipped weapons, unsupported types, and unavailable native APIs without writing. |
| World metadata | Known game-name CRCs; direct and proxy flag traversal; separate S32/F32 arrays; reject invalid types/proxy signatures/permissions; base-shrine bounds. |
| Installer | Twelve fixture checks cover patch detection, exclusive clone backups, read-only preflight, source ownership, version/active-game refusal, source-change detection, private-cache hashes, cache reuse without writes, missing-artwork extraction without a runtime rebuild, and extraction-output verification. |

These checks use controlled test data. They establish validation and protocol behavior, not live-game outcomes.

Run the reproducible checks on an Apple silicon Mac with Xcode's command-line tools:

```sh
./scripts/test.sh
```

This runs the Swift model checks, the native C fixtures in `tests/runtime_checks.c`, and the installer fixture tests. The C fixture compiles with `-Wall -Wextra -Werror` and currently passes 196 assertions. It allocates synthetic pointer chains, clears game-control environment variables, and never opens BOTW or reads a gameplay profile. Build artifacts stay under the ignored `build/` folder.

To run only the native guard checks:

```sh
bash scripts/test-runtime.sh
```

## Live verification — 2026-11-04

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

Keep saves, extracted artwork, memory, raw logs, private paths, and gameplay screenshots out of the public repository. Public screenshots show only the editor's Player tab, which uses original interface graphics and system symbols.

## Release checks

- Run the source build and its model checks against the final sources.
- Run bridge guard/protocol tests against the final runtime headers.
- Run `python3 scripts/test_install_bridge.py`. The test fixtures do not launch or rebuild BOTW.
- Inspect the built editor at standard and reduced window sizes, including disconnected/loading states.
- Verify the app's architecture, deployment target, signature, and bundled catalog/artwork licenses.
- Test the bridge installer against a supported checkout; verify that unsupported versions and dirty/conflicting integration states stop safely.
- Inspect the release archive and tracked tree for game files, keys, firmware, saves, logs, dumps, private paths, and unintended assets.
- Update this document with actual live results and the final release revision before describing provisional features as verified.

Release source and build checks passed on GitHub Actions: [editor build](https://github.com/christianfurr/botw-editor/actions/runs/41937056576).
