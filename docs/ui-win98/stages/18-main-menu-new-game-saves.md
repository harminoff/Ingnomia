# Stage 18 - Main menu, new game and save browser

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 19 is next.

## Design sources

See [07-win98-design-reference.md](../07-win98-design-reference.md), section 3.8:

- **Main menu:** a dialog box (PDF p.169, p.346). The book has no title screen, so this is the nearest documented form.
- **Custom Game:** an advanced wizard (PDF p.304-309).
- **Load Game:** follows the Open dialog box (PDF p.170-172).
- **Sliders:** PDF p.146.

The screens sit on the Windows Standard desktop colour (#008080).

## What changed

### Main menu (APP-01)

`screens/main_menu.rml` is a dialog box captioned "Ingnomia".

- **Header:** the product line and the version.
- **Commands:** one column of equal-width buttons, in groups separated by lines:
  - Continue, Load Game...
  - Tutorial, Quick Start, Custom Game...
  - Settings, Exit
- **Descriptions:** each command has a one-line description beside it. Continue shows the save it will open, or why it is unavailable.
- **Ellipsis:** only where more input follows (Load Game..., Custom Game...).
- **Default command:** Continue has the focus once the save scan finds a save. Otherwise Load Game has it.
- **Fixed (NEW-002):** returning from Custom Game or Load Game gives the focus back to the command that opened it.

### Custom Game wizard (APP-02, APP-03)

`screens/new_game.rml` is a Wizard 97 style wizard captioned "Custom Game", at the book's size of 317 x 193 DLU (476 x 314 px).

- **Pages:**
  - **Welcome:** a watermark band and a bold title.
  - **World:** world size, levels, ground level, flatness.
  - **Settlement:** kingdom name with Random Name, seed with Random Seed, gnomes, starting zone, and the Peaceful beginning check box.
  - **Terrain and Life:** ocean, rivers, river size, trees, plants, wild animals.
  - **Completion:** "You chose these settings:" followed by a summary.
- **Interior pages:** each has the white header with a title and a one-sentence question.
- **Number controls:** each is a "Label:" with a slider, a spin box for the exact value, and a unit.
- **Buttons:**
  - < Back is unavailable on Welcome. Finish takes the place of Next > on Completion. Cancel returns to the main menu.
  - Enter chooses Next, or Finish on Completion.
- **Validation:** Next keeps a page open while its typed numbers are out of range. Finish returns to the page with the invalid value and focuses it.
- **Defaults:** every page starts with the recommended settings. Quick Start remains the one-step path.
- **Retired:** the vertical tab rail, "STEP n OF 4" and the badges.

### Load Game (APP-04)

`screens/load_game.rml` is modelled on the Open dialog box.

- **Look in:** a drop-down that chooses the kingdom (its folder).
- **Save list:** a list view in details view with Name, Modified (a Windows 98 short date and time) and Version.
- **Save name:** a read-only box that names the chosen save.
- **Open:** the default button beside Cancel.
- **Keys and mouse:** a double-click opens a save, and F5 refreshes the list.
- **Incompatible saves:** Open is unavailable, and the reason is stated ("This save was made by version X and cannot be opened by this version.").

### Fixes found on the way

- **Load list rebuilt under a click:** the save list was rebuilt on every state change, including the click that selected a row. It is now rebuilt only when its rows change, and selection restyles the rows in place. This is the same use-after-free class as the Build window.
- **Request left pending:** the save-list requests (`load.refresh`, `load.select_kingdom`) were counted as pending, but nothing ever finished them. From start-up this left Open and Finish unavailable for the whole session. They now count as complete once posted, because their data arrives on its own.
- **Focus taken at start-up:** the game window's HUD and inspector overlays took the keyboard focus from the main menu when they were shown. They now show without taking the focus. Detached windows still take it.
- **Pressed look for held keys:** buttons held with Space or Enter now show the pressed look (`is-key-pressed`), as a mouse press does.

## Verification

| Gate | Result |
| --- | --- |
| Stage 18 focused suite | **4/4 ctest; 199 checks in `ui_Stage18`.** [Output](../evidence/stage-18/acceptance.txt).<br>Main menu: a dialog with one column of equal-width commands and ellipses only where more input follows; Continue unavailable with a reason, then focused once found.<br>Custom Game: wizard pages; Back, Next and Finish availability; Enter as Next; Random Name; fit at four densities; Cancel returns the focus (NEW-002).<br>Load Game: Look in; rows with three columns; selection in place (the row element survives); Save name; Open availability and the incompatible reason; F5; double-click opens; fit.<br>The shell RML contract was rewritten for these screens. |
| Regressions | Stages 04-17 pass. Stage 04 and Stage 05 were updated to test the wizard (Next and Enter, hidden pages out of Tab order, the invalid page kept open) instead of the retired New Game tabs. Connected tabs are still covered by the shared fixture. |
| Live copied-world runs | Four production runs at 100/125/150/200%, starting at the main menu with a fresh 21-file Tutorial Valley copy: **33/33 checks each**, 7 captures each. [Runs](../evidence/stage-18/runs.txt).<br>The run covers: the main menu with Continue focused and nothing pending; the whole wizard with Back and an available Finish; Cancel with the focus returned; Load Game with Look in, a selected save and Open available; and a double-click that opened the save and entered the game. |
| Save safety | All 21 source and copied save files unchanged (opening a save does not write it). |
| Build | `Ingnomia.exe` SHA-256 `6F2771A4E75CD535C77601A202AFFE702688ED31732A4E493B4D8C09F797C2D9`. |

Captures at 1x: [main menu](../evidence/stage-18/1/main-menu.png), [Welcome](../evidence/stage-18/1/wizard-welcome.png), [World](../evidence/stage-18/1/wizard-world.png), [Settlement](../evidence/stage-18/1/wizard-settlement.png), [Terrain and Life](../evidence/stage-18/1/wizard-terrain.png), [Completion](../evidence/stage-18/1/wizard-completion.png), [Load Game](../evidence/stage-18/1/load-game.png). The same set is under `1.25/`, `1.5/` and `2/`.

## Evidence limits

- Input was injected through production RmlUi and Qt adapters. Physical input is not claimed.
- Finish was not pressed live, because generating a world is slow. The focused suite covers Finish and its revalidation.
- **Not provided, because the game has no command for them:** deleting a save from a shortcut menu, and a Save As dialog. Saving happens from the Pause menu (Stage 19).
- The draft's default gnome count comes from the game's configuration, not from this UI.

## Next

Stage 19: settings, pause and loading, plus NEW-004.
