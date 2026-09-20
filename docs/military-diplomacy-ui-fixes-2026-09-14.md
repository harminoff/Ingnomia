**Military & Diplomacy UI fixes — 14 September 2026**

Implemented the layout, flow, and information fixes identified in the [original review](military-diplomacy-ui-review-2026-09-14.md). The workbench now defaults to **960×640** logical units, down from 1120×760. Existing saved window sizes remain respected. Wide windows show a selector beside its details; narrow or heavily scaled windows use list → detail → Back navigation.

| Screen | Result |
|---|---|
| Squads | Add sits with the squad list. Citizen role names and a native role selector appear beside the selected citizen. Roster and unassigned citizens form compact adjacent sections. Assignment/removal/movement actions appear only when applicable, and movement names its destination squad. |
| Roles & Uniforms | Role management stays with the role definition. Civilian on/off includes the alarm explanation. All nine slots appear in a compact Slot / Equipment / Material table, with native equipment and material dropdowns for the selected slot. Material choices load with the role instead of requiring a type change first. |
| Target Priorities | Ranked target rows retain source order. The selected target and its attitude/reorder controls sit above the list. Attitude help is restored; direct choices no longer look like dropdowns. At desktop sizes the list scrolls beneath its editor. |
| Neighbors | Name/type description, travel, attitude, wealth, economy, and military remain present. The incorrect separate Type row is removed. Only supported mission types appear. Automatically selected discovered neighbors now load eligible citizens; loading, failure, and no eligible citizens have distinct messages. |
| Missions | Destination and participants use readable authoritative names. Returned missions show completion duration instead of a misleading zero elapsed time. Empty Missions has a compact explanation and Go to Neighbors action. View missions connects the builder to activity. |

The shared visibility helper now restores stylesheet display rules instead of forcing every shown element to block layout. Scroll areas have bounded dimensions, tabs wrap without clipping, and short windows expose search/sort through a compact Filter / sort action. Each tab remembers its own filter and sort. Small record counts no longer stretch their lists or empty-state panels into large outlined boxes.

The old GUI comparison retained its working data and actions, including civilian/combat explanations, role names, legal uniform choices, neighbor details, and mission phase/result/time. Unsupported legacy placeholders were not added as new features. The design follows the researched [list/details pattern](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/list-details), grouping related actions with their data, and contextual empty states; the original review contains the remaining research and legacy source references.

**Build and verification.**

- Built and staged the canonical Windows package successfully: [Ingnomia.exe](../build-wave8-root-msvc-link-priority2/Ingnomia.exe). Final executable SHA-256: `7fb25064f13daca933772d2de98d9799ebd0599c3cd3fdd18c60fc2c4a8d8409`.
- All **four focused tests pass**: controller behavior, RML contract, bridge integration contract, and the new actual-RmlUi layout/input test. The separate MainWindow wiring contract and targeted `git diff --check` also pass.
- The actual-RmlUi test covers **60 combinations**: five tabs × three sizes (1120×760, 960×640, 720×460) × four UI scales (100%, 125%, 150%, 200%). It verifies reachable tabs/Close, pointer list/detail/Back navigation, wheel access to the end of compact details, all nine uniform slots at the new default, native dropdown keyboard input, correct assignment/priority targets, readable mission fields, and the empty-state route. It uses the real documents, styles, font, layout engine, and context input processing with a stub renderer and command port.
- Captured **18 final-pass packaged OpenGL framebuffers**: all five populated tabs at the new default, selected compact details, minimum-size list states at larger scales, empty Missions, and an actual mission start. Three final captures supersede the earlier squad/empty-state polish. These are native window dimensions plus application UI scale, not independent Windows DPI tests.
- Starting an emissary mission in the disposable discovered-neighbor fixture produced mission `1002035`, status **Leaving the map**, destination **The Shrieked Land**, participant **Doohickey**. The refreshed available list no longer contains Doohickey and selects Zita. Returned-mission presentation uses a separately labeled synthetic fixture with **72 hours total**.
- The two packaged RML documents, management stylesheet, and three shared stylesheets match source byte for byte. The original disposable source save's hash is unchanged. Player saves were not edited.

**Selected captured results.**

| Case | Screenshot |
|---|---|
| Squads, final contextual actions | [960×640](../.verification/military-flow-fixes-20260914/960x640-100-populated-complete/squads.png) |
| All nine uniform slots | [960×640](../.verification/military-flow-fixes-20260914/960x640-100-populated-final/roles.png) |
| Target priorities and editor | [960×640](../.verification/military-flow-fixes-20260914/960x640-100-populated-final/priorities.png) |
| Neighbors and mission builder | [960×640](../.verification/military-flow-fixes-20260914/960x640-100-populated-final/neighbors.png) |
| Returned mission names and time | [960×640](../.verification/military-flow-fixes-20260914/960x640-100-populated-final/missions.png) |
| Compact Roles navigation | [720×460 at 200%](../.verification/military-flow-fixes-20260914/720x460-200-final/roles.png) |
| Selected compact role detail | [720×460 at 150%](../.verification/military-flow-fixes-20260914/720x460-150-populated-detail-final/roles.png) |
| Empty Missions, final compact panel | [960×640](../.verification/military-flow-fixes-20260914/960x640-100-complete/missions.png) · [720×460 at 200%](../.verification/military-flow-fixes-20260914/720x460-200-complete/missions.png) |
| Actual mission start and refreshed citizens | [960×640](../.verification/military-flow-fixes-20260914/960x640-100-discovered-selected-mission-start-final/neighbors.png) |

**Remaining verification limits.**

The native computer-control helper failed to start twice, so physical OS mouse/keyboard use and native dragging/resizing were not verified. Context-level pointer, wheel, and keyboard tests are distinct evidence. Long-language layouts, independent Windows DPI settings, and every supported mission type still need live acceptance. The selected compact role capture uses an existing probe that also adds a role, solely in the disposable fixture.

The automated timed exit still ends with **0xC0000409**, as it did in the original review. All final captures completed without a forced stop, but this is not a clean-shutdown result. That existing shutdown problem remains outside these screen fixes.

**Reproduce from an x64 Visual Studio developer shell in the nested Ingnomia checkout.**

```powershell
cmake --build build-wave8-root-msvc-link-priority2 --config RelWithDebInfo --target Ingnomia
cmake -S tests/ui-management6c -B build-wave8-root-msvc-link-priority2/verification-management6c -G Ninja -DINGNOMIA_RMLUI_BUILD_DIR="$PWD/build-wave8-root-msvc-link-priority2"
cmake --build build-wave8-root-msvc-link-priority2/verification-management6c
ctest --test-dir build-wave8-root-msvc-link-priority2/verification-management6c --output-on-failure
cmake -P tests/ui-foundation/verify-mainwindow-rmlui-wiring.cmake
```

[Final tests](../.verification/military-flow-fixes-20260914/tests-complete.log) · [Package build](../.verification/military-flow-fixes-20260914/package-complete.log) · [Final audit and hashes](../.verification/military-flow-fixes-20260914/final-audit.json) · [Capture harness](../.verification/military-flow-fixes-20260914/capture_fixes.py) · [Task-only patch against the saved dirty baseline](../.verification/military-flow-fixes-20260914/task.patch)

Changes remain uncommitted. The pre-existing dirty checkout was preserved, with original copies and the starting status recorded under `.verification/military-flow-fixes-20260914`.
