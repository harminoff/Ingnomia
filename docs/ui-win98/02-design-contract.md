# Windows 98 interaction and visual contract

This contract applies the Windows 98-era interaction model to Ingnomia’s current Qt/RmlUi game. It does not require pixel-for-pixel reproduction of the operating system. The map and authoritative game commands remain central; management screens use readable, compact property-sheet and report patterns.

## Historical references

- Microsoft’s historical Windows experience guide covers Windows 98 and Windows 2000. The [Microsoft Learn design-guidelines index](https://learn.microsoft.com/en-us/windows/win32/uxguide/) points to those earlier guidelines. A readable [2001 guide PDF mirror](https://blog-geofcrowl-static-images.s3.us-east-1.amazonaws.com/2020-02-17-collection-higs/MS-Windows-User-Experience-2001.pdf) is used as the direct reference; the PDF itself is hosted by a third party.
- [Microsoft property-sheet guidance](https://learn.microsoft.com/en-us/windows/win32/controls/property-sheets) describes tabs as random access to grouped properties and recommends a wizard for sequential tasks. It also describes a framed window with title bar and OK/Cancel, with Apply and Help when needed.
- [Microsoft title-bar guidance](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb226827(v=vs.85)) says to show only supported window commands and keep Close at the right end.
- [Microsoft dialog guidance](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb246466(v=vs.85)) calls for a clear way to leave a dialog and clear handling of pending changes.

## Ingnomia rules

1. **Property sheets for peer settings.** Use persistent tabs when a user may visit settings pages in any order. Use a staged flow only when the user must complete a sequence. Keep a stable title, visible current tab, and consistent action row.
2. **One clear window frame.** The host owns movement and resize behavior. Show only affordances that work in that host. Detached tools keep the map accessible and do not grow decorative minimize/maximize buttons without behavior.
3. **Selection differs from focus.** Selection names the game object or report row. Keyboard focus names the control that will receive keys. Keep both states visible and restore focus after detail/back or close operations.
4. **Visible, labeled commands.** Use short text labels for important actions; icons supplement labels. Destructive operations use the existing typed confirmation path and name the target.
5. **Explicit pending changes.** Drafts show changed values and offer a clear Apply/Cancel path. Immediate toggles remain immediate and report their resulting state. Never present a staged edit as applied before its command succeeds.
6. **Report controls keep their meaning.** Keep stable column identities, visible filter state, scroll position, selection, and item detail. Compact density must not hide quantities or make hits too small to use.
7. **Game state stays authoritative.** Views dispatch through existing controllers/command ports and reflect authoritative snapshots. Presentation redesign must not bypass revisions, target identity, world epoch, modal ownership, or scope checks.
8. **Adapt to real RmlUi.** The project pins RmlUi 6.2. RCSS supports `@media` queries; the verifier must not reject them as browser-only syntax. Generated controls use RmlUi’s actual pseudo-elements, including `selectbox`, `selectvalue`, `selectarrow`, and scrollbar parts such as `slidertrack` and `sliderbar`.

The gray classic-control direction is a visual reference, not permission to make the map, text, or action feedback less readable. Keep token ownership, accessibility/focus states, localization, and actual host behavior testable in the shared component layer.
