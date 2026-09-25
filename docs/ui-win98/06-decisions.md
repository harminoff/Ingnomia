# Implementation decisions

1. Keep this initiative in `docs/ui-win98/`; the existing `docs/ui-migration/` remains an independent migration record and is not overwritten.
2. Use the attached staged brief as the delivery order and preserve all 98 IDs and their original priorities. Historical scores are not acceptance results.
3. Treat the source revision in the attached audit as historical context only. Reconcile paths, route ownership, and behavior against the active `feat/separate-ui-windows` checkout.
4. Keep the game map and world commands authoritative. Use the established nested checkout, detached-window hosts, controllers, command ports, and EventConnector/snapshot flow.
5. Use Windows 98 property-sheet/report conventions for settings and management workflows, adapted for a game that keeps the world visible and uses the existing Qt/RmlUi hosts.
6. `@media` is a supported RmlUi 6.2 RCSS feature; the source verifier permits it. Browser-only layout declarations remain a separate compatibility decision and are not accepted just because they exist in web CSS.
7. Runtime fixtures stay opt-in through environment variables. The new detached inventory capture hook only runs for the inventory fixture when its capture path is explicitly supplied.
8. Keep synthetic fixture data clearly labeled and separate from live-world or save evidence. Never infer command parity from a screenshot.
9. Stage order remains enforced: complete each stage’s exit gate before beginning its dependent production redesign.

## Unresolved baseline decisions

- The separately supplied Windows 98 UI audit and triage index name the six risks; their current source dispositions are recorded in `stages/00-baseline.md` and `03-audit-traceability.md`.
- A disposable Tutorial Valley save was generated, loaded, copied, and reloaded in fresh processes. All 21 source/copy file hashes remained equal; see `evidence/stage-00-disposable-save-manifest.txt`.
- Native physical input is unavailable in the current computer-use surface. In-app probe evidence is useful but does not establish physical drag, keyboard traversal, or pointer delivery.

## Stage 05 decisions

Use a shared validated text editor for integer drafts because the pinned RmlUi forms implementation has no number type. Preserve legal catalogs, custom column filters and authoritative command ports. Stockpile displays rank 1 as highest and translates to zero-based priority. Use native checkbox/radio semantics with original geometric marks; mixed state is allowed only when supported by existing aggregate data. Stage 05 closes shared/pilot scope; downstream screen migration and integrated accessibility gates remain explicit in its checkpoint.

## Stage 06 decisions

Share report control geometry, option projection and keyboard navigation while retaining route-owned semantics. Preserve stable selection through sorting; clear a removed/filtered-out target instead of selecting a replacement. Convert scroll pixels using the current density before calculating virtual row indices. Keep Inventory flat and use a common horizontal parent for narrow report headers and bodies. Reset column clears typed and exact filters; All retains its exact-selection-only meaning. Performance evidence separates CPU layout/stub-render submission from production framebuffer proof.

## Stage 10 decisions

Show Craft/Queue only for workshops with a craft catalog (or queued orders) and Trade only for MarketStall; open special workshops on their only useful page instead of an empty Craft page. Keep Stage 09's retained-draft policy (drafts kept per object until world end) rather than a dirty-close prompt, so Close and object switches behave the same in both managers. Queue order-type buttons stay push buttons because each click commits a command; the new-order form uses radios because it edits a draft. Persistent per-order and special-production settings are checkboxes that send their checked value. Load the trade ledger automatically on first view and keep Refresh trade for re-reading the merchant. Disable Review with a visible reason instead of rejecting silently. Per-stockpile Locate is not added because the workshop context has no stockpile-locate command.

## Stage 10 revision - Windows 98 property sheet

The user directed that only documented Windows UI be used (docs/MS-Windows-User-Experience-2001.pdf and MSDN Visual Design ms997612). The workshop is now a property sheet: fixed pages that never scroll (content split into General and Stockpiles tabs), OK / Cancel / Apply outside the pages, every setting pending until Apply or OK, Close with pending changes asks Yes / No / Cancel, and errors use message boxes. This supersedes the earlier staged-name/immediate-option split for workshops; drafts are still kept per workshop ID when switching objects. Linked stockpiles use a multiple-selection list box with flat check boxes; tabular data uses list views in details view. All six managers share the Windows 98 caption with a caption Close button and no title-bar icon. Windows 98 surfaces use the owner-supplied "MS W98 UI" font (an unlicensed outline conversion of MS Sans Serif, recorded in fonts/notices/MS-W98-UI-PROVENANCE.txt); Lato remains the font elsewhere. The workshop sheet is fixed at 252 x 218 DLU and rendered at a whole-number scale (1x/2x/3x by density) with em-based metrics so the 11 px-grid font and all borders stay pixel-exact; the shared Review Trade/confirmation message box still uses the Stage 07 styling. The Stockpile body keeps the Stage 09 model until it is migrated.


## Stage 11 - agriculture property sheet

Agriculture uses the Stage 10 property-sheet model. Settings (name, suspension, work rules, default crop/tree/animal type, limits, butchering marks, food rules) are pending until Apply/OK; plot commands act immediately on the named selection. Apply ends with `agriculture.refresh` because most agriculture setters publish no snapshot. Live updates rebase untouched fields; edited fields changed elsewhere are conflicts. Pasture limits and foods depend on the animal type, so they are unavailable while a type change is pending and adopt the game's values once it is confirmed. Priority is omitted because the game does not implement it (NEW-003). The workshop, stockpile and agriculture sheets now use a Windows 98 message box (`modals/win98_message_box.rml`, Warning symbol, centered buttons, focused button carries the default outline); other screens move to it in their own stages. Design citations for Stages 11-21 are collected in `07-win98-design-reference.md`; the staged plan carries a Windows 98 design block per stage.

## Stage 12 - population property sheet

Population uses the property-sheet frame with Close as its only commit button, because skills and schedules take effect at once and professions keep an explicit Save Changes/Discard; OK/Cancel/Apply would falsely suggest pending changes. The roster is a list view with sortable headings; paging was removed because the list scrolls. A click selects and double-click/Enter/Properties opens a citizen (list view default command). A removed citizen leaves no selection instead of an arbitrary one. The profession editor uses the Customize Toolbar two-list pattern (the book has no transfer control). Message boxes name the object in the title and state the effect; unsaved profession changes ask Yes/No/Cancel. Schedule grid and citizen detail remain interim until Stages 13 and 16.

## Stage 13 - schedule grid

The schedule uses the Excel 97 grid model within the book's selection rules. The three original operations remain as selections (cell, citizen heading, hour heading) behind one Set Activity command whose exact scope is stated beforehand; any other rectangular range is sent as targeted cell commands. More than one affected citizen asks through a message box that counts offscreen cells. The system navy highlight replaces Excel's inverse video. Enter/Space no longer cycle a cell's activity; they run Set Activity for the stated scope.

## Stage 14 - military property sheet

Military uses five pages over the controller's three views (Squads/Members and Roles/Uniforms share views), with Close only because every military command applies at once. Transfers use the two-list pattern plus an explicit "Move to:" squad (the existing assign command moves from any squad), replacing previous/next-squad moves. The response to a target is a separate group of option buttons; ordering never changes it. Deletions use the shared message box titled with the exact name; the text does not claim what happens to members because that game behaviour is unverified. Drop-down lists are rebuilt through `runtime/SelectOptions.h` (setting a select's inner RML appends options). `connected_tabs::select` ignores tabs without `aria-controls`.

## Stage 15 - diplomacy and the Send Mission wizard

Diplomacy is a property sheet with Close only; mission planning is a simple wizard (three pages, no Welcome/Completion pages, per PDF p.305) opened by "Send Mission..." as a modal dialog over the sheet. The book's 317 x 193 DLU wizard template does not fit the 384 x 380 window, so the wizard fills the window with an 8 px inset. Every wizard page starts from a legal default (the controller's first eligible citizen). Finish revalidates the reviewed kingdom and mission and sends once. Facts the player does not know are shown as "Unknown".

## Stage 16 - inspectors as palette windows

The creature and object inspectors are property inspectors in palette windows (PDF p.167, p.180-181). They follow the selection, apply edits at once and have only a title-bar Close.

- **Size:** the same fixed 384 x 380 px as the sheets, which is within the palette limit of 263 x 263 DLU. They are not resizable, so pages never scroll.
- **One explicit commit:** changing a uniform needs Apply, because the change reaches every member of the role. The page states that scope first.
- **Undocumented controls replaced:** the book documents neither a paper doll nor a meter. The paper doll became a Slot / Item list view, and the need meters became values. Unreported values read "Unknown".
- **One citizen detail:** Population no longer keeps its own detail page. Properties opens the one creature inspector, and per-citizen skill switches stay on Population > Skills.
- **Left for Stage 20:** saving the palette position, and the Always on Top menu entry.

## Stage 17 - game window chrome

The game window is treated as the primary window:

- **Toolbar and status bar:** one toolbar along the top, and a status bar with panes along the bottom.
- **Toolbar buttons:** they have text labels instead of images. The book allows labels, and its standard images cover only common commands.
- **Orders:** the four order groups became drop-down menus. Map overlays became check items in a View menu.
- **Armed tools:** a tool shows in three places: the option-set button, the status bar, and a cross pointer only over the map.
- **Build:** the Build catalog is a fixed 384 x 380 palette with a list view.
- **Events:** events use an Information message box.
- **Not added:** the toolbar grip, docking, and balloon tips.

## Stage 18 - shell screens

- **Main menu:** the book has no title screen, so the main menu is a dialog box on the Windows desktop colour, with one column of commands, as for a dialog's commands.
- **Custom Game:** an advanced wizard at the book's wizard size (317 x 193 DLU).
  - The Welcome and Completion pages have a gradient watermark band, because the game has no artwork.
  - Finish appears only on Completion. Quick Start already covers "start with the defaults".
- **Load Game:** follows the Open dialog box. "Look in:" names the kingdom (a save folder), and the columns are Name, Modified and Version.
- **Not provided:** Delete on a shortcut menu and Save As, because the game has no command for them.
- **Tests:** the Stage 04 and 05 tests moved from the retired New Game tabs to the wizard. Their connected-tab checks remain on the shared fixture.

## Stage 19 - settings, pause, loading

- **Settings:**
  - Every setting takes effect at once, so the sheet follows the property-inspector model: Close, and no OK / Cancel / Apply.
  - Slider values are shown to the right, after the Windows 98 Display Properties slider. The book documents no numeric readout.
- **Pause:** a dialog over the paused map, not the desktop colour. The book has no pause screen.
- **Loading:** shows the game's step text instead of a progress bar, because the game reports no fraction.
- **Leaving a kingdom:** Main Menu and Exit use Yes / No message boxes. The book's Yes / No / Cancel save prompt needs a save-then-leave command the game does not have.

## Stage 20 - keyboard and accessibility

- **Access keys:** use explicit `accesskey` attributes with an underlined letter.
  - A plain letter acts only inside Windows 98 windows when the focus does not take text. Outside them, letters stay with the game's own keys.
  - Message box answers get their key at run time.
- **High Contrast:**
  - The setting is read from Windows at start-up and then every second.
  - It is applied to every document by the host and uses the High Contrast White colours, which keep the black glyphs readable.
- **Palettes:** they reopen where the player left them, saved in the configuration.
- **Not provided:** Always on Top, because owned palettes already stay above the game window.

## Stage 21 - cleanup, migration and localization

- **Stockpile and Inventory:** migrated to the Stage 10 standard (owner decision, 2026-09-25).
  - Stockpile rules become pending check boxes like every other sheet setting. This removes the need for a bulk review dialog, because Cancel undoes them.
  - Templates stay immediate because they are saved copies held by the game. They are unavailable while rule changes are pending.
  - Inventory is a Close-only report. Item detail moves to a subordinate Item Properties sheet instead of in-place navigation.
- **Text and access keys:** every Windows 98 string comes from the catalog (owner decision). The access key is written into the string with "&", as in Windows resources, so a translation chooses its own letter. The shared entries live in `UiTextWin98Entries.inc`, included by all three catalogs.
- **Validation:** an entry out of range in a spin box is reported in a message box naming the field and range, not in red text beside it.
- **Numbers in list views:** headings of numeric columns are right-aligned (PDF p.143).
- **Row sprites:** item sprites are dropped from Stockpile and Inventory rows. The Windows 98 lists elsewhere are text-only, and the sprite frames did not fit the 16 px row.
