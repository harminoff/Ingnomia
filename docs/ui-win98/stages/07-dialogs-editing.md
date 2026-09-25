# Stage 07: Dialogs, edit lifecycles and feedback

Date: 2026-09-24
Status: Verified for shared controls and shell/Population pilots. Stage 08 is ready; not started.

## Crash correction after initial verification

The initial pointer-hover coverage missed a Stage 07 regression. Four user crash dumps identified recursive tooltip hover dispatch. The shared presenter is corrected; Stage 07 now passes 179 assertions, and production pointer stress passes at 100%/200%. See the [crash review](07-population-crash-review.md). Stage 08 remains unstarted.

## Implemented

- `ModalDialog.h` owns a real RmlUi modal document shared by shell confirmation, profession deletion, profession draft decisions and the all-citizens skill review. It encodes dynamic text, preserves the opener with an observer pointer, rejects replacement of an outstanding review, removes listeners before dispatch and consumes each decision once.
- The common document names its heading/detail for accessibility. Actions run left to right: primary action, optional alternate, safe Cancel / Keep editing. Initial focus and the default action are safe. Escape belongs to the modal; held Escape cannot fall through into another shell action. Parent shutdown/world end remove the blocker. Native Population Close checks the draft before parking its surface.
- Classic secondary-window treatment: square raised gray frame, navy caption, compact command buttons, safe default outline, and ordinary gray message body. Dialog width and scrollable body accommodate 100/125/150/200 percent density. No translucent blackout is added to the classic dialog.
- Shell confirmations use Exit or Leave world and restore focus to the actual opener. This does not close NEW-002, which concerns Back navigation across destroyed shell routes.
- Profession deletion captures the reviewed definition and world. Changed, missing or differently selected targets are rejected before dispatch. Bulk skill review states the whole-colony count, including filtered-out citizens, and rechecks world/population revision before submission.
- The profession draft stays separate from authoritative rows. Page changes preserve it. Target switches and UI Close offer Apply, Discard draft or Keep editing. Reopen retains a draft if its controller was closed externally. Pending Apply keeps the draft and disables further edits; it has no fictitious cancel. Apply waits for a matching returned name and skill list before reporting success. Rejection keeps the draft; external changes block stale Apply until the user discards/reloads. Creating another profession cannot silently retarget a dirty draft.
- Immediate Stockpile edits retain their Stage 05 contract. The shared pattern is explicit: immediate controls commit through their existing validated command port; staged controls retain a draft until terminal acceptance or matching authoritative state. A queued command is not success. Discard affects only the draft, not already applied commands.
- Inventory, Population, Military/Diplomacy and Stockpile use the shared tooltip presenter. Stockpile's duplicate placement was removed. Tooltips use square pale-yellow help styling, plain text and measured viewport placement, including wrapped text at 200 percent. Existing hover/focus dismissal remains in the bindings.
- Progress bars and badges have square geometry. Loading retains actual named stages without invented percentages. Its visible failure body now projects the error message; dynamic progress is encoded as text. Decorative loading marks are hidden from accessibility. Empty/unavailable/loading/error distinctions remain in their typed source state; no unavailable quantity is replaced with a fabricated zero.

## Verification

| Gate | Result |
| --- | --- |
| Focused Stage 07 suite | **10/10 tests pass; 79 Stage 07 assertions.** Real modal focus containment, safe default, opening release, held Enter/Escape, repeated accept, nested review refusal, removed/changed targets, rejected Apply, pending matching-state acknowledgement, draft decisions, all-citizens revision check, readable error fallback, minimum viewport at 100/125/150/200%, loading failure and wrapped tooltip placement. [Verbose report](../evidence/stage-07/tests.txt). |
| Regressions | Rebuilt Stage 06 **10/10**, Stage 05 **7/7**, Stage 04 **4/4**. [Stage 06](../evidence/stage-07/stage06-regression.txt), [Stage 05](../evidence/stage-07/stage05-regression.txt), [Stage 04](../evidence/stage-07/stage04-regression.txt). |
| Production build | Canonical `build-wave8-root-msvc-link-priority2`, RelWithDebInfo, passed. |
| Production input | Detached Population receives injected Qt keys: opening safe focus, Escape restoration, native Close draft guard and Keep editing pass in three runs, including 200%. Shell opens, cancels, reopens and dispatches Exit once; the process then exits normally. [Run manifest](../evidence/stage-07/final/runs.json). |
| Visual capture | Final [delete review](../evidence/stage-07/final/delete.png), [draft decision](../evidence/stage-07/final/draft.png), [200%](../evidence/stage-07/final/delete-200.png), [shell confirmation](../evidence/stage-07/final/shell-primary.png). The initial `runtime/` images are intermediate evidence. |
| Source hygiene | Generated theme verification and `git diff --check` pass. [Source hashes](../evidence/stage-07/source-hashes.json). |

`CommandFeedback.h` supplies common readable rejection messages to Stockpile and Population while allowing domain localization overrides. The synthetic Population fixture deliberately has no active simulation world; its attempted skill request can therefore show the real stale-target rejection. That is fixture evidence of rejection presentation, not a successful live write.

Reproduce from the nested checkout in the x64 Visual Studio developer environment:

```powershell
cmake -S tests/ui-stage07 -B .verification/ui-stage07 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="$PWD/.build-support/Qt/6.8.3/msvc2022_64"
cmake --build .verification/ui-stage07 --parallel 8
ctest --test-dir .verification/ui-stage07 --output-on-failure
cmake --build build-wave8-root-msvc-link-priority2 --config RelWithDebInfo --parallel 8
cmake -DSOURCE_ROOT="$PWD" -P tests/ui-design-system/verify-design-system.cmake
```

The existing Inventory fixture seam accepts `INGNOMIA_AUTOMATE_DIALOGS_PROBE=1` to open the Population pilot instead, `INGNOMIA_AUTOMATE_DIALOGS_RESULT=<file>` for four Qt checks, and `INGNOMIA_AUTOMATE_DIALOGS_DRAFT=1` to leave its draft decision visible. It uses the established detached capture variable. Capture orchestration is retained in `.verification/stage07-work/capture.py`.

The focused harness uses real RmlUi layout/events and production bindings/controllers with a stub renderer and recording command ports. Native runtime probes inject Qt events; they are not physical input. The production profession fixture does not represent a live game-thread mutation.

## Remaining caller migrations and limits

| Owner | Remaining acceptance |
| --- | --- |
| Stage 08 Inventory | Visible Watch affordance, real catalog and complete command workflow; preserve stable report identity. |
| Stage 09 Stockpile | Template replacement and other inline confirmations, full bulk scope and all settings lifecycle paths. Immediate form behavior remains covered by Stage 05. |
| Stage 10 Workshop | Order-draft target changes, queue pending/rejection feedback and complete production command effects. |
| Stage 11 Agriculture | All bulk plot scopes and error/unknown/empty states. |
| Stage 12 Population | Complete profession create/delete/update gameplay round trip, per-gnome/bulk skill and schedule scope parity, and broader route restyling. |
| Stage 13 Military | Migrate existing inline squad/role confirmations to the shared modal; retain registry modal identity and target validation. |
| Stages 14-17 Diplomacy/trade/other workbenches | Migrate outstanding review and draft callers, with each domain's authoritative reconciliation and consequences. |
| Stages 18-19 Shell/Settings | Route-history focus (NEW-002), complete retry/save/load/settings draft lifecycle and every error state. |
| Stage 20 | Physical mouse/keyboard, arbitrary translations, accessibility devices, detached high contrast/theme propagation and full minimum-size matrix. |

Population checks the review against the latest UI snapshot immediately before dispatch. This does not establish atomic compare-and-swap across the asynchronous simulation queue. Existing command ports still validate world/action contracts. A queued write with no matching authoritative response remains pending; it is not reported as successful or cancelable. Correlated command outcomes and queue-time domain version checks remain explicit route acceptance work. No undo is invented.

No user save is opened or written by the synthetic dialog fixtures. Existing dirty work, including the unrelated layer-scroll review, is preserved. No commit is created.

## Reference

Microsoft's original *The Windows Interface Guidelines: A Guide for Designing Software* (1995), Chapter 8, especially message-box actions on PDF pages 172-173, specifies the least destructive default and explicit action labels when Yes/No is ambiguous. This is a pre-Windows-98 primary design reference, not a claim that it is a Windows 98-specific manual. [Archived Microsoft-authored manual](https://www.aoisnow.net/blog/wp-content/uploads/2019/10/Microsoft_WindowsGuidelines.pdf). The established Stage 02-05 Windows 98 palette and bevel contract is retained. [RmlUi document semantics](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/documents.html) confirms that modal documents prevent other documents receiving focus.
