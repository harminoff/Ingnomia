# Stage 03 — Shared window chrome, typography, glyphs, and grouping

Updated: 2026-09-24

## Review correction

The Stage 01-03 review reopens this stage. Its original conclusions below are historical and include overstated claims: scroll reachability and empty-title fallback were not established. See [current review](01-03-review.md).

## Original status

Implementation and production-renderer scale checks are complete for the shared component scope. The full interaction exit gate remains open: this session had no physical pointer/keyboard surface for checking title recovery on hover, close/reopen, focus transfer between native hosts, or ordinary user-driven window movement. Existing synthetic inspector drag/close evidence is linked below and is not described as physical input.

## Implemented

- Shared frames use square corners, the Windows gray work surface, clear inset content regions, and restrained borders. The active caption is navy; the inactive caption is gray. The treatment follows Microsoft's documented raised/sunken border styles, grouping border, and title-bar command placement at the shared-pattern level; game-specific palettes and content surfaces remain with their owning stages. See [Border Style](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb226804%28v%3Dvs.85%29), [Group Boxes](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb246434%28v%3Dvs.85%29), and [Title Bars: Design Guidelines](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb226827%28v%3Dvs.85%29).
- The main application keeps its native Qt frame. Detached RmlUi windows own their frameless title bars and drag handles, so the document does not draw a second native caption. All current detached-window creation paths explicitly disable resizing; the decorative resize grip is hidden and no maximize/minimize buttons were added. The system frame remains responsible for main-window resizing.
- Shared title text uses the existing licensed Lato face and tokenized sizes: display 36dp, route title 26dp, section title 20dp, subheading 17dp, body/control 13dp, and metadata 12dp. Title text ellipsizes in the bar and has an overlay rule for focus/hover recovery while preserving space for Close.
- Title-bar Close controls receive a shared minimum 64×22dp target. Visible Close text remains the accessible/localized action name in production consumers; the fixture pairs that label with an aria-hidden multiplication-sign mark.
- Original RCSS border geometry supplies dropdown, spinner, sort, disclosure, and warning/close roles without adding an unlicensed font or binary icon pack. The gallery no longer uses the unsupported Lato checkmark/down-arrow characters that rendered as empty boxes. The close mark uses the bundled font's supported U+00D7 character; warning remains the text mark `!`.
- `.c-group-box` and `.c-content-region` are exercised in the shared gallery to separate light field/list surfaces from raised frames. Route-specific dense headings and groups remain owned by their feature stages.

## Verification

- `RelWithDebInfo` production target built successfully with the Visual Studio 2026 x64 environment; content staging completed.
- `cmake -DSOURCE_ROOT="$PWD" -P tests/ui-design-system/verify-design-system.cmake` passed.
- The production executable rendered the shared component fixture at 100%, 125%, 150%, and 200% UI scale in an isolated temporary data root. All four traces report `layout=pass`, nonzero body/header/grid sizes, and actual generated RmlUi `selectarrow/selectvalue/selectbox` and `slidertrack/sliderbar/sliderarrowdec/sliderarrowinc` parts. Captures and traces are in the [scale evidence manifest](../evidence/stage-03-components-scale-manifest.md).
- The gallery includes long truncated titles, active/inactive captions, an untitled-window fallback example, numeric labels, glyph roles, group/content regions, and a bounded scroll region. At 200% the full gallery is taller than the viewport and remains reachable through its scroll region; the screenshot is the initial viewport, not a claim that every lower section fits simultaneously.
- The Stage 00 live tile-inspector probe recorded synthetic close/reopen and detached-window drag behavior in the production host; see the [tile-route manifest](../evidence/stage-00-route-tile-live-manifest.txt). The component fixture itself is visual/static and does not establish action wiring or hover behavior.

## Remaining gates and ownership

- Verify full-title recovery on hover/focus, title-bar Close hit testing, active/inactive host transitions, and supported main-window resize with physical input when a native input surface is available. Detached inspectors are fixed-size by current implementation, so they have no resize affordance to test.
- The component glyph roles are implemented and rendered. Existing route-local scrollbar arrows and native select-arrow parts remain styled by their owning RCSS rules; consolidate their appearance with the shared roles during Stages 05–06 when those inputs and scroll regions are migrated. The generated parts were confirmed in the fixture, but their interaction or assistive-technology behavior was not inferred from that inspection.
- Remaining screen-specific small labels and dense groupings are recorded for their route stages; this stage does not claim a full-route clipping audit.

## Handoff

Stage 04 may proceed after review of the shared control primitives. Keep the physical-input checks open in the final verification matrix; do not describe fixture rendering or injected Qt events as physical interaction.
