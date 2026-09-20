**Military & Diplomacy UI review — 14 September 2026**

The five tabs form a sensible set, but the current layouts do **not** meet the requested standard for space use, sizing, or task flow. The largest problem is a shared layout defect: the list and detail panes stack even in a wide window. Important controls then fall below the clipped body while the right side remains empty. There are also two confirmed information regressions from the old GUI—visible citizen role names and explanatory combat/civilian help—and several problems in the new diplomacy presentation.

This is a review. Game source, RML, styles, packaged assets, and player saves were not changed. The new files are this report and its isolated review evidence.

**Evidence and limits.**

- Reviewed the active nested checkout, HEAD `fd1df94912ed36da2d4c8d5a44596e553057c49f`, including its existing uncommitted work.
- Ran the existing `build-wave8-root-msvc-link-priority2/Ingnomia.exe`. SHA-256: `1bf9bb138763d59a353a2f7e25ceea56b3b45e7af7673bba5a4842deacd5b620`. The two RML documents, their stylesheet, and the three shared stylesheets matched the packaged files byte for byte. C++ was inspected separately; no rebuild was performed.
- Captured 15 actual detached OpenGL framebuffers through the game's existing HUD/tab probes. All five tabs were checked at **1120×760 / 100%**; Roles, Neighbors, and empty Missions at the native **720×460 minimum / 100%**; Roles and Neighbors at **720×460 / 150%**. These are native window dimensions and application UI scale, not independent Windows DPI tests.
- A separately labeled populated fixture contains one discovered neighbor, a citizen with an assigned role, and a completed mission. These are synthetic presentation cases made in a copy of the earlier disposable save. The original test save's hash remains unchanged. A taller neighbor capture checks content below the default window's clipping boundary.
- Both existing checks passed: `cmake -P tests/ui-management6c/verify-management6c-rml.cmake` and `cmake -P tests/ui-management6c/verify-management6c-integration.cmake`. They do not establish visual correctness: the captured layouts still fail.
- Physical mouse dragging/resizing, wheel reachability, keyboard-only completion, 125%/200% scale, and long-language layouts were not accepted by this review. The automated tab activations are not substitutes for those checks.
- Successful captures were followed by process exit `0xC0000409` during the timed exit path. Two initial hidden launches failed before capture. These results are retained as runtime limitations; this report does not claim clean shutdown. Only the copied fixture's camera layer was normalized during capture setup.

[Capture script and evidence folder](../.verification/military-flow-review-20260914/capture_review.py) · [Baseline and hashes](../.verification/military-flow-review-20260914/baseline.json)

**Findings, in order of importance.** P1 blocks access or navigation; P2 significantly obscures information or disrupts a task; P3 adds avoidable clutter.

1. **P1 — The shared visibility helper defeats the intended two-pane layout.**

   At 1120×760, the squad/role/neighbor selector stays about 270 pixels wide, but details start below it across the full window. Neighbors leaves most of the right side blank while placing its detail panel below ten rows. Roles shows only the first few uniform slots; Target Priorities hides its lower reorder/attitude controls beneath the initial viewport.

   The direct cause is [Management6CRmlBinding.cpp:554](../src/gui/ui/screens/management6c/Management6CRmlBinding.cpp:554): every shown element receives inline `display: block`. This overrides the flex display declared for `military_main`, `diplomacy_main`, and the detail panes in [management6c.rcss:115](../content/rmlui/windows/management6c.rcss:115). The main body then clips overflow.

   Restore each container's intended display mode while preserving the existing protection against hidden tabs leaking content. Give the list and detail areas bounded heights and deliberate scrolling. Do this before tuning overall window dimensions; increasing the outer size currently enlarges the waste.

   Evidence: [Squads](../.verification/military-flow-review-20260914/1120x760-100/squads.png), [Roles](../.verification/military-flow-review-20260914/1120x760-100/roles.png), [Priorities](../.verification/military-flow-review-20260914/1120x760-100/priorities.png), [Neighbors](../.verification/military-flow-review-20260914/1120x760-100/neighbors.png).

2. **P1 — The supported minimum and larger text lose both content and tabs.**

   At 720×460 / 100%, Roles ends at the rename section; the assignment and uniform areas are outside the visible body. At 150%, only the first three tabs remain visible. Neighbors is active in its capture but its own tab, Missions, and essentially all useful content are hidden.

   The native minimum is fixed at 720×460 in [mainwindow.cpp:2586](../src/gui/mainwindow.cpp:2586), while layout dimensions are scaled. At 150%, that provides only roughly 480×307 logical units. The wrapping tab strip and toolbar consume the remaining content height without a usable compact arrangement.

   Reserve the tab strip's actual wrapped height and prevent navigation from shrinking out of view. At narrow effective widths, show a list or its selected detail with a clear Back action; do not stack the entire list above the entire detail. Keep Close, tab navigation, and the current task's actions reachable. A scale-aware minimum can supplement this, but cannot replace adaptation to the monitor's available area.

   Evidence: [Roles at minimum](../.verification/military-flow-review-20260914/720x460-100/roles.png), [Roles at 150%](../.verification/military-flow-review-20260914/720x460-150/roles.png), [Neighbors at 150%](../.verification/military-flow-review-20260914/720x460-150/neighbors.png).

3. **P2 — Squads loses role names and separates assignment from the citizen.**

   The old roster displayed each citizen's selected role in an adjacent selector. The new roster says only `Assigned role`, and unassigned-to-squad citizens show only `No squad`, even when they have a role. The role ID is still available; the binding simply discards its readable name ([Management6CRmlBinding.cpp:760](../src/gui/ui/screens/management6c/Management6CRmlBinding.cpp:760)).

   Changing a role now requires selecting a citizen, switching to Roles & Uniforms, choosing a role, and pressing a separate assignment button. That puts a person-specific operation in a role-definition screen and relies on remembered selection. The prominent `Selected citizen` section also wastes space when visiting Roles to edit equipment.

   Put the role name and role assignment control beside the citizen in Squads. Keep Roles & Uniforms focused on defining roles. Put Add squad with the squad list; group rename/order/delete with the selected squad; place assignment controls next to the roster or unassigned selection they act on. Make destination names explicit when moving a citizen.

   Evidence: [Populated roster](../.verification/military-flow-review-20260914/1120x760-100-populated/squads.png). Legacy contract: [SquadGnomeTemplate](../migration-quarantine/wave8-legacy-content-xaml/styles/mainmenu/styles.xaml:1089).

4. **P2 — Uniform editing is spread across three long lists, and civilian behavior is unexplained.**

   All nine equipment slots and the type/material data are present. The regression is how they are arranged: slot list, then legal-type list, then material list, after role management and citizen assignment. The selected slot and its editing choices can be far apart. The old GUI placed slot, equipment type, and material in the same row.

   Use a compact `Slot | Equipment | Material` layout, or a bounded slot list beside a clearly labeled editor for that slot. Keep None/Any, the selected type/material, and the backend's legal choices. Avoid rendering unrelated type/material sections as permanent full-width lists.

   Replace `Toggle civilian` with a stateful Civilian control. Restore the old explanation that civilian roles retreat to an appropriate area during an alarm. This is needed behavioral information, not decorative help. Give slot names readable display text while retaining their internal IDs.

   Sources: [Current role sections](../content/rmlui/windows/military_manager.rml:51), [legacy uniform row](../migration-quarantine/wave8-legacy-content-xaml/styles/mainmenu/styles.xaml:1161), [legacy civilian explanation](../migration-quarantine/wave8-legacy-content-xaml/styles/mainmenu/styles.xaml:1207).

5. **P2 — Target Priorities separates the target from its controls and loses behavior help.**

   The list preserves target names, order, and attitude. Reorder and attitude controls are below the entire target list, so they are absent from the default capture. The old GUI kept order and attitude controls with each target and explained Flee, Defend, and Attack.

   Use compact target rows with a visible rank, target name, and attitude. Keep the selected target's reorder/attitude controls in a fixed nearby area or inline. Preserve source priority order; distinguish sorting the squad selector from changing target priority. Restore the behavior tooltips. The attitude buttons are direct choices, so their dropdown markers are misleading.

   Sources: [Current priority section](../content/rmlui/windows/military_manager.rml:59), [legacy priority controls and tooltips](../migration-quarantine/wave8-legacy-content-xaml/styles/mainmenu/styles.xaml:1050).

6. **P2 — Neighbors can falsely report that no eligible citizens exist.**

   Opening the discovered-neighbor fixture auto-selects the kingdom and exposes its mission builder, but shows `No eligible gnomes are available.` The fixture contains five citizens who are not on missions. Explicitly selecting the already highlighted neighbor triggers the missing request. However, a taller 1120×1000 capture after that request still shows a blank citizen list with a scrollbar. The names remain inaccessible even when the list fits on screen; fetching eligibility alone does not resolve the observed presentation failure.

   `diplomacy.refresh` requests neighbors and missions only ([Management6CQtCommandPort.cpp:224](../src/gui/ui/controllers/management6c/Management6CQtCommandPort.cpp:224)). Automatic selection in [applyNeighbors](../src/gui/ui/screens/management6c/Management6CController.cpp:660) does not perform the available-citizen request made by [selectNeighbor](../src/gui/ui/screens/management6c/Management6CController.cpp:382).

   Request eligibility when the mission builder first becomes available, including automatic selection. Show Loading while awaiting that response, then show a true empty state only after it returns. Distinguish no eligible citizens from no citizen selected, and verify that returned citizens actually appear as readable, selectable names.

   Evidence: [Before explicit selection](../.verification/military-flow-review-20260914/1120x760-100-populated/neighbors.png), [after selection](../.verification/military-flow-review-20260914/1120x760-100-populated-selected/neighbors.png), [blank picker in the taller window](../.verification/military-flow-review-20260914/1120x1000-100-populated-selected/neighbors.png).

7. **P2 — Neighbors adds a contradictory Type row and a cramped mission builder.**

   The discovered kingdom's title says it is a gnome kingdom, but its Type field says `undiscovered`. The aggregator incorporates type into the name but never sets `GuiNeighborInfo::type`; the adapter forwards the default string. The old visible template did not show a separate Type row.

   Either supply an actual type value from the authoritative kingdom data or remove the redundant row while preserving type in the heading. Keep travel time, attitude, wealth, economy, and military strength; those are meaningful old-GUI fields.

   The mission builder also fixes its button groups to 360dp and individual buttons to 106dp. Labels such as Improve relations and Invite ambassador are clipped, while adjacent width remains unused. The Start mission control falls below the default captured area. Use a clear flow: mission type → relevant action → eligible citizen → Start mission. Size controls to their labels and available pane width, and keep Start reachable.

   Evidence: [Discovered neighbor and builder](../.verification/military-flow-review-20260914/1120x760-100-populated/neighbors.png). Sources: [aggregator](../src/gui/aggregatorneighbors.cpp:58), [default type value](../src/gui/aggregatorneighbors.h:38), [fixed builder dimensions](../content/rmlui/windows/management6c.rcss:150).

8. **P2 — Missions exposes identifiers and confusing timing instead of a useful summary.**

   The populated capture shows Target `1000000` and Participants `1000934`, although the fixture has the named kingdom and Doohickey. It also says `Elapsed 0 hours` beside `Success; total 72 hours` for a returned mission. A restored mission's elapsed counter initializes to zero; that does not make zero a useful completed-mission summary.

   Show `Mission | Destination | Participants | Status | Time` in the list, with a concise selected result/detail area. Resolve names from authoritative data where available, preserve masking for undiscovered kingdoms, and label any unavoidable unknown identifier clearly. Show phase-aware timing: elapsed for active missions, completion duration for returned missions. Do not present internal scheduler ticks as an ETA. Preserve result and duration when reported.

   Target/participant fields are useful additions rather than old-GUI regressions; their current representation is the problem. The old mission template showed type, a readable phase/result message, and time.

   Evidence: [Populated Missions](../.verification/military-flow-review-20260914/1120x760-100-populated/missions.png). Sources: [mission rendering](../src/gui/ui/screens/management6c/Management6CRmlBinding.cpp:1033), [legacy mission presentation](../migration-quarantine/wave8-legacy-src-gui-xaml/neighborsmodel.cpp:105), [mission serialization](../src/game/eventmanager.cpp:70).

9. **P2 — Empty Missions gives the wrong context and no next step.**

   It says `No neighbors or missions` even though the same loaded world has ten neighbor entries. The footer sits around y=286–320 in a 760-pixel window, leaving roughly 430 pixels of blank lower panel. Search and two sort buttons add little to this state.

   Use `No missions` and explain that missions can be started from a discovered neighbor. Add a direct Go to Neighbors action. Do not suggest a discovery action until its supported route is verified. Make the empty state compact within the shared workbench, or use the body deliberately with the footer at the bottom. Preserve the player's chosen window size when switching tabs; avoid an automatic resize on every empty/populated transition.

   Evidence: [Empty Missions](../.verification/military-flow-review-20260914/1120x760-100/missions.png). Source: [diplomacy_missions.rml:24](../content/rmlui/windows/diplomacy_missions.rml:24).

10. **P3 — Repeated chrome and implementation language compete with the actual task.**

    The full-width Search toolbar, two sort buttons, Previous row/Next row strip, `Selection: ... selected` text, duplicated title/name field, and permanent loaded-status strip take substantial space before useful content. A 40dp minimum row height also overrides the shared 30dp compact row style.

    Use one compact contextual list toolbar, a single Sort control, and a visible selected-row state. Keep keyboard navigation and actual paging when needed. Show status messages for loading/errors/actions or a selection hidden by a filter; avoid a permanent paragraph confirming an obvious selection. Consider 30–36dp desktop rows with readable text and scaling, subject to runtime verification.

    Use consistent task language: `Missions` rather than the runtime catalog's `Mission activity`; `Equipment` rather than `Legal types`; `Material` rather than `Materials reported for this type`; `New mission` rather than `Start one supported mission`. Remove the permanent paragraph about unavailable cancellation/history. Replace false dropdown markers with real selectors or unadorned direct-choice controls. Contextualize Search/Sort by tab; currently Roles says Squad order and Missions says Neighbor order, while each domain shares one filter across unlike lists.

    Sources: [military toolbar](../content/rmlui/windows/military_manager.rml:24), [diplomacy toolbar](../content/rmlui/windows/diplomacy_missions.rml:18), [runtime text catalog](../src/gui/ui/screens/management6c/Management6CText.h:69), [row size](../content/rmlui/windows/management6c.rcss:168).

**Recommended arrangement and sizing.**

Keep the five tabs in the current order. Squads, Roles & Uniforms, and Target Priorities concern military configuration; Neighbors and Missions concern diplomacy. Preserve that relationship with simple grouping and clear selected context.

| Screen | Left side | Main area | Actions that should stay near the data |
|---|---|---|---|
| Squads | Compact squad selector and Add squad | Selected squad name; roster with actual role names; unassigned citizens | Role selector on the citizen; assignment beside the relevant citizen list; selected-squad rename/order/delete |
| Roles & Uniforms | Role selector and Add role | Role name, Civilian state/help, nine-row uniform editor | Equipment and material selectors beside their slot |
| Target Priorities | Squad selector | Ranked target list with current attitude | Reorder and attitude beside the selected target or fixed immediately above/below the bounded list |
| Neighbors | Kingdom names and a useful known-state summary | Travel time, relationship, wealth/economy/military, then mission creation | Type/action/citizen choices followed immediately by Start mission |
| Missions | Compact mission table or list | Selected phase/result, named destination/participants, relevant timing | Go to Neighbors for creation, including from the empty state |

Use a roughly **220–260dp selector rail**, an **8–12dp gap**, and the remaining width for details at desktop sizes. Keep a concise header and footer outside a bounded, scrollable body. Uniforms and priorities should spend their height on data, not introductory management cards. Preserve text size while reducing redundant chrome.

The current 1120×760 default is a reasonable roomy baseline **once the flex defect is fixed**. A **960×640 logical default** is a candidate for a tighter layout, not a verified replacement. At the 720×460 minimum, and whenever UI scale makes the effective width too small for two readable panes, use the compact list/detail flow. Do not shrink fonts to make the broken layout fit. A shared workbench should retain a stable user-chosen size across these tabs.

**Old-GUI parity checklist.** The reference is the quarantined XAML **and its models**, rather than placeholder buttons alone.

| Needed information or action | Current state |
|---|---|
| Squad name, squad order, add/rename/remove | Present in bindings; placement needs improvement |
| Citizen names, roster, unassigned citizens, moving/removing citizens | Present; lower access fails in captures |
| Actual role name on each citizen and direct role assignment | Role ID retained, visible name lost; assignment moved across tabs |
| Role name, add/rename/remove, civilian flag | Present |
| Explanation of civilian alarm behavior | Missing from the new controls |
| Nine uniform slots, selected type/material, legal choices, None/Any | Retained; current arrangement buries choices |
| Per-squad target order, target names, Flee/Defend/Attack/Hunt | Retained; controls placed below the list |
| Explanations for combat attitudes | Missing from the new controls |
| Neighbor name/type description, travel time, attitude, wealth, economy, military | Legacy information retained; new separate Type row is incorrect |
| Mission availability by discovered kingdom | Retained through capability flags and controller validation |
| Emissary action choices: improve, insult, invite trader, invite ambassador | Retained; labels/placement need correction |
| Eligible citizen selection | Automatic neighbor selection omits the request; explicit selection still leaves a blank picker in the captured layout |
| Mission type, readable phase/result, time | Data retained; presentation and completed timing need improvement |
| Mission destination and participants | Added fields currently render numeric IDs |
| Abort/cancel, historical relationships, role-order arrows | Not confirmed working legacy features; do not invent replacements for placeholders |

The old Spy/Raid/Sabotage handlers were stubs, and Abort was commented out. Role-order arrows referenced commands that were not registered in the legacy model. Their mere presence in XAML is not evidence of a needed working feature. The current backend's capability flags remain authoritative. No extra health, combat-stat, quality-policy, or relationship-history sections are justified by this legacy screen comparison.

**Online guidance used and how it applies.**

- Place related controls near their data and use spacing to show real groups: move role assignment beside citizens and uniform choices beside slots. [NN/g: Proximity Principle](https://www.nngroup.com/articles/gestalt-proximity/).
- Show list and details side by side when they fit; use a deliberate list-to-detail transition when narrow. [Microsoft: List/details pattern](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/list-details).
- Reveal dependent editing choices when relevant instead of keeping every auxiliary section expanded. [NN/g: Progressive Disclosure](https://www.nngroup.com/articles/progressive-disclosure/).
- Empty states should explain the situation and help the user take the next useful action. [NN/g: Designing Empty States in Complex Applications](https://www.nngroup.com/articles/empty-state-interface-design/).
- Preserve readable text and user scaling as density improves. The guidance's TV viewing numbers are not applied as desktop sizing requirements. [Game Accessibility Guidelines: Readable default font size](https://gameaccessibilityguidelines.com/use-an-easily-readable-default-font-size/).

**Acceptance for the follow-up implementation.**

1. Correct display modes and bounded scrolling first. Verify all five tabs at 1120×760 and the chosen smaller default; verify the real minimum at 100%, 125%, 150%, and 200% scale within monitor bounds. Every tab and Close must remain reachable.
2. With multiple squads, roles, citizens, and more than one page of records, complete assignment, role change, uniform editing, and target reordering through physical input. Keep the selected item and its action visible or predictably reachable by scrolling.
3. Check unknown and discovered neighbors, automatic and explicit selection, loading and empty eligibility, all supported mission types/actions, and a successful mission-start transition. Verify empty, active, returned, and filtered Missions independently.
4. Restore role names and help; verify real type information, readable mission names, long labels, and localization fallback. Keep existing IDs, filtering/paging safety, confirmation semantics, and simulation ownership.
5. Re-run the focused contracts and relevant behavior tests, then capture the real package again. Diagnose the observed abnormal exit separately; a successful screenshot is not a clean-shutdown result.
