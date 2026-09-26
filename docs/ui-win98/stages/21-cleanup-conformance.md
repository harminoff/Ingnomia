# Stage 21 - Cleanup, migration of the early windows, localization and conformance

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. This is the last stage of the plan.

## Design sources

See [07-win98-design-reference.md](../07-win98-design-reference.md):

- **Conformance checklist:** section 3.11.
- **Stockpile property sheet:** section 3.12.
- **Inventory report window:** section 3.13.
- **Localized text and access keys:** section 3.14.

The owner decided two scope questions at the start of this stage (2026-09-25):

- **Stages 08 and 09:** the Stockpile and Inventory windows were built before the documented-only rule, so they are migrated to the Stage 10 standard before the commit.
- **Localization:** every string in the Windows 98 documents goes back under catalog keys, with the access key inside the string.

## What changed

### 21a - Stockpile property sheet (replaces the Stage 09 body)

- **Window:** "<Name> Properties", 384 x 380 px at 1x, tabs Contents / Allow List / General, and OK / Cancel / Apply.
- **Pending model:** name, priority, the three General check boxes and every Allow List check box stay pending until Apply or OK.
  - Apply sends one `stockpile.set_basics` for the General fields that changed.
  - It sends at most two `stockpile.set_filters` batches (the rules to allow, the rules to block).
  - It waits for the game's snapshot. A change made elsewhere is a conflict, reported in a message box.
  - Close with pending changes asks Yes / No / Cancel.
- **Contents:** Find, a Category drop-down list, and a list view (Item, Material, Stored, Total) sorted by clicking a heading.
- **Allow List:**
  - Find and Category, then a virtualized list view with a check box per rule (Item, Material, Type).
  - Allow All and Block All stage the rows shown, so no review dialog is needed; the count beside them names the scope.
  - Space toggles the selected rule, and arrows, Page Up/Down, Home and End move the selection.
- **Templates:** a drop-down combo box (a new shared `w98-combo` control, PDF p.140) with Load and Save.
  - Both act at once on the applied allow list, and each asks Yes / No before replacing anything.
  - Both are unavailable, with a line of explanation, while rule changes are pending.
- **Removed:** header filter combos, section headings and help paragraphs, the bulk review dialog, "Back to rule search", Apply / Revert on the settings page, the tooltip element, and the item sprites (text-only rows, as in the other Windows 98 lists).
- **Controller:** `StockpileDraft` now carries `StockpileOptions` and a map of pending rule changes, rebased on every snapshot like the workshop draft. `setStockpileBasics`, `setStockpileFilterMatches` and `commitStockpileMatches` are gone.

### 21b - Inventory report window (replaces the Stage 08 body)

- **Window:** "Inventory", 384 x 380 px at 1x, with Close only.
- **List:**
  - Find, a Category drop-down list and a list view (Item, Material, In Stock, Total).
  - The check box on each row is the watch state and acts at once. This closes **NEW-001**: watching now has a visible control.
  - "Show only items the settlement has" check box, and a count ("N of M items").
  - Properties, Enter or a double-click opens the item.
- **Item Properties:** a subordinate property sheet drawn over the window, with Close only.
  - **General:** counts and "Watch this item".
  - **Stockpiles:** list view, and Properties opens the stockpile.
  - **Recipes:** "Made by:" and "Used to make:"; Properties opens a product.
  - **History:** list view with day, total, made and used.
  - This replaces the in-place detail view with "Back to inventory".
- **Removed:** header filter combos, the in-place detail and its back stack, the history bar chart, sprites and the tooltip.

### 21c - Localized text and access keys

- **Keys:** every literal in the 16 Windows 98 documents now has a `data-l10n` key (597 strings), and tooltips have `data-l10n-title`.
  - The text lives in `src/gui/ui/localization/UiTextWin98Entries.inc`, included by all three catalogs (shell and game, Population/Inventory, Military/Diplomacy).
  - `en.json` and the `qps-long` pseudo-locale carry every key.
- **Access keys travel with the string:** "&Name:" underlines N and sets `accesskey` on the owning label or button when the text is applied, and "&&" is a literal ampersand (PDF p.214).
  - Stage 20's letters are kept.
  - Every property sheet, palette and wizard control now has one, unique within its window and page.
  - OK, Cancel and Close have none, and A is kept for Apply wherever Apply exists.
  - This closes the Stage 20 gap for property sheets.
- **Drop-down options:** `applyRmlText` now localizes drop-down list options, which RmlUi keeps outside the child list.
- **Removed:** 440 static keys and about 430 catalog entries that only the retired pre-Windows 98 markup used.
- **Released-literal contract:** extended from 10 to all 18 released documents. The head `<title>` and the caption Close glyph are documented technical text.

### Cleanup

- **Stale foundation contracts:** hot-reload order, inspector wiring, tutorial actions and registry counts were stale since the base commit. The canonical build now passes **13/13** (was 10/13).
- **Retired test and styles:**
  - The unregistered 6C RmlUi layout test was removed.
  - 680 dead RCSS rules were removed from `shell.rcss`, `management6a/6b/6c.rcss`, `management_window.rcss` and the template, plus 351 rules of the retired Stockpile and Inventory bodies.
  - Legacy contracts no longer require retired rules.
- **Fonts:** the Stage 04-08 test harnesses now load the MS W98 UI font. Without it, RmlUi's text input crashed on cursor movement, because a field with no font face has no text lines.
- **Message boxes:**
  - An entry out of range in a spin box (priority, pasture limits) is reported in a message box naming the field and range. Before this, the red inline text appeared on the workshop and stockpile, and the pasture limits gave no report at all.
  - A command the game refuses is shown in a message box on the Stockpile sheet.
- **Probes:** the Stage 09 and form probes follow the pending model. A new Stage 21 Inventory probe and `MainWindow::inventoryStage21Probe` were added.

## Verification

| Gate | Result |
| --- | --- |
| Stage 21 focused suite | **4/4 ctest; 1971 checks** in `ui_Stage21`. [Output](../evidence/stage-21/localization/acceptance.txt). Checks:<br>- all 16 documents load with their production catalog;<br>- no missing-key marker in any keyed string;<br>- each access key underlined once, on its own letter;<br>- letters unique per window and page;<br>- no access key on OK, Cancel or Close;<br>- A only on Apply where Apply exists;<br>- Alt+F and Alt+C reach the Stockpile Find box and Category list. |
| Stockpile (Stage 09 suite rewritten) | **10/10 ctest; 123 checks**:<br>- pending General and rule changes, with one Apply payload each;<br>- conflicts, rejection, Close prompt and Cancel;<br>- templates with Yes/No;<br>- the combo box;<br>- virtualization of 2,000 rules;<br>- fit at four densities. |
| Inventory (Stage 08 suite rewritten) | **10/10 ctest; 91 checks**:<br>- 10,000 virtualized rows, sorting, Find, Category and owned-only;<br>- Space to watch, Enter to Properties, and all four Item Properties pages fitting at four densities;<br>- watch, stockpile and product Properties;<br>- removal of a shown item. |
| Regressions | Stages 04-20 all pass. Legacy suites (6A, 6B, 6C, HUD, shell, inspector, accessibility, debug) all pass. Canonical build ctest 13/13. |
| Live copied-world runs | **Stockpile:** 35/35 checks at 100/125/150/200%. The probe creates a one-field stockpile and runs through:<br>- invalid priority;<br>- name, priority and hauling;<br>- Block All then Apply, and Allow All then Apply (107 rules changed);<br>- template save, No, and Yes;<br>- suspension, with the inspector and manager snapshots agreeing.<br>**Inventory:** 18/18 at four scales: toolbar, Find, Properties, four pages, watch twice, Close.<br>**Stage 20 menu probe:** 17/17 after localization (Alt+L, Esc, Alt+C). |
| Save safety | All 21 source and copied save files unchanged in every run. |

Captures: [Stockpile](../evidence/stage-21/stockpile/) and [Inventory](../evidence/stage-21/inventory/) at each scale, with traces. [Localization](../evidence/stage-21/localization/) holds the main menu, Load Game, HUD and Population with catalog access keys.

## Conformance checklist (design reference section 3.11)

| Check | Result | Evidence |
| --- | --- | --- |
| Window type matches the task | Pass | Property sheets (workshop, stockpile, agriculture, population, military, diplomacy, settings); palettes (inspectors, Build, tutorial); dialog boxes (main menu, pause, Load Game); wizard (Custom Game, missions); message boxes. Inventory is a Close-only report window with a subordinate Item Properties sheet. Stages 10-21. |
| Caption rule; no icon, Minimize or Maximize | Pass | "<Object> Properties", command names, the application name for the shell. Shared `w98-caption`. |
| Documented size; no page scrolls | Pass | 384 x 380 px at 1x (252 x 218 DLU) for sheets and palettes. Every page is checked for no scrolling at four densities in the Stage 09-21 suites. Only lists and grids scroll. |
| DLU margins, spacing, 50 x 14 buttons, 14 DLU fields | Pass | Template tokens (`@N@`). Checked by the Stage 04-05 geometry tests. |
| Commit buttons outside the pages, OK and Cancel adjacent, default outline | Pass | `w98-buttonbar` in every sheet, with `w98-button--default`. |
| Capitalization, colons, ellipses, unique access keys | Pass | Stage 21 suite. HUD drop-down menu items carry catalog access keys; with a menu open, its letter (with or without Alt) chooses the item and other letters are swallowed. The toolbar has none by design (ToolTips). |
| Only the five border styles; no rounded corners, soft shadows or gradients except the caption | Pass | Stage 01 design-system verifier; template. |
| Dotted focus rectangle; `#000080` / `#FFFFFF` selection | Pass | `win98-focus` decorator; template list, grid and menu states. |
| Unavailable controls engraved, not hidden | Pass | Disabled commands stay visible and engraved (for example Load/Save while rules are pending, and Properties with nothing selected). |
| Mixed values | Pass where they occur | Agriculture uses documented mixed states. The stockpile allow list shows leaf rules only, so there is no mixed row. |
| Tables are list views in details view, with right-aligned numbers and click-to-sort headings | Pass | Inventory, Stockpile, Workshop queue and trade, Population, Military, Load Game. Headings and cells share one 5 px inset; the Stage 06 and 08 tests hold them within 1 px. |
| ToolTip on every image-only control; flat toolbar buttons with hot-track only | Pass | Caption Close, spin arrows and combo arrow have keyed titles. Toolbar hot-track is in the template. |
| Message boxes: title, symbol, buttons, least-destructive default; no "error" or "failed"; one box per condition | Pass | Shared `win98_message_box.rml`. Messages scanned: those words appear only in keys, never in text. |
| Keyboard reaches everything; tab order; Esc and Enter | Pass (injected) | Stage 20 and 21 suites, plus live Qt key events. Physical input is not claimed. |
| High Contrast; color never the only cue; system font at whole-number scale | Pass (forced) | Stage 20. Windows' own switch was not toggled. |
| Section 4 items absent | Pass | Excel-style header filters, in-place detail navigation, section headings with help text, red inline errors, hover highlights and card layouts are removed from the migrated windows. |

## Evidence limits and remaining work

- **Text built at run time is still English literals.** Binding code (status lines, messages, generated rows and menus) is not yet in the catalog. The RML documents are fully keyed.
- **The 30 percent text expansion (PDF p.380) is not verified for layout.** The qps-long pseudo-locale has every key, but pages were not checked for fit with it.
- **Input and High Contrast:** keys were injected as Qt key events; physical input and the Windows High Contrast switch were not exercised.
- **Font licence:** the "MS W98 UI" font is an unlicensed conversion chosen by the owner. Flag it before any public release.

## Follow-up review of Stages 0-9 (2026-09-25)

The Codex-era stages were re-checked with fresh 1x captures of every live probe (Stages 10-21 windows, HUD, shell) and the full regression. Fixed:

- **HUD menu access keys:** 31 menu items carry "&" letters in the catalog; `AccessKeys` routes letters to the open menu.
- **List-view headings:** the 2 px offset is gone (shared cell inset).
- **Retired inventory probes:** the in-place detail, scroll, pointer and filter-combo automation was removed from `src/main.cpp` and `MainWindow`.
- **Drop-downs:** the value box and arrow now sit inside the sunken edge. RmlUi ignores margins on `selectvalue`/`selectarrow`, so the select has a 1 px padding ring.
- **Legacy ID rules:** 67 rules keyed to Win98 window IDs (8 px black frame on Agriculture, 12 px page padding on Workshop General, old scroll bars and selects) and 20 `.m6c-workbench` descendant rules were removed; they outranked the template.
- **Captions:** "<Object> Properties" uses book-title capitalization; default lower-case names get every word capitalized ("Market Stall"), player names keep their own case.
- **Diplomacy wizard:** it covers its owner window, so the owner's Close button no longer shows beside it.
- **Tile palette heading:** one line, "Tile at x, y, z".
- **Inventory:** controls sit on the dialog face; there is no tab-page frame without tabs.
- **Load Game:** Cancel sits under Open in the right-hand button column (Open dialog, PDF p.171).
- **Settings:** "Keyboard pan speed:" was clipped by the label column and is now "Pan speed:".

- **Primary window frame:** the game window drew the Windows 11 frame. It is now frameless (`Qt::FramelessWindowHint`, keeping the system menu and Minimize/Maximize styles for the taskbar) and `MainWindowFrame` draws the Windows 98 frame from `documents/window_frame.rml` above every other document (PDF p.311-314): a 4 px sizing border, an 18 px caption with the 16 px icon, the title and 16 x 14 Minimize, Maximize/Restore and Close buttons (Close 2 px apart), and a 1 px line above the client area. Dragging the caption or a border hands the move or resize to the system (`startSystemMove`/`startSystemResize`); double-clicking the caption maximizes or restores; a maximized window has no border; full screen has no frame. The HUD and shell documents are inset to the client area. The caption turns inactive while another application is active. Checked headless in the Stage 17 suite at four scales (normal and maximized) and live in the Stage 17 probe (maximize fills the work area, double-click restores, Minimize, Close ends the game). Dragging and sizing with a physical mouse were not exercised.

- **Caption buttons:** every Close now draws the same pixel glyph as the main window (a 14 x 12 mark in the 16 x 14 button; an 11 x 9 mark in a palette's 13 x 11 button) instead of the text "×". The Stage 21 suite checks every caption in every Windows 98 document: 18 px (15 px for palettes, PDF p.180), buttons 16 x 14 (13 x 11), 2 px from the caption's right end, each with its glyph.
- **Drop-down arrows:** RmlUi sizes a select's value box from the select's border box, so the value covered the arrow's left 4 px; its right margin is now 20 px (16 + border and padding on both sides). Drop-down, combo, spin and scroll arrows use the window border's outer top-left colour, button face (PDF p.317), so the arrow's edge shows against the white field.

- **Long lists:** removing the legacy ID rules dropped the workshop's height bound, and the Craft page's crafts list grew to the full catalog and pushed OK/Cancel out of the window. A list's flex basis is now 0 for every window: lists scroll, so their rows never size the page. The Stage 10 fit test now uses a 40-product catalog and checks that every control is inside the page and OK is inside the window; the Stage 10 live probe captures the Craft page.
- **Inventory:** the controls of a window without tabs share the window's side margin with the command buttons, so the list and Properties line up with Close (checked in the Stage 08 suite). Properties beside the list and Close as the only commit button is the report-window pattern (Device Manager: object commands under the list, the window's command at the bottom).

- **Scroll bars (PDF p.100-102, p.146):** a list view's vertical scroll bar now runs the whole height of the view, with the up arrow beside the column headings; the schedule grid's bars run its whole edges (up arrow beside the hour headings, left arrow under the names, as in Excel 97). This is done with a negative margin on the scroll bar, which RmlUi adds to its position and length, so the rows keep their own client area and keyboard scrolling is unchanged. `ScrollArrows.h` marks each list and grid after layout so an arrow is unavailable (engraved) when the view cannot scroll further that way; with all content visible both are, and the scroll box fills the shaft. The shaft is the dithered highlight/face pattern. Checked in the Stage 08 suite (up arrow beside the headings; top and end states on a 2,000-row list) and the Stage 13 suite (grid arrows).

- **Size grip (PDF p.98-100, p.155, p.159, p.181):** the game window is a sizable primary window with a status bar, so its size grip sits at the far corner of the status bar (never also at a scroll bar corner) and sizes the window's lower right corner through the system, like the border. It is hidden while the window is maximized or full screen. Secondary windows (property sheets, dialog boxes, wizards, message boxes) stay fixed-size, as the book advises unless sizing gives a benefit; the palettes stay fixed-size (the book allows either). Inventory and Load Game are the only windows where sizing would show more information; they remain fixed until the owner decides. Checked headless in Stage 17 (hidden unless sizable; 12 x 12 at the corner, after the last pane) and live in the Stage 17 probe (shown in a normal window, hidden when maximized). Dragging the grip with a physical mouse was not exercised.

### Window rules validation (Chapter 7 "Windows" and Chapter 9 "Secondary Windows")

Every normative statement in PDF p.88-186 was checked against the windows. Gaps found and fixed:

- **Window menu (p.95, p.113, p.181):** every title bar button needs its command on the window's shortcut menu. `WindowMenu.h` draws it as a Windows 98 menu. The game window's menu has Restore, Move, Size, Minimize, Maximize and Close (Alt+F4, bold default), with unavailable commands engraved. Secondary windows and palettes have Move and Close; they are fixed-size, so there is no Size. The menu opens from a right-click on the title bar, a click on the title-bar icon (game window) or Alt+Space. Letters, arrows, Enter and Esc work. Move and Size start the system's keyboard move or size (`NativeWindowCommands`). Double-clicking the icon closes the game window. Close in a secondary window acts as its title-bar Close, so pending changes still prompt.
- **Title text (p.93):** the game window names the open kingdom, then the game ("Tutorial Valley - Ingnomia"), and returns to "Ingnomia" at the main menu.
- **Restored position (p.97):** the saved game-window size and position are kept only while they fit a current screen's work area; otherwise the window is fitted and centered on the primary screen.
- **Open dialog (p.171):** Load Game's "Look in" keeps the kingdom the last game was opened from; Cancel does not change it.

Already met: secondary windows are owned by the game window (no taskbar entry, drawn above it, hidden when it is minimized), have no Minimize/Maximize or icon (p.156), stay within 263 DLU (p.157), and clamp their saved positions to the work area (p.159). Default buttons are never destructive (p.160), OK and Cancel have no access keys (p.162), and Close with pending changes asks first (p.166). Message boxes carry no "error"/"warning" title and no question-mark symbol (p.182-183).

Optional features, added afterwards:

- **What's This? (PDF p.95, p.111, p.157, p.285-288):**
  - **Where it starts:** property sheets and the Inventory windows have a ? button before Close in the title bar. The game window has a What's This? toolbar button (a primary window may not have a ? title bar button). Shift+F1 starts the mode in the game window.
  - **The mode:** the pointer becomes the Help pointer over that window only. The next click shows a pop-up window explaining the item and ends the mode. Esc, choosing What's This? again, clicking outside the window or clicking an item without Help cancels it.
  - **Other ways in:** the secondary button on any control offers a What's This? shortcut menu. F1 explains the focused control, and so does Shift+F1 in a secondary window. Any click or key closes the pop-up.
  - **The pop-up:** the Help font on the ToolTip colors, below the item and kept inside the window.
  - **Help text:** 497 entries in `UiTextWin98Help.inc` (mirrored in en.json and qps-long.json) under "win98.help.<id>". Two scoped forms exist: "win98.help.<document>.<id>" where an id means different things in two windows (the inspector's read-only summaries), and "win98.help.common.*" for OK, Cancel, Apply, Close and the ? button. A label resolves to its control, a spin arrow to its field and a generated row to its list. Group titles and static text have no Help (p.288). Each entry begins with a verb, answers "what is this" and "why would I use it", and ends with a period.
- **Always on Top (PDF p.113, p.158-159, p.181):** a palette's window menu has an Always on Top command with a check mark. While it is set, the palette is raised above its peers and the game window whenever a game window becomes active. It is not made system-topmost, so it never covers another application's windows. The setting lasts while the palette exists.

Checks:
- **Stage 21:** every control in every Windows 98 document resolves to Help, labels and spin arrows share their control's Help, and every entry follows the writing rules.
- **Stage 17 (headless):** the mode, the pop-up (text, position, colors), cancel on static text, choosing again, Esc, the shortcut menu and its W, F1 and Shift+F1, and any key closing the pop-up.
- **Live, game window (Stage 17 probe):** the toolbar button shows the Help pointer, a click on Pause shows its Help, and the next click closes the pop-up.
- **Live, Build palette (Stage 17 probe):** Always on Top sets and shows its check mark; the secondary button offers What's This? and W explains the control.
- **Live, Carpenter sheet (Stage 10 probe):** the ? button starts the mode with the Help pointer, clicking Priority explains it, and the next click closes the pop-up.

Checks: the Stage 17 suite tests the menu (access keys, unavailable commands, default, separator, keyboard highlight, Up wrap, Enter, letters, Esc, stays inside the window). The Stage 17 live probe opens the game-window menu with Alt+Space in normal and maximized states, maximizes with X and restores with R, and closes the Build palette with its menu's C. The title is checked live. The restored position and Load Game's kingdom memory are verified by code review only.

`tests/ui-common/SheetFit.h` checks that every visible control lies inside its page frame; Stage 12 uses it with a 24-skill catalog.

