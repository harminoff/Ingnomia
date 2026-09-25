# Stage 15 - Diplomacy and mission planning

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 16 is next.

## Design sources

See [07-win98-design-reference.md](../07-win98-design-reference.md), section 3.5. Neighbour standing is a property sheet page. Mission planning is a simple wizard (PDF p.304-309). Facts the player does not know are shown as the word "Unknown", never as zero or a placeholder.

## What changed

The Diplomacy window (`content/rmlui/windows/diplomacy_missions.rml`) is a fixed Windows 98 property sheet.

### Sheet

- **Size:** 384 x 380 px at 1x, drawn at a whole-number scale.
- **Tabs:** Neighbors and Missions. The side rail, "Views" toggle, search, sort and "Filter / sort" controls are gone.
- **Commit button:** Close only. F5 refreshes, and Ctrl+Tab switches pages.
- **Selection:** each page keeps its own selected kingdom or mission when you switch pages.

### Pages

- **Neighbors (DIP-02):**
  - A "Kingdoms:" list box beside a group named after the selected kingdom.
  - The group lists Distance, Type, Attitude, Wealth, Economy and Military. A missing value reads "Unknown".
  - An undiscovered kingdom says why its details are unknown and why no mission can be sent.
  - "Send Mission..." is unavailable, with its reason stated, when the kingdom cannot receive a mission. The ellipsis is there because the button opens a dialog.
- **Missions (DIP-04):**
  - A Mission / Destination / Status list view.
  - A group for the selected mission: Task, Status, Citizens, Time, Result.
  - A running mission reads "Not reported yet" and "N hours so far". A returned mission reads "Succeeded" or "Did not succeed", with "Unknown" when the game did not report a duration.
  - A running mission cannot be mistaken for a finished one.

### Send Mission wizard (DIP-03)

- **Form:** a modal dialog over the sheet, titled "Send Mission to <kingdom>". It has three interior pages, each with a white header, a bold title and a one-sentence question.
  1. **Mission:** Emissary / Spy / Raid / Sabotage option buttons. Missions the kingdom does not allow are unavailable. For an Emissary, the "Emissary's task" option buttons are Improve relations, Insult, Invite a trader and Invite an ambassador. Every page starts from a legal default.
  2. **Citizen:** a list of the citizens who can go. The first eligible citizen is the default.
  3. **Review:** Kingdom, Mission (with task) and Citizen, with "Finish sends this mission once."
- **Buttons:**
  - < Back is unavailable on the first page. On Review, Finish takes the place of Next >. Cancel and Esc close the wizard without sending anything.
  - Enter is the default button: Next, or Finish on Review.
- **Choices are values:** choosing a type or task never starts a mission.
- **Finish revalidates:**
  - The kingdom and the mission must still be the reviewed ones and still legal. Otherwise the wizard returns to its first page with a note, and nothing is sent.
  - A second Finish cannot send again.
  - If the kingdom disappears, the wizard closes.
- **Size:** the book's wizard template (317 x 193 DLU) is larger than this 384 x 380 window, so the wizard fills the window with an 8 px inset (decision recorded in `06-decisions.md`).

## Verification

| Gate | Result |
| --- | --- |
| Stage 15 focused suite | **13/13 ctest; 184 checks.** [Output](../evidence/stage-15/acceptance.txt). Covers:<br>- sheet structure and caption;<br>- Unknown facts for an undiscovered kingdom, and Send Mission unavailable for it;<br>- wizard pages, Back/Next/Finish availability, unavailable mission types, legal defaults;<br>- a task choice starts nothing;<br>- default and chosen citizen; review text; Back keeps choices;<br>- a change after review is caught at Finish;<br>- one mission sent with the reviewed kingdom, task and citizen; a second Finish is blocked;<br>- Esc cancels; a removed kingdom closes the wizard;<br>- running versus returned mission text;<br>- sheet and wizard pages inside the window at four scales. |
| Regressions | Stage 04 4/4, 05 7/7, 06-14 passed after the shared changes. |
| Live copied-world runs | Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: **23/23 checks each**, 6 captures each. [Runs](../evidence/stage-15/runs.txt).<br>The tutorial world has discovered no kingdom, so the probe discovers the first one in memory; the save on disk is unchanged.<br>The wizard was walked through, and the game reported exactly one new mission for that kingdom. |
| Save safety | All 21 source and copied save files unchanged. |
| Build | `Ingnomia.exe` SHA-256 `93FF85A7DD0EECAAADA5CE5073BE7A91C07DE6ED46E7697CE94056F8436DCB7A`. |

Captures at 1x: [undiscovered](../evidence/stage-15/1/neighbor-undiscovered.png), [kingdom](../evidence/stage-15/1/neighbor.png), [wizard: mission](../evidence/stage-15/1/wizard-mission.png), [wizard: citizen](../evidence/stage-15/1/wizard-citizen.png), [wizard: review](../evidence/stage-15/1/wizard-review.png), [missions](../evidence/stage-15/1/missions.png). The same set is under `2/`.

## Evidence limits

- Input was injected through production RmlUi and Qt adapters. Physical mouse and keyboard input is not claimed.
- Spy, Raid and Sabotage were covered by the focused suite only, because the discovered tutorial kingdom allows only an Emissary.
- The mission's outcome was not waited for.

## Next

Stage 16: Inspectors, citizen detail and equipment scope.
