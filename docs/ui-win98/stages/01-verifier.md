# Stage 01 checkpoint — verifier preflight

Date: 2026-09-24

## Changes

- Updated `tests/ui-design-system/verify-design-system.cmake` to remove RCSS block comments before active palette/style assertions.
- The verifier now reads named semantic color values from `styles/tokens.json` with CMake JSON parsing and requires them in active shared RCSS. A legacy palette comment can no longer satisfy the check.
- Removed the blanket `@media` rejection. Pinned RmlUi 6.2 documentation lists media-query support for width, height, aspect ratio, resolution, orientation, and theme.
- Refreshed the smoke asset SHA-256 values to match the current committed `smoke.rml` and `smoke.rcss`; the integrity assertions remain active for all three smoke assets.
- Added opt-in `INGNOMIA_AUTOMATE_UI_FIXTURE_DETACHED_CAPTURE_PATH` support for the inventory fixture. It asks the detached inventory host to capture through its own renderer and does not change normal launches.
- Added an explicit `components` fixture selector that loads `fixtures/components.rml` in the primary production RmlUi host. It is opt-in through `INGNOMIA_AUTOMATE_UI_FIXTURE=components`.
- Inspected the pinned RmlUi 6.2 implementation: selects generate `selectarrow`, `selectvalue`, and `selectbox`; scrollbars generate `slidertrack`, `sliderbar`, `sliderarrowdec`, and `sliderarrowinc`. Replaced the fixture's invalid `track`/`slider` selectors with the engine's actual `slidertrack`/`sliderbar` selectors.
- Corrected every active `scrollbarvertical track/slider` rule in shared, inspector, population and management-window styles. Expanded the verifier from the shared scroll region to all production RCSS files, stripping comments before checking so an inert example cannot trigger a false failure.
- Added a fixture-only runtime assertion that inspects the actual RmlUi context tree and `ElementScroll`/`ElementFormControlSelect` APIs. It reports option count and generated part tags in the existing automation trace.
- Changed the component gallery to use the production viewport with four columns at wide sizes, then three, two, or one column at narrower breakpoints. RmlUi's documented inline-block display and supported media queries keep the fallback explicit; responsive narrow sizes remain Stage 20 verification.

## Verification

- Focused verifier: passed.
- Negative control: replacing active `#252d31` uses in a temporary copy while retaining the legacy comment failed on `color.surface.panel` as intended.
- Negative control: deleting `is-invalid` from the temporary component fixture failed on the required fixture feature as intended.
- Negative control: changing the real generated `slidertrack`/`sliderbar` selectors to nonexistent `track`/`slider` failed on the required scrollbar selector.
- Route-level negative control: adding a `track` selector to `windows/management6c.rcss` failed with the invalid generated-part diagnostic; an equivalent selector inside an RCSS comment passed.
- Production `Ingnomia` target rebuilt successfully with the documented Visual Studio developer environment.
- Repeated the registered CTest suite after the asset rebuild: 11/13 passed. The same two current checkout mismatches remain (`ui_foundation_registry` counts/`suspend_job`, and the wiring test's tile-selection phrase); the design-system source verifier and all other registered tests pass.
- Inventory fixture opened and rendered through the production detached host in two separate launches. Captures were byte-identical, SHA-256 `8091d7cad157f66028c29b579024b7a843136a02fc15a9bd9d4b0700b1aab33c`.
- Before the viewport reflow, the fixture's first captures exposed that its 560dp column left most of the frame unused and placed lower controls outside the screenshot. The subsequent responsive reflow and accepted two-run proof are recorded below. The dialog/modal example remains static markup.
- Opened the live HUD event-prompt modal from two fresh copies of the tutorial save. Both traces show the database-backed prompt handoff and response; the [paired captures](../evidence/stage-01-modal-live-manifest.txt) confirm the same dialog structure. The data-driven prompt amount differs between runs, so these images are not expected to be byte-identical. The typed Exit cancel/confirm path is separately covered by the [Stage 00 confirmation manifest](../evidence/stage-00-confirmation-exit-manifest.txt).
- Updated the gallery layout to span the renderer width. The [full-viewport manifest](../evidence/stage-01-component-full-manifest.txt) records two 3231×1631 production-host captures; both generated-part assertions pass and the PNGs match byte-for-byte. All listed component states fit within one captured frame without scrolling.
- Repeated Stockpile and shell Settings sentinels on two isolated launches each. Both pairs of production-host PNGs match byte-for-byte; Settings also reports the same dispatched values and isolated config snapshot. The [sentinel manifest](../evidence/stage-01-repeat-sentinels-manifest.txt) links each capture and trace.
- Repeated the live four-plot Farm bulk-plan sentinel on two fresh copies of the same 21-file Tutorial Valley save. Both probes selected the same two plot IDs, verified two planned/two untouched plots and two live updates, preserved all save hashes, and produced byte-identical detached captures. The scheduler created zero planting jobs in each short run; the manifest records this limit.
- The Inventory detached fixture already has two byte-identical production-host captures; the live copied-save Space-watch trace separately confirms one `watch.set` path and an authoritative revision change. The creature inspector was opened on three fresh save copies for Camera, Stats, and Expertise; the live event prompt repeated on two copies. The typed Exit cancel/confirm trace is linked from Stage 00.
- Re-ran the focused verifier after the gallery CSS change; it passed. `git diff --check` passed (Git reported only configured LF-to-CRLF notices for modified files).
- Rebuilt the current default target set and reran all 13 registered CTest cases: 11 passed. The same two checkout-level failures remain: `ui_foundation_registry` expects outdated tool/action counts and no `suspend_job`, while `ui_foundation_mainwindow_rmlui_wiring` expects a stale inspector-visibility phrase. These are unchanged from Stage 00 and neither targets the verifier or component fixture.

## Gate

**VERIFIED.** Stage 00's baseline inventory and traceability gate is satisfied. The component fixture and generated select/scrollbar checks pass repeatably; the full-viewport gallery, Inventory, Stockpile, two-of-four Farm scope, Settings, inspector, and modal sentinels have repeat production evidence. The typed Exit cancel/confirm trace is also covered. Verifier negative controls and the current focused verifier pass. The 13-case CTest rerun has the same 11/13 result and same two unrelated checkout-level failures already recorded at Stage 00. Physical input remains outside the available runtime surface, and narrow breakpoint behavior is assigned to Stage 20.

Stage 01's preflight gate is closed. Continue with Stage 02; retain the two CTest baseline failures in the integrated verification record.
