# Ingnomia — Windows 98 UI modernization
## Staged implementation brief for a coding agent

**Repository:** `https://github.com/harminoff/Ingnomia`  
**Execution model:** 22 ordered stages, numbered **00–21**, with resumable checkpoints.  
**Scope:** All **98** element/control-group findings in the supplied UI audit, plus any additional production-route gaps discovered during reconciliation.  
**Input audit revision:** `bca0bee3acf79d460360c5f6b96fde08db784585` (the audit described this as a `master` snapshot).  
**Important:** That revision is historical context, not an instruction to reset the working repository. Work against the actual authorized checkout and record its current revision.

> Implement this brief. Do not respond with another high-level plan and stop. Begin with Stage 00, execute each eligible stage, satisfy its gate, record evidence, and continue to the next stage. Do not ask for routine approval between stages. When execution must end, leave an accurate, actionable checkpoint rather than implying unfinished work is complete.

## Contents

- [Mission and evidence boundaries](#mission)
- [Non-negotiable safeguards](#safeguards)
- [Design and interaction contract](#contract)
- [Agent execution and checkpoint protocol](#execution)
- [Stage roadmap](#roadmap)
- [Detailed stages 00–21](#stages)
- [Production verification matrix](#verification)
- [Release definition of done](#release)
- [Audit-to-stage coverage index](#coverage)
- [Embedded element-level worklist: all 98 items](#embedded-audit)
- [Resume instruction](#resume)

<a id="mission"></a>
## 1. Mission and evidence boundaries

Make the existing player-facing RmlUi interface feel like **one coherent Windows 98-inspired application**, while preserving Ingnomia's gameplay, information, existing commands, and map-centered workflow. Improve information hierarchy, target/scope clarity, editing behavior, keyboard use, and readable density—not just colors and bevels.

Keep the strongest existing patterns: flat resource reports, construction explanations, contextual object inspectors, profession and squad dual lists, real unavailable states, explicit consequential-action reviews, game-specific sprites, and the equipment paper doll. Do not replace them with dashboard cards or remove functionality to make a screenshot cleaner.

This handoff transforms an existing **source-only audit** into an implementation plan. No new checkout inspection, build, gameplay session, or screenshot review was performed while preparing this plan. The audit's **67/100** is a subjective historical assessment, not a measured failure rate or an acceptance target. Do not optimize a self-awarded score or claim a verified improvement without evidence.

### Sources of truth

1. The user's current instructions and repository-local instructions, including applicable `AGENTS.md` files.
2. Current production code, action/command contracts, actual game behavior, supported platforms, and save compatibility.
3. This brief's design requirements and explicit acceptance gates.
4. The embedded audit, used as a historical checklist to reconcile against current code.

Distinguish **design requirements** from **claims about present defects**. Reproduce alleged runtime faults before reporting them as confirmed. A historical issue already fixed can be closed with current evidence; do not undo a newer correct implementation to recreate this plan's assumed starting point. If a path moved, locate its current equivalent and record the mapping. Do not fabricate missing files, controller methods, routes, or measurements.

The appendix includes the original observation, preservation requirement, fix, and acceptance test for every audit ID. It is intentionally sufficient to use this file without the earlier chat or separate audit attachments. Its observations remain tied to the historical revision until Stage 00 revalidates them.

### Scope exclusions

Do not change simulation balance, invent new mission economics, redesign world generation, change save formats, replace RmlUi, rewrite the application architecture, or build a browser-only replacement UI. Small controller/presenter changes are in scope when needed to expose existing legal operations safely. Adding a missing target-specific command adapter is permitted only after inspecting the command layer, preserving existing contracts, and adding focused tests. Unsupported gameplay capabilities must remain honestly unavailable.

<a id="safeguards"></a>
## 2. Non-negotiable safeguards

### Repository and saved-data safety

- Inspect and record the starting branch, HEAD, worktree changes, submodules, and applicable instructions. Preserve all user changes. Never run a destructive reset, clean, force checkout, automatic stash, force push, or broad reformat as part of this work.
- Use the existing checkout. Clone only when no suitable checkout exists and the environment permits it. Do not switch to the historical audit revision merely because it appears above.
- Keep changes small and reviewable. Commit only this task's changes when repository policy permits; never stage unrelated files. Do not push, merge a remote branch, publish a release, or alter external services without separate authorization.
- Run mutation and save/load tests on disposable copies of saves/worlds. Record provenance and checksums or equivalent identity. Never overwrite the only copy of a player's save to validate a UI change.
- Retain a runnable production application after each integrated stage. Preserve unsupported-platform boundaries rather than deleting a platform to make checks pass.

### Functional and architectural safety

- Preserve public action identifiers, stable object identities, command legality, EventConnector/game-thread boundaries, and supported asynchronous update paths. Do not mutate game data directly from a presentation callback to bypass a command contract.
- Read both templates/styles and the controller/host that implements each interaction. Trace dynamically generated rows, inline styling, event bindings, and detached-host behavior; markup alone is not the whole screen.
- Keep the complete existing action inventory. Moving a command is allowed; losing it is not. Replacing a cycling control with direct selection must use a real stable target, not hidden repeated clicks that can race a refresh.
- Preserve table virtualization or other large-list behavior. Do not replace it with unbounded element creation just to simplify styles.
- Treat command success, pending, rejection, target deletion, stale selection, and concurrent refresh as separate states. Never show final success before authoritative acknowledgment.
- Never silently expand an action from a selected record to all records, from visible rows to every match, or from an individual to an entire role.

### Evidence and tooling safety

- Confirm the pinned RmlUi version and supported syntax from the repository and matching official documentation. Do not assume browser CSS, DOM, ARIA behavior, or web automation APIs are available in RmlUi.
- Use actual generated controls when testing selects and scrollbars. The historical audit flags `track`/`slider` versus `slidertrack`/`sliderbar`; inspect the pinned implementation before replacing selectors.
- Preserve real license, font, and asset-integrity checks. Do not copy proprietary Windows fonts, icons, or assets without a valid license. Prefer the existing licensed font first and original or appropriately licensed chrome glyphs.
- A passing build, fixture, static verifier, screenshot, or accessibility attribute proves only what it actually tests. None alone proves end-to-end gameplay correctness or assistive-technology integration.
- No fabricated screenshots, synthetic screenshots presented as gameplay, invented test results, placeholder success reports, invented progress percentages, or unbacked game data.

<a id="contract"></a>
## 3. Design and interaction contract

These are **project design decisions**, not claims that historical Windows mandated every detail. Stage 02 should encode them as the single production theme contract.

### 3.1 Visual language

| Concern | Target | Avoid |
|---|---|---|
| Chrome | Restrained gray surfaces, square corners, directional raised/sunken edges. | Competing cave, bronze, blue-card, and Win98 skins in the same production theme. |
| Title bars | Compact title, optional small object icon, active/inactive distinction, consistent Close. | Oversized stacked headings, duplicate native/custom chrome, decorative nonfunctional window buttons. |
| Buttons | Neutral command face; reversed bevel on press; independent default and focus cues. | Yellow/orange ordinary hover, layout shifts between states, every action looking primary. |
| Data regions | White inset editable/list surfaces; consistent report headers and numeric alignment. | A separate raised card for every count or field. |
| Selection | A consistent selected-row treatment, with a defined inactive-host variant. | Yellow in one list, navy in another, and focus mistaken for selection. |
| Tabs | Connected top tabs for a small set of peer pages, with the active tab joining the page. | Command-shaped rails for two or three peer views; enormous tab strips for hierarchical catalogs. |
| Groups | Thin group boxes and modest legends; clear label/control alignment. | Nested panels that resemble multiple windows inside a simple form. |
| Special game UI | Retain the plot matrix, paper doll, map labels, item sprites, and build hierarchy. | Flattening all game-specific interactions into generic forms. |

Suggested initial palette values, to validate in the actual renderer: face `#C0C0C0`, editable surface `#FFFFFF`, text `#000000`, active selection/title `#000080` with white text, bevel highlight `#FFFFFF`, bevel shadow `#808080`, dark edge `#000000`. Treat these as normal-theme starting values, not immutable archival specifications. High-contrast variants may override them coherently.

Suggested starting metrics: **12–13dp** essential text, **24–30dp** command/report-row heights, **16–24dp** inline icons, and **16–18dp** scrollbars. Retain larger hit areas/thumbnails where useful. Validate text metrics, renderer scaling, and physical pixel edges before finalizing. Do not hard-code tiny historical dimensions at the expense of legibility. Preserve the existing font until there is a measured reason to change it.

Use semantic tokens such as `surface.window`, `surface.field`, `text.normal`, `text.disabled`, `selection.active`, `selection.inactive`, `border.light`, `border.shadow`, `focus`, and `status.error`. These names are illustrative; integrate with the actual token tooling instead of assuming a particular JSON schema or CSS-variable implementation. Generated RCSS must remain compatible with the pinned engine.

### 3.2 Control meaning and keyboard behavior

| Element | Contract |
|---|---|
| Push button | Performs a command. Focus alone does not invoke it. Enter/Space activation respects the actual input model and cannot dispatch twice. |
| Default button | Has a separate outer indication. Enter triggers it only in the appropriate context, not while a multiline editor or open popup owns Enter. A destructive operation is not the accidental default. |
| Checkbox | Represents one persistent Boolean; unmistakable check mark, clickable label, Space activation. Show mixed state only when real data supports it. |
| Exclusive value group | Exactly one legal value is selected. Radio/combo/toolbar-group semantics describe a setting, not an immediate mission, attack, or transfer command. |
| Property tabs | Use a single tab stop into the tab strip and roving focus. Default plan: Left/Right move tab focus; Enter/Space activates; Ctrl+Tab/Ctrl+Shift+Tab switch peer pages when the host supports them without a shortcut conflict. Programmatic focus alone never changes the page. Record any necessary host-consistent alternative. |
| List/report | Stable ID-based selection, separate focus, direct row targeting, sortable aligned headers, and a documented keyboard model. Preserve selection by ID rather than numerical row index. |
| Combo/popup | Supports open, select, cancel, click-away, sensible focus restoration, host-bound placement, and legal catalog values. Hidden/closed popup entries are not keyboard stops. |
| Numeric editor | Exact typing, integrated stepping, units, validated bounds, safe paste, and explicit empty/invalid behavior. Validation and authoritative values agree. |
| Slider | Useful for approximate adjustment; exact parameters also have synchronized numeric editing. |
| Tree/matrix | Tree expansion is separate from activation. Matrix cells expose row/column identity, keyboard focus, and edit scope. Do not turn a flat catalog into a tree for nostalgia. |
| Tooltip | Supplementary help on hover and keyboard focus. Essential scope, destructive consequences, and errors also appear persistently. |
| Modal | Focus containment and restoration; safe deliberate default; Escape cancels the current dismissible modal; background input is blocked. |
| Escape routing | The topmost relevant popup/modal/tool handles the event once. The same key press/release must not also dismiss a parent, cancel a world tool, or activate the map. |

Every interactive state needs a visual and behavioral contract: normal, hovered, pressed, focused, selected, checked, disabled, default, pending, rejected, and inactive-host. Not every state applies to every control. Test combinations that do apply, especially **focused + selected**, **focused + default**, and **selected + status/error**.

### 3.3 Editing and action scope

| Edit type | Default behavior for this project |
|---|---|
| Stockpile/workshop/agriculture name and priority | Retain staged edits with explicit Apply. Preserve drafts on peer-tab switches. On object switch or Close, offer Apply / Discard draft / Keep editing when dirty; Keep editing is the safe default. Do not discard already applied instant options. |
| Profession and equipment editors | Preserve their explicit Save/Apply and Cancel model. Make dirty state and individual-versus-shared scope visible. Reconcile remote changes before overwriting stale drafts. |
| Persistent toggles | Preserve the actual existing immediate/staged model, label it clearly, and keep it consistent within a group. Do not silently turn an instant setting into a draft or vice versa. |
| Application settings | Retain the documented immediate-save model with Close. A true staged model is out of scope unless deliberately implemented and fully tested; never add a fake Cancel button. |
| New-game setup | Use a coherent Back / Next / Start kingdom wizard by default, with Start on the review page. Preserve drafts, validation, quick-start/tutorial entry points, and legal parameters. |
| Bulk edits | State the object/selection/filter scope and affected count before dispatch. Use scoped confirmation where a safe undo transaction does not already exist; do not invent rollback capability. |
| Trades, missions, deletion | Review exact target and consequences; commit once; revalidate stale targets/eligibility and remain recoverable on rejection. |

For every mutating surface, identify the stable target ID(s), submitted values, affected scope, acknowledgment/rejection path, and refresh behavior. A refresh must not redirect an edit to whatever object happens to occupy the old row index. If targets or counts materially change after a review, recompute and require a fresh review rather than applying a different operation silently. Use existing authoritative command/version mechanisms where available; do not require a wholesale command-bus rewrite.

<a id="execution"></a>
## 4. Agent execution and checkpoint protocol

### 4.1 Working documents

Use `docs/ui-win98/` for this initiative unless an existing equivalent location should be extended. Do not overwrite an unrelated migration worklist. Link existing relevant documentation and record the final paths.

| Artifact | Required content |
|---|---|
| `00-status.md` | Actual HEAD/worktree, current stage, stage table, blockers, last checks, exact next step. |
| `01-baseline.md` | Build/run commands actually discovered and executed; dependency versions; supported hosts; route inventory; baseline failures; data provenance. |
| `02-design-contract.md` | Tokens, component states, keyboard model, layout metrics, host chrome ownership, edit semantics, and supported preferences. |
| `03-audit-traceability.md` | All 98 IDs, current observation/disposition, primary stage, consuming routes, changed files, evidence, remaining gaps. Add discoveries as `NEW-001`, etc. |
| `04-command-parity.md` | Route/control/action IDs, targets, command legality, immediate/staged behavior, old and new access paths, and verification. |
| `05-verification-matrix.md` | Test cases, environments, expected/observed results, pass/fail/blocked, and evidence references. |
| `06-decisions.md` | Small decisions with rationale, alternatives, compatibility consequences, and follow-up. |
| `stages/NN-<slug>.md` | Per-stage scope, implementation summary, tests, evidence, review, remaining audit coverage, and handoff. |
| `evidence/` or existing artifact store | Actual captures/logs with host, scale, runtime revision, scenario, and provenance. Avoid committing large binaries against repository policy. |

Do not create an elaborate new orchestration framework. Markdown plus the existing build/test tooling is sufficient. Reuse existing tests and production capture mechanisms where viable.

### 4.2 Execute this loop for each stage

1. **Resume:** Read repository instructions, status, relevant audit entries, prior stage results, and current diff. Confirm that upstream changes have not invalidated the assumed baseline.
2. **Inspect:** Trace templates/styles, dynamic projection, host input, and the authoritative command path. Confirm the current issue and preservation requirements.
3. **Specify:** Record the smallest implementation slice, affected files, tests, and screenshot/runtime scenarios before editing. Establish a failing regression test where practical.
4. **Implement:** Reuse shared primitives; avoid route-local state definitions. Preserve old behavior until the replacement works. Add focused tests alongside logic changes.
5. **Verify:** Build, run focused tests, run shared regressions, and inspect the actual production route. Capture visual states and exercise commands against disposable real data.
6. **Review:** Compare before/after and the command-parity inventory. Inspect the diff for unrelated edits, stale selectors, unsafe target scope, dead controls, and unsupported APIs. A reviewer subagent may assist but cannot substitute for evidence.
7. **Repair:** Fix findings in the same stage and rerun affected checks. After three unsuccessful repair cycles on the same blocker, document the diagnosed blocker and a concrete next experiment; do not cycle indefinitely or weaken the gate.
8. **Checkpoint:** Update the stage report, audit traceability, verification matrix, and next action. Commit only the task's work where allowed. Advance automatically when the scoped exit gate passes.

Default scheduling is sequential: each stage depends on the preceding stage's integrated output. The roadmap names additional important dependencies. This is not permission to skip prerequisite tests. Small independent investigation or test-writing tasks may run in parallel, but only one integrator owns shared styles, templates, routing, and event contracts at a time.

### 4.3 Status rules and blockers

**Stages:** `NOT_STARTED`, `IN_PROGRESS`, `BLOCKED`, `VERIFIED`.  
**Audit items:** `NOT_STARTED`, `IN_PROGRESS`, `IMPLEMENTED`, `VERIFIED`, `NOT_APPLICABLE`, `BLOCKED`.

A stage's `VERIFIED` status means its **explicit stage-scoped deliverables and gate** passed. It does not automatically close all audit IDs it helps implement. For example, the shared-tab stage can pass after testing its real primitive and sentinel production routes, while SYS-11 remains `IMPLEMENTED` until every required manager is migrated and checked. Record the outstanding consumers explicitly; Stage 21 cannot finish with that item still open.

An audit item becomes `VERIFIED` only when its embedded acceptance test and all relevant production-route checks have passed on current code. `NOT_APPLICABLE` requires concrete evidence, such as a genuinely removed unsupported route or a historical claim disproved by current behavior; it is not a substitute for an unavailable test environment. Already-fixed items still need current verification.

If runtime access, dependency installation, credentials, platform support, or authorization blocks a mandatory gate, mark it `BLOCKED`, report the exact unverified behavior, and do not claim production completion. Continue only clearly independent safe work; record that it was performed out of sequence. Do not treat a web mockup as a replacement for native-runtime evidence. Routine uncertainty should be resolved by repository inspection and reversible decisions, not repeated requests for design approval.

### 4.4 Delegation and handoff

Delegate bounded tasks by files and audit IDs: investigation, a disjoint screen, targeted tests, or independent review. Give each subagent the design contract, invariants, allowed files, prerequisites, expected tests, and required evidence. Require it to return changed files, actual commands/results, and unresolved risks. Do not let multiple agents edit the same shared component concurrently. The integrating agent must rerun combined checks; subagent claims are not acceptance evidence by themselves.

At the end of each working session, leave this information in `00-status.md`:

```text
Current revision and task-owned worktree changes:
Current stage and status:
Last verified stage:
Audit items implemented but not fully verified:
Files changed in this session:
Commands actually run and results:
Production runtime scenarios and evidence locations:
Outstanding failures/blockers (including exact reproduction):
Next concrete action and why it is next:
Any prerequisite or evidence invalidated by subsequent changes:
```

Never report work as completed merely because a time/context budget ended. On resume, start at the first unmet prerequisite or acceptance gate rather than repeating finished cosmetic work.

<a id="roadmap"></a>
## 5. Stage roadmap

**Primary ownership** identifies where an item is delivered, not the only place it is tested. Shared controls require downstream route adoption and final integrated verification. Stage 00 reconciles all items; Stage 21 verifies final closure. All audit priorities are preserved, but dependencies determine implementation order. Any newly reproduced crash, wrong-target mutation, save risk, or input-isolation fault takes precedence over cosmetic work and must be recorded and contained immediately.

| Stage | Deliverable | Primary audit IDs |
|---|---|---|
| [00](#stage-00) | Reconcile the audit and establish a production baseline | All 98: baseline reconciliation |
| [01](#stage-01) | Repair verification and create a real component-state test bed | SYS-32 |
| [02](#stage-02) | Consolidate the production theme and cascade | SYS-01–SYS-02 |
| [03](#stage-03) | Standardize window chrome, typography, icons, and grouping | SYS-03–SYS-06, SYS-13 |
| [04](#stage-04) | Unify buttons, focus, default actions, and connected tabs | SYS-07–SYS-12 |
| [05](#stage-05) | Unify form fields, choices, combos, and numeric editors | SYS-14–SYS-19 |
| [06](#stage-06) | Unify reports, filters, selection, scrolling, and matrices | SYS-20–SYS-24 |
| [07](#stage-07) | Standardize dialogs, feedback, edit lifecycles, and command scope | SYS-25–SYS-29, APP-09 |
| [08](#stage-08) | Migrate Inventory & Resources as the reference report screen | INV-01–INV-04 |
| [09](#stage-09) | Migrate stockpile management and explicit rule scopes | STO-01–STO-07 |
| [10](#stage-10) | Migrate workshops, production queues, and direct trade editing | WRK-01–WRK-07 |
| [11](#stage-11) | Migrate farms, groves, and pastures with explicit targets | AGR-01–AGR-08 |
| [12](#stage-12) | Migrate the population roster, skills, and profession editor | POP-01–POP-03 |
| [13](#stage-13) | Repair the 24-hour schedule matrix and bulk scope preview | POP-04 |
| [14](#stage-14) | Reorganize military around squad, citizen, role, and destination | MIL-01–MIL-07 |
| [15](#stage-15) | Migrate diplomacy and mission planning | DIP-01–DIP-04 |
| [16](#stage-16) | Consolidate inspectors, citizen detail, and equipment scope | INS-01–INS-08, POP-05 |
| [17](#stage-17) | Clarify the HUD, armed tools, build navigation, and onboarding | HUD-01–HUD-07 |
| [18](#stage-18) | Align the main menu, new-game setup, and save browser | APP-01–APP-04 |
| [19](#stage-19) | Align immediate settings and safe pause/save/load transitions | APP-05–APP-08 |
| [20](#stage-20) | Verify scaling, keyboard coverage, accessibility variants, and performance | SYS-30–SYS-31 |
| [21](#stage-21) | Remove obsolete styles and complete the evidence-backed handoff | All 98 + discoveries: final closure |

<a id="stages"></a>
## 6. Detailed stage instructions

For each stage, complete its implementation checklist, verification cases, and linked embedded audit acceptance tests within that stage's declared scope. The stage gate is in addition to—not a replacement for—the global execution rules. Source paths are audit-derived starting points; Stage 00 must resolve renamed paths and dynamic/controller counterparts.

<a id="stage-00"></a>
### Stage 00 — Reconcile the audit and establish a production baseline

**Dependencies:** None. This is the mandatory starting point.

**Audit coverage:** All 98 original items and any current-production discoveries.

**Objective:** Know what actually exists and works before changing presentation or behavior.

**Inspect/change as required:**

- Repository instructions, build files, dependency manifests, and current UI entry points.
- `content/rmlui/`
- `tests/ui-design-system/verify-design-system.cmake`
- Controllers, presenters, EventConnector handlers, and native/detached hosts discovered from current code.

**Implementation checklist**

- [ ] Record the current branch/HEAD/worktree and compare the checkout with the historical audit without resetting it. Identify any earlier modernization work already present.
- [ ] Read project-local instructions and discover actual configure, build, test, launch, and screenshot commands for this environment. Record the pinned RmlUi version, renderer, fonts, token generator, operating system, and supported host modes.
- [ ] Inventory every player-facing route, page, conditional panel, popup, modal, generated list, and inspector entry point. Map all 98 audit IDs to current paths and controllers. Add any uncovered production surface as a NEW item with acceptance criteria.
- [ ] Trace representative actions end to end: a harmless navigation action, a staged edit, an immediate toggle, a scope-sensitive bulk edit, a destructive confirmation, and save/load. Build the initial command-parity matrix.
- [ ] Launch the real application and create protected disposable test worlds/saves. Capture representative populated and empty screens, selected/focused/disabled states, one open popup, one modal, and each supported host type. Clearly label fixture versus live-world evidence.
- [ ] Record existing build/test/runtime failures separately from design findings. Inventory current runtime capture/debug hooks; do not assume a browser driver can operate native RmlUi.

**Required verification**

- [ ] Reproduce the recorded build and launch sequence from the documented working directory.
- [ ] Open every reachable production route at least once and record routes blocked by game prerequisites. Build or select a legal scenario for each blocked route rather than calling it absent.
- [ ] Check the six source risks named in the audit against the actual checkout; mark each confirmed-in-source, reproduced-at-runtime, already-fixed, or not-yet-verified.
- [ ] Verify a test-save copy can be loaded without changing the source save.

**Exit gate:** Baseline documents, route inventory, command map, and all 98 reconciled audit records exist. The production build/launch is reproducible, or the exact environmental blocker is recorded and the stage remains BLOCKED. Historical test failures are captured honestly; unrelated pre-existing failures need not be silently repaired here.

**Checkpoint:** Update `stages/00-baseline.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-01"></a>
### Stage 01 — Repair verification and create a real component-state test bed

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [SYS-32](#audit-sys-32).

**Objective:** Make the checks trustworthy before relying on them to judge a new design.

**Inspect/change as required:**

- `tests/ui-design-system/verify-design-system.cmake`
- Existing UI fixtures, render tests, and test registration discovered in Stage 00.
- `content/rmlui/styles/base.rcss`

**Implementation checklist**

- [ ] Reproduce and resolve the historical verifier contradiction involving @media only if it still exists. Align allowed syntax with the pinned engine and current supported production features, rather than blindly removing responsive rules.
- [ ] Replace comment-satisfiable palette string checks with active token/structure or renderer-backed checks. Preserve genuine asset, license, font, and integrity assertions.
- [ ] Create or extend a component-state fixture that renders through the same RmlUi engine and shared styles as production. Include actual generated select and scrollbar parts, tabs, inputs, checkboxes, numeric controls, reports, tooltips, and dialogs.
- [ ] Add repeatable state cases and a small set of sentinel production routes: inventory, stockpile, a scope-sensitive manager, an inspector, settings, and a modal. Reuse a smaller legal baseline set until later stages make all scenarios available.
- [ ] Record command-event and authoritative-snapshot checks for double dispatch and target identity where the existing test architecture supports them. Keep test-only injection out of release behavior.

**Required verification**

- [ ] Run the verifier against the intended current baseline and show how known unrelated failures are distinguished.
- [ ] Introduce a temporary wrong active token and an invalid/missing required component state in a test copy; verify checks fail, then restore the test copy. A comment containing the expected value must not make the check pass.
- [ ] Capture actual scrollbar/select generated elements and demonstrate that the fixture is not drawing hand-made substitutes.
- [ ] Run the component fixture and sentinel smoke checks twice to identify unstable evidence before adopting screenshot comparisons.

**Exit gate:** The verifier checks the current intended contract without a known self-contradiction. Negative controls fail for the right reasons, and the real-renderer fixture plus sentinel routes are repeatable. A fixture does not close any gameplay workflow item.

**Checkpoint:** Update `stages/01-verifier.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-02"></a>
### Stage 02 — Consolidate the production theme and cascade

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [SYS-01](#audit-sys-01), [SYS-02](#audit-sys-02).

**Objective:** Establish one authoritative theme without adding another final override layer.

**Inspect/change as required:**

- `content/rmlui/styles/tokens.json`
- `content/rmlui/styles/base.rcss`
- `content/rmlui/styles/components.rcss`
- `content/rmlui/styles/accessibility.rcss`
- `content/rmlui/templates/management_window.rml`
- Route styles identified in the baseline.

**Implementation checklist**

- [ ] Trace actual stylesheet load order, dynamic inline values, generator inputs, and per-route overrides. Write a cascade map and distinguish layout rules from visual skin.
- [ ] Define semantic normal-theme tokens for surfaces, borders, text, selection, focus, disabled, and feedback states. Implement them through the existing generation/loading approach supported by RmlUi.
- [ ] Replace superseded production cave/classic-park/route-specific values incrementally. Preserve an old theme only when it is genuinely selectable and correctly scoped; do not keep unused historical declarations as fallback clutter.
- [ ] Make accessibility overrides deliberately ordered and sufficiently scoped. Do not solve specificity collisions by repeatedly increasing selector weight or copying colors into more routes.
- [ ] Add reproducibility checks for generated output and update theme documentation. Avoid global layout changes in this stage except those required to separate structure from skin.

**Required verification**

- [ ] Change a token in a test copy, regenerate, and verify its intended consumers change in the component fixture and sentinel production routes.
- [ ] Check computed properties or renderer-observed equivalents for semantically identical controls on different routes.
- [ ] Confirm repeated generation has no unexplained diff and old theme comments cannot satisfy active-theme checks.
- [ ] Smoke-test all reachable routes to detect unreadable text or inherited-state regressions from cascade changes.

**Exit gate:** The documented production token source and load order account for shared styles. Remaining route-specific migration work is explicit; no new undocumented theme layer or accessibility precedence regression is introduced.

**Checkpoint:** Update `stages/02-theme.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-03"></a>
### Stage 03 — Standardize window chrome, typography, icons, and grouping

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [SYS-03](#audit-sys-03), [SYS-04](#audit-sys-04), [SYS-05](#audit-sys-05), [SYS-06](#audit-sys-06), [SYS-13](#audit-sys-13).

**Objective:** Make every window, group, field, and heading belong to the same visual hierarchy.

**Inspect/change as required:**

- `content/rmlui/styles/components.rcss`
- `content/rmlui/styles/base.rcss`
- `content/rmlui/styles/management_window.rcss`
- `content/rmlui/templates/management_window.rml`
- `content/rmlui/windows/management6b.rcss`
- `content/rmlui/screens/shell.rcss`
- Host-window implementations mapped in Stage 00.

**Implementation checklist**

- [ ] Implement compact square frames and title bars, with a consistent Close hit area and accessible name. Specify active/inactive appearance and full-name recovery for truncated titles.
- [ ] Resolve native versus custom chrome ownership per host. Include resize grips only where resize actually works; do not add decorative maximize/minimize buttons.
- [ ] Define and apply a restrained text hierarchy using the current licensed font first. Raise essential tiny labels and align baselines; avoid arbitrary per-route font reductions to make content fit.
- [ ] Create/reuse a small licensed/original chrome glyph set for close, dropdown, spinner, sort, expand/collapse, and warnings. Keep item sprites separate and avoid inconsistent literal v/^ substitutes.
- [ ] Replace unnecessary nested raised sections with group boxes and clear content regions. Keep game branding and specialized controls intact.
- [ ] Migrate shared frame consumers and the sentinel windows first. Record any remaining local headings/groups for the owning route stages.

**Required verification**

- [ ] Drag, resize where supported, close, reopen, and switch active hosts; verify no duplicate title bar or unreachable Close.
- [ ] Render long titles, empty titles with a legal fallback, numeric labels, and icons at 100/125/150/200% UI scale in the component fixture.
- [ ] Inspect before/after captures for text clipping, glyph alignment, excessive framing, and inconsistent bevel direction.
- [ ] Confirm chrome and font changes preserve existing asset/license checks.

**Exit gate:** Shared chrome, typography, and grouping are usable in actual supported hosts. Every remaining screen-specific adoption task has an owner; foundational changes introduce no inaccessible window controls.

**Checkpoint:** Update `stages/03-chrome.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-04"></a>
### Stage 04 — Unify buttons, focus, default actions, and connected tabs

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [SYS-07](#audit-sys-07), [SYS-08](#audit-sys-08), [SYS-09](#audit-sys-09), [SYS-10](#audit-sys-10), [SYS-11](#audit-sys-11), [SYS-12](#audit-sys-12).

**Objective:** Separate commands, page selection, focus, and persistent state visually and behaviorally.

**Inspect/change as required:**

- `content/rmlui/styles/base.rcss`
- `content/rmlui/styles/components.rcss`
- `content/rmlui/styles/management_window.rcss`
- Shared tab/input event helpers discovered in Stage 00.
- Representative production tab consumers.

**Implementation checklist**

- [ ] Implement neutral normal/hover buttons, pressed bevel/content offset, stable dimensions, readable disabled state, an independent focus cue, and a separate default outline.
- [ ] Keep destructive commands precise and proportionate. Do not turn ordinary hover into a warning color or turn every harmless operation into a confirmation.
- [ ] Reuse the existing connected-tab primitive after inspecting it. Implement selected-page attachment, tab/panel associations, roving focus, keyboard activation, hidden-page focus exclusion, and preserved per-page state.
- [ ] Keep hierarchical build/catalog navigation as a category control. Define small peer sets for migration in later stages: military, population, diplomacy, stockpile, and workshop.
- [ ] Integrate the shared primitives with real production pilot consumers and update the fixture. Do not declare the global tab audit closed until downstream managers are migrated.
- [ ] Document shortcut conflicts and host-specific behavior; scope accelerators to the active UI context so editing or map controls do not receive them accidentally.

**Required verification**

- [ ] Demonstrate active tab, focused control, and default action simultaneously in one production window.
- [ ] Test mouse and keyboard activation, key repeat, held Space/Enter, focus-only movement, disabled controls, and dynamically hidden tabs.
- [ ] Verify selected-tab geometry has no page-facing seam, with no content jump on focus/hover/press.
- [ ] Check Ctrl+Tab handling and Escape ownership without sending navigation events to the world.

**Exit gate:** The shared interaction/state contract works in the real renderer and pilot production routes. The item tracker explicitly lists managers still awaiting tab migration.

**Checkpoint:** Update `stages/04-buttons-tabs.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-05"></a>
### Stage 05 — Unify form fields, choices, combos, and numeric editors

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [SYS-14](#audit-sys-14), [SYS-15](#audit-sys-15), [SYS-16](#audit-sys-16), [SYS-17](#audit-sys-17), [SYS-18](#audit-sys-18), [SYS-19](#audit-sys-19).

**Objective:** Make field purpose, legal values, persistent state, and exact editing predictable.

**Inspect/change as required:**

- `content/rmlui/styles/base.rcss`
- `content/rmlui/styles/components.rcss`
- `content/rmlui/windows/management6c.rcss`
- Existing combo/filter and numeric-control implementations discovered in Stage 00.

**Implementation checklist**

- [ ] Provide shared persistent labels, editable/read-only/disabled styles, and inline validation. A placeholder is not the only field label.
- [ ] Unify checkbox appearance and input dispatch, including clickable labels. Add exclusive-choice controls with clear selected state; mixed state is allowed only for supported aggregate data.
- [ ] Consolidate editable and read-only combo variants without removing custom filtering. Use real legal catalogs and preserve popup selection/cancellation behavior.
- [ ] Build or reuse a numeric editor with integrated arrows, bounds, visible units, exact typing, and priority direction. Define temporary invalid input separately from a committed value.
- [ ] Add synchronized slider-plus-numeric composition for exact parameters. Preserve approximate sliders for values such as volume where appropriate.
- [ ] Pilot the controls in stockpile settings and another real form before broad route migration. Keep event/action identifiers compatible and avoid emitting duplicate changes from label plus input handlers.

**Required verification**

- [ ] Exercise typing, paste, whitespace, empty input, invalid characters, min/max, stepping, Enter, Escape, and controller rejection using real catalog/bound values.
- [ ] Test checked/unchecked/disabled and backed mixed state without color. Confirm one label click emits one logical update.
- [ ] Open, navigate, choose, cancel, click away, resize, and close the parent while a combo is open; verify host bounds and restored focus.
- [ ] Verify slider and numeric entry converge to the same authoritative value without feedback loops or unintended commits.

**Exit gate:** Every shared form-control variant has tested validation, state, and event semantics plus real production usage. Unsupported or unverified capabilities are not silently simulated.

**Checkpoint:** Update `stages/05-form-controls.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-06"></a>
### Stage 06 — Unify reports, filters, selection, scrolling, and matrices

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [SYS-20](#audit-sys-20), [SYS-21](#audit-sys-21), [SYS-22](#audit-sys-22), [SYS-23](#audit-sys-23), [SYS-24](#audit-sys-24).

**Objective:** Create consistent large-data controls while preserving performance and stable targets.

**Inspect/change as required:**

- `content/rmlui/styles/components.rcss`
- `content/rmlui/windows/management6b.rcss`
- `content/rmlui/windows/management6c.rcss`
- `content/rmlui/windows/inventory_browser.rml`
- `content/rmlui/windows/stockpile_manager.rml`
- Generated-row/list implementations mapped in Stage 00.

**Implementation checklist**

- [ ] Extract or share the existing six-column report header/filter implementation at an appropriate level for this repository. Keep route-specific field semantics separate from visual/control reuse.
- [ ] Implement aligned sortable headers, numeric alignment, visible active filters, a clear reset path, match count where backed, truncation help, and distinct focused/selected/inactive selection.
- [ ] Reconcile scrollbar selectors with the pinned generated elements. Cover both axes, thumb, track, arrows, corner, and disabled/no-range behavior; remove obsolete selectors only after checking actual consumers.
- [ ] Preserve virtualization, stable ID selection, scroll anchoring, and header/body synchronization. Define selection behavior when filtering or deletion removes the selected record.
- [ ] Provide reusable matrix focus and row/column identity behavior for schedules/plots. Preserve real tree expansion behavior where trees exist; keep inventory flat.
- [ ] Establish a narrow-host policy: synchronized horizontal scrolling, a justified minimum size, or an intentional column adaptation that keeps every essential field reachable.

**Required verification**

- [ ] Sort/filter/reset/page/refresh/delete/reorder large lists while holding stable record IDs; verify displayed actions keep targeting the intended object.
- [ ] Test horizontal and vertical overflow, tiny scroll ranges, wheel, drag, track paging, arrow controls, and keyboard scrolling in nested panels.
- [ ] Measure element counts and update/render timing against the baseline on representative large data; diagnose significant regressions rather than choosing an arbitrary passing threshold.
- [ ] Check tree expand versus activate and matrix focus movement without accidental value mutation.

**Exit gate:** Shared reports and scrolling work on actual generated controls, preserve data/selection semantics, and have no demonstrated large-list regression. Inventory and stockpile final workflow acceptance remains in their route stages.

**Checkpoint:** Update `stages/06-reports-scrolling.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-07"></a>
### Stage 07 — Standardize dialogs, feedback, edit lifecycles, and command scope

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [SYS-25](#audit-sys-25), [SYS-26](#audit-sys-26), [SYS-27](#audit-sys-27), [SYS-28](#audit-sys-28), [SYS-29](#audit-sys-29), [APP-09](#audit-app-09).

**Objective:** Make consequential actions explicit and ensure visual changes cannot hide incorrect command behavior.

**Inspect/change as required:**

- `content/rmlui/modals/confirm_destructive.rml`
- `content/rmlui/styles/components.rcss`
- Shared dialog, popup, command-feedback, and edit-state implementations discovered in Stage 00.
- Production callers in military, population, stockpile, trade, and shell.

**Implementation checklist**

- [ ] Implement a common modal shell with object-specific title/body/action labels, deliberate safe default, consistent action order, focus containment/restoration, and background input isolation.
- [ ] Require destructive callers to provide meaningful copy; a generic Confirm fallback must not be the only explanation. Avoid turning every routine action into a modal.
- [ ] Standardize pending/success/rejection and empty/unknown/zero/unavailable/loading/error presentation. Use truthful progress or a named indeterminate stage; cancellation is exposed only where safe.
- [ ] Implement shared dirty-draft and immediate-setting patterns using the existing architecture. Preserve drafts on page changes; define target-switch, close, revert, rejection, and external-update reconciliation.
- [ ] Provide consistent scope summaries and revalidation for bulk or irreversible commands. Use stable targets and existing command/version checks; do not implement UI-only fake undo.
- [ ] Unify tooltip presentation and keyboard-focus help, while keeping scope and consequences visible without hover. Pilot shared behavior in real dialogs and edits.

**Required verification**

- [ ] Test opening-event key release, held Enter/Escape, repeated click, nested event arrival, target removal, failed commands, canceled commands, and a parent window closing.
- [ ] Prove focus cannot leave the modal and that closing it neither leaves a blocker nor delivers the same event to the map.
- [ ] Test dirty versus immediate changes through Apply, Discard draft, Keep editing, tab switch, object switch, Close, reopen, and rejected Apply.
- [ ] Verify a materially changed review target/scope is not silently committed, and unknown data is not displayed as zero.

**Exit gate:** Shared modal/edit/feedback behavior is verified on production pilot routes and command-layer tests. Every remaining caller migration is tracked. No fake progress, fake Cancel, generic destructive explanation, or optimistic false success is introduced.

**Checkpoint:** Update `stages/07-dialogs-editing.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-08"></a>
### Stage 08 — Migrate Inventory & Resources as the reference report screen

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [INV-01](#audit-inv-01), [INV-02](#audit-inv-02), [INV-03](#audit-inv-03), [INV-04](#audit-inv-04).

**Objective:** Finish one strong production report end to end before duplicating the pattern elsewhere.

**Inspect/change as required:**

- `content/rmlui/windows/inventory_browser.rml`
- `content/rmlui/windows/management6b.rcss`
- Inventory presenters/controllers and detail navigation mapped in Stage 00.

**Implementation checklist**

- [ ] Adopt the common frame, title, six-column header/filter controls, selection, scrollbar, typography, and feedback styles. Keep category/type/item/material/stock/total semantics as currently backed.
- [ ] Make numeric read-only filters look and behave like choices rather than editable text. Expose active filters, clear-all, and a backed match count without duplicating an existing controller-projected equivalent.
- [ ] Implement coherent compact density with smaller inline sprites, preserving a useful larger presentation when justified. Do not shrink header text to compensate for oversized rows.
- [ ] Reproduce the historical minimum-width/horizontal-hiding risk at supported host sizes and scales. Fix the actual cause via a documented minimum or synchronized scrolling; do not remove columns.
- [ ] Keep item identity visible in details and retain recipes, outputs, stockpile locations, and history as compact grouped content. Save and restore filters, sort, selected ID, scroll anchor, and focus across detail/Back transitions.
- [ ] Remove route-local duplicates only after the shared implementation matches functionality and actual rendered behavior.

**Required verification**

- [ ] Exercise all six sort/filter controls with empty, single-item, large, long-name, and filtered-empty datasets.
- [ ] Open a deeply scrolled filtered item, follow supported location/detail navigation, return, and verify exact context restoration.
- [ ] Refresh or remove the selected item while viewing details; prevent stale actions and explain missing targets.
- [ ] Verify all columns and commands remain reachable at minimum supported dimensions and 100/125/150/200% scale; inspect real captures and large-list timing.

**Exit gate:** INV-01 through INV-04 meet their embedded acceptance tests in production. Inventory becomes the documented report reference, not just a component-fixture screenshot.

**Checkpoint:** Update `stages/08-inventory.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-09"></a>
### Stage 09 — Migrate stockpile management and explicit rule scopes

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [STO-01](#audit-sto-01), [STO-02](#audit-sto-02), [STO-03](#audit-sto-03), [STO-04](#audit-sto-04), [STO-05](#audit-sto-05), [STO-06](#audit-sto-06), [STO-07](#audit-sto-07).

**Objective:** Clearly separate physical stock, acceptance rules, staged settings, and immediate state changes.

**Inspect/change as required:**

- `content/rmlui/windows/stockpile_manager.rml`
- `content/rmlui/screens/inspector.rml (stockpile summary)`
- Stockpile presenters, rule/template commands, and snapshots mapped in Stage 00.

**Implementation checklist**

- [ ] Replace peer-view rail buttons with connected Stock / Allow list / Settings tabs. Move Center on map into an object toolbar; keep name and Active/Suspended state visible.
- [ ] Reuse the verified inventory report and filter pattern. Inspect the real meaning of Total and explain it accurately; do not assume global versus local scope from the label alone.
- [ ] Show the exact matching-rule count and scope beside Allow matches / Block matches, including offscreen matches when that is the intended operation. Use proportional confirmation or supported recovery and revalidate before commit.
- [ ] Separate Save new template from Update existing; retain a named overwrite review. Handle duplicate names, empty names, canceled updates, and unsaved template text.
- [ ] Keep name/priority staged with Apply and a visible 1-is-highest explanation. Add the agreed dirty-draft behavior; retain true labeled hauling checkboxes and state whether they apply immediately.
- [ ] Synchronize suspend/resume, priority, names, and relevant counts between all open inspectors/managers using authoritative snapshots.

**Required verification**

- [ ] Filter to a subset of rules, review affected count, apply allow/block, and compare changed stable rule IDs against the promised scope.
- [ ] Test new/overwrite/canceled/duplicate-name template cases and verify both saved rules and current unsaved rules.
- [ ] Exercise dirty name/priority through tab switches, object switches, close, rejection, and reopen; immediate hauling changes must not be falsely discarded.
- [ ] Toggle suspension in each entry point while the other is open, including stale targets and rejected/pending commands; inspect tabs and popups at supported sizes.

**Exit gate:** All seven stockpile findings are verified, with no physical-stock/allow-list confusion, silent draft loss, template overwrite surprise, or inspector/manager disagreement.

**Checkpoint:** Update `stages/09-stockpiles.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-10"></a>
### Stage 10 — Migrate workshops, production queues, and direct trade editing

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [WRK-01](#audit-wrk-01), [WRK-02](#audit-wrk-02), [WRK-03](#audit-wrk-03), [WRK-04](#audit-wrk-04), [WRK-05](#audit-wrk-05), [WRK-06](#audit-wrk-06), [WRK-07](#audit-wrk-07).

**Objective:** Preserve the good craft-order flow while repairing ambiguous settings and trade targeting.

**Inspect/change as required:**

- `content/rmlui/windows/workshop_manager.rml`
- `content/rmlui/screens/inspector.rml (workshop summary)`
- Workshop queue, link, special-option, and trade command paths mapped in Stage 00.

**Implementation checklist**

- [ ] Use Craft / Queue / Settings / Trade tabs, with Trade present only when actually supported. Keep object identity and Center on map separate from navigation.
- [ ] Keep craft search/list → selected product requirements → quantity → Add order. Display missing prerequisites beside the action and use the shared exact quantity editor.
- [ ] Preserve queue execution semantics, selected-job editing, and only the supported reorder/repeat/suspend/remove operations. Stabilize selection and drafts when orders move, complete, or disappear.
- [ ] Adopt explicit staged name/priority versus immediate-option grouping. Convert persistent special options to consistent checkboxes without exposing options on unsupported workshop types.
- [ ] Present linked stockpiles as a directly selectable compact list; retain collection precedence explanation and supported link/unlink/locate behavior.
- [ ] Replace Next trade row as the primary selection path with direct stable-row targeting and exact offered quantities. Preserve old commands as accelerators where useful. Review both sides, backed totals/net value, and irreversibility before one deliberate commit. Do not fabricate prices or values absent from backing data.

**Required verification**

- [ ] Confirm one activation creates exactly one craft order; invalid prerequisites and duplicate/held inputs are handled safely.
- [ ] Reorder and edit queues during live completion/removal, ensuring a stale row index cannot edit a different job.
- [ ] Visit representative workshop types; confirm only legal special settings and links appear and persist as declared.
- [ ] Build a trade with several rows, revise quantities, cancel with no exchange, then commit once. Exercise stale merchant offers, target disappearance, rejection, and authoritative total refresh.

**Exit gate:** All seven workshop/trade findings pass production scenarios. Direct targeting is backed by real commands, queue semantics are preserved, and trade review matches the committed exchange.

**Checkpoint:** Update `stages/10-workshops-trade.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-11"></a>
### Stage 11 — Migrate farms, groves, and pastures with explicit targets

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [AGR-01](#audit-agr-01), [AGR-02](#audit-agr-02), [AGR-03](#audit-agr-03), [AGR-04](#audit-agr-04), [AGR-05](#audit-agr-05), [AGR-06](#audit-agr-06), [AGR-07](#audit-agr-07), [AGR-08](#audit-agr-08).

**Objective:** Make every agriculture operation disclose whether it changes selected plots, defaults, a queue, an animal, or a standing rule.

**Windows 98 design (owner rule, 2026-09-25):** Only UI documented in `docs/MS-Windows-User-Experience-2001.pdf` and MSDN Visual Design (ms997612) may be used; gaps are researched (Excel 97 for grids). Detailed citations: [07-win98-design-reference.md](07-win98-design-reference.md). For this stage:

- The agriculture manager becomes a fixed **property sheet** exactly like the Stage 10 workshop reference: "<name> Properties" caption, 252 x 218 DLU (384 x 380 px at 1x, whole-number scale), one row of book-title-caps tabs, pages that never scroll, OK / Cancel / Apply outside the pages, Close with pending changes asks Yes / No / Cancel (PDF p.163-166).
- **Settings are pending until Apply/OK:** name, suspension, work rules (harvest, hay, tame, pick, plant, fell), the default crop / tree / animal type, male and female limits, per-animal butchering marks and per-food rules. Apply sends only changed fields, each to its own stable target.
- **Commands act immediately** and name their target (like Add Order on the workshop Craft page): Assign Crop, Use Farm Default, Queue Plantings, Queue Repeat, and the plot-queue Move Up / Move Down / Remove.
- **Pages by designation type:** General (all), Plots and Plot Queue (farm), Crops/Trees (farm default crop or grove tree, list view details), Animals and Food (pasture). Hidden pages are rejected by the controller.
- **Plot grid:** a list view in icon view drawn in a field border (PDF p.136); it is the only scrolling region on its page. Selection follows the list view and Excel 97 model: click selects one plot, Ctrl+click toggles, Shift+click selects the rectangle from the anchor, Ctrl+A / Select All selects every plot; arrow keys move the focus rectangle and Space toggles. Selected plots use the navy selection highlight with a dotted focus rectangle (PDF p.49-60). State is shown by a glyph as well as the crop icon (untilled, tilled, planted, ready), with a text legend, so color is never the only cue (PDF p.313).
- **Lists:** crops, animals and food rules use list view details view with column headers (PDF p.136, p.143); animal butchering marks and food rules are flat check boxes in the list (Controls: multiple-selection list box, flat appearance). Male and female limits are labelled spin boxes (PDF p.141).
- **Statistics** are static text in a group box, never cards. Tilled, planted and ready are shown as "of N plots", not as additive totals.
- **Priority:** `FarmingManager::setFarmPriority`, `setGrovePriority` and `setPasturePriority` are empty stubs and the getters return -1, so no Priority control is offered for agriculture (unsupported capability stays unavailable). Record as a discovery.
- Center on Map sits in the General page Status group as on the workshop sheet.

**Inspect/change as required:**

- `content/rmlui/panels/agriculture_manager.rml`
- `content/rmlui/screens/inspector.rml (agriculture summary)`
- Plot, planting-queue, grove, animal, cap, and food-rule commands mapped in Stage 00.

**Implementation checklist**

- [ ] Use a compact backed overview of plots/tilled/planted/ready states without implying those categories are additive when they are not.
- [ ] Give the plot matrix non-color state cues, stable plot identity, separate focus/selection, and keyboard single/multiple/all selection. Preserve the spatial relationship to the actual farm.
- [ ] Keep selected crop and legal availability visible. Separate Selected plots from Farm default; label assignment, clearing overrides, and changing the fallback as distinct actions.
- [ ] Show selected-plot count and plantings per plot before queue changes. Preserve repeat movement, later orders, and fallback semantics; expose supported queue edits directly.
- [ ] Render harvest/pick/plant/fell options as persistent Boolean controls. Preserve immediate/staged semantics and the implications of long-lived work rules.
- [ ] Use direct animal and food-rule selection, labeled male/female cap spinners, and a visible named target before any mutation. Inspect whether target-specific commands already exist; add a minimal tested adapter only if required, never emulate selection by racing Next animal calls.
- [ ] Adopt the shared object toolbar, name/priority draft handling, suspension state, and consistent Center on map wording.

**Required verification**

- [ ] Compare changed plot IDs for single, multiple, all, and offscreen selections; verify selection/focus and state remain understandable without color.
- [ ] Test assign, use default, set default, queue multiple plantings, repeat, and supported reorder/remove operations against actual resulting plot/queue state.
- [ ] Edit a non-first animal and non-first food rule, then refresh/reorder/delete nearby records; confirm commands retain the reviewed target.
- [ ] Validate cap bounds and legal combinations, switch farm/grove/pasture types, and exercise dirty settings, suspension, and missing targets.

**Exit gate:** All eight agriculture findings pass real designation scenarios. Scope is visible and equals authoritative mutations; no per-plot count is presented as a global total and no arbitrary first record remains the only primary target.

**Checkpoint:** Update `stages/11-agriculture.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-12"></a>
### Stage 12 — Migrate the population roster, skills, and profession editor

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [POP-01](#audit-pop-01), [POP-02](#audit-pop-02), [POP-03](#audit-pop-03).

**Objective:** Make population management compact and predictable without losing the dual-list profession workflow.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#32-stage-12-population-roster-skills-and-professions).

- The Population manager becomes a fixed property sheet like the Stage 10 workshop: Citizens / Skills / Professions / Schedules tabs in one row, pages that never scroll, commit buttons outside the pages.
- The roster is a list view in details view. Clicking a column heading sorts by it and a second click reverses; the heading shows a down arrow only for descending order (PDF p.143). Sort Ascending / Sort Descending are also reachable from the keyboard. Numbers are right-aligned with right-aligned headings.
- Paging buttons are replaced by the scrolling list view with virtualised rows; the list is the only scrolling region.
- Skills use a list view with check box state images. A bulk skill change names the skill and the number of citizens affected in static text and asks a Yes/No message box that states the effect (no "Are you sure"; PDF p.182-187).
- Profession editor: the Customize Toolbar two-list pattern (available list, Add -> and <- Remove between the lists, Move Up / Move Down to the right of the current list). Buttons are unavailable when they cannot act, and focus returns to the list after a move (PDF p.132, p.161, p.323). Delete Profession is separated from the list commands and confirms with a message box.

**Inspect/change as required:**

- `content/rmlui/windows/population_manager.rml`
- `content/rmlui/windows/management6b.rcss`
- Citizen roster, bulk-skill, and profession command paths mapped in Stage 00.

**Implementation checklist**

- [ ] Adopt connected Citizens / Skills / Professions / Schedules tabs, shared frame and reports. Preserve supported route names and controller identifiers.
- [ ] Verify whether both historical Refresh controls are concurrently visible; consolidate actual duplication while retaining a consistent keyboard-accessible refresh path.
- [ ] Use sortable roster headers and one paging/status area. Preserve selected citizen ID and context through filter, refresh, paging, and detail navigation.
- [ ] For all-citizen skill changes, show selected skill, affected population scope/count, and proportionate confirmation or supported recovery. Keep per-citizen checkbox state synchronized with authoritative results.
- [ ] Retain Profession skills / Available skills dual lists. Place Add/Remove and priority controls beside their targets, isolate Delete from Save/reordering, and implement dirty/revert/target-switch behavior.
- [ ] Define the shared citizen-detail interface for Stage 16. Do not duplicate a new equipment/profession editor here; keep existing supported entry points functional until consolidated.

**Required verification**

- [ ] Exercise duplicate/long names, changing profession, filtered-empty results, paging boundaries, disappearance of the selected citizen, and refresh during navigation.
- [ ] Apply one bulk skill change and verify only that skill changes for exactly the disclosed citizens, including offscreen records and rejection.
- [ ] Add/remove/reorder profession skills, switch professions with unsaved changes, cancel deletion, save, and reopen.
- [ ] Verify the new tabs maintain hidden-page focus exclusion and do not reset the current schedule page before Stage 13.

**Exit gate:** POP-01 through POP-03 pass their production acceptance tests. The tab shell and roster state are stable; schedule implementation and citizen-detail consolidation remain explicitly owned by Stages 13 and 16.

**Checkpoint:** Update `stages/12-population-professions.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-13"></a>
### Stage 13 — Repair the 24-hour schedule matrix and bulk scope preview

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [POP-04](#audit-pop-04).

**Objective:** Make schedule editing safe and understandable at cell, citizen-day, and hour-for-all scopes.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#33-stage-13-schedule-24-hour-matrix).

- The schedule is an Excel 97 style grid (the book has no grid control): button-face row headings (citizens) and column headings (hours 0-23) that stay frozen while the cells scroll, a Select All corner button, one active cell with a heavy black border, and navy range selection.
- Selection follows the book's spreadsheet model (PDF p.53-60): click one cell, Shift+click or Shift+arrow extends from the anchor, clicking a row or column heading selects the citizen's day or the hour for everyone, Ctrl+A or the corner selects all.
- Each cell shows a letter code for its activity as well as its tint, so color is never the only cue (PDF p.314); the legend is the labelled option buttons.
- The activity palette is a group of option buttons (at most 7) with the legend text. Applying is an explicit Set command button that names the scope in static text ("Set 12 hours for 3 citizens to Work"), so focus changes never mutate cells.

**Inspect/change as required:**

- `content/rmlui/windows/population_manager.rml (Schedules)`
- `content/rmlui/windows/management6b.rcss`
- Schedule projection and cell/row/column command paths mapped in Stage 00.

**Implementation checklist**

- [ ] Retain the explicit Set cell / Set citizen's day / Set hour for all operations. Do not collapse them into an ambiguous paint action.
- [ ] Use a clearly exclusive activity palette with non-color activity symbols and a visible legend. Programmatic focus changes must not mutate activities.
- [ ] Implement cell focus, stable citizen/hour identity, keyboard traversal of all 24 hours, and synchronized scrolling/headers. Do not lose row labels while navigating horizontally.
- [ ] Preview exactly the selected cell, citizen row, or hour column scope, including a textual summary of offscreen affected targets. Show the chosen activity beside the apply controls.
- [ ] Revalidate affected citizen IDs and scope at dispatch and after refresh. Use supported recovery or proportionate review for broad changes; avoid invented atomic undo.
- [ ] Preserve current matrix performance and focus anchors during live roster updates and paging.

**Required verification**

- [ ] Run one cell, one full-day row, and one all-citizens hour mutation; compare exact changed IDs/hours against the preview.
- [ ] Navigate every hour with keyboard at narrow width and high scale, keeping row/hour context visible.
- [ ] Test refresh, citizen removal/addition, filtered/paged views, stale selected cell, invalid activity, rejection, and held activation keys.
- [ ] Capture the three scope previews and the resulting authoritative schedules; check non-color comprehension.

**Exit gate:** POP-04 is verified for cell, row, and column operations. No focus-only edit, hidden scope expansion, or offscreen-target ambiguity remains.

**Checkpoint:** Update `stages/13-schedules.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-14"></a>
### Stage 14 — Reorganize military around squad, citizen, role, and destination

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [MIL-01](#audit-mil-01), [MIL-02](#audit-mil-02), [MIL-03](#audit-mil-03), [MIL-04](#audit-mil-04), [MIL-05](#audit-mil-05), [MIL-06](#audit-mil-06), [MIL-07](#audit-mil-07).

**Objective:** Implement the sequence: choose squad → choose citizen → inspect role/destination → act.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#34-stage-14-military).

- The Military manager is a fixed property sheet: Squads / Roles and Uniforms / Target Priorities tabs.
- Squads are a single-selection list with Move Up / Move Down stacked to its right (unavailable at the ends). Members and unassigned citizens use the two-list Add -> / <- Remove pattern; moving to another squad uses a labelled "Move to squad:" drop-down list and an explicit command button.
- Role uniform slots are labelled drop-down lists (sentence caps with colons); a mixed value is blank (PDF p.134-135). Civilian is a check box with its help text beside it.
- Target priorities are a list view with Move Up / Move Down; the Flee / Defend / Attack / Hunt response is a group of option buttons in a group box, separate from ordering (PDF p.126).
- Removing a squad or role uses a Yes/No (or specific verb) message box with the Warning symbol, the object name as title, and the least destructive button as default (PDF p.182-187).

**Inspect/change as required:**

- `content/rmlui/windows/military_manager.rml`
- `content/rmlui/windows/management6c.rcss`
- Squad, membership, role, uniform, and target-response commands mapped in Stage 00.

**Implementation checklist**

- [ ] Replace the peer-view rail with Squads / Roles & Uniforms / Target Priorities connected tabs. Keep hidden cross-route controls out of visual and keyboard navigation.
- [ ] Keep a compact squad list with selected identity and meaningful Move up/down semantics. Separate Delete from routine selection, rename, and order controls; disable boundary operations appropriately.
- [ ] Preserve roster and unassigned dual lists. Put transfer commands adjacent to their source/target lists and the member editor after meaningful selection. Provide an explicit named destination selector when transferring to another squad.
- [ ] Keep all supported membership commands accessible, with stable citizen/squad IDs and a visible source/destination summary. Never transfer membership as a side effect of focusing a row.
- [ ] Use a labeled Civilian checkbox with retreat help. Unify role naming/dirty/delete behavior with profession and squad editors.
- [ ] Present uniform slot/type/material rows and one selected-slot editor using legal catalogs. State shared role versus individual scope before applying.
- [ ] Separate target reordering from the exclusive Flee/Defend/Attack/Hunt response choice. Adopt the shared exact-target removal dialogs, pending status, and rejection handling.

**Required verification**

- [ ] Create/select/rename/reorder/delete squads on a disposable world, including empty squads and list boundaries.
- [ ] Assign, remove, and transfer non-first citizens among multiple squads; refresh or delete targets during a selection and verify the final stable IDs.
- [ ] Change a civilian role and uniform rule, checking all and only the intended role/individual consumers. Validate legal material/type restrictions.
- [ ] Reorder a target without changing response, change response without issuing an attack command, and exercise all confirmation keyboard/stale-target cases.

**Exit gate:** All seven military findings pass. Production captures show target-first flow and connected tabs; command checks prove no membership, role-scope, or attitude/order regression.

**Checkpoint:** Update `stages/14-military.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-15"></a>
### Stage 15 — Migrate diplomacy and mission planning

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [DIP-01](#audit-dip-01), [DIP-02](#audit-dip-02), [DIP-03](#audit-dip-03), [DIP-04](#audit-dip-04).

**Objective:** Make mission planning a clear reviewed sequence while preserving honest unknown information.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#35-stage-15-diplomacy-and-missions).

- Neighbour standing is a property sheet page; unknown facts are the word "Unknown" as static text, never zero or an invented glyph.
- Mission planning is a wizard (property sheet control without tabs) with < Back, Next >, Finish and Cancel; Back is unavailable on the first page and pages never advance by themselves. A short task uses a simple wizard without Welcome/Completion pages (PDF p.304-309).
- Mission type and action are option buttons (exclusive values), not command buttons. The last page reviews destination, action and citizen before Finish starts the mission once.

**Inspect/change as required:**

- `content/rmlui/windows/diplomacy_missions.rml`
- `content/rmlui/windows/management6c.rcss`
- Neighbor, mission-eligibility, dispatch, and result paths mapped in Stage 00.

**Implementation checklist**

- [ ] Use Neighbors / Missions tabs, preserving independent selected IDs, scroll, and any draft mission when switching between them.
- [ ] Retain compact labeled neighbor properties and the undiscovered explanation. Show only backed distance, attitude, wealth, economy, military, timing, and result values; unknown is not zero.
- [ ] Arrange the mission builder as Destination → Mission type → Action → Citizen → Review/Start. Use exclusive value selection for type/action rather than ordinary command styling.
- [ ] Show prerequisite and eligibility failures adjacent to Start. Keep the reviewed destination, action, participant, and any available backed consequence visible before dispatch.
- [ ] Revalidate the chosen combination at commit and show pending/rejected states. Selection or focus changes must not start a mission.
- [ ] Preserve structured status, destination, participants, time, and results in activity details, with text-based completed/failed/unknown presentation.

**Required verification**

- [ ] Exercise discovered, undiscovered, absent, and removed neighbors without fabricating properties or leaving stale actions.
- [ ] Start each supported mission/action combination using an eligible citizen; verify exactly one authoritative mission with the reviewed IDs.
- [ ] Test unsupported types, ineligible/removed participants, changed destination state, canceled review, repeated activation, and rejection.
- [ ] Switch planning/activity tabs, refresh running/completed/failed missions, and verify selection/draft retention plus honest timing/results.

**Exit gate:** DIP-01 through DIP-04 pass their acceptance tests; navigation, value selection, and mission dispatch are distinct and all displayed facts are backed.

**Checkpoint:** Update `stages/15-diplomacy.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-16"></a>
### Stage 16 — Consolidate inspectors, citizen detail, and equipment scope

**Dependencies:** Stage 15 plus the roster/profession, military uniform, and shared edit/state contracts from Stages 12, 14, and 07.

**Primary audit work:** [INS-01](#audit-ins-01), [INS-02](#audit-ins-02), [INS-03](#audit-ins-03), [INS-04](#audit-ins-04), [INS-05](#audit-ins-05), [INS-06](#audit-ins-06), [INS-07](#audit-ins-07), [INS-08](#audit-ins-08), [POP-05](#audit-pop-05).

**Objective:** Keep the best contextual inspectors and remove conflicting copies of citizen/property editing.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#36-stage-16-inspectors).

- The tile/creature inspector is a property inspector in a palette window: a short caption with only Close, it follows the current selection, and its edits apply immediately, so it has no OK/Cancel/Apply (PDF p.167, p.180-181).
- Tabs are allowed; pages do not scroll, only lists do. Crowded tiles list their objects in a list view so any object can be chosen directly. Hierarchies use a tree view (PDF p.137).
- Deliberate citizen editing (profession, equipment) uses the Stage 12 property sheet with pending changes. The paper doll stays as game-specific UI inside a page, with labelled slots.

**Inspect/change as required:**

- `content/rmlui/screens/inspector.rml`
- `content/rmlui/windows/population_manager.rml (citizen detail)`
- Inspector projection, selected-object lifetime, camera, profession, and equipment paths mapped in Stage 00.

**Implementation checklist**

- [ ] Keep tile identity stable and make crowded-tile creatures/items directly selectable by ID. Retain existing shortcuts, but do not make Inspect first creature the only primary multi-object path.
- [ ] Preserve the construction inspector's blocker/material/worker/priority/skill/tool explanation as a reference design. Improve alignment/grouping and isolate destructive cancellation without reducing diagnostic content.
- [ ] Adopt the shared visual tab model while retaining the creature view's existing tab/panel associations. Keep name and relevant live state visible; preserve camera functionality without dominating the property editor.
- [ ] Align attributes and needs, with textual values/units/direction and honest missing data. Share profession combo/skill display logic with population where feasible.
- [ ] Keep the equipment paper doll. Label every real slot, distinguish decorative positions, retain Apply/Cancel, and show individual-versus-shared-role scope before editing/commit. Separate actual equipment from desired rules only where those facts are backed.
- [ ] Use shared reports for carried inventory, with distinct worn/carried/empty states. Keep quick stockpile/workshop/agriculture inspectors compact and link to full managers for complex editing.
- [ ] Consolidate citizen detail components or choose one canonical implementation while retaining supported entry points and Back-to-roster context. Share authoritative projections rather than maintaining divergent editors.

**Required verification**

- [ ] Inspect a crowded changing tile; choose a non-first object and refresh/remove others without changing the target accidentally.
- [ ] Change resources or priority on a blocked construction and verify the explanation follows the actual job.
- [ ] Edit a profession and each supported equipment slot from all entry points. Prove individual versus role-wide updates affect exactly the disclosed scope; decorative slots must not be focusable actions.
- [ ] Switch creature views/entry points, cancel drafts, remove the inspected creature, empty/fill carried inventory, and return to the previous roster/filter/scroll context.
- [ ] Keep a manager and matching inspector open while changing their shared object; verify synchronized pending, success, and rejection states.

**Exit gate:** All eight inspector findings plus POP-05 pass. The canonical/shared detail model preserves every supported command and avoids duplicate state or unintended role-wide edits.

**Checkpoint:** Update `stages/16-inspectors.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-17"></a>
### Stage 17 — Clarify the HUD, armed tools, build navigation, and onboarding

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [HUD-01](#audit-hud-01), [HUD-02](#audit-hud-02), [HUD-03](#audit-hud-03), [HUD-04](#audit-hud-04), [HUD-05](#audit-hud-05), [HUD-06](#audit-hud-06), [HUD-07](#audit-hud-07).

**Objective:** Make commands, overlays, active tools, and world feedback unmistakable without obstructing the game.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#37-stage-17-hud).

- HUD command groups are toolbars: flat buttons (22 x 21 with 16 x 16 images, or 28 x 26 with 20 x 20) that show a 1 px raised border only on hover, grouped with etched separators, each with a ToolTip in book-title caps (PDF p.148, p.152-155, p.319, p.325).
- Persistent overlays and armed tools are toolbar toggle / option buttons with the option-set (pressed, dithered) appearance; an armed tool changes the pointer and Esc cancels it (PDF p.48, p.154, p.322).
- Time, speed and world facts are status-bar panes with the status-field border; essential information is not only in the status bar (PDF p.288-289, p.320).
- Events that need a decision are message boxes (one per condition, no chains); non-critical events go to the status bar (PDF p.182-187).
- Build menus use at most one cascade level, separators, and grayed unavailable items (PDF p.111-119).

**Inspect/change as required:**

- `content/rmlui/screens/game_hud.rml`
- `content/rmlui/screens/orders_tools.rml`
- `content/rmlui/screens/inspector.rml (world labels)`
- HUD/tool, map-input, event-dialog, and tutorial paths mapped in Stage 00.

**Implementation checklist**

- [ ] Group time/speed controls separately from world facts, with distinct requested versus authoritative pause feedback. Preserve compact access to essential simulation controls.
- [ ] Differentiate manager launchers, armed tools, and persistent overlay toggles. Clarify duplicate labels such as Jobs/Designations using action meaning and context.
- [ ] Provide one persistent armed-tool indicator with Cancel and relevant Rotate guidance across integrated and detached tool hosts. Define what closing or switching a tool window does to the armed tool and make that result visible.
- [ ] Retain hierarchical build categories and preserve selected category/product/material/orientation on supported back navigation. Keep requirements adjacent to placement.
- [ ] Keep world labels/pointer feedback compact, within bounds, and non-obstructive. Explain invalid placement with text or symbols as well as color.
- [ ] Prioritize the tutorial's next objective, move restart/skip to a secondary area, and distinguish skipped/Continue anyway from verified completion. Preserve tutorial progress and do not hide the target needed to complete the objective.
- [ ] Migrate event dialogs to the common modal contract and expose only the relevant command set. Review all close/keyup/repeat/input-consumption paths with a world tool armed.

**Required verification**

- [ ] Arm every supported tool family, navigate managers, rotate, cancel, switch hosts, and close/reopen tool UI. Verify no stale tool silently executes.
- [ ] Test click, double-click, wheel, drag, and keyboard over every UI region, including popup and modal closure; no map order, layer change, or world drag may leak through.
- [ ] Exercise speed changes and delayed pause acknowledgment; selected visual state must match the documented authoritative/pending meaning.
- [ ] Complete, skip, restart, and bypass supported tutorial steps; verify progress truthfulness, focus, and target visibility.
- [ ] Handle successive/nested events and held Enter/Escape without accepting another event or acting on the map.

**Exit gate:** All seven HUD findings pass. Tool state and cancellation are visible, command/overlay meaning is consistent, and UI interaction is isolated from map commands.

**Checkpoint:** Update `stages/17-hud-tools.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-18"></a>
### Stage 18 — Align the main menu, new-game setup, and save browser

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [APP-01](#audit-app-01), [APP-02](#audit-app-02), [APP-03](#audit-app-03), [APP-04](#audit-app-04).

**Objective:** Make application entry and setup coherent without changing supported game parameters or save compatibility.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#38-stage-18-main-menu-new-game-save-browser).

- The main menu is a dialog box with a column of 50 x 14 DLU command buttons in book-title caps, default command first (the book has no title screen; PDF p.169, p.346).
- Custom new game is an advanced wizard (Welcome, interior pages with header title and subtitle, Completion) with Back / Next / Finish / Cancel and defaults on every page (PDF p.304-309). Presets are a drop-down list; the seed is a labelled text box.
- The save browser follows the Open dialog: caption "Load Game", a list view in details view (Name / Kingdom / Modified), Open as default button beside Cancel, double-click opens, Delete confirms with a Yes/No message box (PDF p.170-173).

**Inspect/change as required:**

- `content/rmlui/screens/main_menu.rml`
- `content/rmlui/screens/new_game.rml`
- `content/rmlui/screens/load_game.rml`
- `content/rmlui/screens/shell.rcss`
- Start/setup/save-browser controllers mapped in Stage 00.

**Implementation checklist**

- [ ] Keep the main menu's Continue/Load and tutorial/quick/custom-start hierarchy, useful disabled reasons, and game branding. Reduce redundant framing and use shared states; Exit must not dominate the normal task flow.
- [ ] Implement a coherent custom-setup wizard with Back / Next / Start kingdom on review. Preserve quick-start/tutorial routes. If the current code already implements a correct property-page model, record the evidence and keep it coherent rather than reverting it unnecessarily.
- [ ] Retain every supported generation parameter and draft value. Replace excessive small cards with group boxes, aligned numeric editors, visible units, and concise dependency help.
- [ ] Make field and cross-field validation explicit. Review must display the actual submitted draft, not independent stale summary values. Prevent repeated Start from generating multiple worlds.
- [ ] Keep Kingdoms/Saves as a master-detail report. Show identifying metadata and compatibility/rejection reasons only when available from the backend; make Load selected save target explicit.
- [ ] Preserve selection and recoverability through refresh, changing kingdoms, failed loads, and back navigation. Continue using disposable save copies.

**Required verification**

- [ ] Open the menu with no compatible save, a compatible save, and an incompatible save; verify commands and reasons.
- [ ] Change every legal setup parameter exactly, test invalid combinations, navigate backward/forward, cancel/reopen as documented, and compare submitted values to the review.
- [ ] Exercise quick/tutorial/custom entry points and repeated Start activation without changing gameplay semantics.
- [ ] Refresh/reorder/delete save entries, change kingdoms, load a non-first save, fail a load, and recover without loading an unintended file.

**Exit gate:** APP-01 through APP-04 pass. Setup navigation is internally coherent, no parameter is lost, and save-browser actions retain explicit stable targets.

**Checkpoint:** Update `stages/18-menu-setup-saves.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-19"></a>
### Stage 19 — Align immediate settings and safe pause/save/load transitions

**Dependencies:** Previous stage integrated and verified.

**Primary audit work:** [APP-05](#audit-app-05), [APP-06](#audit-app-06), [APP-07](#audit-app-07), [APP-08](#audit-app-08).

**Objective:** Preserve truthful setting persistence, authoritative transition state, and recoverable loading behavior.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#39-stage-19-settings-pause-loading).

- The book documents two models: a property sheet whose settings stay pending until OK/Apply (OK/Cancel/Apply), and a property inspector whose settings apply immediately (Close only). Replacing Cancel with Close on a sheet is not documented. Settings must use one of these honestly; a staged sheet is permitted when fully implemented and tested.
- Exact values use spin boxes; approximate ones use sliders with parallel Low/High range labels (PDF p.141, p.146).
- Unsaved progress on Load or Main Menu asks "Do you want to save changes to <save>?" with Yes / No / Cancel (PDF p.166, p.183-185).
- Pause is a dialog box with command buttons, Resume as the default; Esc resumes (PDF p.48, p.169).
- Loading is a progress message box: a solid progress indicator with text outside the bar; no invented percentage (use named stages); Stop instead of Cancel if the load cannot be undone (PDF p.145, p.185).

**Inspect/change as required:**

- `content/rmlui/screens/settings.rml`
- `content/rmlui/screens/pause_menu.rml`
- `content/rmlui/screens/loading.rml`
- `content/rmlui/screens/shell.rcss`
- Settings persistence, pause, saving/loading, and recovery paths mapped in Stage 00.

**Implementation checklist**

- [ ] Use compact Display / Controls / Audio / Saving property pages or equivalent restrained groups, preserving the documented immediate-save model and Close. Do not add decorative Apply/Cancel.
- [ ] Keep actual preference dependencies explicit, use numeric editing where exact values matter, and show units. Make Reset defaults disclose whether it affects one page or all preferences; preserve that declared scope.
- [ ] Verify and document a safe recovery path from unusable UI-scale/display settings using existing mechanisms. Add a minimal appropriate recovery mechanism only when necessary and tested; do not leave the application inaccessible.
- [ ] Keep requested versus authoritative pause distinct, preserve save feedback, and apply deliberate unsaved-progress confirmation to load/menu transitions.
- [ ] Prevent repeated activation from triggering multiple saves, loads, or conflicting transitions. Preserve the intended pause state after canceled/failed operations and when closing settings.
- [ ] Adopt truthful progress or named indeterminate stages in loading. Focus actionable errors, keep meaningful diagnostics and working Back/Retry paths, and avoid orphaned input blockers.

**Required verification**

- [ ] Change each supported setting, close/reopen, restart where required, and verify actual persistence. Test dependencies, bounds, defaults scope, and scale/display recovery.
- [ ] Exercise save success/failure, canceled load, failed load, settings from pause, return to menu with dirty progress, and repeated activation.
- [ ] Simulate or legally induce slow/failing operations using a clearly labeled test hook when needed; also verify a genuine successful save/load with a disposable world.
- [ ] Check error focus, Retry/Back, key release on modal closure, and pause restoration without a world click-through.

**Exit gate:** APP-05 through APP-08 pass. No fake Cancel, invented loading percentage, silent persistence mismatch, duplicate transition, or unrecoverable scale change remains.

**Checkpoint:** Update `stages/19-settings-transitions.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-20"></a>
### Stage 20 — Verify scaling, keyboard coverage, accessibility variants, and performance

**Dependencies:** All production-route stages 08–19 plus shared components 01–07 are integrated; no unresolved upstream safety prerequisite.

**Primary audit work:** [SYS-30](#audit-sys-30), [SYS-31](#audit-sys-31).

**Objective:** Test the integrated application across environments and states rather than relying on isolated screen success.

**Windows 98 design:** Only UI documented in the Windows User Experience book and MSDN Visual Design may be used (owner rule, 2026-09-25); citations are in [07-win98-design-reference.md](07-win98-design-reference.md#310-stage-20-scaling-keyboard-accessibility).

- Tab order left to right, top to bottom, commit buttons last; arrows move within option groups and lists; Ctrl+Tab / Ctrl+Page Down / Ctrl+Page Up switch tabs; Enter = default button, Esc = Cancel / close drop-down / stop mode (PDF p.47-48, p.147, p.161-162).
- The dotted focus rectangle is always drawn in the active window and is separate from selection (PDF p.50, p.325, p.358).
- High Contrast replaces the palette and hides images behind text (PDF p.373-374); color and sound are never the only cue.
- Whole-number scaling of the pixel-exact font and metrics; windows fit a 640 x 480 screen at 1x (PDF p.342).

**Inspect/change as required:**

- All current production RmlUi routes and supported host implementations.
- `content/rmlui/styles/accessibility.rcss`
- Regression/capture/large-data suites established in earlier stages.

**Implementation checklist**

- [ ] Execute the full verification matrix below against every supported route/conditional variant, host mode, and documented scale. Record actual OS DPI and UI scale separately; do not confuse logical context size with screen resolution.
- [ ] Test minimum supported host dimensions, a representative ordinary size, and large/high-DPI usage. Fix clipping, inaccessible columns, popup escape, hidden focus, and unusable title/close controls without shrinking text ad hoc.
- [ ] Complete keyboard-only coverage of every command in the parity inventory, including hidden/conditional pages, bulk actions, popup cancellation, focus restoration, and all supported accelerators.
- [ ] Trace real high-contrast/reduced-motion preference application and platform accessibility integration. Verify only genuinely supported capabilities; record missing platform bridges honestly instead of claiming ARIA equals screen-reader support.
- [ ] Run long-name, large-list, rapid-refresh, stale-target, rejection, and save/reopen scenarios. Compare performance against the Stage 00 baseline using measured durations/element counts on the same conditions.
- [ ] Fix any cross-route regression, then rerun both the affected route and shared/sentinel tests. Update all shared-system IDs with actual consumer-level evidence, not just fixture coverage.

**Required verification**

- [ ] Run every mandatory row of the production verification matrix with pass/fail/blocked and evidence; no unrun case may be recorded as pass.
- [ ] Demonstrate one complete keyboard-only management workflow through selection, edit, confirmation, rejection/recovery, and return to the world for each route family.
- [ ] Recheck UI-to-world input isolation in every supported host with a tool armed and through modal/popup key release.
- [ ] Review actual rendered captures for contrast, focus, state combinations, baseline alignment, tab seams, scroll reachability, and typography at each supported scale.
- [ ] Rerun representative large-data measurements and investigate material regressions instead of suppressing or changing the baseline silently.

**Exit gate:** SYS-30 and SYS-31 meet verified supported-surface acceptance, and the integrated matrix has no unexplained failures. Platform/runtime evidence that cannot be obtained is explicitly BLOCKED and prevents an unconditional completion claim.

**Checkpoint:** Update `stages/20-integrated-verification.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---

<a id="stage-21"></a>
### Stage 21 — Remove obsolete styles and complete the evidence-backed handoff

**Dependencies:** Stage 20 and every scoped upstream acceptance gate, followed by regression checks after cleanup.

**Audit coverage:** All 98 original items and any current-production discoveries.

**Objective:** Finish the whole migration without silently dropping audit items, commands, or verification requirements.

**Windows 98 design:** run the conformance checklist and the "Not documented: do not use" list in [07-win98-design-reference.md](07-win98-design-reference.md#311-stage-21-conformance-checklist) against every Win98 surface.

**Inspect/change as required:**

- All task-owned code/tests/documentation.
- `docs/ui-win98/ or the documented existing equivalent`
- Obsolete theme/route assets only after reference and runtime verification.

**Implementation checklist**

- [ ] Review all 98 original IDs and every NEW item. Require current acceptance evidence for VERIFIED or a specific defensible reason for NOT_APPLICABLE; no open IMPLEMENTED-only item is a finished result.
- [ ] Search for obsolete palette overrides, duplicate filters/combos, superseded fixtures, dead hidden controls, inaccessible shortcuts, outdated theme names, and route-local state definitions. Check dynamic references before removing anything.
- [ ] Remove genuinely unused code/assets and update verifier expectations, component documentation, screenshots, and contributor instructions. Preserve selectable themes and compatibility shims that still have real consumers.
- [ ] Rerun the full build/test/verifier suite and affected production matrix after cleanup. A cleanup commit can invalidate earlier evidence; refresh relevant captures and workflow checks.
- [ ] Compare the final command-parity matrix against the baseline: every supported action remains reachable and uses correct target, scope, edit, and acknowledgment behavior.
- [ ] Prepare a final report naming baseline/final revisions, changed areas, checks actually run, evidence, compatibility notes, supported limitations, and any externally blocked gates. Do not claim 100/100 or complete parity merely because the checklist is long.

**Required verification**

- [ ] Validate that every original audit ID appears exactly once in the traceability register and has a defensible final disposition; validate each additional discovery too.
- [ ] Perform a fresh configured build using documented commands, without cleaning or resetting the user worktree destructively, and run existing plus new tests.
- [ ] Execute a representative disposable-world walkthrough: load/create, inspect construction, manage inventory/stockpile/workshop/farm/population/military/missions, edit equipment, save, reload, and verify state.
- [ ] Inspect the final diff, dynamic selector/action references, asset licenses, final screenshots, and evidence links; remove temporary test hooks from release paths.

**Exit gate:** The release definition of done below is satisfied. Otherwise report PARTIAL / BLOCKED with specific outstanding gates and a resume checkpoint; never relabel missing verification as success.

**Checkpoint:** Update `stages/21-final-handoff.md`, `00-status.md`, audit/command traceability, and evidence with actual results. Record outstanding downstream coverage explicitly.

---


<a id="verification"></a>
## 7. Production verification matrix

Run relevant cases during each stage; run the full integrated coverage in Stage 20 and refresh affected evidence after Stage 21 cleanup. Screenshots must come from the actual production RmlUi runtime, not a browser mockup. Clearly distinguish a component fixture, a test-injected state, and real-world gameplay.

| Family | Required scenarios | What must be demonstrated |
|---|---|---|
| Build and asset integrity | Actual supported build/configuration; pinned dependencies; token generation; license/font checks. | Documented commands run; no newly introduced compiler, parser, missing-asset, or active-style errors. |
| Route completeness | Every production route, tab, conditional panel, popup, modal, and inspector entry point. | All baseline commands are still reachable; new discoveries are added to the tracker rather than ignored. |
| Host/window behavior | Each supported integrated/detached/native-host variant; move, resize, active/inactive, close/reopen. | No duplicate chrome, inaccessible title/Close, orphan popup, or incorrect host ownership. |
| Size and scale | Minimum supported dimensions, ordinary working size, long text; 100%, 125%, 150%, and 200% UI scale where supported, with OS DPI recorded separately. | Every essential value/control is readable and reachable. If a target scale is not supported, document the actual limitation and test supported endpoints; do not quietly call it passed. |
| Control-state combinations | Focus + selection, focus + default, disabled + selected, error/status + selection, hover + press, pending. | States remain independent and comprehensible without layout movement or misleading color meaning. |
| Keyboard | Tab/Shift+Tab, arrows, Space, Enter, Escape, and real supported accelerators. | All actions reachable; focus visible; hidden/disabled alternatives excluded appropriately; no mutation from programmatic focus alone. |
| Generated elements | Select popup and actual scrollbar thumb, track, arrows, both axes, corner, no-range state. | Selectors match real generated parts, not stand-in markup; controls work with mouse and keyboard. |
| World input isolation | Click/double-click/wheel/drag/key events in UI; popup/modal close; key-up/repeat while a tool is armed. | No unintended map order, world drag, layer change, or second operation from the same event. |
| Modal lifecycle | Open/cancel/accept, held keys, rapid repeated click, nested event, target deletion, parent close. | Exactly one deliberate action; background blocked; focus restored; no residual blocker or accidental acceptance. |
| Data lifecycle | Empty, filtered-empty, zero, unknown, undiscovered, unavailable, loading, populated, pending, rejected, deleted. | Correct distinctions, accurate messages, no fabricated values, useful recovery where actually supported. |
| Selection stability | Sort/filter/reset/page/refresh/reorder/delete/tab/detail/Back, duplicate names, rapid updates. | Stable ID target preserved or intentionally relocated; scroll/focus restored; row index never silently retargets an edit. |
| Draft/immediate semantics | Modify, Apply/Save, immediate toggle, Discard/Revert, tab change, target change, Close, reopen, external update. | Actual persistence matches the declared model. Discard draft does not pretend to undo instant changes. |
| Scope correctness | Matching allow rules; all-citizen skills; schedule cell/day/hour; selected plots/default/queue; role equipment; trade/mission review. | Before/after authoritative target IDs and values equal the reviewed affected scope, including offscreen matches. |
| Stale/rejected commands | Target removed, eligibility changed, data refreshed, invalid legal value, command rejection, delayed acknowledgment. | No false success, duplicate command, wrong-target mutation, or silent overwrite of an outdated draft. |
| Accessibility variants | Keyboard-only; high contrast/reduced motion when wired; semantic labels and actual platform bridge if present. | Focus and meaning survive route styles. Report unsupported assistive technology truthfully; markup alone is not certification. |
| Performance and long sessions | Representative large inventories/rosters, rapid updates, repeated open/close, scrolling, popups. | No significant unexplained regression against a comparable measured baseline; no accumulating handlers/elements or repeated command dispatch. |
| Persistence and transitions | Existing compatible save copy; new save; save success/failure; canceled/failed load; retry; return to menu; pause restore. | Correct file/target, preserved intended state, no corruption or silent compatibility change, no click-through on recovery. |
| Visual review | Before/after with comparable host/data/scale; inspect rendered output directly. | Connected tabs, consistent chrome, restrained grouping, legible text, aligned reports, coherent state grammar. |

### Evidence record per case

```text
Case ID and related audit IDs:
Stage / route / exact reproduction steps:
Build revision and any uncommitted task-owned changes:
Operating system / renderer / pinned RmlUi version:
Host mode / client size / RmlUi context size / OS DPI / UI scale:
Input method:
Data provenance (real disposable save, component fixture, or injected test state):
Expected visual and behavioral result:
Observed result:
Target stable IDs / reviewed scope / dispatched command / before-after state:
Actual command or test invocation, exit result, and log path:
Screenshot or runtime capture path (where applicable):
Result: PASS / FAIL / BLOCKED
Remaining uncertainty or supported-platform limitation:
```

Include enough information for another agent to reproduce the result. Use focused structured command/state evidence for mutations and screenshots for visual claims. Screenshot-only proof does not establish that the correct citizens, plots, jobs, or saves were changed.

### Required integrated walkthrough

Use one or more disposable legal worlds/saves that collectively expose the necessary features. A feature not yet unlocked is a test-data prerequisite, not evidence of an absent UI.

1. Start or load a world; verify pause/speed and a supported placement tool.
2. Open an inventory filter, inspect an item, and return with context intact.
3. Change a stockpile rule subset and its staged priority; verify scope and persistence.
4. Create and edit a workshop order; perform a reviewed trade in a legal trade scenario.
5. Assign farm plot crops, change a farm default, modify a planting queue, and directly edit a non-first pasture rule.
6. Modify a profession, one schedule cell, a citizen's day, and an hour across the disclosed population.
7. Assign and transfer squad members, edit a role/uniform rule, and change target response separately from target order.
8. Plan a supported mission, review exact target/participant, and inspect running/completed results as available.
9. Inspect construction blockers, edit a creature's backed profession/equipment settings with explicit scope, and verify all entry points agree.
10. Change a supported setting, save, reload, and verify persisted game/settings state. Exercise a rejected/canceled operation and recover without a world input leak.

<a id="release"></a>
## 8. Release definition of done

The migration is complete only when all of the following are true:

- [ ] All 22 stage gates are `VERIFIED`; there is no mandatory runtime check disguised as a documentation-only completion.
- [ ] Every one of the 98 original audit IDs is `VERIFIED` or has a specific evidence-backed `NOT_APPLICABLE` disposition. All additional in-scope discoveries are also resolved. No item is omitted because it was low priority.
- [ ] The current production theme has one coherent token/cascade contract. Equivalent controls have consistent state semantics and actual rendered treatment across consumers.
- [ ] Every baseline-supported action remains reachable with the correct stable target, scope, constraints, persistence behavior, and authoritative acknowledgment path.
- [ ] All applicable destructive/bulk/draft/immediate operations satisfy their behavioral gates; no wrong-target edit, fake Cancel, fabricated value, hidden scope expansion, or duplicated operation remains.
- [ ] All supported hosts pass input-isolation, modal/popup, keyboard, minimum-size, and scale checks. Any unsupported capabilities are described accurately, not counted as passing supported capabilities.
- [ ] Required builds, verifier checks, new/affected tests, and production scenarios have current results. Unrelated pre-existing failures are separated clearly; never describe a failing full suite as green.
- [ ] Large-list performance, save compatibility, settings persistence, and existing gameplay semantics have no unexplained regression attributable to this work.
- [ ] Actual before/after production screenshots and behavioral evidence are linked, reproducible, and labeled with data provenance and revision.
- [ ] Obsolete styles/components are removed only after dynamic-reference checks; legal asset checks and relevant compatibility paths remain intact.
- [ ] The final diff contains no unrelated edits, unsafe repository operations, temporary release-path test hooks, or accidental external publication.
- [ ] The final handoff states exactly what changed, what was tested, supported limitations, and the final revision/checkpoint.

An external test-environment limitation may justify a **partial delivery**, but not a claim that the full migration is verified. Do not weaken acceptance criteria or mark blocked tests `NOT_APPLICABLE` simply to close the task.

### Final report structure

```text
Outcome: VERIFIED COMPLETE / PARTIAL / BLOCKED
Baseline revision:
Final revision and task-owned uncommitted changes:
Stages verified / blocked:
Audit IDs verified / not applicable / still open:
Main changes by shared system and route:
Preserved gameplay and command contracts:
Build/test commands actually run and their outcomes:
Production runtime and visual evidence:
Save/settings compatibility and performance results:
Supported limitations and unresolved blockers:
Exact resume entry point, if unfinished:
```

<a id="coverage"></a>
## 9. Audit-to-stage coverage index

There are exactly **98 original IDs** below, each with one primary delivery stage. Original priority is retained: P1 is next-pass verification/interaction risk, P2 usability/layout, and P3 polish. These priorities are not proof of reproduced runtime severity. The audit established no P0 runtime defect.

Initialize the working traceability register from this index, adding current disposition, consuming routes, changed files, test/evidence references, and remaining gaps. The links go to the embedded details, so no separate audit attachment is required.

| Audit ID | Element/control group | Original priority | Primary stage |
|---|---|---|---|
| [SYS-01](#audit-sys-01) | Theme ownership | P1 | [02](#stage-02) |
| [SYS-02](#audit-sys-02) | Stylesheet cascade | P1 | [02](#stage-02) |
| [SYS-03](#audit-sys-03) | Window frame and resize affordance | P2 | [03](#stage-03) |
| [SYS-04](#audit-sys-04) | Title bars and Close controls | P2 | [03](#stage-03) |
| [SYS-05](#audit-sys-05) | Icons and glyphs | P3 | [03](#stage-03) |
| [SYS-06](#audit-sys-06) | Typography | P2 | [03](#stage-03) |
| [SYS-07](#audit-sys-07) | Ordinary push buttons | P2 | [04](#stage-04) |
| [SYS-08](#audit-sys-08) | Default/primary button | P2 | [04](#stage-04) |
| [SYS-09](#audit-sys-09) | Destructive buttons | P2 | [04](#stage-04) |
| [SYS-10](#audit-sys-10) | Keyboard focus versus selection | P1 | [04](#stage-04) |
| [SYS-11](#audit-sys-11) | Property tabs | P1 | [04](#stage-04) |
| [SYS-12](#audit-sys-12) | Category navigation rails | P2 | [04](#stage-04) |
| [SYS-13](#audit-sys-13) | Group boxes and content panels | P2 | [03](#stage-03) |
| [SYS-14](#audit-sys-14) | Text inputs and labels | P2 | [05](#stage-05) |
| [SYS-15](#audit-sys-15) | Checkboxes and persistent Boolean options | P1 | [05](#stage-05) |
| [SYS-16](#audit-sys-16) | Exclusive choices | P1 | [05](#stage-05) |
| [SYS-17](#audit-sys-17) | Combos and drop-down lists | P1 | [05](#stage-05) |
| [SYS-18](#audit-sys-18) | Numeric fields and spinners | P2 | [05](#stage-05) |
| [SYS-19](#audit-sys-19) | Sliders | P2 | [05](#stage-05) |
| [SYS-20](#audit-sys-20) | Report tables and list views | P2 | [06](#stage-06) |
| [SYS-21](#audit-sys-21) | Column filters | P2 | [06](#stage-06) |
| [SYS-22](#audit-sys-22) | Selected, hovered and inactive rows | P1 | [06](#stage-06) |
| [SYS-23](#audit-sys-23) | Scrollbars | P1 | [06](#stage-06) |
| [SYS-24](#audit-sys-24) | Trees and matrices | P2 | [06](#stage-06) |
| [SYS-25](#audit-sys-25) | Tooltips | P2 | [07](#stage-07) |
| [SYS-26](#audit-sys-26) | Progress and loading indicators | P2 | [07](#stage-07) |
| [SYS-27](#audit-sys-27) | Status bars and badges | P2 | [07](#stage-07) |
| [SYS-28](#audit-sys-28) | Modal dialog contract | P1 | [07](#stage-07) |
| [SYS-29](#audit-sys-29) | Empty, unavailable and error states | P2 | [07](#stage-07) |
| [SYS-30](#audit-sys-30) | Narrow windows and scaling | P1 | [20](#stage-20) |
| [SYS-31](#audit-sys-31) | Accessibility and input verification | P1 | [20](#stage-20) |
| [SYS-32](#audit-sys-32) | Design-system verifier | P1 | [01](#stage-01) |
| [HUD-01](#audit-hud-01) | Top rail: level, time, speed and summary | P2 | [17](#stage-17) |
| [HUD-02](#audit-hud-02) | Command shelf versus overlays | P1 | [17](#stage-17) |
| [HUD-03](#audit-hud-03) | Active-tool and cancellation feedback | P1 | [17](#stage-17) |
| [HUD-04](#audit-hud-04) | Build categories and catalog | P2 | [17](#stage-17) |
| [HUD-05](#audit-hud-05) | World labels and selection pointer | P2 | [17](#stage-17) |
| [HUD-06](#audit-hud-06) | Tutorial panel | P2 | [17](#stage-17) |
| [HUD-07](#audit-hud-07) | Event dialogs | P1 | [17](#stage-17) |
| [INV-01](#audit-inv-01) | Six-column report header | P2 | [08](#stage-08) |
| [INV-02](#audit-inv-02) | Header filter inputs and popups | P2 | [08](#stage-08) |
| [INV-03](#audit-inv-03) | Inventory row density and narrow width | P1 | [08](#stage-08) |
| [INV-04](#audit-inv-04) | Item detail and Back to inventory | P2 | [08](#stage-08) |
| [STO-01](#audit-sto-01) | Stock / Allow list / Settings navigation | P2 | [09](#stage-09) |
| [STO-02](#audit-sto-02) | Physical stock table | P2 | [09](#stage-09) |
| [STO-03](#audit-sto-03) | Allow list and bulk matching actions | P1 | [09](#stage-09) |
| [STO-04](#audit-sto-04) | Saved allow-list templates | P2 | [09](#stage-09) |
| [STO-05](#audit-sto-05) | Name, priority and Apply | P2 | [09](#stage-09) |
| [STO-06](#audit-sto-06) | Hauling checkboxes | P2 | [09](#stage-09) |
| [STO-07](#audit-sto-07) | Suspend/resume state | P2 | [09](#stage-09) |
| [WRK-01](#audit-wrk-01) | Craft / Queue / Settings / Trade navigation | P2 | [10](#stage-10) |
| [WRK-02](#audit-wrk-02) | Available crafts and new order | P2 | [10](#stage-10) |
| [WRK-03](#audit-wrk-03) | Production queue and order editor | P2 | [10](#stage-10) |
| [WRK-04](#audit-wrk-04) | Workshop settings edit model | P1 | [10](#stage-10) |
| [WRK-05](#audit-wrk-05) | Linked stockpiles | P2 | [10](#stage-10) |
| [WRK-06](#audit-wrk-06) | Special production options | P2 | [10](#stage-10) |
| [WRK-07](#audit-wrk-07) | Trade ledger and confirmation | P1 | [10](#stage-10) |
| [AGR-01](#audit-agr-01) | Overview and statistics | P2 | [11](#stage-11) |
| [AGR-02](#audit-agr-02) | Farm plot grid | P1 | [11](#stage-11) |
| [AGR-03](#audit-agr-03) | Crop catalog and availability | P2 | [11](#stage-11) |
| [AGR-04](#audit-agr-04) | Assign crop / Use farm default / Set farm default | P1 | [11](#stage-11) |
| [AGR-05](#audit-agr-05) | Planting counts and repeat queue | P1 | [11](#stage-11) |
| [AGR-06](#audit-agr-06) | Farm and grove work toggles | P2 | [11](#stage-11) |
| [AGR-07](#audit-agr-07) | Pasture animals, caps and food rules | P1 | [11](#stage-11) |
| [AGR-08](#audit-agr-08) | Designation settings and locate | P2 | [11](#stage-11) |
| [POP-01](#audit-pop-01) | Citizen roster, search, sort and paging | P2 | [12](#stage-12) |
| [POP-02](#audit-pop-02) | Skill comparison and bulk enable/disable | P1 | [12](#stage-12) |
| [POP-03](#audit-pop-03) | Profession editor | P1 | [12](#stage-12) |
| [POP-04](#audit-pop-04) | 24-hour schedule grid | P1 | [13](#stage-13) |
| [POP-05](#audit-pop-05) | Citizen detail overlap | P2 | [16](#stage-16) |
| [MIL-01](#audit-mil-01) | Military page navigation | P1 | [14](#stage-14) |
| [MIL-02](#audit-mil-02) | Squad list, identity and ordering | P2 | [14](#stage-14) |
| [MIL-03](#audit-mil-03) | Roster, unassigned citizens and transfers | P1 | [14](#stage-14) |
| [MIL-04](#audit-mil-04) | Roles and civilian behavior | P2 | [14](#stage-14) |
| [MIL-05](#audit-mil-05) | Uniform slot/type/material editor | P2 | [14](#stage-14) |
| [MIL-06](#audit-mil-06) | Target priorities and attitudes | P1 | [14](#stage-14) |
| [MIL-07](#audit-mil-07) | Military confirmations and feedback | P1 | [14](#stage-14) |
| [DIP-01](#audit-dip-01) | Neighbors / Missions navigation | P2 | [15](#stage-15) |
| [DIP-02](#audit-dip-02) | Neighbor list and properties | P2 | [15](#stage-15) |
| [DIP-03](#audit-dip-03) | New mission builder | P1 | [15](#stage-15) |
| [DIP-04](#audit-dip-04) | Mission details and results | P2 | [15](#stage-15) |
| [INS-01](#audit-ins-01) | Tile inspector and context actions | P2 | [16](#stage-16) |
| [INS-02](#audit-ins-02) | Construction/blueprint inspector | P2 | [16](#stage-16) |
| [INS-03](#audit-ins-03) | Creature preview navigation and camera | P2 | [16](#stage-16) |
| [INS-04](#audit-ins-04) | Attributes and needs | P2 | [16](#stage-16) |
| [INS-05](#audit-ins-05) | Expertise and profession selector | P2 | [16](#stage-16) |
| [INS-06](#audit-ins-06) | Equipment paper doll and slot editor | P1 | [16](#stage-16) |
| [INS-07](#audit-ins-07) | Carried inventory and empty states | P2 | [16](#stage-16) |
| [INS-08](#audit-ins-08) | Stockpile/workshop/agriculture summaries | P2 | [16](#stage-16) |
| [APP-01](#audit-app-01) | Main menu | P2 | [18](#stage-18) |
| [APP-02](#audit-app-02) | Setup navigation model | P2 | [18](#stage-18) |
| [APP-03](#audit-app-03) | World-generation fields and layout | P2 | [18](#stage-18) |
| [APP-04](#audit-app-04) | Kingdom/save browser | P2 | [18](#stage-18) |
| [APP-05](#audit-app-05) | Settings shell and grouping | P2 | [19](#stage-19) |
| [APP-06](#audit-app-06) | Display, controls, audio and saving controls | P2 | [19](#stage-19) |
| [APP-07](#audit-app-07) | Pause/save/load/menu transitions | P1 | [19](#stage-19) |
| [APP-08](#audit-app-08) | Loading, error and retry screen | P2 | [19](#stage-19) |
| [APP-09](#audit-app-09) | Generic destructive confirmation | P1 | [07](#stage-07) |

<a id="embedded-audit"></a>
## 10. Embedded element-level worklist

**Provenance:** Extracted from the supplied `Ingnomia_Windows_98_UI_Audit.md`, which describes revision `bca0bee3acf79d460360c5f6b96fde08db784585`. Observations and original scores below are historical source-level assessments, not new runtime findings. Reconcile each against the authorized checkout before making changes. The stage instructions add sequencing, execution safeguards, and verification; they do not turn historical risks into confirmed bugs.

**Path convention:** Within each original **Source** field, paths are relative to `content/rmlui/` unless they begin with `tests/`. Resolve current equivalents in Stage 00 and inspect associated dynamic/controller/host code as well.

Preserve each **Keep** requirement. Implement each valid **Fix** through the common system and owning route, then run its **Acceptance** test. A disproved or obsolete observation needs a documented current disposition; do not force an unnecessary rewrite. Historical scores do not need to be re-awarded to finish this project.

<a id="audit-sys-01"></a>
### SYS-01 — Theme ownership

**Primary delivery stage:** [02](#stage-02). **Original priority:** P1. **Historical score:** 4/10.

**Source:** `styles/tokens.json; styles/base.rcss`.

**Observed in source:** The token document identifies classic-park, retains a cave-design schema and dark palette, and specifies dimensions different from later shared overrides. Management-specific styles add a third, Win98-gray palette.

**Keep:** A named token source and reusable styles already exist.

**Fix:** Establish a single production theme contract. Give surfaces, text, borders, focus, selection and disabled state semantic names. Keep historical themes only behind explicit theme scope; regenerate expanded RCSS values.

**Acceptance:** One documented production token set accounts for every shared state. Changing a token changes all intended production controls, without relying on obsolete comments.

<a id="audit-sys-02"></a>
### SYS-02 — Stylesheet cascade

**Primary delivery stage:** [02](#stage-02). **Original priority:** P1. **Historical score:** 4/10.

**Source:** `styles/base.rcss; styles/components.rcss; templates/management_window.rml`.

**Observed in source:** Dark declarations are followed by classic-park replacements, then route-specific Win98 rules. Accessibility is loaded before additional management styling.

**Keep:** Shared templates provide a useful consolidation point.

**Fix:** Separate structural rules from skin rules. Remove superseded production declarations after coverage exists. Explicitly test accessibility precedence rather than appending another override block.

**Acceptance:** A component-state gallery and representative production windows report the same computed colors, borders and dimensions for equivalent controls.

<a id="audit-sys-03"></a>
### SYS-03 — Window frame and resize affordance

**Primary delivery stage:** [03](#stage-03). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/components.rcss; windows/management6b.rcss`.

**Observed in source:** Shared windows use a pale-blue frame, small radius and hard shadow; inventory explicitly uses gray square chrome without shadow. The shared resize grip starts hidden.

**Keep:** Movable framed windows and explicit content regions fit an object-management application.

**Fix:** Use one gray square frame and consistent raised edges. Decide which host owns native versus custom chrome. Show resize affordances only where resizing really works; do not add decorative minimize/maximize controls.

**Acceptance:** Drag, resize, close, reopen and change UI scale in each supported host. The frame remains reachable and there is no duplicate native/custom title bar.

<a id="audit-sys-04"></a>
### SYS-04 — Title bars and Close controls

**Primary delivery stage:** [03](#stage-03). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/components.rcss; windows/military_manager.rml; windows/workshop_manager.rml`.

**Observed in source:** Most templates have a word-sized Close button in their header. Several object managers have a subtype plus title, while other windows use a single-line shared title.

**Keep:** The selected object is identified in the window header.

**Fix:** Standardize a compact title, optional small object icon, and small close glyph with an accessible name. Move subtype and coordinates into a secondary information row where needed. Differentiate active and inactive hosts.

**Acceptance:** Long names truncate safely while the full name is recoverable. Close has the same hit area and keyboard behavior in every window.

<a id="audit-sys-05"></a>
### SYS-05 — Icons and glyphs

**Primary delivery stage:** [03](#stage-03). **Original priority:** P3. **Historical score:** 5/10.

**Source:** `styles/components.rcss; screens/game_hud.rml; screens/inspector.rml`.

**Observed in source:** Title marks include bars or letters, and multiple controls use literal v and ^ characters. Actual sprite raster quality was not visually inspected.

**Keep:** Text labels prevent important commands from being icon-only.

**Fix:** Create a small consistent icon set for close, drop-down, spinner arrows, sorting, expand/collapse, warnings and object kinds. Keep game-item sprites separate from chrome icons.

**Acceptance:** Glyphs align consistently and remain recognizable at supported scales. Every actionable icon has an explicit name or nearby label.

<a id="audit-sys-06"></a>
### SYS-06 — Typography

**Primary delivery stage:** [03](#stage-03). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/base.rcss; windows/management6b.rcss; screens/shell.rcss`.

**Observed in source:** The shared family is LatoLatin. Body text is 13dp, but inventory filter/header text reaches 10dp and setup scale labels 9dp.

**Keep:** A pinned licensed font makes output deterministic.

**Fix:** First normalize text hierarchy and raise essential small text. A more period-like licensed font is optional, not the first fix; changing the font also requires updating metric and asset-verifier expectations.

**Acceptance:** Names, numeric values, filter labels and errors remain legible at default scale. No clipping at 100%, 125%, 150% and 200%, including long test strings.

<a id="audit-sys-07"></a>
### SYS-07 — Ordinary push buttons

**Primary delivery stage:** [04](#stage-04). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `styles/base.rcss`.

**Observed in source:** The shared button has an appropriate raised bevel, but default hover turns yellow and pressed state orange.

**Keep:** Raised and pressed edges already communicate activation.

**Fix:** Keep command buttons neutral. Reverse the bevel and make a small content offset on press; do not make every hovered command a colored primary action. Preserve dimensions between states.

**Acceptance:** Normal, hover, pressed, focused and disabled versions are distinguishable without layout movement or a change in semantic color meaning.

<a id="audit-sys-08"></a>
### SYS-08 — Default/primary button

**Primary delivery stage:** [04](#stage-04). **Original priority:** P2. **Historical score:** 5/10.

**Source:** `styles/components.rcss; modals/confirm_destructive.rml`.

**Observed in source:** Primary buttons are yellow. The generic destructive confirmation assigns the primary class to Cancel.

**Keep:** The safe action receives visual emphasis in the generic confirmation.

**Fix:** Separate default keyboard action from marketing-style primary color. Use a classic outer default outline while retaining a separate inner focus cue. Set default behavior deliberately per dialog.

**Acceptance:** The visible default matches what Enter actually activates. Focus can move to a different control without falsely transferring the default outline.

<a id="audit-sys-09"></a>
### SYS-09 — Destructive buttons

**Primary delivery stage:** [04](#stage-04). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/components.rcss; windows/military_manager.rml; windows/workshop_manager.rml`.

**Observed in source:** Destructive commands use an orange/salmon skin; some labels describe the object well, while a generic Confirm fallback also exists.

**Keep:** Destructive actions and several confirmations are explicitly represented.

**Fix:** Prefer neutral command chrome plus a precise action verb and consequence. Use warning color sparingly. Preserve existing confirmations and add recovery where supported, rather than making every action modal.

**Acceptance:** Delete squad, overwrite template, discard progress and complete trade each name the affected object and result. Harmless actions do not acquire unnecessary warnings.

<a id="audit-sys-10"></a>
### SYS-10 — Keyboard focus versus selection

**Primary delivery stage:** [04](#stage-04). **Original priority:** P1. **Historical score:** 4/10.

**Source:** `styles/management_window.rcss; styles/base.rcss`.

**Observed in source:** The management-rail selected selector and :focus selector share the same pressed appearance. Shared focus-visible replaces border color with orange.

**Keep:** Focusable controls and explicit state selectors are present.

**Fix:** Give focus its own dotted or similarly unambiguous inner indicator, implemented with supported RCSS/decorators. Keep it independent from selected, checked, default, hover and disabled states.

**Acceptance:** A keyboard user can distinguish the active page, focused control and default action simultaneously. Moving focus does not falsely indicate a changed setting.

<a id="audit-sys-11"></a>
### SYS-11 — Property tabs

**Primary delivery stage:** [04](#stage-04). **Original priority:** P1. **Historical score:** 7/10.

**Source:** `styles/components.rcss; windows/military_manager.rml; windows/population_manager.rml`.

**Observed in source:** The shared .c-tabs__tab already implements a raised selected tab connected to the page. Several main managers instead render vertical rail buttons.

**Keep:** The existing connected-tab geometry is a better starting point than a new widget.

**Fix:** Use top property tabs for small peer sets such as Squads / Roles & Uniforms / Target Priorities. Link each tab to its panel and use a deliberate keyboard activation model. Do not turn hierarchical catalogs into dozens of tabs.

**Acceptance:** One selected tab joins the page with no bottom seam. Focus is separate. Tab, arrows and Ctrl+Tab follow the chosen contract without firing unrelated commands.

<a id="audit-sys-12"></a>
### SYS-12 — Category navigation rails

**Primary delivery stage:** [04](#stage-04). **Original priority:** P2. **Historical score:** 5/10.

**Source:** `styles/management_window.rcss; screens/management6a.rcss`.

**Observed in source:** Several managers spend a fixed 112dp on a rail of peer-view buttons, sometimes alongside another fixed list pane.

**Keep:** Hierarchical category navigation is appropriate for the build catalog.

**Fix:** Reserve side navigation for categories or hierarchy. Move small peer-view sets to top tabs, and keep object lists in their own clearly labeled panes. Maintain a compact fallback when space genuinely runs out.

**Acceptance:** Military and population no longer require rail + object list + editor at ordinary size. Build categories remain understandable and do not become an overlong tab strip.

<a id="audit-sys-13"></a>
### SYS-13 — Group boxes and content panels

**Primary delivery stage:** [03](#stage-03). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/components.rcss; screens/shell.rcss; windows/management6c.rcss`.

**Observed in source:** The code uses nested raised sections, inset cards and colored section heading bars in different screens.

**Keep:** Related fields are already grouped in markup.

**Fix:** Use thin group-box borders with a small legend for related fields. Reserve sunken borders for editable/list content and full raised frames for windows or genuine toolbars.

**Acceptance:** A user can identify each window, group and data region from border treatment alone; a simple form does not look like several nested windows.

<a id="audit-sys-14"></a>
### SYS-14 — Text inputs and labels

**Primary delivery stage:** [05](#stage-05). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `styles/base.rcss; windows/stockpile_manager.rml; screens/new_game.rml`.

**Observed in source:** Named fields often have visible labels, but some search and setup fields rely on placeholders or visual span labels.

**Keep:** Inset editable surfaces are established.

**Fix:** Standardize labels, editable white fields, read-only treatment and inline validation. Associate labels programmatically through the actual RmlUi/application input model. Do not use a placeholder as the only persistent label.

**Acceptance:** Users can recover the field purpose after typing. Invalid input is explained in text, and read-only values do not falsely invite editing.

<a id="audit-sys-15"></a>
### SYS-15 — Checkboxes and persistent Boolean options

**Primary delivery stage:** [05](#stage-05). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `styles/base.rcss; windows/stockpile_manager.rml; panels/agriculture_manager.rml`.

**Observed in source:** Native checkbox inputs coexist with large check-buttons and text glyphs. The shared checked checkbox style uses a filled colored square/border.

**Keep:** Stockpile hauling options already use labeled checkbox inputs.

**Fix:** Adopt one checkbox model with an unmistakable check mark and full clickable label. Add mixed state only where the underlying data really supports it. Convert persistent Boolean button substitutes where appropriate.

**Acceptance:** Checked, unchecked, mixed and disabled are distinguishable in grayscale. Space toggles the focused checkbox and the label activates the same state exactly once.

<a id="audit-sys-16"></a>
### SYS-16 — Exclusive choices

**Primary delivery stage:** [05](#stage-05). **Original priority:** P1. **Historical score:** 5/10.

**Source:** `windows/military_manager.rml; windows/diplomacy_missions.rml; windows/population_manager.rml`.

**Observed in source:** Attitudes, mission types and schedule activities are expressed as groups of ordinary or check-style buttons.

**Keep:** All choices are visible rather than hidden behind unexplained cycling.

**Fix:** Use radio groups, a proper single-select combo, or a correctly marked exclusive toolbar group. Clearly separate choosing a value from executing the action that uses it.

**Acceptance:** Exactly one supported value is selected. Merely changing focus cannot issue a command. Screen state and authoritative selection remain synchronized.

<a id="audit-sys-17"></a>
### SYS-17 — Combos and drop-down lists

**Primary delivery stage:** [05](#stage-05). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `styles/base.rcss; windows/management6c.rcss; windows/inventory_browser.rml`.

**Observed in source:** The shared select subparts retain dark styling while military/diplomacy define their own light subparts; inventory implements custom input-plus-button popups.

**Keep:** The custom filters have explicit labels, expanded state and listbox containers.

**Fix:** Centralize editable and noneditable combo variants, arrow glyph, popup selection, disabled state and open/close behavior. Preserve keyboard filtering and legal catalog-backed values.

**Acceptance:** Open by mouse and keyboard; choose, cancel, click away and reopen. Focus returns sensibly, selection persists, and popups remain within the relevant host bounds.

<a id="audit-sys-18"></a>
### SYS-18 — Numeric fields and spinners

**Primary delivery stage:** [05](#stage-05). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/components.rcss; windows/stockpile_manager.rml; windows/workshop_manager.rml`.

**Observed in source:** There are different stepper arrangements; some priority fields use text inputs and literal arrow characters. A tooltip explains that 1 is highest.

**Keep:** Quantity and priority inputs expose bounds in several templates.

**Fix:** Use a shared numeric edit with integrated up/down arrows, visible units, bounds and meaning. Explain priority direction beside the field rather than only on hover.

**Acceptance:** Typing, stepping, paste, bounds, empty input and invalid characters produce the same authoritative value. Raise priority decreases the number where 1 is highest.

<a id="audit-sys-19"></a>
### SYS-19 — Sliders

**Primary delivery stage:** [05](#stage-05). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/components.rcss; screens/new_game.rml; screens/settings.rml`.

**Observed in source:** Setup and settings expose many exact integer quantities primarily through sliders. Their readouts are separate text, not editable numeric fields.

**Keep:** Readouts and ranges make values discoverable.

**Fix:** Retain sliders for approximate adjustments such as volume. Add synchronized numeric editing for map size, depth, population, frame rate and other exact quantities; use a rectangular classic thumb if pursuing fidelity.

**Acceptance:** A user can enter an exact value without repeated dragging. Keyboard increments, displayed units and field validation agree.

<a id="audit-sys-20"></a>
### SYS-20 — Report tables and list views

**Primary delivery stage:** [06](#stage-06). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/inventory_browser.rml; windows/management6b.rcss; styles/components.rcss`.

**Observed in source:** Inventory exposes category, type, item, material and numeric columns. Shared styles include numeric alignment support.

**Keep:** Comparable facts are presented as dense rows, not separate cards.

**Fix:** Standardize neutral header buttons, sort indicators, numeric alignment, row selection and truncation. Preserve the existing flat inventory model unless a different hierarchy is actually requested.

**Acceptance:** Headers align with every row at all tested widths. Sort order, focused row and selected row remain identifiable after filtering and refresh.

<a id="audit-sys-21"></a>
### SYS-21 — Column filters

**Primary delivery stage:** [06](#stage-06). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `windows/inventory_browser.rml; windows/stockpile_manager.rml`.

**Observed in source:** Inventory and stockpile duplicate six-column filter/sort markup and custom popup controls.

**Keep:** Column-specific filtering is a strong management-game capability.

**Fix:** Extract a shared header/filter component. Give active filters a visible indicator, provide one clear-all action and report the current match count where not already supplied dynamically.

**Acceptance:** Identical filters behave identically across inventory and stockpile. Empty results, reset and back navigation preserve predictable state.

<a id="audit-sys-22"></a>
### SYS-22 — Selected, hovered and inactive rows

**Primary delivery stage:** [06](#stage-06). **Original priority:** P1. **Historical score:** 5/10.

**Source:** `styles/components.rcss; windows/management6b.rcss`.

**Observed in source:** Shared list selection is yellow, generic button selection pale blue, and inventory selection navy with white text.

**Keep:** Inventory already demonstrates a clear classic selected-row treatment.

**Fix:** Define one selection grammar, including inactive-window selection and a distinct focus cue. Reserve severity colors for semantic state, not arbitrary selection.

**Acceptance:** Selecting the same kind of object in different managers does not change the meaning of the highlight. Selected error/status rows retain readable text.

<a id="audit-sys-23"></a>
### SYS-23 — Scrollbars

**Primary delivery stage:** [06](#stage-06). **Original priority:** P1. **Historical score:** 5/10.

**Source:** `styles/components.rcss; windows/management6c.rcss; windows/management6b.rcss`.

**Observed in source:** Some shared and military/diplomacy rules target scrollbarvertical track/slider. RmlUi documents slidertrack/sliderbar. Other inventory/population rules already use the documented child tags.

**Keep:** Explicit scrollbar sizing is present, and some routes have the correct implementation.

**Fix:** Consolidate on scrollbarvertical/horizontal plus slidertrack, sliderbar, sliderarrowdec and sliderarrowinc. Remove rules that cannot match the standard generated children. Verify the pinned engine rather than assuming browser defaults.

**Acceptance:** Every primary scroll region shows and operates its thumb, track and arrows. Test both axes, tiny ranges and the scrollbar corner. Do not infer that every current scrollbar is broken.

<a id="audit-sys-24"></a>
### SYS-24 — Trees and matrices

**Primary delivery stage:** [06](#stage-06). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/components.rcss; windows/population_manager.rml`.

**Observed in source:** A shared tree component exists; production inventory has deliberately moved to a flat catalog. The duty board is a grid.

**Keep:** Different data structures can use different controls.

**Fix:** Do not reintroduce a tree into inventory merely for nostalgia. For real trees, separate expansion from selection; for matrices, provide cell focus, row/column identity and controlled scrolling.

**Acceptance:** Tree expansion does not activate the node. Matrix navigation reaches each cell without changing values until an explicit edit action.

<a id="audit-sys-25"></a>
### SYS-25 — Tooltips

**Primary delivery stage:** [07](#stage-07). **Original priority:** P2. **Historical score:** 5/10.

**Source:** `styles/components.rcss; windows/military_manager.rml`.

**Observed in source:** The shared tooltip retains dark rounded shadowed styling. Managers supply tooltip containers and many explanatory titles.

**Keep:** Help text already exists for many nonobvious actions.

**Fix:** Use one compact, consistently positioned tooltip treatment, optionally pale info-yellow for the classic theme. Show equivalent help on focus. Keep essential consequences visible in the form itself.

**Acceptance:** No tooltip blocks its target, escapes its host, survives closure or covers the focused option. Critical instructions are not hover-only.

<a id="audit-sys-26"></a>
### SYS-26 — Progress and loading indicators

**Primary delivery stage:** [07](#stage-07). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `styles/components.rcss; screens/loading.rml`.

**Observed in source:** The loading template uses stage text and three indicator spans; the shared progress primitive still has dark/bronze styling.

**Keep:** Loading, error and retry states are distinct.

**Fix:** Use a classic inset progress bar only when meaningful progress is available. Otherwise show a truthful indeterminate indicator plus a named stage. Offer cancel only where cancellation is actually safe.

**Acceptance:** No invented percentage or completion animation is shown. Slow, failed and retried transitions produce understandable states.

<a id="audit-sys-27"></a>
### SYS-27 — Status bars and badges

**Primary delivery stage:** [07](#stage-07). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `styles/components.rcss; windows/population_manager.rml; screens/game_hud.rml`.

**Observed in source:** Status chips, per-manager footers and a hidden HUD hint strip coexist. Some status elements expose role=status.

**Keep:** The UI has dedicated places to surface feedback.

**Fix:** Use a quiet segmented status strip for counts, filters and pending results. Reserve strong color and modal interruption for actionable severity. Keep active-tool/cancel information available while a tool is armed.

**Acceptance:** Command pending, success and failure are visible without obscuring work. Status never overwrites a critical error before the user can recover.

<a id="audit-sys-28"></a>
### SYS-28 — Modal dialog contract

**Primary delivery stage:** [07](#stage-07). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `modals/confirm_destructive.rml; windows/military_manager.rml; windows/population_manager.rml`.

**Observed in source:** Confirmation templates differ in structure and ordering; population includes explicit dialog metadata that is absent from some other static confirmation templates.

**Keep:** Destructive operations already have dedicated confirmation surfaces.

**Fix:** Share a single modal shell with object-specific copy, safe default, consistent command order and appropriate semantics. Audit focus containment, background input blocking and restoration in the actual host code.

**Acceptance:** Tab cannot escape; Escape cancels; Enter respects the deliberate default; holding a key cannot accept twice. Closing returns focus to the invoking control. ARIA text alone is not proof of platform accessibility.

<a id="audit-sys-29"></a>
### SYS-29 — Empty, unavailable and error states

**Primary delivery stage:** [07](#stage-07). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/diplomacy_missions.rml; screens/load_game.rml; windows/military_manager.rml`.

**Observed in source:** The templates distinguish no records, undiscovered neighbors, no compatible save, loading and error/retry.

**Keep:** These are useful semantic distinctions and should survive a reskin.

**Fix:** Standardize compact state presentation and next steps. Keep missing data different from zero. Include a retry only where a backing operation exists.

**Acceptance:** An empty filter does not resemble a loading failure, an undiscovered neighbor does not show fabricated zero statistics, and disabled actions explain the prerequisite.

<a id="audit-sys-30"></a>
### SYS-30 — Narrow windows and scaling

**Primary delivery stage:** [20](#stage-20). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `styles/management_window.rcss; windows/management6b.rcss; windows/management6c.rcss`.

**Observed in source:** The stylesheet has media fallbacks. Inventory also has a 580dp minimum flat-table width and a horizontally hidden list region. Host minimum sizes and controller behavior were not executed.

**Keep:** Responsive handling and minimum readable panes are explicitly considered.

**Fix:** Verify the RmlUi context size of each detached host. Either enforce a valid minimum, enable synchronized horizontal scrolling, or adapt columns deliberately. Do not silently clip essential controls.

**Acceptance:** At every supported host size and 100/125/150/200% scale, all fields, commands and columns remain reachable. Classify actual failures only after reproducing them.

<a id="audit-sys-31"></a>
### SYS-31 — Accessibility and input verification

**Primary delivery stage:** [20](#stage-20). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `styles/accessibility.rcss; screens/settings.rml`.

**Observed in source:** High-contrast and reduced-motion classes exist; the reviewed settings template does not expose these preferences. Some templates have detailed roles/labels while others do not.

**Keep:** Explicit semantic states and scaling are good foundations.

**Fix:** Trace how supported preferences are applied, then test every route under them. Complete consistent keyboard semantics and verify whether a platform accessibility bridge exists before making assistive-technology claims.

**Acceptance:** All actions are keyboard reachable, focused content remains visible, and high contrast survives route-specific selectors. Report unsupported capabilities honestly.

<a id="audit-sys-32"></a>
### SYS-32 — Design-system verifier

**Primary delivery stage:** [01](#stage-01). **Original priority:** P1. **Historical score:** 3/10.

**Source:** `tests/ui-design-system/verify-design-system.cmake; styles/base.rcss`.

**Observed in source:** The verifier checks legacy palette strings, which can occur in comments, and rejects @media even though current production styles contain @media.

**Keep:** A verifier and component-fixture inventory already exist.

**Fix:** Replace historical string-presence checks with current token/structure checks and renderer-backed state tests. Align feature checks with the pinned RmlUi version. Retain genuine license and asset-integrity checks.

**Acceptance:** The verifier can run against the current intended design without a contradiction, fails real style drift and cannot pass a required visual state solely because a token appears in a comment.

<a id="audit-hud-01"></a>
### HUD-01 — Top rail: level, time, speed and summary

**Primary delivery stage:** [17](#stage-17). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/game_hud.rml`.

**Observed in source:** Level controls, pause/normal/fast, settlement name, clock/date and summary counts share the HUD rail.

**Keep:** Persistent simulation controls are easy to locate.

**Fix:** Group simulation speed separately from world facts, show an unambiguous paused state, and keep count labels readable when the rail contracts.

**Acceptance:** Only the selected speed appears active; queued pause acknowledgment is distinguishable from actual pause. Narrow layouts do not remove essential controls.

<a id="audit-hud-02"></a>
### HUD-02 — Command shelf versus overlays

**Primary delivery stage:** [17](#stage-17). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `screens/game_hud.rml`.

**Observed in source:** The shelf contains both commands and persistent overlay toggles; some labels repeat, such as Jobs and Designations.

**Keep:** Management and map tools are categorized.

**Fix:** Give persistent overlays checked/toggled presentation and command launchers ordinary command presentation. Clarify duplicate labels using section context or names such as Show jobs.

**Acceptance:** A new user can tell whether a click opens a manager, arms a tool or toggles an overlay before clicking.

<a id="audit-hud-03"></a>
### HUD-03 — Active-tool and cancellation feedback

**Primary delivery stage:** [17](#stage-17). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `screens/game_hud.rml; screens/orders_tools.rml`.

**Observed in source:** The main HUD hint strip is initially hidden, while the detached tools template exposes Cancel and Rotate. Controllers may project additional state.

**Keep:** Explicit cancellation and rotation actions exist.

**Fix:** Guarantee one persistent armed-tool indicator with contextual cancellation and rotation help across both tool hosts. Keep tools from remaining silently armed after a window closes.

**Acceptance:** Arm every placement/mining tool, switch windows and cancel by all supported paths. The world never receives a stale or accidental action from closing UI.

<a id="audit-hud-04"></a>
### HUD-04 — Build categories and catalog

**Primary delivery stage:** [17](#stage-17). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/game_hud.rml; screens/orders_tools.rml`.

**Observed in source:** Build uses a category rail, a type subpage and a catalog area.

**Keep:** A category navigator suits hierarchical building choices.

**Fix:** Retain category navigation, preserve selected category on return, and show selected product/material requirements adjacent to placement. Ensure category and type levels are not confused with property tabs.

**Acceptance:** A user can select a buildable, inspect its requirements, go back and resume without losing the intended item or placement orientation.

<a id="audit-hud-05"></a>
### HUD-05 — World labels and selection pointer

**Primary delivery stage:** [17](#stage-17). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** The inspector defines a world label and a pointer tip for tool/selection size.

**Keep:** Selection feedback is spatially connected to the map.

**Fix:** Keep labels small and readable without covering the target. Include invalid placement explanations in text and do not rely exclusively on a red/green tint.

**Acceptance:** Labels remain within host bounds and clear when the operation ends. Invalid areas cannot be mistaken for valid placement in grayscale.

<a id="audit-hud-06"></a>
### HUD-06 — Tutorial panel

**Primary delivery stage:** [17](#stage-17). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/game_hud.rml`.

**Observed in source:** Tutorial UI has objective, instructions, checklist, warning and Continue/Skip/Restart/Hints/Continue anyway actions.

**Keep:** Actionable objectives and progress already exist.

**Fix:** Make the next step the dominant action. Put skip/restart in a secondary area and distinguish Continue anyway from verified completion. Permit non-obstructive placement/minimization only with retained progress.

**Acceptance:** The tutorial does not cover the required target or claim a skipped objective was completed. Keyboard focus remains on the tutorial when interacting with it, not on the world beneath.

<a id="audit-hud-07"></a>
### HUD-07 — Event dialogs

**Primary delivery stage:** [17](#stage-17). **Original priority:** P1. **Historical score:** 7/10.

**Source:** `screens/game_hud.rml`.

**Observed in source:** The event blocker has dialog semantics and Continue/Yes/No controls.

**Keep:** Modal events are separated from passive HUD information.

**Fix:** Project only the relevant command set, use action-specific labels where practical, and apply the common safe-default/focus contract.

**Acceptance:** Invisible alternatives are not focusable. Repeated Enter/Escape cannot accept a second event or act on the world after the first closes.

<a id="audit-inv-01"></a>
### INV-01 — Six-column report header

**Primary delivery stage:** [08](#stage-08). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/inventory_browser.rml; windows/management6b.rcss`.

**Observed in source:** The report exposes six meaningful columns with sort buttons and direction spans. Its route styles implement neutral gray beveled headers.

**Keep:** This is one of the strongest Win98-oriented components.

**Fix:** Reuse this header grammar across comparable managers. Keep numerical columns right-aligned and their units/definitions consistent.

**Acceptance:** Category, type, item, material, stock and total align and sort correctly without inconsistent header visuals.

<a id="audit-inv-02"></a>
### INV-02 — Header filter inputs and popups

**Primary delivery stage:** [08](#stage-08). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `windows/inventory_browser.rml; windows/management6b.rcss`.

**Observed in source:** Each header includes a custom filter combo; numeric filters are readonly. The route specifies 10dp filter text.

**Keep:** Specific filters support a large resource catalog.

**Fix:** Make read-only filters look like drop-down choices, standardize arrows and enlarge essential text. Add a clear-all filter command and match summary if not already projected by the controller.

**Acceptance:** All six filters are usable by keyboard and mouse; popups are neither clipped nor obscured; active filters remain obvious after detail navigation.

<a id="audit-inv-03"></a>
### INV-03 — Inventory row density and narrow width

**Primary delivery stage:** [08](#stage-08). **Original priority:** P1. **Historical score:** 6.5/10.

**Source:** `windows/management6b.rcss`.

**Observed in source:** Flat rows are at least 48dp tall with 40dp sprites, while header text is 10dp. The flat table has a 580dp minimum width and horizontal hiding in the list style.

**Keep:** Sprites make items recognizable, and explicit virtual-spacer styling exists.

**Fix:** Offer a compact report density with smaller sprites and coherent text sizes. Verify clipping before changing the host contract; use synchronized horizontal scrolling or a documented minimum width.

**Acceptance:** Large inventories remain navigable, chosen rows stay visible, and no column becomes permanently inaccessible at supported UI scales.

<a id="audit-inv-04"></a>
### INV-04 — Item detail and Back to inventory

**Primary delivery stage:** [08](#stage-08). **Original priority:** P2. **Historical score:** 7.5/10.

**Source:** `windows/inventory_browser.rml`.

**Observed in source:** The item page groups recipes, outputs, stockpile locations and history in four regions.

**Keep:** The relationships answer practical production questions.

**Fix:** Retain the content, but present it as compact grouped lists or property pages. Preserve source filters, sort, row selection and scroll when returning; keep the item identity visible.

**Acceptance:** Open a deeply scrolled filtered item, follow a supported location link and return. The prior inventory context is restored.

<a id="audit-sto-01"></a>
### STO-01 — Stock / Allow list / Settings navigation

**Primary delivery stage:** [09](#stage-09). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** Three peer views use a vertical rail and Center on map sits beside them.

**Keep:** Stock and acceptance rules are correctly separated.

**Fix:** Use three connected top tabs and place Center on map in a small object toolbar. Keep the stockpile name and suspended state persistently visible.

**Acceptance:** Switching pages preserves filters and drafts, and locating the stockpile does not look like another property page.

<a id="audit-sto-02"></a>
### STO-02 — Physical stock table

**Primary delivery stage:** [09](#stage-09). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** The stock page explicitly describes items physically stored here and exposes a six-column report.

**Keep:** Actual contents are not conflated with the allow list.

**Fix:** Keep the shared report pattern. Explain the meaning of Total relative to this stockpile and other locations, using actual backing data rather than assumed semantics.

**Acceptance:** The displayed quantities can be reconciled with the authoritative stockpile snapshot. Empty and filtered-empty states are different.

<a id="audit-sto-03"></a>
### STO-03 — Allow list and bulk matching actions

**Primary delivery stage:** [09](#stage-09). **Original priority:** P1. **Historical score:** 7/10.

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** Allow matches and Block matches operate from the rule-search page.

**Keep:** Bulk actions are explicitly scoped to matches in their labels.

**Fix:** Display the number and scope of affected rules beside the bulk commands. Preserve a mixed state only where backing rules support it. Provide revert/undo or proportional confirmation for broad changes.

**Acceptance:** Filtering to a subset then applying either action changes exactly that intended subset, including any offscreen matches described by the UI.

<a id="audit-sto-04"></a>
### STO-04 — Saved allow-list templates

**Primary delivery stage:** [09](#stage-09). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** An editable template combo, Save new button and named overwrite confirmation are present.

**Keep:** The overwrite dialog names the template being replaced.

**Fix:** Make Save new versus Update existing explicit, preserve unsaved text, and share combo/dialog styling. Warn about an overwrite rather than silently replacing rules.

**Acceptance:** New, duplicate-name, overwritten, empty-name and canceled template operations leave the correct saved and current rules intact.

<a id="audit-sto-05"></a>
### STO-05 — Name, priority and Apply

**Primary delivery stage:** [09](#stage-09). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** Name and numeric priority are staged behind Apply; arrows expose raise/lower semantics and a 1-is-highest tooltip.

**Keep:** A separate Apply command makes the edit boundary visible.

**Fix:** Show priority direction inline and add an explicit dirty/revert strategy. Use the shared spinner and align labels/control widths.

**Acceptance:** Changing tabs or closing with an unapplied name/priority produces the documented result without silently discarding a draft.

<a id="audit-sto-06"></a>
### STO-06 — Hauling checkboxes

**Primary delivery stage:** [09](#stage-09). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/stockpile_manager.rml`.

**Observed in source:** The two hauling rules use labeled checkbox inputs describing transfer direction.

**Keep:** These are already a good fit for Boolean settings.

**Fix:** Keep the wording and semantics; adopt the final shared check mark, focus and disabled styles. State whether these changes apply immediately.

**Acceptance:** Each checkbox changes only the intended rule and remains clear when one or both are disabled or checked.

<a id="audit-sto-07"></a>
### STO-07 — Suspend/resume state

**Primary delivery stage:** [09](#stage-09). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `windows/stockpile_manager.rml; screens/inspector.rml`.

**Observed in source:** Suspension is exposed both in the manager and an inspector summary.

**Keep:** The command is available near the object being managed.

**Fix:** Use a persistent Active/Suspended value plus a clearly labeled Suspend or Resume action. Keep every surface synchronized with the same authoritative state.

**Acceptance:** Toggle from either surface while the other is open. Both update consistently, including command rejection and pending state.

<a id="audit-wrk-01"></a>
### WRK-01 — Craft / Queue / Settings / Trade navigation

**Primary delivery stage:** [10](#stage-10). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** Workshop views are vertical rail buttons; Trade is conditionally hidden.

**Keep:** Craft creation and existing queue editing are separate tasks.

**Fix:** Use connected tabs for supported peer views, retain conditional availability and keep workshop identity/Center on map separate from navigation.

**Acceptance:** Unsupported trade pages do not leave empty tabs or keyboard stops; supported pages retain selection and pending edits.

<a id="audit-wrk-02"></a>
### WRK-02 — Available crafts and new order

**Primary delivery stage:** [10](#stage-10). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** A craft list, search, quantity field, help, Add order and feedback form a list-detail flow.

**Keep:** This is a useful select-then-configure-then-act structure.

**Fix:** Keep recipe/material requirements next to the quantity and Add order button; use one numeric editor and show any blocked reason at the action.

**Acceptance:** A valid click creates one order. Missing inputs disable or reject it with a specific explanation, not a silent no-op.

<a id="audit-wrk-03"></a>
### WRK-03 — Production queue and order editor

**Primary delivery stage:** [10](#stage-10). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** The queue explains top-to-bottom execution and has a selected-job quantity editor with Apply.

**Keep:** Order sequence has a stated meaning.

**Fix:** Expose clear reorder, suspend/resume, repetition and removal controls only for supported operations. Keep selection stable after moving/removing an order.

**Acceptance:** The visible order matches authoritative execution order. Editing a selected job cannot accidentally affect a newly selected or removed job.

<a id="audit-wrk-04"></a>
### WRK-04 — Workshop settings edit model

**Primary delivery stage:** [10](#stage-10). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** Name/priority use Apply, while generated-order and auto-craft options are checkbox inputs and suspension is a button.

**Keep:** Settings are grouped by purpose.

**Fix:** Choose one staged form or explicitly distinguish instant options from draft fields. Add a revert/close-dirty contract and remove uncertainty about which changes have committed.

**Acceptance:** A user can predict the result of Close, Apply, changing tabs and reopening after both successful and rejected updates.

<a id="audit-wrk-05"></a>
### WRK-05 — Linked stockpiles

**Primary delivery stage:** [10](#stage-10). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** A stockpile select, Link command, existing-links region and collection-order explanation are present.

**Keep:** The help describes a useful gameplay relationship.

**Fix:** Use a compact list with names, locate/unlink commands where supported and a clearly labeled selection combo. Preserve the explanation of collection precedence.

**Acceptance:** Linking an already linked or deleted stockpile is handled safely; names and status stay current without losing keyboard focus.

<a id="audit-wrk-06"></a>
### WRK-06 — Special production options

**Primary delivery stage:** [10](#stage-10). **Original priority:** P2. **Historical score:** 6.5/10.

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** Butcher and fisher options use button-shaped persistent settings in conditional sections.

**Keep:** Unsupported workshop-specific choices are initially hidden.

**Fix:** Render persistent options as consistent checkboxes, with clear state and any consequential behavior explained. Do not expose unsupported options for visual symmetry.

**Acceptance:** Opening different workshop types shows only legal settings and each checked value matches the current workshop.

<a id="audit-wrk-07"></a>
### WRK-07 — Trade ledger and confirmation

**Primary delivery stage:** [10](#stage-10). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `windows/workshop_manager.rml`.

**Observed in source:** Trading exposes Next trade row, Offer one less/more, Review trade and an explicit irreversible-trade confirmation.

**Keep:** Irreversibility is already called out.

**Fix:** Replace opaque sequential row cycling as the primary workflow with directly selectable rows and quantity editing. Present both sides and net value before commit; retain keyboard shortcuts as accelerators.

**Acceptance:** A user can inspect the exact exchange and cancel without effect. Confirm commits once, handles stale offers and refreshes authoritative totals.

<a id="audit-agr-01"></a>
### AGR-01 — Overview and statistics

**Primary delivery stage:** [11](#stage-11). **Original priority:** P2. **Historical score:** 7.5/10.

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Farm overview exposes plots, tilled, planted and ready-to-harvest counts with a product summary.

**Keep:** These counts describe production status directly.

**Fix:** Keep them in a compact summary strip rather than oversized dashboard cards. Use consistent units and explain relationships only as supported by the underlying data.

**Acceptance:** Values remain readable, do not falsely imply additive categories and agree with the authoritative designation snapshot.

<a id="audit-agr-02"></a>
### AGR-02 — Farm plot grid

**Primary delivery stage:** [11](#stage-11). **Original priority:** P1. **Historical score:** 7/10.

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** The plot grid has Select all/Clear and a legend describing gray, brown, green and gold states.

**Keep:** Spatial editing connects a plan to actual plots.

**Fix:** Add a non-color state cue and distinct selected/focused plot treatment. Provide keyboard selection and accessible plot names in the implemented input system.

**Acceptance:** A user can identify empty, tilled, planted and ready plots without color alone, and can select precisely one, several or all plots.

<a id="audit-agr-03"></a>
### AGR-03 — Crop catalog and availability

**Primary delivery stage:** [11](#stage-11). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Search, crop catalog, chosen-crop text and counts are adjacent to plot assignment.

**Keep:** Selection precedes the assign action.

**Fix:** Keep the selected crop and availability visible while scrolling. Explain disabled or unavailable choices without substituting invented values.

**Acceptance:** The selected crop does not change silently after refresh; an unavailable crop has a specific backed reason.

<a id="audit-agr-04"></a>
### AGR-04 — Assign crop / Use farm default / Set farm default

**Primary delivery stage:** [11](#stage-11). **Original priority:** P1. **Historical score:** 6.5/10.

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Three nearby commands affect related but different scopes.

**Keep:** The default-setting button already has a useful tooltip.

**Fix:** Separate Selected plots from Farm default in two named groups. State the affected plot count and explain the fallback relationship inline.

**Acceptance:** Users can predict whether an action changes selected plots, removes plot overrides or changes the farm-wide fallback.

<a id="audit-agr-05"></a>
### AGR-05 — Planting counts and repeat queue

**Primary delivery stage:** [11](#stage-11). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** The form explains that counts mean plantings on each selected plot and repeat orders move behind later orders.

**Keep:** The unusual queue semantics are documented.

**Fix:** Show a review sentence such as selected-plot count and plantings per plot, then a readable queue with explicit repeat state and supported reorder/remove actions. Keep the existing repeat semantics.

**Acceptance:** The UI never presents a per-plot count as a global total; queue mutation preserves later orders and the documented fallback.

<a id="audit-agr-06"></a>
### AGR-06 — Farm and grove work toggles

**Primary delivery stage:** [11](#stage-11). **Original priority:** P2. **Historical score:** 6.5/10.

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Harvest, pick fruit, plant trees and fell trees use large check-buttons and checkbox glyphs.

**Keep:** The actions are split by designation type.

**Fix:** Use true persistent checkbox styling and consistent wording. Keep destructive or long-lived implications in the group help rather than relying on orange color.

**Acceptance:** Toggling a work rule changes only that rule, and the state remains clear after switching designation types.

<a id="audit-agr-07"></a>
### AGR-07 — Pasture animals, caps and food rules

**Primary delivery stage:** [11](#stage-11). **Original priority:** P1. **Historical score:** 5/10.

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** The pasture template exposes Next animal, four separate cap increment/decrement buttons and Toggle first food rule.

**Keep:** There is already an animal list and explicit cap controls.

**Fix:** Make the target directly selectable, replace cap button clusters with labeled male/female numeric spinners, and provide direct per-food-rule editing. Retain existing actions as underlying commands, not the main user workflow.

**Acceptance:** The affected animal or food rule is always named before mutation. Users can edit any rule without cycling through unrelated records.

<a id="audit-agr-08"></a>
### AGR-08 — Designation settings and locate

**Primary delivery stage:** [11](#stage-11). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `panels/agriculture_manager.rml`.

**Observed in source:** Name/priority use Apply name and priority, suspension is separate, and a Locate command appears in the rail.

**Keep:** The staged action label is explicit.

**Fix:** Use the common object toolbar and staged-versus-instant contract. Make Locate wording match Center on map elsewhere.

**Acceptance:** Name/priority drafts survive navigation as documented, and locate/suspend behavior is consistent with stockpiles and workshops.

<a id="audit-pop-01"></a>
### POP-01 — Citizen roster, search, sort and paging

**Primary delivery stage:** [12](#stage-12). **Original priority:** P2. **Historical score:** 6.5/10.

**Source:** `windows/population_manager.rml`.

**Observed in source:** The template contains two Refresh controls in different containers, separate Name/Profession sort commands, a keyboard hint and row paging.

**Keep:** Search, keyboard guidance and pagination already exist.

**Fix:** Verify whether both refresh controls are visible at once, then retain one consistent refresh location. Use sortable report headers and one paging/status area; preserve existing keyboard behavior.

**Acceptance:** Refresh/filter/page changes preserve or deliberately relocate selection without duplicate commands, unexpected focus loss or hidden rows.

<a id="audit-pop-02"></a>
### POP-02 — Skill comparison and bulk enable/disable

**Primary delivery stage:** [12](#stage-12). **Original priority:** P1. **Historical score:** 6.5/10.

**Source:** `windows/population_manager.rml`.

**Observed in source:** The selected skill can be enabled or disabled for all citizens.

**Keep:** Global and per-person editing are separate surfaces.

**Fix:** Keep scope visible beside the action, show the affected count and provide an appropriate recovery/confirmation strategy for broad changes. Use consistent checkbox state in the citizen list.

**Acceptance:** Only the selected skill changes across the stated population scope; a rejected bulk update cannot leave the presentation falsely successful.

<a id="audit-pop-03"></a>
### POP-03 — Profession editor

**Primary delivery stage:** [12](#stage-12). **Original priority:** P1. **Historical score:** 7/10.

**Source:** `windows/population_manager.rml`.

**Observed in source:** The editor already has Profession skills and Available skills lists, priority movement, Add/Remove, Save and Delete.

**Keep:** The two-list ordering model fits the task well.

**Fix:** Retain the dual lists, position transfer controls next to their targets, and show dirty state with Revert/Cancel semantics. Keep Delete away from routine save/reorder actions.

**Acceptance:** Adding, removing and reordering affect the selected profession; switching profession with unsaved edits follows an explicit contract.

<a id="audit-pop-04"></a>
### POP-04 — 24-hour schedule grid

**Primary delivery stage:** [13](#stage-13). **Original priority:** P1. **Historical score:** 6.5/10.

**Source:** `windows/population_manager.rml; windows/management6b.rcss`.

**Observed in source:** The schedule supports activity selection and explicit Set cell, Set citizen's day, and Set hour for all operations.

**Keep:** Those scope labels are valuable and should not be removed.

**Fix:** Give each scope a visible highlight preview. Use a single-select activity palette, stable row/hour headers, non-color activity symbols and a clear multi-target confirmation/recovery model.

**Acceptance:** Arrow navigation and keyboard editing reach all 24 hours. Applying a row, column or cell changes precisely the highlighted scope, including offscreen data.

<a id="audit-pop-05"></a>
### POP-05 — Citizen detail overlap

**Primary delivery stage:** [16](#stage-16). **Original priority:** P2. **Historical score:** 6.5/10.

**Source:** `windows/population_manager.rml; screens/inspector.rml`.

**Observed in source:** Population contains a long citizen detail section, while the inspector has a newer multi-view creature preview.

**Keep:** Citizen identity, profession, needs, skills and equipment are available.

**Fix:** Choose a canonical detail presentation or share its components. Keep an explicit Back to citizens path with roster-state restoration, without maintaining divergent copies of the same property editor.

**Acceptance:** Changes made in either supported entry point update the same citizen and expose the same available actions and constraints.

<a id="audit-mil-01"></a>
### MIL-01 — Military page navigation

**Primary delivery stage:** [14](#stage-14). **Original priority:** P1. **Historical score:** 5/10.

**Source:** `windows/military_manager.rml`.

**Observed in source:** Three visible peer views use the shared vertical rail, with hidden cross-route diplomacy buttons also present.

**Keep:** Squads, roles/uniforms and priorities are sensible peer topics.

**Fix:** Use a top tab strip. Keep hidden cross-route implementation details out of navigation and the focus order. Preserve current route and controller identifiers where needed.

**Acceptance:** Only supported military pages are reachable; the active page remains obvious during keyboard navigation and narrow-window adaptation.

<a id="audit-mil-02"></a>
### MIL-02 — Squad list, identity and ordering

**Primary delivery stage:** [14](#stage-14). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `windows/military_manager.rml`.

**Observed in source:** Squad controls include Add, Rename, Delete, Earlier squad and Later squad.

**Keep:** Squad identity and ordering are directly editable.

**Fix:** Keep a compact squad list with a selected-name heading. Use Move up/down with an explanation of what order means, and isolate Delete from routine selection and ordering.

**Acceptance:** Reordering cannot rename or delete another squad; boundary actions disable with a clear reason.

<a id="audit-mil-03"></a>
### MIL-03 — Roster, unassigned citizens and transfers

**Primary delivery stage:** [14](#stage-14). **Original priority:** P1. **Historical score:** 5.5/10.

**Source:** `windows/military_manager.rml`.

**Observed in source:** A member/role action editor precedes the two roster columns and offers assign/remove and previous/next-squad transfer commands.

**Keep:** A dual roster/unassigned layout is already present.

**Fix:** Arrange the page as select squad, select citizen, inspect role, then act. Place transfer commands adjacent to the lists and consider an explicit destination selector instead of previous/next as the primary path.

**Acceptance:** Every transfer shows the citizen and source/destination. Moving selection alone cannot transfer membership. Preserve all supported underlying commands.

<a id="audit-mil-04"></a>
### MIL-04 — Roles and civilian behavior

**Primary delivery stage:** [14](#stage-14). **Original priority:** P2. **Historical score:** 6.5/10.

**Source:** `windows/military_manager.rml`.

**Observed in source:** Role name editing and a Civilian button with retreat help are present.

**Keep:** The consequence of civilian behavior is explained.

**Fix:** Use a labeled checkbox for persistent civilian state. Keep role identity and the help adjacent, and make rename/delete behavior match squad and profession editors.

**Acceptance:** Civilian state is visibly on/off rather than appearing as a momentary action; affected citizens update through the authoritative role contract.

<a id="audit-mil-05"></a>
### MIL-05 — Uniform slot/type/material editor

**Primary delivery stage:** [14](#stage-14). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `windows/military_manager.rml`.

**Observed in source:** The interface separates uniform rows from the selected slot's equipment and material choices.

**Keep:** Slot, equipment and material are clearly distinguished.

**Fix:** Use one readable report table and one labeled selected-slot editor. Keep legal choices catalog-backed and show whether rules affect an individual or all users of a role.

**Acceptance:** Changing a slot applies to the named scope only; unavailable types/materials never appear as selectable fabricated options.

<a id="audit-mil-06"></a>
### MIL-06 — Target priorities and attitudes

**Primary delivery stage:** [14](#stage-14). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `windows/military_manager.rml`.

**Observed in source:** Move up/down and Flee/Defend/Attack/Hunt appear together as ordinary buttons above the priorities list.

**Keep:** Both order and response are editable.

**Fix:** Separate ordering commands from a labeled exclusive response choice. Keep the currently selected target and its current response visible.

**Acceptance:** Moving a target does not change its response, and choosing a response cannot be mistaken for an immediate attack command.

<a id="audit-mil-07"></a>
### MIL-07 — Military confirmations and feedback

**Primary delivery stage:** [14](#stage-14). **Original priority:** P1. **Historical score:** 7/10.

**Source:** `windows/military_manager.rml`.

**Observed in source:** Delete commands have ellipses and a separate removal dialog; loading, empty and retry panels exist.

**Keep:** Potentially destructive actions are not presented as silent direct edits.

**Fix:** Adopt the shared modal contract and name the exact squad/role and consequence. Keep rejection/pending status next to the initiating action.

**Acceptance:** Cancel, Enter, Escape, stale selections and removed records all leave the game and selection in a consistent state.

<a id="audit-dip-01"></a>
### DIP-01 — Neighbors / Missions navigation

**Primary delivery stage:** [15](#stage-15). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `windows/diplomacy_missions.rml`.

**Observed in source:** Two peer pages use the same wide rail structure as military.

**Keep:** The distinction between planning and activity is useful.

**Fix:** Use two top tabs and preserve list selection independently in each page.

**Acceptance:** Switching to mission activity and back returns to the intended neighbor and draft without unnecessary reselection.

<a id="audit-dip-02"></a>
### DIP-02 — Neighbor list and properties

**Primary delivery stage:** [15](#stage-15). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/diplomacy_missions.rml`.

**Observed in source:** Distance, attitude, wealth, economy and military are separate labeled values; undiscovered neighbors have a specific explanatory state.

**Keep:** Unknown information is not presented as fabricated detail.

**Fix:** Keep a compact property list and the undiscovered-state explanation. Use consistent units and missing-value presentation.

**Acceptance:** Newly discovered or removed neighbors refresh without losing context or exposing stale editable actions.

<a id="audit-dip-03"></a>
### DIP-03 — New mission builder

**Primary delivery stage:** [15](#stage-15). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `windows/diplomacy_missions.rml`.

**Observed in source:** Mission type, action, eligible citizen list and Start mission are visible in one form, with many similarly styled choice buttons.

**Keep:** The necessary choices are brought together.

**Fix:** Arrange Destination → Mission type → Action → Citizen → Review/Start. Use exclusive choice controls and make unavailable prerequisites visible beside Start. Do not invent cost, risk or ETA data.

**Acceptance:** A valid mission starts once for the reviewed target/citizen. Invalid, unsupported and stale combinations are blocked with a specific reason.

<a id="audit-dip-04"></a>
### DIP-04 — Mission details and results

**Primary delivery stage:** [15](#stage-15). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `windows/diplomacy_missions.rml`.

**Observed in source:** Action, status, destination, participants, time and reported result have distinct fields.

**Keep:** The report structure answers the main status questions.

**Fix:** Keep these fields; add consistent empty/unknown and completed/failed styling with text rather than badges alone.

**Acceptance:** Only authoritative timing and results are shown; a completed or missing mission cannot be confused with a currently running one.

<a id="audit-ins-01"></a>
### INS-01 — Tile inspector and context actions

**Primary delivery stage:** [16](#stage-16). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** The inspector provides coordinates, Center on map, refresh, content/job details and context commands.

**Keep:** This is strongly object-centered.

**Fix:** Keep the selected object identity stable and make the target of every command explicit. Prefer direct list selection over Inspect first creature as the primary multi-creature workflow.

**Acceptance:** Inspecting a crowded tile exposes each supported object without acting on a different first item after a refresh.

<a id="audit-ins-02"></a>
### INS-02 — Construction/blueprint inspector

**Primary delivery stage:** [16](#stage-16). **Original priority:** P2. **Historical score:** 8.5/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** Missing resources, material/status/needed columns, worker, priority, skill/tool and job actions are grouped together.

**Keep:** This directly explains why work is blocked and how to respond.

**Fix:** Preserve this structure as a reference pattern for other managers. Standardize numeric alignment, compact group boxes and placement of the destructive cancel action.

**Acceptance:** The displayed blocker matches the current job; updating resources or priority updates the explanation without fabricated progress.

<a id="audit-ins-03"></a>
### INS-03 — Creature preview navigation and camera

**Primary delivery stage:** [16](#stage-16). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** Camera, Stats, Expertise, Equipment and Inventory have explicit tab/panel relationships and roving tab-index values in markup.

**Keep:** This is among the strongest semantic tab implementations in the reviewed templates.

**Fix:** Preserve the associations and use the shared visual tab/navigation contract. Keep the creature name and relevant live state visible on all pages; do not let the camera dominate property editing.

**Acceptance:** Each view is reachable by the chosen keyboard model; changing views cannot change the selected creature or steal focus into the game.

<a id="audit-ins-04"></a>
### INS-04 — Attributes and needs

**Primary delivery stage:** [16](#stage-16). **Original priority:** P2. **Historical score:** 8/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** Attributes are labeled numeric values; needs have both meters and text values.

**Keep:** Text accompanies visual meters, avoiding color-only interpretation.

**Fix:** Use a compact aligned property layout, explain direction/units where necessary, and ensure missing data is distinct from zero.

**Acceptance:** Meters and displayed numbers agree; unusual/empty values render safely and remain understandable in high contrast.

<a id="audit-ins-05"></a>
### INS-05 — Expertise and profession selector

**Primary delivery stage:** [16](#stage-16). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** A custom profession drop-down and a scrollable skills area are defined, with specific empty states.

**Keep:** The user can change profession close to the affected skills.

**Fix:** Share the combo implementation, align skill name/level/active columns, and clarify which changes are profession-wide versus individual overrides.

**Acceptance:** Choosing or canceling a profession preserves focus correctly; the skill list reflects the authoritative result rather than optimistic stale values.

<a id="audit-ins-06"></a>
### INS-06 — Equipment paper doll and slot editor

**Primary delivery stage:** [16](#stage-16). **Original priority:** P1. **Historical score:** 7/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** The paper doll has clickable equipment slots and an Apply/Cancel editor. It also includes decorative N/O placeholders and a scope text element.

**Keep:** Slot-based editing is appropriate for the game and need not imitate a generic file dialog.

**Fix:** Keep the doll, label each interactive slot, visually distinguish decorative positions, and state the individual-versus-role scope before Apply. Show actual equipment separately from requested rules when backed.

**Acceptance:** Users can select each real slot by keyboard, cannot activate decorative slots, and never unintentionally edit every citizen sharing a role.

<a id="audit-ins-07"></a>
### INS-07 — Carried inventory and empty states

**Primary delivery stage:** [16](#stage-16). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** A separate carried-items view and No carried items state exist.

**Keep:** Worn equipment and carried inventory are not conflated.

**Fix:** Use the shared report/list style with quantities and material details where available; keep the empty state quiet and accurate.

**Acceptance:** Transitions from no items to items and back maintain consistent selection and do not leave stale rows.

<a id="audit-ins-08"></a>
### INS-08 — Stockpile/workshop/agriculture summaries

**Primary delivery stage:** [16](#stage-16). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/inspector.rml`.

**Observed in source:** Object-specific inspector summaries overlap with detailed management windows.

**Keep:** Quick inspection can avoid opening a larger editor.

**Fix:** Keep inspectors read-mostly and compact; link to the full manager for complex edits. Share labels and state projection to prevent conflicting representations.

**Acceptance:** An action from either the inspector or manager updates every open representation of the same object.

<a id="audit-app-01"></a>
### APP-01 — Main menu

**Primary delivery stage:** [18](#stage-18). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/main_menu.rml; screens/shell.rcss`.

**Observed in source:** The menu separates Continue/Load from tutorial/quick/custom start, with a reason when no compatible save is available.

**Keep:** The task hierarchy is clear and game branding is appropriate.

**Fix:** Retain the hierarchy and brand; reduce redundant framing and normalize button states. Exit should not dominate the normal start/continue flow.

**Acceptance:** With no save, compatible save and incompatible save, the correct commands and explanations appear and keyboard order follows the task hierarchy.

<a id="audit-app-02"></a>
### APP-02 — Setup navigation model

**Primary delivery stage:** [18](#stage-18). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `screens/new_game.rml`.

**Observed in source:** The UI labels numbered Setup steps and has a review page, but the footer exposes Start kingdom rather than a standard Back/Next/Finish progression.

**Keep:** Draft retention and review are explicitly described.

**Fix:** Choose a coherent model: a wizard with Back/Next/Finish, or freely selectable property pages without sequential promises. Preserve draft state and the actual supported generation parameters.

**Acceptance:** Users know how to reach the next section and review the final draft. Navigation does not reset values or bypass mandatory validation.

<a id="audit-app-03"></a>
### APP-03 — World-generation fields and layout

**Primary delivery stage:** [18](#stage-18). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `screens/new_game.rml; screens/shell.rcss`.

**Observed in source:** Many exact parameters use sliders inside small bordered cards, with some 9–10dp explanatory text.

**Keep:** Parameter ranges and implications are described.

**Fix:** Use a smaller number of group boxes, aligned numeric editors and concise help. Keep interdependent bounds and validation beside the affected fields.

**Acceptance:** Every supported parameter can be set exactly, invalid combinations identify the affected fields and the final summary matches the submitted draft.

<a id="audit-app-04"></a>
### APP-04 — Kingdom/save browser

**Primary delivery stage:** [18](#stage-18). **Original priority:** P2. **Historical score:** 7.5/10.

**Source:** `screens/load_game.rml`.

**Observed in source:** Kingdoms and saves appear in a two-pane layout with meaningful empty states, refresh and Load selected save.

**Keep:** This is a strong master-detail structure.

**Fix:** Keep it; ensure each save exposes identifying metadata and compatibility/rejection reasons actually available from the backend. Use a clear default Load action and safe selection behavior.

**Acceptance:** Refreshing, changing kingdoms and trying a failed load preserve recoverable state and do not load an unintended save.

<a id="audit-app-05"></a>
### APP-05 — Settings shell and grouping

**Primary delivery stage:** [19](#stage-19). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `screens/settings.rml; screens/shell.rcss`.

**Observed in source:** Settings has a 65dp title area and multiple inset cards with blue heading bars; the page states that changes save automatically.

**Keep:** The immediate-apply model is honestly stated.

**Fix:** Use a compact property-sheet layout with Display, Controls, Audio and Saving pages or restrained group boxes. Keep immediate apply with Close, or deliberately implement true staged Apply/Cancel; do not add a fake Cancel.

**Acceptance:** Every setting follows the declared model. Closing and reopening does not surprise the user about which values were saved.

<a id="audit-app-06"></a>
### APP-06 — Display, controls, audio and saving controls

**Primary delivery stage:** [19](#stage-19). **Original priority:** P2. **Historical score:** 7/10.

**Source:** `screens/settings.rml`.

**Observed in source:** The template includes fullscreen, refresh matching, frame-rate limit, UI scale, minimum light, camera speed, wheel behavior, master volume and autosave controls.

**Keep:** The available preferences are concrete, not ornamental.

**Fix:** Keep enabled/disabled dependencies explicit, add exact numeric editing where useful, show units and make Reset defaults disclose its scope. Verify an escape path from an unusable scale/display choice.

**Acceptance:** All controls persist through the backing settings contract, restore defaults only for the disclosed scope and remain usable at supported scale extremes.

<a id="audit-app-07"></a>
### APP-07 — Pause/save/load/menu transitions

**Primary delivery stage:** [19](#stage-19). **Original priority:** P1. **Historical score:** 7.5/10.

**Source:** `screens/pause_menu.rml`.

**Observed in source:** The pause screen has an authoritative pause-state field, save feedback, errors and explicit resume/save/load/settings/menu commands.

**Keep:** It does not assume requested pause is already authoritative.

**Fix:** Preserve that distinction. Apply a consistent transition confirmation strategy for unsaved progress and ensure no repeated activation triggers multiple saves or loads.

**Acceptance:** Save success/failure, canceled load and return-to-menu paths preserve the intended pause state and cannot click through into the resumed world.

<a id="audit-app-08"></a>
### APP-08 — Loading, error and retry screen

**Primary delivery stage:** [19](#stage-19). **Original priority:** P2. **Historical score:** 6/10.

**Source:** `screens/loading.rml`.

**Observed in source:** Loading uses descriptive text and three marks, with separate Back/Retry when an error occurs.

**Keep:** Failure recovery is explicitly represented.

**Fix:** Adopt the shared truthful-progress control, focus the error when it appears and retain useful diagnostic text without raw internal jargon.

**Acceptance:** A slow load, failed load and successful retry each leave one clear state with working controls and no orphaned blocker.

<a id="audit-app-09"></a>
### APP-09 — Generic destructive confirmation

**Primary delivery stage:** [07](#stage-07). **Original priority:** P1. **Historical score:** 6/10.

**Source:** `modals/confirm_destructive.rml`.

**Observed in source:** The template defaults to Confirm action and Confirm labels, with a warning about discarding unsaved progress.

**Keep:** A safe Cancel action is present and emphasized.

**Fix:** Require action-specific title/body/button text from the caller and use the shared modal focus/default contract. Do not let a generic fallback be the only explanation of an irreversible action.

**Acceptance:** The dialog states exactly what will be discarded or changed and does not accidentally accept on the key release that opened it.


<a id="resume"></a>
## 11. Resume instruction

A user can give the agent this instruction with this file:

> Read this implementation brief and all applicable repository instructions. Execute the Ingnomia Windows 98 UI modernization in order, starting with Stage 00 or the first incomplete stage in the existing checkpoint. Inspect current code before treating a historical audit item as a present defect. Preserve gameplay, action identifiers, command scopes, saves, and the dirty worktree. Implement and test each stage rather than merely rewriting the plan. Continue after a verified stage without routine approval prompts. Keep `docs/ui-win98/` status, audit traceability, command parity, and actual runtime evidence current. Do not call the migration complete while any required runtime gate is blocked.

At each resumed session, read the status document first, then the current stage and its linked audit entries. Recheck HEAD/worktree drift and any changed shared dependencies before relying on old evidence. Continue from the first unmet gate, not from the beginning of the visual redesign.

---

**Preparation note:** This is a staged transformation of the supplied `Ingnomia_Windows_98_UI_Audit.md` and its 98-item triage scope, not a new live repository audit. Proposed documents, stages, and implementation decisions above are instructions to the future coding agent. Historical source observations below/above are not asserted as newly verified facts.
