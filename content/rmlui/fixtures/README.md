# Shared component fixture

`components.rml` is a non-routable Wave 2 fixture. It demonstrates all required
shared component structures and representative rest, hover-capable,
focus-capable, checked/selected, disabled, pending, error, loading, empty,
warning, danger, and modal visuals. It contains illustrative copy only and does
not dispatch game actions.

## Bounded checks

Run the dependency-free source check:

```powershell
cmake -P tests/ui-design-system/verify-design-system.cmake
```

To render through the already-built isolated Qt/OpenGL spike without changing
its source or the Wave 1 smoke files:

1. Copy the complete `content/rmlui` tree into a temporary copy of the spike's
   `assets` directory.
2. Copy `fixtures/components.rml` to that temporary asset root as `spike.rml`.
3. Copy the spike executable and Qt runtime beside the temporary asset root.
4. Run `ingnomia_rmlui_qt_spike.exe --self-test` and inspect the emitted
   `spike-framebuffer.bmp` plus log.

This proves only that the fixture document, templates, native controls, styles,
font selected by the spike, and clipping render in the isolated fixture host.
It does not prove full-game navigation, data binding, action dispatch, modal
focus trapping, tooltips, world click-through, packaged font licensing, or the
final resolution/scale matrix.

## Manual state matrix

- Tab through every native button/input/select/range and verify focus remains visible.
- Hover and press buttons, list/tree rows, select arrow/options, slider, and scrollbar.
- Check/uncheck toggle/tree examples and distinguish checked from focus.
- Inspect selected + focus, pending, invalid, disabled, warning, and danger combinations.
- Scroll the page and bounded ledger; ensure controls remain clipped to their regions.
- Resize across compact/standard widths; this fixture wraps for inspection but does not stand in for screen breakpoint controllers.
- Apply `is-high-contrast` or `is-reduced-motion` to the body in a temporary build copy and inspect semantic redundancy.
