# Stage 05: Form controls

Date: 2026-09-24
Status: VERIFIED for shared controls and production pilots. Stage 06 is ready, not started.

## Implemented

- Shared editable, read-only, disabled and invalid field treatment; persistent labels; read-only text supports selection/copy while mutation paths are blocked.
- Native checkbox check marks and radio dots with fixed geometry, clickable labels, release-owned Space activation and exclusive radio arrow navigation. The original glyph atlas is recorded in `content/rmlui/ASSET_LICENSES.md`.
- `NumericEditor` separates an integer draft from the controller-accepted value. It supports trimmed typing/paste, explicit range errors, bounds, stepping, Enter/blur commit, Escape restore, and rejected-commit feedback. It does not silently commit invalid text.
- New Game ranges retain their bindings and gain exact fields, stacked arrows and units. Slider and numeric edits converge through the controller. Invalid drafts block Start and reveal their owning page.
- Stockpile Settings uses the shared editor, preserves 1-based displayed rank / 0-based payload and makes “1 is highest” visible. Apply rejects invalid priority. Name and hauling edits keep the established authoritative command path.
- Editable Stockpile template combo keeps its legal catalog and custom behavior, adding keyboard navigation, cancellation/focus restoration, click-away and parent-close handling. Native material combos restore their opening selection on Escape.
- Workshop new-order mode uses native exclusive radios. Changing mode updates the draft without queuing a craft. Existing queue-edit mode buttons remain assigned to Stage 10.
- Square beveled slider thumbs replace rounded filled-track treatment. Selector contents and arrow layout were corrected after renderer inspection. High-contrast numeric units and selector values are readable.

## Design and implementation references

The Windows 98 baseline remains the [original Microsoft UI guidance](https://learn.microsoft.com/en-us/previous-versions/ms997615(v=msdn.10)). Additional Microsoft [control guidance](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb226806(v=vs.85)) and [numeric-input guidance](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb246443(v=vs.85)) were consulted as supplementary later documentation. These are adapted to the game's scalable Qt/RmlUi controls, not claimed as pixel-exact native Windows widgets.

Context7 and the pinned RmlUi implementation were consulted for [native form semantics](https://mikke89.github.io/RmlUiDoc/pages/rml/forms.html). The pinned forms implementation has no numeric input type, so the shared integer editor deliberately uses text controls and explicit validation. Native labels own checkbox/radio click forwarding; custom duplicate label toggles are avoided.

## Verification and evidence

| Gate | Result / evidence |
| --- | --- |
| Production build | Passed, canonical `build-wave8-root-msvc-link-priority2`, RelWithDebInfo. |
| Stage 05 focused suite | **7/7 passed; 78 Stage 05 assertions**, including malformed/empty/whitespace/overflow input, clipboard paste, min/max/steps, blur/Enter/Escape, disabled and rejected edits, label dispatch, exclusivity, readonly protection, slider convergence and production binding pilots. [Final verbose results](../evidence/stage-05/accepted-final/tests-final.txt). |
| Stage 04 rebuilt regression suite | **4/4 passed**, 82 Stage 04 assertions. [Results](../evidence/stage-05/accepted-final/stage04-regression.txt). |
| Production Qt host | 167 Rml + 13 Qt host + 10 form checks; zero pause/world-key signals. [Normal trace](../evidence/stage-05/accepted-final/new-game.trace.txt), [high-contrast trace](../evidence/stage-05/accepted-final/new-game-hc.trace.txt). |
| Visual matrix | 10 successful runs: New Game normal/high contrast, Stockpile Settings, template popup, gallery 100/125/150/200%, gallery high contrast 100/200%. [Run manifest](../evidence/stage-05/accepted-final/runs.json), [source hashes](../evidence/stage-05/accepted-final/source-hashes.json). Final two added assertions are recorded separately in [test provenance](../evidence/stage-05/accepted-final/test-followup.json). |
| Live Stockpile authority | Invalid Apply blocked; name, priority and hauling changes observed on the game-thread Stockpile object. **All 21 copied-save files and source files unchanged.** [Trace](../evidence/stage-05/live/trace.txt), [manifest](../evidence/stage-05/live/manifest.json), [after capture](../evidence/stage-05/live/after.png). |
| Live Workshop projection | Actual loaded-world Carpenter catalog, order-mode radios and material combos rendered. [Capture](../evidence/stage-05/accepted-final/workshop-live.png), [trace](../evidence/stage-05/accepted-final/workshop-live.trace.txt), [21-file unchanged-save manifest](../evidence/stage-05/accepted-final/workshop-live-manifest.json). Model was probe-created in memory; normal construction and completed crafting are not proven. |
| Source checks | Design-system verifier and `git diff --check` passed (line-ending warnings only). |

Representative final images: [New Game](../evidence/stage-05/accepted-final/new-game.png), [Stockpile Settings](../evidence/stage-05/accepted-final/stockpile.png), [template popup](../evidence/stage-05/accepted-final/template-popup.png), [200% forms](../evidence/stage-05/accepted-final/forms-200.png), [200% high contrast](../evidence/stage-05/accepted-final/forms-hc-200.png).

The earlier `first/` and `accepted/` directories retain intermediate defect evidence; use `accepted-final/` for visual acceptance. Visual review corrected stale validation text, range-edge artifacts, redundant priority instructions, clipped selector contents, missing gallery radio labels, and white unit labels on light route surfaces.

## Reproduction

From the nested repository after entering the x64 Visual Studio environment:

```powershell
cmake --build .verification/ui-stage05 --parallel 8
ctest --test-dir .verification/ui-stage05 --output-on-failure
cmake --build .verification/ui-stage04 --parallel 8
ctest --test-dir .verification/ui-stage04 --output-on-failure
cmake -DSOURCE_ROOT="$PWD" -P tests/ui-design-system/verify-design-system.cmake
cmake --build build-wave8-root-msvc-link-priority2 --config RelWithDebInfo --parallel 8
```

The focused suite has its own `tests/ui-stage05/CMakeLists.txt`. Production form probing is opt-in with `INGNOMIA_AUTOMATE_FORM_CONTROLS=1` alongside the existing shell-tab probe; live Stockpile edits require `INGNOMIA_AUTOMATE_STOCKPILE_FORM_PROBE=1` alongside its existing open probe. Only disposable data roots/copied tutorial saves were used. Capture orchestration remains in `.verification/stage05-work/`.

## Explicit remaining coverage

- Native physical mouse/keyboard delivery remains unverified; these checks inject Rml/Qt events. No full-suite or all-screen acceptance is claimed.
- Stage 06/08/09 own the complete column-filter, large catalog, scroll, report selection and Stockpile scope matrix. The template popup is checked inside its supported 720px host and native combo identity survives resize; arbitrary narrow-window and long-translation bounds remain Stage 20.
- Stages 10-15 own remaining Workshop queue editors, agriculture/population/military/diplomacy forms and ordinary-button choice migrations. Existing aggregate mixed-state data is retained; no unsupported third Boolean state is invented. Mixed aggregate route behavior is still downstream.
- Stage 18/19 own full shell/Settings restyling and Settings numeric coverage; approximate volume interaction remains valid. New integer validation text and unit captions currently use English fallback text; full localization/long-translation review remains open.
- Stage 20 owns integrated high contrast, scaling and physical input. Existing gallery secondary text/Close/tree glyph issues and the broader New Game palette remain open; the pilot's introduced unit/selector contrast defects were corrected here.
- Existing dirty tracked/untracked work was preserved. No commit, broad cleanup, normal user-save modification, Stage 06 implementation or simulation redesign was performed.
