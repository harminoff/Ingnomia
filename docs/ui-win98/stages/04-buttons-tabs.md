# Stage 04 — buttons and connected tabs

Updated: 2026-09-24

## Status: verified for shared controls and production pilot

Stages 01–03 were reviewed first; see [their review](01-03-review.md). Stage 04's shared component and New Game pilot exit is met. Stage 05 is ready and has not started. Global manager tab migration remains open under the owners below; physical input remains unverified.

## Implemented

- Neutral command faces and hover, reversed pressed bevels, compensated one-dp label displacement, stable bounds, engraved disabled text, and disabled pointer/tab exclusion. Destructive commands use explicit labels on neutral faces.
- Independent dotted focus geometry and an outer default-action stroke retain the button bevel. The shared arrow command composes its atlas glyph with the focus decorator. High contrast preserves readable command/focus states and the selected tab-to-page join.
- The Qt adapter owns focused-button Space on release and Enter on first key-down. Held/repeated events cannot activate commands again across route changes. Blur, Escape, mouse press, context loss, and document unload cancel pending activation; weak element references protect document lifetimes.
- Explicit `data-default-action` opts an enabled button into Enter handling within the focused document. Focused buttons retain their own Enter action. Text editing retains Space, and multiline/select/specialized inputs retain Enter. Visual primary styling alone does not assign a command.
- Shared `c-connected-tabs` behavior supports horizontal/vertical arrows, Home/End, Ctrl+Tab/Ctrl+Shift+Tab, roving RCSS focus, selected/ARIA state, and associated panel visibility. Hidden/disabled candidates are skipped; losing an active tab repairs selection and page focus. Mounted pages preserve edits.
- New Game is the production pilot; the fixture supplies the horizontal case. Initial focus now lands on World. Escape uses the existing Back path for setup/load/settings. Main-window UI receives Space before game pause; shell key events cannot leak into world commands.
- New Game field/randomize commands now complete after successfully queuing their existing EventConnector updates. The prior pending result never received completion and could leave Start kingdom disabled after an edit. Queue ordering and authoritative application paths are retained.

## Verification

- Production MSVC target builds successfully in `build-wave8-root-msvc-link-priority2`.
- Focused CTest suite: **4/4 pass**, including **82 Stage 04 assertions**, shell controller, RML, and integration contracts. [Full output](../evidence/stage-04/accepted/tests.txt).
- The focused suite uses the real Rml documents, adapter, binding/controller and a recording command port. It verifies single dispatch, repeat/release/cancellation, default ownership, disabled states, document reload, edited-state persistence, hidden-page exclusion, dynamic focus repair, pointer hit dispatch, label offsets, stable geometry and independent context ownership. Its renderer is a layout stub, so it is not framebuffer proof.
- Production New Game probe: **167 Rml assertions plus 13 actual Qt-host assertions**, with **zero pause signals and zero world-key signals** during shell input. Injected QKeyEvents enter MainWindow and the real adapter; field editing uses the production command port. [Trace](../evidence/stage-04/accepted/pilot-normal.trace.txt).
- Production renderer captures cover the gallery at 100/125/150/200%, high contrast at 100/200%, and normal/high-contrast New Game: **8/8 probe runs pass**. [Run manifest](../evidence/stage-04/accepted/runs.json), [source hashes](../evidence/stage-04/accepted/source-hashes.json).
- [Normal pilot](../evidence/stage-04/accepted/pilot-normal.png) shows selected Settlement, focused Back, and enabled default Start simultaneously. [High contrast pilot](../evidence/stage-04/accepted/pilot-high-contrast.png) preserves the connected page join. [Gallery](../evidence/stage-04/accepted/components-100.png) shows the focus ring coexisting with the arrow glyph and only the selected tab panel visible.
- Visual inspection caught inactive fixture panels displaying initially despite passing navigation tests. The panel hidden selector now wins the cascade, and two initial-state assertions prevent recurrence. Earlier `completion/` captures are retained as superseded evidence; use `accepted/` for this checkpoint.
- Design-system verifier and `git diff --check` pass. Existing unrelated full-suite baseline failures are not claimed resolved. No user save was opened.

## Remaining route migrations and limits

| Surface | Peer pages / classification | Owner |
|---|---|---|
| Stockpile | Stock, Allowlist, Settings; Center on map is a command | Stage 09 |
| Workshop | Craft, Queue, Settings; Trade stays capability-dependent/hidden; Center on map is a command | Stage 10 |
| Population | Citizens, Skills, Professions, Schedules | Stages 12–13 |
| Military | Squads, Roles & Uniforms, Target Priorities; hidden diplomacy controls are not exposed as peers | Stage 14 |
| Diplomacy | Neighbors, Missions; hidden military controls remain hidden | Stage 15 |
| Build/catalog | Hierarchical category navigation stays a rail | Stage 17 |

SYS-11 remains open until these migrations pass their feature-stage checks. Other route-owned glyph decorators, button padding overrides and default-action choices require their owner's adoption checks. New Game's overall blue palette is route-owned shell work (Stage 18); shared checkbox/radio/input treatment belongs to Stage 05. The high-contrast gallery still exposes pre-existing low-contrast window headings, secondary report text and Close/tree glyphs; these are explicitly open for the owning typography/report/icon work and Stage 20, so the eight probe passes do not certify full-gallery accessibility. At 150/200%, scrolling is programmatically verified; the initial framebuffer does not show every gallery section. Full application high contrast and native physical keyboard/pointer, host switching and gameplay parity remain in the final acceptance matrix. The current evidence uses injected events, not physical input.

## Design authority

Microsoft's original [Windows 98/2000 Design of Visual Elements](https://learn.microsoft.com/en-us/previous-versions/ms997615(v=msdn.10)) informed the bevels, one-pixel pressed label displacement, engraved unavailable labels and independent focus treatment reviewed in Stages 01–03. This implementation uses the repository's licensed Lato substitute and density scaling; it does not claim pixel identity. RmlUi lifetime/input behavior was checked against the pinned source and [RmlUi core documentation](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/core_overview.html).
