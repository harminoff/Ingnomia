# Stage 09 - Stockpiles

Date: 2026-09-24
Status: Implemented; automated acceptance verified. Stage 10 is next.

## Changes

- Connected **Stock / Allow list / Settings** property-sheet tabs use the shared tab behavior. Center on map is an object command above the tabs. The caption retains the authoritative name and the toolbar always shows Active or Suspended.
- Stock and Allow list use 13dp headers, 32dp rows and 24dp item sprites, synchronized horizontal report scrolling, shared sort/filter behavior and full-field quantity choice buttons. Quantity choices always include All / Has (>0) / None (0). Report popups and the saved-template picker escape clipped report containers and stay inside the host; switching tabs closes report popups.
- **In Stock** counts this stockpile's stored items. **Total** is `Inventory::itemCountDetailed(item, material).total`, which counts matching registered items without excluding held or job-assigned items. It is global inventory, not capacity, incoming deliveries, or immediately available stock. The Stock help explains local versus settlement-wide scope.
- Matching-rule count is beside Allow matches / Block matches and explicitly includes offscreen matches. A real modal reviews the count and stockpile name. Commit compares the captured stockpile ID, revision and complete matching stable-ID vector with current state. Cancel and changed-scope reviews issue no mutation.
- **Save new** and **Update existing...** are separate. Save new rejects duplicate names; the backend also prevents a race from turning creation into overwrite. Updates name the existing template and revalidate before dispatch. Selecting a saved template reviews replacement of the current allow list. Empty names, missing targets/templates, failed writes and canceled updates produce no silent replacement. Current rules and saved copies are explicitly distinguished.
- Name and priority are one staged form with **Apply / Revert**. Typing, blur and spinner arrows do not mutate the simulation. Priority is displayed from 1, with 1 highest. Invalid or rejected drafts remain editable; successful Apply waits for a matching snapshot. External name/priority changes require Revert before another edit can overwrite them.
- Drafts and unfinished template text are retained by stockpile ID across tab changes, object changes and close/reopen, until the world ends. This policy is visible beside the form. A focused editor is explicitly refreshed when its object changes.
- Hauling checkboxes and suspension apply immediately; Revert affects only the staged name and priority. Pending changes block conflicting basic-setting commands.
- Manager and an already-open inspector consume the same authoritative snapshots, guarded by stable target identity. Inspector priority now uses the same one-based display. Late updates for another requested stockpile cannot retarget the manager. Modal ownership is cleared when the world or stockpile changes.

## Verification

| Gate | Result |
| --- | --- |
| Stage 09 focused suite | **10/10 tests; 90 assertions.** Draft lifecycle, focused object switch, rejection/pending, invalid priority, conflicts/removal, exact rule scope, template create/update/cancel/empty/stale review, virtualized 2,000-rule list, four-scale quantity popups and tab dismissal. [Summary](../evidence/stage-09/acceptance-final.txt), [verbose](../evidence/stage-09/acceptance-verbose.txt). |
| Earlier form pilot | Stage 05 rebuilt: **7/7**. Expectations now require staged arrows and an explicit template replacement review. [Report](../evidence/stage-09/focused-tests.txt). |
| Earlier report/Inventory suites | Rebuilt Stage 06 and Stage 08: **10/10 each**. The report check found and corrected a 4dp header/body mismatch. [Report](../evidence/stage-09/regression-tests-final.txt). |
| Production build | Canonical RelWithDebInfo executable rebuilt successfully; final stylesheet staged. [Build](../evidence/stage-09/production-final.txt), [asset staging](../evidence/stage-09/package-final.txt). |
| Minimum host | Production Stock and popup captures at 100/125/150/200%; Allow list, Settings, bulk and named-template reviews also captured at 100/200%. Minimum host is 640 x 420. At high density the property page scrolls vertically and reports horizontally; no column is removed. [Final captures](../evidence/stage-09/final/runs.json). |
| Final popup/default-window review | Final build captures complete default-size Stock/Allow list/Settings and all four quantity-popup scales. These supersede earlier popup captures. [Manifest](../evidence/stage-09/final-review/runs.json). |
| Authoritative copied-world commands | Real one-field Stockpile, one Oak RawWood item. Invalid Apply blocked; valid name/priority and hauling reached the game object. Filtered scope: 28 stable rule IDs. Block validated all 28 already-blocked rules without changing other leaves; Allow changed exactly 28 leaves. Duplicate Save new and canceled Update preserved the saved filter; confirmed Update persisted the current filter. Manager suspension then inspector suspension produced matching authoritative state. [Trace](../evidence/stage-09/live-final/trace.txt). |
| Save safety / shutdown | All **21 source and copied save files unchanged**. Only the isolated profile's template/settings files were written. Live probe process exited normally. [Manifest](../evidence/stage-09/live-final/manifest.json). |

## Visual evidence

- [Default Stock](../evidence/stage-09/final-review/stock-1.png)
- [Default Allow list and explicit rule scope](../evidence/stage-09/final-review/allow-1.png)
- [Default staged Settings](../evidence/stage-09/final-review/settings-1.png)
- [200% minimum report](../evidence/stage-09/final/stock-2.png)
- [200% bulk review](../evidence/stage-09/final/bulk-2.png)
- [200% named template review](../evidence/stage-09/final/template-2.png)
- [200% quantity choices](../evidence/stage-09/final-review/popup-2.png)

`visual/` and `verified/` contain intermediate captures and are superseded by `final/` and `final-review/`. The first live run is retained under `live/`; `live-final/` uses the final compiled interaction code. Captures use the actual Qt/OpenGL/RmlUi renderer; synthetic fixture images and copied-world command evidence are distinct.

## Evidence limits

- Input was exercised through real RmlUi/Qt adapters and opt-in production probes. Physical mouse/keyboard, OS dragging and native window switching are not claimed.
- UI/controller revalidation occurs immediately before dispatch. This does not establish a general atomic simulation-queue revision transaction, consistent with the Stage 07 boundary.
- Backend duplicate-name protection and persisted template contents were exercised. Disk-write failure handling exists but a disk failure was not injected.
- The copied-world inspector test verifies live suspension synchronization. Name, priority and counts use the same snapshot projection; arbitrary simultaneous edits in multiple windows were not stress-tested.
- No new crash dumps appeared during the runs; the latest remains the previously diagnosed Population dump from 15:59 on September 24.

## Next

Stage 10: Workshops, production queues and direct trade editing. No Stage 10 implementation was started here.
