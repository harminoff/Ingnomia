# Command parity baseline

The supplied brief requires six representative action traces. Inventory immediate toggle and save/load pass on disposable tutorial-world copies. The Farm plan controls assigned a crop and count to exactly two of four selected plots; both plot plans survived deserialization, while job execution remains unproven. A Workshop Name draft stayed local until Apply, then appeared in the authoritative snapshot. A typed Exit confirmation canceled without leaving the main-menu route, then confirmed one `app.exit` action. A shell route transition and Back return were traced; Back restores the route but not the initiating focus target (NEW-002). Workshop Priority semantics remain inconsistent and its target-switch/close-draft behavior is assigned to Stage 10. Each gameplay trace compares the dispatched command with the authoritative state returned to the owning controller.

Stage 02 changed shared appearance tokens and stylesheet order only. It changed no command model, action port, controller contract, or gameplay mutation path; the parity results below remain the baseline for later stages.

| Action shape | Source path to trace | Required parity evidence | Status |
|---|---|---|---|
| Harmless navigation | `ShellRmlBinding` → `ShellController` → `ShellQtCommandPort` → route registry/document binding | One navigation action changes to the expected registered route once; Back/Close restores the prior route and invoking focus. | Route path traced: `shell.main_menu` → `shell.new_game` → `shell.main_menu`. Focus restoration failed: the probe focused `shell-back`, then Back returned focus to `shell-load` rather than the invoking `shell-new-setup`. Track fix in Stage 18 as NEW-002. [Trace](evidence/stage-00-command-navigation-focus-return-trace.log), [returned-route capture](evidence/stage-00-command-navigation-focus-return.png), [manifest](evidence/stage-00-command-navigation-focus-return-manifest.txt). |
| Staged edit | Workshop settings draft in `Management6AController` and management command port | Change a value without a command, then Apply once; the authoritative workshop snapshot matches the committed value. | Passed for Workshop Name: the form-only `Draft Carpenter` value left the snapshot unchanged, then Apply changed it. Stage 10: priority typing/arrows now stage only and Apply dispatches once; drafts persist by workshop ID across tab/object/Close ([Stage 10](stages/10-workshops-trade.md)). [Trace](evidence/stage-00-workshop-staged-edit-trace.log), [capture](evidence/stage-00-workshop-staged-edit.png), [manifest](evidence/stage-00-workshop-staged-edit-manifest.txt). |
| Immediate toggle | Inventory report grid keyboard path through `Management6BRmlBinding` and `Management6BController::toggleSelectedWatch` | Select a report row with the grid's arrow-key handler; one Space key dispatches one `watch.set` for that stable `InventoryRowId`; the returned Inventory snapshot reflects the new watched value at a newer revision. | Passed on copied Tutorial Valley save: `Drinks/Alcoholic/Wine/Apple`, watched `false` at revision 2 → `true` at revision 3. The production detached Qt window delivered Down and Space through `RmlUiQtInputAdapter`; [trace](evidence/stage-00-route-inventory-watch-live-trace.log), [capture](evidence/stage-00-route-inventory-watch-live.png), [manifest](evidence/stage-00-route-inventory-watch-live-manifest.txt). This is injected key-event evidence, not physical-input evidence. |
| Scope-sensitive bulk edit | Population skill/profession or agriculture multi-target action | Capture the selected target IDs and current world/revision; prove no hidden/stale row receives the command and the returned snapshot matches the intended scope. | Passed for Agriculture: selected two of four Farm plots, assigned Strawberry with count two only to those plots, and verified both other plots stayed unchanged after serialization. Planting-job execution remains open. [Trace](evidence/stage-00-route-agriculture-live-scoped-plan-trace.log), [manifest](evidence/stage-00-route-agriculture-live-scoped-plan-manifest.txt). |
| Destructive confirmation | `UiActionRegistry` typed `DestructiveConfirmation` modal; `app.exit` is registered as confirmation-required | Cancel leaves the app/world unchanged; confirm dispatches once from the active top modal and names the target. | Passed on an empty disposable app root: Cancel kept `shell.main_menu` with `confirm-cancel` focused; a second modal Confirm dispatched `app.exit` and produced one `aboutToQuit`. No world/save existed in the probe. [Trace](evidence/stage-00-confirmation-exit-trace.log), [capture](evidence/stage-00-confirmation-exit.png), [manifest](evidence/stage-00-confirmation-exit-manifest.txt). |
| Save/load | `EventConnector::onSaveGame()` → `GameManager`/IO and `onLoadGame(folder)` | Save a disposable world, copy its save, load only the copy, and compare world/visible state; do not modify normal user saves. | Passed with an isolated five-gnome tutorial world; all 21 file hashes match after copy-load, and source/copy surface captures are byte-identical. See [manifest](evidence/stage-00-disposable-save-manifest.txt), [source automation trace](evidence/stage-00-disposable-source-automation.log), [copied-load trace](evidence/stage-00-disposable-copy-load-trace.log), [source capture](evidence/stage-00-disposable-source-load-surface.png), and [copy capture](evidence/stage-00-disposable-copy-load-surface.png). |

## Shared command safeguards already present in source

`UiActionRegistry` registers typed action definitions with application, presentation, or world scope. Validation checks the active route, world epoch, expected revision, top modal ownership, and confirmation kind before an action is accepted. `ShellQtCommandPort` queues the existing EventConnector path. This is source evidence only; route-by-route command parity remains a required runtime gate.

The `DeleteStockpile` tile command calls the existing designation-removal path in `EventConnector::onTerrainCommand`; this destructive world mutation is not treated as a substitute for the typed confirmation flow. The UI must preserve current target identity and confirmation requirements where they apply.

Supplementary live world-context evidence: the generated-world tile probe dispatched Delete stockpile and Remove floor through the production inspector binding. It verified that the stockpile and both tile flags were removed while the item stayed on its tile, then observed an authoritative `RemoveFloor` job and canceled it through the exposed control. See the [assertion trace](evidence/stage-00-route-tile-live-trace.log) and [run manifest](evidence/stage-00-route-tile-live-manifest.txt). These direct context actions do not cover the typed destructive-confirmation modal, staged edits, or scope-sensitive bulk edits; the probe input was injected, not physical.

The live Agriculture probe adds one scope-sensitive bulk-edit trace: selecting two of four Farm plots and assigning Strawberry with count two changed only those plot plans and survived serialization. The saved world copy remained unchanged, and no planting job was created during the short run. See the [scoped-plan trace](evidence/stage-00-route-agriculture-live-scoped-plan-trace.log) and [manifest](evidence/stage-00-route-agriculture-live-scoped-plan-manifest.txt). This does not cover Stockpile scope, staged edits, farm defaults, or crop execution.

Stage 01 repeated that Farm scope probe on two fresh copies. Both identified the same two plot IDs (`287500`, `287501`), changed exactly those two of four, left two untouched, produced two live updates without Refresh, and returned byte-identical captures. Each source save copy retained all 21 file hashes. See the [repeat sentinel manifest](evidence/stage-01-repeat-sentinels-manifest.txt). This is repeatability and target-scope evidence; it does not establish crop job execution.

## Stage 04 command ownership — 2026-09-24

The focused adapter/binding suite records single command dispatch for held Space/Enter, cancellation, default actions, route reload and independent contexts (82 assertions). The production Qt-host probe injects key events through MainWindow and passes 13 checks, including a Space edit that leaves Start enabled, navigation repeat suppression, and Escape cancellation; pause/world-key signal counts are zero. Existing EventConnector command paths remain authoritative. New Game field/randomize operations report completion after successful enqueue to avoid an unresolvable pending state. These are command-routing and shell-edit checks, not a new-world simulation outcome or physical-input proof. See [Stage 04 evidence and limits](stages/04-buttons-tabs.md).

## Stage 05 form commands — 2026-09-24

Live Stockpile probing confirmed invalid priority blocks Apply and valid name, rank and hauling edits reach the authoritative game object through the existing binding/command/EventConnector path. All 21 copied-save files and the source remained unchanged. New Game exact fields and sliders converge without duplicate actions; invalid drafts block Start. Workshop radio and material choices update only the order draft; finished crafting remains unverified. See [Stage 05 evidence and limits](stages/05-form-controls.md).

## Stage 06 report identity — 2026-09-24

A 10000-record Inventory fixture preserves its selected stable ID through sort and unrelated patch; the emitted Watch payload retains that ID. Deleting it clears selection and prevents a replacement-target command. Filter exclusion clears the hidden target. Schedule focus navigation sends no mutation command and keeps creature/hour identity through reorder. Qt injection verifies actual detached-window end navigation and filter cancellation. These are binding/controller and input-routing checks, not new simulation outcomes. See [Stage 06](stages/06-reports-scrolling.md).

## Stage 07 review and edit boundaries — 2026-09-24

Shell Exit/Leave world and Population destructive/draft/scope reviews share one real modal document. Profession deletion validates the captured world and definition; all-citizens skill submission validates the reviewed population revision. A queued profession Apply retains the draft and reports pending until matching returned name/skills arrive; immediate and asynchronous rejection preserve it. Tests cover the command-port seam, including changed targets and repeat suppression. Production Qt probes cover opening, cancel, exact focus restoration and native Close protection; shell Exit reaches normal process termination. These do not establish atomic simulation-queue version checks or a new live profession persistence outcome. See [Stage 07 evidence and downstream owners](stages/07-dialogs-editing.md).

## Stage 08 — 2026-09-24

Inventory keeps typed command ports and authoritative snapshots. Detail Watch uses the current item identity; location links validate membership in the current detail snapshot. Removed details cannot dispatch stale actions. Filters/sort and two-axis report context survive nested details and Back. See the [Stage 08 checkpoint](stages/08-inventory.md).

## Stage 09 - 2026-09-24

Stockpile commands retain EventConnector/aggregator ownership. Bulk review captures the complete stable rule-ID scope and checks ID/revision/matches before dispatch. Template creation cannot overwrite; confirmed updates explicitly set replacement intent. Name/priority issue one Apply payload and remain draft until authoritative acceptance; hauling/suspension stay immediate. A copied-world run verified all 28 matching leaves, persisted template preservation/update, and suspension from manager and inspector. See [Stage 09 evidence and limits](stages/09-stockpiles.md).

## Stage 10 - 2026-09-24

Workshop commands keep the controller -> Management6AQtCommandPort -> EventConnector -> AggregatorWorkshop path. Settings are a property sheet: name, priority, suspension, production options, Butcher/Fishery options and stockpile links are edited as one pending draft; Apply or OK sends `workshop.set_basics`, `workshop.set_butcher_options`, `workshop.set_fisher_options` and one `workshop.set_stockpile_link` per changed stockpile, only for values that changed, and completion waits for the authoritative snapshot. Cancel and Close > No discard the draft with no command. Queue: Update Order sends mode, quantity and move-back together (`workshop.set_job`); Move to Top/Up/Down/Bottom, Suspend and Cancel Order act immediately on the selected stable job ID. Trade: Set Offer targets the selected stable row with the trader ID and revision; Review Trade... opens a message box and `trade.execute` is bound to the reviewed revision and merchant. Offer one less/more were removed (the spin box plus Set Offer covers them). Opening Trade dispatches `trade.refresh` once; Refresh remains. See [Stage 10](stages/10-workshops-trade.md).

## Stage 11 - agriculture

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Apply name and priority | General page Name + OK/Apply (`agriculture.set_basics`) | designation | pending; priority passed through (not implemented in game) |
| Suspend toggle | General > Status > Suspend all work | designation | pending |
| Harvest / Hay / Tame toggles | General > Work check boxes (`agriculture.set_harvest_options`) | designation | pending |
| Pick / Plant / Fell toggles | General > Work check boxes (`agriculture.set_grove_options`) | grove | pending |
| Next product + Use selected product / Set farm default | Crops/Trees list or pasture Type drop-down (`agriculture.select_product`) | designation | pending |
| Plot grid toggle, Select all, Clear | Plots grid (click, Ctrl, Shift, keys) + Select All / Clear | plot positions | local selection |
| Assign crop / Use farm default | Plots > Assign / Use Default (`agriculture.set_plot_crop`) | selected plot positions | immediate |
| Queue plantings / Queue repeat | Plots > Per plot + Queue / Queue Repeat (`agriculture.queue_plot_crop`) | selected plot positions | immediate |
| Order up / down / remove buttons per row | Plot Queue > select planting + Move Up / Move Down / Remove (`agriculture.move_plot_order`, `agriculture.cancel_plot_order`) | plot + planting ID | immediate |
| Next animal + Toggle butchering | Animals list check box per animal (`agriculture.set_butchering`) | creature ID | pending |
| Male/Female cap +/- (4 buttons) | Animals > Population limits spin boxes (`agriculture.set_population_caps`) | pasture + gender | pending |
| Toggle first food rule | Food page check box per food (`agriculture.set_food_allowed`) | pasture + item + material | pending |
| Locate | General > Status > Center on Map | designation | immediate |
| Refresh / Sort / paging / search | removed: lists scroll; Apply requests `agriculture.refresh` | designation | - |

## Stage 12 - population

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Views rail / Views toggle | Citizens / Skills / Professions / Schedules tabs (Ctrl+Tab) | view | navigation |
| Refresh (two copies) | Citizens > Refresh, F5 anywhere (`population.refresh`) | population | immediate |
| Name / Profession sort buttons | Clickable column headings, second click reverses | roster order | view |
| Previous / Next rows | removed: the roster scrolls | - | - |
| Row click opens detail | Row click selects; double-click, Enter or Properties opens (`inspect.select`) | creature ID | immediate |
| Skill citizen row toggle | Check box per citizen (`population.set_skill`) | creature ID + skill | immediate |
| Enable / Disable for all citizens | Enable for All / Disable for All + message box (`population.set_skill_for_all`) | skill, all citizens | immediate after review |
| Profession list rows | Profession drop-down list | profession ID | selection |
| New profession name + Create | New (numbered New Profession) (`profession.create`), rename in Name + Save Changes | profession | immediate / staged |
| Add skill / Remove / Move up / Move down | Add -> / <- Remove / Move Up / Move Down | profession draft | staged |
| Save profession / Discard draft | Save Changes (`profession.update`) / Discard | profession ID | explicit |
| Delete profession | Delete + named message box (`profession.delete`) | profession ID | immediate after review |
| Schedule activity toggle buttons | Activity option buttons | activity | selection only |
| Set cell / day / hour for all | Set Cell / Set Citizen's Day / Set Hour for All | cell / citizen / hour | immediate |

## Stage 13 - schedules

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Set cell | Select a cell, Set Activity (Enter/Space) (`population.set_schedule_cell`) | creature ID + hour | immediate |
| Set citizen's day | Citizen heading (Shift+Space), Set Activity (`population.set_schedule_row`) | creature ID | immediate |
| Set hour for all | Hour heading (Ctrl+Space), Set Activity + message box (`population.set_schedule_column`) | hour, all citizens | immediate after review |
| (new) range | Shift+click / Shift+arrows, Set Activity + message box when several citizens (`population.set_schedule_cell` per cell) | creature IDs x hours | immediate after review |
| Enter/Space cycled the focused cell's activity | Enter/Space run Set Activity for the stated scope | selection | explicit |

## Stage 14 - military

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Views rail, Filter/sort, search, Refresh | Tabs; F5 refresh (`military.refresh`) | view | navigation |
| Add squad / Earlier / Later / Rename / Delete... | Squads: New / Move Up / Move Down / Rename / Delete + message box (`military.add_squad`, `move_squad`, `rename_squad`, `remove_squad`) | squad ID | immediate (delete after review) |
| Assign to squad / Remove from squad | Members: Add -> / <- Remove (`military.assign_squad`, `military.remove_gnome`) | creature ID + squad ID | immediate |
| Move to previous / next squad | Members: Move to: <squad> + Move (`military.assign_squad`) | creature ID + named squad | immediate |
| Member role choices | Members: Role drop-down (`military.assign_role`) | creature ID + role ID | immediate |
| Add role / Rename / Delete... | Roles: New / Rename / Delete + message box (`military.add_role`, `rename_role`, `remove_role`) | role ID | immediate (delete after review) |
| Civilian toggle button | Roles: Civilian check box (`military.set_role_civilian`) | role ID | immediate |
| Uniform type / material rows | Uniforms: Equipment / Material drop-down lists (`military.set_uniform_slot`) | role ID + slot | immediate |
| Priority Move up / down | Targets: Move Up / Move Down (`military.move_priority`) | squad ID + target | immediate |
| Flee / Defend / Attack / Hunt buttons | Targets: Response option buttons (`military.set_attitude`) | squad ID + target | immediate |

## Stage 15 - diplomacy

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Views rail, search, sort, Refresh | Neighbors / Missions tabs; F5 (`diplomacy.refresh`) | view | navigation |
| Neighbor row + in-page mission builder | Neighbors page + Send Mission... wizard | kingdom ID | wizard |
| Spy / Emissary / Raid / Sabotage buttons | Wizard Mission page option buttons | draft type | value |
| Improve / Insult / Invite trader / Invite ambassador buttons | Wizard "Emissary's task" option buttons | draft action | value |
| Eligible citizen rows | Wizard Citizen page list (`diplomacy.refresh_available_gnomes`) | creature ID | value |
| Start mission | Wizard Review + Finish (`diplomacy.start_mission`) | kingdom + type + action + creature | once, after review |
| View missions / Go to Neighbors | Missions / Neighbors tabs (Finish opens Missions) | view | navigation |

## Stage 16 - inspectors

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Creature view rail (Camera/Stats/Expertise/Equipment/Inventory) | General / Attributes / Skills / Equipment / Inventory tabs; Ctrl+Tab | page | navigation |
| Skill "Order by" rail buttons | Skill / Level / Active column headings | sort | view |
| Profession toggle + menu | Profession drop-down list (applies at once) | creature ID | immediate |
| Paper-doll slot buttons | Slot / Item list view rows | slot | selection |
| Type / Material choice buttons | Type / Material drop-down lists + Apply (scope stated) | role + slot | explicit commit |
| Tile rows with per-row action buttons | Type / Name list + command buttons below | tile | immediate |
| "Inspect first creature" / per-row Open | Creature row (by ID) + Inspect, or double-click | creature ID | navigation |
| Blueprint Cancel beside Raise/Lower | Cancel Blueprint set apart below a separator | tile job | immediate |
| Population citizen detail page | Population Properties opens the creature inspector | creature ID | navigation |

## Stage 17 - HUD

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Top rail Z-/Z+ | Level Down / Level Up toolbar buttons | view level | immediate |
| Pause/Resume, Normal, Fast | Pause, Normal Speed, Fast Speed (option-set) | clock | immediate |
| Sidebar Management buttons | Inventory, Population, Military, Missions toolbar buttons | window | navigation |
| Sidebar Orders (Mine, Agriculture, Designations, Jobs pages) | Toolbar menu buttons with drop-down menus | tool | armed mode |
| Sidebar Map overlays toggles | View menu check items | overlay | immediate (echoed) |
| Hint strip Cancel / Rotate | Cancel Tool / Rotate toolbar buttons; Esc / right-click | tool | immediate |
| Sidebar tooltips | Status bar message pane | help | read-only |
| Build rail, type buttons, item cards | Build palette: Categories list, Material drop-down, Item list view, Build | catalog item | armed mode |
| Event modal (Continue / Yes / No) | Information message box (OK / Yes / No) | prompt | once |

## Stage 18 - main menu, new game, saves

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Continue / Load game / Tutorial / Quick start / Custom game / Settings / Exit cards | One column of dialog commands (Load Game..., Custom Game...) | route | navigation |
| New Game tab rail (World, Settlement, Terrain, Review) | Custom Game wizard pages with < Back / Next > | draft page | navigation |
| Start kingdom | Finish (Completion page; Enter) | draft | once, revalidated |
| Back (New Game) | Cancel | route | navigation |
| Kingdoms list | Look in: drop-down | kingdom | selection |
| Saves list | Name / Modified / Version list view; double-click opens | save | selection |
| Load selected save | Open (default) | save | once |
| Refresh | F5 | save list | immediate |

## Stage 19 - settings, pause, loading

| Old control | New access path | Target | Model |
| --- | --- | --- | --- |
| Settings cards (Display, Camera and controls, Audio, Saving) | Settings tabs Display / Controls / Sound / Saving | setting | immediate |
| Restore defaults | Defaults | all settings | immediate |
| Back (Settings) | Close | route | navigation |
| Pause card commands | Pause dialog: Resume, Save Game, Load Game..., Settings, Main Menu; Esc resumes | route / simulation | once |
| Return to main menu / Exit confirmations | Win98 message box, Yes / No (No focused) | world / app | once, confirmed |
| Loading progress and error row | Loading message box; failure with Retry / Cancel | world transition | once |

## Stage 20 - keyboard

| Command | Keyboard path | Scope |
| --- | --- | --- |
| Any command with an underlined letter | Alt+letter, or the letter when the focus does not take text | shell dialogs, wizard, Settings, Load Game, message boxes |
| Next / previous tab | Ctrl+Tab, Ctrl+Page Down / Ctrl+Shift+Tab, Ctrl+Page Up; Left/Right on a tab | every property sheet |
| Default command / cancel | Enter / Esc | every dialog, message box and wizard |

## Stage 21 - Stockpile and Inventory

| Command | Mouse | Keyboard |
| --- | --- | --- |
| Toggle an allow-list rule (pending) | Check box on the row | Space on the selected row |
| Allow or block every rule shown (pending) | Allow All / Block All | Alt+L / Alt+B |
| Apply pending Stockpile changes | Apply or OK | Alt+A, or Enter for OK |
| Load or save an allow-list template | Load / Save | Alt+O / Alt+S |
| Watch an item | Check box on the row, or Watch this item | Space on the selected row |
| Open Item Properties | Properties or double-click | Enter on the selected row |
| Open a stockpile or product from Item Properties | Properties or double-click | Tab to the list, arrow keys, Properties |
