# Stage 08 â€” Inventory & Resources

Date: 2026-09-24
Status: VERIFIED for automated implementation and acceptance scope. Stage 09 not started.

## Implemented

- Retained Category / Type / Item / Material / In Stock / Total, with shared sort indicators, selection, focus, bevels, and scrollbars. Essential header/filter text is 13dp. Numerical cells remain right-aligned.
- In Stock and Total are full-hit-area choice buttons with All / Has (>0) / None (0), supporting Enter/Space, arrow navigation and Escape. Active filter markers remain visible; Clear filters clears typed and exact selections in one controller notification. The summary reports the actual projected leaf count and distinguishes loading from zero matches.
- Compact report rows are **32dp**, with **24dp inline sprites**. The detail preview remains 40dp. Virtual row positioning, spacers, keyboard focus and density calculations use the same row height. Keyboard navigation reveals the selected row after density changes.
- The six-column table uses a **700dp content minimum**, with one horizontal scroll parent for header and body. The supported native minimum remains **640 x 420**, and the ordinary host remains non-resizable. No column is removed. Popups are moved outside that scrolling parent and clamped to the client area; their own scroll range makes the final command reachable.
- Details retain recipes, products, stockpile locations and real history. The item heading and Back/Watch controls remain visible; metrics and grouped content share a scroll region. Narrow layouts stack the groups without overlap. This avoids both the former 760dp fixed-detail width and an unusably short content area at 200%.
- Detail entry records the report's stable row anchor, pixel offset and horizontal position before hiding it. Back restores both scroll axes, selected identity and row focus. Filters and sort remain controller-owned throughout related-item navigation. Missing history targets are skipped. A removed current target returns to the report with an explanation and cannot issue a stale Watch/location action.
- Watch item / Stop watching exposes the existing `watch.set` command for the current detail identity. Location commands are revalidated against the current detail snapshot before dispatch.

## Defects found during implementation

1. Fixed-width detail cards clipped at small hosts. Initial stacked flex cards also collapsed vertically; framebuffer inspection caught the overlap and the test now checks group height/separation.
2. A materialized selected row could stay outside the visible area after changing density. Focus now computes the current row range and reveals it before focusing.
3. Filter popups were clipped by the table's horizontal overflow. In RmlUi, `fixed` retains its positioned ancestor, so browser-style viewport coordinates alone were insufficient. The popups now live under the window frame. See [RmlUi positioning rules](https://github.com/mikke89/rmluidoc/blob/master/pages/rcss/visual_formatting_model.md).
4. The old direct detail/Back path did not explicitly bookmark hidden-list scroll geometry. Restoration now uses a stable anchor and does not depend on hidden-element layout.

## Acceptance evidence

| Gate | Result |
| --- | --- |
| Stage 08 suite | **10/10 tests, 190 focused assertions.** All six sort directions and filter choices, active markers/Clear filters, large/long-name/single/empty/filtered-empty data, detail refresh/removal, related products, history, location dispatch, Watch identity, four-scale alignment and Back restoration. [Verbose report](../evidence/stage-08/acceptance.txt). |
| Stage 06 / Stage 07 regressions | **10/10 each**, including Population pointer-hover stack-overflow coverage. [Report](../evidence/stage-08/regressions.txt). |
| Large catalog | 10,000 leaf records, 32 materialized row children at the initial test viewport. Initial projection/layout measured about **115ms** in the recorded headless run. This is a local measurement with a stub renderer, not a GPU/frame-time guarantee. |
| Production minimum-size matrix | Report, quantity-popup and detail captures at **100/125/150/200%**. Each run traversed a filtered 10,000-entry list to the end, opened details, and verified exact scroll plus row-focus return. [100% check](../evidence/stage-08/verified/report-1.checks.txt), [125%](../evidence/stage-08/verified/report-1.25.checks.txt), [150%](../evidence/stage-08/verified/report-1.5.checks.txt), [200%](../evidence/stage-08/verified/report-2.checks.txt). |
| Visual inspection | [Compact report](../evidence/stage-08/verified/report-1.png), [unclipped choices](../evidence/stage-08/verified/choices-1.png), [200% choices](../evidence/stage-08/verified/choices-2.png), [stacked details](../evidence/stage-08/verified/detail-1.png), [200% scrollable details](../evidence/stage-08/verified/detail-2.png). |
| Live copied save | Tutorial Valley produced real item sprites, recipes, outputs and `1:ready` history. Pointer injection followed Wine â†’ Vinegar â†’ Wine and Wine â†’ Fruit â†’ Wine. All 21 source and copy hashes stayed unchanged. [Trace](../evidence/stage-08/live/trace.txt), [manifest](../evidence/stage-08/live/manifest.json), [real detail capture](../evidence/stage-08/live/detail.png). |

## Live location follow-through

The existing opt-in Stockpile probe created one field and one Oak RawWood item in memory on another isolated Tutorial Valley copy. Inventory showed **Total 1 / In Stock 1**, the location name/count and returned history. Injected pointer input followed RawWood → Plank → RawWood, returned to the report, reopened the original detail, and activated the stockpile location. The real Stockpile manager displayed that item. All **21 source and copied save files remained hash-identical**; no save command was issued.

[Trace](../evidence/stage-08/live-location/trace.txt), [save manifest](../evidence/stage-08/live-location/manifest.json), [Inventory detail](../evidence/stage-08/live-location/detail.png), [opened Stockpile](../evidence/stage-08/live-location/stockpile.png). The raw-material probe correctly finds no ingredient link; its extra Back attempt after already reaching the report is a no-op. The seeded stockpile is diagnostic in-memory setup, not a normal player-created construction claim.

Final production build and static contracts pass; `git diff --check` passes. No new WER crash dumps appeared during Stage 08 runs. [Build output](../evidence/stage-08/production-build.txt).

## Reference contract for later stages

Use the shared report controls and the current Inventory source for the 13dp header, 32dp virtual rows/24dp sprite, numeric choice buttons, visible filter markers, and one horizontal table range. Keep field identity and quantity semantics route-owned. Keep popups outside clipped table ancestors. Use document-only layout updates inside input handlers; never reintroduce `Context::Update()` in hover/focus callbacks.

The historical skill reference's 48dp/40dp report geometry and 10dp header values are superseded by this checkpoint. Its architecture, command routing and flat catalog semantics still apply.

## Evidence limits

- `verified/` is the current scale evidence. `runtime/` and `final/` contain intermediate captures; they exposed fixture/capture timing and layout defects and are not acceptance evidence.
- Synthetic fixture rows intentionally have no real world. Their history command rejection is displayed as readable feedback; this is not a fabricated successful history result.
- Input was injected through real Qt/RmlUi paths. Physical mouse/keyboard use, dragging and extended gameplay are not claimed.
- No normal user save was opened or changed. Live runs load isolated copies; no save command is issued. No new stockpile editing behavior is part of this stage.
