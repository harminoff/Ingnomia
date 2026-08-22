# RmlUi section worklist

Status: working backlog for the post-migration UI pass  
Baseline map: [`current-ui-map.json`](current-ui-map.json) (23 original surfaces)  
Visual reference: classic park-management density, bevels, compact controls, and
the accepted inventory hierarchy treatment.

This is the review list for finishing the new GUI. A replacement document or
controller means the surface exists; it does not mean that real gameplay,
save/reload, physical input, or final visual acceptance has passed.

## How to close a section

For each row, check the applicable gates in order:

- [ ] **Visual:** title bar, tabs, rows, spacing, icons, focus/pressed states,
  clipping, scale, and empty/loading/error states match the shared style.
- [ ] **Interaction:** mouse, keyboard, wheel, drag, modal blocking, and Escape
  behavior work in the real executable.
- [ ] **Authoritative data:** the section is fed by the real aggregator/bridge;
  actions are typed, queued, validated, and reflected back from authoritative
  state.
- [ ] **Persistence/lifecycle:** new/load/save/reload/world-unload and repeated
  open/close cycles retain or clear state correctly.
- [ ] **Evidence:** focused tests, production build, physical/runtime proof,
  inspected framebuffer, and any remaining limitation are recorded.

## Open-gate index

The numbered rows below are the review queue. A row remains **open** when any
of its five gates is still outstanding; a green fixture/native proof does not
close the corresponding live-game, physical-input, or persistence gate.

### Phase A - host, shell, and world interaction

| # | Surface still needing review | Primary open gates |
|---:|---|---|
| 1 | Application host, rendering, input boundary | Production host build and physical mouse/keyboard/focus-loss/resize/Unicode/shutdown probes pass; true IME composition and multi-scale/DPI change remain environment-dependent |
| 2 | Root route shell | Real-save routes and physical new-game text/load/pause/title paths are evidenced; three repeated Load/Settings/New Game teardown cycles now pass with clean exit and unchanged copied-save metadata; true IME composition remains |
| 3 | Main menu | Physical setup-game route and committed Unicode form input are evidenced; physical Continue/Load/Exit and focus restoration remain |
| 4 | New-game setup | Physical new-game route and Unicode kingdom-name entry are evidenced; validation/options/seed behavior and save creation remain |
| 5 | Continue/load browser | Compatible/incompatible/empty saves, physical row activation, and return route |
| 6 | Settings | Apply/revert and persisted scale/fullscreen behavior |
| 7 | Loading page | Authoritative progress/error/retry and input blocking |
| 8 | Pause/save/resume/title | Physical Pause/Save/Resume/return-to-title and Load/reload are evidenced; world-epoch reset is runtime-traced; true IME composition and multi-scale DPI remain |
| 9 | HUD/time/watch/tools | Physical world input, selection feedback, event/pause persistence; authoritative pressed-state feedback is now evidenced for pause/speed/Designations, and a DB-backed migration prompt/response is now literal-tested |
| 10 | Build/designation/tools | Category/item matrix, placement, rotate/cancel, material errors, and save proof |

### Phase B - contextual and management workbenches

| # | Surface still needing review | Primary open gates |
|---:|---|---|
| 11 | Tile inspector and terrain/object actions | Live discovered save, designate/locate, empty context, and click ownership |
| 12 | Creature inspector | Live updates, stale selection, and persistence; the original screen has no creature command surface, and supported gnome skills are now projected and runtime-evidenced |
| 13 | Agriculture | Farm/pasture/grove rosters, empty/error states, and save/reload |
| 14 | Stockpiles | Live filters/content, priority/toggle responses, and reload |
| 15 | Workshop/crafting/trade | Full supported mutation matrix, authoritative progress, and reload |
| 16 | Population/professions/skills/schedules | Live roster updates, schedule mutation, and large-list persistence |
| 17 | Inventory/resources | Remaining thumbnail coverage, history/watch responses, and reload |
| 18 | Military | Live squads/roles/priorities/materials, mutation errors, and reload |
| 19 | Neighbors/diplomacy/missions | Live updates, mission responses, confirmation/focus, and persistence |
| 20 | Events/questions/notifications | Every live event source, modal ordering/dismissal, and save/load behavior |

### Phase C - release and cross-cutting gates

| # | Surface still needing review | Primary open gates |
|---:|---|---|
| 21 | Developer/debug UI | Developer-only registry/package teardown recheck |
| 22 | Shared styles/localization/assets/world art | Long strings, locale, scale/contrast/reduced motion, and release-owner asset/licensing sign-off |
| 23 | Keyboard-command catalog/editor | **Deferred** until dispatch, conflict detection, persistence/reset, and IME rules are unified |

Use the detailed row for each section as the acceptance record; check off a
gate only after the corresponding evidence is attached to the review log.

## Indexed sections

| # | Section | Current RmlUi owner | Current state | Next work to review |
|---:|---|---|---|---|
| 1 | Application host, rendering, and input boundary | `RmlUiHost`, `MainWindow`, `app_shell.rml` | Implemented; production startup/build proven; native WM_CLOSE now routes through orderly Qt teardown and exits 0 in the physical desktop pass; the isolated foreground probe also proves world-drag focus cancellation, one-scale resize, physical Pause/Resume, and committed Unicode delivery | True IME composition, multi-scale/DPI transition, and broader world/UI click-through matrix remain. |
| 2 | Root route shell | `src/gui/ui/screens/shell/*`, `documents/app_shell.rml` | Styled/routed; static contracts pass; listener teardown hardened for route, load-row, and confirmation callbacks; native shell proof now exits 0 and the physical new-game/load/pause/title paths are evidenced; a foreground production matrix completed three Load, three Settings, and three New Game open/back cycles with 10 main-menu loads and exit code 0 | True IME composition remains; the New Game route requires a visible window placement for its non-centered wide Back control. |
| 3 | Main menu | `screens/main_menu.rml` | Functional surface exists; production executable rebuilt; real Continue dispatch reached `init` and `postCreationInit` against the newest compatible save; Settings → Back route smoke passed; isolated production save/end/reload now returns to a live world after title transition | Exercise Load/Exit with physical input and retain route/focus proof. |
| 4 | New-game setup | `screens/new_game.rml`, shell controller | Typed form/randomize actions exist; production new-game automation generated a visible centered world and completed post-init; the real foreground shell route accepted and rendered committed Unicode in the focused kingdom-name field | Exercise seed/name/options validation, physical option editing, and save creation. |
| 5 | Continue/load browser | `screens/load_game.rml` | Stable save rows exist; explicit `TheFragmentedLand/1` load completed in production and produced a post-load world framebuffer; queued production reload from an isolated copied slot completed `init`/`postCreationInit` and captured the reloaded world; physical foreground selection of the kingdom and newest compatible slot now loads the live HUD/map | Test compatible/incompatible saves, empty/error states, and return to the correct route. |
| 6 | Settings | `screens/settings.rml` | Five backed settings are exposed; production shell Settings → Back automation dispatched both live listeners successfully | Verify round-trip persistence, reset/revert, scale/fullscreen behavior, and omission of unsupported settings. |
| 7 | Loading page | `screens/loading.rml` | `EventConnector` now publishes authoritative world-transition start/progress/finish signals; shell lifecycle state shows the loading route, blocks world input, renders generator text, and exposes retryable failure state; focused lifecycle tests and a rebuilt production load/save/reload probe pass | Add a captured production loading/error frame and physical retry/back input proof. |
| 8 | Pause, save, resume, and return-to-title | `screens/pause_menu.rml` | Pause/resume route exists; isolated production lifecycle now saves slot 2, returns to menu, reloads slot 2 on the queued game-thread path, and captures a live reloaded world (`persistence-probe3-reloaded.bmp`, SHA-256 `986445D5C5E141AF820E286A77CFF08176E8D5B8811F55C17501A991B8E1DB89`); the physical Pause/Save/Resume sequence is captured in `build-wave8-root-msvc/physical-pause-resume-20260815bu/` with frame hashes `4DA05DFDE93690BF81A9334A11BC9A19A74E37C19698DD2832A7B021E5701DAC`, `09BBCC34F213C2D01063D6FB8333F92BA4F6FCB39A742A0D88E5A2E27C30D122`, and `FEB986418866E28304EFB336D2FB1BF23C4AA27EE739C1F3FCAECE0A10EC6FB0`; physical Return-to-main-menu is captured in `physical-return-title-20260815bv/`; physical Load browser selection and live reload are captured in `physical-load-reload-20260815bx/`; the queued production trace now records menu `true` at epoch 1 followed by reload menu `false` and epoch begin 2 | The epoch reset boundary is runtime-evidenced; true IME composition, multi-scale/DPI, and repeated route teardown remain cross-cutting. |
| 9 | In-game HUD, time, watch list, render toggles, and speed | `screens/game_hud.rml`, HUD controller | Classic-park HUD and typed controls exist; authoritative clock/settlement/render/event producers are wired; watch-list producer renders clickable compact rows; the migrated ToggleButton-equivalent states now project `is-selected` and `aria-pressed` from authoritative pause, speed, and overlay state; focused HUD CTest and a real-save foreground framebuffer matrix pass | Finish physical world-input/selection/tool ownership and event/pause persistence under a real save. |
| 10 | Build/designation command tiers and active selection | HUD build catalog, tool controller, `game_hud.rml` | RCT2-style tabbed catalog, authoritative build rows with cropped thumbnails, explicit `Fill hole`/`Replace`/`Build` action tiers, three-column cards, draggable window, unavailable-material state, active build preview, typed tool seams, and physical terrain input after styling the dynamic scrollbar; the real copied save opened Build → Walls, selected the live `Palisade` card, activated all three action routes, rendered a literal production frame with complete un-clipped buttons and `Active tool: BuildWall`, and retained the earlier physical Fill hole/Replace placement/cancel evidence (`physical-build-explicit-build-20260816e/explicit-build.png`, SHA-256 `2093110E83DE989ADB478938C4C7F5AF45340F9B75E2E7AA665C64948308A37B`) | Exercise every DB category with real sprites, second-component/material-error behavior, placement validation/commit, and save/reload proof; physical pointer proof for the new explicit Build button and world right-click remain distinct gates. |
| 11 | Tile inspector and terrain/object actions | `screens/inspector.rml`, inspector controller | Classic-park contextual dock styling applied; real TileInfo/Selection producers and queued context/locate/rotate/cancel seams are wired; the original terrain action set now has typed Remove floor/Fell tree/Remove plant buttons gated by authoritative tile state; the active-job panel now projects authoritative WorkablePosition and RequiredItems; queued cancel refresh clears the job panel and restores Mine wall; focused Inspector CTest and literal production Mine/Fell/Remove plant/Harvest/Manage/Remove floor evidence pass; both authoritative priority bounds now disable their unavailable action at 0 and 9; a real inspector Mine/Raise mutation survives production save/end/reload and is visible at priority 1 after reload | Physical map/tool ownership, completion/disappearance, placement/cancel, selection changes, and broader inspector persistence remain. |
| 12 | Creature inspector | `screens/inspector.rml` | Creature detail route, queued CreatureInfo refresh, stable selection/back/locate, live supported equipment rows, authoritative carried-inventory projection with a truthful empty state, and unload/stale-ID clearing exist; a rebuilt production run now proves the real tile-to-creature route, equipment rendering, and `No carried items` | Exercise skills/actions, stale-selection rejection after an actual disappearance, non-empty inventory rendering, and persistence; the original uniform/inventory icon assets remain unavailable. |
| 13 | Agriculture management | Agriculture inspector section, `panels/agriculture_manager.rml` | Farm/pasture/grove DTO adapters and typed priority/harvest/butchering/grove mutations are wired; focused inspector proof covers all three kinds | Exercise real farm/pasture/grove rosters, empty/error states, and save/reload confirmation. |
  | 14 | Stockpile management | `windows/stockpile_manager.rml`, management 6A | Stockpile surface, stable paging/filter rows, queued filter/toggle/basic mutations, and bounded tests/native proof exist; real production captures now preserve the original category/group/item/material preorder, apply the original tri-state disclosure rule (mixed parent rows expanded, fully on/off descendants hidden), show compact depth/disclosure markers and live storage controls, project the original one-based priority labels over the zero-based game index, prove live suspended/pull/allow-pull responses, suppress zero-count allowed-filter summaries from `Current contents` as the original model does, expose the selected filter's authoritative label/state/depth, match the original mixed-parent mutation rule in a focused contract test, and prove priority Apply plus filter, pull, and allow-pull mutation save/end/reload with the normal post-reload `tile_manage` route | Exercise broader filter persistence, complete physical keyboard/focus evidence and Enter/Space save/reload evidence; the original copy/paste buttons are unbound and the limits controls are commented out with `Limits implemented later`, so the migrated warning/omission contract is now the accepted truthful boundary. |
| 15 | Workshop, crafting, butcher/fisher, and trader | `windows/workshop_manager.rml`, management 6A | Workshop/queue/trade surface, confirmation guard, stable row keyboard/search, representative queue/agriculture/trade seams, and an explicit Link stockpile toggle now exist; native 6A proof passes and the toggle preserves cached-link behavior for older callers while dispatching explicit true/false intent | Exercise every supported queue/craft/trade/butcher/fisher mutation, authoritative progress, positive LinkStockpile state with a real adjacent stockpile, and save/reload. The retained real save has no adjacent stockpile at either inspected workshop input position, so the positive link state remains open. |
| 16 | Population, professions, skills, and schedules | `windows/population_manager.rml`, management 6B | Current style baseline accepted; real population/profession/skills/schedule signals are wired, hierarchy/list controls and exact schedule actions pass focused/native proof; a production real-save capture shows three authoritative citizens in the live roster (`live-population.bmp`, SHA-256 `46C82E558B168D1AEE20F7EDBA6E71CC6DFE3B1BC73B07B75173E65956C787E2`), and a second real-save frame shows Knute hour 0 updated to checked Eat after a live schedule dispatch (`live-population-schedule-mutation.bmp`, SHA-256 `912471C0F6FF69C48E9BA72C790AD855A2DA77D7A05DDF1D7A9D58CD8658205C`). The shared production save/end/reload lifecycle also completes without stale-world crash. | Exercise physical row focus/keyboard input, profession/skill mutation persistence, and large-list save/reload persistence in a saved world. |
| 17 | Inventory and resources | `windows/inventory_browser.rml`, management 6B | Current canonical style accepted; category/group/item hierarchy, thumbnails, expansion, sort/filter/search/paging, watched rows, and authoritative category snapshots pass focused/native proof; a production real-save capture shows the hierarchy and thumbnails (`live-inventory.bmp`, SHA-256 `D842ECEBA647DE01D384368258475C17933F38AF24F6F0A125DFC661D189B5A`), and a second real-save frame shows a selected `raw wood` leaf with `[x] Watching selected` and the HUD watch row after the live `watch.set` response (`live-inventory-mutation.bmp`, SHA-256 `F11D286D85487CA8106962868B5A238FCB5BD03C06307885EF3D8FA447A7616F`). The typed history bridge now survives a copied-save save/end/reload: request and post-reload request both dispatched, and post-reload status reached `14:ready`. | Continue missing DB-thumbnail audit and broader inventory mutation coverage. |
| 18 | Military management | `windows/military_manager.rml`, management 6C | Typed military surface with compact pale-steel rows/fixed footer, queued AggregatorMilitary bridge, stable keyboard/actions, discovery/unassigned guards, and native proof passes; production real-save capture shows the live squad/unassigned roster (`live-military-classic.bmp`, SHA-256 `108E4E6490AB169FBDF70E147D4CA100E9E12B05B6F4488267703D313C101CC4`), and a live role probe added a real role and rendered the selected role, uniform slots, and assignment surface (`live-military-mutation-fixed3.bmp`, SHA-256 `56432C25CF63E1FDBA4F8C507C2635F43C323E2F7CD05336A51082CFCE327A2C`). | Exercise physical row focus, squad/member/priority/material mutations, mutation errors, and save/reload. |
| 19 | Neighbors, diplomacy, and missions | `windows/diplomacy_missions.rml`, management 6C | Classic-park detail fields and fixed footer are live-tested; production real-save capture shows masked neighbors, IDs, mission controls, and visible Start action (`live-diplomacy-classic.bmp`, SHA-256 `B6E51EBD268CD461DED05BC53626543C54F4F93B97FA372735EA2EE75F62FFFF`), and a live probe selects an authoritative masked neighbor while preserving the game's undiscovered placeholder policy (`live-diplomacy-selection-fixed.bmp`, SHA-256 `9E61002C33EB838A06589E12D6693A16CE26562420F6F03FCE4B3311CD2671D9`). | Exercise discovered-neighbor mission responses, confirmation/focus restoration, and persistence; undiscovered masking is now production-proven. |
| 20 | Event questions and notifications | HUD event/prompt binding and modal stack | Typed acknowledge/yes-no FIFO, response-target validation, pause request, centered 500x300 blocking modal, role/ARIA metadata, first-action focus, and DB-backed migration/acknowledge-only prompts are literal-tested; a fresh production queue probe now shows migration Yes/No followed by invasion Continue in order; no alert-history panel fabricated | Connect every supported live event source, prove required-modal ordering/dismissal for more source combinations, physical pointer ownership, and save/load behavior. |
| 21 | Developer/debug UI | `developer_ui/debug_panel.rml` | Developer-gated, release-dormant, typed debug actions/catalog requests, stale-epoch and dirty-counter proof exist; native developer proof has passed | Keep out of release registries and recheck only developer builds/teardown after package cleanup. |
| 22 | Shared localization, styles, templates, fonts, icons, and world art | `styles/*`, `templates/*`, localization adapter, staged assets | Shared classic-park style, RmlUi/FreeType notices, Lato font, 17 staged tilesheets, localization/accessibility verifiers, and all 16 static section verifiers pass; the eight remaining DB names were audited against every Git object and `origin/gh-pages` and are genuinely unavailable. A real-save 200% capture now keeps the inventory workbench within the viewport with full-width toolbar/columns, aligned thumbnails, and a Windows-98 scrollbar (`inventory-scale-200-final81.bmp`, SHA-256 `EC7F150866788407754365A1269C6E169DAF67DDCBE54CE1AC57BECF63DFA223`). | Finish runtime long-string/locale/contrast/reduced-motion proof, verify the scaled workbench footer/history reachability, and obtain release-owner confirmation for the documented absent-sheet/provenance exception. |
| 23 | Keyboard-command catalog/editor | Deferred; no released document | Intentionally not exposed | Unify key dispatch, conflict detection, persistence/reset, and focus/IME rules before adding a keybinding editor. |

### Row 7 correction after the 2026-08-16 iteration

The loading Retry gate is closed for the exercised missing-`world.dat` failure
class. The prior row summary's "Add a captured production loading/error frame"
follow-up is superseded by the 2026-08-16 review entry and the retained error and
recovered-world frames under `build-wave8-root-msvc/physical-loading-retry-20260816cc/`.
Other failure sources, repeated teardown, and the ranked cross-section work remain
open.

### Row 5 correction after the 2026-08-16 iteration

The Continue/load browser gate is closed for the exercised real-metadata state
matrix. The original `content/xaml/LoadGamePage.xaml` exposes two bound list boxes,
automatic first-item selection, Load, and Back, but no explicit empty or error
surface. The production `screens/load_game.rml` now retains that hierarchy while
adding explicit no-kingdom, no-save, and save-list-error states. `AggregatorLoadGame`
now reports malformed or vanished metadata through a narrow Qt error signal;
successful refreshes clear the error, valid rows remain visible, and an
incompatible selected row cannot dispatch `app.load_game`.

Literal production evidence uses only copied real save metadata. Empty kingdom
frame `build-wave8-root-msvc/physical-load-empty-20260816db/empty-load.png` has
SHA-256 `FEC234DD129A0158BB9CAFCA07577570DC651AEEAA6CC48D164F775DB0A51539`;
the selected-kingdom/no-save frame is
`build-wave8-root-msvc/physical-load-empty-saves-20260816df/saves-empty.png`
(`A1AF5E516DC4D36EB60B31F88B19C618828094FA88A3FAF31040E981D843A0BB`); the
malformed copied `game.json` frame is
`build-wave8-root-msvc/physical-load-save-error-20260816de/save-list-error.png`
(`C41BECEE7EBBBB549E8E4E751A3395A91B87D196D88B6ED94DF319675DDBDBA0`); and
the old-version row remained on the browser after a physical Load attempt with
zero `Start loading world` markers (`physical-incompatible-load.trace.log`,
process exit 0). Physical Back returned to the main menu in
`physical-load-back-click-final.trace.log`, whose route log records
`shell.load_game` then `shell.main_menu`; the resulting frame hash is
`AB118BBD74F19A14EA93735BF3B72F7729FFFB839F2D2125D520624B563A7152`.
Focused shell CTest remains 3/3, the production executable is
`04B821A2AB8422CD3D700555D2D898B50CAE4AB76A2452E907C89F2603CB190A`, and
the copied source metadata remained byte-sized at 80,130 bytes while only the
isolated malformed copy was 80,129 bytes.

### Row 2 correction after the 2026-08-16 iteration

The original `MainPage.xaml`, `LoadGamePage.xaml`, `SettingsPage.xaml`, and
`NewGamePage.xaml` contracts were re-inspected against the production RmlUi
documents. The migrated routes retain the original navigation hierarchy and
Back ownership; `ShellRmlBinding::loadRoute` unloads the previous document and
detaches route/load-row listeners before binding the next one, while shutdown
clears callback storage. A real foreground production run against the copied
`TheFragmentedLand/1` save completed three cycles of Load, Settings, and New
Game open/back transitions. The authoritative log records 3
`shell.load_game`, 3 `shell.settings`, 3 `shell.new_game`, and 10
`shell.main_menu` loads; the process exited `0`; and the copied
`game.json` remained SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` before
and after. The inspected final production frame is
`build-wave8-root-msvc/physical-load-compatible-20260816dd/physical-route-teardown-final.png`
(`892593416D6C5365AD65549E2640C5E9836442CB549DB448621FDD1F6722CCBE`), and
the complete input/process trace is
`physical-route-teardown-20260816-final.trace.log`
(`41103E5C17E1530BF6F550EDBFE72475D2FC39C17426C16E9FDB83A85B582418`).
The probe moved the window to `(0,0)` only to expose the non-centered wide New
Game Back control; this placement observation remains a visual/layout note,
not a route-teardown failure.

This closes the repeated shell-route teardown gate. True IME composition,
multi-scale/DPI, and section-specific live mutation/persistence remain open.

### Row 9 correction after the 2026-08-16 iteration

The original `content/xaml/GameGui.xaml` used bound `ToggleButton.IsChecked`
state for pause, normal/fast speed, and Designations/Jobs/Walls/Axles. The
production `screens/game_hud.rml` previously authored ordinary buttons without
projecting those authoritative states, so actions worked but the selected mode
was not visible or exposed as pressed state. `HudRmlBinding::stateChanged` now
projects `is-selected` and `aria-pressed` from `ClockCalendarState` and
`RenderOverlayState`; no controller or EventConnector contract changed.

Focused HUD CTest passed 5/5, including the pressed-state RML contract. The
rebuilt production executable is SHA-256
`02B956A62B725D45383AA82AFAF441696CDEE880728EBAB0BA4E536CE1E48A22`, and the
staged runtime HUD RML matches source at SHA-256
`6C10EA23151611EBD8030C34AE45FB693713BD5FAE6A5F354C89ED213AF921C2`. A
foreground Windows run loaded copied real save metadata, used Space to keep
the HUD mounted while toggling pause, then clicked Fast and Designations on
the live rail. It exited `0` with unchanged copied `game.json`; inspected
frames are `physical-hud-pressed-space-unpaused.png`
(`AA8B6A530EEC2CEF1D0344ECEF1ABF2B3A7C63D572ED44C1B76D715DBA8F7F74`),
`physical-hud-pressed-speed-fast.png`
(`33CE8DB59404C0D886A895524D005C8495E02656A37E309481D46DCB8216F9C9`),
and `physical-hud-pressed-designations-on.png`
(`D12825A9A5AB0054EE010AEA80FDFBBA8350F1C278C6222D73ED3B993543289F`).
The complete trace is `physical-hud-pressed-20260816-space.trace.log`.

The HUD row remains open for physical map selection feedback, tool ownership,
world/UI click-through, and event/pause persistence; the Pause button's
separate blocking `game.pause` route is intentionally not conflated with the
HUD-mounted Space toggle.

### Row 9/11 correction after the 2026-08-16 map-selection iteration

The original `GameGui.xaml` contract routes a plain Inspect world click into
the bound `TileInfo` region while retaining the world view and the bottom
selection/status region. The production path remains
`MainWindow` -> queued `AggregatorSelection::onLeftClick` ->
`signalSelectTile` -> `AggregatorTileInfo::onShowTileInfo`; no parallel UI
selection model was introduced.

A corrected foreground Windows probe loaded the copied real save directory,
moved the window to `(0,0)` only to make the measured client coordinates
visible, and physically clicked the live map at client `(1600,820)`. The
before frame shows the live HUD/map; the after frame shows the authoritative
`Tile 1000332` inspector with `X 32 Y 3 Z 100` and `Floor: dirt soil` while
the world remains visible. The process left no running Ingnomia instance,
the copied `game.json` stayed SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and
the captures are `build-wave8-root-msvc/physical-map-selection-20260816b/map-before.png`
(`B0616765760091F9ECC39EEFA0943C2BB202E318D033B9E20BE3B242AD099865`) and
`map-after-inspect-click.png`
(`B3938CA38721C8CF19C25696061118A69766812A15D1E961B701C6DAC1A3CD46`).

This closes plain tile-selection feedback for the exercised live tile. The
remaining Row 9/11 interaction gates are inspector close/map click ownership,
entity/empty-context changes, tool-mode handoff, and event/pause persistence;
the first failed probe used a file path where the explicit automation seam
requires a save directory and is not product evidence.

### Row 11 correction after the 2026-08-16 terrain-action iteration

The original `TileInfo.xaml` terrain template exposed `Mine`/`Replace` for a
wall, `Remove`/`Replace` for a bare floor, and `Harvest` plus `Destroy` or
`Fell` for plants. The production adapter already computed
`canRemoveFloor`, `canFell`, and `canRemovePlant`, but the RmlUi surface did
not render or dispatch those states. The safe correction adds three typed
`TileContextAction` values, binds `Remove floor`, `Fell tree`, and `Remove
plant`, gates each button from the authoritative `TileInspectorState`, and
maps them through the existing queued `EventConnector::onTerrainCommand`
contract to `Remove`, `Fell`, and `Destroy` jobs. No new game-thread bridge or
parallel selection model was introduced.

Focused Inspector CTest passed 3/3 (controller, RML contract, integration
contract), the production Release rebuilt with executable SHA-256
`3549F37E2CBB56707A121D17BE36B0A3503D5E80187825D6979539CE9B03792E`, and the
staged inspector RML matches source at SHA-256
`08D419C1E4E8CE424197B9A427C3F110C767132B82F9550BE5FF35D16D3FA28D`.
Against a fresh copied real save, a physical map click at client `(1600,820)`
opened `Tile 1000332` with `Floor: dirt soil`; a physical click on
`Remove floor` at client `(1910,292)` produced the live `Active job`
section with `RemoveFloor | worker unassigned | priority 0 | skill Mining`
and the map remained visible. Retained frames are
`build-wave8-root-msvc/physical-inspector-actions-20260816/inspector-tile-before-action.png`
(`C4042C04F6B5957A8A94ED926BA73040CB4E5F5B7B9B853307B5C34193B63903`) and
`inspector-remove-floor-result.png`
(`23DAB35E90FE0DA2A59FC7525DF4B9604477A48334A8593EA08A190C3A020DB8`);
the copied `game.json` stayed equal to the source SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and no
Ingnomia process remained.

This closes the implemented Remove floor branch for the exercised live tile.
Fell tree, Remove plant, Mine, Manage, and
save/reload response remain open; the legacy `Replace` action is not claimed
because its current EventConnector contract has no equivalent safe typed
dispatch.

A separate fresh copied-save foreground run exercised the empty-context case:
after selecting the live floor tile, a real click at client `(1600,200)` above
the rendered map produced the authoritative empty `Tile 800000` context at
`X 0 Y 0 Z 80`, with no terrain row or context-action buttons, while the map
remained visible. The three retained frames are
`physical-inspector-empty-20260816/empty-before.png`
(`F3A7D4740FBF6C882AE8B14F4D94E1C672D1FB81235BF15043C1FF397DB87377`),
`empty-selected.png`
(`10CB45590B120D3E8495C0DBA6239F714649094C51258FF349722EE3F69AF078`), and
`empty-after-click.png`
(`4F747EF76B2FB53143C4B51C366DD976D34A07D1F62EEECEF156B69CB194A7F3`).
The process exited `0`, the copied `game.json` stayed at the source SHA-256
above, and no Ingnomia process remained. This closes empty-context rendering
for this valid empty tile; it does not claim invalid-world cursor handling or
entity-specific selection.

A third fresh copied-save run exercised the dock/world boundary with real
foreground input: a map click opened the TileInfo dock, a physical Close click
at client `(2175,100)` removed the dock while leaving the live map/HUD
usable, and a second physical map click reopened the same authoritative tile
inspector. The retained frames are
`physical-inspector-ownership-20260816/ownership-inspector-open.png`
(`EB6FB450618AFAE2EF0577728B3A5071CD476C0AF33B607B38FC909A4F9911AD`),
`ownership-after-close.png`
(`8ED56379A53A7EC7681CF03C9746C8AE099298E082922B5148A7DB78FC1BA5A3`), and
`ownership-map-reopened.png`
(`3F05B7E7FC499911C853CAC90333F3056475D0C63F345B5A71311F3570CC36FF`).
The process exited `0` and the copied `game.json` remained the source SHA.
This closes the exercised inspector Close/map click-through case; modal
blocking, entity selection, and tool handoff are still separate gates.

### Row 12 correction after the 2026-08-16 creature-inspector iteration

The original `content/xaml/CreatureInfo.xaml` is a selected-creature detail
view: it shows the creature name and profession, equipment/uniform slots,
attributes, need bars, and activity. The current RmlUi surface already had the
authoritative name/profession/attributes/needs/activity fields, but it dropped
the supported `GuiCreatureInfo::equipment` payload. Its aggregator also kept a
cached gnome equipment/uniform payload when a later monster or animal refresh
was requested, and an unresolved creature ID previously left the old panel
visible with no signal.

The safe correction reuses the existing management slot mapping and adds
compact text equipment rows for non-empty authoritative slots (`Head`,
`Chest`, `Arms`, `Hands`, `Legs`, `Feet`, `Left hand`, `Right hand`, `Back`),
plus a localized empty state. `AggregatorCreatureInfo` now rebuilds the
payload before every kind lookup and emits `signalCreatureCleared()` when the
requested ID no longer resolves. `MainWindow` closes the panel only when the
cleared signal still corresponds to the active Creature inspector, so a late
signal cannot erase a newer tile or workbench selection. No new game-thread
bridge or visual asset was invented.

Focused Inspector CTest passed `3/3` after the change. The production Release
executable is SHA-256
`6ED3161FBA47D5603915E832C8E6E0830642C40BC65C02697B29C75C5A76BEE3`; source
and deployed `inspector.rml` both hash
`F7E524EF29EF986B1D5DAC08B9EC2378F6B011A703C0DDB34A4641951EE464F3`.
In a fresh copied-save foreground run, the existing renderer/EventConnector
diagnostic centered the real map on the recorded gnome position `50 49 92`.
Physical Lower-walls, tile, and `Inspect first creature` clicks produced the
authoritative tile `X 50 Y 49 Z 92`, `Gnome: Fistelvase`, then the creature
panel with `Profession: Gnomad`, live attributes/needs, and
`Right hand | Pickaxe | Pine`. The inspected final framebuffer is
`build-wave8-root-msvc/physical-inspector-creature-20260816b/creature-inspector-final.png`
with SHA-256
`3FB2828D657E2180ECDB857AB5CFEFA84866E99C5AD30DC554B03C806720EE35`; the
authoritative trace is
`build-wave8-root-msvc/physical-inspector-creature-20260816b/creature.trace.log`
with SHA-256
`A7E0FC0B5D59EBB0EB2176982EF4CB939A7C7EE9D1D531B3FCA6C9EE8EA9FB86`.
The copied `game.json` remained equal to the source at SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained after the run.

This closes the supported equipment-display and exercised live entity-route
slice only. That preceding equipment-only pass left the original profession
ComboBox open; the correction below addresses it. Skills, real activity text,
uniform/inventory icon assets, creature actions, an actual
disappearance-triggered physical stale-selection run, and save/reload
persistence remain open. The stale-ID clear is source- and contract-tested but
is not claimed as a physical disappearance proof.

### Row 12 correction after the 2026-08-16 profession-selector iteration

The original `content/xaml/CreatureInfo.xaml` contract has a two-way
`ComboBox`: `ItemsSource="{Binding Professions}"` and
`SelectedItem="{Binding Profession, Mode=TwoWay}"`. The first creature
inspector slice only rendered the selected profession as text, so the highest
safe remaining Row 12 task was to restore that core mutation without creating
a parallel game-state path.

The inspector now requests the existing `AggregatorCreatureInfo::signalProfessionList`
alongside a creature refresh. `InspectorState` retains the authoritative list,
RmlUi renders each live choice with its selected state, and each generated
choice has a listener that dispatches the already-registered typed
`population.set_profession` action. `InspectorQtCommandPort` queues the
existing `AggregatorCreatureInfo::onSetProfession` call and immediately
requests the authoritative creature refresh. Listener teardown covers both
static inspector controls and regenerated profession choices. No new
game-thread bridge, DTO source, icon, or unavailable asset was invented.

Focused Inspector CTest passed `3/3` (controller mutation payload, RML
contract, and queued integration contract). The rebuilt production executable
is SHA-256
`2C19E19C50A91F065636BE9A898598921AC4951D342B5A078A3A109C85A17C85`; source
and deployed `inspector.rml` hash
`52043FC20352954625D3C26CE33CBFE35BF9285C88A1A403192A66B65A3602DF`.

For literal production proof, an isolated copied real save was loaded and the
existing production RmlUi binding activated the live tile `X 50 Y 49 Z 92`,
the rendered `Inspect first creature` control, and the first real profession
choice (`Farmer`). The final framebuffer shows `Fistelvase`, the live
attributes/needs/equipment, and the authoritative change from `Profession:
Gnomad` to `Profession: Farmer`, with Farmer selected:
`build-wave8-root-msvc/physical-inspector-profession-20260816/profession-final-automated2.png`
(SHA-256
`884F743266971ED020A1B60E755AC83D7094F0FEA8EC5FA10568F9C3C9DA8D11`). The
opt-in production trace records explicit load, tile dispatch, creature
activation, and `inspector_profession requested=Farmer activated=true`:
`build-wave8-root-msvc/physical-inspector-profession-20260816/profession-automated2.trace.log`
(SHA-256
`CC303ECA918A403298767B0A0D9107066BDD3334EAEE9580ACC4E6C5DE17BDC5`). The
shell session could not own a physical foreground pointer, so this iteration
does not claim a physical mouse click on the profession button; the evidence
is a visible foreground production framebuffer plus in-process activation of
the actual RmlUi element and queued command path. The source and copied
`game.json` both remain SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the profession-list rendering and authoritative profession-change
slice only. The original equipment doll/inventory icons, skills, real activity
text, creature actions, actual disappearance-triggered stale-selection proof,
and save/reload persistence remain open at this point in the iteration log.
Keep the row open rather than claiming full CreatureInfo parity.

### Row 12 correction after the 2026-08-16 skills projection and row-layout iteration

The audited original `CreatureInfo.xaml` does not bind a skills surface or any
creature command; its 32 bindings cover the name/profession, equipment doll,
eight inventory placeholders, attributes, four needs, and activity. Skills
were an explicit open item in the modernization worklist because the existing
Population surface already owns authoritative skill data, so this iteration is
recorded as a data-rich enhancement rather than a claim of original-screen
parity. The current authoritative source used by Population is the
`SkillGroups` database table plus `Gnome::getSkillLevel()` and
`Gnome::getSkillActive()`; the inspector had no consumer for that data. This
iteration carries only those DB-defined skills through the existing
`AggregatorCreatureInfo` -> Qt adapter -> InspectorController -> RmlUi path.
No skill names, levels, active states, icons, or assets are fabricated.

`GuiCreatureInfo::Skill` now retains the DB skill ID, localized name, level,
and active flag. The adapter projects each skill in database order as a
read-only row, and the Rml row renderer now uses explicit label/detail/count
spans with a single-line flex layout and ellipsis-safe labels. This fixes the
compact panel's prior multi-line `label | level | active` wrapping while
preserving the shared classic-park visual language. An explicit `No skills
reported` state remains for payloads without skills.

The focused Inspector CTest passes `4/4`, including the new skill ordering,
detail, and world-teardown test; the HUD suite remains `6/6`; both Inspector
Rml/source verifiers pass. The production target compiled and staged its
content; the normal `Ingnomia.exe` link remains blocked by the two pre-existing
locked diagnostic processes, so the current objects were linked as
`Ingnomia-event14.exe` with SHA-256
`36A3FB38B5723937521BAA74EA47E654A8E47DF30F70658A4C95A0E0D295073B`.
Source and staged `inspector.rml` hash
`F65597444A1162F67AAC5F480065F6274B2059164BF2A3045CA45D99C5206435` and
`inspector.rcss` hash
`C7D24DBC3CA771B1F0F25943CD050E30CC1592F8AE39FECB42B14D50F51704DE`.

For literal production proof, a fresh copied real save loaded the gnome tile
`924950` at `X 50 Y 49 Z 92`; the existing production Rml activation opened
`tile_open_creature` for `Fistelvase`. The inspected framebuffer visibly shows
the live creature panel and compact skill rows including `Mining | level 5 |
active`, `Masonry | level 1 | active`, and the remaining DB-defined skills:
`build-wave8-root-msvc/physical-inspector-skills-20260816e/creature-skills.png`
(SHA-256
`BB326815FA44B6C85BF223C00C1E68E189291AC0A569A3DF4463FF4EA302C341`). The
single-run production trace records explicit load, tile dispatch, creature
activation, and capture arming at
`build-wave8-root-msvc/physical-inspector-skills-20260816e/creature-skills.trace.log`
(SHA-256
`97BAAE1246D978B7AEE5922533C5142640427B7922A8E8354F95A4839B112D9E`); the
lifecycle trace records clean init, post-creation init, and shutdown at
`build-wave8-root-msvc/physical-inspector-skills-20260816e/creature-skills.lifecycle.log`
(SHA-256
`B275C0F715DBCAE25BB92536CBE4243507AA334080BC3AD392D67394AD9FE7C1`). The
copied `game.json` stayed equal to the source at SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

The hidden-window launch path exited with `0xC0000005` before the timed probe
for both the current and prior production executable; the normal visible
production window reached the route and closed cleanly, so this evidence does
not claim headless/hidden OpenGL support. It also uses in-process Rml activation
rather than a physical pointer click. The copied save retains the known
`settings/userpresets.json` and `monsters.json` parse warnings. This closes the
supported-gnome skill enhancement and readability slice only; original
equipment/inventory icons, non-empty inventory, unsupported
creature framebuffer coverage, physical pointer ownership, actual
disappearance-triggered stale-selection proof, and save/reload persistence
remain open.

The next-ranked non-empty inventory gate was audited against every retained
real-save `gnomes.json` in the workspace. None contains an authoritative
`InventoryItems` payload; the currently usable gnome has equipment but zero
carried-item rows. Assigning an existing item by hand would change the save
rather than prove the production path, so non-empty inventory remains an
external data-availability blocker until a real save with carried items is
available.

### Row 12 correction after the 2026-08-16 activity-source iteration

The original `CreatureInfo.xaml` binds a visible Activity text block to the
creature view-model. The current game source has one authoritative activity
producer for gnomes: `Gnome::getActivity()` returns the active job's required
skill or `idle`. The prior aggregator instead fabricated `Doing something. tbi`
for gnomes, monsters, and animals. This iteration now projects the real gnome
activity and leaves unsupported monster/animal activity empty; the Rml surface
renders a localized explicit unavailable state instead of inventing a value.

Focused Inspector CTest remained `3/3`. The rebuilt production executable is
SHA-256
`9A3D21809132A74E560BDB121AE6717C8114BAC017990A22D25749A9889C74C3`; deployed
`inspector.rml` remains SHA-256
`52043FC20352954625D3C26CE33CBFE35BF9285C88A1A403192A66B65A3602DF`.

An isolated copied-save production run used the same literal live route and
in-process Rml activation as the profession proof. The final framebuffer
shows `Fistelvase` at `X 50 Y 49 Z 92`, `Profession: Farmer`, selected Farmer,
`Activity: Mining`, live needs/attributes, and `Right hand | Pickaxe | Pine`:
`build-wave8-root-msvc/physical-inspector-activity-20260816/activity-final.png`
(SHA-256
`BE01CFC8DC316C3CA16DBB9F99BF816276AB54135A0779545E775342BFBCB398`). The
authoritative trace records the copied-save load, tile dispatch, creature
activation, and profession activation:
`build-wave8-root-msvc/physical-inspector-activity-20260816/activity.trace.log`
(SHA-256
`25AFAF7FAA81DB31A05DE485061C74F49CFAD5C7DB5C05AD35D29B7A49CC07F0`). The
source and copied `game.json` both remain SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained. As with the profession proof, the shell session
could not provide physical foreground pointer ownership; this is a literal
production framebuffer plus in-process Rml activation, not a physical mouse
click claim.

This closes the authoritative gnome-activity projection slice. The original
equipment doll/inventory icons, skills, creature actions, actual
disappearance-triggered stale-selection proof, and save/reload persistence
remain open.

### Row 12 correction after the 2026-08-16 needs-source iteration

The original `CreatureInfo.xaml` binds four visible needs values: Hunger,
Thirst, Sleep, and Happiness. The current source has authoritative values for
all four gnome needs and only hunger for animals; it has no corresponding
monster-needs producer. Before this iteration, `AggregatorCreatureInfo`
fabricated `100` for every monster need and for animal thirst, sleep, and
happiness. That was an incorrect state projection.

The payload now carries one authoritative availability flag per need. Gnomes
mark all four values reported, animals mark only the existing `hunger()` value,
and monsters leave all four unavailable. The inspector formats unavailable
fields as localized `Not reported` text instead of displaying a fabricated
number. The refresh reset also clears the flags across creature-kind changes.

Focused Inspector CTest passed `3/3`, including the Rml and integration
contracts that reject the old `100` assignments and require the explicit
unavailable projection. The rebuilt production executable is SHA-256
`8670A523F955137F905E2AF2936A7FD3D8CBE3A87D69132E639FE398BCE38335`;
deployed `inspector.rml` remains SHA-256
`52043FC20352954625D3C26CE33CBFE35BF9285C88A1A403192A66B65A3602DF`.

An isolated copied-save production run used the literal saved-game route,
selected `Fistelvase` at `X 50 Y 49 Z 92`, and opened the live creature
inspector. The final framebuffer shows the real gnome values `Hunger 68 |
Thirst 68 | Sleep 67 | Happiness 83`, the Farmer selection, `Activity:
Mining`, and `Right hand | Pickaxe | Pine`:
`build-wave8-root-msvc/physical-inspector-needs-20260816/needs-final.png`
(SHA-256
`B8FCCDF9FCFF47C497EE7094A451565F5CD6586C2452738ABCE6F5C7821FF28D`). The
authoritative trace records the copied-save load, center dispatch, tile
dispatch, creature activation, and profession activation:
`build-wave8-root-msvc/physical-inspector-needs-20260816/needs.trace.log`
(SHA-256
`55D2B5FE65B0864EA6EDE7F538DD3F3143E1869170C49E0002DFC5E9AFBD110A`). The
source and copied `game.json` both remain SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained. The copied save contained three gnomes and zero
animals, so this is literal gnome production evidence plus source/contract
coverage for the unavailable monster/animal branches; it does not claim a
monster/animal framebuffer for the localized `Not reported` state.

The population adapter also now passes through any non-empty authoritative
activity string instead of retaining a comparison against the removed
placeholder. This closes the no-fabricated-needs correction and the live gnome
needs/activity consumer slice.
The original equipment doll/inventory icons, skills, creature actions, actual
disappearance-triggered stale-selection proof, and save/reload persistence
remain open.

### Remaining-gap inspection after the needs iteration: original equipment and inventory contract

The original `CreatureInfo.xaml` still requires a materially different
equipment presentation: twelve square image slots bound to `ImgHead`,
`ImgChest`, `ImgBack`, `ImgNeck`, `ImgArms`, `ImgHands`, `ImgLRing`,
`ImgRRing`, `ImgRightHand`, `ImgLeftHand`, `ImgLegs`, and `ImgFeet`, followed
by an eight-cell inventory grid (`11` through `24`). The current Rml surface
has no image slots, but now renders the authoritative carried-inventory
designations as compact text rows and a localized empty state. It renders only
text rows for the nine authoritative `Equipment` fields currently exposed by the
game payload; the original slot/doll presentation remains intentionally absent.

The original producer's former image path referenced DB sprites backed by
`weapons-armour-UI-large.png`. The asset audit records that exact tilesheet as
absent; the available `weapons_armour.png` is a different DB source and is not
a safe visual substitute. No new slot image or inventory data was fabricated
in this iteration. This is therefore an explicit remaining visual/data gap,
not an acceptance claim. The game-thread producer now exposes the existing
`Creature::inventoryItems()` collection through `Inventory::designation()`;
the inspected real save contains zero carried items, so the non-empty row path
has not been visually exercised. The original icon grid remains blocked until
the release owner supplies or approves the exact source asset and the
corresponding slot-level contract. The supported text equipment and empty
inventory states remain bounded production slices, not full CreatureInfo parity.

### Row 12 correction after the 2026-08-16 inventory-state iteration

The original `CreatureInfo.xaml` allocates an `Inventory` block after the
equipment slots, while the migrated Rml panel had no inventory consumer. The
source exposes an authoritative carried-item collection on `Creature` and a
public `Inventory::designation()` formatter; the safe task was therefore to
project those existing designations without fabricating the unavailable icon
sheet or inventing item rows. The payload now records `inventoryReported`,
resets it across creature-kind changes, and maps each non-empty designation to
a compact Rml row. A queried empty collection renders localized `No carried
items`; a missing or unsupported producer cannot be mistaken for an empty
inventory.

Focused Inspector CTest passed `3/3`. The Rml contract verifier passed, and the
integration verifier passed with the authoritative `inventoryItems()` and
`inventoryReported` producer needles. The rebuilt production executable is
SHA-256
`7686A91D3FEC0A638C43F37B886A22C388F894F868E2BD7A43B20F6BCE607E68`.

For literal production evidence, a fresh isolated copy of the real save was
loaded with the desktop-capable runner. The saved gnome at `X 50 Y 49 Z 92`
(`Fistelvase`) opened through the live tile-to-creature Rml route. The final
2048x1024 framebuffer shows the existing live equipment row
`Right hand | Pickaxe | Pine`, followed by `Inventory` and `No carried items`:
`build-wave8-root-msvc/physical-inspector-creature-inventory-20260816c/creature-inventory-empty.png`
(SHA-256
`EB2A8CCA582458278A13A5C5B62969BA3796FAF379A14F002A44F8E153D7AE81`). The
authoritative production trace is
`build-wave8-root-msvc/physical-inspector-creature-inventory-20260816c/creature-inventory.trace.log`
(SHA-256
`207D2499247A2713461B514F505E33BA95526868986E0D0F86E4F1B66087B960`); it
records explicit save load, map centering, tile `924950`, successful
`tile_open_creature` activation, and capture arming. The lifecycle trace
records `init` and `postCreationInit` for that run and hashes to
`8FF60FF2F192A68A8BE0FF015AFAF359D544848DDE971F25FC5BFF881A460`.
The copied `game.json` remained equal to the source at SHA-256
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`.

The copied save has three gnomes and zero carried-inventory entries, so this is
literal production proof of the authoritative empty state only. The non-empty
designation row path remains source/test-backed until a real saved creature
with carried items is available; the twelve original image slots, eight-cell
inventory grid, skills, creature actions, actual disappearance proof, and
save/reload persistence remain open. Two earlier sandbox-launched diagnostics
were discarded because they had no window, trace, or framebuffer; the final
desktop-capable run above is the accepted runtime evidence. Two older stalled
diagnostic processes from those failed launches remain outside this run and
were not force-terminated.

### Row 20 correction after the 2026-08-16 event-prompt iteration

The original `GameGui.xaml` `LayoutMessage` contract is a centered, blocking
message surface with a title, body, and either an `Ok` button or a `Yes`/`No`
pair. `GameModel::eventMessage` pauses the game when requested, and
`onMessageButtonCmd` answers the event before dequeuing the next message. The
current Rml route already had typed FIFO prompts and target validation, but its
modal lacked the original dialog semantics and focus projection.

The safe task added `role="dialog"`, `aria-modal`, labelled-by/described-by
relationships, an explicit `aria-hidden` projection, and focus of the first
actionable button when a new prompt instance arrives. The existing typed
`HudController`/`HudQtCommandPort`/`EventConnector` route and pause request were
preserved. HUD CTest passed `5/5`; `verify-hud-rml.cmake` and
`verify-hud-integration.cmake` passed.

Literal production evidence used a fresh copy of the real save. The opt-in
probe created the existing `EventMigration` object through
`EventManager::onDebugEvent`, read its DB-backed title/body/amount, and used
the same `EventConnector::onEvent` handoff while leaving the event in
`EventManager` for the real `onAnswer` path. The prompt framebuffer shows
`New Gnomes`, the DB message, Yes/No, and the dimmed paused world:
`build-wave8-root-msvc/physical-hud-event-prompt-20260816h/event-prompt.png`
(SHA-256
`A910A6C17AAAD4A2BB7F3260CA7BB44CF73592DCC877BB395F6E269959D6EA2F`). The
production Rml activation trace records `event_prompt_db_handoff=true`,
`event_prompt_yes activated=true`, and both capture arms; its SHA-256 is
`2EACB591EB6EAB10134B2A6ED98D69FD4C6ED2F264EBB37429F4CABE1B45824D`.

The post-Yes production framebuffer has no modal and the HUD population has
increased from 3 to 4 gnomes, proving the response reached the existing event
execution path:
`build-wave8-root-msvc/physical-hud-event-prompt-20260816h/event-after-response.png`
(SHA-256
`82A12B9B03A79F30E5BE9634393EB1A9187952656E2981B111DCED341D566700`). The
run completed orderly init/post-init and end-current-game lifecycle phases;
the lifecycle trace SHA-256 is
`23024C12665BF3B302C558A7562EE77367CFE25023CDB5446E365E4DF3920C21`, and the
copied input `game.json` remained
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`.
The current source objects were Release-compiled and linked as the alternate
diagnostic executable `Ingnomia-event8.exe` because the original
`Ingnomia.exe` remained locked by two older no-window diagnostics; the
alternate executable SHA-256 is
`88AF172F80CCC75CC317BF16FD9A7B04BD9E07FECACDC88CE9EAE3D3B239AD6D`.

The follow-up HUD-only geometry override restores relative positioning after
the shared absolute `.c-modal` rule, and restores the original 500x300 bound
without changing the shared component contract. After running the real
`stage_content` target, the centered production prompt is visible in
`build-wave8-root-msvc/physical-hud-event-prompt-20260816j/event-prompt.png`
(SHA-256
`1160F469CE5A03C565B50817727A1B20D126302B408098FDC4F56FC7C4573F61`). The
post-Yes frame remains modal-free with four gnomes at
`build-wave8-root-msvc/physical-hud-event-prompt-20260816j/event-after-response.png`
(SHA-256
`9093255B4ACF2803CFA32D313A05CFC5541CB98F61643DC79423A0D7418E3ACF`), and
the staged-run trace hashes to
`9E800FFEDE059D1F2C167185F044249B1D3D6BA5EBD168331AA44D876D984425`.

This closes the bounded prompt/response, accessibility, and modal-geometry
slice, not Row 20: physical pointer ownership, every supported event source,
multiple-prompt ordering, acknowledge-only events, and event save/load remain
open. The DB-handoff probe is explicitly not scheduler-breadth evidence.

### Row 20 correction after the 2026-08-16 acknowledge-only event iteration

The original `GameModel::onMessageButtonCmd` sends an answer only for Yes/No;
an `ok` message simply dismisses the visible message and advances the queued
message list. The current typed controller already models this as
`EventResponseKind::Acknowledge`, validates that Continue cannot answer a
Yes/No prompt, and removes the prompt through the same modal action route.
The focused controller test covered that branch, but no literal production
event had exercised it.

The opt-in probe now uses the existing DB-backed `EventInvasion` success
message (`Invasion`, `A force of 1 goblin has arrived.`) with `yesNo=false`
and event id zero, then activates the live `hud_event_ack` element. The real
frame shows the centered modal with only `Continue` focused:
`build-wave8-root-msvc/physical-hud-event-ack-20260816k/event-ack.png`
(SHA-256
`39CEA5B05B4296A1922AA8168A9827CECEB9215472EDAE946E5F1B2F6A2C89C8`). The
post-Continue frame is modal-free and still shows the unchanged three-gnome
world:
`build-wave8-root-msvc/physical-hud-event-ack-20260816k/event-ack-after.png`
(SHA-256
`C54DA580B84B3BBB65EA7A1DEF4B4D092386720CFCACCF5BBDF97E3BEA89BC27`). The
acknowledge trace records `kind=ack`, `event_id=0`, and
`event_prompt_response element=hud_event_ack activated=true`; its SHA-256 is
`E8E07CE3E53BAA8F0A1F4C9E59B514EE82BC87D7D462C4384CC3D15A02B8088A`.
The copied `game.json` remained
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and
the lifecycle trace hashes to
`8EA6C32F2CD69AD902B17EBC85A65C411881033C45B504235F6FD4C64B929422`.

This closes the literal acknowledge-only slice. It does not close every
source or queue gate: the probe uses the DB-generated message and existing
EventConnector handoff rather than claiming full scheduler coverage; physical
pointer ownership, multiple-prompt ordering, and event save/load remain open.

### Row 20 correction after the 2026-08-16 FIFO event iteration

The original `GameModel::eventMessage` appends a new message to the pending
message list, while `onMessageButtonCmd` answers or dismisses only the visible
message and then calls `layoutMessage` for the next queued item. The current
`HudController` uses the same front-pop discipline: response-type mismatches do
not dispatch or remove the front prompt, and a valid response exposes the next
prompt through the existing `HudRmlBinding` route.

The safe task added `tests/ui-hud/fifo_tests.cpp` and the `ui_hud_fifo` CTest
target. It asserts two prompt registrations, front ordering, invalid
response gating for both prompt kinds, valid Yes removal, and subsequent
acknowledge removal. The focused controller/action set passed `4/4` after the
target was reconfigured and built with the Visual Studio environment.

The opt-in production probe now enqueues the authoritative DB-backed
`EventMigration` question followed by the DB-backed `EventInvasion`
acknowledge-only message through `EventConnector::onEvent`, then activates the
live Rml elements in sequence. The final clean trace records, in order,
`kind=yesno event_id=1003313 title=New Gnomes`, `kind=ack event_id=0
title=Invasion`, `hud_event_yes activated=true`, and
`hud_event_ack activated=true`; the trace is
`build-wave8-root-msvc/physical-hud-event-queue-20260816n/event-queue.trace.log`
(SHA-256
`EC2EC3663C735326728AD50A6D1C507227B2E7DA9FED9FA00AA075573865E34A`). The
centered first frame shows `New Gnomes` with focused Yes/No,
`event-queue-first.png` (SHA-256
`4777B6BD18E510EA996D175840573DB3DAB832ADBC12DC027B7AE48D21F36437`); the
second frame shows the next `Invasion` prompt with only focused Continue,
`event-queue-second.png` (SHA-256
`D63F222711801A34102540246F6556E406AB49287181B8BD8FCA7EA43968E874`); and
the final frame is modal-free after Continue,
`event-queue-after.png` (SHA-256
`3B76D6BD6F027FA205EE3277295CE17CFF89A2CE9CF4B21F201B2EDBE6BE8B75`). The
alternate production executable is
`Ingnomia-event12.exe` (SHA-256
`3F51D25E90F57FE2B7B264D6C8D8F0E4EAB48D2FEDF30B1ABC82021BF53B4841`); the
copied authoritative `game.json` is unchanged at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and
the lifecycle trace records clean game teardown
(`event-queue.lifecycle.log`, SHA-256
`2A9C27610B75CE6ADF8A29BDA14C4547188311652045EC87F759B4F79F38BE49`).

This closes FIFO order and response gating for the two exercised DB-backed
sources. It does not close every event producer, repeated dismissal variants,
physical pointer ownership, event persistence, or scheduler breadth. The run
also retains two inherited copied-save parse warnings for missing
`settings/userpresets.json` and malformed `save/.../monsters.json`; the valid
HUD frames and clean lifecycle make the prompt evidence usable, but those
warnings limit claims about unrelated world-data fidelity. The probe uses
in-process Rml activation rather than a physical pointer claim.

The follow-up source audit found three scheduler event types in the current
`EventManager`: Trader, Migration, and Invasion. Migration and Invasion now
have literal DB-backed prompt evidence. Trader is not silently treated as
covered: its success notification is gated by the authoritative
`FreeMarketStall` requirement, and the copied real save used for these probes
contains no assigned `MarketStall` workshop in `workshops.json`. The existing
debug route only schedules that event and does not manufacture the missing
world state. Trader prompt evidence therefore remains open until a discovered
real save supplies that requirement or the production scheduler naturally
reaches it; no asset, workshop, or event result is fabricated.

### Row 11 correction after the 2026-08-16 TileInfo Manage iteration

The original `TileInfo.xaml` exposes `CmdManage` from both the simple
designation panel and the mini-stockpile panel. The current Rml inspector
exposes the corresponding `tile_manage` action from its contextual-action
section. The production bridge routes that typed `TileContextAction::Manage`
through `InspectorQtCommandPort` to the existing queued
`EventConnector::onManageCommand` path; no parallel management command or
game-thread access was added.

The opt-in production verification seam now accepts
`INGNOMIA_AUTOMATE_INSPECTOR_ELEMENT=tile_manage` after selecting an explicit
tile. Normal launches are unchanged. Against the isolated copied real save,
tile `1004550` (`X 50 Y 45 Z 100`) is the saved `crude workbench` workshop. The
production run dispatched the copied-save load and tile selection, activated
`tile_manage=true`, and the final framebuffer visibly shows the authoritative
`Production workshop / crude workbench` surface with its live craft catalog,
production queue, priority, suspend, generated-item, and order controls:
`build-wave8-root-msvc/physical-inspector-manage-20260816/manage-final.png`
(SHA-256
`A06549C55CCFA0CB649004FA836FC779EB140B13D66A728712D649FBC85EAA1E`). The
authoritative trace is
`build-wave8-root-msvc/physical-inspector-manage-20260816/manage.trace.log`
(SHA-256
`42BE2F1DDAFB22F4656003BF57A4325CBEFFD33AD9A1823640C33BC58E9B7B76`). The
rebuilt production executable is SHA-256
`BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`.
Inspector CTest passed `3/3`; the Management 6B and Inspector integration
contracts passed; the copied `game.json` remained equal to the source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the live Manage route for the saved workshop through the
authoritative production bridge. The action was activated in-process because
the shell session could not provide physical foreground pointer ownership;
this is literal production framebuffer and queued-action evidence, not a
physical mouse-click claim. Physical Manage input, Mine/Fell/Remove plant and
Harvest action coverage, terrain placement/cancel, and save/reload persistence
remain open.

### Row 11 correction after the 2026-08-16 TileInfo Mine iteration

The original `TileInfo.xaml` terrain template exposes the visible terrain
actions through `CmdTerrain`; the current Rml inspector exposes the equivalent
`tile_mine` action from the typed tile context state. The production bridge
routes `TileContextAction::Mine` through `InspectorQtCommandPort` to the
existing queued `EventConnector::onTerrainCommand`, which adds the normal
`Mine` job at the selected `Position`. No parallel command path or direct
game-thread access was added.

The same opt-in production verification seam selected real tile `15050`
(`X 50 Y 50 Z 1`) from the isolated copied save and activated
`tile_mine=true`. The final production framebuffer visibly shows the live
terrain payload (`Wall: basalt stone`, `Floor: basalt stone`, and
`Embedded: platinum metal`) followed by the authoritative active `Mine` job,
including unassigned worker, priority, Mining skill, and Pickaxe level 3
requirement: `build-wave8-root-msvc/physical-inspector-mine-20260816/mine-final.png`
(SHA-256
`A9FA4B495A06F7D44FC6778C02AE272ED7D781BCA258A8C659F7B76EFC9D4E12`). The
authoritative dispatch trace is
`build-wave8-root-msvc/physical-inspector-mine-20260816/mine.trace.log`
(SHA-256
`8F9AEE02E4972A2B86FAC1DC8225CB6F16CA2493D9464C55F8DAA8F50F0983C4`). The
same rebuilt production executable is SHA-256
`BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`.
Inspector CTest passed `3/3`; the Management 6B and Inspector integration
contracts passed; the copied `game.json` remained equal to the source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the live Mine branch for a real solid-wall tile and its queued-job
result. Activation was in-process because the shell session could not provide
physical foreground pointer ownership, so physical Mine input remains open.
Fell, Remove plant, and Harvest still need the same real production-action
matrix, alongside terrain placement/cancel and save/reload persistence.

### Row 11 correction after the 2026-08-16 TileInfo Fell iteration

The original `TileInfo.xaml` terrain template exposes tree actions through the
same visible `CmdTerrain` action surface; the current Rml inspector exposes
`tile_fell` only when the authoritative tile payload reports a tree. The
production bridge routes `TileContextAction::FellTree` through the existing
queued `EventConnector::onTerrainCommand` mapping to the normal `Fell` job.

The isolated copied real save contains an oak tree at tile `1000236`
(`X 36 Y 2 Z 100`). The production run selected that tile, activated
`tile_fell=true`, and the final framebuffer visibly shows `Plant: oak tree`
and the resulting authoritative active `FellTree` job with unassigned worker,
priority, Woodcutting skill, and axe level 1 requirement:
`build-wave8-root-msvc/physical-inspector-fell-20260816/fell-final.png`
(SHA-256
`6F390A3291B0955FFC3620ACCCB40B47E623C4BF6F1927D7E5B533871FEF6078`). The
authoritative dispatch trace is
`build-wave8-root-msvc/physical-inspector-fell-20260816/fell.trace.log`
(SHA-256
`9856D91A1888917CC3DA10F47230E3D4A5F0D5510FFC77F698CE390B105C2F34`). The
rebuilt production executable remains SHA-256
`BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`.
Inspector CTest passed `3/3`; the Management 6B and Inspector integration
contracts passed; the copied `game.json` remained equal to the source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the live Fell branch for a real tree and its queued-job result.
Activation was in-process because the shell session could not provide physical
foreground pointer ownership. Remove plant and Harvest still need real
production-action coverage, in addition to physical input, terrain
placement/cancel, and save/reload persistence.

### Row 11 correction after the 2026-08-16 TileInfo Remove Plant iteration

The original `TileInfo.xaml` terrain template uses the same `CmdTerrain`
command surface for plant removal; the current Rml inspector exposes
`tile_remove_plant` only when the authoritative payload reports a non-tree
plant without a current job. The production bridge routes
`TileContextAction::RemovePlant` through the existing queued
`EventConnector::onTerrainCommand` mapping to the normal `RemovePlant` job.

The isolated copied real save contains a non-tree, non-harvestable mushroom at
tile `600470` (`X 70 Y 4 Z 60`). The production run selected that tile,
activated `tile_remove_plant=true`, and the final framebuffer visibly shows the
live plant payload plus the authoritative active `RemovePlant` job with
unassigned worker, priority, Farming skill, and tool requirement:
`build-wave8-root-msvc/physical-inspector-remove-plant-20260816/remove-plant-final.png`
(SHA-256
`7148F999ED2018B0F413FAB650829CCCB84A219F614CB73A58F48A873E84EFD4`). The
authoritative dispatch trace is
`build-wave8-root-msvc/physical-inspector-remove-plant-20260816/remove-plant.trace.log`
(SHA-256
`AAA0239EC86FE6D014B7B78DECB0BA3C890379E87C0551D62CB1E7281FADECF8`). The
rebuilt production executable remains SHA-256
`BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`.
Inspector CTest passed `3/3`; the Management 6B and Inspector integration
contracts passed; the copied `game.json` remained equal to the source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

The source save labels this mushroom as `error: unset plant designation`; that
is retained as observed producer data and was not replaced with an invented
asset or label. This closes the live Remove plant branch and queued-job result
for a real non-tree plant. Activation was in-process because the shell session
could not provide physical foreground pointer ownership. Harvest still needs a
real production-action result, in addition to physical input, terrain
placement/cancel, and save/reload persistence.

### Row 11 correction after the 2026-08-16 TileInfo Harvest iteration

The original `TileInfo.xaml` terrain template exposes harvest through
`CmdTerrain`; the current Rml inspector exposes `tile_harvest` when the
authoritative plant payload is harvestable and no job is present. The
production bridge routes `TileContextAction::Harvest` through the existing
queued `EventConnector::onTerrainCommand` mapping to the normal `Harvest` job
for non-tree plants.

The isolated copied real save contains a harvestable blackberry plant at tile
`1000326` (`X 26 Y 3 Z 100`). The production run selected that tile, activated
`tile_harvest=true`, and the final framebuffer visibly shows `Plant: blackberry
plant` and the authoritative active `Harvest` job with unassigned worker,
priority, Farming skill, and tool availability:
`build-wave8-root-msvc/physical-inspector-harvest-20260816/harvest-final.png`
(SHA-256
`A6C160FF5B166FFDB5B7ABDB64FC2BE9E49051C4035A4F0093727052A85E577F`). The
authoritative dispatch trace is
`build-wave8-root-msvc/physical-inspector-harvest-20260816/harvest.trace.log`
(SHA-256
`BDADB6D013FC0BFDD61E7AAA60E78D24D23802870BA57C7622F40DE7D548DF28`). The
rebuilt production executable remains SHA-256
`BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`.
Inspector CTest passed `3/3`; the Management 6B and Inspector integration
contracts passed; the copied `game.json` remained equal to the source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the live Harvest branch for a real non-tree plant and its
queued-job result. Activation was in-process because the shell session could
not provide physical foreground pointer ownership. The terrain action matrix
now has production evidence for Remove floor, Mine, Fell, Remove plant, and
Harvest; physical pointer ownership, terrain placement/cancel, actual job
completion/disappearance, and save/reload persistence remain open.

### Row 11 correction after the 2026-08-16 TileInfo job-field iteration

The original `TileInfo.xaml` active-job panel binds both `WorkablePosition` and
the `RequiredItems` collection. The current `GuiTileInfo` producer already
populated those authoritative fields, but the Rml `TileInspectorState` and
binding dropped them. The bounded fix carries both fields through the existing
DTO adapter and renders them inside the compact active-job panel; no producer,
EventConnector, or game-thread contract changed.

Focused Inspector CTest passed `3/3`, including the new Rml IDs and source
contract checks for `requiredItems` and `workPositions`. Release rebuilt with
production executable SHA-256
`739EF4DD212A3F8587B32F05A300C40334C7796846D99279AB234BE87DA9E469`.
Against an isolated copy of the real save, a Mine job at tile `15050`
(`X 50 Y 50 Z 1`) visibly renders `Work positions: false` in
`build-wave8-root-msvc/physical-inspector-job-fields-20260816/job-fields-final.png`
(SHA-256
`AE9E50C4B78A72C7A7862FDCFCA2B3B5C8166AD12A263BE2267887323D767E74`). The
same production run's dispatch trace is
`build-wave8-root-msvc/physical-inspector-job-fields-20260816/job-fields.trace.log`
(SHA-256
`27A34112264220FE96852E1A118D46D7B3EFEAF3E09306634A42C4FD4F815B43`). A
second real workshop tile `1004550` (`X 50 Y 45 Z 100`) visibly renders
`Work positions: true` and `Required items: RawWood | any x1` for the live
`CraftAtWorkshop` job in
`build-wave8-root-msvc/physical-inspector-job-required-items-20260816/job-required-items-final.png`
(SHA-256
`C61A630207ECE4A98567C359A57DEBE63E10DDD382556210764525F4B52C688B`). Its
trace is
`build-wave8-root-msvc/physical-inspector-job-required-items-20260816/job-required-items.trace.log`
(SHA-256
`325EB94492792030E5B1BE9BFFDB55CD20A416213FE7817824D990110D7BDC0`). Both
production runs exited cleanly; the copied `game.json` remained equal to the
source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the original TileInfo active-job information gap for both empty and
non-empty authoritative required-item payloads. The action matrix's physical
pointer, completion/disappearance, placement/cancel, and save/reload gates
remain open.

### Row 11 correction after the 2026-08-16 TileInfo cancel-state iteration

The original `TileInfo.xaml` active-job panel is a stateful projection of the
job payload; the migrated Rml surface adds the existing Cancel job action to
that panel. The queued `TileContextAction::CancelJob` path was exercised
against the same authoritative tile state without adding a direct game-thread
command.

Using an isolated copy of the real saved world, the production run selected
solid-wall tile `15050` (`X 50 Y 50 Z 1`), activated `tile_mine`, then activated
the existing `tile_cancel_job` binding through an opt-in secondary diagnostic
element. The final framebuffer shows the active-job section gone and the
authoritative `Mine wall` context action restored:
`build-wave8-root-msvc/physical-inspector-job-cancel-20260816/job-cancel-final.png`
(SHA-256
`42691F776D992495BE892E3E499D541F99C12853275F21295EC4984B4B814F1B`). The
trace records both queued UI activations and hashes to
`BEBE1BCA257827862D81940040F94F2E66FA732D2FD00AF56654B6C9DAACE7FD`. The
production executable is SHA-256
`DDA9A1F15F85FAF519F0BD67BF37F57E63C7AFB6538E21D630EE83CB1DC3D9D5`.
The copied `game.json` remained equal to the source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, the
process exited cleanly, and the focused Inspector contracts remain required
for the final source state.

This closes the queued cancel/state-refresh case for a real terrain job. The
secondary activation is verification-only and normal launches are unchanged;
physical pointer ownership, priority controls, job completion/disappearance,
placement/cancel, and save/reload persistence remain open.

### Row 11 correction after the 2026-08-16 TileInfo priority-refresh iteration

The original `TileInfo.xaml` active-job panel binds the authoritative priority
value. A production probe found that the migrated `Raise priority` action did
mutate the job but left the Rml panel showing the stale value because
`EventConnector::onTerrainCommand` did not refresh `AggregatorTileInfo` after
priority/cancel operations. The fix refreshes the existing tile aggregator on
the same queued game-thread path after `CancelJob`, `RaisePrio`, and
`LowerPrio`; no parallel job manager or GUI state was introduced.

Focused Inspector CTest passed `3/3`; the Management 6B and Inspector
integration contracts passed; Release rebuilt with production executable
SHA-256
`7A808D0799C9B3362E43F7FF963C50EBF6D2C54E3F5E6D18F319CE5FE205EDC2`.
Against an isolated copied real save, tile `15050` (`X 50 Y 50 Z 1`) was given
a live Mine job and then raised once through the existing Rml action. The
final production framebuffer visibly shows `priority 1` in the authoritative
active-job row:
`build-wave8-root-msvc/physical-inspector-job-priority-fixed-20260816/job-priority-fixed-final.png`
(SHA-256
`D090846AAC876D27D1E07BD2155534749C7F6FB5352FF6FE340C1A36D1AC7027`). The
dispatch trace is
`build-wave8-root-msvc/physical-inspector-job-priority-fixed-20260816/job-priority-fixed.trace.log`
(SHA-256
`2EECC375F6861E803C46A312D915B1CE486B344B9216007FE2B8AC74BCE9A904`). The
copied `game.json` remained equal to the source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the raised-priority state-refresh case for a real terrain job.
Only the upper-boundary/lower-priority cases, physical pointer ownership,
completion/disappearance, placement/cancel, and save/reload persistence remain
open for this job panel.

### Row 10 correction after the 2026-08-16 explicit Build-action iteration

The original `BuildItemTemplate` in `content/xaml/styles/mainmenu/styles.xaml`
exposes three separate commands for each build row: `Fill Hole`, `Replace`, and
`Build`. The migrated Rml catalog previously rendered the two terrain-specific
buttons but relied on the thumbnail card as an implicit Build command. That was
functionally reachable but did not preserve the original information hierarchy
or explicit action affordance.

The catalog now emits a localized explicit `Build` button for every authoritative
row, carries `data-build-action='Build'` through the existing delegated Rml
listener, and maps it to the existing typed `BuildAction::Build` and queued
`EventConnector::onCmdBuild` path. The compact action row now wraps so all three
terrain controls remain visible at the accepted panel width; no new domain or
game-thread route was introduced.

The final Release executable is SHA-256
`CF4F21632EFE62032BB0018421508786734CC37F0156AAADBDB20B8B447350CA`. The
focused HUD matrix passed `5/5`, the HUD Rml contract passed after the final
RCSS change, and the isolated copied-save production trace records
`hud_build_fill_hole dispatched=true`,
`hud_build_replace dispatched=true`, and
`hud_build_build dispatched=true`:
`build-wave8-root-msvc/physical-build-explicit-build-20260816e/explicit-build.trace.log`
(SHA-256
`F4CC658CE618FE3C9935F15B77A6B934E4D6862BA753D2E4E26D475D35A4AE0F`). The
retained framebuffer visibly shows the live Walls catalog, Palisade/Plank/log
rows, complete `Fill hole`/`Replace`/`Build` controls, and `Active tool:
BuildWall` after the explicit Build activation:
`build-wave8-root-msvc/physical-build-explicit-build-20260816e/explicit-build.png`
(SHA-256
`2093110E83DE989ADB478938C4C7F5AF45340F9B75E2E7AA665C64948308A37B`). The
21-file copied save's `game.json` remained equal to the source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the explicit action-affordance gap for the Build row. Physical
pointer activation of the new button, second-component material variation,
placement commit, save/reload, and broader DB-category mutation remain open.

### Row 10 data-availability audit after the 2026-08-16 explicit Build-action iteration

The original `BuildItemTemplate` and the current Rml catalog both represent
each required component by index. The copied real save contains 594 item
records. The authoritative `Workshops_Components` rows for `Crude` in
`content/db/ingnomia.db.sql` define component 0 as `RawWood` and component 1
as `RawStone`; the save contains 88 `RawWood` records with
`AppleWood`, `Oak`, `OrangeWood`, and `Pine`, but 43 `RawStone` records with
only `Sandstone`. The other multi-component workshop rows inspected in the
same save either have no matching inventory instances or only one observed
material for the matching item.

This is evidence that the per-component selector and typed material route are
present, not evidence of second-component variation. No artificial item,
material, inventory record, or fixture was added. The second-component gate
therefore remains open until a real generated or saved world supplies at least
two authoritative materials for a later component; terrain-category coverage,
placement validation/commit, and save/reload remain open independently.

### Row 11 correction after the 2026-08-16 TileInfo job-completion iteration

The original `TileInfo.xaml` active-job panel binds the live job until the job
is completed or removed; the migrated Rml panel now exposes the same job fields,
queued cancel, and priority refresh. The next production check used the real
worked Mine job `1001919` at `X 53 Y 50 Z 92`, with producer data identifying
worker gnome `1001853` at the possible work position `53 49 92`.

Before/after frame capture was taken from an isolated copied real save using the
literal TileInfo inspector. The before frame
`build-wave8-root-msvc/physical-inspector-job-completion-20260816/job-completion-before.png`
(SHA-256
`F3395B73925EC161149D8D2F0BF89E0A52E6EF8F20933105A071C5D7D3875DDD`) and the
after frame
`build-wave8-root-msvc/physical-inspector-job-completion-20260816/job-completion-after.png`
(SHA-256
`8949AF8EC9262C2213422DCA3F6185375A93FEDFF156045CB37C7E390857401B`) both
show the same active Mine job, worker, priority, skill, tool, and
`RequiredToolAvailable=No` state after the additional 30 seconds. The trace is
`build-wave8-root-msvc/physical-inspector-job-completion-20260816/job-completion.trace.log`
(SHA-256
`875D7F44AA573486B78E258464E84A239E89495FBF4C03A9086C04C9276B2AE8`). The
copied `game.json` remained equal to source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, the
Release executable used was SHA-256
`7A808D0799C9B3362E43F7FF963C50EBF6D2C54E3F5E6D18F319CE5FE205EDC2`, and no
process remained.

This is not completion/disappearance proof. The authoritative job producer
reported `RequiredToolAvailable=No` even though the gnome payload showed a held
Pickaxe, so changing the GUI to force completion or to hide the job would
fabricate simulation state. The bounded result is recorded as a simulation/data
dependency: completion must be re-run with an actually available authoritative
tool or after the game-state discrepancy is resolved. Physical pointer,
placement/cancel, and persistence gates remain open.

### Row 11 correction after the 2026-08-16 priority-boundary iteration

The original `TileInfo.xaml` binds the active job's authoritative
`JobPriority`; the migrated Rml panel also exposes the existing Raise/Lower
actions. The game contract in `src/game/job.cpp` clamps priority to 0 through 9.
The TileInfo producer now carries authoritative `canRaisePriority` and
`canLowerPriority` flags through the existing Qt adapter. The Rml projection
sets native `disabled` and `aria-disabled` state, and the verification-only
element activator refuses disabled controls, so the UI cannot present a
lower-priority action at priority 0 or silently dispatch it through the
EventConnector path.

Inspector CTest passed `3/3`; the Rml and integration contracts passed; the
final Release executable is SHA-256
`EC689AD2CD02A602574113ED1D98E394C7D2FD76F48FFFF3F8BC06042FBDC9E5`.
Against the isolated copied real save, the production trace records the
normal `tile_mine` activation and then
`inspector_element_secondary requested=tile_lower_job activated=false`:
`build-wave8-root-msvc/physical-inspector-priority-boundary-20260816/priority-boundary-gl4.trace.log`
(SHA-256
`FFFF8FA53886A2CBD1558E692DF1F52926C3074002C97DC9F2F55671DED909EB`). The
post-composite GL framebuffer visibly shows the live map, tile `15050`, an
active Mine job at priority `0`, and the disabled lower-priority control:
`build-wave8-root-msvc/physical-inspector-priority-boundary-20260816/priority-boundary-gl4.png`
(SHA-256
`D998145C0FD45E216A5BFC9D302983B77566D84B5183576BC3BA33D98E8619AB`). The
copied `game.json` remained equal to its source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the implementation and literal lower-bound behavior for a real
priority-0 job. A real priority-9 production frame, physical pointer proof,
completion/disappearance, placement/cancel, and save/reload remain open; the
upper bound is source-backed but not claimed as runtime-proven.

### Row 11 correction after the 2026-08-16 priority-upper-bound iteration

The original `TileInfo.xaml` continues to bind the authoritative
`JobPriority`; the current Rml action controls now project both engine bounds.
Using the same copied real save and the existing queued terrain route, the
production probe created Mine at tile `15050`, dispatched nine real
`tile_raise_job` activations, and reached priority `9`. A tenth attempted
activation was rejected because the projected Raise control was disabled.

The final trace records `inspector_raise index=1` through `index=9` as
`activated=true` and `inspector_raise index=10 activated=false`:
`build-wave8-root-msvc/physical-inspector-priority-boundary-20260816/priority-boundary-upper-final.trace.log`
(SHA-256
`205D6C4DED4EAB496F27C6015E69E4AE628C6056F44C6136564ABD3C83422ABC`). The
post-composite GL framebuffer visibly shows the live map, tile `15050`, the
active Mine job at priority `9`, and the disabled Raise action:
`build-wave8-root-msvc/physical-inspector-priority-boundary-20260816/priority-boundary-upper-final.png`
(SHA-256
`4921FE828D4972AFF0C3CC23D67BB0FB3CAA12C96544EE9FF359884B3C11688F`). The
same final Release executable is SHA-256
`E4BEE7153EB3E4E4151792592BF52A0C3CFE1FC5200816D47131855FB9C3E824`; the
copied `game.json` remained equal to source at
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained.

This closes the source-backed and literal production priority-boundary slice
for both 0 and 9. The probe remains in-process Rml activation rather than a
physical pointer claim. Physical pointer ownership, completion/disappearance,
placement/cancel, and broader inspector persistence remain open.

### Row 11 correction after the 2026-08-16 priority-persistence iteration

The original `TileInfo.xaml` binds `JobPriority`; `Job::serialize()` writes the
same authoritative `Priority` field and the loader restores it. The existing
Rml Raise action therefore needed a production save/reload check, not a second
state path. The new opt-in probe freezes the copied real world, opens the live
tile `15050` (`X 50 Y 50 Z 1`), activates the real `Mine` and `Raise priority`
controls, saves through `EventConnector::onSaveGame`, ends through
`GameManager::endCurrentGame`, and reloads the generated slot 2 through
`EventConnector::onLoadGame`.

The input slot contained 35 jobs and no job at `50 50 1`. The production trace
records `persistence_priority_mine activated=true`,
`persistence_priority_raise activated=true`, then the normal save/end/reload
dispatches:
`build-wave8-root-msvc/physical-inspector-priority-persistence-20260816/priority-persistence.trace.log`
(SHA-256
`223D1E9F146098A64F9F7400EA74175FC936F3A0F91725BD1BB70F54E37A9D39`). The
saved slot 2 contains the new real `Mine` job at `50 50 1` with `Priority: 1`,
`JobIsWorked: false`, and `RequiredTool: Pickaxe`; the reloaded production
frame visibly shows Tile 15050, the active Mine job, and `priority 1`:
`build-wave8-root-msvc/physical-inspector-priority-persistence-20260816/priority-persistence-reloaded.png`
(SHA-256
`8FD74946143A6A284A4E006E64ED1627ED67F7452EE904243F61D2ED069652C1`). The
lifecycle trace is
`build-wave8-root-msvc/physical-inspector-priority-persistence-20260816/priority-persistence.lifecycle.log`
(SHA-256
`31F90CA5700DD991CCBEB259202D3F9C9014A9C21F26815C5B4850717F9AB62D`), the
Release executable is SHA-256
`476BA043102DE73C8E3D9B8D819FEF836CA69A90726ACA9EF89BB6F6CA3BB7F5`, the
copied input `game.json` remained
`60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no
Ingnomia process remained. Inspector CTest passed `3/3`; both static inspector
contracts passed.

This closes save/reload for this real inspector priority mutation only. It does
not close physical pointer ownership, completion/disappearance, placement/cancel,
selection changes, or persistence of the other inspector and management
surfaces.

## Cross-section backlog

These apply after individual rows are reviewed, not as substitutes for them:

1. **World-art completeness:** the 17 available DB-referenced sheets are staged
   and the remaining eight names are explicitly documented as unavailable in
   `content/tilesheet/ASSET_PROVENANCE.md`; verify a discovered daytime save/new
   world at native resolution and obtain release-owner confirmation before
   packaging.
2. **Input matrix:** physical mouse/keyboard, wheel, drag crossing, right-click
   cancel, Escape ordering, text input/IME, clipboard, DPI, and focus loss.
3. **Persistence:** new world, save, load, reload, return-to-title, and repeated
   world replacement with no stale DTOs or pending actions.
4. **Visual matrix:** 1280x720 through ultrawide, 80–200% scale, long strings,
   high contrast, reduced motion, modal focus, and inspected frame hashes.
5. **Package cleanup:** only after the rows above pass, remove the recoverable
   legacy quarantine, reconcile registries/assets/notices, and run the complete
   case-insensitive legacy audit.

## Suggested review order

1. Close rows **9–10** (HUD/build/tools), because every later inspector and
   management review depends on reliable world/tool input.
2. Close rows **11–13** (selection and contextual inspectors).
3. Close rows **14–19** in management order, starting with the current accepted
   inventory styling in row 17.
4. Close rows **3–8** against real new/load/save/pause flows.
5. Close rows **20–23**, then run the cross-section and package gates.

## Ranked remaining work after the 2026-08-16 iteration

Rank this queue before selecting the next safe slice. The ranking combines player
impact, dependencies, and the cost of obtaining literal production evidence; a
fixture or static contract does not close any row below.

| Rank | Priority class | Remaining gap and evidence | Dependency / verification gate |
|---:|---|---|---|
| 1 | External host/input blocker | True IME composition and a real multi-scale monitor/DPI transition remain unproven; the current desktop exposes only `en-US` (`0409:00000409`) even though committed Unicode, focus loss, and one-scale resize pass in the 2026-08-15 host probe. | Requires an available composition-capable IME and second scale; do not fabricate this evidence. |
| 2 | Missing core gameplay interaction | HUD row 9 now has authoritative pressed-state feedback and a real Inspect map click now opens the authoritative TileInfo inspector; Row 11 restores the authoritative Remove floor/Fell tree/Remove plant affordances, adds live Manage and Mine routes against saved real tiles, retains live Remove floor, Mine, Fell, RemovePlant, and Harvest jobs, projects the original active-job WorkablePosition/RequiredItems fields, proves queued cancel clears the active-job panel and restores Mine wall, fixes Raise priority to refresh the live priority value, disables both priority actions at authoritative bounds 0 and 9, and proves one real inspector priority mutation survives save/end/reload. Creature equipment now renders from the authoritative payload, unresolved creature IDs clear the active creature panel, the profession selector changes Gnomad to Farmer through production Rml activation, gnome activity now shows the authoritative `Mining` value, gnome needs no longer fabricate unsupported values, the creature inspector now has authoritative carried-inventory and supported-gnome-skill consumers with live frames, and the explicit Build action is now visually and functionally routed in production; a DB-generated migration prompt now renders in the production HUD and its Yes response visibly reaches four gnomes; physical tool/Manage/Mine/Fell/RemovePlant/Harvest ownership, profession pointer-click, unsupported-creature framebuffer coverage, non-empty inventory, completion/disappearance with an available authoritative tool, event-source breadth, and event/pause persistence remain open. | Depends on stable map/UI click-through and uses real discovered save tiles plus inspected frames and authoritative traces; Manage, Mine, Fell, RemovePlant, Harvest, cancel, priority, and explicit Build are currently in-process Rml activation because the shell session lacked foreground pointer ownership for those runs; the priority persistence run is also in-process Rml activation, not a physical pointer claim; profession/activity/needs/inventory/skills/event results are in-process Rml activation or source/contract evidence where physical ownership or data is unavailable; the current completion target is blocked by authoritative `RequiredToolAvailable=No`; the event prompt probe used the DB-generated event object and existing `EventConnector::onEvent` handoff, so it does not close scheduler breadth or physical pointer gates. |
| 3 | Missing gameplay matrix | Build row 10 now has explicit Build/Fill hole/Replace controls and a production Walls frame; the inspected real save has a two-component Crude recipe but only one observed RawStone material for component 1, so it still lacks a genuine second-component material change, terrain-category placement matrix, and broader DB-category mutation coverage beyond the already proven workshop/Palisade slices. | Reuse typed `EventConnector`/aggregator seams; verify saved jobs and reload, not only previews; do not fabricate a second component option absent from the save. |
  | 4 | Missing live data/persistence | Rows 11-20 retain section-specific real mutations, physical row focus, error responses, and save/reload gates even where fixture/native proofs pass. Row 14 now has live preorder, state-driven tri-state disclosure, one-based priority projection, suspended/pull/allow-pull and priority-Apply responses, positive-count content projection, authoritative selected filter label/state/depth, a focused mixed-parent mutation contract, a literal mixed-parent response frame, copied-real-save filter/pull/allow-pull persistence, an in-process visible filter-row Down/Space response, and a literal copied-save empty-content frame; physical keyboard/focus and Enter/Space save/reload remain open, while original copy/limits are closed as an evidence-qualified omission because the original buttons are unbound and the limit controls are commented out. Row 15 has an explicit LinkStockpile route, but the retained real save has no adjacent valid stockpile, so positive link mutation and reload remain data-gated. | Retain physical input and Enter/Space persistence as separate evidence gates; do not infer them from the in-process empty-content probe. |
| 5 | Major visual/accessibility gap | Row 22 still needs runtime long-string/locale/contrast/reduced-motion proof, scaled footer/history reachability, and release-owner sign-off for the documented unavailable sheets. | Requires production captures at the target scales/locales and explicit asset provenance approval. |
| 6 | Deferred editor | Keyboard-command catalog/editor remains intentionally deferred until dispatch, conflict detection, persistence/reset, and IME rules are unified. | Do not expose a parallel keybinding model. |

## Review log

| Date | Sections reviewed | Decision/evidence | Follow-up |
|---|---|---|---|
| 2026-08-16 | Settings display controls; current `settings.rml`/`ShellRmlBinding`; original `git show HEAD:content/xaml/SettingsPage.xaml`; pinned RmlUi `WidgetSlider` and checkbox contracts | The original screen binds fullscreen and mouse-wheel behavior as two-way `CheckBox` values and keyboard speed/light minimum as `Slider` values. The current Rml rows are `setting-fullscreen`, `setting-ui-scale`, `setting-minimum-light`, `setting-keyboard-speed`, and `setting-wheel-level`. Pinned RmlUi `WidgetSlider::SetValueInternal` dispatches `Change` with `parameters["value"]` before updating the element's `value` attribute; the old binding read only that stale attribute, so slider changes re-sent the previous value. The binding now reads the event payload (`value`/`checked`), and every supported `AggregatorSettings` setter emits a fresh authoritative settings snapshot. Immediate settings/revert/reset actions now complete their shell request instead of leaving a nonexistent async request pending. Production Rml probe through the live Settings document dispatched UI scale `125`, minimum light `40`, keyboard speed `140`, and wheel-level `true`; trace `build-wave8-root-msvc-link-priority2/settings-ui-probe-20260816b/settings-ui-trace.log` SHA-256 `315FCC852A8D7FD32242FBC92E8482A10A83B22339CA6A88049CA25E5965E4C3` records all four dispatches and persisted values `uiscale=1.25`, `lightMin=0.40`, `keyboardMoveSpeed=140`, `toggleMouseWheel=true`. The literal production Settings framebuffer `build-wave8-root-msvc-link-priority2/settings-ui-probe-20260816b/settings-ui-after.png` is SHA-256 `9430D3B5383769174FC4A00171CA819D9A5E5EC6ED8942A94E7B78D652B406FA` and visibly shows the moved slider thumbs and checked wheel toggle. The isolated probe reported only the expected missing `userpresets.json` parse warning; it exited successfully. The rebuilt production executable is SHA-256 `94ED6E346792F907CCA3FB3E60E2E69D388920521BF66A08712DD87A10238869`; shell CTest passed `3/3`, both shell verifiers passed, and `git diff --check` passed. | The in-process Rml path and literal framebuffer are closed for these four controls. Physical foreground interaction, fullscreen toggle/resize, and settings reload from a normal user data root remain separate gates; do not infer them from the probe. |
| 2026-08-16 | Main menu presentation control; current Rml shell route and `ShellController` contract | Removed the misleading `Skip presentation` button and its complete dead path: Rml element, shell callback mapping, `ShellControl` enum case, unused `presentationComplete` state, static/QPS localization entries, and the obsolete UX wireframe line. The current title copy remains informational and the actionable menu is available immediately. A repository-wide search finds no remaining `shell-skip`, `SkipPresentation`, `Skip presentation`, or `presentationComplete` references. The rebuilt production target succeeded in the Visual Studio developer environment; rebuilt shell CTest passed `3/3`, both shell verifiers passed, and the executable SHA-256 is `BFAE9D99E1372196AC9DD408C9D1F74EE8BA39F299329061912CB257D222C50`. | Restart the already-running pre-change game process before visual inspection; no process was terminated automatically. |
| 2026-08-16 | Stockpile filter keyboard navigation/activation; current Rml `keydown` route; original nested CheckBox contract | Added `Management6AController::moveStockpileFilterSelection` over the existing visible preorder with clamped Up/Down/Home/End movement, a `stockpile_filters` keydown listener, visible-row focus restoration, and `aria-selected`/`aria-checked` projections. Enter/Space now use the existing selected-filter toggle dispatch; no EventConnector or game-thread ownership changed. Rebuilt the production target successfully with the Visual Studio developer environment; Management 6A CTest passed `3/3`, both static verifiers passed, `build.ninja` remained SHA-256 `EFFD6970422D55B949A33FE2642C364C76AD4CF700322427CC6F5E4A085F62F9`, and the final linked executable was SHA-256 `D2129DD6FEC6ABE4D71D9DB5E03753E4A97BF91AAD3B1593B99D6E146D9BCA70`. Against a fresh copied real save, the trace SHA-256 is `4BCCAE358D8A7860D9B5FC66DEA08845B4729374EB0F5638842366BF07B1BA66`: explicit load reached tile `1005242`, the real `RawWood/AppleWood` material was selected and toggled to create the mixed branch, the real `RawWood` mixed parent was selected, Down dispatched, and Space dispatched. The literal production framebuffer `build-wave8-root-msvc-link-priority2/physical-stockpile-keyboard-20260816c/stockpile-keyboard-after.png` is SHA-256 `2853D3892D6AF09E916120DE79C57CD5045D86ABCC5D9A6CBDE73FBC904E8AFB` and visibly reports `Selected: AppleWood | On | depth 3 (hidden by filter)` after the Down then Space route, with the live mixed RawWood tree over the map. The copied source `game.json` and `stockpiles.json` remained SHA-256 `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`. This is in-process Rml event evidence: the trace proves the key dispatch seam was reached, the frame proves visible selection movement and the live On response after Space, and the source save remained unchanged; it does not claim physical keyboard ownership or save/reload persistence. | In-process visible filter navigation and Space response are closed for the exercised real mixed branch. Physical keyboard/focus, Enter response plus save/reload, a clean empty-stockpile frame, and original copy/limits behavior remain open. |
| 2026-08-16 | Stockpile keyboard/focus gap; current `stockpile_manager.rml`/`Management6ARmlBinding.cpp`; original `StockpileGui.xaml` CheckBox/ItemsControl/ScrollViewer contract | Re-inspected the literal mixed-parent production frame `build-wave8-root-msvc-link-priority2/physical-stockpile-mixed-filter-20260816h/stockpile-mixed-filter-after.png` (SHA-256 `3DBFF591D828B97E8C8E36BF5C31B1D72AC64A9F5794A036C6C5A269330763A0`): the live tree is rendered as focusable Rml `<button>` rows and the selected row is visibly preserved after the in-process filter route, but the binding has no `keydown` listener for `stockpile_filters`, no arrow/Home/End selection movement, and no Space/Enter toggle path matching the original nested CheckBox behavior. The original XAML uses nested `ItemsControl` rows inside a `ScrollViewer`; each category/group/item/material is a CheckBox whose `IsChecked` binding directly toggles the authoritative filter and whose mixed state expands descendants. The current explicit `Next rule`/`Toggle selected rule` controls keep the route usable, but they require extra focus movement and do not prove the original tree keyboard semantics. The copied real save exposes one active stockpile with 64 fields and 83 item slots, so no clean empty stockpile is available in this evidence set without a supported gameplay-created stockpile. | Add keyboard navigation and focus restoration for the visible filter rows using the existing controller selection and toggle seams; verify via focused contract/static tests and a copied-save production probe. Keep direct pointer ownership, physical keyboard capture, empty-state creation, and original copy/limits behavior separately bounded. |
 | 2026-08-16 | Stockpile empty-content state; current `Management6AController::renderStockpile`/`stockpile_manager.rml`; original `StockpileModel` and `StockpileGui.xaml` contract | Re-inspected the current status gate: the dedicated empty message is shown only when both authoritative filter and content projections are empty, while a ready stockpile with no positive content rows renders the content section and its explicit `No contents match the filter.` row. The original `StockpileModel` rebuilds a default filter tree and appends `StockedItems` only for positive counts; the original XAML uses nested filter/content `ItemsControl` views, leaves per-item limit controls commented out, and has copy/paste buttons without bindings. The supported route is live: HUD `hud_tool_stockpile` maps through `LegacyToolActionAdapter` to `CreateStockpile`, `Selection::leftClick`, and `StockpileManager::addStockpile`. An opt-in diagnostic path now delivers two Qt map press/release pairs through `MainWindow`'s existing world-input boundary, then opens the newly created stockpile through `AggregatorStockpile::onOpenStockpileInfo` on the game thread; no DTO, filter, content, or save data is synthesized. Against a fresh copied real save with source `game.json` SHA-256 `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and `stockpiles.json` SHA-256 `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`, the queued production trace records explicit load, target camera `48 52 100`, HUD tool activation, accepted first/second clicks, and authoritative creation of stockpile `1003313` with one field at tile `1005348` and `contents=0`. The trace is `build-wave8-root-msvc-link-priority2/physical-stockpile-empty-20260816c/stockpile-empty-trace.txt`, SHA-256 `D3C1B32434E6B11D4E6623D1D042E4DDE5B753DD70848E4C0A744D1634236F17`; the literal production framebuffer is `build-wave8-root-msvc-link-priority2/physical-stockpile-empty-20260816c/stockpile-empty-after.png`, SHA-256 `AEC40F548D555D6D34B3600F6839F193C3E7D64269B64C34042E59238167282`, and visibly shows the live map, `0 / 0 items`, the real allowed-content tree, and `No contents match the filter.` The changed `main.cpp` object compiled under the Visual Studio developer environment and the same generated RSP set linked to `Ingnomia-empty-probe2.exe`, SHA-256 `9F057BFAA5AB769C1EE8EE599C3F4A538EAE4E04DEBE1AB385D751EBE880F898`, using existing staged runtime dependencies; the normal CMake link/post-copy remained blocked by the unrelated locked `Ingnomia.exe`. Management 6A CTest passed `3/3`, both static verifiers passed, and `build.ninja` remained SHA-256 `EFFD6970422D55B949A33FE2642C364C76AD4CF700322427CC6F5E4A085F62F9`. This closes the literal empty-content frame for a real gameplay-created stockpile, not the stricter no-filter/no-content loading state, and makes no physical keyboard/pointer or save/reload claim. | Empty-content visual/functional evidence is closed for this real route. Physical keyboard/focus, Enter/Space save/reload, the unreachable dedicated no-filter/no-content state, and original copy/limits behavior remain bounded follow-ups. |
 | 2026-08-16 | Stockpile copy/paste and limits audit; original `StockpileGui.xaml` contract versus current Rml omission | Re-inspected the original source contract directly. `StockpileGui.xaml` renders `StockpileGui_CopySetup` and `StockpileGui_PasteSetup` buttons without `Command` or `Click` bindings; the Limits column renders `StockpileGui_LimitsImplementedLater`, while `CBLimitMode` and the `ItemsSource="{Binding Limits}"` `ItemsControl` are inside a comment block. A repository search found no authoritative `copy_setup`, `paste_setup`, or limits command/bridge to migrate. The current production empty-content frame `build-wave8-root-msvc-link-priority2/physical-stockpile-empty-20260816c/stockpile-empty-after.png` (SHA-256 `AEC40F548D555D6D34B3600F6839F193C3E7D64269B64C34042E59238167282`) visibly carries the explicit `Unsupported projections and per-item controls are intentionally omitted.` warning and does not fabricate dead controls. `verify-management6a-rml.cmake` now requires the supported stockpile projection IDs and warning while rejecting fabricated `stockpile_copy`, `stockpile_paste`, and limit IDs; Management 6A CTest remains `3/3`, and both Rml/integration verifiers pass. | Closed as an evidence-qualified intentional omission, not an implementation gap. Do not add copy/paste or editable-limit controls without an authoritative source command and persistence contract. Physical keyboard/focus and Enter/Space save/reload remain open independently. |
 | 2026-08-16 | Stockpile physical-input/focus and Enter/Space persistence gate; environment capability audit | The production traces and framebuffer rows above prove only in-process Rml activation and visible response; they intentionally do not claim physical pointer or keyboard ownership. A bounded Windows desktop probe found no enumerated foreground window for the known diagnostic PIDs (`31320`, `35728`, `53500`, `58172`), and PowerShell reported `MainWindowHandle=0` for each still-running `Ingnomia*` diagnostic process. The normal CMake link also remains blocked by the unrelated locked `Ingnomia.exe`; the current linked probe artifact is separate and does not change that process. There is no available foreground desktop-control tool in this environment to produce defensible physical row-focus, pointer, or Enter/Space save/reload evidence. | Concrete external blocker for this gate: stop at the evidence boundary and do not simulate physical ownership through another in-process seam. Reopen with a controllable foreground production window/device; then exercise row focus, arrow/Home/End, Enter/Space, and save/end/reload on a real stockpile. |
 | 2026-08-16 | Stockpile mixed-parent gap; current production filter tree; original `StockpileGui.xaml`, `Filter::update`, and `StockpileModel` contract | The current real copied save contains stockpile `1001406` with active `RawWood` material entries (`AppleWood`, `Birch`, `Oak`, `OrangeWood`, `Pine`, and `Willow`) in `Filter.Items`; the authoritative `Filter` rebuilds these rows from the live inventory catalog and the DB `Wood` material type. The original UI exposes category/group/item/material tri-state CheckBoxes, expands an indeterminate parent, and resolves an indeterminate click to inactive through `Nullable<bool>::HasValue() == false`. The current Rml controller/binding already preserves preorder, derives `Mixed`, expands mixed ancestors, and routes the mixed mutation rule, but no literal production frame has yet exercised a mixed parent. This is a data-backed evidence gap, not permission to synthesize row data. | Use the existing stockpile search and Rml activation route to toggle one real active `RawWood` material, clear the search, and capture the resulting live `Materials`/`Wood`/`RawWood` mixed hierarchy; retain the real-save filter mutation and trace as bounded evidence, then reassess empty-state, keyboard/focus, and copy/limits gaps. |
| 2026-08-16 | Stockpile literal mixed-parent response and original `Filter`/`StockpileGui.xaml` tri-state contract | Re-inspected the original category/group/item/material CheckBox hierarchy and `Filter::update`: the copied real save has active `RawWood_AppleWood` plus the other live `RawWood` material variants, and the original mixed parent expands while an indeterminate click resolves inactive. Added only opt-in production-probe seams that reuse the live search, row selection, and `stockpile_toggle_filter` listener; no game-thread or EventConnector ownership changed. Management 6A CTest passed `3/3`, both static verifiers passed, and `build.ninja` retained hash `EFFD6970422D55B949A33FE2642C364C76AD4CF700322427CC6F5E4A085F62F9`. The final linked/copied probe executable `Ingnomia-mixed-filter5.exe` hashes to `F09531BF9AA6663E505BE024697B6FA7E8818C054EE2EAD5E19C8EABB0FF3CF9`. Against a fresh copied real save, the normal queued explicit load reached stockpile `1001406` at tile `42 52 100` (`1005242`); the trace records search `AppleWood`, exact `RawWood`/`AppleWood` selection, live toggle, `RawWood` parent search, and mixed-parent selection (`selected=true`), hashing to `CF25F5CC18A6A85C4AE9C5364E9F10F24D675A6C6254C6A37EB94A31345334C5`. The literal production framebuffer `build-wave8-root-msvc-link-priority2/physical-stockpile-mixed-filter-20260816h/stockpile-mixed-filter-after.png` hashes to `3DBFF591D828B97E8C8E36BF5C31B1D72AC64A9F5794A036C6C5A269330763A0` and visibly reports `Selected: RawWood | Mixed | depth 2`, with the mixed marker and real material descendants over the live map. The copied source `game.json`/`stockpiles.json` hashes remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`/`2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`; this is a response-only copied-save mutation, not a persistence claim, and the search-assisted frame intentionally keeps the live `RawWood` branch visible. The CMake link completed, but its shared `windeployqt` post-copy was blocked by existing staging locks; proof used the resulting linked executable with the existing staged runtime dependencies. The run inherited the retained missing `monsters.json`/audio warnings and used in-process Rml activation rather than physical pointer ownership. | The literal mixed response is closed for this real `RawWood` branch. A mixed save/reload persistence frame, clean empty-stockpile frame (both retained stockpiles contain items), exact tree keyboard/focus semantics, physical pointer ownership, and original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile AllowPull persistence and original `CBAllowPull`/`Stockpile::serialize` contract | Re-inspected the original `CBAllowPull` two-way binding: `PullFromHere` maps to `m_pullFrom`, is passed as the final `allowPull` argument, and is serialized as `AllowPull`. The opt-in production probe exercised `stockpile_toggle_allow_pull` through save, end, reload, and post-reload `tile_manage`; the distinct executable `Ingnomia-allow-pull-persistence.exe` hashes to `6F8BE684CA1CFBEC0C707117AA7F97874788977E2E5BEB05502B09939B889494`. Management 6A CTest passed `3/3`, and both static verifiers passed. The valid copied-real-save trace hashes to `32375517AA8BF9832382EC97EBD92F943234018270F44D87036F40FB68892CB2`; lifecycle trace hashes to `92678599F7523D0A0365EA89D19CDBF61ED6DBD8952D5148215963E2B78569F0`. The post-reload frame `build-wave8-root-msvc-link-priority2/physical-stockpile-allow-pull-persistence-20260816a/stockpile-allow-pull-persistence-after-reload.png` hashes to `904FFC2E7BC4125FACC1793B1E146EE49360060A2B7AA8370FE95C8BD0FD87F6` and visibly shows `[x] Allow pull from here`, the live map, `Selected: Armor | Off | depth 0`, hierarchy, and positive contents. Slot `1` `game.json`/`stockpiles.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`/`2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`; slot `2` hashes are `40476043321909F83B0826B04E459976F6796DABDE7842A1D6672E76C0B3294E`/`5C79B255A217EDAFC38EF5D97B687CF114B997959045483472532B44C747483E`, and parsed stockpile `1001406` flags changed from `PullOthers=false, AllowPull=false` to `PullOthers=false, AllowPull=true`. The paused pre-capture (`FFF4C98AFF2B49FD94FA6C9E61EC5ADF36ED02D326E2C144D30F150BB1DD9A04`) is timing evidence, not a clean before-frame claim; no physical pointer-ownership claim is made. | Allow-pull persistence and post-reload manager repopulation are closed for this copied save. A literal mixed-parent response/persistence frame, a clean empty-stockpile frame (both retained stockpiles contain items), exact tree keyboard/focus semantics, and original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile pull persistence and original `Stockpile::serialize` flags | Re-inspected the original `CBPullFrom`/`CBAllowPull` two-way bindings and `Stockpile::serialize()`: `PullFromOther` maps to `PullOthers`, `PullFromHere` maps to `AllowPull`, and both are persisted with the stockpile. The opt-in production probe exercised `stockpile_toggle_pull` through save, end, reload, and post-reload `tile_manage`; the distinct executable `Ingnomia-stockpile-pull-persistence.exe` hashes to `2E69459F13BB048320E3A9DF9E5B1435BC396C9A33849F64B05E54DED2EF0B18`. Management 6A CTest passed `3/3`, and both static verifiers passed. The valid copied-real-save trace hashes to `62CF756298FC81078BADF6605CDFD5F593286A149CC1AD4D4EADDB040A6A371B`; lifecycle trace hashes to `9F3CA8512F1C9898F62CEBEEAEF701739505CC8286A62C7D195D2F3F6A9A3C60`. The post-reload frame `build-wave8-root-msvc-link-priority2/physical-stockpile-pull-persistence-20260816a/stockpile-pull-persistence-after-reload.png` hashes to `4386C65A8A597DA3EA449E2E70954DAF99B50199C8A1E6366BE21112C254B0F3` and visibly shows `[x] Pull from others`, the live map, `Selected: Armor | Off | depth 0`, hierarchy, and positive contents. Slot `1` `game.json`/`stockpiles.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`/`2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`; slot `2` hashes are `1E0ED7F2E256056F6E8953657BE68FE4EAB3A9E4A91FB1360C85529DB7D293FC`/`9D4D0D47F2729EAFF6699842FB627105F78351B58EB78923F00C8F5BAE683947`, and parsed stockpile `1001406` flags changed from `PullOthers=false, AllowPull=false` to `PullOthers=true, AllowPull=false`. The paused pre-capture (`3A928F008607A1F44E40A54501D45E16117E34BC53BA2C0B7A22B5E9324F840B`) is timing evidence, not a clean before-frame claim; no physical pointer-ownership claim is made. | Pull persistence and post-reload manager repopulation are closed for this copied save. Allow-pull persistence, literal mixed-parent response/persistence, a clean empty-stockpile frame (both retained stockpiles contain items), exact tree keyboard/focus semantics, and original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile filter persistence and original `Filter::serialize`/`Stockpile::serialize` contract | The original `Filter::serialize()` stores active item-material entries under `Filter.Items`, and `Stockpile::serialize()` includes that map. The opt-in production probe exercised the existing queued `stockpile.set_filter` route through save, end, reload, and post-reload `tile_manage`; no EventConnector or aggregator ownership changed. The distinct executable `Ingnomia-stockpile-filter-persistence.exe` hashes to `ADA929569E121F9F16CF00DB02F322C884738098B482EEDA5C5606906DAFBA74`; Management 6A CTest passed `3/3`, and both static verifiers passed. The valid copied-real-save trace hashes to `8CC6B055CADB95F864529DB797E2813DD38C6F952F16A7677A332F9A2FC38885`; lifecycle trace hashes to `590CEB84C54FA8872311A604F28C48BE9C217194C75FB6D0208F178EE5B77EEB`. The post-reload frame `build-wave8-root-msvc-link-priority2/physical-stockpile-filter-persistence2-20260816a/stockpile-filter-persistence-after-reload.png` hashes to `AA5F8E139D84D7930EE9A83476FC53328EE1A05D343B7AB57450E4F6A9C693DC` and shows the live map plus `Selected: Armor | On | depth 0`, filter hierarchy, controls, and positive contents. Slot `1` retained `game.json`/`stockpiles.json` hashes `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`/`2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`; created slot `2` hashes are `0226FC0666795910163B42FF87DE62BEA9FFF7C84DA8A12FAEEF819BCEE99CB7`/`4DC423C7F934A43AAAB9716179FE9F9E620D49107A2F98FD5ED6B1F04CBFEE9F`. Stockpile `1001406` changed from `95` to `378` serialized active entries, matching the logged `set active ... true "Armor"`; the other stockpile stayed at `18`. The paused pre-capture (`A7453945C25F2C009C52CF6E609272B6DD88492EEB6DEAB9FA3992A8D24BD6C2`) is timing evidence, not a clean before-frame claim. | Filter mutation persistence and post-reload manager repopulation are closed for this copied save. Broader pull persistence, literal mixed-parent response/persistence, a clean empty-stockpile frame (both retained stockpiles contain items), exact tree keyboard/focus semantics, and original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile pull flags and original `StockpileGui.xaml` two-way bindings | Re-inspected the original `CBPullFrom`/`CBAllowPull` bindings and `StockpileModel::setBasicOptions`: `PullFromOther` is the `pullOthers` argument and `PullFromHere` is the `allowPull` argument, both reaching the queued `StockpileProxy::setBasicOptions`/`AggregatorStockpile::onSetBasicOptions` route. The current Rml binding preserves that order and the focused controller assertion now checks both typed payload flags. The rebuilt distinct production executable `Ingnomia-stockpile-pull.exe` hashes to `6E8C131F3BCBC0F8FD64AE628A2C3A50066804BEDF07033652F2F4BFB7D5B952`; Management 6A CTest passed `3/3`, and both static verifiers passed. Two fresh copied-save runs loaded stockpile `1001406` at `42 52 100` (`1005242`), opened it through `tile_manage`, activated `stockpile_toggle_pull` and `stockpile_toggle_allow_pull`, and exited `0` through orderly `endCurrentGame complete` traces. The `Pull from others` frame is `build-wave8-root-msvc-link-priority2/physical-stockpile-pull-20260816a/pull_from_others-after.png` (SHA-256 `16769FEDAF68402781B5245B6F09D27FA4282F7E22D367958EF0EBCD6E6C4E00`) and visibly shows `[x] Pull from others`; its activation/lifecycle traces hash to `6FEC1AFFA21DF622AAA9CDC254E22AB00B085AC0AF30B93180C8E3972126EC79` and `AB452929328DA346819BF015A90E8D249C5E8870D94FD227065FCE4D709E065E`. The `Allow pull from here` frame is `build-wave8-root-msvc-link-priority2/physical-stockpile-allowpull-20260816a/allow_pull_from_here-after.png` (SHA-256 `6BAA2B852650E69CEEF996E5F2DD46F36B61D7340358F5E2545E97CDD6AEE5BF`) and visibly shows `[x] Allow pull from here`; its activation/lifecycle traces hash to `A44ACC12397B47F4130354DBC47D1A0F072F28E3FC83C2EA410A94770B3B0D89` and `3581AD1C6BEF2E2C1CDB33FAF31E6E95CA4EA5BEE538AE0550AC0DBC0FDDE748`. In both copied saves, `game.json` and `stockpiles.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`; the frames prove the live response but not save/reload persistence, and the runs used in-process Rml activation rather than physical pointer ownership. | Pull and allow-pull live responses are closed for this slice. Save/reload persistence, a real mixed filter response, truthful empty-stockpile framing, exact tree keyboard/focus semantics, and original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile filter state projection and original `StockpileModel` tri-state mutation contract | Re-inspected the original `StockpileGui.xaml` four-level checkbox handlers and `StockpileModel`: a mixed parent is passed as an indeterminate `Nullable<bool>`, and `SetState` resolves that value to inactive before the queued `StockpileProxy::setActive`/`AggregatorStockpile::onSetActive` route. The migrated controller had used `state != TriState::On`, incorrectly sending active `true` for mixed rows. The safe fix now sends active only for an authoritative `TriState::Off` row; the Rml manager also renders the selected authoritative label, `On`/`Mixed`/`Off` state, and depth instead of generic selection text. Management 6A CTest passed `3/3`, both RML/integration verifiers passed, and the distinct linked production executable `Ingnomia-filter-state.exe` hashes to `433570D17B82EA463C920A7DA904915754F0083416F7886F116BBFDCA7E18377`. Against a fresh copied real save, the normal production route loaded stockpile `1001406` at `42 52 100` (`1005242`), opened management through `tile_manage`, activated `stockpile_toggle_filter`, and reached `endCurrentGame complete`; the visible capture `build-wave8-root-msvc-link-priority2/physical-stockpile-filter-state-20260816a/stockpile-filter-state-after.png` hashes to `CE99DBA956A85FCC984CBD9C624E989909C2B1F3EDA9F6F26AAFEE98FFC80EAD` and visibly reports `Selected: Armor | On | depth 0` alongside the live map, filter markers, storage controls, and positive contents. The activation trace hashes to `0163C69C916024A78ECE3BC0F5CC2CF2DCC16F8E9C2DEF1CCA7F7F32DF79BE86`; the lifecycle trace hashes to `F321DA92023CF977CB2DDB052659BEAAE30FBA0ACFC34E50ED76B133FCD3A63C` and reaches orderly teardown. The copied `game.json` and `stockpiles.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`. The production frame proves the new authoritative selection projection and live action activation, while the mixed branch is intentionally claimed only by the focused source-contract test because this real save selected an `On` row; no mixed runtime parity is fabricated. The distinct link used the existing staged runtime dependencies; locked older probe files prevent claiming a clean standard post-link deployment, and no physical pointer-ownership claim is made. | Obtain a real mixed row or supported gameplay state for a two-frame mixed response/persistence probe; then exercise broader pull/filter persistence, truthful empty-stockpile framing, exact tree keyboard/focus semantics, and the original copy/limits gaps. |
| 2026-08-16 | Stockpile priority Apply persistence, reload handoff, and original `TileInfo.xaml`/`StockpileGui.xaml` contracts | Re-inspected the original stockpile one-based priority selector and the original `TileInfo.xaml` `CmdManage` path, which sends the cached tile ID through the existing proxy/EventConnector route. The current `AggregatorTileInfo::init()` replaced the `Game*` without clearing its prior tile ID or cached payload, so a reload could reuse an ID from the retired world. The safe lifecycle fix now clears the per-world tile/info caches at the existing aggregator seam; the opt-in pre-direct capture also distinguishes the normal inspector route from the diagnostic direct call. Management 6A CTest remained `3/3`, and both Management 6A verifiers passed. The linked production executable is `Ingnomia-inspector-probe2.exe`, SHA-256 `7BCD72F1CDC1470348EE0DCD7CB72005D1205A3DD0D81C448B1D2EF7FCAFDC19`; its CMake post-link deployment copy was blocked by files locked by older user-owned probes, but the link succeeded and the existing staged runtime dependencies were used. Against a fresh copied 21-file real save, the source slot `1` `game.json` remained SHA-256 `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`; both load boundaries logged `Start loading world`, `Loading world done`, and `Starting game`; slot `1` stockpiles remained `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`, while the saved slot `2` stockpiles hash is `98E0B2C8B6C12F2F206C14AD3B3B2382871149AED2E643C5663E7324E9FA0582` and its order changed from `[1001406,1001726]` to `[1001726,1001406]` after applying display priority `2`. The before frame shows the live Apply result (`priority 2 / 2`, input `2`), and the post-reload pre-direct frame already shows the full live stockpile manager with `priority 2 / 2`, selected `RawWood / Pine`, and positive contents; the pre-direct and after frames are byte-identical at SHA-256 `5417F8BE864330CF8B920A8D27E97F6817205C05CA4AD899E74143001C80A0A5`: `build-wave8-root-msvc-link-priority2/physical-stockpile-applypersistence6-20260816a/stockpile-apply-pre-direct.png` and `stockpile-apply-after-reload.png`. The trace (`D3F5D5DA0448DE0DE1A20480C35F10FBB6BA4EE09894DED68DD26B945428D0C0`) records `persistence_stockpile_reload_manage activated=true`, then the pre-direct capture, then the optional direct queued call; identical frames prove the direct call did not supply the visible response. The lifecycle trace (`D08B029FFAAEABEF6B7537D2ADC34EFB56C14355B2DADA6C0B299495366A5D1E`) reaches `endCurrentGame complete` before and after reload. | Priority Apply, saved order, and normal post-reload manager repopulation are closed for this copied real save. The run used in-process Rml activation, not physical pointer ownership; broader filter/pull response and persistence, truthful empty-stockpile framing, exact tree keyboard/focus semantics, and original copy/limits gaps remain open. The diagnostic direct-call seam remains test-only, and the post-link staging lock prevents a clean CMake deployment claim. |
| 2026-08-16 | Stockpile filter mutation response and original `StockpileModel`/`StockpileProxy` contract | Re-inspected the original four-level `Category`/`Group`/`Item`/`Material` check-state handlers: each calls `StockpileProxy::setActive`, whose queued `signalSetActive` path reaches `AggregatorStockpile::onSetActive`. The migrated controller and Rml binding already dispatch the same typed `stockpile.set_filter` route, so this iteration exercised that existing path against copied stockpile ID `1001406` at tile `42 52 100` (`1005242`). The visible production run activated `stockpile_toggle_filter=true` and captured the live map, stockpile controls, tri-state filter rows, disclosure markers, and positive current contents in `build-wave8-root-msvc-link-priority2/physical-stockpile-filter-20260816a/stockpile-filter-after.png` (SHA-256 `0E6FC5487C59050C0FBC5843CCDFBF5F5CC121D9CCD7D7BE94529802F36054F3`). The activation trace is SHA-256 `FD7DB722014F355125D0D6CA8D3973F2782F3C40F5C6CF4BBDEBB8B70DE99216`; lifecycle trace is `25A28B75891CD95961930D370E5BF1CC773AAB31C98893AA9923D13C8269E769`; copied `game.json` and `stockpiles.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`. Management 6A CTest remained `3/3`, and both static verifiers passed. The frame and trace prove the live Rml activation and controlled copied-save boundary, but the current generic selection text does not identify the exact row/state after the queued refresh; no stronger per-row response claim is made. The run used in-process Rml activation rather than physical pointer ownership, reached `endCurrentGame complete`, and the GUI host process remained after teardown in this sandbox. | Add an observable selected-row/state projection or a two-frame authoritative response probe, then exercise filter persistence/reload and broader pull responses; priority Apply/save-reload, truthful empty-stockpile framing, physical pointer ownership, exact tree keyboard/focus semantics, and original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile priority indexing and original `StockpileGui.xaml` selector contract | Re-inspected the original `StockpileModel::onUpdateInfo`/`setBasicOptions` path and `StockpileGui.xaml`: the model builds user-facing `Priority 1..N` entries while `SelectedPrio` and the proxy retain the game’s zero-based `info.priority` index. The migrated Rml screen was displaying raw `0 / 2` and its `min="1"` field could not represent the authoritative first priority without an accidental mutation. The safe binding fix now renders `priority + 1`, parses the one-based control and sends `priority - 1`, leaving `AggregatorStockpile::onSetBasicOptions` and the queued EventConnector route unchanged. Management 6A CTest passed `3/3`, both RML/integration verifiers passed, and the rebuilt production executable SHA-256 is `5DA96FEA9AE448243B3A5FC8C5E4DD425B9B1A912A3D428DD7A724341E966F8D`. A normal visible-window copied-save run loaded stockpile ID `1001406` at tile `42 52 100` (`1005242`), activated `stockpile_toggle_suspended`, and captured `build-wave8-root-msvc-link-priority2/physical-stockpile-priority-20260816a/stockpile-priority-after.png` (SHA-256 `17B9C084E61D854066AD01E526F68E046E7CF4991A61BAE27ABB2A651B288252`). The frame visibly shows `priority 1 / 2`, the control value `1`, `[x] Resume` after the live suspended toggle, the live filter disclosure, and positive current contents. Activation trace SHA-256 is `DB839A5EF6156229747F505472EDC703B262D9553DBD728E711AAC07D10D5DF0`; lifecycle trace SHA-256 is `E02C125240F80730E92DCCCED02FA06449901C37DBF5ABDCB4AADCC6AF04D240`; copied `game.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and copied `stockpiles.json` remained `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`. Lifecycle reached `endCurrentGame complete`; the GUI host process remained after teardown in this sandbox, so no clean process-exit claim is made. The run used in-process Rml activation rather than physical pointer ownership and inherited the copied-save’s unrelated missing-data/audio warnings. | One-based display and the live suspended response are closed for this slice. Priority Apply mutation/save-reload, pull/filter response and persistence, truthful empty-stockpile framing, physical pointer ownership, exact tree keyboard/focus semantics, and original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile tri-state disclosure and original `StockpileGui.xaml` / `StockpileModel.cpp` contract | Re-inspected the original `FilterCategoryTemplate`/`FilterGroupTemplate`/`FilterItemTemplate` expanders and `getExpanded()` contract: category/group/item nodes expand only when their authoritative tri-state is mixed, while fully on/off nodes hide descendants; the current Rml list had rendered every flattened row. The safe task now preserves the authoritative preorder while suppressing descendants below collapsed ancestors, lets a non-empty search reveal matching descendants, and renders `[v]`/`[>]` plus `aria-expanded` disclosure metadata without inventing an image asset or user-owned expansion state. The focused controller test covers both full mixed preorder and an off group hiding its item/material descendants; Management 6A CTest passed `3/3`, and both RML/integration verifiers passed. The rebuilt production executable SHA-256 is `AF6E9B161858FD69A6F2591E6D63F7F566A04C2C48C2A7A0A5266D06D1D669D7`. A normal visible-window copied-save run loaded stockpile ID `1001406` at tile `42 52 100` (`1005242`), activated `stockpile_next_filter`, and captured `build-wave8-root-msvc-link-stockpile/physical-stockpile-disclosure-20260816a/stockpile-disclosure-after.png` (SHA-256 `1890BEDD27A5A7B46B81C397BED0EB5E77BD791002B3E60F501A768AEAA9576E`). The frame shows the live map and compact stockpile manager with `[>]` collapsed and `[v]` expanded filter rows, depth indentation, storage controls, and positive current contents. The activation trace SHA-256 is `D345D07FA0AF8DAAF6163077ACAA272C0F47CD6CFE86929668EF6C29F3F0687F`; lifecycle trace SHA-256 is `E55CB56FB57CDAA3196844B434C33F9ACCF5F4D36BB74CB3F6F283C72DB8A1C7`; copied `game.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and copied `stockpiles.json` remained `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`. Lifecycle reached clean `endCurrentGame complete`, but the GUI host process remained after teardown in this sandbox and could not be closed through the available process handle; therefore no clean process-exit claim is made. The run used in-process Rml activation rather than physical pointer ownership and inherited the copied-save's unrelated missing-data/audio warnings. | State-driven disclosure and hierarchy readability are closed for this slice. Exact keyboard/focus semantics, truthful empty-stockpile framing, live priority/toggle/filter response, save/reload persistence, physical pointer ownership, and original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile zero-count contents and original `StockpileModel` count guard | Re-inspected the original `StockpileModel::onUpdateInfo` and `onUpdateContent` loops: both append `StockedItems` only when `is.count > 0`, while the current `AggregatorStockpile` summary truthfully includes active filters with zero count. The migrated `Management6AQtDataAdapter::stockpile` was incorrectly forwarding those zeros into the Rml `Current contents` projection. Added the presentation-boundary `if ( value.count > 0 )` guard without changing the authoritative source payload or filter rows; the integration verifier now requires that guard. Management 6A CTest passed `3/3`, and both Management 6A verifiers passed. The rebuilt isolated production executable SHA-256 is `766F8DD016FA262738A21CCAD61E313ADD383A095C7D8B1AAC3B53C87DA8B510`. The same visible real-save route loaded stockpile ID `1001406` at tile `42 52 100` (`1005242`), activated `stockpile_next_filter`, and captured `build-wave8-root-msvc-link-stockpile/physical-stockpile-hierarchy-20260816a/stockpile-count-after.png` (SHA-256 `27CB3588185583C69EE847E19A39FB3EF017403EEF0909BCD794DCE6BB2F507A`). The frame retains the indented live filter hierarchy and now shows only authoritative positive contents (`raw wood | apple | 6`, `raw wood | pine | 16`, `raw wood | oak | 26`), with no fabricated zero-count rows. Activation trace SHA-256 is `A6D9A87AE83C98829DBE8018EC90DDB617E04A0AD208967EE276241A875B5E02`; orderly lifecycle trace SHA-256 is `8EF46961C1621612968F1A51306031744897786035CE11F73A2E28ECB7695756`; copied `game.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and copied `stockpiles.json` remained `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`. The run still inherits missing `monsters.json` and audio warnings; it is a normal visible production window with in-process Rml activation, not a physical pointer claim. | Zero-count suppression and the positive-content projection are closed for this slice. A literal empty-stockpile frame, priority/toggle/filter persistence, physical pointer ownership, full tree expand/collapse/keyboard semantics, and the original copy/limits gaps remain open. |
| 2026-08-16 | Stockpile filter hierarchy and original `StockpileGui.xaml` / `StockpileModel.cpp` contract | Re-inspected the original four-level category/group/item/material filter tree, its tri-state rows, and the original `StockpileModel` ordering/expansion behavior against the production Rml route. The current controller had alphabetically sorted the flattened filter projection, which destroyed tree preorder. The safe task removed sorting from filter rows while retaining content sorting, labeled the sort control `Sort contents`, and added depth-specific compact classic-park indentation (`0/1/2/3`) to the generated rows. The focused controller test now asserts a deliberately non-alphabetical category → group → item → material sequence; Management 6A CTest passed `3/3`, and both Management 6A verifiers passed. The fresh isolated production executable SHA-256 is `DF77E655844822790D995E23012686B5F3D94809E1C7639C53B29347DFECE4ED`; staged `management6a.rcss` SHA-256 is `985A378064618FD0C1230D39A4837A417FA61F143C185EDC6A5F1B1F2A6B305A`. The visible real-save run loaded stockpile ID `1001406`, opened the live tile `42 52 100` (`1005242`) through `tile_manage`, activated `stockpile_next_filter`, and captured `build-wave8-root-msvc-link-stockpile/physical-stockpile-hierarchy-20260816a/stockpile-hierarchy-after.png` (SHA-256 `E842CBE89CBA971879A476D2C96597739A77B74C453CA7FA3D10BF6B5C5ADC13`). The frame shows the live map, Storage controls, `Sort contents A-Z`, selected filter row, and visibly indented filter levels; the activation trace records `management_element requested=stockpile_next_filter activated=true` and hashes to `AA3602EA6E7EB4533DCEFA82F71B17BCE5F20771D93D7B93A5461968CB1378C2`, while the orderly lifecycle trace hashes to `F1D58D95ED4C4685C429AD1CA36E2E83194B3BF6CEBA7F96DBA0D634B45DB4BA`. The copied `game.json` stayed `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498` and copied `stockpiles.json` stayed `2D30144EBA963DD638522F9A9DE5F6E5641415E09F3B2CA5147856A99FE82712`. The same frame exposed the next mismatch: the migrated `Current contents` list rendered authoritative zero-count summaries (`bar | bronze | 0`, `bar | copper | 0`), whereas the original model suppresses them with `if (is.count > 0)`. The run also inherited missing `monsters.json`, missing audio assets, and missing `settings/userpresets.json` warnings; no physical pointer claim is made. | Hierarchy order, depth readability, selected-row activation, live storage controls, and no-write behavior are closed for this slice. Expand/collapse parity, exact tree keyboard semantics, zero-count content suppression, real priority/toggle/filter persistence, physical pointer ownership, and full StockpileGui copy/limits gaps remain open. |
| 2026-08-16 | Workshop LinkStockpile and original `WorkshopGui.xaml` contract | Re-inspected the original normal/butcher workshop bindings: `CBConnectSPN` and `CBConnectSPB` are two-way `CheckBox` controls bound to `LinkStockpile`; the current production workshop manager previously exposed only a summary value. Added the localized Rml `Link stockpile` toggle, an optional explicit `linkStockpile` field to `SetWorkshopBasicsPayload`, the controller/command-port route, and management activation through the existing `MainWindow`/EventConnector path. Older callers retain cached-link semantics, while explicit true/false input takes precedence. Management 6A CTest passed `3/3`; `verify-management6a-rml.cmake` and `verify-management6a-integration.cmake` passed. The fresh production executable SHA-256 is `4C266361AE7240F20D0EA712AF2888E53F49AF13A6AB57BF337C9370035C794F`. A normal visible-window run loaded the copied real save, selected workshop tile `1004550`, opened `tile_manage`, activated `workshop_toggle_linked`, and captured `build-wave8-root-msvc-link/physical-workshop-link-20260816a/workshop-link-after.png` (SHA-256 `1B7F505FC86FA99D32DB039102CC0372FF371D87171CA43EF3775CD620246B77`). The frame visibly shows the live `crude workbench` catalog/queue, the `[ ] Link stockpile` control, and the compact classic-park workbench. The production activation trace records `management_element ... activated=true` and hashes to `D976ABB4BA7932EE97EA66757EB5C419BBFF0DBB29B25F2341DC1852E80722CC`; the lifecycle trace records init/post-init and orderly teardown and hashes to `1472ABD5EA6EC851A9F0B51F0030D6DF3B96ED25023CDB5446E365E4DF3920C21`. The authoritative log records `linked stockpile: 0`: `Workshop::setLinkedStockpile(true)` found no possible adjacent stockpile in the retained real save. Read-only stockpile inspection found no stockpile fields at either workshop input position; copied `workshops.json` remained `7C97DE0D9712C13DC9CB78ABD3A3753AAA2AD805B167683C7827349584ADAB40` and copied `game.json` remained `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`. | The toggle intent, authoritative negative outcome, live catalog/queue rendering, and no-write behavior are closed for this slice. Positive link state, unlink/relink save/reload, the broader queue/craft/trade/butcher/fisher matrix, physical pointer ownership, and inherited copied-save `monsters.json`/audio warnings remain open. Do not fabricate an adjacent stockpile or claim positive LinkStockpile parity from this run. |
| 2026-08-16 | Creature skills enhancement and original CreatureInfo audit | The original `CreatureInfo.xaml` has no skill binding or creature command; the modernization enhancement instead projects DB-defined `SkillGroups` through `Gnome::getSkillLevel`/`getSkillActive` via the existing creature aggregator and Inspector adapter, with a focused skill-order/detail/teardown test. Inspector CTest passes `4/4`, HUD CTest passes `6/6`, and both Inspector verifiers pass. The rebuilt/staged production objects were linked as `Ingnomia-event14.exe` SHA-256 `36A3FB38B5723937521BAA74EA47E654A8E47DF30F70658A4C95A0E0D295073B`; clean copied-save runtime capture `build-wave8-root-msvc/physical-inspector-skills-20260816e/creature-skills.png` is SHA-256 `BB326815FA44B6C85BF223C00C1E68E189291AC0A569A3DF4463FF4EA302C341`, trace `97BAAE1246D978B7AEE5922533C5142640427B7922A8E8354F95A4839B112D9E`, lifecycle `B275C0F715DBCAE25BB92536CBE4243507AA334080BC3AD392D67394AD9FE7C1`, and copied `game.json` remains `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`. The row layout was corrected from multi-line wrapping to compact label/detail columns. | Supported gnome skill enhancement/readability is closed for this slice; creature actions are not an original contract, while icon/non-empty inventory parity, unsupported-creature framebuffer coverage, physical pointer ownership, disappearance, and persistence remain open. Hidden-window OpenGL startup exits `0xC0000005`; final proof used the normal visible production window and in-process Rml activation. |
| 2026-08-16 | FIFO event ordering against original `GameModel::eventMessage` / `onMessageButtonCmd` | Added the focused `ui_hud_fifo` target; controller/action tests pass `4/4`, including invalid response gating and front-pop order. The final literal copied-save probe enqueued DB-backed `New Gnomes` yes/no then `Invasion` acknowledge-only, rendered the centered first and second prompts, activated Yes then Continue through live Rml listeners, and produced the modal-free after frame. Frames: `build-wave8-root-msvc/physical-hud-event-queue-20260816n/event-queue-first.png` SHA-256 `4777B6BD18E510EA996D175840573DB3DAB832ADBC12DC027B7AE48D21F36437`, `event-queue-second.png` `D63F222711801A34102540246F6556E406AB49287181B8BD8FCA7EA43968E874`, `event-queue-after.png` `3B76D6BD6F027FA205EE3277295CE17CFF89A2CE9CF4B21F201B2EDBE6BE8B75`; trace `EC2EC3663C735326728AD50A6D1C507227B2E7DA9FED9FA00AA075573865E34A`, lifecycle `2A9C27610B75CE6ADF8A29BDA14C4547188311652045EC87F759B4F79F38BE49`, executable `Ingnomia-event12.exe` `3F51D25E90F57FE2B7B264D6C8D8F0E4EAB48D2FEDF30B1ABC82021BF53B4841`. | FIFO is closed for these two DB-backed sources; all-source breadth, physical pointer ownership, persistence, and inherited copied-save parse warnings remain bounded follow-ups. |
| 2026-08-16 | Acknowledge-only event source and original `MessageCmd` contract | Re-inspected the original `GameModel::onMessageButtonCmd` split between `ok` dismissal and Yes/No answer, then compared it with the typed `EventResponseKind::Acknowledge` branch and existing controller test. Extended only the opt-in probe to use the DB-backed `EventInvasion` success message (`Invasion`, `A force of 1 goblin has arrived.`) and activate the live `hud_event_ack` button. The centered production frame is `build-wave8-root-msvc/physical-hud-event-ack-20260816k/event-ack.png` (SHA-256 `39CEA5B05B4296A1922AA8168A9827CECEB9215472EDAE946E5F1B2F6A2C89C8`); the post-Continue frame is modal-free and retains three gnomes (`event-ack-after.png`, SHA-256 `C54DA580B84B3BBB65EA7A1DEF4B4D092386720CFCACCF5BBDF97E3BEA89BC27`). The trace SHA-256 is `E8E07CE3E53BAA8F0A1F4C9E59B514EE82BC87D7D462C4384CC3D15A02B8088A`, lifecycle SHA-256 is `8EA6C32F2CD69AD902B17EBC85A65C411881033C45B504235F6FD4C64B929422`, and copied `game.json` stayed `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`. | Acknowledge-only dismissal is closed for the bounded event slice; scheduler breadth, physical pointer ownership, multiple-prompt ordering, and event persistence remain open. |
| 2026-08-16 | Event modal geometry and original centered `LayoutMessage` contract | Re-inspected the current HUD stylesheet after the prompt frame showed the shared absolute `.c-modal` rule pinning the event surface to the upper-left. Added a HUD-only relative-position override, the original 500x300 min/max geometry, compact flex action-row layout, and a static geometry verifier. The `stage_content` target copied the source stylesheet into the production content root. The centered real-save prompt frame is `build-wave8-root-msvc/physical-hud-event-prompt-20260816j/event-prompt.png` (SHA-256 `1160F469CE5A03C565B50817727A1B20D126302B408098FDC4F56FC7C4573F61`); the post-Yes frame has no modal and four gnomes (`event-after-response.png`, SHA-256 `9093255B4ACF2803CFA32D313A05CFC5541CB98F61643DC79423A0D7418E3ACF`). The trace SHA-256 is `9E800FFEDE059D1F2C167185F044249B1D3D6BA5EBD168331AA44D876D984425`, lifecycle SHA-256 is `FA7E787CE4BCCB1527D29AA4717BFA7DDF1C66B6583F297BCC5CA3A40DC234E5`, copied `game.json` stayed `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and the alternate current Release executable is `88AF172F80CCC75CC317BF16FD9A7B04BD9E07FECACDC88CE9EAE3D3B239AD6D`. | Centered geometry is closed for the bounded event slice; source breadth, physical pointer, multiple-prompt ordering, acknowledge-only events, and event persistence remain open. |
| 2026-08-16 | Event prompts and original `GameGui.xaml` `LayoutMessage` contract | Compared the original centered title/body/Ok-or-YesNo message surface and `GameModel` pause/answer/dequeue semantics with the current typed HUD prompt route. Added dialog role, modal/label ARIA metadata, `aria-hidden` projection, and first-action focus on a new prompt; HUD CTest passed `5/5`, and both HUD static/integration verifiers passed. A fresh real-save production probe created the DB-backed `EventMigration`, handed its authoritative title/body through the existing `EventConnector::onEvent` path, and captured `New Gnomes` with Yes/No over the dimmed world (`build-wave8-root-msvc/physical-hud-event-prompt-20260816h/event-prompt.png`, SHA-256 `A910A6C17AAAD4A2BB7F3260CA7BB44CF73592DCC877BB395F6E269959D6EA2F`). The live Rml Yes activation trace records `event_prompt_yes activated=true` and hashes to `2EACB591EB6EAB10134B2A6ED98D69FD4C6ED2F264EBB37429F4CABE1B45824D`; the post-response production frame has no modal and four gnomes (`event-after-response.png`, SHA-256 `82A12B9B03A79F30E5BE9634393EB1A9187952656E2981B111DCED341D566700`). The lifecycle trace is `23024C12665BF3B302C558A7562EE77367CFE25023CDB5446E365E4DF3920C21`, copied `game.json` stayed `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and the alternate-linked current Release executable is `88AF172F80CCC75CC317BF16FD9A7B04BD9E07FECACDC88CE9EAE3D3B239AD6D` because the original executable was locked by two prior no-window diagnostics. | Prompt/response/accessibility is closed for this bounded slice; the modal remains visibly top-left instead of the original centered 500x300 geometry, and source breadth, physical pointer, multiple-prompt ordering, acknowledge-only events, and event persistence remain open. |
| 2026-08-16 | Creature inventory and original `CreatureInfo` Inventory block | Re-inspected the original eight-cell Inventory block and the live producer contract. Added `Creature::inventoryItems()` -> `Inventory::designation()` projection with an explicit `inventoryReported` flag, compact text rows, and localized `No carried items`; no icons or missing assets were fabricated. Inspector CTest passed `3/3`, `verify-inspector-rml.cmake` passed, and `verify-inspector-integration.cmake` passed. The rebuilt production executable SHA-256 is `7686A91D3FEC0A638C43F37B886A22C388F894F868E2BD7A43B20F6BCE607E68`; the desktop-capable literal copied-save frame shows `Fistelvase` at `X 50 Y 49 Z 92`, `Right hand | Pickaxe | Pine`, `Inventory`, and `No carried items` (`build-wave8-root-msvc/physical-inspector-creature-inventory-20260816c/creature-inventory-empty.png`, SHA-256 `EB2A8CCA582458278A13A5C5B62969BA3796FAF379A14F002A44F8E153D7AE81`). Production trace SHA-256 is `207D2499247A2713461B514F505E33BA95526868986E0D0F86E4F1B66087B960`; lifecycle trace SHA-256 is `8FF60FF2F192A68A8BE0FF015AFAF359D544848DDE971F25FC5BFF881A460`; copied `game.json` remained equal to source at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`. The save had zero carried items, so only the truthful empty state is runtime-proven; the original icon grid and non-empty row path remain open. | Empty-state/text projection is closed for this slice; keep Row 12 open for exact icon parity, non-empty inventory evidence, skills/actions, actual disappearance, and persistence. |
| 2026-08-16 | TileInfo priority save/reload and original `JobPriority`/serialization contract | Re-inspected the original `JobPriority` binding, `Job::serialize()`/loader, and the existing EventConnector/GameManager save lifecycle. The opt-in production probe froze a copied real save, activated the live inspector `Mine` and `Raise priority` controls for tile `15050` (`50 50 1`), saved slot 2, ended the world, reloaded slot 2, and reprojected the same tile. The input had 35 jobs and no target job; slot 2 contains the new `Mine` job with `Priority: 1`, and the reloaded production framebuffer visibly shows `priority 1`. Frame SHA-256 `8FD74946143A6A284A4E006E64ED1627ED67F7452EE904243F61D2ED069652C1`, production trace SHA-256 `223D1E9F146098A64F9F7400EA74175FC936F3A0F91725BD1BB70F54E37A9D39`, lifecycle trace SHA-256 `31F90CA5700DD991CCBEB259202D3F9C9014A9C21F26815C5B4850717F9AB62D`, executable SHA-256 `476BA043102DE73C8E3D9B8D819FEF836CA69A90726ACA9EF89BB6F6CA3BB7F5`, copied input `game.json` SHA-256 `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, no process remained, Inspector CTest passed `3/3`, and both static inspector contracts passed. | Priority save/reload is closed for this real inspector mutation only; physical pointer, completion/disappearance, placement/cancel, selection changes, and other surfaces' persistence remain. |
| 2026-08-16 | TileInfo priority-9 boundary and original `JobPriority` contract | Re-inspected the original binding and current Rml projection, then added an opt-in repeated-raise probe that uses the existing queued terrain route. A copied real-save Mine job reached priority 9 after nine real raises; the tenth Raise attempt was rejected as `activated=false`. The final production frame is `build-wave8-root-msvc/physical-inspector-priority-boundary-20260816/priority-boundary-upper-final.png` (SHA-256 `4921FE828D4972AFF0C3CC23D67BB0FB3CAA12C96544EE9FF359884B3C11688F`), trace SHA-256 `205D6C4DED4EAB496F27C6015E69E4AE628C6056F44C6136564ABD3C83422ABC`, final Release executable SHA-256 `E4BEE7153EB3E4E4151792592BF52A0C3CFE1FC5200816D47131855FB9C3E824`, copied `game.json` equal, and no process remained. | Both priority bounds are closed for production state projection; physical pointer ownership, completion/disappearance, placement/cancel, and persistence remain. |
| 2026-08-16 | TileInfo priority-0 boundary and original `JobPriority` contract | Compared the original active-job priority binding with the current authoritative `Job::raisePrio`/`lowerPrio` clamp contract. Added producer/adapter flags for the [0,9] bounds, native Rml `disabled`/`aria-disabled` projection, and a verification activator that honors disabled controls. Inspector CTest passed `3/3`; final Release executable SHA-256 is `EC689AD2CD02A602574113ED1D98E394C7D2FD76F48FFFF3F8BC06042FBDC9E5`. The isolated real-save route created Mine at tile `15050`, then refused `tile_lower_job` with `activated=false`; the post-composite frame is `build-wave8-root-msvc/physical-inspector-priority-boundary-20260816/priority-boundary-gl4.png` (SHA-256 `D998145C0FD45E216A5BFC9D302983B77566D84B5183576BC3BA33D98E8619AB`), trace SHA-256 `FFFF8FA53886A2CBD1558E692DF1F52926C3074002C97DC9F2F55671DED909EB`, copied `game.json` equal, and no process remained. | Lower-bound implementation/runtime proof is closed; upper-bound runtime frame, physical pointer ownership, completion/disappearance, placement/cancel, and persistence remain. |
| 2026-08-16 | Build action affordance and original `BuildItemTemplate` command contract | Compared the original `BuildItemTemplate` (`Fill Hole`, `Replace`, `Build`) with the current generated Rml cards. Added a localized explicit Build action for every row, preserved typed `BuildAction::Build` and the queued `onCmdBuild` route, and added flex wrapping so the compact terrain row does not clip the new control. HUD focused CTest passed `5/5`; final Release executable SHA-256 is `CF4F21632EFE62032BB0018421508786734CC37F0156AAADBDB20B8B447350CA`. The copied real-save trace records Fill Hole, Replace, and Build activation as `true`; final production frame `build-wave8-root-msvc/physical-build-explicit-build-20260816e/explicit-build.png` (SHA-256 `2093110E83DE989ADB478938C4C7F5AF45340F9B75E2E7AA665C64948308A37B`) visibly shows complete action buttons and `Active tool: BuildWall`. | Physical pointer proof for the new Build button, second-component material variation, placement commit, save/reload, and broader DB-category coverage remain open. |
| 2026-08-16 | TileInfo job completion/disappearance attempt and original active-job lifetime contract | Compared the original active-job bindings with a real worked Mine job `1001919` at `53 50 92`. The literal before/after production frames remained unchanged after 30 seconds because the authoritative job payload reported `RequiredToolAvailable=No`; the gnome payload separately showed a held Pickaxe, so no GUI workaround or fabricated completion was introduced. Before SHA-256 is `F3395B73925EC161149D8D2F0BF89E0A52E6EF8F20933105A071C5D7D3875DDD`, after is `8949AF8EC9262C2213422DCA3F6185375A93FEDFF156045CB37C7E390857401B`, trace is `875D7F44AA573486B78E258464E84A239E89495FBF4C03A9086C04C9276B2AE8`, copied `game.json` stayed equal, and no process remained. | Completion/disappearance remains blocked by this authoritative simulation/data discrepancy; re-run with an available tool or corrected state, plus physical pointer and persistence gates. |
| 2026-08-16 | TileInfo priority state refresh and original priority binding | Compared the original active-job priority binding with the current Rml action and queued bridge. The first literal probe showed `Raise priority` activated but the panel remained stale at priority 0; `EventConnector::onTerrainCommand` now refreshes `AggregatorTileInfo` after cancel/raise/lower on the existing game-thread path. Focused Inspector CTest passed `3/3`, Management 6B and Inspector integration contracts passed, and Release executable SHA-256 is `7A808D0799C9B3362E43F7FF963C50EBF6D2C54E3F5E6D18F319CE5FE205EDC2`. A copied real tile `15050` Mine job raised through the production Rml action now visibly shows `priority 1` (`build-wave8-root-msvc/physical-inspector-job-priority-fixed-20260816/job-priority-fixed-final.png`, SHA-256 `D090846AAC876D27D1E07BD2155534749C7F6FB5352FF6FE340C1A36D1AC7027`). Trace SHA-256 is `2EECC375F6861E803C46A312D915B1CE486B344B9216007FE2B8AC74BCE9A904`; copied `game.json` remained equal at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. | Raised-priority state refresh is closed for a real terrain job; lower/upper priority boundaries, physical pointer proof, completion/disappearance, placement/cancel, and persistence remain open. |
| 2026-08-16 | TileInfo queued cancel and active-job state refresh | Compared the original active-job payload surface with the current Rml cancel binding and queued `EventConnector::onTerrainCommand` mapping. Against copied real tile `15050` (`X 50 Y 50 Z 1`), the opt-in production run activated `tile_mine`, then `tile_cancel_job`; the final framebuffer shows no Active job section and the restored `Mine wall` action (`build-wave8-root-msvc/physical-inspector-job-cancel-20260816/job-cancel-final.png`, SHA-256 `42691F776D992495BE892E3E499D541F99C12853275F21295EC4984B4B814F1B`). Trace SHA-256 is `BEBE1BCA257827862D81940040F94F2E66FA732D2FD00AF56654B6C9DAACE7FD`; production executable SHA-256 is `DDA9A1F15F85FAF519F0BD67BF37F57E63C7AFB6538E21D630EE83CB1DC3D9D5`; copied `game.json` remained equal at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. | Queued cancel/state refresh is closed for this real terrain job; physical pointer proof, priority controls, completion/disappearance, placement/cancel, and persistence remain open. |
| 2026-08-16 | TileInfo active-job information and original `WorkablePosition`/`RequiredItems` bindings | Compared the original active-job bindings with the current `GuiTileInfo` producer, `TileInspectorState`, and Rml panel. Carried the already-authoritative `workPositions` and `requiredItems` values through the existing adapter and rendered compact job fields. Inspector CTest passed `3/3`, including new IDs/source checks; Release executable SHA-256 is `739EF4DD212A3F8587B32F05A300C40334C7796846D99279AB234BE87DA9E469`. A real Mine tile visibly shows `Work positions: false` (`physical-inspector-job-fields-20260816/job-fields-final.png`, SHA-256 `AE9E50C4B78A72C7A7862FDCFCA2B3B5C8166AD12A263BE2267887323D767E74`, trace `27A34112264220FE96852E1A118D46D7B3EFEAF3E09306634A42C4FD4F815B43`), while the real workshop tile visibly shows `Work positions: true` and `Required items: RawWood | any x1` (`physical-inspector-job-required-items-20260816/job-required-items-final.png`, SHA-256 `C61A630207ECE4A98567C359A57DEBE63E10DDD382556210764525F4B52C688B`, trace `325EB94492792030E5B1BE9BFFDB55CD20A416213FE7817824D990110D7BDC0D`). Copied `game.json` stayed equal to source at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`; both processes exited cleanly. | The original active-job information gap is closed for empty and non-empty required-item payloads; physical pointer proof, completion/disappearance, placement/cancel, and persistence remain open. |
| 2026-08-16 | TileInfo Harvest route and original `CmdTerrain` plant-action contract | Compared the original `TileInfo.xaml` terrain command surface with the current `tile_harvest` Rml action and queued inspector bridge. Used the saved `plants1.json` producer data to identify harvestable non-tree blackberry tile `1000326` (`X 26 Y 3 Z 100`) without fabricating a target. The opt-in production seam selected the tile and activated `tile_harvest=true`; the final production framebuffer shows `Plant: blackberry plant` and the authoritative active `Harvest` job with worker/priority/Farming/tool fields (`build-wave8-root-msvc/physical-inspector-harvest-20260816/harvest-final.png`, SHA-256 `A6C160FF5B166FFDB5B7ABDB64FC2BE9E49051C4035A4F0093727052A85E577F`). Trace SHA-256 is `BDADB6D013FC0BFDD61E7AAA60E78D24D23802870BA57C7622F40DE7D548DF28`; production executable SHA-256 is `BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`; Inspector CTest passed `3/3`, the Management 6B and Inspector integration contracts passed, the copied `game.json` remained equal at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. | The live Harvest route and queued-job result are closed for this real non-tree plant; physical pointer proof, terrain placement/cancel, completion/disappearance, and persistence remain open. |
| 2026-08-16 | TileInfo Remove Plant route and original `CmdTerrain` plant-action contract | Compared the original `TileInfo.xaml` terrain command surface with the current `tile_remove_plant` Rml action and queued inspector bridge. Used the saved `plants1.json` producer data to identify non-tree, non-harvestable mushroom tile `600470` (`X 70 Y 4 Z 60`) without fabricating a target. The opt-in production seam selected the tile and activated `tile_remove_plant=true`; the final production framebuffer shows the observed `error: unset plant designation` payload and the authoritative active `RemovePlant` job with worker/priority/Farming/tool fields (`build-wave8-root-msvc/physical-inspector-remove-plant-20260816/remove-plant-final.png`, SHA-256 `7148F999ED2018B0F413FAB650829CCCB84A219F614CB73A58F48A873E84EFD4`). Trace SHA-256 is `AAA0239EC86FE6D014B7B78DECB0BA3C890379E87C0551D62CB1E7281FADECF8`; production executable SHA-256 is `BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`; Inspector CTest passed `3/3`, the Management 6B and Inspector integration contracts passed, the copied `game.json` remained equal at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. | The live Remove plant route and queued-job result are closed for this real non-tree plant; physical pointer proof, Harvest, terrain placement/cancel, and persistence remain open. |
| 2026-08-16 | TileInfo Fell route and original `CmdTerrain` tree-action contract | Compared the original `TileInfo.xaml` terrain action surface with the current `tile_fell` Rml action and queued inspector bridge. Used the saved `plants1.json` producer data to identify oak tree tile `1000236` (`X 36 Y 2 Z 100`) without fabricating a target. The opt-in production seam selected the tile and activated `tile_fell=true`; the final production framebuffer shows `Plant: oak tree` and the authoritative active `FellTree` job with worker/priority/Woodcutting/axe fields (`build-wave8-root-msvc/physical-inspector-fell-20260816/fell-final.png`, SHA-256 `6F390A3291B0955FFC3620ACCCB40B47E623C4BF6F1927D7E5B533871FEF6078`). Trace SHA-256 is `9856D91A1888917CC3DA10F47230E3D4A5F0D5510FFC77F698CE390B105C2F34`; production executable SHA-256 is `BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`; Inspector CTest passed `3/3`, the Management 6B and Inspector integration contracts passed, the copied `game.json` remained equal at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. | The live Fell route and queued-job result are closed for this real tree; physical pointer proof, RemovePlant/Harvest, terrain placement/cancel, and persistence remain open. |
| 2026-08-16 | TileInfo Mine route and original `CmdTerrain` contract | Compared the original `TileInfo.xaml` terrain action template with the current `tile_mine` Rml action and queued inspector bridge. Used the saved world binary to identify solid-wall tile `15050` (`X 50 Y 50 Z 1`) without fabricating a target. The opt-in production seam selected the tile and activated `tile_mine=true`; the final production framebuffer shows basalt wall/floor, platinum embed, and the authoritative active `Mine` job with its worker/priority/Mining/Pickaxe fields (`build-wave8-root-msvc/physical-inspector-mine-20260816/mine-final.png`, SHA-256 `A9FA4B495A06F7D44FC6778C02AE272ED7D781BCA258A8C659F7B76EFC9D4E12`). Trace SHA-256 is `8F9AEE02E4972A2B86FAC1DC8225CB6F16CA2493D9464C55F8DAA8F50F0983C4`; production executable SHA-256 is `BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`; Inspector CTest passed `3/3`, the Management 6B and Inspector integration contracts passed, the copied `game.json` remained equal at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. | The live Mine route and queued-job result are closed for this real wall tile; physical pointer proof, Fell/RemovePlant/Harvest, terrain placement/cancel, and persistence remain open. |
| 2026-08-16 | TileInfo Manage route and original `CmdManage` contract | Compared the original `TileInfo.xaml` `CmdManage` bindings with the current `tile_manage` Rml action and queued inspector bridge. Added an opt-in element activation seam only for production verification; normal launches and the existing EventConnector/game-thread path remain unchanged. Against the copied real save, tile `1004550` (`X 50 Y 45 Z 100`) activated `tile_manage=true` and the final production framebuffer shows the live `crude workbench` management surface with queue/catalog controls (`build-wave8-root-msvc/physical-inspector-manage-20260816/manage-final.png`, SHA-256 `A06549C55CCFA0CB649004FA836FC779EB140B13D66A728712D649FBC85EAA1E`). Trace SHA-256 is `42BE2F1DDAFB22F4656003BF57A4325CBEFFD33AD9A1823640C33BC58E9B7B76`; production executable SHA-256 is `BE42628A2AD64601F4B6772DFD39E8A9D58554BF55A00410F4004717B0B297F0`; Inspector CTest passed `3/3`, the Management 6B and Inspector integration contracts passed, the copied `game.json` remained equal at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. | The live Manage bridge is closed for this saved workshop; physical pointer proof, the remaining terrain-action matrix, and persistence remain open. |
| 2026-08-16 | Creature needs and original CreatureInfo needs bindings | Re-inspected the four original needs bindings and the current producers. Removed fabricated monster `100` values and animal thirst/sleep/happiness `100` values; added per-need authoritative availability flags and localized `Not reported` rendering, and removed the population adapter's stale comparison against the retired activity placeholder. Inspector CTest passed `3/3`, the Management 6B integration contract passed, and the final rebuilt production executable SHA-256 is `8670A523F955137F905E2AF2936A7FD3D8CBE3A87D69132E639FE398BCE38335`; final copied-save framebuffer shows live gnome needs `68/68/67/83` with the existing profession/activity/equipment panel (`build-wave8-root-msvc/physical-inspector-needs-20260816/needs-final.png`, SHA-256 `B8FCCDF9FCFF47C497EE7094A451565F5CD6586C2452738ABCE6F5C7821FF28D`). The refreshed trace SHA-256 is `55D2B5FE65B0864EA6EDE7F538DD3F3143E1869170C49E0002DFC5E9AFBD110A`; copied `game.json` remained equal to source at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. The copied save had no animals, so unsupported monster/animal `Not reported` visuals remain source/contract-evidenced only. | The no-fabricated-needs and gnome needs/activity-consumer slices are closed; keep Row 12 open for the original icon/inventory contract, skills/actions, unsupported-creature framebuffer coverage, actual disappearance, and persistence. |
| 2026-08-16 | Creature activity and original CreatureInfo Activity binding | Re-inspected the original Activity binding and the current game producer. Replaced all three fabricated `Doing something. tbi` assignments with `Gnome::getActivity()` for gnomes and an explicit localized unavailable state for unsupported creature kinds. Inspector CTest remained `3/3`; production executable SHA-256 is `9A3D21809132A74E560BDB121AE6717C8114BAC017990A22D25749A9889C74C3`; final copied-save framebuffer shows live `Activity: Mining` with the profession/equipment panel (`build-wave8-root-msvc/physical-inspector-activity-20260816/activity-final.png`, SHA-256 `BE01CFC8DC316C3CA16DBB9F99BF816276AB54135A0779545E775342BFBCB398`). Trace SHA-256 is `25AFAF7FAA81DB31A05DE485061C74F49CFAD5C7DB5C05AD35D29B7A49CC07F0`; copied `game.json` remained equal to source at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. The run used in-process Rml activation because the shell lacked physical foreground pointer ownership. | Gnome activity projection is closed; keep Row 12 open for the original icon/inventory contract, skills, creature actions, physical pointer proof, actual disappearance, and persistence. |
| 2026-08-16 | Creature profession selector and original CreatureInfo ComboBox contract | Compared the read-only Rml profession text with the original two-way `CreatureInfo.xaml` ComboBox. Added the existing profession-list signal to the inspector state, direct teardown-safe listeners for generated live choices, and queued `population.set_profession` through `AggregatorCreatureInfo`. Inspector CTest passed `3/3`; production executable SHA-256 is `2C19E19C50A91F065636BE9A898598921AC4951D342B5A078A3A109C85A17C85`; deployed `inspector.rml` SHA-256 is `52043FC20352954625D3C26CE33CBFE35BF9285C88A1A403192A66B65A3602DF`. An isolated copied-save foreground production run visibly changed `Fistelvase` from `Profession: Gnomad` to `Profession: Farmer`, with selected Farmer choice; final frame SHA-256 is `884F743266971ED020A1B60E755AC83D7094F0FEA8EC5FA10568F9C3C9DA8D11`, trace SHA-256 is `CC303ECA918A403298767B0A0D9107066BDD3334EAEE9580ACC4E6C5DE17BDC5`, copied `game.json` remained equal to source at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. The shell could not provide physical foreground pointer ownership, so this is literal production Rml activation plus framebuffer proof, not a physical mouse-click claim. | Profession selector rendering and authoritative mutation are closed for this slice; keep Row 12 open for skills/actions/activity, original icon/inventory contract, physical pointer proof, actual disappearance, and persistence. |
| 2026-08-16 | Creature inspector and original CreatureInfo contract | Compared `content/rmlui/screens/inspector.rml` with `content/xaml/CreatureInfo.xaml` and the live `AggregatorCreatureInfo` payload. Added supported equipment rows from the existing slot data, reset the cached payload before cross-kind refreshes, and added a queued unresolved-ID clear that only closes an active creature panel. Inspector CTest passed `3/3`; the rebuilt production executable is SHA-256 `6ED3161FBA47D5603915E832C8E6E0830642C40BC65C02697B29C75C5A76BEE3`. A fresh copied-save foreground run physically opened the live tile `X 50 Y 49 Z 92`, selected `Gnome: Fistelvase`, and opened the creature panel; the inspected frame shows live needs/attributes and `Right hand | Pickaxe | Pine` (`build-wave8-root-msvc/physical-inspector-creature-20260816b/creature-inspector-final.png`, SHA-256 `3FB2828D657E2180ECDB857AB5CFEFA84866E99C5AD30DC554B03C806720EE35`). The trace hash is `A7E0FC0B5D59EBB0EB2176982EF4CB939A7C7EE9D1D531B3FCA6C9EE8EA9FB86`; copied `game.json` stayed at `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and no process remained. | Keep Row 12 open for profession editing, skills/actions/activity, actual disappearance proof, and persistence; do not claim the unavailable icon/inventory contract. |
| 2026-08-16 | Root route teardown and original Main/Load/Settings/New Game contracts | Compared the four production RmlUi routes with their original XAML source contracts and rechecked `ShellRmlBinding` document/listener teardown. A real foreground production run completed three Load, Settings, and New Game open/back cycles against copied real save data; route log counts were `shell.main_menu=10`, `shell.load_game=3`, `shell.settings=3`, `shell.new_game=3`, process exit `0`, and copied `game.json` SHA-256 was unchanged. Final frame hash: `892593416D6C5365AD65549E2640C5E9836442CB549DB448621FDD1F6722CCBE`; trace hash: `41103E5C17E1530BF6F550EDBFE72475D2FC39C17426C16E9FDB83A85B582418`. The probe moved the window to `(0,0)` to expose New Game's non-centered Back control. | Repeated route teardown is closed; continue with HUD world-input/selection and external IME/DPI gates. |
| 2026-08-16 | HUD pressed-state parity and original GameGui toggle contract | Compared current `game_hud.rml`/`HudRmlBinding` with original `GameGui.xaml`: the source used `ToggleButton.IsChecked` for pause, speed, and four render overlays, while the migrated buttons lacked authoritative visual/ARIA state. Added `is-selected` and `aria-pressed` projection without changing the typed controller/EventConnector path. HUD CTest passed `5/5`; production executable SHA-256 is `02B956A62B725D45383AA82AFAF441696CDEE880728EBAB0BA4E536CE1E48A22`. Real foreground input on a copied save toggled Space pause, Fast, and Designations; inspected frames are recorded in the Row 9 correction and copied `game.json` remained unchanged. | HUD physical map selection, tool ownership, world/UI click-through, and event/pause persistence remain open. |
| 2026-08-16 | Live Inspect map selection and original GameGui TileInfo contract | Compared the original `GameGui.xaml` TileInfo/selection ownership with `AggregatorSelection` and the queued EventConnector bridge. A corrected production foreground run loaded the copied save directory and physically clicked the live map at client `(1600,820)`; the after frame shows authoritative `Tile 1000332`, `X 32 Y 3 Z 100`, and `Floor: dirt soil` in the inspector while the map remains visible. The process exited without a remaining Ingnomia instance, copied `game.json` SHA-256 stayed `60A00CEEAD93CEBB0D2DCAD2A205B9C10AAE68DBF1E83399F07A340697FAD498`, and frame hashes are `B0616765760091F9ECC39EEFA0943C2BB202E318D033B9E20BE3B242AD099865` and `B3938CA38721C8CF19C25696061118A69766812A15D1E961B701C6DAC1A3CD46` under `physical-map-selection-20260816b/`. | Plain tile selection is closed for this live tile; inspector close/map click ownership, entity and empty-context changes, tool handoff, and event/pause persistence remain open. |
| 2026-08-16 | Original TileInfo terrain actions and live Remove floor job | Re-inspected the original `TileInfo.xaml` terrain template and the current inspector state/bridge. Added typed `RemoveFloor`, `FellTree`, and `RemovePlant` actions, authoritative visibility for the existing `canRemoveFloor`/`canFell`/`canRemovePlant` flags, and queued mapping to the existing `Remove`/`Fell`/`Destroy` terrain commands. Inspector CTest passed `3/3`; production executable SHA-256 is `3549F37E2CBB56707A121D17BE36B0A3503D5E80187825D6979539CE9B03792E`. A fresh copied-save physical run selected `Tile 1000332`, clicked `Remove floor`, and rendered the live `RemoveFloor` job in the inspector; frame hashes and unchanged save hash are recorded in the Row 11 correction. A second copied-save run clicked an empty rendered context and showed `Tile 800000` with no actions; it exited `0` with the copied save unchanged. A third run physically closed and reopened the inspector around the map click; it exited `0` with unchanged copied metadata and the retained frames show the dock consuming Close while the map remains reachable. | The implemented Remove floor branch, bounded empty-context case, and exercised Close/map click-through are closed; exercise Fell/Remove plant/Mine/Manage, entity selection, modal blocking, and persistence next. |
| 2026-08-16 | Continue/load browser state matrix and original LoadGamePage contract | Re-inspection compared production `content/rmlui/screens/load_game.rml` with original `content/xaml/LoadGamePage.xaml`: both retain the two-pane kingdom/save hierarchy, selection, Load, and Back; the migrated surface additionally exposes explicit empty and error panels. Source review found `AggregatorLoadGame` silently skipping malformed `game.json` and `ShellRmlBinding` not rendering `loadGame.error` detail; it also enabled Load for any selected row even though the controller correctly rejected incompatible rows. The production bridge now emits a narrow queued error signal for malformed or vanished metadata, projects a localized `ShellError`, clears it on successful refresh, preserves valid rows when a sibling is unreadable, and gates Load on the selected row's compatibility. Focused shell CTest passed 3/3 and the rebuilt production executable is SHA-256 `04B821A2AB8422CD3D700555D2D898B50CAE4AB76A2452E907C89F2603CB190A`. Real foreground Windows probes used copied 80,130-byte save metadata only: the empty-kingdom frame is `physical-load-empty-20260816db/empty-load.png` (`FEC234DD129A0158BB9CAFCA07577570DC651AEEAA6CC48D164F775DB0A51539`), the empty-save frame is `physical-load-empty-saves-20260816df/saves-empty.png` (`A1AF5E516DC4D36EB60B31F88B19C618828094FA88A3FAF31040E981D843A0BB`), the malformed-slot error frame is `physical-load-save-error-20260816de/save-list-error.png` (`C41BECEE7EBBBB549E8E4E751A3395A91B87D196D88B6ED94DF319675DDBDBA0`), and the incompatible-row attempt remains on the browser with zero authoritative `Start loading world` markers (`physical-load-compatible-20260816dd/physical-incompatible-load.trace.log`, exit 0). A physical Back click at client `(2117,605)` returned to the main menu; `physical-load-empty-20260816db/physical-load-back-click-final.trace.log` records `shell.load_game` then `shell.main_menu`, and `after-back-click-final.png` hashes to `AB118BBD74F19A14EA93735BF3B72F7729FFFB839F2D2125D520624B563A7152`. All test roots were isolated and no source save was changed. | Row 5 is closed for these compatible/incompatible/empty/error/Back cases. Refresh-after-error, repeated teardown, true IME composition, multi-scale DPI, and section-specific live mutation/persistence remain in the ranked queue. |
| 2026-08-16 | Loading Retry action preservation and physical recovery | Re-inspection compared the production `screens/loading.rml` route with the original `content/xaml/WaitPage.xaml` contract: the original was a blocking centered Creating Game / Please Wait surface with no recovery controls, while the migrated route adds a visible authoritative failure banner plus Back/Retry actions. The queued `signalWorldTransitionStarted` callback was found to call `beginWorldTransition(generating, nullopt)` after shell actions had already stored the typed retry envelope, erasing the retry payload. `ShellController::beginWorldTransition` now preserves an existing retry envelope when the controller is already in Loading/Generating, while clearing it for a genuinely new external transition. The focused controller regression and shell RML/integration contracts pass `3/3`; production Release rebuilt with executable SHA-256 `3B479E6361BA31BAC83945449D3CEA0387FD344C8C960FD79F9845315B25C8A5`. A clean foreground Windows run selected the copied real save, removed only its `world.dat`, physically loaded into the live error route (`physical-world-retry-error.png`, SHA-256 `1B0A5D8D75A661431E93FBFF805407956035E58CB444E163107D1C0448538962`), restored the file, physically clicked Retry at `(1745,892)`, and reached the live HUD/world after the second `Start loading world..`; the authoritative log has `failed to load`, then a second `Start loading world..`, `Loading world done`, and `Starting game`. The recovered frame is `physical-world-retry-success.png`, SHA-256 `3EA59C573FEF4A406A2CBC48AD76622DBB64DEF73C9A0207008F85E9F8415C99`; trace `build-wave8-root-msvc/physical-loading-retry-20260816cc/physical-world-retry.trace.log`; process exit was `0`, and the copied `world.dat` was restored. | This closes the loading Retry production gate for this failure class. Remaining work is the ranked queue above: compatible/incompatible/empty load states, repeated teardown, the host external IME/multi-scale gates, and section-specific live mutation/persistence. |
| 2026-08-14 | Inventory hierarchy and classic-park row styling | Category/group/item alignment and expandable groups accepted in native proof; production content restaged | Continue thumbnail coverage and real inventory producer/history proof |
| 2026-08-14 | Inspector and military/diplomacy workbench styling | Inspector dock, 6C label/value fields, footer, and native 6C/inspector proof reviewed | Continue real-world mutation and persistence gates |
| 2026-08-14 | HUD/build producer and interaction pass | Watch-list signal is now bridged into compact clickable HUD rows; build cards carry authoritative thumbnail crops and unavailable-material state; active build selection updates typed tool preview; focused HUD tests 3/3, production Release build, and native HUD framebuffer proof passed | Run a real-save category/placement/rotate/cancel matrix and confirm world selection feedback |
| 2026-08-14 | Inspector and management bridge review | Tile/Creature/Workshop/Stockpile/Agriculture producers and typed queued adapters were source-audited; 6A/6B/6C and inspector focused/native fixture gates are green | Live save mutation, persistence, and world-click proof remain explicit gates; no fixture-only row is called fully complete |
| 2026-08-14 | Shell route proof attempt and teardown repair | Initial native route run exposed `0xC0000005`; explicit route/load-row/modal listener detachment was added before document unload. Rebuilt spike now exits 0 and writes `shell-self-test.log` with main/new/back/load/settings/exit, pause pending, listener teardown, 100 cycles, and 0 GL warnings; pause framebuffer visually inspected | Exercise the same flows against real saves in production and retain physical input/IME/DPI gates |
| 2026-08-14 | Shell focused suite | Fresh MSVC/Qt Core build and CTest 3/3 pass (controller, RML contract, integration contract) after the teardown repair | Keep production real-save and physical-input gates open |
| 2026-08-14 | Diplomacy lower-panel visual pass | Reworked mission builder width/height and classic compact controls; native 6C proof remains exit 0 with `LowerMissionControlsReachable=true`, 100 lifecycle cycles, and 0 GL warnings; lower framebuffer visually inspected with Start mission visible | Keep live mission response and persistence gates open |
| 2026-08-14 | Cross-cutting proof sweep | Re-ran inspector, management 6A/6B/6C native proofs: all exit 0; every proof reports 100 load/unload cycles and 0 OpenGL warnings; all 16 static verifiers pass | Remaining work is live save/persistence and physical-input coverage, not another fixture claim |
| 2026-08-14 | Final build/evidence refresh | Production `build-wave8-root-msvc` Release rebuilt successfully; executable SHA-256 `727F04E0DDAF07AD16B752785127C100FD8991D10BF3A8994DB0CDF6A439E7B7`; shell native proof now pass-log backed; HUD framebuffer `0F338E55640630B9669AE5670FF6594D11E2ED1813281E0A25B35A7394939198`; diplomacy lower framebuffer `C542B75CFDC94C41E912FC79B8C99EC69F00FC163E4830ED20B01B6EEC73A23C` | Live-save, physical-input, and persistence gates remain deliberately listed rather than inferred from fixtures |
| 2026-08-14 | Production real-save/new-world/pause sweep | Release executable rebuilt (`A30E13E5B296CAC4A01B5D601E6E81384FE8FC2EDF2E59E084911D16CDBA4FCA`). Explicit `TheFragmentedLand/1` load completed with `postCreationInit`; post-load framebuffer `real-save-zfix.bmp` is 1200x675 and visibly contains the saved terrain/objects (`04F96D05DF113AE4E7BF6A9E3C80A4EB246FCCBAF6DF6D30FDC98807CF4E2CDA`). Automated new game generated and centered a visible world (`real-new-game-final.bmp`, `AD3D2D911B5775666BDCF7E9446BE2D97265E5D66ED2A7007C2ABA67F3A9E809`). The real-save pause probe drove the HUD Space path through GameManager; lifecycle trace records `setPaused request=false before=true` and `setPaused applied=false` (`pause-probe-lifecycle.log`, `5D79601BAFAB12094C7F7E6955119D677730A099241B5B9D47F97C1506E1A4DB`). | Reverse pause/save/return-to-title, physical input, and save/reload mutation gates remain open; these probes are explicit automation evidence, not full physical parity |
| 2026-08-14 | Tilesheet completeness audit | Parsed all 25 unique DB `BaseSprites.Tilesheet` values: 17 are byte-identical staged PNGs from pinned `origin/gh-pages`; the remaining eight are absent from `origin/gh-pages` and every Git object, with no fabricated substitutions. `content/tilesheet/ASSET_PROVENANCE.md` records the exact exception and case-correct DB spelling. | Runtime locale/scale/contrast/reduced-motion and release-owner asset/licensing confirmation remain open; world art is no longer an untracked missing-file mystery |
| 2026-08-14 | Active legacy/package audit | Case-insensitive audit across active `src`, CMake, `3rdparty`, content staging, CI, README, and tests now reports zero `Noesis`/`XAML`/`NsGui`/`FindNoesis`/PT Root hits; the 310-file legacy surface remains recoverable under `migration-quarantine/` until the live gates above are accepted. | Remove quarantine only after physical-input, persistence, locale/scale, and release-asset-owner gates are closed |
| 2026-08-14 | Build-thumbnail regeneration | `scripts/generate_build_icons.py` regenerated 333 deterministic TGA crops from the current DB and staged sheets with `skipped=0`; provenance now records the current count. | Exercise the production build catalog across every category and retain the eight unavailable-sheet exception |
| 2026-08-14 | Release restage after asset refresh | Production Release rebuilt and content restaged successfully; current executable SHA-256 is `9EAD904755D99ED9DC4F0BA04BB6D06CB46DB9644B8F4607CD9966696C42118F`; deployed content contains 333 build crops plus the 17 PNG sheets and notices. | Re-run live build-catalog visual/placement proof against this current asset-restaged executable |
| 2026-08-14 | Production shell route smoke | Current Release executable (`A79D007C5CB6F880BDDEAE4E14A01B858D62DF90E6A75D05F90B3F1F60D373F2`) dispatched `shell-settings` and `shell-back` through `ShellRmlBinding`/`ShellController`; deterministic trace records both `dispatched=true`. The real Continue path also dispatched and reached GameManager `init`/`postCreationInit` against the newest compatible save. | Physical route input, Load-row selection, settings Apply/Revert persistence, Exit confirmation, and save/reload mutation remain open |
| 2026-08-14 | Production build-catalog visual smoke | Current production save probe dispatched Build and Workshops through the live HUD listeners; inspected framebuffer `build-menu-visual.bmp` (1200x675, SHA-256 `BFC90FB2B822DA489783CD42E0B1D331F14EBDDF2708DFE638494B9A93A15A71`) shows the RCT2-style title bar, selected Workshops tab, three-column workshop cards, and real cropped thumbnails over the saved world. | Item selection, placement, rotation/cancel, invalid-material feedback, and save proof remain open |
| 2026-08-14 | Production build interaction smoke | Rebuilt Release after the HUD tool-state repair; live save probe selected `hud_build_Crude`, delivered `hud_tool_rotate`, sent a map press/release with both events accepted, and dispatched `hud_tool_cancel`. The inspected final frame shows the active tool cleared to Inspect (`build-selection-placement-fixed.bmp`, SHA-256 `C9CD556AA29C7D7530591087D8946F0278F489DEDDF13AB28607656B64D26E4D`). | Placement validation/commit, invalid-material feedback, and save/reload proof remain open |
| 2026-08-14 | Production evidence refresh after accepted inventory styling pass | Release executable rebuilt successfully (`D75AB64B49CC1BADDDA087C0EE788F5B935097BBF39C7C95A3BF99E310C0164A`). All 16 static verifiers pass; focused shell/HUD/management/debug binaries remain green; live inventory capture `live-inventory-actions.bmp` shows the classic category/group/item hierarchy with aligned columns, thumbnails, selected `Sort: total v`, and the watch callback path. | This confirms the current visual baseline only; physical input, live mutation response, and save/reload persistence remain open across rows 9-20 |
| 2026-08-14 | Production management mutation/visibility refresh | Release executable rebuilt successfully (`AB9F6D016675B3BBA3C2D8453F8301C912AF9B69BB757BAFDF60A48B55066EF1`). Real-save probes now show a checked population schedule cell, watched `raw wood` with a HUD watch row, a newly-added military role with readable uniform/assignment controls, and selected masked diplomacy neighbor details; the 6C pane visibility hardening removed the role-detail vertical-wrap/leaked-pane defect. All 16 static verifiers pass. | Live save/reload persistence, physical-device input, discovered-neighbor mission response, and deeper squad/member/priority/material coverage remain open |
| 2026-08-14 | Focused suite refresh after live management pass | Management 6A/6B/6C, shell, debug, and HUD controller binaries pass under the VS runtime (HUD required its Qt debug DLL path); all 16 static verifiers pass; production Release remains linked and staged. | The focused suites do not replace physical-device or save/reload evidence; those gates remain open |
| 2026-08-14 | Production population/inventory mutation smoke | Against the real `TheFragmentedLand/1` save, the population route selected the first live schedule cell and dispatched `population.set_schedule_cell`; the inspected frame shows Knute hour 0 changed to checked Eat (`live-population-schedule-mutation.bmp`, SHA-256 `912471C0F6FF69C48E9BA72C790AD855A2DA77D7A05DDF1D7A9D58CD8658205C`). The inventory route selected the first live leaf (`raw wood`), dispatched `watch.set`, and the authoritative response rendered `[x] Watching selected` plus the HUD watch row (`live-inventory-mutation.bmp`, SHA-256 `F11D286D85487CA8106962868B5A238FCB5BD03C06307885EF3D8FA447A7616F`). | This closes a production interaction/data-response slice for rows 16-17; physical-device input and save/reload persistence remain open |
| 2026-08-14 | Production management workbench visual smoke | Real-save launcher probes dispatched population, inventory, military, and diplomacy through the live HUD listeners. Inspected captures show authoritative population rows, inventory hierarchy/thumbnails, squad/unassigned citizens, and masked-neighbor mission controls; hashes are recorded in rows 16–19. | Physical keyboard/mouse mutation, response/error handling, and save/reload remain open |
| 2026-08-14 | Production save/end/reload lifecycle | Rebuilt Release executable (`24ED9A111F65CB4A9B30D3D9968AEC4F7C9089253E90C89CC9E0D1CB457197BB`). On an isolated copy of `TheFragmentedLand/1`, the queued production path saved slot 2, returned to menu, reloaded slot 2 (`init` and `postCreationInit` both completed), and captured a live reloaded world (`persistence-probe3-reloaded.bmp`, SHA-256 `986445D5C5E141AF820E286A77CFF08176E8D5B8811F55C17501A991B8E1DB89`). Original user saves were not modified; temporary probe copies were removed after capture. | This closes the basic production world replacement/no-stale-world crash gate; row-specific mutation persistence, physical input, and repeated replacement remain open. |
| 2026-08-14 | Inventory watch persistence | Opt-in production probe selected the first authoritative inventory leaf, dispatched `watch.set`, saved slot 2, returned to menu, reloaded slot 2 through the queued game-thread path, and visually confirmed the persisted HUD watch row (`bone shirt : 0 0`) in `persistence-watch.bmp` (SHA-256 `F1D4698413A70B2CFEB345644787E5CFC79DE73774188166BA416B0DC90A68B0`). The copied save root was removed afterward. | Physical search/scroll input and the remaining thumbnail/history audit stay open. |
| 2026-08-14 | Physical pause toggle | Against an isolated copied save, two real Windows foreground Space key events were sent through `user32.dll` to the production window. The lifecycle trace records both directions: `setPaused request=false before=true`/`applied=false`, then `setPaused request=true before=false`/`applied=true` (`build-wave8-root-msvc/physical-pause.lifecycle.log`). The exact process was stopped after verification and the copied save was removed. | Physical mouse route, focus loss, IME, and broader input ownership matrix remain open. |
| 2026-08-14 | Current production rebuild and verification | Release executable rebuilt and staged successfully (`BE9FEF7C10EBF1EDB4308F409196262F768A842333AEBBF44B802121929E96F7`). The 16 static verifiers and seven focused controller binaries pass; no Ingnomia process remains active. | Continue the remaining physical mouse/IME/DPI matrix, thumbnail/history audit, and row-specific mutation persistence. |
| 2026-08-14 | Population schedule persistence | Opt-in production probe opened Population, selected Schedules, selected the first authoritative schedule cell, dispatched `schedule_set_eat`, saved slot 2, returned to menu, and reloaded slot 2 through the queued game-thread path. The reloaded slot’s `gnomes.json` parses with `Knute` schedule[0] = `eat`; trace records `tab=true selected=true changed=true`, followed by completed reload `init`/`postCreationInit`. | Physical population row focus and broader profession/skill mutation persistence remain open. |
| 2026-08-14 | Military role persistence | Opt-in production probe opened Military, selected Roles, selected the first authoritative role, and dispatched `role_add` (`tab=true selected=true added=true`). After save, return to menu, and queued reload of slot 2, `settings/military.json` contained the newly added role (`ID=1001396`, `Name="new role"`); reload completed `init`/`postCreationInit` with no process left running. | Physical military row focus, squad/member/priority/material mutations, and broader save/reload coverage remain open. |
| 2026-08-14 | Current production rebuild and verification refresh | Release executable rebuilt and staged successfully; current executable SHA-256 is `022CE681D129D0EB68646645DCDA4F4849CF1E5153735DABB646A716D5D8FE0B`. | Re-run the complete static/focused sweep after this source refresh; physical mouse/IME/DPI and remaining row-specific persistence gates remain open. |
| 2026-08-14 | Post-refresh verification sweep | After the `022CE681...` Release rebuild, all 16 repository `verify-*.cmake` contracts pass and all seven focused controller binaries pass under the Qt runtime path; no Ingnomia or persistence-probe process remains. | Physical mouse/IME/DPI, additional row-specific mutation persistence, visual scale/locale/accessibility matrix, and release asset-owner/package gates remain open. |
| 2026-08-14 | Authoritative loading lifecycle | Added EventConnector world-transition start/progress/finish signals from GameManager generation/load paths. ShellController now enters `shell.loading`, blocks world input, displays generator status text, exposes retryable failure state, routes successful transitions to `game.hud`, and restores `shell.main_menu` on unload. Focused shell lifecycle test passes; production Release rebuilt and an isolated population save/end/reload probe completed `init`/`postCreationInit` twice with no stale-world process left. | Capture a production loading/error frame and exercise physical retry/back input. |
| 2026-08-14 | Loading lifecycle implementation and rebuild | Production Release rebuilt after authoritative loading start/progress/finish wiring; executable SHA-256 `4558D30EF194FBEA6261E7E7EC865F808521FDD7C70FC038C1A7E5AA5AB19F8B`. All 16 static verifiers and seven focused suites pass. An isolated real-save load, population mutation, save, return-to-menu, and queued reload completed twice through `init`/`postCreationInit`. | Physical loading/retry/back input and a captured transition/error framebuffer remain open. |
| 2026-08-14 | Settings persistence probe | Isolated production run dispatched `settings_mutation queued=true` through `AggregatorSettings`, then refreshed and exited through the normal EventConnector path. The resulting copied `settings/config.json` persisted `uiscale=1.25`, `keyboardMoveSpeed=140`, `lightMin=0.4`, and `toggleMouseWheel=true`; the user settings tree was not touched. | Add physical settings editing/reset/revert and captured scale/fullscreen proof. |
| 2026-08-14 | Loading error framebuffer | Production Release was pointed at an intentionally empty isolated save path; the real loading route rendered `Preparing kingdom`, the authoritative failure banner, and Back/Retry controls in `loading-error.bmp` (1200x675, SHA-256 `465998FDFAB3AED319C86E0444405D2B4D1F39B74FFB37C63DF41683F0F4D308`). The exact process was stopped and the isolated root removed. | Physical Back/Retry input and a successful transition capture remain open. |
| 2026-08-14 | New-game validation hardening | ShellController now validates the authoritative NewGame field ranges (world size, Z levels, terrain controls, densities, animals, gnomes, and start zone), surfaces invalid drafts, and blocks configured start until corrected. Focused shell tests cover an invalid gnome count followed by correction; production Release rebuilt successfully. | Exercise physical form editing, randomize controls, option changes, and save creation in the real window. |
| 2026-08-14 | Loading retry repair | Failed-load capture showed the Retry control was enabled without a retained request. ShellController now retains the typed transition envelope, exposes it in retryable error state, and redispatches it while returning to `Loading`; focused shell test passes and Release rebuild succeeds. | Repeat Retry through physical input and capture a successful retry transition. |
| 2026-08-14 | Final verification after loading/retry/new-game/settings changes | Current Release executable SHA-256 `5D345F2322FC4933CC43338C48EF8A46F5D77178812C104AAF64682448BD3F0C`; all 16 static verifiers and seven focused controller binaries pass, including shell lifecycle, range validation, retained Retry, management, HUD, debug, and release-registry checks; no Ingnomia process remains. | Physical mouse/IME/DPI matrix, remaining row-specific mutation/save gates, visual scale matrix, and package/asset-owner sign-off remain open. |
| 2026-08-14 | Clean New Game runtime regression check | Current Release launched against an empty isolated data root with `INGNOMIA_AUTOMATE_NEW_GAME=1`; lifecycle trace completed `createNewGame` generation and `postCreationInit` without a crash, and `newgame-current.bmp` visibly contains the generated world/map plus active HUD (`SHA-256 459D5971E67BB61EA6751D5C995DF690716886C88467C05DAF51500FF11E9E16`). | Physical form editing/options/validation and save-creation confirmation remain open. |
| 2026-08-14 | Build placement and save proof | Current Release exercised an isolated copy of `TheFragmentedLand/1` through the real UI listener path: `Build -> Workshops -> Crude -> Rotate -> map sweep` accepted 25 valid world-tile clicks, then Save. The copied save created slot 3 with `jobs.json` count 35 -> 51 and 16 new `BuildWorkshop` jobs (IDs `1003313`–`1003328`); resulting `jobs.json` SHA-256 is `3FD58A2177549E88958EDAB4EA32A451A4F2E3FBE9EDA7EAE0FBDE4D92DBCAB6`. Trace: `build-wave8-root-msvc/build-placement.trace.log`. User saves were not modified. | Retain a loaded-world placement framebuffer, exercise invalid-material feedback and physical drag/cancel, and inspect the saved placement after reload. |
| 2026-08-14 | Post-probe Release verification | Temporary Build diagnostics were removed; the current Release executable rebuilt and staged successfully (`3E7D5BFA4AA9042CE7B3BFBCFB83FDAFCA18F0A9C5DBA99A59A2CFB366E9DB9A`). All 16 repository static verifiers pass. Six focused controller binaries pass directly, and the HUD controller passes with the deployed Qt runtime path; no Ingnomia process remains. | Keep physical mouse/IME/DPI, invalid-material, loaded-world placement capture, remaining row-specific persistence, visual scale/locale, and package/asset-owner gates open. |
| 2026-08-14 | Loaded-world Build framebuffer | A fresh Release run loaded the copied real save, opened the production Build panel over the rendered world, and produced `build-wave8-root-msvc/build-placement-loaded.bmp` (1200x675, SHA-256 `835EE76429A5E89013C941C9C924F5ADE22993506C53D980DAF569EB93AB41A1`). The frame visibly contains the live terrain/objects, HUD, movable Build title bar, category tabs, and Close control; the trace confirms the queued Build/workshop/item/rotate/map/cancel sequence. | Capture a daytime loaded-world frame with the selected item card visible; invalid-material feedback and physical drag/cancel remain open. |
| 2026-08-14 | Build placement reload proof | The isolated Build probe saved slot 3 with 51 jobs, then a second production process loaded that exact slot. The reloaded `jobs.json` contains 17 `BuildWorkshop` records (including the 16 newly placed jobs), SHA-256 `8439DCAB49EED896CF3867E7E69B30CDAF131EEF7844F81406163B145B1DA96B`; `build-placement-reloaded.bmp` (SHA-256 `E63D2F45D717597F231C0C39DBC86FCB816CAE72A5D5DFB54F17BC848937757A`) visibly shows the persisted placement markers on the rendered world. | The first probe process returned a non-zero exit during automated shutdown after saving; rerun with a physical/user-driven exit and inspect invalid-material behavior before closing the gate. |
| 2026-08-14 | Save/Exit shutdown repair | The non-zero Save → Exit probe was traced to `MainWindow::onExit()` closing the native surface before RmlUi teardown. `onExit()` now quits the Qt event loop first, and main stops the Game on its owning thread before orderly `QThread::quit()`/wait. A fresh Release probe saved slot 3 with 51 jobs and exited `0`; the retained log has no RmlUi shutdown fatal (`shutdown-probe2.trace.log`). Current executable SHA-256: `2EFD21AF1263EED5DC6A8381D05B3EE534A60BE147987A3FACFBD3021AA1D1DC`. | Physical/user-driven Exit and repeated world replacement still need direct verification; invalid-material feedback remains open. |
| 2026-08-14 | Physical Build launcher | A real foreground Windows mouse press/release at the DPI-adjusted Build shelf coordinate opened the production Build panel against the loaded save; `physical-build-y280.bmp` (SHA-256 `67FA4D6353444E4A6AA3A8D17CA0486C2ECB28E1BB8BCE241B9EDE748F0B3285`) visibly shows the Build control pressed/active, title bar, categories, and live world. | Physical category/item selection, drag, placement, cancel, and Exit still need direct input coverage. |
| 2026-08-14 | Build unavailable-material feedback | Added a typed `unavailableReason` to authoritative HUD build rows, populated it from each missing required item/amount, blocked unavailable catalog dispatches, and rendered the reason in the card detail/title plus a visible HUD status message. Fresh Release build/stage succeeded (exe SHA-256 `FD73F29DF1824E249717BBCAC24DD1F54ACF46D3A2F86264F230D20183E3AC4D`); all 16 static verifiers and 8 focused HUD/management/shell/debug binaries pass, including the new unavailable-build test. | Exercise this state against a real material-starved item in a loaded save and capture the user-facing message; physical category/item and drag/cancel gates remain open. |
| 2026-08-14 | Real-save unavailable-material probe | Against a copied `TheFragmentedLand/1` save, the production HUD dispatched Build → Furniture → Chair and recorded `hud_invalid_build_status=Chair x1` after the unavailable selection; no placement command was dispatched and the process exited cleanly. Current rebuilt executable SHA-256 `FCD2A7BF5CF73F88912754442B9560EFDDC7470E63226CCA09B549E77AB0ACE2`; trace retained at `build-wave8-root-msvc/invalid-build.trace.log`. | The deterministic runtime status is proven; retain a later-timed visual capture of the status text plus physical category/item and drag/cancel gates before closing row 10. |
| 2026-08-14 | Inventory missing-thumbnail treatment | Inventory item/material rows with no authoritative crop now render a centered, accessible initial glyph instead of a transparent blank tile; category/group disclosure rows remain intentionally iconless. Production Release rebuilt and all 16 static verifiers pass; current executable SHA-256 `BC1F23489CB3169C5188123ACE7056360485277FCABAC1A957ACDD89E07065D4`. | The glyph is an honest missing-asset fallback, not a fabricated sprite; complete the remaining DB asset-owner decision and physical inventory scroll/search proof. |
| 2026-08-14 | Physical inventory scroll probe | Against an isolated copy of `TheFragmentedLand/1`, a real foreground Windows wheel event over the inventory list moved the list from the initial raw-wood rows to later stick rows while retaining aligned headers, real thumbnails, the Windows-98 scrollbar, and the live world; capture `build-wave8-root-msvc/physical-inventory-search-scroll-desktop3.bmp` (SHA-256 `621E634DC01D64F153A2BC3B4238A2816C9C5FFF2AF1DF558EBCF917EC89B25F`). The copied save was removed and no user data was changed. | Physical search typing still needs a clean capture; inventory history/request response and save/reload remain open. |
| 2026-08-14 | Inventory search-state retention repair | `Management6BRmlBinding::stateChanged` now restores the controller’s authoritative filter into the native RmlUi form control after asynchronous inventory/category snapshots, preventing refreshes from clearing the field or dropping focus between characters. Release rebuild succeeded (executable SHA-256 `EE76F9093F6D44CFAE449E0ED980624C80A865F47E4CBB253E90082F7485E119`); all 16 static verifiers and 8 focused controller tests pass. | A clean physical typed-search capture is still required; no claim is made for that gate yet. |
| 2026-08-14 | Inventory history request/response and visual panel | Added the typed `inventory.request_history` bridge from the existing `ItemHistory` producer, item-only validation, bounded month series, stale-world target checks, and an RmlUi history panel for the selected item/material. A copied-save production run recorded `inventory_history_request dispatched=true` and `inventory_history_status=14:ready`; the inspected frame `build-wave8-root-msvc/inventory-history-final4.bmp` (SHA-256 `EFF11FB59FC906AD19575A1A456ACDADD15DFFBBE64033BEE9E3E868AF187846`) shows the selected `raw wood` row and readable daily totals/created/destroyed values. The copied save root was removed after the run. | Physical typed-search capture, history persistence across reload, and broader inventory mutation coverage remain open. |
| 2026-08-14 | Inventory history discoverability repair | Moved the History request control out of the initially hidden result panel into an always-visible footer launch row; added compact classic styling; Release content restaged successfully (`Ingnomia.exe` SHA-256 `D340B7F0FC5BCF0573AE7D8940DACEBA47CFA30FE30687BEE10B8EFB158B58E4`). Fresh management 6B CTest passes 3/3. | Physical typed-search capture, history persistence across reload, and broader inventory mutation coverage remain open. |
| 2026-08-14 | Inventory history save/reload probe | On a clean copied real-save root, the first history request dispatched and the process completed save, world end, and slot-2 reload without a crash. The post-reload inventory reopen/request path still reported `status=0:loading` after the authoritative snapshot wait, so the persistence gate remains open and the copied root was removed. | Trace `build-wave8-root-msvc/inventory-history-persistence.trace.log`; diagnose the post-reload history response/target path before claiming persistence. |
| 2026-08-14 | Release rebuild after history probe instrumentation | Production Release links and stages successfully; current executable SHA-256 is `F845039C7BA0437E642F1A3829A0C9E88D2215A989166BF6623C0F6DFA446D01`. Management 6B focused CTest remains 3/3 green and no game process or copied-save root remains. | Keep the post-reload history response gate open until a ready response is observed. |
| 2026-08-14 | Inventory history persistence closure | Delayed the post-reload status sample until after the queued UI response. On a clean copied real-save root, the run recorded `persistence_inventory_history_after_reload requested=true` followed by `persistence_inventory_history_status_after_reload=14:ready`; lifecycle trace shows both reload `init`/`postCreationInit` completions and clean shutdown. The copied root was removed. Final Release executable SHA-256: `01F4D39E5CDA0AC56D141DD538A213891B43BBF2469670B5DB3CBE22111AA9A2`. | Physical inventory search typing and remaining thumbnail/row mutation coverage remain open. |
| 2026-08-14 | Physical inventory search typing | A real foreground Windows click was DPI/client-coordinate mapped to the inventory search field, then `bone` was typed through `user32` key events. The inspected production capture shows the search field containing `bone`, filtered `bones`/`bone` rows, aligned columns, thumbnails, and the history panel; capture `build-wave8-root-msvc/physical-inventory-search.bmp` SHA-256 `5B3B21B923C4C7F381B54001690B166E35E2768110E86EAF4FC64CFF5E0C25A0`. The copied save was removed and no process remains. | Broader inventory mutation coverage and remaining missing-thumbnail/release-owner decisions remain open. |
| 2026-08-14 | Inventory 200% scale capture | A clean copied real save was loaded at `uiscale=2`; the production capture keeps the workbench inside the viewport, preserves the full-width category/toolbar/header/row geometry, shows the centered raw-wood thumbnail and classic scrollbar, and exits cleanly with `inventory_history_status=14:ready`. Capture `build-wave8-root-msvc/inventory-scale-200-final81.bmp` SHA-256 `EC7F150866788407754365A1269C6E169DAF67DDCBE54CE1AC57BECF63DFA223`; executable SHA-256 `01F4D39E5CDA0AC56D141DD538A213891B43BBF2469670B5DB3CBE22111AA9A2`. | This is scale/layout evidence only: the lower footer/history reachability at 200% and the remaining locale/contrast/reduced-motion gates are still open. |
| 2026-08-15 | Physical Build close and side-rail routing regression | The rebuilt production Release executable (`Ingnomia.exe` SHA-256 `61B126FDE5D3F8F35E94EBC2C959C9B8E37C72EC8225B7A9F9EACEEA0C410F1B`) was run against an automated new world with the Build menu open. A real foreground Windows mouse press/release on Build Close closed the window and revealed the live map (`build-wave8-root-msvc/physical-build-fixed-after-close.png`, SHA-256 `FD7A2BAFABAC273E6624CBD27F904FB9EF7EFD9DA27FBFC79E3F35AA12E44D72`). A second real click on Population opened the Population & Work panel (`build-wave8-root-msvc/physical-build-fixed-after-population.png`, SHA-256 `C9A2021DF71FEB53DD7732C9113B6BC6FAD1B01377650A2292B4C27AA30596C6`). Root cause was a duplicate unbound `game.hud` document loaded by the shell route above the dedicated bound HUD; the shell now delegates that route to `HudRmlBinding`. HUD RML and shell integration contracts pass; HUD controller passes with the deployed Qt runtime; no temporary input traces remain. | Exercise the remaining physical HUD/build tool buttons and drag/cancel matrix; keep the broader physical-input gate open. |
| 2026-08-15 | Build parity comparison and material-choice pass | The quarantined original `GameGui.xaml`/`styles/mainmenu/styles.xaml` confirms three build tiers plus per-item required components, available-material selectors, and explicit item actions. The RmlUi Build catalog now carries pointer-free required-component/option state, renders per-component selectors, and retains the selected materials for typed placement dispatch. Production Release rebuilt (`Ingnomia.exe` SHA-256 `26F54622BC7EAF47DE2B5EC7A007A712141A6460B9357DADE3845CF9D1E62D5D`); HUD focused CTest is 4/4, all repository static verifiers pass, and the copied real-save lifecycle trace reaches `init`/`postCreationInit` plus Build/Workshops dispatch (`build-wave8-root-msvc/build-material-selection-framebuffer2.trace.log`). The post-load OpenGL capture did not emit in this desktop session, so the rendered selector layout and physical selector-change/placement proof remain unclaimed. | Resolve the production capture/session limitation, then visually and physically exercise a real item selector and Build/Fill Hole/Replace action matrix before closing row 10. |
| 2026-08-15 | Build action parity routing pass | The original `BuildItemTemplate` exposes `Build`, `Fill Hole`, and `Replace` as separate commands; the RmlUi boundary now carries a typed `BuildAction` in `ChooseBuildPayload`, delegates terrain action buttons through `HudRmlBinding`, and maps them to the existing `EventConnector::onCmdBuild` parameter seam without fabricating new domain actions. HUD RML verification and a fresh production Release rebuild/stage pass; the copied real-save probe reached `explicit_load`, `hud_build_menu`, and `hud_build_workshop` but then exited with Windows `0xC00000FD` (stack overflow) before item/action capture. The GL framebuffer was retained (`build-wave8-root-msvc/iteration2-build-actions.png`) and shows the loaded HUD/map surface, but not the Build catalog. | Diagnose the loaded-save stack-overflow/runtime exit, then re-run a captured selector and Build/Fill Hole/Replace matrix; keep visual parity unclaimed until the catalog is visible. |
| 2026-08-15 | Build catalog re-entrancy repair and captured workshop pass | Load-only and Build-open-only probes were stable; the crash was isolated to RmlUi dispatching delegated select changes while the dynamic workshop catalog was inside `SetInnerRML`, recursively re-entering `HudRmlBinding::stateChanged`. Added a catalog render cache plus an update-time event guard. The copied-save production probe now reaches Build → Workshops → Crude → Rotate → map click → cancel without crashing (intentional timeout leaves the normal window open); captured framebuffer `build-wave8-root-msvc/iteration2-build-actions-visible.png` (1200x675, SHA-256 `5E300840A40C40811D4BAB060D9E286C1DF95B7B27089FCF105469AF430F59CD`) visibly shows the loaded world, Build title bar, category tabs, real workshop thumbnails/material selectors, and the adjacent Selection panel. Final executable SHA-256 `3EF2E9A93889501C48F0E5B8E5FF029C1461B706D5A03891A92CA4FB7A0956BF`; trace `build-wave8-root-msvc/iteration2-build-actions-visible.trace`. | Capture a terrain row with the new Fill Hole/Replace controls and exercise those two actions through the real loaded UI; the current frame proves the selector catalog and crash fix, not the terrain-action visuals. |
| 2026-08-15 | Typed Build action focused proof | Added `tests/ui-hud/build_action_tests.cpp`; direct MSVC run with the deployed Qt runtime passes Build, Fill Hole, and Replace payload assertions. The normal CTest runner still omits the Qt DLL directory for this target (`0xC0000135`), while the same executable passes when launched with the documented Qt `bin` path; controller/RML/integration CTest tests pass 3/3 and all repository verifiers pass. | Keep the real terrain-action framebuffer/physical click gate open. |
| 2026-08-15 | Terrain Build action runtime proof | Added an opt-in production probe that opens the real Wall catalog and activates the first authoritative `Palisade` row's `Fill hole` and `Replace` controls through the same RmlUi listener path. The copied-save trace records `hud_build_terrain_category dispatched=true`, `hud_build_fill_hole dispatched=true`, `hud_build_replace dispatched=true`, and `hud_build_terrain_actions_ready=true`; the retained framebuffer `build-wave8-root-msvc/iteration2-terrain-actions.png` (1200x675, SHA-256 `E0F71332BA64E4C806A76D040C798A2F45B806580AA2BF4221BB4158DD13A130`) visibly shows real terrain thumbnails, centered material selectors, Fill hole/Replace buttons, and the authoritative `ReplaceWall` active tool. Final executable SHA-256 `00C61598931716F740D525A82FC7BE445991A17BB28A55CA4D6C9D6487A3672E`; probe timeout is intentional because the diagnostic leaves the normal window open. | Physical mouse click/drag placement and save/reload for these terrain actions remain open; the queued listener/action and rendered controls are now proven. |
| 2026-08-15 | Native close boundary repair | Physical foreground close of the loaded production window previously destroyed the native surface before RmlUi teardown and returned `0xC0000409`. `MainWindow::closeEvent` now ignores WM_CLOSE and routes through `QCoreApplication::quit`, preserving the GL surface; the rebuilt executable exited `0` in the physical desktop pass. A focused static contract now protects the declaration and implementation. | Focus-loss and broader physical input/DPI/IME ownership gates remain open. |
| 2026-08-15 | Physical Build selection, placement, and cancellation | A fresh copied real save was exercised with foreground Windows mouse input: Build opened, Workshops selected, `crude workbench` selected, the live Selection pane reported `Active tool: BuildWorkshop`, Rotate was delivered, a map click rendered the rotated red placement preview, and a true right-click cleared the preview. Captures: `build-wave8-root-msvc/physical-input-placement3-20260815-placed.png` and `build-wave8-root-msvc/physical-input-placement3-20260815-right-cancel.png`; the process exited `0`. The earlier drag pass also visibly panned the world, proving the world/UI boundary for a drag begun on the map. | Exercise physical terrain Fill Hole/Replace controls, material-selector changes, placement commit and save/reload; synthetic production save/reload evidence remains separate from this physical pass. |
| 2026-08-15 | Physical terrain catalog hit-test and Fill Hole pass | The physical terrain probe initially proved the click was intercepted by an unstyled `hud_build_catalog` scrollbar whose computed hit box covered the 440dp item region. The production CSS now gives `#hud_build_items scrollbarvertical` an explicit 18dp track/14dp slider and keeps the catalog itself non-scrolling. Rebuilt production runtime then routed the real mouse click to `button#hud_build_Palisade`, routed the action click to `button#hud_build_action_FillHole_Palisade`, rendered the red preview after a physical map click (`physical-fill-hole-map.png`, SHA-256 `55BAF055E791378AA0184C825387CAEFB80A1C09C1ADD98F413A50BC524B4718`), and cleared it through the physical HUD Cancel button (`physical-fill-hole-ui-cancel.png`, SHA-256 `CD6806A265398B6B49D5CA3011BEF5E1F513C0E6B2B7C7E6C695FF6FCEB39CB2`). Process exit was `0`; temporary source traces were removed after the pass. | Physical Replace action, right-click terrain cancel, material-selector changes, placement commit, and save/reload remain open. |
| 2026-08-15 | Physical Replace and full-cancel repair | The physical real-save pass routed the Replace button to the live Palisade action and rendered a solid red replacement preview (`physical-replace-map.png`, SHA-256 `0D0E4CA4EE0C5F55696645B430C5987CD4323041B83778F24F641C93D15221E5`). It exposed that UI Cancel reused legacy right-click anchor semantics and left the preview visible; `AggregatorSelection::onCancelSelection` now clears the authoritative `Selection` fully for typed `tool.cancel` while preserving world right-click behavior. The rebuilt executable then cleared the Replace preview through the physical HUD Cancel control (`physical-replace-ui-cancel.png`, SHA-256 `E3B881B7A2AC18F2818B664570B63BC3E928D01FF8F1E8DEC75647F3727C834A`) and exited `0`. | Physical material-selector changes, placement commit, save/reload, and broader category coverage remain open. |
| 2026-08-15 | Physical material-selector and clean production pass | `HudController::selectBuildMaterial` now retains the selected component in the typed build payload; active-build changes dispatch `tool.set_material` through `HudQtCommandPort` to the authoritative `Selection`, and the RmlUi change listener updates the controller without rebuilding the live dropdown tree during its event. The focused Build-action binary passes the active material payload assertion; HUD RML/integration contracts pass. A clean copied-save foreground Windows run opened Build → Workshops, opened a real material selector, clicked a different option, selected the live Crude row, rendered the active placement/map-preview state, cleared it through HUD Cancel, and exited `0`. Captures: `physical-material-before.png` SHA-256 `D5033129A5721D3C504EED50E4553E92787A5FF94FAC14512A84BEC98895B4EF`; `physical-material-first-open.png` SHA-256 `444217406F92A9F7E180A87B312D3A4A9423185FC8DD5C1EA28428238644D667`; `physical-material-first-changed.png` SHA-256 `11974460A1ACFE68929E4FBD24898632E0EB38C9A4FCE12573CA092AD82DE1D9`; `physical-material-selected-placement.png` SHA-256 `BA43FF445A401E005DF74C1B970480930755A6A1AA232F3F7FDBA63620D098A3`; `physical-material-map-preview.png` SHA-256 `B533F9D337641E92755F671AAE9268D350E0BB56267D0A7BC55C49664788A06A`; `physical-material-cancel.png` SHA-256 `E39730669C997CA13128F3CB2520CE7A373747B691F65F73E31FDF2274916D6D`; final `Ingnomia.exe` SHA-256 `65134FA5D7547FF20B04E2C7AFCA2280C06F3268AD2BFF7C9C74B899C9EE5D5A`. | Physical placement commit, save/reload, second-component/material-error coverage, and broader category coverage remain open. |
| 2026-08-15 | HUD focused CTest runtime closure | `tests/ui-hud/CMakeLists.txt` now injects the configured Qt DLL directory for the Qt-linked executables when CTest launches them. Reconfigured and rebuilt focused tests now pass `5/5` through the normal CTest runner, including controller, Build/Fill Hole/Replace payloads, unavailable-build feedback, RML contract, and integration contract. | This closes the test-runner environment gap only; production physical placement commit, save/reload, material-error, and broader category gates remain open. |
| 2026-08-15 | HUD pause route and production Save-path proof | The HUD Pause listener now delegates to the existing `ShellController::OpenPause` route, which dispatches typed `sim.set_paused` and loads the production `game.pause` document; the prior fallback only toggled the clock state. A clean copied real-save production probe activated `pause-save` through `MainWindow`'s existing Shell RmlUi binding, exited `0`, created save slot `2` with the full 20-file snapshot, and produced byte-different authoritative state files (`animals1.json`, `config.json`, `game.json`, `gnomes.json`, `itemhistory.json`, `items1.json`, `jobs.json`, `stockpiles.json`, `world.dat`). Trace: `build-wave8-root-msvc/pause-save-route-20260815bm/pause-save.trace.log`; the clean rebuilt executable is SHA-256 `506E3B15312D3316E03B9EEF543C5B8EAED8B05289654E542759FCEA1C9C8E95`. | This proves the production pause route and Save command/artifact path, not a physical Save-button click or post-save reload; physical placement commit, save/reload, second-component/material-error, and broader category coverage remain open. |
| 2026-08-15 | Physical workshop placement commit and save/reload | A clean copied real save received real foreground Windows clicks through Build → Workshops → `crude workbench`, then one physical map click. The authoritative slot-2 save contains 36 jobs versus 35 in the source slot, with two `BuildWorkshop` jobs versus one; the new job is `Crude` at `48 51 100` with `Pine`/`Sandstone` component materials. The lifecycle trace records production save, end, and reload dispatches; the reloaded production framebuffer shows the live world with the selection cleared, and the process exited `0`. Captures: `physical-placement-selected.png` SHA-256 `18703219DC49097C639BCF0A92C4008CC2136C2CA9961FF4E73892E1DF575A25`; `physical-placement-committed.png` SHA-256 `E77451A13136A2EB1A564F93EA02CD8C84749F2DF3A29FB588E7346B44F64F13`; `physical-placement-reloaded.png` SHA-256 `06733115A07A675F7B0ACB1B79CD710304C54584072FE678CB38E3987087B174`; lifecycle trace `build-wave8-root-msvc/physical-placement-commit-20260815bn/lifecycle.trace.log`; executable SHA-256 `506E3B15312D3316E03B9EEF543C5B8EAED8B05289654E542759FCEA1C9C8E95`. | The physical placement/save/reload slice is closed for this workshop; second-component/material-error behavior and broader DB category coverage remain open. |
| 2026-08-15 | Production material-error feedback | Against a copied real save, the live RmlUi path opened Build → Furniture, activated the unavailable `Chair` row, and recorded the authoritative status `Chair x1` without dispatching a build. The retained production framebuffer shows the disabled Chair card with `none available` material feedback over the live world (`build-wave8-root-msvc/material-error-20260815bo/material-error.png`, SHA-256 `EEE0F420D68C44ECAC0E7A70678FFFDF5A958271D3EC29058E285EABFD3AE403`); trace `build-wave8-root-msvc/material-error-20260815bo/material-error.trace.log`; process exit was `0`. | Second-component physical selector change and broader DB category coverage remain open. |
| 2026-08-15 | Physical Build category coverage | A clean copied real save received real foreground clicks on all nine production Build tabs: Furniture, Workshops, Containers, Utility, Walls, Floors, Stairs, Ramps, and Fences. Each tab rendered its live catalog over the world; representative captures visibly show furniture, container, wall, floor, and fence entries with real thumbnails/material rows and terrain Fill hole/Replace actions. All per-category captures and the click trace are retained under `build-wave8-root-msvc/physical-build-categories-20260815bq/`; hashes: Furniture `EDDF84F92E233B3C5FFF6501BA62FF6FC23F68496D51CF5FA3EC147D2F42DC68`, Workshops `B7040FE14234145B01C603DE8D0561D0C690B997354579908A0A1C2E16366DC5`, Containers `6D6F72D62BEB11BB482090BD1042D565B2E25E673546FA20771C2E13596B3E78`, Utility `099EDC8356BF8C0BF55AFF242E8FC8D3E6D8BAFA04529F596BB45FC961C2FABD`, Walls `A70431C39D558180D27BE8E6A1D4BA8DAFEB15F99074ECCC2A1E8CB2D8034870`, Floors `5AA67D7B0F5A534AED3B211B6776A69C17C7AEC9FCA89A9B7259088852E72527`, Stairs `54A5013503698A227D5DF68E73A65B34E14B23BF43207AC084452210B0B5C3B9`, Ramps `5DEC8C926A80CE2B22572ED979776EEF1CF53CE612A7D2691F8FF31783530570`, Fences `37F84DA30C833652B5897CF4693AEDFB5F98FA1BCB477505B1C7C66723E82530`; process exit was `0`. | Second-component selection with a genuinely different material, terrain category placement matrix, and broader management/input gates remain open. |
| 2026-08-15 | Pause save completion projection | The production game-thread save now returns artifact-backed success through `GameManager`, emits `EventConnector::signalSaveGameFinished`, and projects typed Saving/Saved/error states through `ShellController` into the existing pause RmlUi surface; authoritative pause requests also clear only when the requested state is observed. Focused shell CTest passes `3/3`, the final clean executable is SHA-256 `B76F98E11ABEC54B5666CF38CEC3990F796B237ECC6C8208482EA0756029FC23`, and the temporary probe hook is absent from source. An isolated copied real save used the live HUD `hud_pause` and shell `pause-save` listeners, exited `0`, created slot `2` with `21` authoritative files, and produced `build-wave8-root-msvc/pause-save-status-20260815bs/pause-save-status.png` (SHA-256 `F58035E2A2032E16DA4B6B36298F4E0C254976A0CE3013B76ED625648737796F`) visibly showing `Paused`, `Simulation paused`, and `Game saved.`; trace: `pause-save-status.trace.log`. | This closes the production Save completion/status projection, not physical Pause/Save button input or reverse pause/return-to-title; those physical lifecycle gates and remaining rows stay open. |
| 2026-08-15 | Physical Pause, Save, and Resume lifecycle | A fresh copied real save received real foreground Windows input at the measured DPI-adjusted client points: Pause `(720,22)` -> screen `(1324,75)`, Save `(1616,827)` -> screen `(2220,880)`, and Resume `(1616,786)` -> screen `(2220,839)`. The Pause framebuffer, Save framebuffer, and resumed live-world framebuffer are retained under `build-wave8-root-msvc/physical-pause-resume-20260815bu/`: `physical-pause.png` SHA-256 `4DA05DFDE93690BF81A9334A11BC9A19A74E37C19698DD2832A7B021E5701DAC`, `physical-pause-saved.png` SHA-256 `09BBCC34F213C2D01063D6FB8333F92BA4F6FCB39A742A0D88E5A2E27C30D122`, and `physical-pause-resumed.png` SHA-256 `FEB986418866E28304EFB336D2FB1BF23C4AA27EE739C1F3FCAECE0A10EC6FB0`. The saved copy created slot `2` with `21` files; the process exited `0` and the temporary input script was removed. | Physical Pause/Save/Resume is closed for this slice; physical Return to main menu, load/reload, focus-loss/DPI/IME matrix, and remaining section rows remain open. |
| 2026-08-15 | Physical Return-to-main-menu confirmation | A fresh copied real save was loaded by the normal production load command, then real foreground Windows input opened Pause at client `(720,22)` -> screen `(1288,77)`, selected Return to main menu at measured client `(1122,689)` -> screen `(1690,744)`, and confirmed the destructive route at `(307,151)` -> screen `(875,206)` on a `2243x1121` client. The retained frames show the live Pause surface, the in-app confirmation modal, and the resulting main menu: `build-wave8-root-msvc/physical-return-title-20260815bv/physical-return-title-paused.png` SHA-256 `100F4BC3DD4741EE62764DC122FEDEC231DA36F031C5EE20184DFEE978FC2742`, `physical-return-title-menu.png` SHA-256 `4B784F3EC37A9EF2786B7AD2F0367A3A11BEE066F7250B321251EF912C0FAD00`, and `physical-return-title-main-menu.png` SHA-256 `0162C1B11B85FA5CFFC460650A976D8ED8F3EB4D29289D534C3CC4EFFB6F04B5`. The process exited `0`; the source slot was untouched and the copied save retained `21` files. Trace: `physical-return-title.trace.log`; the temporary input script was removed. | Physical Return-to-main-menu is closed for this slice; prove explicit world-epoch reset and focus-loss/DPI/IME behavior next. |
| 2026-08-15 | Physical Load browser and saved-slot reload | The clean production Load route received real foreground Windows clicks on a `3231x1631` client: Load at `(1616,930)` -> screen `(2220,983)`, kingdom row at `(1246,677)` -> screen `(1850,730)`, newest compatible save row at `(1846,707)` -> screen `(2450,760)`, and Load selected save at `(1654,999)` -> screen `(2258,1052)`. The retained frames under `build-wave8-root-msvc/physical-load-reload-20260815bx/` show the browser, both highlighted selections, and the live reloaded HUD/map: `physical-load-main-menu.png` SHA-256 `749140FD78013C8750DD25CE52E468888645541BC836CE4E57AF91121E71DC97`, `physical-load-browser.png` `12D49A2E5671CB85D92AFC285A4A02D69DEB1A159008F2245FC949268B8A9A39`, `physical-load-kingdom-selected.png` `71C321AAE4F3AE8D2D472A5185BA127F5CEEA485E1E7C8ACE5B4F22024753400`, `physical-load-slot-selected.png` `19316B0EE0A13CCF37F73E198418D7206206F62891FD3B0E119511253EEDEDE5`, and `physical-load-reloaded-world.png` `3D81C6C1B6136D806FDCDCACD63FD0F6EB73F7BFFEE7E571B8B04D74FB7D2BB5`. The authoritative `log.txt` records slot `2` load completion through world/items/constructions/jobs/gnomes and `Starting game`; copied slots `1` and `2` each retain `21` files and the process exited `0`. | Explicit world-epoch reset and focus-loss/DPI/IME behavior remain open. |
| 2026-08-15 | Load-list DOM identity hardening | Production `SaveSlotId` values remain typed callback payloads while generated RmlUi row ids use indexed transport-safe DOM identifiers, eliminating the production abort encountered when materializing `kingdom/slot` save rows. The clean rebuilt executable is SHA-256 `42A92838A0BE1609B379CF2AE6494331D64B21473F5AE26798D6601515128AC8`; the physical load/reload pass above completed without the temporary diagnostic logging. | Keep compatible/incompatible/empty/error states and focus-loss/DPI/IME coverage under physical verification. |
| 2026-08-15 | Physical host focus, resize, and committed Unicode proof | The production window received real foreground input on an isolated copied save. The focus probe held a world drag, moved focus to another window, released, restored Ingnomia, resized the client from `3231x1631` to `1280x720` at measured 96 DPI, physically toggled Pause/Resume, and exited `0`; captures are retained under `build-wave8-root-msvc/physical-host-input-20260815by/`: `focus-before.png` `CBA40D5FAF03CED37754EA3E3A3A34318BDDBECC1C924C7D4642B865F7286577`, `focus-after-return.png` `E978FB06DE131D38E5DB1AC81E4B523CCD49122F491DAAB838FE0BDDBA0FE6B5`, `resized-live.png` `05F04524D59459EC53A865D15E04B94BE9E956F0A6FED1CFBC11F4301753B0BF`, `resized-paused.png` `3E5450F5B131EF8373CB3B8008404DC6621B676F4BB9C3CE0FA42E847E53B9F7`, and `resized-resumed.png` `69875832A61E3ABCE607E9E5F1FF0F66F71186FE548A6342790B9557C22CDC0A`. A separate real foreground click opened `shell.new_game`; physical Unicode `Δwarf` was received by `MainWindow` and rendered in the focused kingdom-name field (`build-wave8-root-msvc/physical-unicode-input-20260815bz/new-game-unicode.png`, SHA-256 `C83BE013296BFDCA28B946A467EC13153040253CAE690A52909057243568B7CE`); the probe process exited `0`. The temporary diagnostics and probe scripts were removed before the clean rebuild (`Ingnomia.exe` SHA-256 `B1F0E279919687CD141387D6A247B1EE0D9EFDE9A21BC55568AF76CA1F95FC21`). | This proves committed Unicode and one-scale resize/focus behavior; this desktop exposes only `en-US` (`0409:00000409`), so a real composition-capable IME and multi-scale monitor transition remain external-environment gates. |
| 2026-08-15 | Runtime world-epoch reset trace | A production queued save/end/reload run on an isolated copied save recorded `signalInMenu(false)` at epoch `0` then `world epoch begin 1`; after the real end transition it recorded `signalInMenu(true)` at epoch `1`; reloading the saved slot completed world loading and recorded `signalInMenu(false)` at epoch `1` then `world epoch begin 2`. The saved/reloaded copied slot retained `21` authoritative files, and the exact lifecycle trace is `build-wave8-root-msvc/physical-host-input-20260815by/data/epoch-persistence.trace.log` with the corresponding production log in the same data root; the probe exited `0`. | The epoch boundary is now runtime-evidenced; keep repeated route teardown and the true IME composition gate open. |
| 2026-08-15 | Final clean production verification | After removing all temporary runtime diagnostics and physical-probe scripts, the production executable rebuilt successfully with SHA-256 `D1D1F3F28C99711DDE5D024D3E5AB6CFFB9E9F16EEFC73F643C1D24310A409FC`; shell CTest passed `3/3`, HUD CTest passed `5/5`, all targeted shell/HUD/host static contracts passed, and `git diff --check` passed. | Continue the remaining compatible/incompatible/empty load-state, repeated teardown, true IME composition, multi-scale DPI, and section-specific live mutation/persistence gates. |
| 2026-08-15 | Physical Settings edit/reset/apply/revert | A real foreground Windows click opened Settings on an isolated copied data root. The live fullscreen checkbox changed the client from `3231x1631` to `3440x1440` and persisted `fullscreen=true`; physical Reset returned fullscreen to `false` and restored the authoritative defaults (`uiscale=1`, `keyboardMoveSpeed=100`, `lightMin=0.3`, `toggleMouseWheel=false`), then physical Apply and Revert completed on the same route. Retained production frames are under `physical-settings-20260815ca/`: `settings-screen.png` SHA-256 `42D5EBF8003FBBB2220DE313A10901F4D03862BDC6AAEA2682B5A9C817ACF49B`, `settings-checkbox.png` `444829E79D90AD0FEAB0E975283E838226232BE844CD6FA97736951CC5272B0C`, `settings-reset.png` `2DB00BCFAA2186D6FA380B739AF7F866110D10A4DD51B66860BCA250EE7476C2`, `settings-applied.png` `669C0F322B6D08A6100A0BFC8ABAE22F5E83A3AB39C36E1EB80090D9881BBC81`, and `settings-reverted.png` `991788739FEE6464774AAB1BE2B7624FFF45BACA6EBB13A5DB3BC5438BE431F9`; the source settings tree was untouched and no Ingnomia process remains. | Physical UI-scale slider mutation and broader option editing/round-trip proof remain open; unsupported rows stay omitted by the existing static contract. |
| 2026-08-15 | Physical loading error and Back recovery | An isolated invalid-save production run rendered the live `Preparing kingdom` error state; a real foreground click targeted Retry and retained the same error frame, but the log does not prove a second retry dispatch, so Retry remains open. A real foreground click on Back at client `(1664,891)` -> screen `(2268,944)` returned to `shell.main_menu`; retained frames under `physical-loading-retry-20260815cb/` are `loading-error-physical.png` SHA-256 `F16DE78842C05D7397376F115581D74BCEE9288ED1C82525C5DE4D6935C378FD`, `loading-after-retry.png` `D69407628FB9EEA112E8E588E989DE4A7625AE7FF4BDD87DBA4675A06BEC0A19`, and `loading-after-back.png` `E45F3D16CE73717BF85B041936D60BA9451FDBA59A7EA241EA060A2546CD5EFE`; the process ended and the copied root was isolated. | Prove physical Retry dispatch and a successful loading transition with authoritative log/frame evidence. |
