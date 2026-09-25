# Stage 20 - Scaling, keyboard and accessibility

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 21 is next.

## Design sources

See [07-win98-design-reference.md](../07-win98-design-reference.md), section 3.10:

- **Tab order:** PDF p.161.
- **Access keys:** PDF p.46-47, p.161, p.328.
- **Tab keys:** PDF p.147, p.400.
- **Focus:** PDF p.50, p.325.
- **High Contrast:** PDF p.373-374.
- **Scaling:** PDF p.317, p.327, p.342, p.374.
- **Palette placement:** PDF p.160, p.181.
- **Titles:** PDF p.372.

## What changed

### Keyboard (SYS-31)

- **Access keys:**
  - A control's underlined letter is named by its `accesskey` attribute and drawn with the `w98-ak` underline.
  - `AccessKeys.h` handles Alt+letter anywhere. The plain letter works when the focused control does not take text and sits in a Windows 98 window. Outside those windows, letters stay with the game.
  - The key has the effect of a click. A label passes it to its control: a text box, drop-down list or slider takes the focus, and a check box toggles.
  - Access keys are unique within each visible page, and OK and Cancel have none.
- **Where access keys were added:**
  - the main menu (C, L, T, Q, u, S, x);
  - Pause (R, S, L, t, M);
  - every Custom Game wizard control and < Back / Next > / Finish (B, N, F);
  - Load Game (Look in, Open);
  - every Settings control and Defaults;
  - message-box answers (Yes, No, Retry, set at run time by `ModalDialog`).
- **Tab strips:** Ctrl+Page Down and Ctrl+Page Up now move between tabs, as Ctrl+Tab and Ctrl+Shift+Tab already did. Left and Right move between tabs when a tab has the focus.
- Earlier stages cover Enter, Esc, and tab order within pages.

### High Contrast (SYS-31)

- **Detection:**
  - The game reads the Windows High Contrast setting (`SPI_GETHIGHCONTRAST`) at start-up and then once a second, so a change applies without a restart.
  - `INGNOMIA_AUTOMATE_HIGH_CONTRAST=1` forces it on for tests.
- **Where it applies:** `RmlUiHost` marks every document in the game window and in every detached window with `is-high-contrast` on each update. Documents that bindings load later are covered as well.
- **Colours:** the Windows 98 chrome switches to the High Contrast White colours:
  - white faces, black text and one-pixel black edges;
  - a black caption, and a black selection with white text;
  - option-set toolbar buttons in black.
- **Removed:** the dither and gradients.
- **Kept:** the black glyphs (check marks, arrows), which stay readable.
- **Precedence:** the rules carry a `body.` prefix, so they outrank the older, dark high-contrast rules for pre-Win98 components.

### Scaling and fit (SYS-30)

- Every window was already sized in em of the snapped 11/22/33 px system font and drawn at whole-number scales (Stages 09-19). Each stage's suite checks that its pages fit at 100/125/150/200%.
- Stage 20 adds a check that these fit a 640 x 480 screen at 1x: the main menu, the Custom Game wizard, Settings, Load Game and the message box.

### Palettes remember their place

- The Build palette and the creature, tile and blueprint inspectors open at a fixed offset from the game window the first time. After that they open where the player last put them.
- The position is saved in the configuration when the palette closes (`UiWindow.<kind>.position.x/y`).

### Titles

Every window document has a title. Six had none: HUD, inspector, Build, Stockpile, Workshop and Agriculture.

## Verification

| Gate | Result |
| --- | --- |
| Stage 20 focused suite | **4/4 ctest; 182 checks in `ui_Stage20`.** [Output](../evidence/stage-20/acceptance.txt). Checks:<br>- access keys unique and underlined once on every main menu, wizard, Settings and Load Game page;<br>- a plain U opening Custom Game; Alt+K reaching the Kingdom name box through its label; N typed into a text box instead of pressing Next;<br>- Alt+N and Alt+B for Next and Back; R pressing Random Name; P toggling the check box;<br>- Ctrl+Page Down, Ctrl+Page Up and Right on Settings tabs;<br>- Yes and No access keys, with N answering No;<br>- High Contrast colours on the main menu;<br>- fit on 640 x 480;<br>- a title for all 15 window documents. |
| Regressions | Stages 04-19 pass. Tests that compared label text now strip the underline markup. |
| Live copied-world runs | Four production runs at 100/125/150/200% from the main menu (**17/17 checks each**), plus one with High Contrast forced (17/17). [Runs](../evidence/stage-20/runs.txt).<br>Real Qt key events went through the game window: Alt+L opened Load Game; Esc closed it with the focus back on Load Game; Alt+C continued into the copied save.<br>The run then opened Population and the Build palette. It moved the palette 60 x 40 px and closed it, and the saved position matched. |
| Save safety | All 21 source and copied save files unchanged. |
| Build | `Ingnomia.exe` SHA-256 `9BBE099479ED823AA6CAD2BA8F46F64E7198279DB85249F1BA5418DB2007D6DF`. |

High Contrast captures: [main menu](../evidence/stage-20/high-contrast/main-menu.png), [Load Game](../evidence/stage-20/high-contrast/load-game.png), [HUD](../evidence/stage-20/high-contrast/hud.png), [Population](../evidence/stage-20/high-contrast/population.png), [Build](../evidence/stage-20/high-contrast/build.png). Standard captures are under `1/`, `1.25/`, `1.5/` and `2/`.

## Evidence limits

- Keys were injected as Qt key events into the production window; mouse and keyboard hardware input is not claimed. The Windows High Contrast switch itself was forced by the test variable, not toggled in Windows.
- **Access keys not yet added:** the property sheets from Stages 09-16 (workshop, stockpile, agriculture, population, military, diplomacy, inspectors) and the HUD toolbar. They are reachable by Tab, arrow keys and Ctrl+Tab. This is recorded for the Stage 21 conformance checklist.
- The High Contrast Black scheme is shown with High Contrast White colours; the palette follows the setting, not its scheme.
- Always on Top on a palette shortcut menu is not provided. Palettes are owned windows, so they already stay above the game window.

## Next

Stage 21: cleanup, conformance checklist and final handoff.
