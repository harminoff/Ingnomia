# Stage 10 - Workshops, production queues and direct trade editing

Date: 2026-09-24
Status: Implemented; automated acceptance verified. Stage 11 is next.

Stage 10 was started in an earlier session, which added controller drafts, stable-ID queue edits, targeted trade revisions and focused tests, but never ran live or wrote a checkpoint. This session reviewed that work against WRK-01..07, ran the live probe for the first time, fixed the defects it exposed, and then, at the user's direction, rebuilt the workshop window as a Windows 98 property sheet.

## Design sources

The user directed that only documented Windows UI be used:

- `docs/MS-Windows-User-Experience-2001.pdf` (Microsoft Windows User Experience; page numbers are the PDF's "Page N of 421" headers).
- MSDN "Design Specifications and Guidelines - Visual Design" (`learn.microsoft.com/en-us/previous-versions/ms997612(v=msdn.10)`), with its Layout (ms997619), Design of Visual Elements (ms997615) and Controls (ms997492) pages.

Where a rule was taken from these sources it is cited below. Text uses "MS W98 UI" (`content/rmlui/fonts/MSW98UI-Regular.ttf` / `-Bold.ttf`), an outline conversion of MS Sans Serif supplied by the project owner. It carries no license and derives from Microsoft's font; the owner chose to use it knowing this, and provenance is recorded in `fonts/notices/MS-W98-UI-PROVENANCE.txt`. Review before any public distribution. The Close glyph is the text character `×`.

## Windows 98 caption (all six managers)

Workshop, Stockpile, Inventory, Population, Military and Diplomacy now share one caption (`content/rmlui/screens/win98_classic.rcss`, markup `w98-caption`): an 18dp title bar with the navy-to-blue gradient, bold 11dp white title, and a 16x14 caption Close button with the button border style. Inactive windows use the gray caption. Secondary windows carry no title-bar icon (PDF p.156), so the decorative marks were removed. The shared title-truncation logic still applies (`c-title-bar__title`, `c-title-bar__drag-handle`).

## Workshop property sheet

- **Caption:** "<name> Properties" (PDF p.163).
- **Size and pixel-exact scale:** the window is the book's standard property sheet, 252 x 218 DLU (Layout, "Size of Common Dialog Box Controls"), plus caption and frame: 384 x 380 px at 1x, fixed and not resizable (PDF p.159). The MS W98 UI outlines sit exactly on an 11 px grid, so the sheet's font snaps to 11, 22 or 33 physical pixels (1x below 1.5 density, 2x from 1.5, 3x from 2.5) and every sheet length is written in em of that font. Text, borders and spacing therefore land on whole pixels, and the window is sized to the same whole-number scale (`MainWindow::ensureDetachedManagement6A`). Check marks, option dots and arrows are exact Windows 98 glyphs drawn at 1x (`icons/w98-marks.tga`) and sampled with nearest filtering (`IngnomiaRmlUiRenderer`). At 1.5 density the sheet is 768 x 760 physical pixels, about 45% of the previous window's area.
- **Tabs:** unselected tabs rest on the page's top edge; the selected tab is 2 px taller and wider, overlaps that edge and has no line beneath it, so it joins its page.
- **Tabs (one row, same width, book-title caps; Controls: Tabs):** Craft, Queue, General, Stockpiles, Trade. Craft/Queue/Stockpiles appear only for crafting workshops (Stockpiles also needs a linkable stockpile); Trade only for a MarketStall. MarketStall opens on Trade, Butcher/Fishery on General. Hidden pages are rejected by the controller.
- **Pages never scroll.** Windows property sheets are fixed-size and split content across tabs instead of scrolling (PDF p.163-164, p.147). Settings that did not fit were split into General and Stockpiles. Only list boxes scroll, as documented. A focused test checks every page at the 640x420 minimum window and 100/125/150/200% density.
- **OK / Cancel / Apply outside the pages (PDF p.165-166; Layout "Grouping"):** OK is the default button (Enter), Cancel is Escape. All General and Stockpiles settings stay pending until Apply or OK; switching pages keeps them. Apply sends only what changed (basics, Butcher, Fishery, one link command per changed stockpile) and waits for the authoritative snapshot. OK closes only once that snapshot confirms the values; a rejection or conflict keeps the sheet open. Cancel discards and closes. Close (×) with pending changes asks "Do you want to apply the changes you made to <name>?" Yes / No / Cancel (PDF p.166). Validation errors use a message box.
- **Layout and metrics (Layout page):** 7 DLU margins (11dp), 4 DLU related spacing (6dp), 50x14 DLU command buttons (75x23dp), 14 DLU text boxes, 10 DLU check boxes and option buttons, group boxes with 11 DLU first-control offset; labels end with a colon (PDF p.138, p.142); sentence caps for labels, book-title caps for buttons, tabs and column headers (PDF p.328-330).
- **Borders (Design of Visual Elements):** window border (raised outer + inner) for pages, button border with the pressed sunken state and 1dp label shift, field border for text boxes/spin boxes/list boxes/drop-downs, grouping border for group boxes and the separator; disabled labels are engraved.

### Pages

- **Craft (WRK-02):** "Available crafts:" text box and single-selection list box; "New order" group with an order-type drop-down list (Craft number / Stock limit / Repeat), one labelled drop-down list per material, a Quantity spin box, a note naming any material that is short, and Add Order. One activation creates one order; invalid quantities and recipes are rejected.
- **Queue (WRK-03):** list view in details view (Order / Amount / State) with button-style column headers and Move Up / Move Down / Move to Top / Move to Bottom stacked beside it. The "Selected order" group edits the order type (drop-down), Quantity and "Move to bottom after each item" locally and sends them together with Update Order; Suspend and Cancel Order act immediately. Every command targets the selected stable job ID.
- **General (WRK-04, WRK-06):** Name and Priority (spin box, 1 = highest) above a separator; "Production" group (accept generated orders, auto-craft missing components) for crafting workshops; "Butchering" or "Fishing" group only for those workshops; "Status" group with "Suspend production" and Center on Map (a page command, right-aligned in its group).
- **Stockpiles (WRK-05):** multiple-selection list box drawn as flat check boxes (Controls: Multiple-Selection List Boxes, Figure 8.24; Visual Design: Flat Appearance). Checking a stockpile stages the link; focus stays on the same box after refresh; deleted stockpiles are ignored.
- **Trade (WRK-07):** two list views, "Merchant goods (you receive)" and "Settlement goods (you give)", each with Item / Available / Offer / Value. Offer quantity spin box and Set Offer target the selected stable row; the totals line shows both values and the balance. Refresh and Review Trade... are right-aligned; Review is disabled with a stated reason when nothing is offered or the settlement's side is worth less. The review message box lists both sides and irreversibility; Cancel exchanges nothing; the commit is bound to the reviewed merchant and trade revision. Offer one less/more were removed as redundant with the spin box.

## Fixes found by the live runs

- Craft and Queue tabs were shown for MarketStall/Butcher/Fishery; now only supported pages appear.
- Butcher option values leaked into the next opened workshop's snapshot (`AggregatorWorkshop::updateWorkshopInfo` now resets them).
- The trade ledger stayed empty until Refresh; it now loads when the Trade page opens.
- Review Trade silently did nothing when the settlement's offer was worth less; it is now disabled with the shortfall stated.
- Probe defects: a guessed material list, coarse timers reordering steps, and a capture taken after a page switch.

## Verification

| Gate | Result |
| --- | --- |
| Stage 10 focused suite | **10/10 tests; 125 checks.** Staged check boxes and links, one combined Apply, invalid-input message box, Close prompt (No discards), Cancel, OK after confirmation, draft retention by ID, Update Order, stable queue targeting, special options, trade offers/review/stale/duplicate, supported pages, no page overflow at the minimum window at four densities. [Summary](../evidence/stage-10/acceptance-final.txt), [verbose](../evidence/stage-10/acceptance-verbose.txt). |
| Earlier stage suites | Stage 04 4/4, 05 7/7, 06-09 10/10 each after the caption change. |
| Registered suite | 10/13; the same three pre-existing failures (see Findings). |
| Live copied-world runs | Four production runs at 100/125/150/200% on fresh 21-file Tutorial Valley copies: **40/40 checks each**, 10 captures each; captures are 384x380 px at 1x and 768x760 px at 2x. [Run summary](../evidence/stage-10/final/runs.json). |
| Caption | Production captures of Population, Military, Diplomacy (copied saves) and Inventory, Stockpile (fixtures). [Captures](../evidence/stage-10/caption/). |
| Save safety | All 21 source and copied save files unchanged after every run; no crash dumps. |
| Build | `Ingnomia.exe` SHA-256 `2E06760C694E7723A743F310BCDAB46C2667DC8F6E3EBD70059D4741168E7897`. |

## Visual evidence

- [Craft](../evidence/stage-10/final/craft-1.png), [Queue](../evidence/stage-10/final/queue-1.png), [General](../evidence/stage-10/final/settings-1.png), [Stockpiles](../evidence/stage-10/final/stockpiles-1.png)
- [Butcher](../evidence/stage-10/final/butcher-1.png), [Fishery](../evidence/stage-10/final/fishery-1.png)
- [Trade](../evidence/stage-10/final/trade-1.png), [unbalanced](../evidence/stage-10/final/trade-unbalanced-1.png), [review](../evidence/stage-10/final/trade-review-1.png), [after Cancel](../evidence/stage-10/final/trade-cancelled-1.png)
- 200%: [Craft](../evidence/stage-10/final/craft-2.png), [Trade](../evidence/stage-10/final/trade-2.png)

## Evidence limits

- Workshops, stockpiles and the merchant are probe-created on the copied save; normal construction and an event-spawned merchant were not exercised. No gnome performed a craft, butcher or fishing job.
- Input was injected through production RmlUi/Qt adapters and opt-in probes; physical mouse/keyboard is not claimed.
- The Stockpile window body still uses the Stage 09 model (staged name/priority, immediate options). Bringing it to the same property-sheet model is not part of this change.
- The MS W98 UI font is an unlicensed Microsoft-derived conversion used by owner decision; the rest of the game still uses Lato.

## Findings outside Stage 10

- `ui_foundation_rmlui_hot_reload` fails a stale source-order check (Stage 01 gallery helpers call `update()` before the frame loop's `processHotReload`); runtime order is correct. Stage 21.
- The unregistered `tests/ui-management6c` layout test expects pre-Stage-09 geometry and old workshop controls. Stage 21.
- Legacy route rules in `management6a.rcss` for removed workshop markup were deleted; older `#workshop_workbench` rules above them remain for Stage 21 clean-up.

## Next

Stage 11: Farms, groves and pastures with explicit targets.
