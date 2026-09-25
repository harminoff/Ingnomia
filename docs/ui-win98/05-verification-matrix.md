# Verification matrix

This matrix separates source checks, compile/build, packaged launch, renderer capture, in-app probe, physical input, and authoritative gameplay evidence. A pass in one column does not imply a pass in another.

| Stage | Focus | Status | Evidence or open gate |
|---:|---|---|---|
| 00 | Audit/source baseline, routes, commands, legal test save | Verified for baseline scope | All 17 non-developer production routes have been opened at least once; the 98-item source register, command map, six audit-risk dispositions, copied-save provenance, and route limits are recorded. Representative action traces cover navigation, staged Workshop Name draft/Apply, Inventory immediate watch, Farm scope-sensitive bulk edit, typed Exit cancel/confirm, and save/load. See [Stage 00 route coverage](stages/00-route-coverage.md). Feature-stage scenarios remain open by design. Full CTest: 11/13 passed; two checkout-level source-contract mismatches are recorded in status. |
| 01 | Design-system verifier and rendered component-state fixture | Reviewed; shared automated scope passes | Verifier and negative controls pass; generated select/scroll parts are introspected. The full-viewport component gallery is byte-identical across two launches. Inventory, Stockpile, two-of-four Farm scope, Settings, inspector states, and modal sentinels have repeat production evidence. CTest is 11/13 with the same two checkout-level source-contract failures recorded at Stage 00. |
| 02 | Theme ownership and cascade | Verified for shared theme scope | Generated Windows 98 semantic tokens are the single shared palette source; RCSS generation is deterministic and accessibility styles load after route styles. Final production matrix: 21/21 supported surfaces captured. The separate `kingdom_panel` plan could not activate its configured `hud_open_kingdom` element. Route-specific palettes remain owned by later feature stages. See [Stage 02](stages/02-theme.md) and [manifest](evidence/stage-02-theme-manifest.txt). |
| 03 | Shared frame, title, type, icons, groups | Reviewed; shared automated scope passes | Production fixture rendered at 100/125/150/200%; verifier passed. Physical hover/close/host-switch/main-resize checks remain open. See [Stage 03](stages/03-chrome.md) and [scale manifest](evidence/stage-03-components-scale-manifest.md). |
| 04 | Buttons, default/destructive states, focus and tabs | Verified for shared controls and pilot | 4/4 focused tests; 82 adapter/binding assertions; 167 Rml + 13 Qt-host assertions with zero pause/world signals; 8/8 production capture runs. Physical input and downstream manager migrations remain open. See [Stage 04](stages/04-buttons-tabs.md). |
| 05 | Inputs, checks, choices, numeric fields and sliders | Verified shared/pilot scope | 7/7 tests, 78 assertions, Qt host checks, 10 capture runs and live Stockpile authority. [Checkpoint and limits](stages/05-form-controls.md). |
| 06 | Reports, filters, selection and scrollbars | Verified shared/pilot scope | 10/10 tests, 56 assertions, 10000-row baseline comparison, production Qt/capture checks. [Checkpoint and limits](stages/06-reports-scrolling.md). |
| 07 | Dialogs, edit lifecycles, feedback and tooltips | Verified shared/pilots | 10/10 tests, 79 assertions; production shell/Population captures and Qt input; [limits](stages/07-dialogs-editing.md). |
| 08 | Inventory | Verified for automated scope | 10/10 tests, 190 assertions; four-scale captures and copied-save data checks. [Stage 08](stages/08-inventory.md). |
| 09 | Stockpile | Verified for automated scope | 10/10 tests, 90 assertions; four-scale captures; live copied-world rule/template/settings commands. [Stage 09](stages/09-stockpiles.md). |
| 10 | Workshop | Verified for automated scope | Windows 98 property sheet: 10/10 tests, 125 checks; 40/40 live checks at 100/125/150/200% on copied saves; caption on all six managers; physical input open. [Stage 10](stages/10-workshops-trade.md). |
| 11 | Agriculture | Verified for automated scope | Windows 98 property sheet for farm, grove and pasture: 10/10 tests, 407 checks; 59/59 live checks at 100/125/150/200% on copied saves; shared Win98 message box; physical input open. [Stage 11](stages/11-agriculture.md). |
| 12 | Population | Verified for automated scope | Windows 98 property sheet (Citizens/Skills/Professions/Schedules, Close only): 10/10 tests, 189 checks; 32/32 live checks at 100/125/150/200% on copied saves; physical input open. [Stage 12](stages/12-population-professions.md). |
| 13 | Schedule grid | Verified for automated scope | Excel 97 grid with exact scope preview and review: 10/10 tests, 115 checks; 24/24 live checks at 100/125/150/200% on copied saves; physical input open. [Stage 13](stages/13-schedules.md). |
| 14 | Military | Verified for automated scope | Windows 98 property sheet (Squads/Members/Roles/Uniforms/Targets, Close only): 13/13 ctest, 282 checks; 45/45 live checks at 100/125/150/200% on copied saves; physical input open. [Stage 14](stages/14-military.md). |
| 15 | Diplomacy | Verified for automated scope | Property sheet (Neighbors/Missions, Close only) and the Send Mission wizard: 13/13 ctest, 184 checks; 23/23 live checks at 100/125/150/200% on copied saves; physical input open. [Stage 15](stages/15-diplomacy.md). |
| 16 | Inspectors | Verified for automated scope | Creature and object inspectors as palette windows (property inspectors, Close only), crowded tiles chosen by ID, one citizen detail: 9/9 ctest, 408 checks; 33/33 live checks at 100/125/150/200% on copied saves; physical input open. [Stage 16](stages/16-inspectors.md). |
| 17 | HUD | Verified for automated scope | Toolbar, drop-down menus and status bar of the game window, Build palette, tutorial palette and event message box: 7/7 ctest, 222 checks; 46/46 live checks at 100/125/150/200% on copied saves; physical input open. [Stage 17](stages/17-hud.md). |
| 18 | Main menu, new game, saves | Verified for automated scope | Main menu dialog, Custom Game wizard, Load Game (Open dialog model): 4/4 ctest, 199 checks; 33/33 live checks at 100/125/150/200% from the main menu on copied saves; physical input open. [Stage 18](stages/18-main-menu-new-game-saves.md). |
| 19 | Settings, pause, loading | Verified for automated scope | Settings property sheet (Close only), Pause dialog, Loading message box, NEW-004 fixed: 4/4 ctest, 144 checks; 34/34 live checks at 100/125/150/200% plus missing and truncated save runs; physical input open. [Stage 19](stages/19-settings-pause-loading.md). |
| 20 | Scaling, keyboard, accessibility | Verified for automated scope | Access keys, tab page keys, Windows High Contrast, 640 x 480 fit, window titles, palette positions: 4/4 ctest, 182 checks; 17/17 live checks at 100/125/150/200% plus a High Contrast run; physical input open. [Stage 20](stages/20-scaling-keyboard-accessibility.md). |
| 21 | Cleanup, migration of Stages 08-09, localization, conformance | Verified for automated scope | Stockpile and Inventory migrated to the Windows 98 standard; every Windows 98 string keyed with access keys in the catalog; stale contracts fixed (canonical ctest 13/13); dead CSS and keys removed; conformance checklist. Stage 21 suite 1971 checks; Stage 09 123, Stage 08 91; live Stockpile 35/35 and Inventory 18/18 at four scales. [Stage 21](stages/21-cleanup-conformance.md). |

## Evidence recorded in this checkpoint

- Production target build: passed, command in [baseline](01-baseline.md).
- Focused source verifier: passed with `cmake -DSOURCE_ROOT="$PWD" -P tests/ui-design-system/verify-design-system.cmake`.
- Verifier negative controls in temporary copies: active palette value removed while the inert source comment remained → expected failure; required `is-invalid` fixture state removed → expected failure.
- Additional verifier negative control: replacing generated `slidertrack`/`sliderbar` selectors with `track`/`slider` fails on the expected selector requirement.
- Route-level verifier negative control: a temporary invalid scrollbar selector in `management6c.rcss` is rejected, while the same text inside an RCSS comment is ignored.
- Runtime: main shell and detached inventory fixture captured; repeat inventory PNG is byte-identical.
- Runtime: a live Tutorial Valley gnome selection opened the detached creature inspector; the captured 21-file save copy remained hash-identical. See [creature route manifest](evidence/stage-00-route-creature-live-manifest.txt).
- Runtime: the same live profile opened Stats and Expertise through the production navigation listeners; the Stats page showed Winkle's attributes/needs and Expertise showed 47 live skill rows. Both additional save copies retained all 21 hashes.
- Runtime: the live tile inspector opened through the production HUD binding in an isolated generated world. All 28 in-app assertions passed, including stable tile selection, stockpile removal with item preservation, authoritative Remove floor job creation/cancel, close/reopen, native-window drag, and replacement-floor picker. The map patch and stockpile were probe-created in memory; no save was created. See the [captures, assertion trace, and run limits](evidence/stage-00-route-tile-live-manifest.txt).
- Runtime: the Agriculture manager opened in Overview on a fresh Tutorial Valley save copy with one in-memory Farm; its readiness held across reselection, and all 21 copied-save file hashes matched. See the [live capture and run limits](evidence/stage-00-route-agriculture-live-manifest.txt).
- Runtime: the Agriculture plan page assigned Strawberry with count two to exactly two of four Farm plots through the production controls. Both plans survived deserialization, the other two plots were unchanged, and the copied save hashes matched a separately verified baseline. The scheduler created no planting jobs during the run. See the [capture, trace, and limits](evidence/stage-00-route-agriculture-live-scoped-plan-manifest.txt).
- Runtime: the Stockpile manager opened through the production tile-selection path on a fresh Tutorial Valley save copy with one in-memory field and one Oak RawWood item. Its row rendered, and all 21 copied-save file hashes matched. See the [live capture and run limits](evidence/stage-00-route-stockpile-live-legal-manifest.txt).
- Runtime: the Workshop manager opened through the production tile-selection path on another fresh copy with an in-memory Carpenter model. The Craft page rendered its catalog and missing-material order state; all 21 copied-save hashes matched. The normal construction workflow was not used. See the [live capture and run limits](evidence/stage-00-route-workshop-live-manifest.txt).
- Component gallery: production-renderer captures and fixture traces recorded across two launches; repeat capture is byte-identical. The in-process report found three generated select parts and four generated scrollbar parts. A live event-prompt dialog is captured twice; see [modal run record](evidence/stage-01-modal-live-manifest.txt). The gallery's visual modal contract card remains static, and the typed destructive-confirmation path is not yet covered.
- After scrollbar selector corrections, a fresh production launch and capture still reported the generated select and scrollbar parts. See the [post-correction capture](evidence/stage-01-component-postscrollbar.png) and [trace](evidence/stage-01-component-postscrollbar-trace.log).
- Route evidence and its limits are recorded in [Stage 00 route coverage](stages/00-route-coverage.md). The live Inventory and Settings runs include [isolated save-copy/hash provenance](evidence/stage-00-route-inventory-live-manifest.txt) and [isolated config provenance](evidence/stage-00-route-settings-manifest.txt).
- CTest: all 13 executables were built and run; 11 passed. `ui_foundation_registry` and `ui_foundation_mainwindow_rmlui_wiring` fail their current source-contract assertions, recorded in [status](00-status.md).
- Save/load and copied-save safety: passed through the normal EventConnector/IO path in isolated data roots; all 21 file hashes match and the source/copy z=70 captures are byte-identical. See the [manifest](evidence/stage-00-disposable-save-manifest.txt), [source capture](evidence/stage-00-disposable-source-load-surface.png), and [copied-world capture](evidence/stage-00-disposable-copy-load-surface.png). Native pointer/key interaction and other gameplay command effects: not verified.

## Stage 08 — 2026-09-24

Stage 08: 10/10 tests, 190 assertions; four-scale minimum-size report/popup/detail captures; rebuilt Stage 06 and Stage 07 regressions; real copied-save recipes, products, history and hash checks. Physical input remains unverified. See the [Stage 08 checkpoint](stages/08-inventory.md).

## Stage 09 checkpoint

Stockpile automated acceptance: 10/10 tests, 90 assertions; Stage 05 7/7 and Stage 06/08 10/10 each. Production captures cover 100/125/150/200% and named modal reviews. The final copied-world run validates rule/template/settings commands and cross-entry suspension with all 21 source/copy save hashes unchanged. Physical OS input remains unverified. See [Stage 09](stages/09-stockpiles.md).

## Stage 10 checkpoint

Workshop property sheet: 10/10 tests, 125 checks (staged settings, Apply/OK/Cancel/Close prompt, queue and trade targeting, no page overflow at the 640x420 minimum at four densities); Stage 04 4/4, Stage 05 7/7, Stages 06-09 10/10 each. Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: 40/40 live checks each, all save hashes unchanged. Caption captured on the other five managers. Registered suite 10/13 (three pre-existing failures). Physical OS input remains unverified. See [Stage 10](stages/10-workshops-trade.md).

## Stage 11 checkpoint

Agriculture property sheet: 10/10 tests, 407 checks (plot selection model, exact plot and planting targets, pending settings with Apply/OK/Cancel/Close prompt, per-record butchering and food targets, pasture type-dependent limits, live-update rebase and conflicts, pages and commit buttons inside the sheet with long lists at four densities); Stage 04 4/4, Stage 05 7/7, Stages 06-10 10/10 each. Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: 59/59 live checks each with farm, grove and pasture model checks; all save hashes unchanged. See [Stage 11](stages/11-agriculture.md).

## Stage 12 checkpoint

Population property sheet: 10/10 tests, 189 checks (sheet structure, sortable list view with selection by ID, per-citizen and bulk skill scope, two-list profession editor with availability rules and unsaved-change prompts, schedule option buttons and cell targeting, pages inside the sheet at four densities); Stage 04 4/4, Stage 05 7/7, Stages 06-11 10/10 each. Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: 32/32 live checks each, all save hashes unchanged. See [Stage 12](stages/12-population-professions.md).

## Stage 13 checkpoint

Schedule grid: 10/10 tests, 115 checks (structure, cell/day/hour/range scopes compared with the exact commands, keyboard traversal of all 24 hours with aligned headings, review, stale review, removed range corner, fit at four densities); Stage 04 4/4, Stage 05 7/7, Stages 06-12 10/10 each. Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: 24/24 live checks each, all save hashes unchanged. See [Stage 13](stages/13-schedules.md).

## Stage 14 checkpoint

Military property sheet: 13/13 ctest, 282 checks (structure and caption, squad order/rename/delete review, transfers by explicit destination, civilian check box, legal uniform choices and scope, target ordering separate from the response, fit at four densities); Stage 04 4/4, Stage 05 7/7, Stages 06-13 10/10 each. Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: 45/45 live checks each, all save hashes unchanged. See [Stage 14](stages/14-military.md).

## Stage 15 checkpoint

Diplomacy property sheet and Send Mission wizard: 13/13 ctest, 184 checks (Unknown facts, unavailable missions, wizard navigation and defaults, review revalidation, single send, running versus returned missions, fit at four densities); Stages 04-14 regressions pass. Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies (one kingdom discovered in memory): 23/23 live checks each, all save hashes unchanged. See [Stage 15](stages/15-diplomacy.md).

## Stage 16 checkpoint

Inspectors as Windows 98 palette windows: 9/9 ctest, 408 checks (palette structure, Unknown values, tabs and Ctrl+Tab, skill sorting, profession drop-down single send, equipment scope and Apply, crowded-tile choice by ID, blueprint/workshop/stockpile/grove pages, fit at four densities); Stages 04-15 regressions pass. Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies (two citizens placed on one tile in memory): 33/33 live checks each, all save hashes unchanged. See [Stage 16](stages/16-inspectors.md).

## Stage 17 checkpoint

Game window chrome: 7/7 ctest, 222 checks (toolbar and status bar placement and states, status messages with unavailable reasons, menus and Esc, armed-tool feedback and pointer, View check marks, Build palette selection/arming/fit, tutorial check boxes, one message box at a time, fit at four densities); Stages 04-16 regressions pass. Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: 46/46 live checks each, all save hashes unchanged. See [Stage 17](stages/17-hud.md).

## Stage 18 checkpoint

Shell screens: 4/4 ctest, 199 checks (menu layout, default and returned focus, wizard navigation and fit, Load Game selection in place, Open availability, F5, double-click); Stage 04-05 tests moved from New Game tabs to the wizard; Stages 04-17 pass. Four production runs at 100/125/150/200% from the main menu on fresh 21-file Tutorial Valley copies: 33/33 live checks each, all save hashes unchanged. See [Stage 18](stages/18-main-menu-new-game-saves.md).

## Stage 19 checkpoint

Settings, Pause and Loading: 4/4 ctest, 144 checks (settings pages and fit, immediate settings, Yes/No message boxes, loading and failure boxes, transition request answered, Pause over the map, Esc resumes); Stages 04-18 pass (Stage 07 exit dialog now Yes/No). Four production runs at 100/125/150/200%: 34/34 live checks each; NEW-004 missing-folder and truncated-world runs 5/5 each without a crash; all save hashes unchanged. See [Stage 19](stages/19-settings-pause-loading.md).

## Stage 20 checkpoint

Keyboard and accessibility: 4/4 ctest, 182 checks (unique underlined access keys, Alt and plain letters, labels reaching controls, Ctrl+Page Down/Up, message box keys, High Contrast colours, 640 x 480 fit, titles for 15 documents); Stages 04-19 pass. Four production runs at 100/125/150/200% plus one High Contrast run: 17/17 live checks each (Alt+L, Esc, Alt+C through real Qt key events; Build palette position saved), all save hashes unchanged. See [Stage 20](stages/20-scaling-keyboard-accessibility.md).

## Stage 21 checkpoint

Stage 21 suite 4/4 (1971 checks: catalog text for 16 documents, access keys underlined once and unique per window and page, none on OK/Cancel/Close, A only for Apply, Alt+letter reaching controls). Rewritten Stage 09 (123) and Stage 08 (91) suites; Stages 04-20, all legacy suites and the canonical build (13/13) pass. Live copied-world runs: Stockpile 35/35 and Inventory 18/18 at 100/125/150/200%, Stage 20 menu probe 17/17 after localization; all save hashes unchanged. See [Stage 21](stages/21-cleanup-conformance.md).
