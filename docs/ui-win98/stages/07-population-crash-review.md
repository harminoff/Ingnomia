# Population crash review — 2026-09-24

Stage 08 remains unstarted. The reported crash is corrected in the canonical playable build.

## Cause

All four recent WER dumps (PIDs 25448, 12720, 76432, 47340; 15:57:49–15:59:19 local time) report **0xc00000fd, stack overflow**. Matching executable/PDB symbols show the same repeating chain:

`Management6BRmlBinding mouseover → showManagementTooltip → Context::Update → UpdateHoverChain → mouseover`

Stage 07 introduced the context update to measure wrapped tooltip height. A pointer entering a titled Population control re-entered hover dispatch before its previous dispatch finished. Clicking around Population triggered hovering; the dump evidence points to tooltip recursion, rather than an authoritative population command.

[Latest stack](../evidence/population-crash-20260924/latest-stack.txt), [76432](../evidence/population-crash-20260924/stack-76432.txt), [12720](../evidence/population-crash-20260924/stack-12720.txt), [25448](../evidence/population-crash-20260924/stack-25448.txt).

## Correction

- The shared tooltip presenter now calls `ElementDocument::UpdateDocument()` to measure layout without re-entering context hover handling. This covers Population, Inventory, Stockpile, Military and Diplomacy consumers.
- Inventory row focus also uses document-only layout updating inside its input callback.
- Wrapped tooltip measurement and viewport clamping remain intact, including 200% density.
- Added actual pointer movement/button input through the Qt input adapter in the focused test and RmlUi pointer input in the production detached Population fixture. Each scale exercises four tab controls repeatedly, then leaves/re-enters the Professions tooltip target and checks visibility.

The pinned RmlUi implementation updates document styles/layout/position without `UpdateHoverChain`. Its [document API guidance](https://github.com/mikke89/rmluidoc/blob/master/pages/cpp_manual/documents.md#manually-updating-the-document) specifies this operation before querying modified element dimensions.

## Verification

- Canonical RelWithDebInfo game rebuilt successfully. [Build and focused suite](../evidence/population-crash-20260924/final-build-tests.txt), [binary hash](../evidence/population-crash-20260924/binary-sha256.txt).
- Stage 07: **10/10 tests; 179 assertions**, including actual hover entry/leave at 100% and 200%, and the prior 200% wrapped-tooltip viewport check. [Assertions](../evidence/population-crash-20260924/assertions.txt).
- Stage 06 rebuilt: **10/10 tests**. [Report](../evidence/population-crash-20260924/stage06.txt).
- Production at each scale: **100 tab clicks, 25/25 visible tooltip entries**, followed by passing modal-open, Escape-focus restoration, native-close draft guard and Keep editing checks. [100%](../evidence/population-crash-20260924/runtime/delete.hover.txt), [200%](../evidence/population-crash-20260924/runtime/delete-200.hover.txt), [runs](../evidence/population-crash-20260924/runtime/runs.json).
- No new WER dumps appeared during these runs. `git diff --check` passed.

## Limits and earlier coverage gap

The earlier Stage 07 suite called the tooltip helper directly and exercised keyboard focus; it missed hover-dispatch reentrancy. Its earlier green result did not establish mouse safety.

Current runtime checks use synthetic fixture rows in an isolated application-data profile. They do not load or alter the user's normal save. Physical mouse input and extended gameplay remain unverified. This fix addresses the common stack found in all four recent dumps; it is not a claim that every possible Population crash is resolved.
