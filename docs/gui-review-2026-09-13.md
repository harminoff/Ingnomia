# GUI review — 13 September 2026

Review only. No game code, RML, styles, localization, or build configuration was edited. Findings below describe the current working tree, including its existing uncommitted changes.

## Scope and evidence

Applied the game-ui-ux skill's checks for layout, scaling, readable text, navigation, sensible window sizing, and use of screen space.

- Reviewed the active nested checkout at `C:/Programming/Repos/Ingnomia2/Ingnomia`, HEAD `fd1df94912ed36da2d4c8d5a44596e553057c49f`.
- Used the existing `build-wave8-root-msvc-link-priority2/Ingnomia.exe`, version 0.9.0.0, SHA-256 `577a44dff998ab630a261741938b4c8ab7d4bcc2f61b1e9a71f4130e2ab3b514`. Its packaged RmlUi assets matched the current source assets byte for byte. No rebuild was performed; this does not establish that every C++ source file matches the executable.
- Captured the real OpenGL framebuffers at **1600×900 / 100% UI scale**, selected screens at **1280×720 / 100%**, and **1280×720 / 150%**. These are application UI-scale checks, not a separate Windows display-DPI test.
- Used an isolated profile and a copied local test save. Original player saves/settings were not edited. Data-heavy detached windows were recaptured after 90 rendered frames, so their initial loading frames are not used as evidence of permanently missing content.
- Visually inspected main menu, load screen, all four setup steps, Settings, HUD, Build, Mining, Agriculture, Designations, Job commands, Inventory, Population roster, Military squads, Missions empty state, an event prompt, and the creature fixture. Source inspection supplemented the captures.

Evidence is under [the review capture folder](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913). The black margins in detached framebuffer images are transparent surface margins; they are **not** evidence that players see black rectangles.

**Priority:** P1 = controls/content become unusable; P2 = significant usability/layout problem; P3 = polish or unnecessary space. There are **19 findings**, followed by one additional source/log risk requiring a targeted visual check.

## Findings

### GUI-01 — P1 — Main menu and Settings collapse at 150% scale

**Observed:** At 1280×720 / 150%, the panel collapses into a narrow strip against the left edge. Text wraps into a vertical column or paints outside the panel. Settings controls overlap, sliders lose their usable width, and lower actions fall out of view. The main-menu result remained broken in a later, frame-60 capture.

**Evidence:** [Main menu](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-150/main_menu.png), [Settings](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1280x720-150/settings.png). Relevant layout: [shell.rcss](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/shell.rcss:23), especially the centered screen and chamber sizing rules.

**Resolve:** Give the shell panel an explicit usable width bounded by the scaled viewport, and prevent flex layout from collapsing the panel when it is taller than the screen. Top-align tall panels and provide vertical scrolling. Keep Back and the Settings action row reachable; preserve a reliable way to restore a usable scale.

**Check:** Main menu, Settings, and their exit/back/reset actions remain readable and reachable at 1280×720 with 100%, 125%, 150%, and 200% scale.

### GUI-02 — P1 — Setup loses lower fields and Start kingdom

**Observed:** At 1280×720 / 150%, Settlement stops partway through Starting settlement. Terrain & life loses its lower cards. World foundation also extends below the viewport, and Start kingdom is off-screen. The setup stylesheet explicitly hides overflow and both scrollbar types.

**Evidence:** [Settlement](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1280x720-150/setup_settlement.png), [Terrain](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1280x720-150/setup_terrain.png), [shell.rcss](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/shell.rcss:165).

**Resolve:** Constrain the setup body to the available height and scroll the active step. Keep the heading/navigation and Start kingdom footer outside that scroll region. Reduce or replace the wide navigation rail at small effective widths. Remove the blanket scrollbar suppression.

**Check:** Every field and validation message can be reached on all four steps, with Start kingdom visible or predictably reachable at every supported scale.

### GUI-03 — P1 — Detached windows do not accommodate larger UI scale

**Observed:** The Mining window remains 260×300 pixels at 150% scale. Its title wraps, only three orders fit fully, and the remaining orders are cut off. Inventory's left-hand content and footer also fall outside its native surface at this scale. Larger text is being placed inside essentially unchanged native window dimensions.

**Evidence:** [Mining at 150%](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-150/mine_menu.png), [Inventory at 150%](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1280x720-150/inventory-detached.png). Fixed initial sizes: [mainwindow.cpp](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/mainwindow.cpp:157) and [management window creation](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/mainwindow.cpp:2390).

**Resolve:** Derive native window sizes from the selected UI scale and content needs, then cap them to the monitor's usable area. When the content cannot fit, scroll the body rather than clipping it. Apply the same rule to orders, management, and creature windows.

**Check:** All mining orders, Inventory's first column, and its bottom actions remain reachable at 150% and 200%, including on a smaller display.

### GUI-04 — P1 — HUD statistics overlap the speed controls

**Observed:** At 1280×720 / 150%, `Gnomes 5` paints across the Fast button. The level controls, kingdom name, speed controls, and summary compete for one unbroken row.

**Evidence:** [HUD overlap](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1280x720-150/hud.png), [hud.rcss](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/hud.rcss:31).

**Resolve:** Establish a compact HUD arrangement based on available width. Reserve space for level and speed controls; truncate the kingdom name with a full-name tooltip and move secondary statistics into a second compact row or expandable summary. Account for long kingdom names and watched resources.

**Check:** No text crosses another control at the tested scales, with long names and multiple watched resources.

### GUI-05 — P2 — Three Settings checkboxes have no visible primary label

**Observed:** Fullscreen, Follow monitor refresh, and Wheel changes level show their descriptions and checkboxes, but their actual setting names are missing. This occurs at both 1600×900 and 1280×720 at 100%.

**Evidence:** [Settings at 100%](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1600x900/settings.png), [settings.rml](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/settings.rml:12), [label styling](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/shell.rcss:158).

**Resolve:** Give text-only checkbox labels a reliable block layout, or wrap their text in an explicit child as the visible slider headings do. Keep the label adjacent to its checkbox and make the label activate that checkbox.

**Check:** All seven setting names appear, including after reopening Settings and changing scale.

### GUI-06 — P2 — Data-heavy management windows should be resizable

**Observed/source-confirmed:** Management windows have draggable title bars but no visible resize grip. The native window uses a frameless style and implements title dragging, with no corresponding resize interaction. This prevents players from widening Inventory or giving rosters and assignment screens more height. Inventory shows only about five visible entries despite reporting 2,301 rows in the test state.

**Evidence:** [Inventory](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/inventory.png), [RmlUiDetachedWindow.cpp](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/ui/runtime/RmlUiDetachedWindow.cpp:39), [mouse handling](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/ui/runtime/RmlUiDetachedWindow.cpp:265).

**Resolve:** Add clear edge/corner resizing to Inventory, Population, Military, diplomacy, and production management. Define a readable minimum size, adapt inner panes, and remember size per window type. Simple order menus should normally size themselves to their content. Creature windows should preserve the existing three-button row beneath the portrait when adapting their size.

**Check:** Dragging each resize edge changes the actual window and usable content area; controls stay reachable after shrink, reopen, and a scale change. Physical resize interaction was not tested in this review.

### GUI-07 — P2 — Inventory clips its right-hand controls even at 100%

**Observed:** The right edges of the sort toolbar, History action, and Watch selected area run into the window boundary. The body and its children are assigned 760dp, the same width as the outer workbench, despite the surrounding body padding and borders.

**Evidence:** [Inventory at 100%](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/inventory.png), [management6b.rcss](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/windows/management6b.rcss:2).

**Resolve:** Size the body and rows to the inner content box, including scrollbar allowance. Let the toolbar wrap or simplify sorting controls. Give the data table a deliberate column-sizing policy and horizontal scrolling only if its minimum readable width cannot fit.

**Check:** The right border and complete hit area of every toolbar/footer control remain visible, with History both closed and open.

### GUI-08 — P2 — Population's paging footer is clipped

**Observed:** The settled Citizens screen shows the roster and the bottom revision strip, but its `Previous rows`, `Next rows`, and page-count footer are absent. Those controls exist directly after the roster in the document. The roster takes 100% of its section's height, while the section clips overflow, leaving no space for its heading and footer.

**Evidence:** [Population roster](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/population.png), [population_manager.rml](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/windows/population_manager.rml:22), [roster sizing](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/windows/management6b.rcss:16).

**Resolve:** Make the section a vertical layout with a fixed heading/footer and a roster that uses only the remaining height. Hide paging controls explicitly when unnecessary, rather than allowing layout clipping to hide them.

**Check:** A roster larger than one page can reach its final citizen; page controls and count remain visible at minimum window height.

### GUI-09 — P2 — Military's lower assignment area has almost no usable height

**Observed:** At 100%, the Unassigned citizens list is reduced to a thin strip immediately above the status footer. The empty roster message also crowds its heading. At 150%, the screen is already clipped at the Squad name section. The settled capture contains loaded squad data, so this is not the loading state.

**Evidence:** [Military at 100%](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/military.png), [Military at 150%](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-150/military.png), [management6c.rcss](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/windows/management6c.rcss:114).

**Resolve:** Give the squad list and selected-squad details separate, bounded regions. Scroll the detail body as a whole, keep its status/actions outside that scroll region, and reserve enough height for member selection. Let a wider window use the intended list/detail arrangement; use a clear list-to-detail flow when narrow.

**Check:** With a squad and unassigned citizens, the player can see/select citizens and reach all assignment actions at both scales.

### GUI-10 — P2 — Build cards waste space and force unnecessary scrolling

**Observed:** Furniture cards are fixed at 200×216dp. Simple cards leave roughly 80–100 pixels unused below their Build button. Three columns leave another large unused strip to the right, and the next row's actions are below the initial viewport.

**Evidence:** [Settled Build catalog](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/build_menu.png), [fixed card rules](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/hud.rcss:229), [later fixed-size override](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/hud.rcss:330).

**Resolve:** Use compact, content-sized cards or rows. Lay out the icon/name, requirements, material controls, and action with a consistent small gap. Calculate columns from available width and distribute the space. Allow complex recipes to grow without imposing their height on every simple item.

**Check:** More complete simple items fit in the initial view, no large unused strip remains, and recipes with several components still show every requirement/action.

### GUI-11 — P3 — Small order menus have unnecessary bottom gaps

**Observed:** Agriculture leaves about 70px under Remove plant; Designations leaves about 70px under Remove; Job commands leaves about 100px under Raise priority. These are empty areas inside the visible panels, not transparent capture margins.

**Evidence:** [Agriculture](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/detached-100/agriculture_menu.png), [Designations](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/detached-100/designations_menu.png), [Job commands](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/detached-100/jobs_menu.png), [window sizes](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/mainwindow.cpp:157).

**Resolve:** Fit each simple menu to its header, help, buttons, and a small bottom inset. Cap the resulting height to the screen and enable body scrolling when necessary. Recalculate after localization or scale changes.

**Check:** Only a deliberate small inset remains below the final button at 100%; larger scales do not turn the compact layout into clipping.

### GUI-12 — P3 — Empty Missions leaves a large dead panel area

**Observed:** With no active missions, the empty message and revision strip occupy the upper half of the panel, followed by more than 300px of blank silver space. The panel's height remains anchored between fixed top/bottom offsets regardless of its content.

**Evidence:** [Settled Missions empty state](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/missions.png), [workbench sizing](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/windows/management6c.rcss:14).

**Resolve:** Give an empty Missions window a compact initial height, or use the available body as one intentional empty-state region with the footer at the bottom. Explain the relevant next step in game terms. Preserve a player's manually chosen size after resize support is added.

**Check:** Empty, loading, and populated states each have deliberate spacing; the status strip is not stranded halfway down the window.

### GUI-13 — P3 — Setup spreads sparse content across an oversized layout

**Observed:** At 1600×900, World and Review finish most content around y=350–370, but their footer sits near y=590, with a further large unused area below. Terrain distributes five controls across its first row and gives Wild animals an almost full-width second row. The result is visually unbalanced and needlessly separates Start kingdom from the choices.

**Evidence:** [World](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1600x900/setup_world.png), [Terrain](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1600x900/setup_terrain.png), [Review](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1600x900/setup_review.png), [setup layout](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/shell.rcss:172).

**Resolve:** Use a bounded, centered workbench and a balanced two- or three-column card layout. Keep a consistent action location close to the workbench, with only enough minimum body height to avoid distracting jumps between steps. Combine this with GUI-02's scrollable body for short screens.

**Check:** Sparse steps have no large internal hole, terrain cards have comparable widths, and the footer stays both easy to find and reachable.

### GUI-14 — P2 — Load game reports “No saves” before a kingdom is selected

**Observed:** On initial entry, the left side lists a kingdom without a selected-row highlight. The right side says `No saves for this kingdom` and suggests selecting another kingdom, even though the user has not selected one yet. Continue can find a compatible save in this same isolated profile.

**Evidence:** [Load screen](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1600x900/load_game.png), [empty-state text](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/load_game.rml:11), [binding](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/ui/screens/shell/ShellRmlBinding.cpp:433).

**Resolve:** Distinguish “Select a kingdom to see its saves” from an actually empty save list and from loading/error states. Alternatively, select the most recent compatible kingdom and request its saves on entry. Preserve separate messages for no kingdoms and no saves.

**Check:** Initial entry never implies missing saves just because a selection has not yet been made.

### GUI-15 — P2 — Management search fields appear as unlabeled blank boxes

**Observed:** Inventory, Population, Military, and Missions show blank search fields with no visible label or placeholder. Their RML contains placeholder text, but it does not appear in these captures. Players must infer the fields' purpose and search scope.

**Evidence:** [Inventory](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/inventory.png), [Population](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/population.png), [Military](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/military.png). Example markup: [military_manager.rml](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/windows/military_manager.rml:23).

**Resolve:** Add a persistent concise Search label or identifiable search icon with accessible text, and render a supported empty-field hint describing the current scope. Keep the label visible after typing and provide a clear action to remove the filter.

**Check:** The field's purpose is obvious when empty, focused, populated, and after switching tabs.

### GUI-16 — P2 — Sort controls imply dropdown menus and one label is clipped

**Observed/source-confirmed:** Inventory and Population use several adjacent buttons labeled `Sort: … v`; their callbacks directly change the sort instead of opening a dropdown. Military uses the unclear `Source order v`. Missions' `Name/type order` label is clipped by its fixed-width button and the adjacent Refresh button.

**Evidence:** [Inventory](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/inventory.png), [Missions](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/missions.png), [sort callbacks](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/ui/screens/management6b/Management6BRmlBinding.cpp:196), [toolbar sizing](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/windows/management6c.rcss:101).

**Resolve:** Use one genuine Sort selector with a separate direction control, or sortable column headers with clear ascending/descending indicators. Name the actual ordering criterion rather than “Source.” Size labels to content and preserve the selected sort state.

**Check:** The control's visual appearance matches its interaction; all labels fit, and the current sort/direction is understandable without clicking each option.

### GUI-17 — P3 — New game and Set up game are too similar in meaning

**Observed:** The main menu presents New game, which immediately starts the default setup, separately from Set up game, which opens configuration. Users have to read small help text to discover that the expected configuration step is skipped by New game.

**Evidence:** [Main menu at 100%](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/1600x900/main_menu.png), [main_menu.rml](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/main_menu.rml:21).

**Resolve:** Keep both routes but label them by outcome, for example Quick start and Custom game. Explain the chosen defaults plainly. Make the primary action depend on whether a usable Continue save exists, and give Tutorial a clear place without competing ambiguous start actions.

**Check:** A new player can choose immediate play, custom setup, or tutorial from the button labels alone.

### GUI-18 — P3 — Player-facing text contains implementation language and unclear units

**Observed:** Examples include `1 x WoodBedFrame` / `FancyBedFrame` in Build; `Population revision 1`; `Squads r1 / Roles r1`; `No current rows`; “authoritative game-thread snapshot”; “accepted default setup”; and “current game-backed peaceful setting.” Setup values include `1 width`, `100 high`, and `1 settlers`. Job commands refers to a “top-bar Jobs button,” although overlays now live in the sidebar.

**Evidence:** [Build](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/build_menu.png), [Population](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/population.png), [Missions](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/missions.png), [Job commands](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/detached-100/jobs_menu.png), [range-value formatting](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/ui/screens/shell/ShellRmlBinding.cpp:607), [build requirement formatting](C:/Programming/Repos/Ingnomia2/Ingnomia/src/gui/ui/screens/hud/HudRmlBinding.cpp:226).

**Resolve:** Resolve item/material IDs to their existing localized display names. Keep revisions, bridge terminology, and request IDs in diagnostics. Use plain messages such as Loading citizens or No active missions, describe Peaceful beginning's actual gameplay effect, and display units/plurals correctly. Update the Jobs help to refer to the sidebar overlay.

**Check:** No internal IDs or implementation terminology appears in normal screens; values communicate their meaning and remain grammatically correct for one and many.

### GUI-19 — P2 — Tool mode, cancellation, and rotation guidance are hidden

**Source-confirmed:** The HUD contains an active-tool label, status text, cancellation guidance, and rotation hint, but the final `.l-hud-hints` rule sets the strip to `display: none`. Cancel/Rotate buttons are also hidden in `hud_keyboard_commands`, and the detached tool-window style hides its hint strip. Once a tool window is dismissed, the UI has no persistent textual reminder of the selected operation or these controls.

**Evidence:** [HUD markup](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/game_hud.rml:18), [hint markup](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/game_hud.rml:82), [hidden strip](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/hud.rcss:385), [detached overrides](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/orders_tools.rcss:19).

**Resolve:** Show a small contextual strip only while an order/placement tool is active: a player-readable operation name, Cancel, and Rotate when applicable. Use the existing command bindings and actual shortcuts. Keep it compact so the map retains its space.

**Check:** After choosing an order and closing/moving its palette, the current operation remains identifiable, cancellation is discoverable, and rotation is shown only for operations that support it. Map placement/cancellation was not physically exercised during this review.

## Additional risk to verify

**Inspector long-text wrapping:** Current runtime logs reject several `overflow-wrap` declarations in the inspector stylesheet. The intended long-word wrapping therefore cannot be assumed to work. The ordinary creature fixture did not demonstrate a long-name overflow, so this is a source/log risk rather than a confirmed visual defect.

- Evidence: [runtime log](C:/Programming/Repos/Ingnomia2/Ingnomia/.verification/gui-review-20260913/settled-100/creature_profile.log.txt), [inspector.rcss](C:/Programming/Repos/Ingnomia2/Ingnomia/content/rmlui/screens/inspector.rcss:45).
- Resolve: replace rejected declarations with behavior supported by the pinned renderer; constrain long names and use wrapping or ellipsis plus a full-text tooltip as appropriate.
- Check: long unbroken names, long material descriptions, and pseudo-localized text in both ordinary and detached inspectors. Preserve the Stats / Center on map / Professions & Skills row beneath the portrait.

## Suggested order and review limits

1. Fix scaling and access to controls first: GUI-01 through GUI-04.
2. Fix missing labels, window resizing, and clipped management content: GUI-05 through GUI-09.
3. Improve use of space: GUI-10 through GUI-13.
4. Clarify navigation, searching, sorting, copy, and tool feedback: GUI-14 through GUI-19.

The existing capture matrix's `pause_menu` case activates the simulation Pause/Resume button; it did not capture the actual pause menu. Its Kingdom launcher case references a missing current element. Several other original matrix images captured only the main game behind detached windows; the detached and delayed captures linked above supersede those images. These capture-tool mismatches are not counted as GUI defects.

The review does not claim exhaustive coverage of every inventory expansion, schedule, military subtab, production/stockpile/farm editor, tutorial step, long localized string, or save-error condition. Those areas received either partial source inspection or no live exercise. Keyboard/gamepad traversal, physical drag/resize, and gameplay mutation were not acceptance-tested. Ordinary centered main-menu background space, empty roster capacity by itself, and the event prompt's normal spacing were not counted as defects.

Only this report and isolated review evidence were added. Existing user changes were preserved.
