# Stage 00 checkpoint — baseline reconciliation

Date: 2026-09-24

## Completed

- Confirmed nested checkout branch/HEAD and preserved the pre-existing modified documentation file.
- Recorded the MSVC/Qt/RmlUi/FreeType/build/rendering/font/host baseline.
- Enumerated all 17 standard production route IDs plus developer-only `debug.panel`.
- Created the 98-row audit register with original ID, priority, primary stage, historical source paths, path-existence checks, preliminary route scope, and current disposition.
- Rebuilt and launched the actual production executable in an isolated temporary data folder.
- Captured the primary shell and detached Inventory fixture. Added an opt-in detached capture environment variable after the first hook captured only the primary host.
- Repeated the detached fixture launch and verified byte-identical PNGs.
- Added a source-level command-parity map and a staged verification matrix.
- Ran all 13 registered CTest cases after building the previously missing test executables. Eleven passed; two current source-contract mismatches remain.
- Created a legal five-gnome Tutorial Valley test world in an isolated temporary data root, saved it through the production EventConnector path, ended and reloaded it, copied the 21-file save slot to a second isolated root, and loaded that copy in a fresh production process. All 21 SHA-256 entries still match after copied-save load. The copied-world capture uses the actual tutorial surface at z=70.

## Results and limits

The path-existence check found every embedded historical source path at the expected `content/rmlui/` or `tests/` location. This does not establish that each historical observation is still true or that its controller/command path is correct; each assigned stage still must reconcile those details.

The computer-use integration reports that native computer APIs are disabled. Runtime captures therefore prove rendered output on the primary and detached production hosts, but not physical input. The inventory fixture uses synthetic rows; the separate copied-save capture is a generated live tutorial world.

The separately supplied [Windows 98 UI audit](C:/Programming/Repos/Ingnomia2/.codex-remote-attachments/01a0d19e-f0c5-71f2-8cc1-fa1cf41ceb56/c0ea677a-2354-4740-bb6d-ff80ffae8602/1-Ingnomia_Windows_98_UI_Audit.md) and [triage index](C:/Programming/Repos/Ingnomia2/.codex-remote-attachments/01a0d19e-f0c5-71f2-8cc1-fa1cf41ceb56/c0ea677a-2354-4740-bb6d-ff80ffae8602/2-Ingnomia_UI_Triage_Index.md) identify the six source risks. They are reconciled against this checkout below; this is source evidence only and does not claim reproduction of runtime failures. A normal user save was not opened or changed. The generated test save and copied load are verified in the [save manifest](../evidence/stage-00-disposable-save-manifest.txt). The source-slot and copied-slot [surface captures](../evidence/stage-00-disposable-source-load-surface.png) are byte-identical; see the [copied capture](../evidence/stage-00-disposable-copy-load-surface.png) and [load trace](../evidence/stage-00-disposable-copy-load-trace.log).

## Six source risks from the supplied audit

| # | Audit IDs | Current source evidence | Stage 00 disposition |
|---:|---|---|---|
| 1 | SYS-01, SYS-02 | `styles/tokens.json` names `classic-park` but retains the cave schema and dark cave color values; active `base.rcss` consumes dark values while production screen and route styles contain pale classic-park and Windows 98 overrides. Stylesheet link order also varies by route. | Confirmed as source-level theme ownership and cascade ambiguity. Theme consolidation is Stage 02 work. |
| 2 | SYS-10 | `styles/management_window.rcss` applies the same inset colors to `.c-management-rail__button.is-selected` and `:focus`. | Confirmed in source. Separate persistent selection from transient keyboard focus in Stage 04. |
| 3 | SYS-23 | Shared and route-level scroll rules used `track`/`slider`; the production probe reports RmlUi creates `slidertrack`/`sliderbar`. The route rules in `components.rcss`, `inspector.rcss`, `management6b.rcss`, and both `management6c.rcss` theme blocks now use the generated names. | Source defect corrected. The verifier now rejects those invalid child selectors across active production RCSS; Stage 06 still owns route rendering and interaction checks. |
| 4 | SYS-32 | The verifier previously rejected `@media` despite production use and could find expected tokens in comments. | Existing fixes retained: comments are stripped before checks, supported `@media` remains allowed, and temporary-copy negative controls cover active-token and scrollbar failures. Stage 01 sentinel-route/state coverage remains open. |
| 5 | SYS-11, SYS-12 | Military and Population expose three/four peer pages as vertical tab rails, while shared connected tab styles are present. Stockpile/workshop/agriculture include rails with mixed navigation and direct commands. | Confirmed as a source design review item, not a runtime fault. Evaluate small peer property sets in Stage 04; preserve real hierarchies and direct actions as rails/commands. |
| 6 | SYS-15, SYS-16, SYS-18 | Across Stockpile, Workshop, Agriculture and Military, persistent Booleans are represented by native checkbox inputs, custom check buttons and text-symbol buttons; exclusive choices include ordinary buttons; numeric values mix `type=number` and custom spinner fields. | Confirmed as inconsistent control representation in source. Stage 05 must classify semantics and preserve command bindings before normalizing controls. |

Historical guideline check: Microsoft’s [February 1995 interface guidelines, printed p. 175](https://ics.uci.edu/~kobsa/courses/ICS104/course-notes/Microsoft_WindowsGuidelines.pdf) recommend property sheets for related peer property sets. This supports reviewing tabs for peer pages; it does not make a vertical rail incorrect where it represents hierarchy or direct navigation. Stage 04 will apply that scope distinction.

The full registered test run completed: `ui_foundation_registry` fails its expected tool/action counts and `suspend_job` check; `ui_foundation_mainwindow_rmlui_wiring` fails its static assertion for the tile-selection/inspector visibility phrase. These findings are from the current checkout baseline; this UI work does not change the registry or the source path targeted by the wiring assertion. The remaining 11 tests passed.

## Gate

**VERIFIED for baseline scope.** The current branch/build/runtime, 98-item audit register, route inventory, command map, six source-risk dispositions, primary/detached host captures, representative empty/populated and selected/focused/disabled states, open popup, typed confirmation, and copied-save safety are recorded. All 17 non-developer production routes were opened at least once. The representative action traces cover navigation, a staged Workshop Name edit, Inventory immediate toggle, Farm scope-sensitive bulk edit, typed Exit cancel/confirm, and save/load. CTest baseline failures are recorded as 11/13. Known production limitations remain assigned to their delivery stages: NEW-002 focus return, inconsistent Workshop Priority semantics, normal Workshop construction/production, Stockpile editing, Farm job execution/Grove/Pasture states, and loading error/retry. Stage 00 baseline requirements are complete; Stage 01 is still in progress.

New Game, Load Game, Pause, both Settings entry points, and Inventory, Population, Military, Diplomacy, tile, creature, Stockpile, Workshop, and Agriculture routes were opened through their registered production paths. The route-by-route evidence distinguishes live copied-world models from fixtures and lists remaining feature-stage scenarios in the [route coverage checkpoint](00-route-coverage.md).

## Evidence

- Build: [baseline record](../01-baseline.md).
- Primary shell: [stage-00-main-menu.png](../evidence/stage-00-main-menu.png).
- Detached inventory fixture and repeat: [first](../evidence/stage-01-inventory-detached.png), [repeat](../evidence/stage-01-inventory-detached-repeat.png).
- Launch traces: [first run](../evidence/stage-01-inventory-fixture-trace.log), [repeat run](../evidence/stage-01-inventory-fixture-repeat-trace.log).
- Full test run: 11/13 CTest cases passed; see [status](../00-status.md) for the two failures.
- Source save run: [automation trace](../evidence/stage-00-disposable-source-automation.log), [lifecycle trace](../evidence/stage-00-disposable-source-lifecycle.log), and [IO load trace](../evidence/stage-00-disposable-source-load.log).
- Source/copy save load: [21-file SHA-256 manifest](../evidence/stage-00-disposable-save-manifest.txt), [source automation trace](../evidence/stage-00-disposable-source-automation.log), [copied automation trace](../evidence/stage-00-disposable-copy-load-automation.log), [copied IO load trace](../evidence/stage-00-disposable-copy-load-trace.log), and byte-identical [source](../evidence/stage-00-disposable-source-load-surface.png)/[copy](../evidence/stage-00-disposable-copy-load-surface.png) surface captures.
- Route probes: [registered route coverage and limitations](00-route-coverage.md), [isolated Settings run](../evidence/stage-00-route-settings-manifest.txt), and [live Inventory copy/hash manifest](../evidence/stage-00-route-inventory-live-manifest.txt).
- Action coverage: [Workshop Name draft/Apply](../evidence/stage-00-workshop-staged-edit-manifest.txt), [Farm two-of-four scoped plan](../evidence/stage-00-route-agriculture-live-scoped-plan-manifest.txt), and [typed Exit confirmation](../evidence/stage-00-confirmation-exit-manifest.txt).
- Popup: the detached production Stockpile filter options are open in [this capture](../evidence/stage-00-popup-stockpile-filter.png), with fixture-data limits in its [manifest](../evidence/stage-00-popup-stockpile-filter-manifest.txt).
- Shell and transition probes: [New Game, Load Game, Pause, and Settings route runs](../evidence/stage-00-route-shell-runs.txt).
- Stockpile route probe: [live manager capture and copy safety](../evidence/stage-00-route-stockpile-live-manifest.txt); the failed valid-field condition remains explicit.
