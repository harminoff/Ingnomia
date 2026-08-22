# Competitive UI/UX research for Ingnomia

**Research date:** 2026-08-12  
**Scope:** management-game interface patterns relevant to Ingnomia's RmlUi rebuild  
**Primary archives:** [Game UI Database](https://www.gameuidatabase.com/) and [Interface In Game](https://interfaceingame.com/)  
**Artifact policy:** no third-party screenshot, icon, texture, layout, or other copyrighted asset has been copied into the repository.

## Executive conclusion

The strongest common pattern is a **map-first operating shell**: a thin settlement pulse around the perimeter, an explicit active-tool state, and a contextual panel that opens only when the player asks for detail. Dense colony work moves into a dedicated management surface with strong tabs, filters, and row identity; it should not permanently crowd the map.

Ingnomia should combine:

- Against the Storm's legible separation between persistent status, selected-object detail, and full management work;
- Frostpunk's severity hierarchy and time/temperature prominence;
- Prison Architect's direct overlay-driven construction and deployment workflows;
- Anno 1800's sparse peripheral HUD and contextual world tooltips;
- Cities: Skylines' explicit tool/overlay modes and selected-area feedback; and
- Northgard's compact resource rail, pinned objective, and in-context build confirmation.

It should not reproduce any one composition. Ingnomia has a distinctive requirement that these archives do not solve: **vertical underground navigation**. Current z-level, layer transition controls, and cross-level consequences must be first-class rather than buried in a generic camera menu.

## Method and evidence boundaries

This is a screenshot-archive study, not a claim about every current release or every interaction in each game.

- A finding is recorded only when a named screen, category, or visible interaction was present in one of the two requested archives.
- Archive category labels are useful evidence of screen coverage, but they do not prove keyboard behavior, response time, or the complete information model.
- Visual observations are paraphrased. No screenshot was downloaded into or committed to the project.
- Game UI Database pages were inspected in the public browser because its text endpoint blocks automated crawling.
- Interface In Game page and screen URLs are linked directly wherever available.
- Platform differences matter. The Game UI Database Prison Architect entry is the console edition, so its radial input vocabulary is treated as evidence for tool-state clarity, not as a PC layout to copy.

### Availability audit

The brief requested at least six close comparisons subject to archive availability. Three named close peers had dedicated, directly inspectable archive pages: Against the Storm, Prison Architect, and Frostpunk. Dedicated game pages were not located during this pass for Dwarf Fortress, RimWorld, Oxygen Not Included, Gnomoria, Songs of Syx, Timberborn, Going Medieval, Factorio, Clanfolk, Norland, or KeeperRL. Searches that only matched incidental image text were not counted as game coverage.

To retain six evidence-backed comparisons, this study adds three adjacent management games that are actually represented: Anno 1800, Cities: Skylines, and Northgard. They are clearly marked as adjacent fallbacks in the matrix.

| Comparable | In requested group | Game UI Database | Interface In Game | Evidence use |
|---|---:|---|---|---|
| Against the Storm | Yes | No dedicated game page located | [Dedicated page](https://interfaceingame.com/games/against-the-storm/) | Close colony-management reference |
| Frostpunk | Yes | [PC page](https://www.gameuidatabase.com/gameData.php?id=38) | [Dedicated page](https://interfaceingame.com/games/frostpunk/) | Close survival-city reference |
| Prison Architect | Yes | [Console page](https://www.gameuidatabase.com/gameData.php?id=603) | [Dedicated page](https://interfaceingame.com/games/prison-architect/) | Close designation/overlay reference |
| Anno 1800 | Adjacent fallback | [Dedicated page](https://www.gameuidatabase.com/gameData.php?id=1118) | [Dedicated page](https://interfaceingame.com/games/anno-1800/) | Settlement summary and diplomacy reference |
| Cities: Skylines (PC) | Adjacent fallback | [Dedicated page](https://www.gameuidatabase.com/gameData.php?id=526) | Not used | Overlay, placement, and map-preservation reference |
| Northgard | Adjacent fallback | [Dedicated page](https://www.gameuidatabase.com/gameData.php?id=566) | Not used | Compact RTS/settlement reference |

## Research matrix

The matrix uses the required decision vocabulary:

- **Adopt:** the behavior can transfer with little conceptual change.
- **Adapt:** preserve the underlying UX principle, redesigned for Ingnomia's data and controls.
- **Reject:** avoid the observed tradeoff or do not transfer a platform-specific pattern.

| Game | Screen/category examined | Source URL | Observation | Strength | Weakness | Relevance to Ingnomia | Decision | Rationale |
|---|---|---|---|---|---|---|---|---|
| Against the Storm | Global HUD and selected gathering node | [HUD](https://interfaceingame.com/screenshots/against-the-storm-hud/) | Resource/time status is concentrated at the top, species portraits stack on the left, primary tools sit on a bottom rail, and selection opens a bounded right inspector while the world remains visible. | Clear separation of settlement pulse, people, tools, and context. | Many small emblem-like icons demand familiarity; portrait stack consumes vertical space as population types grow. | Strong model for Ingnomia's map shell and tile/workshop inspector. | Adapt | Keep the perimeter zones and right inspector, but use plainer icon-label pairs, configurable summary chips, and scalable lists. |
| Against the Storm | Consumption policy table | [Consumption](https://interfaceingame.com/screenshots/against-the-storm-consumption/) | A full management panel uses top-level tabs, resource columns, species rows, checkboxes, and explicit current/max impact values. | Supports batch policy decisions and cross-group comparison in one view. | Dense icon matrix is difficult to scan without learned icon meanings; the panel covers most of the playfield. | Relevant to stockpile permissions, workshop inputs, professions, uniforms, and population rules. | Adapt | Use a dedicated management workbench with sticky text headers, filters, row summaries, and an optional compact mode rather than an icon-only matrix. |
| Against the Storm | Newcomers and population choice | [Newcomers](https://interfaceingame.com/screenshots/against-the-storm-newcomers/) | Population intake is framed as a bounded choice rather than buried in a generic population table. | Turns a consequential state change into a legible decision moment. | Modal presentation can interrupt routine play if overused. | Useful for migration waves, diplomats, military events, and emergency choices. | Adapt | Reserve choice modals for consequential, non-routine decisions; ordinary arrivals belong in the event stream and population panel. |
| Against the Storm | Key mapping and settings coverage | [Key mapping](https://interfaceingame.com/screenshots/against-the-storm-key-mapping/), [game page](https://interfaceingame.com/games/against-the-storm/) | The archive exposes dedicated key-mapping plus alert, gameplay, and subtitle settings. | Treats controls and notification behavior as user-configurable systems. | Archive evidence does not establish conflict detection or full keyboard navigation. | Ingnomia has many build/designation and camera commands that need discoverability. | Adopt | Provide searchable bindings, visible shortcut hints, reset-by-section, conflict feedback, and alert-channel preferences. |
| Frostpunk | Global HUD, clock, temperature, objectives, severity | [Game UI Database PC page](https://www.gameuidatabase.com/gameData.php?id=38), [Interface In Game page](https://interfaceingame.com/games/frostpunk/) | Time and temperature dominate the top center; resources form a narrow top rail; Hope/Discontent anchor the bottom; objectives stack at left; timed events appear at right. | Exceptional at conveying threat, time pressure, and settlement condition without a permanent dashboard window. | Ornamental framing and multiple simultaneous edge stacks can become visually loud. | Ingnomia needs readable time, season, population condition, warnings, and alerts around a large map. | Adapt | Give time, season, z-level, and critical warnings stable high-salience positions, but reduce ornament and keep one coherent alert queue. |
| Frostpunk | Construction and placement | [Construct resource](https://interfaceingame.com/screenshots/frostpunk-construct-resource/), [Game UI Database PC page](https://www.gameuidatabase.com/gameData.php?id=38) | Construction uses a categorized tool surface while the placement preview and world remain visible. | Maintains spatial reasoning during build selection. | Category depth can require repeated movement between menu and map; archive does not show a search-first workflow. | Directly relevant to workshops, furniture, walls, floors, stairs, agriculture, and designations. | Adapt | Use category breadcrumbs, recent/favorite items, search, hotkeys, explicit cost/availability, placement validity, and a persistent cancel affordance. |
| Frostpunk | Object stats and economy overlays | [Coal mine](https://interfaceingame.com/screenshots/frostpunk-coal-mine/), [game page](https://interfaceingame.com/games/frostpunk/) | Building detail is tied to an in-world object and its production state; the archive also exposes economy, heating, stats, and inventory categories. | Makes causes and bottlenecks spatially inspectable. | Strong full-screen overlays can hide local context; warning colors may compete. | Ingnomia needs to explain why a workshop is stalled, what a stockpile contains, and which job or input blocks production. | Adopt | Every production surface should answer state, cause, next action, inputs, outputs, workers, queue, and linked storage without requiring a wiki. |
| Frostpunk | Tutorial overlay and progression | [Construction tutorial](https://interfaceingame.com/screenshots/frostpunk-construction/), [game page](https://interfaceingame.com/games/frostpunk/) | Tutorials are attached to a concrete screen/task, while progress unlocks and laws use dedicated moments. | Contextual teaching aligns instruction with the player's immediate goal. | Tutorials and unlock ceremonies can interrupt experts or repeat known concepts. | Useful for Ingnomia's deep designation, workshop, stockpile, military, and verticality systems. | Adopt | Add dismissible, non-blocking task coaching with “show me” focus and a searchable help/history record; never force repeated tutorials. |
| Prison Architect | Desktop HUD and patrol overlay | [Patrols](https://interfaceingame.com/screenshots/prison-architect-patrols/), [game page](https://interfaceingame.com/games/prison-architect/) | A slim top status strip, left task list, right clock/speed control, bottom category rail, and bottom-left tool palette surround an overlay-marked world. | Overlay mode communicates where a rule applies while preserving the facility plan. | Bottom category rail plus open palette can become crowded; small icons and low contrast are vulnerable at high UI scale. | Highly relevant to zones, stockpiles, rooms, patrols, farms, groves, and designation overlays. | Adapt | Use explicit named overlay modes, a compact category rail, a resizable palette, text labels at first use, and a one-action return to Inspect mode. |
| Prison Architect | Placement and contextual job state | [Game UI Database console page](https://www.gameuidatabase.com/gameData.php?id=603) | Placement highlights the target footprint, labels the pending job next to the target, and keeps population, time, and money visible. | Immediate spatial and status feedback reduces ambiguity after placement. | Console radial/cross menus are inefficient for a PC mouse-and-keyboard game and should not be copied. | Ingnomia needs clear valid/invalid tiles, job creation feedback, priority, repetition, and cancellation. | Adapt | Keep footprint, validity reason, material cost, created-job confirmation, and priority feedback; reject the console radial interaction in favor of keyboard-aware PC controls. |
| Prison Architect | Tutorial and settings inventory | [Game page](https://interfaceingame.com/games/prison-architect/) | The archive includes four tutorial screens, three settings screens, and targeted examples for rooms, utilities, construction, and patrols. | Teaching is attached to specific systems rather than one monolithic manual. | The archive does not demonstrate unified search, full accessibility, or advanced management tables. | Ingnomia's onboarding should follow the same system-by-system structure. | Adapt | Teach the first successful loop for each system, then expose advanced options progressively with a permanent help entry. |
| Anno 1800 | Global HUD and regional map | [Game UI Database page](https://www.gameuidatabase.com/gameData.php?id=1118), [Interface In Game page](https://interfaceingame.com/games/anno-1800/) | Settlement counters sit in a compact top-left group; connectivity, daylight, pause/speed, and settings sit top-right; a narrow left rail provides secondary tools; context labels appear near world targets. | The central map remains extremely dominant and readable. | Sparse icons can conceal breadth; detached corners require eye travel on very wide screens. | Strong reference for a low-noise default Ingnomia HUD. | Adopt | Keep default chrome shallow and peripheral; reveal labels/tooltips quickly and support width-capped edge groups on ultrawide displays. |
| Anno 1800 | Overview, inventory, trade, diplomacy, notifications | [Game UI Database page](https://www.gameuidatabase.com/gameData.php?id=1118) | Archive coverage includes overview/stats, maintenance, inventory, trading, missions, area map, notifications, and a large relationship-oriented diplomacy surface. | Demonstrates distinct workspaces for economy, logistics, and external relations. | The radial diplomacy composition spends substantial area on presentation and is unsuitable for dense comparison. | Ingnomia needs neighbors, trade, diplomacy, inventory, reports, and settlement history. | Adapt | Use a data-rich diplomacy/neighbor table and relationship timeline; reserve illustrative presentation for headers and major events, not the comparison surface. |
| Cities: Skylines (PC) | Construction, selected footprint, overlays, time | [Game UI Database page](https://www.gameuidatabase.com/gameData.php?id=526) | A substantial bottom tool/status bar changes with the active system; placed/selected areas receive a clear outline; overlay modes recolor the world while time and city status remain visible. | Active mode and affected geometry are hard to miss. | The bottom bar consumes notable height and becomes icon-dense; color overlays can be inaccessible without redundant encoding. | Relevant to construction, mining, agriculture, rooms, stockpiles, pathing, and utility/debug views. | Adapt | Use a shallower tool shelf, explicit mode title, cancel/confirm state, pattern or outline redundancy, and a legend that explains overlay values. |
| Cities: Skylines (PC) | Statistics and maintenance workspaces | [Game UI Database page](https://www.gameuidatabase.com/gameData.php?id=526) | The archive includes repeated overview/stat and maintenance screens plus codex/news surfaces. | Acknowledges that large simulations need trend and diagnostic views beyond the live map. | Separating every system can fragment causal investigation and force back-and-forth navigation. | Ingnomia needs inventory trends, job pressure, workshops, population, military, and event history. | Adapt | Create a shared management shell with consistent filters, selection retention, related-links, and “locate on map” actions instead of unrelated window patterns. |
| Northgard | Compact HUD, placement, pinned objective, tutorial | [Game UI Database page](https://www.gameuidatabase.com/gameData.php?id=566) | Resources form one compact top rail; objective text stays at upper right; tutorial guidance appears top-center; placement feedback and confirm/cancel controls remain near the target; minimap sits bottom-left. | Very strong map preservation and immediate tool-state feedback. | A referenced capture uses gamepad prompts, and isolated icon counters do not scale to Ingnomia's larger resource catalog. | Useful for the shape of Ingnomia's moment-to-moment shell, not its full management depth. | Adapt | Keep the compact rail, pinned task, and local placement feedback; aggregate resources into expandable groups and substitute PC hotkey hints. |
| Northgard | Trade, codex, and rules coverage | [Game UI Database page](https://www.gameuidatabase.com/gameData.php?id=566) | Archive categories include trading, codex/journal, custom rules, button layouts, dialogue, and world-map selection. | Shows that even a compact shell can lead to bounded secondary workspaces. | Limited evidence for deep population, workshop, military, search, sorting, or history workflows. | Useful as a simplicity ceiling for the outer shell, not as the management architecture. | Adopt | Use Northgard-like restraint by default, then open Ingnomia-specific data workbenches only when requested. |

## Required-topic coverage ledger

“Not evidenced” means the archive sample did not establish the behavior; it is not a claim that the game lacks it.

| Evaluation topic | Against the Storm | Frostpunk | Prison Architect | Anno 1800 | Cities: Skylines | Northgard | Ingnomia implication |
|---|---|---|---|---|---|---|---|
| Global HUD and playfield protection | Four edge zones plus right inspector | Transparent edge stacks around city | Top status, bottom tools, small dock | Very sparse corners and left rail | Deep bottom tool/status shelf | Thin top rail, minimap, objective | Default to shallow edge chrome; context panels must dock, resize, or collapse. |
| Settlement summary and resources | Resource counters and species groups | Resources plus Hope/Discontent | Population/capacity/money counters | Compact economic counters | City finance, population, demand/status | Small production/resource rail | Show a curated “settlement pulse,” not every item; expand by category on demand. |
| Time, date, season, pause, speed | Clock and multi-speed controls | Time and temperature are dominant | Day/time plus large clock and speed | Daylight plus pause/speeds | Date, time, and speeds in bottom bar | Year/month state appears near minimap | Combine calendar, season, speed, and pause with current z-level in one stable world-control group. |
| Z-level or vertical layers | Not evidenced | Not evidenced | Not evidenced | Not evidenced | Not evidenced | Not evidenced | Treat verticality as an original Ingnomia requirement: current level, up/down, layer context, and off-level alerts must be persistent. |
| Build/designation taxonomy | Bottom categories and contextual recipes | Categorized construction | Bottom categories and tool palette | Left/tool workspaces | Bottom system categories | Build invoked after region selection | Use task language (Dig, Build, Zone, Farm, Remove), searchable subcategories, recent/favorite tools, and breadcrumbs. |
| Tool selection, cancellation, validity | Local target and right inspector | Placement overlay | Footprint plus pending-job text | Context labels near targets | Strong active-mode shelf and outlines | Local confirm/cancel compass | Always expose active mode, target footprint, validity reason, cost, confirm/repeat behavior, and Esc/right-click cancellation. |
| Selected-object inspection and contextual actions | Bounded right inspector | Spatial object stats | Hover/object detail and overlay markers | Contextual world tooltip | Selection outline and info surfaces | Local target controls | One authoritative inspector should explain state, cause, available action, and “locate related” links. |
| Stockpiles, production, workshops, inventory | Consumption/recipes are dense policy tables | Economy and building production | Utilities/rooms/object categories | Inventory, trade, maintenance | Maintenance/stat workspaces | Trade is present; deep production not evidenced | Use a shared management workbench with stable rows, queues, inputs/outputs, worker state, linked storage, search, sort, and filters. |
| Population, jobs, health, needs, morale | Species portraits, newcomers, consumption impacts | Hope/Discontent and health/shelter surfaces | Prisoner/guard/capacity counters and deployment | Population/economy summaries | Population/services/stat views | Small population cap; deeper needs not evidenced | Provide summary chips plus drill-down tables; prioritize exceptions and explain why a need or assignment is failing. |
| Military, diplomacy, neighbors | Not strongly evidenced in reviewed sample | External expedition/event contacts | Patrol/deployment overlay | Dedicated relationship/diplomacy surface | Service/area overlays; diplomacy not central | Objectives and combat context; management depth not evidenced | Use distinct Military and Neighbors workspaces, spatial overlays, schedules, relationship history, and map-locate actions. |
| Alerts, events, reports, history | Alerts plus side task/read-more stacks | High-severity edge events and objectives | To-do list plus warnings | Notifications and missions | News/codex/stat views | Pinned objective and tutorial toast | One severity-ranked event stream needs timestamp, location, subject, cause, acknowledgment, mute/filter, and history. |
| Search, filter, sort, table density | Dense policy matrix; search/sort not evidenced | Multiple management surfaces; search not evidenced | Palettes/categories; deep tables not evidenced | Multiple workspaces; search not evidenced | Repeated stat/maintenance panels | Limited table evidence | Ingnomia must add first-class search, column sorting, filters, saved views, sticky headers, row counts, and empty/loading/error states. |
| Tooltips and explanation | Context panel contains description and effects | Object stats and tutorials | Context hover and task guidance | Target tooltip | Mode legend/overlay context varies | Tutorial callout and local labels | Tooltips must name the thing, explain consequence and blockers, show hotkey, and link to deeper help without becoming the only label. |
| Tutorial, onboarding, hotkey discoverability | Hint/tutorial plus key mapping | Context tutorials and unlock moments | System-specific tutorials | Button-layout and mission coverage | Codex/news; hotkeys not established | Tutorial overlay and button layouts | Use contextual, dismissible coaching; visible shortcut badges; searchable command palette/help; expert suppression. |
| Settings and accessibility | Gameplay, alerts, subtitles, key mapping | Options, audio, display; archive lists six settings screens | Three settings screens in archive | Gameplay/display/audio/button layouts | Gameplay/options/audio | Options/button layouts | Require UI scale, text scale, rebindable controls, contrast-safe palettes, non-color severity, reduced motion, tooltip delay, and notification controls. |
| Modal versus docked | Full policy modal; right contextual dock | Many transparent overlays plus major full screens | Docked palette and world overlays | Sparse HUD plus full workspaces | Tool shelf plus full data screens | Mostly light HUD and compact overlays | Routine inspection stays docked/non-modal; modal only for destructive, blocking, or consequential decisions; management panels preserve selection. |
| Color, iconography, severity | Bronze frames, portraits, colored impacts | Red/yellow threat emphasis with strong ornament | Neutral shell plus bright mode/marker colors | Restrained gold/cream symbols | Bright system colors and overlay recoloring | Compact icons with blue tutorial emphasis | Use semantic tokens with icon + text + shape redundancy; theme through material and rhythm, never texture noise or color alone. |

## Cross-cutting findings

### 1. The map should remain the primary instrument

All six useful references keep the simulation visible during routine inspection and placement. Their best panels occupy an edge or a limited portion of the screen. The weakest moments are full-screen management layouts that still behave like oversized icon grids. Ingnomia should reserve full workspaces for intentional, data-heavy tasks and provide a one-action return to the exact map selection.

### 2. Persistent information should be a curated pulse

Resource rails work because they show a small, stable set of totals and trends. Ingnomia's item catalog is too large to expose flatly. Persistent chrome should show population, food/security pressure, critical materials, active alerts, time/season/speed, and current z-level. Inventory details belong behind expandable groups and searchable tables.

### 3. Tool state is a contract, not decoration

The active tool must be unmistakable from selection through cancellation:

1. player chooses a named task and subtype;
2. the HUD shows mode, hotkey, repeat setting, and cancel action;
3. the world shows footprint/brush and valid/invalid reason;
4. the click or drag previews cost and affected tiles;
5. confirmation creates a visible designation/job;
6. feedback names what changed and how to undo/cancel it; and
7. Inspect mode is always one action away.

### 4. Dense work requires text, stable rows, and comparison

Against the Storm demonstrates the power and risk of a compact policy matrix. For Ingnomia, icons may support recognition but cannot replace text headers, values, units, state, and cause. Tables need stable identity, sticky headers, searchable/filterable columns, row counts, bulk actions, selection retention, and an obvious “locate on map” bridge.

### 5. Severity must be semantic and configurable

Frostpunk makes critical conditions prominent, but Ingnomia should avoid permanent crisis theater. Severity should control color token, icon, sound, persistence, auto-pause behavior, and placement in the event stream. The same meaning must survive grayscale and common color-vision deficiencies.

### 6. Verticality is the major competitive gap

None of the reviewed archive samples offers a transferable z-level solution. Ingnomia must design one deliberately: persistent current level, explicit up/down actions, layer range and ground/underground identity, off-level selection cues, cross-level job/alert links, and predictable wheel/key behavior that never conflicts with scrolling a UI panel.

## Original Ingnomia design principles

These principles synthesize the evidence but are original to Ingnomia. They are constraints for the later UX specification and design system, not instructions to copy any reference.

1. **The colony is the canvas.** Routine chrome should frame the world, not sit on top of its center. The map remains dominant at common 16:9, 16:10, ultrawide, and compact window sizes.
2. **One settlement pulse, many drill-downs.** Persistent HUD values are curated summaries with trend/severity; every summary opens a relevant filtered detail view.
3. **Verticality is always legible.** Show current z-level and layer character beside time/camera controls. Up/down navigation, off-level alerts, and cross-level links must never be implicit.
4. **Inspect is the safe home mode.** Build, designate, zone, overlay, and target modes are explicit temporary states with visible exit, repeat, and cancellation behavior.
5. **Context docks; decisions interrupt.** Tile, creature, workshop, stockpile, and object information uses a docked inspector. Only destructive, blocking, or genuinely consequential choices use modal focus.
6. **Every stalled system explains itself.** A workshop, job, stockpile, farm, squad, or need must show current state, cause, dependencies, and the most likely corrective action.
7. **Management views are workbenches.** Use a shared shell with search, filters, sorting, sticky headers, bulk actions, stable row selection, saved view state, and “locate on map.”
8. **Exceptions rise; routine recedes.** Default tables and summaries prioritize shortages, stalled queues, unsafe conditions, unassigned work, unmet needs, and recent changes while retaining access to the complete data.
9. **Color never carries meaning alone.** Pair semantic color with icon, text, position, and shape. Respect contrast and provide color-vision-safe overlay legends.
10. **Hotkeys teach themselves.** Menus show shortcut badges, tooltips show alternates/modifiers, remapping detects conflicts, and all major workflows remain keyboard reachable.
11. **Feedback closes every loop.** Actions produce visible state change, a concise message, affected count/location, and undo/cancel guidance where the simulation permits it.
12. **Theme comes from structure.** Dwarven character should come from carved proportions, restrained stone/metal/wood materials, engraved separators, warm highlights, and measured motion—not textured noise, tiny ornament, or rune-like body text.
13. **Scale changes composition, not just pixels.** Edge groups wrap or collapse by priority; management workbenches become tabbed or sequential at small widths; essential actions and current mode never disappear.
14. **Respect the simulation boundary.** UI summaries and commands must reflect authoritative game state. Presentation may aggregate or explain; it must not invent a parallel rules model.

## Adopt, adapt, reject summary

### Adopt

- peripheral settlement pulse and map-first layout;
- explicit time/pause/speed controls;
- docked selected-object inspector;
- spatial placement preview and selection outline;
- named overlay modes with legends;
- contextual, dismissible tutorials;
- dedicated key mapping and notification settings; and
- “locate on map” from management data.

### Adapt

- dense icon matrices into labeled, searchable tables;
- resource rails into configurable summary groups;
- full-screen management into a shared workbench with selection retention;
- crisis indicators into configurable severity semantics;
- bottom tool shelves into a shallower, keyboard-aware PC taxonomy; and
- relationship diagrams into sortable neighbors/diplomacy data plus history.

### Reject

- copied layouts, art, icons, textures, or distinctive compositions;
- console radial/cross menus as the primary PC interaction;
- icon-only controls without labels, tooltips, or shortcut discovery;
- color-only overlays or warning states;
- permanent panels that cover the central map;
- routine confirmation modals;
- one flat inventory/resource list in the persistent HUD; and
- buried or ambiguous z-level state.

## UX handoff for the next design wave

The information-architecture and visual-system work should treat these as the initial shell zones:

| Zone | Default responsibility | Behavior |
|---|---|---|
| Top-left | Settlement identity, population, critical material/food pulse | Compact groups; expandable detail; severity and trend |
| Top-center | Time, date/season, pause/speed, current z-level | Always visible; keyboard-operable; never moves between modes |
| Top-right | Alerts, event stream, settings/help entry | Severity-ranked; count and newest item; configurable auto-pause |
| Left/bottom tool shelf | Inspect, Dig, Build, Zone, Agriculture, Remove, overlays | Named active mode, breadcrumb, recent/favorite/search, shortcut badges |
| Right inspector | Tile/object/creature/workshop/stockpile context | Docked, resizable, collapsible; cause and actions; locate related |
| Management workbench | Population, jobs, inventory, production, military, neighbors, reports | Shared table shell; search/filter/sort; bulk actions; preserves map selection |
| World overlay | Footprints, designations, zones, paths, hazards, ownership, work state | Legend, non-color redundancy, transparent intensity, explicit exit |

The next wave should explicitly prototype and test:

- z-level controls beside clock/speed at 1280×720, 1920×1080, 2560×1440, and ultrawide;
- inspector widths that preserve a useful map viewport;
- build/designation category depth with search and keyboard-only completion;
- a 100+ row population or inventory table with selection retention and filtering;
- overlay legends in normal, grayscale, protanopia, deuteranopia, and tritanopia simulations;
- alert severity and auto-pause preferences; and
- a complete loop from a stalled workshop row to the relevant map object and corrective action.

## Source index

### Close named comparables

- Against the Storm: [archive page](https://interfaceingame.com/games/against-the-storm/), [HUD](https://interfaceingame.com/screenshots/against-the-storm-hud/), [Consumption](https://interfaceingame.com/screenshots/against-the-storm-consumption/), [Newcomers](https://interfaceingame.com/screenshots/against-the-storm-newcomers/), [Key Mapping](https://interfaceingame.com/screenshots/against-the-storm-key-mapping/)
- Frostpunk: [Game UI Database PC page](https://www.gameuidatabase.com/gameData.php?id=38), [Interface In Game page](https://interfaceingame.com/games/frostpunk/), [Construct resource](https://interfaceingame.com/screenshots/frostpunk-construct-resource/), [Coal mine](https://interfaceingame.com/screenshots/frostpunk-coal-mine/), [Construction tutorial](https://interfaceingame.com/screenshots/frostpunk-construction/)
- Prison Architect: [Game UI Database console page](https://www.gameuidatabase.com/gameData.php?id=603), [Interface In Game page](https://interfaceingame.com/games/prison-architect/), [Patrols](https://interfaceingame.com/screenshots/prison-architect-patrols/)

### Adjacent archive fallbacks

- Anno 1800: [Game UI Database](https://www.gameuidatabase.com/gameData.php?id=1118), [Interface In Game](https://interfaceingame.com/games/anno-1800/)
- Cities: Skylines (PC): [Game UI Database](https://www.gameuidatabase.com/gameData.php?id=526)
- Northgard: [Game UI Database](https://www.gameuidatabase.com/gameData.php?id=566)
