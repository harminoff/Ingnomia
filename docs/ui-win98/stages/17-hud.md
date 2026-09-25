# Stage 17 - HUD

Date: 2026-09-25
Status: VERIFIED for automated and injected-input scope. Stage 18 is next.

## Design sources

See [07-win98-design-reference.md](../07-win98-design-reference.md), section 3.7:

- **Toolbar:** PDF p.151-155, p.319, p.322, p.325 and p.345.
- **Menus:** PDF p.111-126 and p.326.
- **Status bar:** PDF p.155, p.157 and p.288-289.
- **Armed tools and the pointer:** PDF p.48, p.154 and p.355-356.
- **Message boxes:** PDF p.182-187.
- **Palette windows:** PDF p.180-181.

## What changed

The game window (`content/rmlui/screens/game_hud.rml`) now has the chrome of a Windows 98 primary window.

### Toolbar

One toolbar runs along the top. Its groups are separated by etched separators:

- Pause, Normal Speed, Fast Speed
- Level Down, Level Up
- Inventory, Population, Military, Missions
- Inspect, Build, Deconstruct, Mine ▾, Agriculture ▾, Designations ▾, Jobs ▾
- Cancel Tool, Rotate
- View ▾

The buttons follow the book's flat toolbar rules:

- **Appearance:** buttons are flat and have text labels. A raised border appears only under the pointer, and the button looks pressed while it is clicked.
- **State on:** a state that is on (Pause, the current speed) or an armed mode (Inspect, Build, Deconstruct, or the menu whose tool is armed) uses the option-set look: a pressed border and a dithered background. Pause keeps its label; the pressed state shows that the game is paused.
- **Menu buttons:** a button that opens a menu shows the triangular arrow and stays pressed while its menu is open.
- **Unavailable buttons:** they are grayed. Level Up is unavailable at the top level, and Cancel Tool and Rotate are unavailable while no tool is armed.
- The slide-out sidebar, its pages and their Back buttons are gone.

### Drop-down menus

- Mine, Agriculture, Designations and Jobs list their orders as menu items, grouped with separators. The armed order has an option dot.
- View holds the map overlays (Designations, Jobs, Lowered Walls, Axles). Each is an independent setting with a check mark.
- Choosing an item closes the menu. Esc, or the button again, closes it without a choice.
- **Fixed:** changing a View setting did not reach the HUD, because the game never echoed it. `EventConnector::onSetRenderOptions` now echoes the settings, so the check marks show what the renderer uses.

### Status bar

The status bar runs along the bottom and is divided into panes with the status-field border.

- **Message pane:** it describes the control under the pointer with a present-tense sentence, for example "Opens the citizens, their skills, professions and schedules." An unavailable control also says why.
- **Other panes:** the armed mode's name, the kingdom, "Level N", "Day D, Year Y", the time, Day or Night, the counts of gnomes, animals and items, and any watched items.
- Nothing essential lives only in the status bar.
- **Fixed:** the Level pane read "Level 0" after loading. The HUD is now given the saved view level when a world starts. The top level is dimZ - 1.

### Armed tools (HUD-03)

- Choosing an order arms its tool. The toolbar shows the mode, and the status bar names it: "Click or drag on the map to use the tool. Right-click or Esc cancels."
- The pointer becomes a cross, but only over the map. Toolbar, menus and windows keep the arrow. `QtRmlSystemInterface::setMapCursor` shows the map pointer wherever no RmlUi element is under the pointer in the game window.
- Cancel Tool, Esc or right-click leaves the mode and restores the arrow.

### Build window (HUD-04)

The Build window (`orders_tools.rml`) is a palette window, 384 x 380 px at 1x and not resizable.

- **Categories:** a list box on the left.
- **Items:** a Material drop-down (the category's types) above an Item / Status list view. A missing item reads "Needs items".
- **Chosen item:** a group names the item, says what is missing, and gives a drop-down for each component. Its commands are Fill Hole and Replace for terrain, and Build (or Place Blueprint when materials are missing).
- **Arming:** choosing a category or an item arms nothing. Build arms placement, and the game window shows the mode.
- **Fixed:** a crash. Clicking an item rebuilt the list that held the clicked row while the click was still being dispatched. Lists are now rebuilt only when their content changes, and a selection only restyles the rows in place.

### Tutorial (HUD-06)

- The tutorial is a palette window beside the map. It shows the lesson number, the explanation, the objective in bold, and "Do this" and "Lessons" group boxes.
- Steps and lessons are read-only check boxes. Skipped lessons and the current lesson are marked in words.
- The commands are Continue, Finish, Skip, Restart, and Show Hints or Hide Hints.
- The tutorial steps and explanations now name the new controls: the toolbar buttons, the Tile Properties window, the Skills tab, the Stockpiles tab, OK, and the Plots and Plot Queue tabs.

### Events (HUD-07)

- An event that needs an answer shows a message box: the event title as its caption, the Information symbol (a new glyph), and OK, or Yes and No.
- One message box shows at a time, and the next one follows after an answer. The first button has the focus and the default outline.

### Other (HUD-05)

- The labels near the pointer are ToolTips, done in Stage 16.

## Verification

| Gate | Result |
| --- | --- |
| Stage 17 focused suite | **7/7 ctest; 222 checks in `ui_Stage17`.** [Output](../evidence/stage-17/acceptance.txt). Checks:<br>- toolbar and status bar placement; the toolbar is one row at 1x; labels are laid out;<br>- the status panes; unavailable buttons; hover messages and why a control is unavailable;<br>- option-set speed and pause, and the Pause label does not change;<br>- menus: position, one open at a time, Esc, arming a tool and closing, the dot for the armed tool, check marks for View settings;<br>- Cancel Tool, and the pointer callback;<br>- Build: category request, list, selection without arming, blueprint text, Material filtering, component drop-down, one Build arming, fit inside 384 x 380 at four densities;<br>- tutorial check boxes and commands;<br>- two queued events shown one at a time, with OK focused;<br>- the toolbar and status bar inside the window at four densities.<br>The HUD RML contract was rewritten for the new design. |
| Regressions | Stages 04-16 pass. |
| Live copied-world runs | Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: **46/46 checks each**, 7 captures each. [Runs](../evidence/stage-17/runs.txt).<br>The run covers: status messages; the Mine menu; arming Mine Walls (cross pointer, then Cancel Tool restores the arrow); the View menu and the Jobs setting, confirmed by the game and turned off again; Fast Speed; the Build window with Furniture; arming Build from the window; and an information message box answered with OK. |
| Save safety | All 21 source and copied save files unchanged. |
| Build | `Ingnomia.exe` SHA-256 `B5DDBA419F5F8ED0D0C7A5C2C9FB31E23904739BBB3DDFA6CB7456E4CE7607F3`. |

Captures at 1x: [HUD](../evidence/stage-17/1/hud.png), [Mine menu](../evidence/stage-17/1/mine-menu.png), [armed](../evidence/stage-17/1/armed.png), [View menu](../evidence/stage-17/1/view-menu.png), [Build window](../evidence/stage-17/1/build.png), [Build armed](../evidence/stage-17/1/build-armed.png), [message box](../evidence/stage-17/1/message-box.png). The same set is under `1.25/`, `1.5/` and `2/`.

## Evidence limits

- Input was injected through production RmlUi and Qt adapters. Physical mouse and keyboard input is not claimed.
- The message box in the live run was queued by the probe. Game-raised events use the same path, but none was waited for.
- **Not done:**
  - Toolbar buttons have text labels and no images. The book allows text labels, and standard images exist only for common commands (PDF p.353-355).
  - There is no toolbar grip, docking, or shortcut menu.
  - Balloon tips are not used, as recorded in section 3.7.
- The old "hud.jobs.overlay_note" string is now unused (Stage 21 cleanup).

## Next

Stage 18: main menu, new game and save browser.
