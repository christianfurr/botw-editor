# Decisions

- 2026-11-04: Support Nintendo Switch BOTW 1.6.0 through MacNativeGames commit `b1a49a2` first; refuse unknown versions rather than guessing layouts.
- 2026-11-04: Use a native SwiftUI companion and a local runtime bridge; no external service or standard Switch cheat-file interpreter is required.
- 2026-11-04: Queue edits from the command-file thread and apply them on the game's presentation thread; report success only after a matching request acknowledgement and readback.
- 2026-11-04: Keep locks off by default and disable edits when the game session, bridge, or required pointers are unavailable.
- 2026-11-04: Treat shrine unlock as activation for travel. Full Access raises towers and skips the opening tower activation sequence; it leaves shrine-clear and story-quest completion fields alone.
- 2026-11-04: Preserve an application/profile restore point before bridge installation or destructive progress changes.
- 2026-11-04: Publish only original editor assets, source, bridge patches, and editor-only screenshots; exclude all player game files and data.
- 2026-11-04: Keep compiled checks and live gameplay verification separate in release claims.
- 2026-11-04: Store installer ownership records and backups under Application Support, outside the MacNativeGames checkout; preserve unrelated dirty files and refuse unowned differing headers.
- 2026-11-04: Disclose ad hoc signing and use Apple's manual Open Anyway flow for downloaded releases; do not disable Gatekeeper or remove quarantine attributes.
- 2026-11-04: Display weapon durability in points and convert to native integer units (100 per point; maximum 100,000,000 raw units). Keep arrow/material stacks capped at 999 and reject equipped weapon edits in both the model and runtime.
- 2026-11-04: Extract terrain and item artwork only from the player's local game into Application Support. The app and repository contain no extracted art; public screenshots use the editor's Player or Graphics controls.
- 2026-11-04: Keep known-destination travel separate from arbitrary map coordinates. The map uses fixed terrain bounds and can show the live player position.

- 2026-11-04: Accept armor's native -1 no-dye sentinel separately from editable weapon/stack values; expose inventory readiness and its actual blocker instead of assuming the game menu is open.
- 2026-11-04: Keep catalog browsing available offline and give quantity/result controls a persistent place in the icon-grid sheet; only mutations require a ready inventory.
- 2026-11-04: Apply optional native Metal color/sharpness filters to final SDR output. Default Off preserves the original image; custom shader failure falls back to it, and applied status requires the matching presenter revision.
- 2026-11-04: Support a 0.1.0 installation upgrade by applying only missing allowed source hooks and adding the owned presenter header; refuse conflicting edits rather than replacing them.
