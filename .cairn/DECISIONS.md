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

- 2026-11-04: Keep MetalFX Temporal upscaling separate from final-picture color filters; expose the camera-derived-motion limitation and report actual dimensions/completed GPU work without a DLSS-quality or FPS promise.
- 2026-11-04: Store one canonical restart-required preset for both scene-resolution and temporal paths. Confirm a saved selection only against a fresh new PID with its immutable matching boot token and real output evidence.
- 2026-11-04: Treat synthetic guard passes, probe completion, candidate live execution and final stable replay as separate evidence; repeated fallback/history resets block a final stability claim.

- 2026-11-05: Treat `Weapon_Sword_070` as the usable Master Sword; refuse internal world/appearance forms and duplicate grants with specific reasons. Require ownership and unequipped state before applying a level.
- 2026-11-05: Configure sword awakening only through the native additive-power/full-power pair. Preserve beam, durability, recharge and quest fields; distinguish normal attack from the game's situational awakening.
- 2026-11-05: Expose pouch capacity as three explicit increase-only actions within native category bounds. Capacity edits neither grant equipment nor modify Korok/Hestu progress.
- 2026-11-05: Keep in-game Slate access separate from terrain reveal and Full Access. Set only the two repeatable PortableUnit flags verified in the pickup event; do not infer door, quest or permission flags.
- 2026-11-05: Make new readbacks optional and gate each new command by its runtime capability so an older bridge remains readable and requests an update for unsupported controls.
- 2026-11-05: Treat shrine travel activation as both Open and positive Location discovery; preserve positive per-store discovery values. Limit the native read-only discovery exception to exact NX160 Dungeon001 metadata and verified permission-forwarding instructions.
- 2026-11-05: Keep explicit shrine clearing separate from activation and Full Access. Increment the clear counter only for newly cleared targets; require agreement between stores and preserve Spirit Orbs, Enter, quests, chests and rewards.
- 2026-11-05: Include both verified Slate-access booleans in base-rune acquisition. Use the port's default hold-3/select and F/use instructions; verify selection in unmounted gameplay rather than inferring it from flag readback.
- 2026-11-05: Handle rune ownership and selection as separate steps. Request Bomb 0 only when both requested/current selection are absent, using the native nonblocking mutex path; let the normal game update apply and persist it. Preserve valid selections and report a post-ownership busy/failure as unconfirmed selection rather than no changes.
- 2026-11-05: Apply horse speed to four land gears in the shared table for 26 standard horse types, bounded to 1.00–2.00×. Keep it session-only, restore owned originals with Off and refuse unknown resource or competing changes; leave special mounts, other animals and saves alone.
- 2026-11-05: Extract place labels from the player's original map positions and localized text into the private cache. Declutter them by zoom and keep labels separate from known travel destinations and public assets.

- 2026-11-06: Reuse the pinned MacNativeGames RomFS tools for BOTW 1.6.0. Separate the editor library, prepare overlays under the native game-slot lock, retain prior overlays, and report preparation separately from a successful game launch.
- 2026-11-06: Import complete Switch RomFS and merged BCML exports. Leave raw BNP and within-file merging to BCML or UKMM. Keep local demo assets outside the repository and match stock resource sizes.
