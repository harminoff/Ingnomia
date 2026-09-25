# Stage 16 - Inspectors, citizen detail and equipment scope

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 17 is next.

## Design sources

See [07-win98-design-reference.md](../07-win98-design-reference.md), section 3.6. An inspector is a **property inspector** in a **palette window** (PDF p.167, p.180-181):

- It follows the selection.
- Edits apply at once, so it has no OK, Cancel or Apply buttons.
- Its title bar is short and has only a Close button.
- Tabs are allowed (PDF p.147). Pages never scroll; only the lists inside them do.

ToolTips use the ToolTip control colors (PDF p.148; Visual Design colors).

## What changed

Both inspectors in `content/rmlui/screens/inspector.rml` are Windows 98 palette windows. Each is 384 x 380 px at 1x, drawn at a whole-number scale, and opens in its own fixed-size native window.

### Creature inspector ("<Name> Properties")

It has five tabs. A creature only gets the tabs it reports: an animal has no Skills or Equipment tab. Ctrl+Tab and Ctrl+Shift+Tab switch pages.

- **General (INS-03):**
  - A picture well holds the follow camera.
  - Lines show Type, Activity and Position.
  - Center on Map sits beside the position. The old vertical view rail is gone.
- **Attributes (INS-04):**
  - Two group boxes, Attributes and Needs, with aligned "Label:" / value lines.
  - A value the game did not report reads "Unknown", never 0.
  - A note says that needs run from 0 to 100. The meters are gone: the book has no meter control for a quantity.
- **Skills (INS-05):**
  - "Profession:" is a drop-down list. Choosing a profession applies it at once and sends it once. Rebuilding the list never sends a change.
  - A Skill / Level / Active list view follows. Its headings sort the list and show the sort mark.
- **Equipment (INS-06):**
  - The scope is stated before any change: "This is the uniform of the <role> role. A change applies to every citizen with this role." A citizen without a role is told why nothing can change.
  - A Slot / Item list view replaces the paper doll, which had decorative, unlabeled positions.
  - Selecting a slot opens a "Change <slot>" group with Type and Material drop-down lists, Apply and Cancel. This is the one explicit commit on the page, because the change reaches every member of the role.
- **Inventory (INS-07):**
  - A "Carried item" list view.
  - "No carried items." is shown when the list is empty or the game did not report one.

### Object inspector (tile, blueprint, workshop, stockpile, agriculture)

Every page has one line naming the object and where it is, with Center on Map and Refresh along the bottom.

- **Tile (INS-01):**
  - One Type / Name list view shows everything on the tile: terrain, items, creatures, job, job needs and designation. A "Gnome: Winkle" entry becomes type "Gnome", name "Winkle".
  - The commands that apply to the tile are buttons below the list: Mine, Remove Floor, Replace Floor..., Harvest, Fell Tree, Remove Plant, Cancel Job, Raise/Lower Priority, Manage, Delete Stockpile. A command that does not apply is not shown. A command that applies but cannot run now is unavailable.
  - **Crowded tiles:**
    - Each creature is its own row, chosen by ID. Inspect, or a double-click, opens the chosen one.
    - The choice is kept by ID across refreshes. If that creature leaves, the choice falls back to the first creature.
    - This replaces "Inspect first creature".
- **Blueprint (INS-02):**
  - The status line comes first.
  - A "Missing materials" group lists Item / Material / Needed, showing missing materials only.
  - A "Construction job" group shows Worker, Priority (with Raise/Lower), Skill and Tool.
  - Cancel Blueprint is set apart below a separator, with a sentence saying what it does.
- **Workshop, stockpile and agriculture (INS-08):**
  - A group of "Label:" / value lines.
  - The stockpile adds a contents list view: Item with its 16 px icon, Material, Quantity.
  - Suspend/Resume and the harvest or pick command apply at once.

### One citizen detail (POP-05)

- Population no longer has its own citizen detail page.
- Properties, Enter or a double-click on a citizen opens the creature inspector for that citizen. The roster stays on screen.
- Per-citizen skill switches remain on Population > Skills.

### Other

- The selection size hint and the tile label near the pointer are ToolTips: pale yellow, black text, 1 px black border.

## Verification

| Gate | Result |
| --- | --- |
| Stage 16 focused suite | **9/9 ctest; 408 checks in `ui_Stage16`.** [Output](../evidence/stage-16/acceptance.txt).<br>Creature inspector:<br>- palette structure and the "<Name> Properties" caption; no button bar;<br>- General values; Unknown for unreported attributes and needs; Ctrl+Tab and Ctrl+Shift+Tab;<br>- skill columns, sorting and the single sort mark;<br>- profession drop-down: current value, no send on rebuild, a single send on choice;<br>- equipment scope with and without a role; unavailable slots without a role; a slot opens its editor; type and material are drafts until Apply; one uniform change carrying role, slot, type and material;<br>- inventory rows; animals get only the tabs they report.<br>Tile inspector:<br>- one row per creature, chosen by ID; Inspect and double-click open it; the choice is kept across a refresh and falls back when the creature leaves;<br>- only applicable commands are shown; priority commands follow the game; Fell Tree acts on the tile.<br>Other inspectors: blueprint groups and the set-apart Cancel Blueprint; workshop, stockpile and grove values.<br>Every page and control fits inside the window at 100/125/150/200%; pages do not scroll, and the busy list scrolls instead.<br>The inspector RML and integration contracts and the Population contract (no second detail) were rewritten for the palette design. |
| Regressions | Stages 04-15 pass (Stage 12 updated for POP-05). |
| Live copied-world runs | Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: **33/33 checks each**, 8 captures each. [Runs](../evidence/stage-16/runs.txt).<br>The probe moves a second citizen onto the first one's tile in memory.<br>It inspects that tile, chooses the second citizen's row and presses Inspect. The game opened that citizen's inspector.<br>It walks the five pages. It changes the profession through the drop-down; the game applied the change, and the probe restored the original.<br>It opens a different citizen from Population > Properties. The creature inspector opened, and Population has no detail page. |
| Save safety | All 21 source and copied save files unchanged. |
| Build | `Ingnomia.exe` SHA-256 `A00E7A74CFE6F278AB7022CA45DF1115C859344F7E123252BE6D5112A85491C9`. |

Captures at 1x: [tile](../evidence/stage-16/1/tile.png), [tile, creature chosen](../evidence/stage-16/1/tile-chosen.png), [General](../evidence/stage-16/1/general.png), [Attributes](../evidence/stage-16/1/attributes.png), [Skills](../evidence/stage-16/1/skills.png), [Equipment](../evidence/stage-16/1/equipment.png), [Inventory](../evidence/stage-16/1/inventory.png), [from Population](../evidence/stage-16/1/population-properties.png). The same set is under `1.25/`, `1.5/` and `2/`.

## Evidence limits

- Input was injected through production RmlUi and Qt adapters. Physical mouse and keyboard input is not claimed.
- The live runs did not cover these; the focused suite did:
  - blueprint, workshop, stockpile and agriculture inspectors;
  - uniform Apply, because the tutorial citizens have no military role.
- The palette's saved position and the Always on Top shortcut-menu entry (PDF p.181) are not implemented. Windows open at a fixed offset from the game window. This is recorded for Stage 20.
- Old CSS rules for the removed Population detail page remain in `management6b.rcss` (Stage 21 cleanup).

## Next

Stage 17: HUD.
