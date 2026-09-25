# Review of Stages 01-03

## Reference

Microsoft links its original Windows 98/2000 guide from the [historical design index](https://learn.microsoft.com/en-us/windows/apps/design/guidelines-overview). The [Design of Visual Elements chapter](https://learn.microsoft.com/en-us/previous-versions/ms997615(v=msdn.10)) supplies the border, state and focus reference.

## Confirmed findings and corrections

- Gallery widths were tailored to a 3231-pixel viewport. Replaced desktop-specific fixed widths with viewport-relative sizing and responsive columns. Percentage/auto child widths resolved to zero in the current host; those unsuccessful attempts are not acceptance evidence.
- RmlUi has no HTML default stylesheet. Missing block layout on divs left white regions collapsed, selected list labels invisible, and table content clipped. Added low-specificity block defaults and explicit table display rules. The production capture now shows selected rows and all table cells.
- Inline templates do not inherit their template body's class. The framed-window gallery consumer now explicitly owns its c-window class.
- Replaced unsupported fixture disclosure/check characters and supplied an original TGA directional atlas for generated dropdown and scrollbar controls, including existing route-local caret decorators.
- Close width is now a minimum with natural text sizing, allowing longer localized labels.
- Report headers use gray control faces. Group edges use shadow/highlight treatment, pending close visual inspection.
- Strengthened the production fixture probe to check viewport containment at 640 by 480, 1280 by 900 and actual host dimensions, plus programmatic scroll-to-end. At 100 percent all checks passed. This is not physical-input evidence.
- Current production build, design-system verifier and whitespace check passed.

## Interim gates (resolved or qualified in the outcome below)

- Complete high-scale renderer runs and source-pinned evidence manifest.
- Review title recovery, empty-title behavior and active/inactive host transitions. The prior fixture paragraph was not an empty-title implementation.
- Check production Inventory, Settings and inspector sentinels after shared default/style changes.
- Verify precise double-edge frame geometry, glyph alignment and high-scale reachability; do not call the entire theme pixel-identical to Windows 98.
- Native physical input remains unavailable in this session. Keep this separate from injected events and renderer geometry checks.

At that interim checkpoint, Stage 04 was pending. Historical Stage 01-03 manifests are retained without being relabeled as current proof.

## Review outcome

Stage 01's shared fixture and verifier, Stage 02's shared theme/cascade, and Stage 03's shared chrome now pass the reviewed automated scope. Original completion language remains superseded by this scoped result.

- Final 100/125/150/200 percent runs all pass generated-part, localized empty-title, pointer-driven long-title recovery, 640/1280 host containment and programmatic scrolling assertions. The corrected title recovery is conditional on measured truncation; the Inventory sentinel caught and prompted correction of unconditional hover expansion.
- Final scale captures/traces and exact source hashes: [review evidence](../evidence/review-01-03/final/source-hashes.json).
- [Corrected production Inventory](../evidence/review-01-03/inventory-caption-corrected.png) retains one caption row and unobscured report headers. Its actual combo and scrollbar arrows now use the original glyph atlas.
- [Settings](../evidence/review-01-03/shell/settings.png) and [stockpile inspector](../evidence/review-01-03/stockpile-primary.png) opened through production bindings with isolated fixture data. The latter is a primary-host inspector capture, not a detached capture. Their route-owned blue palette remains assigned to later route stages.
- The production build, design-system verifier, Management 6B RML contract and inspector RML contract pass. Corrected two stale contracts: accessibility must load after route styles; scroll arrows must use the actual atlas rather than require caret text.
- Detached focus events update caption state; a render-time check now also synchronizes newly loaded/hot-reloaded documents. Native physical host switching/drag/close is still unverified, and earlier injected inspector evidence has not been relabeled as physical.

The retained Lato font is an explicit licensed substitute, not the Windows 98 system font. This result establishes the shared Windows 98 visual language and the stated runtime behaviors, not pixel identity across every route. Disabled button state/focus/default distinction proceeds in Stage 04; checkbox/radio styling in Stage 05; reports in Stage 06; complete route migrations remain downstream.

Stage 04 may now proceed with those limits recorded.
