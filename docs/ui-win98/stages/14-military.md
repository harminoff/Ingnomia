# Stage 14 - Military: squad, citizen, role and destination

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 15 is next.

## Design sources

Only UI documented in the Windows User Experience book and MSDN Visual Design is used ([07-win98-design-reference.md](../07-win98-design-reference.md), section 3.4). Transfers use the Customize Toolbar two-list pattern, because the book has no transfer control.

## What changed

The Military manager (`content/rmlui/windows/military_manager.rml`) is a fixed Windows 98 property sheet.

### Sheet

- **Size:** 384 x 380 px at 1x, drawn at a whole-number scale and not resizable.
- **Tabs:** Squads, Members, Roles, Uniforms and Targets in one row. The side rail, "Views" toggle, search, sort and "Filter / sort" controls are gone.
- **Page mapping:** the controller keeps its three views. The binding maps Squads and Members onto the Squads view, and Roles and Uniforms onto the Roles view.
- **Commit button:** every military command acts at once, so Close is the only commit button.
- **Keyboard:** F5 refreshes, and Ctrl+Tab / Ctrl+Shift+Tab switch pages.

### Pages

- **Squads (MIL-02):**
  - A numbered squad list, with New, Move Up, Move Down and a separated Delete beside it.
  - Move Up and Move Down are unavailable at the ends. The page states that they "change only the order of squads in this list".
  - Name: and Rename act only on the selected squad.
- **Members (MIL-03):**
  - A "Squad:" drop-down list chooses the squad.
  - "Not in a squad:" and "Members of <squad>:" lists sit side by side, with Add -> and <- Remove between them.
  - Selecting a citizen never transfers anyone. The "<citizen> (<squad>)" group then offers:
    - a Role drop-down list;
    - a "Move to:" drop-down list naming every other squad, with a Move command.
  - Move uses the existing `military.assign_squad` command, which moves the citizen from any squad. The old previous/next-squad buttons are replaced by this explicit destination.
- **Roles (MIL-04):**
  - A role list with New and a separated Delete.
  - Name: and Rename.
  - Civilian is a persistent check box with its help text: "Civilian (retreats to safety during an alarm)".
  - "N citizens have this role."
- **Uniforms (MIL-05):**
  - "Role:" drop-down list and a Slot / Equipment / Material list view.
  - One "<slot>" group with Equipment and Material drop-down lists, which offer only the game's legal choices.
  - The scope is stated: "Changes apply at once to every citizen with the <role> role (N citizens)."
- **Targets (MIL-06):**
  - "Squad:" drop-down list and a numbered Target / Response list view.
  - Move Up and Move Down sit beside the list.
  - A separate "Response to <target>" group holds the Flee / Defend / Attack / Hunt option buttons. The group says it does not start an attack.
  - Reordering keeps the response.
- **Deletion (MIL-07):** Windows 98 message boxes titled with the squad or role name, for example "Deleting Stage 14 A removes the squad and its target list. You cannot undo this." with Delete / Cancel. This replaces the in-page removal layer.

### Shared fixes

- **Drop-down lists:** setting a select's inner RML appends options to its list box instead of replacing them. `runtime/SelectOptions.h` rebuilds options through the select API, and is now used by the agriculture, profession and military drop-down lists.
- **Tabs:** a tab without `aria-controls` made `connected_tabs::select` call `GetElementById("")`, which returned the first element without an id. That was the Military caption, which was therefore hidden. The helper now skips such tabs.

## Verification

| Gate | Result |
| --- | --- |
| Stage 14 focused suite | **13/13 ctest; 282 checks.** [Output](../evidence/stage-14/acceptance.txt). Covers:<br>- sheet structure and the caption above the tabs;<br>- squad order boundaries, Move Up target, Rename target, Delete review and Cancel;<br>- selection never transfers;<br>- Add, Move to (lists only other squads), Role and Remove with exact IDs;<br>- Civilian check box (one click sends one change) and role usage;<br>- uniform legal options and scope;<br>- target selection, Move Up without a response change, response option, end boundary;<br>- pages and Close inside the sheet at four scales.<br>The management 6C controller and contract tests were updated for the sheet. |
| Regressions | Stage 04 4/4, 05 7/7, 06-13 10/10 each. |
| Live copied-world runs | Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: **45/45 checks each**, 8 captures each. [Runs](../evidence/stage-14/runs.txt). Checked against the game's roster:<br>- two squads created, one renamed;<br>- a citizen added, given a role and moved to the other squad;<br>- a role made civilian;<br>- a target's response set to Hunt and then moved up keeping Hunt;<br>- the renamed squad deleted through the message box, leaving the others;<br>- Close. |
| Save safety | All 21 source and copied save files unchanged. |
| Build | `Ingnomia.exe` SHA-256 `2B95BB7C5EBA767C536A03E4634D708183ACB24C8D132338A7981F9B201AAB6D`. |

Captures at 1x: [squads](../evidence/stage-14/1/squads.png), [renamed](../evidence/stage-14/1/squads-renamed.png), [members](../evidence/stage-14/1/members.png), [roles](../evidence/stage-14/1/roles.png), [uniforms](../evidence/stage-14/1/uniforms.png), [targets](../evidence/stage-14/1/targets.png), [delete review](../evidence/stage-14/1/squad-delete-review.png). The same set is under `2/`.

## Evidence limits

- Input was injected through production RmlUi and Qt adapters. Physical mouse and keyboard input is not claimed.
- The message box does not say what happens to a deleted squad's members, because the game's behaviour was not verified. `MilitaryManager::removeSquad` removes the squad record only.

## Next

Stage 15: Diplomacy and missions.
