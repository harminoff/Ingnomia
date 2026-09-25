# Ingnomia — Windows 98 UI audit and element-level fix list

**Repository:** harminoff/Ingnomia  
**Reviewed revision:** `bca0bee3acf79d460360c5f6b96fde08db784585` (`master` snapshot)  
**Overall design-alignment score:** **67/100**  
**Element/control-group work items:** **98**

## Scope and confidence

This is a source-based design audit of the player-facing RmlUi templates, shared styles, selected route overrides and the design-system verifier. It is not a live gameplay test, pixel-perfect screenshot review, accessibility certification or complete controller/engine audit. No repository changes were made. The application was not built or run for this review, and the repository screenshot image bytes could not be visually validated in this session. The README explicitly distinguishes the real fresh-world screenshot from management screenshots populated with deterministic demo data.

Every item below separates a source observation from a proposed fix and a runtime acceptance test. A control's presence in markup does not prove that its dynamic state or keyboard behavior works. Conversely, the absence of behavior in a template does not prove that a controller fails to implement it. Dynamic row contents, inline runtime styling and host-window constraints require the tests described here. Historical review documents were not treated as proof of current bugs. Developer-only tools and fixture screens are not assigned player-facing UI scores.

Scores are subjective design judgments, not measured defect rates. The overall score is calculated from the weighted rubric below, not from an average of all element scores. Per-element scores indicate the observed design foundation for that control group, including its authored layout/semantics where rendering was not available.

## Verdict

The project has a strong object-management foundation and several convincing classic controls, especially the inventory report and construction inspector. The main weakness is not lack of bevels: it is inconsistent control meaning and overlapping theme systems. A coherent shared implementation would improve this UI more than another route-by-route cosmetic reskin.

Keep the map-centered game, detachable management concept, flat resource tables, contextual inspectors, dual-list editors, real unavailable states and explicit consequential-action confirmations. Do not replace them with modern dashboard cards or remove commands to make screens look simpler.

## Weighted score

| Dimension | Weight | Score / 10 | Weighted points |
|---|---:|---:|---:|
| Object- and task-centered organization | 20% | 8 | 16 |
| Control meaning and state consistency | 20% | 6 | 12 |
| Visual-system coherence | 15% | 5.5 | 8.25 |
| Navigation and workflow predictability | 15% | 6.5 | 9.75 |
| Density and readable layout | 10% | 7 | 7 |
| Feedback and safety affordances | 10% | 8 | 8 |
| Keyboard/accessibility evidence in source | 10% | 6 | 6 |
| **Total** | **100%** | | **67/100** |

## Historical target, not a pixel-copy mandate

The reference baseline is Microsoft’s *The Windows Interface Guidelines — A Guide for Designing Software*, a February 1995 draft from the Windows 95/98 design lineage, including the property-sheet illustration on printed page 175 (PDF page index 155). It is not represented here as a Windows 98-specific pixel specification. Its useful distinction is between peer property pages, commands, and selection-following inspectors. Its editing model distinguishes applying a draft from closing a window. Modern accessibility, readable scaling and game-specific controls should remain improvements, not be sacrificed to historical limitations.

For this project I recommend restrained gray chrome, square corners, directional bevels, neutral command buttons, white inset data regions, a consistent navy selected-row treatment, and a separate focus cue. These are proposed visual targets, not claims that Windows 98 prohibited custom colors, side navigation or game-specific imagery. A build-category navigator and an equipment paper doll should remain specialized controls.

## Priority and evidence rules

**P1:** Resolve or verify in the next implementation pass because inconsistency affects state, scope, reachable controls, safety or the validity of tests. A P1 verification item is not an assertion of an already reproduced runtime failure.  
**P2:** Next usability/layout pass.  
**P3:** Finish and polish after the component/state contract is stable.  
**P0:** None established by this source-only review. No crash, data-loss event or unusable runtime state was reproduced.

All paths below are relative to `content/rmlui/`, except paths beginning `tests/`. Inspect the corresponding controller and host before implementation. Preserve public action IDs, game-thread/EventConnector contracts, catalog constraints, saved data compatibility and existing supported behaviors. Changing a presentation must not silently broaden a command's target scope.

## Source-confirmed issues to address first

1. Competing cave, classic-park and route-specific Win98 definitions; the token file is not a dependable description of all effective production values.
2. The common navigation-rail rule uses the same styling for selected and focused buttons.
3. Shared scrollbar rules use `track`/`slider`, while the documented generated RmlUi children are `slidertrack`/`sliderbar`; some routes already use the correct names.
4. The verifier bans `@media` while production styles use it, and legacy token checks can be satisfied by comments.
5. Several manager templates use vertical command-shaped controls for a small set of peer property pages despite having a connected-tab primitive available.
6. Exact numeric parameters, persistent Boolean rules and exclusive value choices do not use a consistent set of control types.

Do not turn source risks into invented runtime bug reports. For example, the inventory minimum-width/horizontal-clipping combination needs a supported-host/scale reproduction before claiming a column is inaccessible.

## Element-level audit and fix list

### Shared control system

#### SYS-01 — Theme ownership · 4/10 · P1

**Source:** `styles/tokens.json; styles/base.rcss`.

**Observed in source:** The token document identifies classic-park, retains a cave-design schema and dark palette, and specifies dimensions different from later shared overrides. Management-specific styles add a third, Win98-gray palette.

**Keep:** A named token source and reusable styles already exist.

**Fix:** Establish a single production theme contract. Give surfaces, text, borders, focus, selection and disabled state semantic names. Keep historical themes only behind explicit theme scope; regenerate expanded RCSS values.

**Acceptance:** One documented production token set accounts for every shared state. Changing a token changes all intended production controls, without relying on obsolete comments.

#### SYS-02 — Stylesheet cascade · 4/10 · P1

**Source:** `styles/base.rcss; styles/components.rcss; templates/management_window.rml`.

**Observed in source:** Dark declarations are followed by classic-park replacements, then route-specific Win98 rules. Accessibility is loaded before additional management styling.

**Keep:** Shared templates provide a useful consolidation point.

**Fix:** Separate structural rules from skin rules. Remove superseded production declarations after coverage exists. Explicitly test accessibility precedence rather than appending another override block.

**Acceptance:** A component-state gallery and representative production windows report the same computed colors, borders and dimensions for equivalent controls.

#### SYS-03 — Window frame and resize affordance · 6/10 · P2

**Source:** `styles/components.rcss; windows/management6b.rcss`.

**Observed in source:** Shared windows use a pale-blue frame, small radius and hard shadow; inventory explicitly uses gray square chrome without shadow. The shared resize grip starts hidden.

**Keep:** Movable framed windows and explicit content regions fit an object-management application.

**Fix:** Use one gray square frame and consistent raised edges. Decide which host owns native versus custom chrome. Show resize affordances only where resizing really works; do not add decorative minimize/maximize controls.

**Acceptance:** Drag, resize, close, reopen and change UI scale in each supported host. The frame remains reachable and there is no duplicate native/custom title bar.

#### SYS-04 — Title bars and Close controls · 6/10 · P2

**Source:** `styles/components.rcss; windows/military_manager.rml; windows/workshop_manager.rml`.

**Observed in source:** Most templates have a word-sized Close button in their header. Several object managers have a subtype plus title, while other windows use a single-line shared title.

**Keep:** The selected object is identified in the window header.

**Fix:** Standardize a compact title, optional small object icon, and small close glyph with an accessible name. Move subtype and coordinates into a secondary information row where needed. Differentiate active and inactive hosts.

**Acceptance:** Long names truncate safely while the full name is recoverable. Close has the same hit area and keyboard behavior in every window.

#### SYS-05 — Icons and glyphs · 5/10 · P3

**Source:** `styles/components.rcss; screens/game_hud.rml; screens/inspector.rml`.

**Observed in source:** Title marks include bars or letters, and multiple controls use literal v and ^ characters. Actual sprite raster quality was not visually inspected.

**Keep:** Text labels prevent important commands from being icon-only.

**Fix:** Create a small consistent icon set for close, drop-down, spinner arrows, sorting, expand/collapse, warnings and object kinds. Keep game-item sprites separate from chrome icons.

**Acceptance:** Glyphs align consistently and remain recognizable at supported scales. Every actionable icon has an explicit name or nearby label.

#### SYS-06 — Typography · 6/10 · P2

**Source:** `styles/base.rcss; windows/management6b.rcss; screens/shell.rcss`.

**Observed in source:** The shared family is LatoLatin. Body text is 13dp, but inventory filter/header text reaches 10dp and setup scale labels 9dp.

**Keep:** A pinned licensed font makes output deterministic.

**Fix:** First normalize text hierarchy and raise essential small text. A more period-like licensed font is optional, not the first fix; changing the font also requires updating metric and asset-verifier expectations.

**Acceptance:** Names, numeric values, filter labels and errors remain legible at default scale. No clipping at 100%, 125%, 150% and 200%, including long test strings.

#### SYS-07 — Ordinary push buttons · 7/10 · P2

**Source:** `styles/base.rcss`.

**Observed in source:** The shared button has an appropriate raised bevel, but default hover turns yellow and pressed state orange.

**Keep:** Raised and pressed edges already communicate activation.

**Fix:** Keep command buttons neutral. Reverse the bevel and make a small content offset on press; do not make every hovered command a colored primary action. Preserve dimensions between states.

**Acceptance:** Normal, hover, pressed, focused and disabled versions are distinguishable without layout movement or a change in semantic color meaning.

#### SYS-08 — Default/primary button · 5/10 · P2

**Source:** `styles/components.rcss; modals/confirm_destructive.rml`.

**Observed in source:** Primary buttons are yellow. The generic destructive confirmation assigns the primary class to Cancel.

**Keep:** The safe action receives visual emphasis in the generic confirmation.

**Fix:** Separate default keyboard action from marketing-style primary color. Use a classic outer default outline while retaining a separate inner focus cue. Set default behavior deliberately per dialog.

**Acceptance:** The visible default matches what Enter actually activates. Focus can move to a different control without falsely transferring the default outline.

#### SYS-09 — Destructive buttons · 6/10 · P2

**Source:** `styles/components.rcss; windows/military_manager.rml; windows/workshop_manager.rml`.

**Observed in source:** Destructive commands use an orange/salmon skin; some labels describe the object well, while a generic Confirm fallback also exists.

**Keep:** Destructive actions and several confirmations are explicitly represented.

**Fix:** Prefer neutral command chrome plus a precise action verb and consequence. Use warning color sparingly. Preserve existing confirmations and add recovery where supported, rather than making every action modal.

**Acceptance:** Delete squad, overwrite template, discard progress and complete trade each name the affected object and result. Harmless actions do not acquire unnecessary warnings.

#### SYS-10 — Keyboard focus versus selection · 4/10 · P1

**Source:** `styles/management_window.rcss; styles/base.rcss`.

**Observed in source:** The management-rail selected selector and :focus selector share the same pressed appearance. Shared focus-visible replaces border color with orange.

**Keep:** Focusable controls and explicit state selectors are present.

**Fix:** Give focus its own dotted or similarly unambiguous inner indicator, implemented with supported RCSS/decorators. Keep it independent from selected, checked, default, hover and disabled states.

**Acceptance:** A keyboard user can distinguish the active page, focused control and default action simultaneously. Moving focus does not falsely indicate a changed setting.

#### SYS-11 — Property tabs · 7/10 · P1

**Source:** `styles/components.rcss; windows/military_manager.rml; windows/population_manager.rml`.

**Observed in source:** The shared .c-tabs__tab already implements a raised selected tab connected to the page. Several main managers instead render vertical rail buttons.

**Keep:** The existing connected-tab geometry is a better starting point than a new widget.

**Fix:** Use top property tabs for small peer sets such as Squads / Roles & Uniforms / Target Priorities. Link each tab to its panel and use a deliberate keyboard activation model. Do not turn hierarchical catalogs into dozens of tabs.

**Acceptance:** One selected tab joins the page with no bottom seam. Focus is separate. Tab, arrows and Ctrl+Tab follow the chosen contract without firing unrelated commands.

#### SYS-12 — Category navigation rails · 5/10 · P2

**Source:** `styles/management_window.rcss; screens/management6a.rcss`.

**Observed in source:** Several managers spend a fixed 112dp on a rail of peer-view buttons, sometimes alongside another fixed list pane.

**Keep:** Hierarchical category navigation is appropriate for the build catalog.

**Fix:** Reserve side navigation for categories or hierarchy. Move small peer-view sets to top tabs, and keep object lists in their own clearly labeled panes. Maintain a compact fallback when space genuinely runs out.

**Acceptance:** Military and population no longer require rail + object list + editor at ordinary size. Build categories remain understandable and do not become an overlong tab strip.

#### SYS-13 — Group boxes and content panels · 6/10 · P2

**Source:** `styles/components.rcss; screens/shell.rcss; windows/management6c.rcss`.

**Observed in source:** The code uses nested raised sections, inset cards and colored section heading bars in different screens.

**Keep:** Related fields are already grouped in markup.

**Fix:** Use thin group-box borders with a small legend for related fields. Reserve sunken borders for editable/list content and full raised frames for windows or genuine toolbars.

**Acceptance:** A user can identify each window, group and data region from border treatment alone; a simple form does not look like several nested windows.

#### SYS-14 — Text inputs and labels · 7/10 · P2

**Source:** `styles/base.rcss; windows/stockpile_manager.rml; screens/new_game.rml`.

**Observed in source:** Named fields often have visible labels, but some search and setup fields rely on placeholders or visual span labels.

**Keep:** Inset editable surfaces are established.

**Fix:** Standardize labels, editable white fields, read-only treatment and inline validation. Associate labels programmatically through the actual RmlUi/application input model. Do not use a placeholder as the only persistent label.

**Acceptance:** Users can recover the field purpose after typing. Invalid input is explained in text, and read-only values do not falsely invite editing.

#### SYS-15 — Checkboxes and persistent Boolean options · 6/10 · P1

**Source:** `styles/base.rcss; windows/stockpile_manager.rml; panels/agriculture_manager.rml`.

**Observed in source:** Native checkbox inputs coexist with large check-buttons and text glyphs. The shared checked checkbox style uses a filled colored square/border.

**Keep:** Stockpile hauling options already use labeled checkbox inputs.

**Fix:** Adopt one checkbox model with an unmistakable check mark and full clickable label. Add mixed state only where the underlying data really supports it. Convert persistent Boolean button substitutes where appropriate.

**Acceptance:** Checked, unchecked, mixed and disabled are distinguishable in grayscale. Space toggles the focused checkbox and the label activates the same state exactly once.

#### SYS-16 — Exclusive choices · 5/10 · P1

**Source:** `windows/military_manager.rml; windows/diplomacy_missions.rml; windows/population_manager.rml`.

**Observed in source:** Attitudes, mission types and schedule activities are expressed as groups of ordinary or check-style buttons.

**Keep:** All choices are visible rather than hidden behind unexplained cycling.

**Fix:** Use radio groups, a proper single-select combo, or a correctly marked exclusive toolbar group. Clearly separate choosing a value from executing the action that uses it.

**Acceptance:** Exactly one supported value is selected. Merely changing focus cannot issue a command. Screen state and authoritative selection remain synchronized.

#### SYS-17 — Combos and drop-down lists · 6/10 · P1

**Source:** `styles/base.rcss; windows/management6c.rcss; windows/inventory_browser.rml`.

**Observed in source:** The shared select subparts retain dark styling while military/diplomacy define their own light subparts; inventory implements custom input-plus-button popups.

**Keep:** The custom filters have explicit labels, expanded state and listbox containers.

**Fix:** Centralize editable and noneditable combo variants, arrow glyph, popup selection, disabled state and open/close behavior. Preserve keyboard filtering and legal catalog-backed values.

**Acceptance:** Open by mouse and keyboard; choose, cancel, click away and reopen. Focus returns sensibly, selection persists, and popups remain within the relevant host bounds.

#### SYS-18 — Numeric fields and spinners · 6/10 · P2

**Source:** `styles/components.rcss; windows/stockpile_manager.rml; windows/workshop_manager.rml`.

**Observed in source:** There are different stepper arrangements; some priority fields use text inputs and literal arrow characters. A tooltip explains that 1 is highest.

**Keep:** Quantity and priority inputs expose bounds in several templates.

**Fix:** Use a shared numeric edit with integrated up/down arrows, visible units, bounds and meaning. Explain priority direction beside the field rather than only on hover.

**Acceptance:** Typing, stepping, paste, bounds, empty input and invalid characters produce the same authoritative value. Raise priority decreases the number where 1 is highest.

#### SYS-19 — Sliders · 6/10 · P2

**Source:** `styles/components.rcss; screens/new_game.rml; screens/settings.rml`.

**Observed in source:** Setup and settings expose many exact integer quantities primarily through sliders. Their readouts are separate text, not editable numeric fields.

**Keep:** Readouts and ranges make values discoverable.

**Fix:** Retain sliders for approximate adjustments such as volume. Add synchronized numeric editing for map size, depth, population, frame rate and other exact quantities; use a rectangular classic thumb if pursuing fidelity.

**Acceptance:** A user can enter an exact value without repeated dragging. Keyboard increments, displayed units and field validation agree.

#### SYS-20 — Report tables and list views · 8/10 · P2

**Source:** `windows/inventory_browser.rml; windows/management6b.rcss; styles/components.rcss`.

**Observed in source:** Inventory exposes category, type, item, material and numeric columns. Shared styles include numeric alignment support.

**Keep:** Comparable facts are presented as dense rows, not separate cards.

**Fix:** Standardize neutral header buttons, sort indicators, numeric alignment, row selection and truncation. Preserve the existing flat inventory model unless a different hierarchy is actually requested.

**Acceptance:** Headers align with every row at all tested widths. Sort order, focused row and selected row remain identifiable after filtering and refresh.

#### SYS-21 — Column filters · 7/10 · P2

**Source:** `windows/inventory_browser.rml; windows/stockpile_manager.rml`.

**Observed in source:** Inventory and stockpile duplicate six-column filter/sort markup and custom popup controls.

**Keep:** Column-specific filtering is a strong management-game capability.

**Fix:** Extract a shared header/filter component. Give active filters a visible indicator, provide one clear-all action and report the current match count where not already supplied dynamically.

**Acceptance:** Identical filters behave identically across inventory and stockpile. Empty results, reset and back navigation preserve predictable state.

#### SYS-22 — Selected, hovered and inactive rows · 5/10 · P1

**Source:** `styles/components.rcss; windows/management6b.rcss`.

**Observed in source:** Shared list selection is yellow, generic button selection pale blue, and inventory selection navy with white text.

**Keep:** Inventory already demonstrates a clear classic selected-row treatment.

**Fix:** Define one selection grammar, including inactive-window selection and a distinct focus cue. Reserve severity colors for semantic state, not arbitrary selection.

**Acceptance:** Selecting the same kind of object in different managers does not change the meaning of the highlight. Selected error/status rows retain readable text.

#### SYS-23 — Scrollbars · 5/10 · P1

**Source:** `styles/components.rcss; windows/management6c.rcss; windows/management6b.rcss`.

**Observed in source:** Some shared and military/diplomacy rules target scrollbarvertical track/slider. RmlUi documents slidertrack/sliderbar. Other inventory/population rules already use the documented child tags.

**Keep:** Explicit scrollbar sizing is present, and some routes have the correct implementation.

**Fix:** Consolidate on scrollbarvertical/horizontal plus slidertrack, sliderbar, sliderarrowdec and sliderarrowinc. Remove rules that cannot match the standard generated children. Verify the pinned engine rather than assuming browser defaults.

**Acceptance:** Every primary scroll region shows and operates its thumb, track and arrows. Test both axes, tiny ranges and the scrollbar corner. Do not infer that every current scrollbar is broken.

#### SYS-24 — Trees and matrices · 6/10 · P2

**Source:** `styles/components.rcss; windows/population_manager.rml`.

**Observed in source:** A shared tree component exists; production inventory has deliberately moved to a flat catalog. The duty board is a grid.

**Keep:** Different data structures can use different controls.

**Fix:** Do not reintroduce a tree into inventory merely for nostalgia. For real trees, separate expansion from selection; for matrices, provide cell focus, row/column identity and controlled scrolling.

**Acceptance:** Tree expansion does not activate the node. Matrix navigation reaches each cell without changing values until an explicit edit action.

#### SYS-25 — Tooltips · 5/10 · P2

**Source:** `styles/components.rcss; windows/military_manager.rml`.

**Observed in source:** The shared tooltip retains dark rounded shadowed styling. Managers supply tooltip containers and many explanatory titles.

**Keep:** Help text already exists for many nonobvious actions.

**Fix:** Use one compact, consistently positioned tooltip treatment, optionally pale info-yellow for the classic theme. Show equivalent help on focus. Keep essential consequences visible in the form itself.

**Acceptance:** No tooltip blocks its target, escapes its host, survives closure or covers the focused option. Critical instructions are not hover-only.

#### SYS-26 — Progress and loading indicators · 6/10 · P2

**Source:** `styles/components.rcss; screens/loading.rml`.

**Observed in source:** The loading template uses stage text and three indicator spans; the shared progress primitive still has dark/bronze styling.

**Keep:** Loading, error and retry states are distinct.

**Fix:** Use a classic inset progress bar only when meaningful progress is available. Otherwise show a truthful indeterminate indicator plus a named stage. Offer cancel only where cancellation is actually safe.

**Acceptance:** No invented percentage or completion animation is shown. Slow, failed and retried transitions produce understandable states.

#### SYS-27 — Status bars and badges · 7/10 · P2

**Source:** `styles/components.rcss; windows/population_manager.rml; screens/game_hud.rml`.

**Observed in source:** Status chips, per-manager footers and a hidden HUD hint strip coexist. Some status elements expose role=status.

**Keep:** The UI has dedicated places to surface feedback.

**Fix:** Use a quiet segmented status strip for counts, filters and pending results. Reserve strong color and modal interruption for actionable severity. Keep active-tool/cancel information available while a tool is armed.

**Acceptance:** Command pending, success and failure are visible without obscuring work. Status never overwrites a critical error before the user can recover.

#### SYS-28 — Modal dialog contract · 6/10 · P1

**Source:** `modals/confirm_destructive.rml; windows/military_manager.rml; windows/population_manager.rml`.

**Observed in source:** Confirmation templates differ in structure and ordering; population includes explicit dialog metadata that is absent from some other static confirmation templates.

**Keep:** Destructive operations already have dedicated confirmation surfaces.

**Fix:** Share a single modal shell with object-specific copy, safe default, consistent command order and appropriate semantics. Audit focus containment, background input blocking and restoration in the actual host code.

**Acceptance:** Tab cannot escape; Escape cancels; Enter respects the deliberate default; holding a key cannot accept twice. Closing returns focus to the invoking control. ARIA text alone is not proof of platform accessibility.

#### SYS-29 — Empty, unavailable and error states · 8/10 · P2

**Source:** `windows/diplomacy_missions.rml; screens/load_game.rml; windows/military_manager.rml`.

**Observed in source:** The templates distinguish no records, undiscovered neighbors, no compatible save, loading and error/retry.

**Keep:** These are useful semantic distinctions and should survive a reskin.

**Fix:** Standardize compact state presentation and next steps. Keep missing data different from zero. Include a retry only where a backing operation exists.

**Acceptance:** An empty filter does not resemble a loading failure, an undiscovered neighbor does not show fabricated zero statistics, and disabled actions explain the prerequisite.

#### SYS-30 — Narrow windows and scaling · 6/10 · P1

**Source:** `styles/management_window.rcss; windows/management6b.rcss; windows/management6c.rcss`.

**Observed in source:** The stylesheet has media fallbacks. Inventory also has a 580dp minimum flat-table width and a horizontally hidden list region. Host minimum sizes and controller behavior were not executed.

**Keep:** Responsive handling and minimum readable panes are explicitly considered.

**Fix:** Verify the RmlUi context size of each detached host. Either enforce a valid minimum, enable synchronized horizontal scrolling, or adapt columns deliberately. Do not silently clip essential controls.

**Acceptance:** At every supported host size and 100/125/150/200% scale, all fields, commands and columns remain reachable. Classify actual failures only after reproducing them.

#### SYS-31 — Accessibility and input verification · 6/10 · P1

**Source:** `styles/accessibility.rcss; screens/settings.rml`.

**Observed in source:** High-contrast and reduced-motion classes exist; the reviewed settings template does not expose these preferences. Some templates have detailed roles/labels while others do not.

**Keep:** Explicit semantic states and scaling are good foundations.

**Fix:** Trace how supported preferences are applied, then test every route under them. Complete consistent keyboard semantics and verify whether a platform accessibility bridge exists before making assistive-technology claims.

**Acceptance:** All actions are keyboard reachable, focused content remains visible, and high contrast survives route-specific selectors. Report unsupported capabilities honestly.

#### SYS-32 — Design-system verifier · 3/10 · P1

**Source:** `tests/ui-design-system/verify-design-system.cmake; styles/base.rcss`.

**Observed in source:** The verifier checks legacy palette strings, which can occur in comments, and rejects @media even though current production styles contain @media.

**Keep:** A verifier and component-fixture inventory already exist.

**Fix:** Replace historical string-presence checks with current token/structure checks and renderer-backed state tests. Align feature checks with the pinned RmlUi version. Retain genuine license and asset-integrity checks.

**Acceptance:** The verifier can run against the current intended design without a contradiction, fails real style drift and cannot pass a required visual state solely because a token appears in a comment.

### HUD, tools and onboarding

#### HUD-01 — Top rail: level, time, speed and summary · 7/10 · P2

**Source:** `screens/game_hud.rml`.

**Observed in source:** Level controls, pause/normal/fast, settlement name, clock/date and summary counts share the HUD rail.

**Keep:** Persistent simulation controls are easy to locate.

**Fix:** Group simulation speed separately from world facts, show an unambiguous paused state, and keep count labels readable when the rail contracts.

**Acceptance:** Only the selected speed appears active; queued pause acknowledgment is distinguishable from actual pause. Narrow layouts do not remove essential controls.

#### HUD-02 — Command shelf versus overlays · 6/10 · P1

**Source:** `screens/game_hud.rml`.

**Observed in source:** The shelf contains both commands and persistent overlay toggles; some labels repeat, such as Jobs and Designations.

**Keep:** Management and map tools are categorized.

**Fix:** Give persistent overlays checked/toggled presentation and command launchers ordinary command presentation. Clarify duplicate labels using section context or names such as Show jobs.

**Acceptance:** A new user can tell whether a click opens a manager, arms a tool or toggles an overlay before clicking.

#### HUD-03 — Active-tool and cancellation feedback · 6/10 · P1

**Source:** `screens/game_hud.rml; screens/orders_tools.rml`.

**Observed in source:** The main HUD hint strip is initially hidden, while the detached tools template exposes Cancel and Rotate. Controllers may project additional state.

**Keep:** Explicit cancellation and rotation actions exist.

**Fix:** Guarantee one persistent armed-tool indicator with contextual cancellation and rotation help across both tool hosts. Keep tools from remaining silently armed after a window closes.

**Acceptance:** Arm every placement/mining tool, switch windows and cancel by all supported paths. The world never receives a stale or accidental action from closing UI.

#### HUD-04 — Build categories and catalog · 7/10 · P2

**Source:** `screens/game_hud.rml; screens/orders_tools.rml`.

**Observed in source:** Build uses a category rail, a type subpage and a catalog area.

**Keep:** A category navigator suits hierarchical building choices.

**Fix:** Retain category navigation, preserve selected category on return, and show selected product/material requirements adjacent to placement. Ensure category and type levels are not confused with property tabs.

**Acceptance:** A user can select a buildable, inspect its requirements, go back and resume without losing the intended item or placement orientation.

#### HUD-05 — World labels and selection pointer · 7/10 · P2

**Source:** `screens/inspector.rml`.

**Observed in source:** The inspector defines a world label and a pointer tip for tool/selection size.

**Keep:** Selection feedback is spatially connected to the map.

**Fix:** Keep labels small and readable without covering the target. Include invalid placement explanations in text and do not rely exclusively on a red/green tint.

**Acceptance:** Labels remain within host bounds and clear when the operation ends. Invalid areas cannot be mistaken for valid placement in grayscale.

#### HUD-06 — Tutorial panel · 7/10 · P2

**Source:** `screens/game_hud.rml`.

**Observed in source:** Tutorial UI has objective, instructions, checklist, warning and Continue/Skip/Restart/Hints/Continue anyway actions.

**Keep:** Actionable objectives and progress already exist.

**Fix:** Make the next step the dominant action. Put skip/restart in a secondary area and distinguish Continue anyway from verified completion. Permit non-obstructive placement/minimization only with retained progress.

**Acceptance:** The tutorial does not cover the required target or claim a skipped objective was completed. Keyboard focus remains on the tutorial when interacting with it, not on the world beneath.

#### HUD-07 — Event dialogs · 7/10 · P1

**Source:** `screens/game_hud.rml`.

**Observed in source:** The event blocker has dialog semantics and Continue/Yes/No controls.

**Keep:** Modal events are separated from passive HUD information.

**Fix:** Project only the relevant command set, use action-specific labels where practical, and apply the common safe-default/focus contract.

**Acceptance:** Invisible alternatives are not focusable. Repeated Enter/Escape cannot accept a second event or act on the world after the first closes.

### Inventory and resources

#### INV-01 — Six-column report header · 8/10 · P2

**Source:** `windows/inventory_browser.rml; windows/management6b.rcss`.

**Observed in source:** The report exposes six meaningful columns with sort buttons and direction spans. Its route styles implement neutral gray beveled headers.

**Keep:** This is one of the strongest Win98-oriented components.

**Fix:** Reuse this header grammar across comparable managers. Keep numerical columns right-aligned and their units/definitions consistent.

**Acceptance:** Category, type, item, material, stock and total align and sort correctly without inconsistent header visuals.

#### INV-02 — Header filter inputs and popups · 7/10 · P2

**Source:** `windows/inventory_browser.rml; windows/management6b.rcss`.

**Observed in source:** Each header includes a custom filter combo; numeric filters are readonly. The route specifies 10dp filter text.

**Keep:** Specific filters support a large resource catalog.

**Fix:** Make read-only filters look like drop-down choices, standardize arrows and enlarge essential text. Add a clear-all filter command and match summary if not already projected by the controller.

**Acceptance:** All six filters are usable by keyboard and mouse; popups are neither clipped nor obscured; active filters remain obvious after detail navigation.

#### INV-03 — Inventory row density and narrow width · 6.5/10 · P1

**Source:** `windows/management6b.rcss`.

**Observed in source:** Flat rows are at least 48dp tall with 40dp sprites, while header text is 10dp. The flat table has a 580dp minimum width and horizontal hiding in the list style.

**Keep:** Sprites make items recognizable, and explicit virtual-spacer styling exists.

**Fix:** Offer a compact report density with smaller sprites and coherent text sizes. Verify clipping before changing the host contract; use synchronized horizontal scrolling or a documented minimum width.

**Acceptance:** Large inventories remain navigable, chosen rows stay visible, and no column becomes permanently inaccessible at supported UI scales.

#### INV-04 — Item detail and Back to inventory · 7.5/10 · P2

**Source:** `windows/inventory_browser.rml`.

**Observed in source:** The item page groups recipes, outputs, stockpile locations and history in four regions.

**Keep:** The relationships answer practical production questions.

**Fix:** Retain the content, but present it as compact grouped lists or property pages. Preserve source filters, sort, row selection and scroll when returning; keep the item identity visible.

**Acceptance:** Open a deeply scrolled filtered item, follow a supported location link and return. The prior inventory context is restored.

### Stockpiles

#### STO-01 — Stock / Allow list / Settings navigation · 7/10 · P2

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** Three peer views use a vertical rail and Center on map sits beside them.

**Keep:** Stock and acceptance rules are correctly separated.

**Fix:** Use three connected top tabs and place Center on map in a small object toolbar. Keep the stockpile name and suspended state persistently visible.

**Acceptance:** Switching pages preserves filters and drafts, and locating the stockpile does not look like another property page.

#### STO-02 — Physical stock table · 8/10 · P2

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** The stock page explicitly describes items physically stored here and exposes a six-column report.

**Keep:** Actual contents are not conflated with the allow list.

**Fix:** Keep the shared report pattern. Explain the meaning of Total relative to this stockpile and other locations, using actual backing data rather than assumed semantics.

**Acceptance:** The displayed quantities can be reconciled with the authoritative stockpile snapshot. Empty and filtered-empty states are different.

#### STO-03 — Allow list and bulk matching actions · 7/10 · P1

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** Allow matches and Block matches operate from the rule-search page.

**Keep:** Bulk actions are explicitly scoped to matches in their labels.

**Fix:** Display the number and scope of affected rules beside the bulk commands. Preserve a mixed state only where backing rules support it. Provide revert/undo or proportional confirmation for broad changes.

**Acceptance:** Filtering to a subset then applying either action changes exactly that intended subset, including any offscreen matches described by the UI.

#### STO-04 — Saved allow-list templates · 8/10 · P2

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** An editable template combo, Save new button and named overwrite confirmation are present.

**Keep:** The overwrite dialog names the template being replaced.

**Fix:** Make Save new versus Update existing explicit, preserve unsaved text, and share combo/dialog styling. Warn about an overwrite rather than silently replacing rules.

**Acceptance:** New, duplicate-name, overwritten, empty-name and canceled template operations leave the correct saved and current rules intact.

#### STO-05 — Name, priority and Apply · 7/10 · P2

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** Name and numeric priority are staged behind Apply; arrows expose raise/lower semantics and a 1-is-highest tooltip.

**Keep:** A separate Apply command makes the edit boundary visible.

**Fix:** Show priority direction inline and add an explicit dirty/revert strategy. Use the shared spinner and align labels/control widths.

**Acceptance:** Changing tabs or closing with an unapplied name/priority produces the documented result without silently discarding a draft.

#### STO-06 — Hauling checkboxes · 8/10 · P2

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** The two hauling rules use labeled checkbox inputs describing transfer direction.

**Keep:** These are already a good fit for Boolean settings.

**Fix:** Keep the wording and semantics; adopt the final shared check mark, focus and disabled styles. State whether these changes apply immediately.

**Acceptance:** Each checkbox changes only the intended rule and remains clear when one or both are disabled or checked.

#### STO-07 — Suspend/resume state · 6/10 · P2

**Source:** `windows/stockpile_manager.rml; screens/inspector.rml`.

**Observed in source:** Suspension is exposed both in the manager and an inspector summary.

**Keep:** The command is available near the object being managed.

**Fix:** Use a persistent Active/Suspended value plus a clearly labeled Suspend or Resume action. Keep every surface synchronized with the same authoritative state.

**Acceptance:** Toggle from either surface while the other is open. Both update consistently, including command rejection and pending state.

### Workshops and trade

#### WRK-01 — Craft / Queue / Settings / Trade navigation · 7/10 · P2

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** Workshop views are vertical rail buttons; Trade is conditionally hidden.

**Keep:** Craft creation and existing queue editing are separate tasks.

**Fix:** Use connected tabs for supported peer views, retain conditional availability and keep workshop identity/Center on map separate from navigation.

**Acceptance:** Unsupported trade pages do not leave empty tabs or keyboard stops; supported pages retain selection and pending edits.

#### WRK-02 — Available crafts and new order · 8/10 · P2

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** A craft list, search, quantity field, help, Add order and feedback form a list-detail flow.

**Keep:** This is a useful select-then-configure-then-act structure.

**Fix:** Keep recipe/material requirements next to the quantity and Add order button; use one numeric editor and show any blocked reason at the action.

**Acceptance:** A valid click creates one order. Missing inputs disable or reject it with a specific explanation, not a silent no-op.

#### WRK-03 — Production queue and order editor · 7/10 · P2

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** The queue explains top-to-bottom execution and has a selected-job quantity editor with Apply.

**Keep:** Order sequence has a stated meaning.

**Fix:** Expose clear reorder, suspend/resume, repetition and removal controls only for supported operations. Keep selection stable after moving/removing an order.

**Acceptance:** The visible order matches authoritative execution order. Editing a selected job cannot accidentally affect a newly selected or removed job.

#### WRK-04 — Workshop settings edit model · 6/10 · P1

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** Name/priority use Apply, while generated-order and auto-craft options are checkbox inputs and suspension is a button.

**Keep:** Settings are grouped by purpose.

**Fix:** Choose one staged form or explicitly distinguish instant options from draft fields. Add a revert/close-dirty contract and remove uncertainty about which changes have committed.

**Acceptance:** A user can predict the result of Close, Apply, changing tabs and reopening after both successful and rejected updates.

#### WRK-05 — Linked stockpiles · 7/10 · P2

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** A stockpile select, Link command, existing-links region and collection-order explanation are present.

**Keep:** The help describes a useful gameplay relationship.

**Fix:** Use a compact list with names, locate/unlink commands where supported and a clearly labeled selection combo. Preserve the explanation of collection precedence.

**Acceptance:** Linking an already linked or deleted stockpile is handled safely; names and status stay current without losing keyboard focus.

#### WRK-06 — Special production options · 6.5/10 · P2

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** Butcher and fisher options use button-shaped persistent settings in conditional sections.

**Keep:** Unsupported workshop-specific choices are initially hidden.

**Fix:** Render persistent options as consistent checkboxes, with clear state and any consequential behavior explained. Do not expose unsupported options for visual symmetry.

**Acceptance:** Opening different workshop types shows only legal settings and each checked value matches the current workshop.

#### WRK-07 — Trade ledger and confirmation · 6/10 · P1

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** Trading exposes Next trade row, Offer one less/more, Review trade and an explicit irreversible-trade confirmation.

**Keep:** Irreversibility is already called out.

**Fix:** Replace opaque sequential row cycling as the primary workflow with directly selectable rows and quantity editing. Present both sides and net value before commit; retain keyboard shortcuts as accelerators.

**Acceptance:** A user can inspect the exact exchange and cancel without effect. Confirm commits once, handles stale offers and refreshes authoritative totals.

### Agriculture: farms, groves and pastures

#### AGR-01 — Overview and statistics · 7.5/10 · P2

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Farm overview exposes plots, tilled, planted and ready-to-harvest counts with a product summary.

**Keep:** These counts describe production status directly.

**Fix:** Keep them in a compact summary strip rather than oversized dashboard cards. Use consistent units and explain relationships only as supported by the underlying data.

**Acceptance:** Values remain readable, do not falsely imply additive categories and agree with the authoritative designation snapshot.

#### AGR-02 — Farm plot grid · 7/10 · P1

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** The plot grid has Select all/Clear and a legend describing gray, brown, green and gold states.

**Keep:** Spatial editing connects a plan to actual plots.

**Fix:** Add a non-color state cue and distinct selected/focused plot treatment. Provide keyboard selection and accessible plot names in the implemented input system.

**Acceptance:** A user can identify empty, tilled, planted and ready plots without color alone, and can select precisely one, several or all plots.

#### AGR-03 — Crop catalog and availability · 7/10 · P2

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Search, crop catalog, chosen-crop text and counts are adjacent to plot assignment.

**Keep:** Selection precedes the assign action.

**Fix:** Keep the selected crop and availability visible while scrolling. Explain disabled or unavailable choices without substituting invented values.

**Acceptance:** The selected crop does not change silently after refresh; an unavailable crop has a specific backed reason.

#### AGR-04 — Assign crop / Use farm default / Set farm default · 6.5/10 · P1

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Three nearby commands affect related but different scopes.

**Keep:** The default-setting button already has a useful tooltip.

**Fix:** Separate Selected plots from Farm default in two named groups. State the affected plot count and explain the fallback relationship inline.

**Acceptance:** Users can predict whether an action changes selected plots, removes plot overrides or changes the farm-wide fallback.

#### AGR-05 — Planting counts and repeat queue · 6/10 · P1

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** The form explains that counts mean plantings on each selected plot and repeat orders move behind later orders.

**Keep:** The unusual queue semantics are documented.

**Fix:** Show a review sentence such as selected-plot count and plantings per plot, then a readable queue with explicit repeat state and supported reorder/remove actions. Keep the existing repeat semantics.

**Acceptance:** The UI never presents a per-plot count as a global total; queue mutation preserves later orders and the documented fallback.

#### AGR-06 — Farm and grove work toggles · 6.5/10 · P2

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Harvest, pick fruit, plant trees and fell trees use large check-buttons and checkbox glyphs.

**Keep:** The actions are split by designation type.

**Fix:** Use true persistent checkbox styling and consistent wording. Keep destructive or long-lived implications in the group help rather than relying on orange color.

**Acceptance:** Toggling a work rule changes only that rule, and the state remains clear after switching designation types.

#### AGR-07 — Pasture animals, caps and food rules · 5/10 · P1

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** The pasture template exposes Next animal, four separate cap increment/decrement buttons and Toggle first food rule.

**Keep:** There is already an animal list and explicit cap controls.

**Fix:** Make the target directly selectable, replace cap button clusters with labeled male/female numeric spinners, and provide direct per-food-rule editing. Retain existing actions as underlying commands, not the main user workflow.

**Acceptance:** The affected animal or food rule is always named before mutation. Users can edit any rule without cycling through unrelated records.

#### AGR-08 — Designation settings and locate · 7/10 · P2

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Name/priority use Apply name and priority, suspension is separate, and a Locate command appears in the rail.

**Keep:** The staged action label is explicit.

**Fix:** Use the common object toolbar and staged-versus-instant contract. Make Locate wording match Center on map elsewhere.

**Acceptance:** Name/priority drafts survive navigation as documented, and locate/suspend behavior is consistent with stockpiles and workshops.

### Population and work

#### POP-01 — Citizen roster, search, sort and paging · 6.5/10 · P2

**Source:** `windows/population_manager.rml`.

**Observed in source:** The template contains two Refresh controls in different containers, separate Name/Profession sort commands, a keyboard hint and row paging.

**Keep:** Search, keyboard guidance and pagination already exist.

**Fix:** Verify whether both refresh controls are visible at once, then retain one consistent refresh location. Use sortable report headers and one paging/status area; preserve existing keyboard behavior.

**Acceptance:** Refresh/filter/page changes preserve or deliberately relocate selection without duplicate commands, unexpected focus loss or hidden rows.

#### POP-02 — Skill comparison and bulk enable/disable · 6.5/10 · P1

**Source:** `windows/population_manager.rml`.

**Observed in source:** The selected skill can be enabled or disabled for all citizens.

**Keep:** Global and per-person editing are separate surfaces.

**Fix:** Keep scope visible beside the action, show the affected count and provide an appropriate recovery/confirmation strategy for broad changes. Use consistent checkbox state in the citizen list.

**Acceptance:** Only the selected skill changes across the stated population scope; a rejected bulk update cannot leave the presentation falsely successful.

#### POP-03 — Profession editor · 7/10 · P1

**Source:** `windows/population_manager.rml`.

**Observed in source:** The editor already has Profession skills and Available skills lists, priority movement, Add/Remove, Save and Delete.

**Keep:** The two-list ordering model fits the task well.

**Fix:** Retain the dual lists, position transfer controls next to their targets, and show dirty state with Revert/Cancel semantics. Keep Delete away from routine save/reorder actions.

**Acceptance:** Adding, removing and reordering affect the selected profession; switching profession with unsaved edits follows an explicit contract.

#### POP-04 — 24-hour schedule grid · 6.5/10 · P1

**Source:** `windows/population_manager.rml; windows/management6b.rcss`.

**Observed in source:** The schedule supports activity selection and explicit Set cell, Set citizen's day, and Set hour for all operations.

**Keep:** Those scope labels are valuable and should not be removed.

**Fix:** Give each scope a visible highlight preview. Use a single-select activity palette, stable row/hour headers, non-color activity symbols and a clear multi-target confirmation/recovery model.

**Acceptance:** Arrow navigation and keyboard editing reach all 24 hours. Applying a row, column or cell changes precisely the highlighted scope, including offscreen data.

#### POP-05 — Citizen detail overlap · 6.5/10 · P2

**Source:** `windows/population_manager.rml; screens/inspector.rml`.

**Observed in source:** Population contains a long citizen detail section, while the inspector has a newer multi-view creature preview.

**Keep:** Citizen identity, profession, needs, skills and equipment are available.

**Fix:** Choose a canonical detail presentation or share its components. Keep an explicit Back to citizens path with roster-state restoration, without maintaining divergent copies of the same property editor.

**Acceptance:** Changes made in either supported entry point update the same citizen and expose the same available actions and constraints.

### Military

#### MIL-01 — Military page navigation · 5/10 · P1

**Source:** `windows/military_manager.rml`.

**Observed in source:** Three visible peer views use the shared vertical rail, with hidden cross-route diplomacy buttons also present.

**Keep:** Squads, roles/uniforms and priorities are sensible peer topics.

**Fix:** Use a top tab strip. Keep hidden cross-route implementation details out of navigation and the focus order. Preserve current route and controller identifiers where needed.

**Acceptance:** Only supported military pages are reachable; the active page remains obvious during keyboard navigation and narrow-window adaptation.

#### MIL-02 — Squad list, identity and ordering · 6/10 · P2

**Source:** `windows/military_manager.rml`.

**Observed in source:** Squad controls include Add, Rename, Delete, Earlier squad and Later squad.

**Keep:** Squad identity and ordering are directly editable.

**Fix:** Keep a compact squad list with a selected-name heading. Use Move up/down with an explanation of what order means, and isolate Delete from routine selection and ordering.

**Acceptance:** Reordering cannot rename or delete another squad; boundary actions disable with a clear reason.

#### MIL-03 — Roster, unassigned citizens and transfers · 5.5/10 · P1

**Source:** `windows/military_manager.rml`.

**Observed in source:** A member/role action editor precedes the two roster columns and offers assign/remove and previous/next-squad transfer commands.

**Keep:** A dual roster/unassigned layout is already present.

**Fix:** Arrange the page as select squad, select citizen, inspect role, then act. Place transfer commands adjacent to the lists and consider an explicit destination selector instead of previous/next as the primary path.

**Acceptance:** Every transfer shows the citizen and source/destination. Moving selection alone cannot transfer membership. Preserve all supported underlying commands.

#### MIL-04 — Roles and civilian behavior · 6.5/10 · P2

**Source:** `windows/military_manager.rml`.

**Observed in source:** Role name editing and a Civilian button with retreat help are present.

**Keep:** The consequence of civilian behavior is explained.

**Fix:** Use a labeled checkbox for persistent civilian state. Keep role identity and the help adjacent, and make rename/delete behavior match squad and profession editors.

**Acceptance:** Civilian state is visibly on/off rather than appearing as a momentary action; affected citizens update through the authoritative role contract.

#### MIL-05 — Uniform slot/type/material editor · 7/10 · P2

**Source:** `windows/military_manager.rml`.

**Observed in source:** The interface separates uniform rows from the selected slot's equipment and material choices.

**Keep:** Slot, equipment and material are clearly distinguished.

**Fix:** Use one readable report table and one labeled selected-slot editor. Keep legal choices catalog-backed and show whether rules affect an individual or all users of a role.

**Acceptance:** Changing a slot applies to the named scope only; unavailable types/materials never appear as selectable fabricated options.

#### MIL-06 — Target priorities and attitudes · 6/10 · P1

**Source:** `windows/military_manager.rml`.

**Observed in source:** Move up/down and Flee/Defend/Attack/Hunt appear together as ordinary buttons above the priorities list.

**Keep:** Both order and response are editable.

**Fix:** Separate ordering commands from a labeled exclusive response choice. Keep the currently selected target and its current response visible.

**Acceptance:** Moving a target does not change its response, and choosing a response cannot be mistaken for an immediate attack command.

#### MIL-07 — Military confirmations and feedback · 7/10 · P1

**Source:** `windows/military_manager.rml`.

**Observed in source:** Delete commands have ellipses and a separate removal dialog; loading, empty and retry panels exist.

**Keep:** Potentially destructive actions are not presented as silent direct edits.

**Fix:** Adopt the shared modal contract and name the exact squad/role and consequence. Keep rejection/pending status next to the initiating action.

**Acceptance:** Cancel, Enter, Escape, stale selections and removed records all leave the game and selection in a consistent state.

### Diplomacy and missions

#### DIP-01 — Neighbors / Missions navigation · 6/10 · P2

**Source:** `windows/diplomacy_missions.rml`.

**Observed in source:** Two peer pages use the same wide rail structure as military.

**Keep:** The distinction between planning and activity is useful.

**Fix:** Use two top tabs and preserve list selection independently in each page.

**Acceptance:** Switching to mission activity and back returns to the intended neighbor and draft without unnecessary reselection.

#### DIP-02 — Neighbor list and properties · 8/10 · P2

**Source:** `windows/diplomacy_missions.rml`.

**Observed in source:** Distance, attitude, wealth, economy and military are separate labeled values; undiscovered neighbors have a specific explanatory state.

**Keep:** Unknown information is not presented as fabricated detail.

**Fix:** Keep a compact property list and the undiscovered-state explanation. Use consistent units and missing-value presentation.

**Acceptance:** Newly discovered or removed neighbors refresh without losing context or exposing stale editable actions.

#### DIP-03 — New mission builder · 6/10 · P1

**Source:** `windows/diplomacy_missions.rml`.

**Observed in source:** Mission type, action, eligible citizen list and Start mission are visible in one form, with many similarly styled choice buttons.

**Keep:** The necessary choices are brought together.

**Fix:** Arrange Destination → Mission type → Action → Citizen → Review/Start. Use exclusive choice controls and make unavailable prerequisites visible beside Start. Do not invent cost, risk or ETA data.

**Acceptance:** A valid mission starts once for the reviewed target/citizen. Invalid, unsupported and stale combinations are blocked with a specific reason.

#### DIP-04 — Mission details and results · 8/10 · P2

**Source:** `windows/diplomacy_missions.rml`.

**Observed in source:** Action, status, destination, participants, time and reported result have distinct fields.

**Keep:** The report structure answers the main status questions.

**Fix:** Keep these fields; add consistent empty/unknown and completed/failed styling with text rather than badges alone.

**Acceptance:** Only authoritative timing and results are shown; a completed or missing mission cannot be confused with a currently running one.

### Inspectors and creature equipment

#### INS-01 — Tile inspector and context actions · 8/10 · P2

**Source:** `screens/inspector.rml`.

**Observed in source:** The inspector provides coordinates, Center on map, refresh, content/job details and context commands.

**Keep:** This is strongly object-centered.

**Fix:** Keep the selected object identity stable and make the target of every command explicit. Prefer direct list selection over Inspect first creature as the primary multi-creature workflow.

**Acceptance:** Inspecting a crowded tile exposes each supported object without acting on a different first item after a refresh.

#### INS-02 — Construction/blueprint inspector · 8.5/10 · P2

**Source:** `screens/inspector.rml`.

**Observed in source:** Missing resources, material/status/needed columns, worker, priority, skill/tool and job actions are grouped together.

**Keep:** This directly explains why work is blocked and how to respond.

**Fix:** Preserve this structure as a reference pattern for other managers. Standardize numeric alignment, compact group boxes and placement of the destructive cancel action.

**Acceptance:** The displayed blocker matches the current job; updating resources or priority updates the explanation without fabricated progress.

#### INS-03 — Creature preview navigation and camera · 7/10 · P2

**Source:** `screens/inspector.rml`.

**Observed in source:** Camera, Stats, Expertise, Equipment and Inventory have explicit tab/panel relationships and roving tab-index values in markup.

**Keep:** This is among the strongest semantic tab implementations in the reviewed templates.

**Fix:** Preserve the associations and use the shared visual tab/navigation contract. Keep the creature name and relevant live state visible on all pages; do not let the camera dominate property editing.

**Acceptance:** Each view is reachable by the chosen keyboard model; changing views cannot change the selected creature or steal focus into the game.

#### INS-04 — Attributes and needs · 8/10 · P2

**Source:** `screens/inspector.rml`.

**Observed in source:** Attributes are labeled numeric values; needs have both meters and text values.

**Keep:** Text accompanies visual meters, avoiding color-only interpretation.

**Fix:** Use a compact aligned property layout, explain direction/units where necessary, and ensure missing data is distinct from zero.

**Acceptance:** Meters and displayed numbers agree; unusual/empty values render safely and remain understandable in high contrast.

#### INS-05 — Expertise and profession selector · 7/10 · P2

**Source:** `screens/inspector.rml`.

**Observed in source:** A custom profession drop-down and a scrollable skills area are defined, with specific empty states.

**Keep:** The user can change profession close to the affected skills.

**Fix:** Share the combo implementation, align skill name/level/active columns, and clarify which changes are profession-wide versus individual overrides.

**Acceptance:** Choosing or canceling a profession preserves focus correctly; the skill list reflects the authoritative result rather than optimistic stale values.

#### INS-06 — Equipment paper doll and slot editor · 7/10 · P1

**Source:** `screens/inspector.rml`.

**Observed in source:** The paper doll has clickable equipment slots and an Apply/Cancel editor. It also includes decorative N/O placeholders and a scope text element.

**Keep:** Slot-based editing is appropriate for the game and need not imitate a generic file dialog.

**Fix:** Keep the doll, label each interactive slot, visually distinguish decorative positions, and state the individual-versus-role scope before Apply. Show actual equipment separately from requested rules when backed.

**Acceptance:** Users can select each real slot by keyboard, cannot activate decorative slots, and never unintentionally edit every citizen sharing a role.

#### INS-07 — Carried inventory and empty states · 7/10 · P2

**Source:** `screens/inspector.rml`.

**Observed in source:** A separate carried-items view and No carried items state exist.

**Keep:** Worn equipment and carried inventory are not conflated.

**Fix:** Use the shared report/list style with quantities and material details where available; keep the empty state quiet and accurate.

**Acceptance:** Transitions from no items to items and back maintain consistent selection and do not leave stale rows.

#### INS-08 — Stockpile/workshop/agriculture summaries · 7/10 · P2

**Source:** `screens/inspector.rml`.

**Observed in source:** Object-specific inspector summaries overlap with detailed management windows.

**Keep:** Quick inspection can avoid opening a larger editor.

**Fix:** Keep inspectors read-mostly and compact; link to the full manager for complex edits. Share labels and state projection to prevent conflicting representations.

**Acceptance:** An action from either the inspector or manager updates every open representation of the same object.

### Application shell and settings

#### APP-01 — Main menu · 7/10 · P2

**Source:** `screens/main_menu.rml; screens/shell.rcss`.

**Observed in source:** The menu separates Continue/Load from tutorial/quick/custom start, with a reason when no compatible save is available.

**Keep:** The task hierarchy is clear and game branding is appropriate.

**Fix:** Retain the hierarchy and brand; reduce redundant framing and normalize button states. Exit should not dominate the normal start/continue flow.

**Acceptance:** With no save, compatible save and incompatible save, the correct commands and explanations appear and keyboard order follows the task hierarchy.

#### APP-02 — Setup navigation model · 6/10 · P2

**Source:** `screens/new_game.rml`.

**Observed in source:** The UI labels numbered Setup steps and has a review page, but the footer exposes Start kingdom rather than a standard Back/Next/Finish progression.

**Keep:** Draft retention and review are explicitly described.

**Fix:** Choose a coherent model: a wizard with Back/Next/Finish, or freely selectable property pages without sequential promises. Preserve draft state and the actual supported generation parameters.

**Acceptance:** Users know how to reach the next section and review the final draft. Navigation does not reset values or bypass mandatory validation.

#### APP-03 — World-generation fields and layout · 6/10 · P2

**Source:** `screens/new_game.rml; screens/shell.rcss`.

**Observed in source:** Many exact parameters use sliders inside small bordered cards, with some 9–10dp explanatory text.

**Keep:** Parameter ranges and implications are described.

**Fix:** Use a smaller number of group boxes, aligned numeric editors and concise help. Keep interdependent bounds and validation beside the affected fields.

**Acceptance:** Every supported parameter can be set exactly, invalid combinations identify the affected fields and the final summary matches the submitted draft.

#### APP-04 — Kingdom/save browser · 7.5/10 · P2

**Source:** `screens/load_game.rml`.

**Observed in source:** Kingdoms and saves appear in a two-pane layout with meaningful empty states, refresh and Load selected save.

**Keep:** This is a strong master-detail structure.

**Fix:** Keep it; ensure each save exposes identifying metadata and compatibility/rejection reasons actually available from the backend. Use a clear default Load action and safe selection behavior.

**Acceptance:** Refreshing, changing kingdoms and trying a failed load preserve recoverable state and do not load an unintended save.

#### APP-05 — Settings shell and grouping · 6/10 · P2

**Source:** `screens/settings.rml; screens/shell.rcss`.

**Observed in source:** Settings has a 65dp title area and multiple inset cards with blue heading bars; the page states that changes save automatically.

**Keep:** The immediate-apply model is honestly stated.

**Fix:** Use a compact property-sheet layout with Display, Controls, Audio and Saving pages or restrained group boxes. Keep immediate apply with Close, or deliberately implement true staged Apply/Cancel; do not add a fake Cancel.

**Acceptance:** Every setting follows the declared model. Closing and reopening does not surprise the user about which values were saved.

#### APP-06 — Display, controls, audio and saving controls · 7/10 · P2

**Source:** `screens/settings.rml`.

**Observed in source:** The template includes fullscreen, refresh matching, frame-rate limit, UI scale, minimum light, camera speed, wheel behavior, master volume and autosave controls.

**Keep:** The available preferences are concrete, not ornamental.

**Fix:** Keep enabled/disabled dependencies explicit, add exact numeric editing where useful, show units and make Reset defaults disclose its scope. Verify an escape path from an unusable scale/display choice.

**Acceptance:** All controls persist through the backing settings contract, restore defaults only for the disclosed scope and remain usable at supported scale extremes.

#### APP-07 — Pause/save/load/menu transitions · 7.5/10 · P1

**Source:** `screens/pause_menu.rml`.

**Observed in source:** The pause screen has an authoritative pause-state field, save feedback, errors and explicit resume/save/load/settings/menu commands.

**Keep:** It does not assume requested pause is already authoritative.

**Fix:** Preserve that distinction. Apply a consistent transition confirmation strategy for unsaved progress and ensure no repeated activation triggers multiple saves or loads.

**Acceptance:** Save success/failure, canceled load and return-to-menu paths preserve the intended pause state and cannot click through into the resumed world.

#### APP-08 — Loading, error and retry screen · 6/10 · P2

**Source:** `screens/loading.rml`.

**Observed in source:** Loading uses descriptive text and three marks, with separate Back/Retry when an error occurs.

**Keep:** Failure recovery is explicitly represented.

**Fix:** Adopt the shared truthful-progress control, focus the error when it appears and retain useful diagnostic text without raw internal jargon.

**Acceptance:** A slow load, failed load and successful retry each leave one clear state with working controls and no orphaned blocker.

#### APP-09 — Generic destructive confirmation · 6/10 · P1

**Source:** `modals/confirm_destructive.rml`.

**Observed in source:** The template defaults to Confirm action and Confirm labels, with a warning about discarding unsaved progress.

**Keep:** A safe Cancel action is present and emphasized.

**Fix:** Require action-specific title/body/button text from the caller and use the shared modal focus/default contract. Do not let a generic fallback be the only explanation of an irreversible action.

**Acceptance:** The dialog states exactly what will be discarded or changed and does not accidentally accept on the key release that opened it.

## Recommended implementation order

### Pass 1 — Establish a reliable baseline

Pin the revision; capture production screens and control states in the actual application. Record host size, UI scale, input method and data provenance. Include empty, populated, selected, focused, disabled, pending, failed and destructive-confirmation states. Keep deterministic fixture shots for component consistency, but do not use them as proof of gameplay correctness.

Repair the verifier's contradiction before using it to judge new work. Confirm the pinned RmlUi version and supported syntax. Retain license/asset checks. Make a shared control gallery that includes real generated scrollbar/select child elements, not hand-drawn stand-ins.

### Pass 2 — Fix primitives once

Consolidate tokens and the cascade. Implement window frame, title bar, button/default/focus states, tabs, group boxes, input/check/radio/combo/numeric controls, report headers, selection, scrollbars, tooltips and modal shell. Prefer the best existing implementation rather than replacing every component from scratch. Preserve separate comfortable and compact density options when justified.

Proposed compact dimensions should be validated rather than treated as archival facts: approximately 24–30dp command and report-row heights, 12–13dp essential text, 16–24dp inline item/icon sizes, and 16–18dp scrollbars are reasonable starting ranges. Retain larger thumbnails or hit areas where gameplay requires them. Use whole-device-pixel edge placement where the renderer supports it, without disabling useful scaling.

### Pass 3 — Migrate the strongest patterns first

Use the inventory report, construction inspector, stockpile hauling checkboxes and profession dual lists as reference patterns. Apply the common components to inventory and stockpile first, then workshop and agriculture. Refactor repeated filter header/combo markup into shared components without changing filter semantics.

### Pass 4 — Repair complex workflows

Military: tab → squad → citizen → role/destination → command. Diplomacy: destination → mission type/action → citizen → review/start. Population: preserve the scope of bulk skill and schedule operations. Agriculture: distinguish selected-plot assignment, per-plot queue and farm-default changes. Equipment: reveal individual-versus-role scope before mutation. Trade: direct row/quantity editing and an exact pre-commit exchange review.

### Pass 5 — Align the application shell

Keep the main menu's clear task hierarchy. Choose a coherent wizard or property-page model for new-game setup. Convert settings to a restrained property-sheet layout while retaining the stated edit model. Keep pause/loading feedback authoritative and protect save/load/menu transitions.

### Pass 6 — Verify actual behavior

Only mark work complete after the production runtime meets the matrix below. A successful build or passing textual fixture verifier is necessary evidence at most, not proof of usable GUI parity.

## Production acceptance matrix

| Test family | Required cases | Pass condition |
|---|---|---|
| Scale and host size | Every supported host at 100%, 125%, 150%, 200%; minimum supported dimensions; long names | Every essential control and value remains reachable and readable. |
| Keyboard | Tab/Shift+Tab, arrows, Space, Enter, Escape; supported page/list accelerators | Focus is visible, logical and distinct from values; no command fires on focus alone. |
| World input isolation | Click, double-click, wheel, drag and key events while pointer/focus are in UI | No unintended map order, layer change or world drag leaks through. |
| Modal behavior | Open, cancel, accept, key repeat, nested event arrival, stale target | Exactly one deliberate action; no focus escape or residual blocker. |
| Data lifecycle | Empty, unavailable, zero, unknown, loading, populated, updating, rejected | Correct distinctions and useful recovery; no fabricated data. |
| Selection stability | Sort, filter, page, refresh, delete, reorder, switch tabs, open details, Back | Selection and scroll are preserved or intentionally relocated with clear feedback. |
| Edit semantics | Draft change, Apply, immediate change, Cancel/Revert, close, reopen | Behavior matches the declared edit contract. |
| Scope | Cell/row/column schedule changes, all-citizen skills, matching rules, plot queue, role equipment | The displayed affected scope equals the backend mutation scope. |
| Generated controls | Drop-down open/close, nested popup, both scrollbar axes, track, thumb, arrows | Correct actual RmlUi elements are styled and fully operable. |
| Accessibility variants | High contrast and reduced motion where truly supported; keyboard-only | Route-specific styles do not defeat the preference or hide focus. |
| Regression | Existing save, new save, each workshop/designation/creature kind, supported mission types | No lost commands, fabricated choices or persistence regression. |

## Reference and evidence notes

Repository source paths and identifiers in each item refer to the pinned revision above. The accompanying chat report provides source citations. The principal historical reference is Microsoft's 1995 Windows Interface Guidelines draft, hosted by the University of California, Irvine. RmlUi's official *Element scrollbars*, *Style guide for the core elements* and *Media queries* documentation establish the generated child-element names and context-based media features. Check these against the project's pinned dependency before changing syntax.

This worklist intentionally avoids claiming that static ARIA attributes prove screen-reader integration, that CSS proves runtime resizing works, that a screenshot proves command correctness, or that a field not present in a template is absent from dynamic projection. Those questions belong in the acceptance tests.
