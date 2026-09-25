# Stage 11 - Farms, groves and pastures with explicit targets

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 12 is next.

## Design sources

The owner requires that only UI documented in `docs/MS-Windows-User-Experience-2001.pdf` and MSDN "Design Specifications and Guidelines - Visual Design" (ms997612) be used; gaps are researched. The per-stage citations are collected in [07-win98-design-reference.md](../07-win98-design-reference.md) (section 3.1). The workshop sheet from Stage 10 is the reference implementation.

## What changed

The agriculture manager (`content/rmlui/panels/agriculture_manager.rml`) is now a fixed Windows 98 property sheet like the workshop:

- **Caption and size:** "<name> Properties" (PDF p.163), 252 x 218 DLU = 384 x 380 px at 1x, drawn at a whole-number scale (768 x 760 at 1.5 and 2 density). `MainWindow::ensureDetachedManagement6A` sizes the agriculture window exactly like the workshop.
- **Tabs by designation type (one row, book-title caps):**
  - Farm: General, Plots, Plot Queue, Crops. Opens on Plots.
  - Grove: General, Trees. Opens on General.
  - Pasture: General, Animals, Food. Opens on General.
  - A hidden page is rejected by the controller (`agricultureSupportsPane`).
- **Pages never scroll;** only list boxes and the plot grid scroll inside their field border. Long lists take the space that is left (zero flex basis), so they cannot push the page past the sheet.
- **OK / Cancel / Apply** sit outside the pages (PDF p.165-166). Close with pending changes asks "Do you want to apply the changes you made to <name>?" with Yes / No / Cancel. Enter is OK and Esc is Cancel.

### Settings are pending until Apply or OK

The following settings stay pending until Apply or OK:

- the name;
- Suspend all work;
- the work rules (Harvest ripe crops; Pick fruit, Plant trees, Fell trees; Harvest animal products, Harvest hay, Tame wild animals);
- the default crop, tree type or animal type;
- the male and female limits;
- each animal's butchering mark;
- each food rule.

Apply sends one command per changed setting, each to its own stable target: the designation, the creature ID, or the food item and material. It then requests a snapshot (`agriculture.refresh`), because most agriculture setters do not publish one. The sheet waits for the authoritative snapshot. OK closes only once the snapshot confirms the values.

Live updates refresh every field the user has not changed. The user's own edits stay pending, and an edit whose value was changed elsewhere is reported as a conflict instead of being overwritten.

### Commands act immediately and name their scope

These commands act at once, like command buttons on a page:

- Assign, Use Default, Queue and Queue Repeat act on exactly the selected plots.
- Move Up, Move Down and Remove act on the selected planting of the one selected plot.

The group title shows the number of selected plots. With several plots selected, the status line states "Commands change each of the N selected plots; plantings are per plot." The count box is labelled "Per plot:".

### Plot grid (AGR-02)

The plot grid is a list view in small-icon view that keeps the field's layout (PDF p.136).

- **Mouse:** click selects one plot. Ctrl+click adds or removes a plot. Shift+click selects the rectangle from the anchor. Select All and Clear are buttons.
- **Keyboard:** arrows move the dotted focus rectangle. Shift+arrow extends the rectangle. Space selects the focused plot, and Ctrl+Space adds it. Ctrl+A selects all. (PDF p.53-60, Excel 97 range model.)
- **State without colour:** each plot is outlined. Tilled ground has a furrow glyph and a ready crop has a boxed check. Planted crops show their icon, and planned crops show a faded icon. A legend explains the glyphs, and every cell has a text tooltip and label (PDF p.314).
- **Selection:** selected plots use the navy highlight. The focus rectangle is separate from the selection.

### Honest omissions and discoveries

- **No Priority control.** `FarmingManager::setFarmPriority`, `setGrovePriority` and `setPasturePriority` are empty stubs, and the getters return -1. The unimplemented capability is not offered. Apply passes the authoritative value through unchanged. (NEW-003)
- **Pasture limits need an animal type.** The game keeps limits and food rules per animal type and resets them when the type changes. So:
  - A new pasture has no type, and a blank drop-down entry keeps it that way until the user chooses one.
  - While a type change is pending, the limit spin boxes are unavailable, with the reason stated beside them.
  - Once the new type is confirmed, the sheet adopts that type's limits, foods and roster.
- **Removed controls:** the old cycling controls (Next product, Next animal, Toggle first food rule, and the four cap +/- buttons) and the paging and search controls are gone. Every record is directly selectable by ID.

### Shared Windows 98 message box

- `content/rmlui/modals/win98_message_box.rml` is the documented message box (PDF p.182-187):
  - a caption with the object name;
  - the 32 x 32 Warning symbol (`w98m-warning` in `icons/w98-marks.tga`);
  - the message text;
  - centered 75 x 23 buttons, where the focused button carries the default outline.
- Esc and the caption Close act as Cancel.
- The workshop, stockpile and agriculture sheets use it through `ModalDialog::setDocumentPath`. Other screens keep `confirm_destructive.rml` until their own stages.

### Shared style fixes (`tools/win98-style/win98_classic.template.rcss`)

- The hidden state now wins over every display rule in the sheet. Before this fix, "Loading..." and "No data..." stayed visible.
- Horizontal scroll bars were added.
- Plot cells clear the 40 dp minimum size that older base styles give buttons.

## Verification

| Gate | Result |
| --- | --- |
| Stage 11 focused suite | **10/10 tests; 407 checks.** [Output](../evidence/stage-11/acceptance.txt). Covers:<br>- selection model: click, Ctrl, Shift, Ctrl+A, arrows, Space and Shift+arrow;<br>- exact plot targets for Assign, Use Default, Queue and Repeat, and invalid counts;<br>- planting targeting by ID across a reorder, and boundary disabling;<br>- pending name, work rule and default crop, one command per change, confirmation, Close prompt (No), Cancel, and OK after confirmation;<br>- drafts kept per designation, rejection message box;<br>- butchering and food rules on non-first records after reorder and removal;<br>- limits; the pending type locks and then adopts the type's values; rebasing untouched fields; conflict detection; Fell trees;<br>- pages and commit buttons inside the sheet with 40-row lists at 100/125/150/200%. |
| Regressions | Stage 04 4/4, 05 7/7, 06-10 10/10 each; management 6A and 6B contract suites pass. |
| Live copied-world runs | Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: **59/59 checks each**, 12 captures each (384 x 380 px at 1x, 768 x 760 px at 2x). [Runs](../evidence/stage-11/runs.json). |
| Model checks (live) | Checked in the game model:<br>- two of four plots planned with two Strawberry plantings each, surviving deserialization;<br>- rename and harvest-off applied to the Farm;<br>- default crop applied on OK;<br>- the grove's Fell trees changed alone;<br>- pasture type and male limit applied;<br>- Close prompt No discarded the pending name. |
| Save safety | All 21 source and copied save files unchanged after every run. |
| Build | `Ingnomia.exe` SHA-256 `040F2EDC3981E5ED95094D05DE4B5D52E43314C5C897875977B3B673ED24BB82`. |

Captures at 1x: [plots](../evidence/stage-11/1/farm-plots.png), [queue](../evidence/stage-11/1/farm-queue.png), [general pending](../evidence/stage-11/1/farm-general-pending.png), [crops](../evidence/stage-11/1/farm-crops.png), [grove](../evidence/stage-11/1/grove-general.png), [trees](../evidence/stage-11/1/grove-trees.png), [pasture type pending](../evidence/stage-11/1/pasture-type-pending.png), [animals](../evidence/stage-11/1/pasture-animals.png), [food](../evidence/stage-11/1/pasture-food.png), [close prompt](../evidence/stage-11/1/pasture-close-prompt.png). The same set is under `2/` for 2x.

## Defects found by the live runs and fixed

- Most agriculture setters publish no snapshot, so the grove sheet waited forever. Apply now ends with a refresh.
- Rebuilding a drop-down list selected its first entry and marked the sheet dirty. Drop-down lists now start with a blank entry until a value is chosen, and change events that only echo the current value are ignored.
- After a pasture type change the game supplies new limits and foods, so the sheet never settled and reported a false conflict. This is fixed as described under "Honest omissions".
- Apply silently did nothing while a limit spin box was unavailable, because `NumericEditor::commit()` refuses disabled boxes. Apply now skips unavailable editors.
- Farm plot rows collapsed at 1.25x, and untilled plots were white on white. Plots are now outlined, and rows size to their cells.

## Evidence limits

- Farms, groves and pastures are probe-created on the copied save. The pasture had no animals, so butchering marks were exercised only by the focused suite.
- No gnome performed planting during the short run (WARN, as in Stage 00).
- Input was injected through production RmlUi and Qt adapters. Physical mouse and keyboard input is not claimed.
- The game copies `content/` into the build at build time, so RCSS-only changes need a rebuild before a live run.
- An access violation occurs when `INGNOMIA_AUTOMATE_LOAD_PATH` points at a save folder that no longer exists. This was seen once when a disposable copy was deleted underneath the run, and is recorded as NEW-004 for Stage 19 (load errors).

## Next

Stage 12: Population roster, skills and profession editor.
