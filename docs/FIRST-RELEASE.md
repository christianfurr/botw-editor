# First stable BOTW Editor release

Scope is the Apple silicon editor for MacNativeGames BOTW 1.6.0 and its supported bridge. The 0.1.5 pre-release is the baseline. MacNative Mods has a separate release and test matrix.

| Priority | Work | Acceptance check |
| --- | --- | --- |
| P0 | Make setup and recovery discoverable | Added a disconnected setup link and a Help menu entry. Inspected the offline editor at 1000 and 1280 points wide, plus the waiting and full-pouch states, using local SwiftUI snapshots. |
| P0 | Harden requests across a game restart | Added a session-bound acknowledgement check. An isolated test verified that a reply from a new PID and a stale reply cannot confirm an edit, and that a disconnect is reported without an automatic status request replacing the result. |
| P0 | Show and honor the armor limit | Checked against the live game on a disposable copy. The editor left an owned upgraded Champion's Tunic in place, stopped after one grant on request, then added nine base pieces and stopped at the 100-slot cap. |
| P0 | Rehearse save recovery | Saved the edited copy, observed four changed save files, restored its backup, and relaunched to the original armor inventory. All 14 normal save files matched their pre-test checksums. |
| P1 | Verify distribution | The candidate build suite passed. Installer preflight recognized a pristine supported checkout as needing setup and confirmed the already-installed runtime and three private caches. Both checks left the checkout and app untouched. |
| P1 | Sign, notarize, and test a public stable build | Pending Developer ID Application signing and Apple notarization. Open the downloaded, stapled ZIP on a supported clean Mac with Gatekeeper enabled and follow the setup guide from scratch. |

The stable release should preserve the existing Inventory, Player, World, and graphics scope. A faster native batch protocol, new game versions, and more mod-manager features can ship independently once they have their own verification.

## Sign and verify the final download

Build from the release revision. On a Mac with a usable Developer ID Application identity and a configured notarytool keychain profile, work on a copy of the built app. Replace the placeholders below with the local identity and profile names.

```sh
codesign --force --options runtime --timestamp --sign "<Developer ID Application identity>" "BOTW Editor.app"
codesign --verify --strict "BOTW Editor.app"
ditto -c -k --keepParent "BOTW Editor.app" "BOTW.Editor.notarization.zip"
xcrun notarytool submit "BOTW.Editor.notarization.zip" --keychain-profile "<notary profile>" --wait
```

After Apple reports **Accepted**, staple and validate the app. Rebuild the ZIP from the stapled app and generate the checksum for that ZIP.

```sh
xcrun stapler staple "BOTW Editor.app"
xcrun stapler validate "BOTW Editor.app"
spctl --assess --type execute --verbose "BOTW Editor.app"
ditto -c -k --keepParent "BOTW Editor.app" "BOTW.Editor.zip"
shasum -a 256 "BOTW.Editor.zip" > "BOTW.Editor.zip.sha256"
```

Test a fresh download before promoting the release. Verify the checksum and confirm that the editor opens, the bridge installs on the supported game build, and the editor connects after loading a save. [Apple notarization documentation](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
