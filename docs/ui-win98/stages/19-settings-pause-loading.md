# Stage 19 - Settings, pause and loading

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. NEW-004 fixed. Stage 20 is next.

## Design sources

See [07-win98-design-reference.md](../07-win98-design-reference.md), section 3.9:

- **Settings:** a property sheet, but every setting takes effect at once. The documented model for that is the property inspector (PDF p.167), so the sheet has Close instead of OK / Cancel / Apply.
- **Sliders:** PDF p.146.
- **Pause:** a dialog box (PDF p.169). Esc resumes, because Esc is Cancel (PDF p.48).
- **Loading:** a progress message box (PDF p.145, p.184).
- **Pointer:** the hourglass while the window cannot respond (PDF p.356).
- **Message boxes:** PDF p.182-185.

## What changed

### Settings (APP-05, APP-06)

`screens/settings.rml` is a 384 x 380 property sheet captioned "Settings".

- **Pages:**
  - **Display:** a Screen group (Full screen, Match the monitor refresh rate, Frame rate limit) and an Interface and map group (Interface size, Minimum light).
  - **Controls:** a Camera group (Pan speed, "The mouse wheel changes the level") and a Language note.
  - **Sound:** Master volume.
  - **Saving:** Save every n days, and "Keep time running after an autosave".
- **Sliders:**
  - A "Label:" on the left and the current value on the right.
  - Parallel range words under the track (Low / High, Small / Large, Dark / Bright, Slow / Fast, Quiet / Loud).
  - The book documents no numeric readout. The value on the right follows the Windows 98 Display Properties "Screen area" slider, and this is recorded as a period observation.
  - Frame rate limit is unavailable while the monitor refresh rate is matched.
- **Buttons:** Defaults restores every setting, and "Changes take effect at once." is stated beside Close. Pages never scroll.

### Pause (APP-07)

`screens/pause_menu.rml` is a dialog box captioned "Pause" over the paused map.

- The first line shows the pause state, and a second line shows the save status.
- **Commands:** Resume, Save Game, Load Game..., Settings, and (set apart) Main Menu, each with a description.
- Esc resumes. Settings opened from Pause also sits over the map, and its Close returns to Pause.
- **Message boxes:** Main Menu and Exit now ask in the Windows 98 message box: captioned "Ingnomia", Warning symbol, the whole question as text, and Yes / No, with No focused.
- **Fixed:** a finished world transition (Continue, Load or Start) never cleared its own pending request, so every Pause command stayed unavailable in a loaded game. `finishWorldTransition` now answers it.

### Loading (APP-08)

`screens/loading.rml` is a message box captioned "Preparing Kingdom".

- **While loading:**
  - It has the Information symbol and the game's current step in words. The game reports steps, not a fraction, so a bar would be invented.
  - It has no button, because the load cannot be stopped.
  - The pointer is the hourglass (`cursor: wait`, now mapped to the Windows wait cursor).
- **On failure:** it becomes a Warning message box. It says what failed in one sentence, how to leave, and offers Retry (available only when the game allows it) and Cancel. The pointer returns to the arrow.

### NEW-004: a missing or damaged save no longer crashes

- **Root cause:** the loader read whatever tiles `world.dat` held and never checked them against the world size from `game.json`. If files were missing or deleted while being read (as when a disposable copy was removed under a run), the world vector no longer matched its dimensions, and later tile lookups ran out of bounds.
- **Fix:**
  - `GameManager::loadGame` checks that `game.json` and `world.dat` exist before tearing down anything or building a game around them.
  - `IO::load` refuses an unreadable `game.json`.
  - `IO::loadWorld` refuses a `world.dat` whose tile count does not match the world size.
  - Each case fails the load cleanly, and the shell shows the failure message box.

## Verification

| Gate | Result |
| --- | --- |
| Stage 19 focused suite | **4/4 ctest; 144 checks in `ui_Stage19`.** [Output](../evidence/stage-19/acceptance.txt).<br>Settings: pages, values, a setting applying at once, Close only, pages fit at four densities.<br>Exit: a Yes / No message box captioned Ingnomia; No keeps the game open.<br>Loading: no buttons while loading; Warning with Retry and Cancel on failure; the failure reads as one sentence; Cancel returns to the menu.<br>Continue's request is answered by the finished transition.<br>Pause over the map with its commands available; Settings from Pause and back; Main Menu asks and No stays; Esc resumes.<br>The shell RML contract gained Settings, Pause and Loading checks. |
| Regressions | Stages 04-18 pass. The Stage 07 exit dialog now expects Yes / No, following the section 3.4 rule, instead of "Exit". |
| Live copied-world runs | Four production runs at 100/125/150/200% from the main menu: **34/34 checks each**, 8 captures each. [Runs](../evidence/stage-19/runs.txt).<br>The run covers: the Settings pages and a setting changed and restored; Close with the focus on Settings; Continue into the copied save; Esc to Pause over the map; Settings from Pause; Main Menu asking with Yes / No; No; and Esc to resume. |
| NEW-004 live | Loading a save folder that does not exist, and a copy whose `world.dat` is cut in half, **5/5 checks each**. The game logged "world.dat does not match the world size 204801 tiles, expected 409600" and showed the failure message box; Cancel returned to the menu. No crash. |
| Save safety | All 21 source and copied save files unchanged. The truncated copy is a separate folder. |
| Build | `Ingnomia.exe` SHA-256 `7EACA17327545944805051AD3439D3D043EC0F9E5633FDF58851C4BA1B97DF29`. |

Captures at 1x: [Settings: Display](../evidence/stage-19/1/settings-display.png), [Controls](../evidence/stage-19/1/settings-controls.png), [Sound](../evidence/stage-19/1/settings-sound.png), [Saving](../evidence/stage-19/1/settings-saving.png), [loading](../evidence/stage-19/1/loading.png), [Pause](../evidence/stage-19/1/pause.png), [Settings in the game](../evidence/stage-19/1/settings-in-game.png), [Main Menu message box](../evidence/stage-19/1/leave-confirmation.png), [failed load](../evidence/stage-19/missing-save/loading-failed.png).

## Evidence limits

- Input was injected through production RmlUi and Qt adapters. Physical input is not claimed.
- **Not done in the live run:**
  - Save Game, because it would write the save. Save status text is covered by existing shell tests.
  - Loading another save from Pause.
- The unsaved-changes prompt ("Do you want to save changes to <save>?" with Yes / No / Cancel) would need a save-then-leave command that the game does not have. Main Menu and Exit state that unsaved progress may be lost and ask Yes / No.

## Next

Stage 20: scaling, keyboard and accessibility.
