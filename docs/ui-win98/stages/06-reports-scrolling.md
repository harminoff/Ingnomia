# Stage 06: Reports, filters, selection and scrolling

Date: 2026-09-24
Status: Verified for shared controls and Inventory/Stockpile report pilots. Stage 07 is ready; not started.

## Changes

- Extracted the Inventory and Stockpile header/filter styles into `components.rcss.in` and its generated stylesheet. Route-specific column widths and field semantics remain in their owners. `ReportControls.h` shares sorted/deduplicated option markup, cached projection, keyboard navigation and sort indicators.
- Preserved substring filters, exact multi-selection, OR within a column / AND between columns, and quantity predicates All / Has (>0) / None (0). Added **Reset column**, which clears both typed and exact filters; All keeps its existing exact-selection-only meaning. Filtered columns have a visible border marker. Existing backed Stockpile counts remain; no new Inventory count or global toolbar was added.
- Arrow/Home/End navigation, Escape/Tab dismissal, focus restoration and outside-click dismissal work on the custom report menus. Option lists remain lazy; unchanged projection keeps their DOM and focus. Sort direction uses the existing original arrow atlas and exposes `aria-sort`.
- Report rows and matrix cells own Enter/Space; held keys dispatch once. Inventory Space keeps Watch semantics instead of opening detail.
- Inventory sort preserves the selected stable ID. Filtering preserves it while visible and clears it when excluded. Deletion clears selection without silently targeting another record. Refresh/sort preserve a surviving visible row anchor; the existing typed command path still owns actions.
- Corrected pixels-versus-dp calculations in Inventory and Stockpile Allow-list virtualization. The fixed row height remains 48dp. Added fixed row bounds and retained a bounded live DOM.
- Header and body share a horizontal scrolling parent at narrow widths. Essential columns remain reachable; the Inventory header reserves its scrollbar gutter and Stockpile headers now reserve theirs. Numeric values align right.
- Selected rows, keyboard focus and inactive native-window selection have separate treatment. Inventory and Stockpile full labels use the existing tooltip helpers and accessible row labels. The glyphs and palette follow the earlier Windows 98 baseline.
- Completed generated scrollbar cross-axis sizing, corner and pressed-thumb treatment using the pinned RmlUi element names. Existing route scrollbar consumers remain intact.
- Added `MatrixNavigation.h` for bounded focus movement without mutation. Schedule cells expose stable creature/hour identity and column labels; sorting/reordering retains identity and removal clears it. Inventory remains flat. Existing tree implementations were not converted into reports.

## Verification

| Gate | Result |
| --- | --- |
| Focused Stage 06 suite | **10/10 tests pass; 56 Stage 06 assertions.** Includes 10,000 Inventory records, 2,000 Stockpile rules, stable sort/patch/delete action targets, filter/reset/cancel, 100/125/150/200% virtual-list reachability, horizontal overflow, column alignment, and matrix movement/reorder/removal without commands. [Verbose results](../evidence/stage-06/tests-final.txt). |
| Generated scrolling | Real RmlUi generated vertical/horizontal arrows, track paging, thumb dragging, nested wheel ownership, tiny-range clamping, no-range behavior and corner presence pass. Renderer in the focused harness is a stub; layout and event dispatch are real RmlUi. |
| Stage 05 regression | **7/7 tests pass.** [Results](../evidence/stage-06/stage05-regression.txt). |
| Production build | Canonical `build-wave8-root-msvc-link-priority2`, RelWithDebInfo, passed. |
| Production input and capture | Synthetic large Inventory fixture exercised through injected Qt key events in the actual detached window: end-of-list materialization, stable sort target, popup open/cancel and bounded DOM. All five assertions pass at 100/125/150/200% and the high-contrast-request run. [Capture/run manifest](../evidence/stage-06/accepted-final/runs.json). |
| Visual review | Shared headers, popup/reset, numeric alignment, selected/focused row, 200% rendering and Stockpile report captured. Earlier captures exposed header cascade, gutter and tooltip timing defects; corrected final images are in `accepted-final/`. |
| Source hygiene | Design-system source verifier and `git diff --check` pass. Existing dirty tracked/untracked work preserved. |

Final examples: [Inventory](../evidence/stage-06/accepted-final/inventory.png), [filter popup](../evidence/stage-06/accepted-final/inventory-popup.png), [200%](../evidence/stage-06/accepted-final/inventory-200.png), [high contrast](../evidence/stage-06/accepted-final/inventory-hc.png), [Stockpile](../evidence/stage-06/accepted-final/stockpile.png).

## Performance comparison

The same production Inventory binding and 10,000-row dataset were measured before the edits and after them. The [baseline](../evidence/stage-06/baseline.txt) recorded **59.5 ms initial projection/update/render submission**, **99.1 ms for twenty scroll/update/render submissions**, **522 initial elements**, **523 after scrolling**, and 26 initial row-container children. Final idle samples are in [performance results](../evidence/stage-06/performance.json). The three idle samples have medians of 64.0 ms / 102.8 ms with the same element counts (small differences from the single baseline sample). A simultaneous capture/test run reached 77 ms; it was repeated without competing capture work rather than treated as an acceptance threshold.

These are CPU-side projection/layout and stub-render submission measurements, not GPU frame times. The live application captures separately establish that the production renderer works. There is no demonstrated large-list DOM growth or material regression in this scope; no claim about total gameplay frame rate is made.

## Reproduction

Enter the x64 Visual Studio developer environment, then run from the nested repository:

```powershell
cmake -S tests/ui-stage06 -B .verification/ui-stage06 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="$PWD/.build-support/Qt/6.8.3/msvc2022_64"
cmake --build .verification/ui-stage06 --parallel 8
ctest --test-dir .verification/ui-stage06 --output-on-failure
cmake --build .verification/ui-stage05 --parallel 8
ctest --test-dir .verification/ui-stage05 --output-on-failure
cmake -DSOURCE_ROOT="$PWD" -P tests/ui-design-system/verify-design-system.cmake
cmake --build build-wave8-root-msvc-link-priority2 --config RelWithDebInfo --parallel 8
```

Production probing extends the existing `INGNOMIA_AUTOMATE_UI_FIXTURE=inventory` route. Set `INGNOMIA_AUTOMATE_REPORTS_PROBE=1`, `INGNOMIA_AUTOMATE_REPORTS_RESULT=<result path>`, and the existing detached capture path. Optional `INGNOMIA_AUTOMATE_REPORTS_POPUP=1` leaves the quantity menu open; `INGNOMIA_AUTOMATE_REPORTS_RIGHT=1` scrolls the report horizontally. The synthetic dataset is opt-in and does not alter a game save. Capture orchestration is retained in `.verification/stage06-work/capture.py`; final source hashes are in [the evidence manifest](../evidence/stage-06/source-hashes.json).

The generated-part behavior follows the [RmlUi scrollbar documentation](https://mikke89.github.io/RmlUiDoc/pages/style_guide.html) and was checked against the pinned source. Existing Windows 98 geometry/palette decisions remain those recorded in Stages 02–05.

## Remaining route acceptance and limits

- Stage 08/09 own final Inventory and Stockpile workflows, real-world catalog diversity, all bulk scopes and complete action parity. This stage checks emitted stable targets through the production binding/controller seams; no new gameplay simulation outcome is claimed.
- Inventory intentionally has no paging/count/global-filter controls. Population paging remains covered by its controller suite; a full redesigned population report belongs to Stage 12.
- Stage 11/13 own plot/schedule workflow acceptance and broader matrix scope. The shared movement helper and Schedule pilot do not establish every matrix or existing tree's runtime behavior. No real tree expansion implementation was changed; complete tree route interaction remains downstream.
- The high-contrast request did not visibly restyle the detached Inventory fixture; full detached-theme propagation remains an explicit Stage 20 gap.
- Arbitrary long translations, all detached minimum sizes, full theme/high-contrast consistency and physical input remain Stage 20. Narrow horizontal reachability is tested in RmlUi; native windows may enlarge with density. Native physical mouse/keyboard input was unavailable, so all input evidence is injected.
- Synthetic stress rows omit full parent/item metadata and use fallback thumbnails; they are performance fixtures, not a claim of complete catalog content. The actual Stockpile fixture retains its item icons. No normal user save was modified.
- `first/` and `accepted/` contain intermediate evidence. Use `accepted-final/` for the final visual record. No Stage 07 implementation or commit was made.

The rebuilt Stage 04 regression suite also passes 4/4 after the report key-ownership change. See [results](../evidence/stage-06/stage04-regression.txt).
