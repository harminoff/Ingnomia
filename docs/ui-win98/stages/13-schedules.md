# Stage 13 - 24-hour schedule grid and scope preview

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 14 is next.

## Design sources

The book has no grid control, so the schedule follows Excel 97 within the book's selection rules (PDF p.53-60, spreadsheet Figure 6.2). See [07-win98-design-reference.md](../07-win98-design-reference.md), section 3.3.

## What changed

The Schedules page of the Population sheet is now a grid.

### Layout and appearance

- **Frozen headings:**
  - Button-face hour headings (0-23) stay above the cells.
  - Citizen headings stay to their left.
  - A Select All button sits in the corner.
  - The headings follow the cell area's scroll position, so row and hour context never scrolls away.
- **Cells:**
  - White cells with gray grid lines.
  - Each cell shows a letter code (E eat, S sleep, T train). Its label names the citizen, the hour and the activity.
- **Selection:**
  - The selected range is navy.
  - The active cell keeps a white face with a heavy black border.
  - Headings of the selected rows and hours are bold.
- **Activity:** a group of option buttons with the letter codes in their labels. Choosing an activity changes nothing by itself.

### Selecting with the mouse

- Click selects a cell, and Shift+click extends the range from the anchor.
- A citizen heading selects that citizen's day.
- An hour heading selects that hour for everyone.
- The corner selects everything.

### Selecting with the keyboard

- Arrows move the active cell, and Shift+arrows extend the range.
- Home and End go to the ends of the row. Ctrl+Home and Ctrl+End go to the first and last cells.
- Page Up and Page Down move eight rows.
- Shift+Space selects the citizen's day, Ctrl+Space selects the hour, and Ctrl+A selects everything.
- Enter or Space runs Set Activity.
- The active cell scrolls into view, so all 24 hours are reachable at any scale.

### Scope and applying a change

- **Scope preview:** a status line states exactly what Set Activity will do, for example:
  - "Set Activity sets hour 5 for Winkle to Sleep."
  - "... all 24 hours for Cass to Eat."
  - "... hour 9 for all 5 citizens to Train."
  - "... hours 10-12 for 2 citizens (A to B) to Eat."
- **Set Activity** applies to exactly that scope and uses the existing commands:
  - one cell command for a single cell;
  - one row command for a full day;
  - one column command for an hour for everyone;
  - one cell command per cell for any other range.
- The three original scopes (cell, citizen's day, hour for all) are kept, now as selections of the one grid.
- **Proportionate review:** when more than one citizen is affected, a message box repeats the scope and counts the cells, "including rows scrolled out of view", before anything changes.
- **Revalidation:**
  - The reviewed scope must still match when Set is pressed. If citizens changed meanwhile, nothing is sent and the status asks for a new review.
  - When a range-corner citizen is removed, the range collapses to the active cell. It never moves to another citizen.

## Verification

| Gate | Result |
| --- | --- |
| Stage 13 focused suite | **10/10 tests; 115 checks.** [Output](../evidence/stage-13/acceptance.txt). Covers:<br>- grid structure and letter codes;<br>- choosing an activity changes nothing;<br>- cell, day, hour-for-all and 2 x 3 range: the preview text and the exact commands, with cell IDs compared;<br>- review Cancel and Accept;<br>- keyboard traversal to hour 23 with the headings aligned, Home/End, Ctrl+Home/End, Shift+arrows, Shift+Space, Ctrl+Space, Ctrl+A and Enter;<br>- held Enter under an open review;<br>- stale review sends nothing;<br>- removed range corner;<br>- the page and Close stay inside the sheet at four scales. |
| Regressions | Stage 04 4/4, 05 7/7, 06-12 10/10 each. Stage 06 "held Space activates once" still passes; Space now runs Set Activity. |
| Live copied-world runs | Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: **24/24 checks each**, 5 captures each. [Runs](../evidence/stage-13/runs.txt). Checked against the game's own schedules:<br>- one cell set to Sleep and only that cell changed;<br>- a citizen's day set to Eat, all 24 hours;<br>- hour 9 set to Train for all citizens after the review;<br>- End reached hour 23. |
| Save safety | All 21 source and copied save files unchanged. |
| Build | `Ingnomia.exe` SHA-256 `B51875B07AE60D62DB978B93C1EE3F94CB8E2E3C108769EE5A6E8249C04C1B99`. |

Captures at 1x: [grid](../evidence/stage-13/1/grid.png), [cell scope](../evidence/stage-13/1/cell-scope.png), [day scope](../evidence/stage-13/1/day-scope.png), [hour review](../evidence/stage-13/1/hour-review.png), [End](../evidence/stage-13/1/keyboard-end.png). The same set is under `2/`.

## Deviations recorded

- Excel 97 drew selected ranges in inverse video. The grid uses the system navy highlight instead, for consistency with every other selection (decision recorded in `07-win98-design-reference.md`, section 5).
- Ctrl+click for several separate ranges (optional in the book) is not implemented.

## Evidence limits

- Input was injected through production RmlUi and Qt adapters. Physical mouse and keyboard input is not claimed.
- The tutorial colony has five citizens. Large rosters are covered by the focused suite's 12-citizen grid and by the scrolling design.

## Next

Stage 14: Military.
