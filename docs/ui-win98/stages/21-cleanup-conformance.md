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
| Capitalization, colons, ellipses, unique access keys | Pass, with one gap | Stage 21 suite (1971 checks). **Gap:** HUD drop-down menu items (generated by the HUD binding) show no underlined letters. The toolbar itself has none by design (ToolTips). |
| Only the five border styles; no rounded corners, soft shadows or gradients except the caption | Pass | Stage 01 design-system verifier; template. |
| Dotted focus rectangle; `#000080` / `#FFFFFF` selection | Pass | `win98-focus` decorator; template list, grid and menu states. |
| Unavailable controls engraved, not hidden | Pass | Disabled commands stay visible and engraved (for example Load/Save while rules are pending, and Properties with nothing selected). |
| Mixed values | Pass where they occur | Agriculture uses documented mixed states. The stockpile allow list shows leaf rules only, so there is no mixed row. |
| Tables are list views in details view, with right-aligned numbers and click-to-sort headings | Pass, with one gap | Inventory, Stockpile, Workshop queue and trade, Population, Military, Load Game. **Gap:** a heading sits 2 px to the right of its column in the shared list view (measured by the Stage 06 test). |
| ToolTip on every image-only control; flat toolbar buttons with hot-track only | Pass | Caption Close, spin arrows and combo arrow have keyed titles. Toolbar hot-track is in the template. |
| Message boxes: title, symbol, buttons, least-destructive default; no "error" or "failed"; one box per condition | Pass | Shared `win98_message_box.rml`. Messages scanned: those words appear only in keys, never in text. |
| Keyboard reaches everything; tab order; Esc and Enter | Pass (injected) | Stage 20 and 21 suites, plus live Qt key events. Physical input is not claimed. |
| High Contrast; color never the only cue; system font at whole-number scale | Pass (forced) | Stage 20. Windows' own switch was not toggled. |
| Section 4 items absent | Pass | Excel-style header filters, in-place detail navigation, section headings with help text, red inline errors, hover highlights and card layouts are removed from the migrated windows. |

## Evidence limits and remaining work

- **Text built at run time is still English literals.** Binding code (status lines, messages, generated rows and menus) is not yet in the catalog. The RML documents are fully keyed.
- **The 30 percent text expansion (PDF p.380) is not verified for layout.** The qps-long pseudo-locale has every key, but pages were not checked for fit with it.
- **Layout gaps:** the list view heading sits 2 px off its column, and HUD menu items have no access keys.
- **Old opt-in probes:** automation in `src/main.cpp` for the Stage 08 in-place inventory detail (back and scroll probes) and the retired filter combos reports false if it is run. None of it runs by default.
- **Input and High Contrast:** keys were injected as Qt key events; physical input and the Windows High Contrast switch were not exercised.
- **Font licence:** the "MS W98 UI" font is an unlicensed conversion chosen by the owner. Flag it before any public release.
