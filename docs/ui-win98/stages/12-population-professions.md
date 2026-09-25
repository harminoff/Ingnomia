# Stage 12 - Population roster, skills and profession editor

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 13 is next.

## Design sources

Only UI documented in the Windows User Experience book and MSDN Visual Design is used ([07-win98-design-reference.md](../07-win98-design-reference.md), section 3.2). The workshop sheet (Stage 10) and the agriculture sheet (Stage 11) are the reference implementations.

## What changed

The Population manager (`content/rmlui/windows/population_manager.rml`) is now a fixed Windows 98 property sheet:

- **Size:** 384 x 380 px at 1x, drawn at a whole-number scale.
- **Tabs:** Citizens, Skills, Professions and Schedules in one row. The side rail, "Views" toggle, duplicate Refresh and paging buttons are gone.
- **Close is the only commit button.** Every change on these pages takes effect at once (skills, schedules), except the profession editor, which keeps its own Save Changes and Discard. Adding OK/Cancel/Apply would suggest a pending model that does not exist.
- **Close with unsaved profession changes** asks "Do you want to save the changes you made to <name>?" with Yes / No / Cancel (PDF p.166).
- **Keyboard:** F5 refreshes, and Ctrl+Tab / Ctrl+Shift+Tab switch pages (PDF p.147). Tabs have text labels and no ToolTips. The caption Close glyph has one.
- **Message boxes** now use the shared Windows 98 message box. The title is the object name ("Population" or the profession's name). The text states the effect, and no box asks "Are you sure" (PDF p.182-187).

### Citizens (POP-01)

- **List view in details view (Name, Profession):**
  - The headings are buttons. Clicking one sorts by that column, and clicking the sorted heading again reverses the order.
  - A small arrow glyph marks the sort. Down means descending (PDF p.143).
  - The whole filtered roster scrolls in its field border, so there is no paging.
- **Find:** a text box above the list filters it. The status line reads "N citizens" or "N of M citizens".
- **Selection follows the list view model:**
  - A click selects a row. Double-click, Enter or Properties opens the citizen.
  - The selection is kept by ID across sorting and refresh.
  - A filter that hides the selection moves it to the first match.
  - A removed citizen leaves no selection, instead of an arbitrary one.
- **One Refresh** command on the Citizens page, plus F5 from any page.

### Skills (POP-02)

- The skill list box is on the left. On the right, "Citizens who may use <skill>:" is a list view with a check box and a right-aligned Level column.
- Checking a box changes only that citizen's skill.
- Enable for All and Disable for All state their scope beside the buttons ("change Mining for all 5 citizens"). Each asks through a message box that names the count, including citizens hidden by the Find text.

### Professions (POP-03)

- **Choosing a profession:**
  - The profession is chosen from a drop-down list. New creates "New Profession", numbered if the name is taken, like a new folder.
  - Delete is on the profession line, away from Save. It asks through a message box titled with the profession's name.
  - Gnomad is protected.
- **Editing skills:** the Customize Toolbar two-list pattern, since the book has no transfer control.
  - The lists are "Available skills:" and "Profession skills:" (in priority order).
  - Add -> and <- Remove sit between the lists, with Move Up and Move Down below them.
  - Every button is unavailable when it cannot act (PDF p.323).
- **Saving:** Save Changes and Discard sit at the bottom right, and unsaved changes are stated beside them. Switching profession with unsaved changes asks first, and Cancel keeps both the profession and the draft.
- **No profession selected:** the editor shows no stale draft.

### Schedules (interim until Stage 13)

- Activity is a group of option buttons: None, Eat, Sleep, Train. Choosing one never changes a cell.
- Cells show letter codes (E, S, T), so colour is never the only cue.
- Set Cell, Set Citizen's Day and Set Hour for All remain explicit commands.

### Citizen detail (interim until Stage 16)

The existing detail is a page reached from Properties, with "< Citizens" to return. Stage 16 consolidates it with the inspector.

## Verification

| Gate | Result |
| --- | --- |
| Stage 12 focused suite | **10/10 tests; 189 checks.** [Output](../evidence/stage-12/acceptance.txt). Covers:<br>- sheet structure: no rail or paging, Close only;<br>- sort and reverse, with selection kept by ID across sort, filter and refresh; a removed citizen is not replaced;<br>- click versus Properties;<br>- per-citizen skill check boxes; bulk review, Cancel and confirmed scope;<br>- profession drop-down, transfer buttons and their availability, reorder, unsaved-change prompt, Save payload, authoritative confirmation, Delete review, numbered New, Gnomad protection;<br>- schedule option buttons and cell targeting;<br>- pages, tabs and Close inside the sheet at 100/125/150/200%. |
| Regressions | Stage 04 4/4, 05 7/7, 06-11 10/10 each. The Stage 07 hover-recursion test now hovers the caption Close. The management 6B contract was updated for the sheet. |
| Live copied-world runs | Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: **32/32 checks each**, 8 captures each. [Runs](../evidence/stage-12/runs.txt). Covers:<br>- HUD open;<br>- sort reversal;<br>- one citizen's Mining skill toggled and restored, confirmed by the game snapshot while the others stayed unchanged;<br>- bulk review cancelled;<br>- New Profession created, one skill added and saved (confirmed by the game), then deleted;<br>- Close. |
| Save safety | All 21 source and copied save files unchanged after every run. |
| Build | `Ingnomia.exe` SHA-256 `504AD3C04543214A316E7EBDD090FD21D7144D8E730505EE713850A97FBB9DF1`. |

Captures at 1x: [citizens](../evidence/stage-12/1/citizens.png), [descending](../evidence/stage-12/1/citizens-descending.png), [skills](../evidence/stage-12/1/skills.png), [bulk review](../evidence/stage-12/1/skills-bulk-review.png), [professions (unsaved)](../evidence/stage-12/1/professions-dirty.png), [professions](../evidence/stage-12/1/professions.png), [delete review](../evidence/stage-12/1/profession-delete-review.png), [schedules](../evidence/stage-12/1/schedules.png). The same set is under `2/` for 2x.

## Defects found and fixed

- An ID rule (`#creature_detail { display: flex }`) in `management6b.rcss` made the citizen detail show under every page.
- The MS W98 UI font has no arrow characters, so a sort indicator glyph was added to `icons/w98-marks.tga`.
- After a profession was deleted, its stale draft name and skills were still shown.
- A second writer overwrote the roster status line with the old "Rows 1-5 of 5" paging text.

## Evidence limits

- Input was injected through production RmlUi and Qt adapters. Physical mouse and keyboard input is not claimed.
- The live run used the tutorial colony's five citizens, so a large roster was covered only by the scrolling design and the focused suite.
- The schedule grid and citizen detail are interim; Stages 13 and 16 own them.

## Next

Stage 13: the 24-hour schedule grid and its scope preview.
