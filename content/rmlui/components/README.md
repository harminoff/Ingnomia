# Shared RmlUi component contract

Wave 2 components are shallow class-based RML structures styled by
`styles/base.rcss`, `styles/components.rcss`, and the optional semantic variant
file `styles/accessibility.rcss`. The five structures whose document order must
stay identical across screens are RmlUi templates in `../templates/`.

The canonical fixture is `../fixtures/components.rml`. It covers:

- `c-panel`, `c-window`, `c-title-bar`, and `c-toolbar`;
- `c-icon-button`, `c-button`, `c-toggle`, `c-segmented`, and `c-tabs`;
- `c-list`, `c-table`, `c-tree`, and `c-scroll-region`;
- native `select`, text input, structural numeric stepper, range slider, and progress;
- `c-badge`, `c-status-chip`, `c-tooltip`, and `c-popover`;
- `c-modal`, `c-alert-row`, `c-state-panel`, `c-key-hint`,
  `c-context-action`, and `c-inspector-section`.

## Controller boundary

RmlUi 6.2 supplies native pointer/focus/checked/disabled state, select controls,
text controls, range controls, progress elements, templates, flex layout, and
overflow scrollbars. Controllers still own these behaviors:

- modal stacking, input blocking outside the top modal, focus trap/restore, and Escape;
- tooltip delay, viewport flip/clamp, anchor lifetime, and keyboard disclosure;
- popover open/close, outside click, focus return, and collision handling;
- tab activation policy and arrow/Home/End navigation;
- tree expansion, stable row identity, indeterminate calculation, and navigation;
- numeric parse/range validation plus increment/decrement dispatch;
- pending/error/stale classes, authoritative action result, and duplicate suppression;
- compact/standard/wide composition selection and platform-backed accessibility variants.

Classes do not encode routes, actions, domain IDs, visibility contracts, or
localized identity. Screen owners must not copy token values or add arbitrary
hex colors, timings, radii, layers, or control sizes.

## Accessible labels and icon policy

The first shared glyph set uses original typographic geometry (`+`, `-`,
`x`, `!`, `i`, arrows, checks, and diamonds) with visible text or `title`
labels. No external raster/vector icon pack is included. A glyph is never the
only carrier of severity or action meaning. Icon-only controls require a
controller-provided localized tooltip and accessible platform name before any
assistive-technology support is claimed.
