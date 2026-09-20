# Stationary canvas during layer changes

The wheel handler was not explicitly panning. All world vertex shaders project
height relative to `uRenderMax.z`, so changing the visible layer shifted every
existing tile by 20 unscaled pixels (60 screen pixels at zoom 3).

`MainWindowRenderer::setViewLevel` now cancels that shift in the camera Y offset
before publishing the new level. Wheel and HUD layer controls share this path;
the actual clamped level delta is used, including multiple events before a
frame. The upper limit is the final valid tile index, `dimensionZ - 1`.
Zoom bindings, selection geometry, item ghosts and job coordinates are unchanged.

## Verification

- Canonical RelWithDebInfo game build succeeded.
- `ui_foundation_placement_math` and `ui_foundation_mainwindow_rmlui_wiring` passed.
- The math regression checks stationary projection and inverse picking at
  several zooms, all four rotations, both directions and clamped boundaries.
- An isolated runtime fixture delivered 56 application-level wheel events
  through `MainWindow::wheelEvent`. All passed: level changes, same-frame bursts,
  four rotations, three zooms, both boundaries and both configurable bindings.
- Actual OpenGL captures at layers 95 and 94 have **0 changed pixels out of
  6,000** in a lower-level terrain patch exposed in both frames. The upper layer
  disappears while the lower terrain stays stationary.

Evidence: `.verification/layer-scroll-stationary/result.txt`,
`layers-before.png` and `layers-after-down.png` in the same directory.
These are scripted application-event and framebuffer checks, not physical
mouse acceptance. The test needed process cleanup after completing its checks
and requesting shutdown; clean exit is not claimed.

To repeat, set `INGNOMIA_PLACEMENT_PROBE` to a new absolute capture directory,
`INGNOMIA_DATA_FOLDER` to an isolated profile beneath it, and
`INGNOMIA_LAYER_SCROLL_PROBE=1`, then run the canonical executable. No user saves
are loaded or changed. Expect no `FAIL` lines and `COMPLETE layer-scroll checks`.

Executable built 2026-09-13 at 17:48:58 local time, SHA-256:
`1BFF09E7886EE89A4885928B7C60039538DF73D1EF37B0955700D7316F6ECF95`.
