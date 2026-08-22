# Cave interface design system

Status: **normative visual and interaction design contract; no implementation or distributable asset is included**

Baseline: upstream commit `4f99266c0f95faa847ca0db2af18cf59aff07f4b`

Companion specifications: `04-ui-data-contracts.md` and `05-ux-specification.md`

## Intent

The interface should feel cut, joined, and maintained by a practical underground settlement. The visual language uses basalt and charcoal fields, slate working surfaces, aged iron structure, restrained bronze emphasis, and warm torchlight focus. Mineral colors identify categories sparingly. Dwarven character comes from proportion, material, rhythm, edge treatment, and deliberate motion—not tiny ornamental type, fantasy runes, neon glow, noisy texture, or copied art.

This document owns visual-token names, component state rules, and class conventions. It does not authorize browser-only CSS assumptions: every RCSS property/pseudo-class must be checked against pinned RmlUi 6.2. All authored dimensions use `dp` under the physical-framebuffer contract in `03-rmlui-architecture.md`; raw `px` is limited to reviewed texture/diagnostic cases.

## Naming contract

### Token names

RCSS custom-property support must be verified before implementation. Regardless of storage mechanism, the semantic names below are canonical in the design-token source and generated styles.

```text
color.<role>[.<state>]
space.<step>
size.<role>
radius.<role>
border.<role>
shadow.<role>
type.<role>.<property>
motion.<role>.<property>
layer.<role>
```

Components consume semantic roles such as `color.surface.panel`; they do not hard-code palette values. High-contrast and debug variants replace semantic roles without changing component markup.

### CSS classes

Use a shallow, component-oriented convention:

```text
c-<component>                    component root
c-<component>__<part>            owned part
c-<component>--<variant>         stable visual variant
is-<state>                       transient state mirrored by controller/model
u-<single-purpose-layout-rule>   rare layout utility
```

Examples: `c-button`, `c-button--danger`, `c-table__row`, `c-tool is-active`, `c-field is-invalid`, `c-panel--debug`. Do not encode routes, `ActionId`s, data identity, visibility rules, or game meaning in class-name string parsing. Prefer native RmlUi pseudo-classes for `:hover`, `:active`, `:focus-visible`, `:disabled`, and `:checked` where supported; use `is-*` only for semantic states the model/controller owns.

## Material hierarchy

The material system has four depths. A new panel may not invent a fifth arbitrary card style.

| Depth | Purpose | Surface | Edge/depth rule |
|---|---|---|---|
| 0: void/world | map and full-screen basalt background | `surface.void` / live world | no decorative frame; HUD root remains pointer-transparent |
| 1: ledge | top bar, tool shelf, hint strip | `surface.ledge` | one iron edge facing the world; shallow shadow only |
| 2: chamber | inspector, menu, workbench, drawer | `surface.panel` | iron outer border + slate inner keyline; 8dp radius maximum |
| 3: inset | table well, field, selected editor, code/diagnostic value | `surface.inset` | darker recessed surface, 1dp inner shadow/keyline |

Modals use chamber material over a scrim, not an additional ornate material. Cards are allowed only for bounded choice groups or empty/error messages; primary information architecture uses lists, tables, sections, and docks instead of giant card grids.

## Color roles

Values are initial sRGB targets for prototype validation, not proof of final monitor/render output. Contrast must be measured from actual framebuffer captures.

### Neutral and structural palette

| Token | Value | Use |
|---|---:|---|
| `color.surface.void` | `#101315` | menu void and deepest backing |
| `color.surface.basalt` | `#171B1D` | shell/background |
| `color.surface.ledge` | `#1D2326` | persistent HUD chrome |
| `color.surface.panel` | `#252D31` | inspector/workbench/menu panels |
| `color.surface.raised` | `#2D373B` | raised control and active section |
| `color.surface.inset` | `#13191B` | fields/table wells |
| `color.surface.scrim` | `rgba(5, 7, 8, 0.76)` | modal input blocker |
| `color.iron.dark` | `#30373A` | outer structure |
| `color.iron` | `#596166` | standard border/divider |
| `color.iron.light` | `#7A858A` | high-emphasis divider, disabled text boundary |
| `color.bronze.dark` | `#6D4B2E` | selected structural edge |
| `color.bronze` | `#A16E3E` | active/selected non-focus accent |
| `color.bronze.light` | `#C69456` | selected text/icon accent |

### Text palette

| Token | Value | Use / minimum expectation |
|---|---:|---|
| `color.text.primary` | `#F1EEE6` | headings and body; >=4.5:1 on used surface |
| `color.text.secondary` | `#C8C7BF` | labels/metadata; >=4.5:1 for body-sized text |
| `color.text.muted` | `#9FA4A1` | nonessential metadata only; never disabled-only semantics |
| `color.text.inverse` | `#121617` | text on light focus/warning fills |
| `color.text.disabled` | `#7F8583` | paired with disabled shape/border; >=3:1 where practical |
| `color.text.link` | `#E5B96D` | actionable inline link, always underlined on focus/hover |

### Focus, selection, and interaction palette

| Token | Value | Use |
|---|---:|---|
| `color.focus.ring` | `#FFD27A` | keyboard focus ring; never reused as a category color |
| `color.focus.inner` | `#1A1C1D` | dark separator inside light ring |
| `color.hover.surface` | `#344046` | hover surface delta |
| `color.pressed.surface` | `#182024` | pressed/inset surface |
| `color.selected.surface` | `#493A2B` | selected row/tab/tool background |
| `color.selected.border` | `#D19A55` | selected structural border |
| `color.pending.surface` | `#34383A` | pending mutation surface with activity mark |

Focus and selection are independent. A selected row that receives keyboard focus shows the bronze selected fill **and** the amber focus ring.

### Semantic palette

| Role | Strong | Surface | Border | Redundant marker |
|---|---:|---:|---:|---|
| information | `#71B7D3` | `#19323B` | `#4E9AB6` | circle with `i`, solid line |
| success | `#77BE78` | `#1D3525` | `#569D5C` | check, double-short line |
| warning | `#E0B45D` | `#3B301A` | `#B98A36` | triangle/exclamation, diagonal hatch |
| danger | `#E27B70` | `#3C201F` | `#B7554E` | octagon/exclamation, cross hatch |
| opportunity/notice | `#A9A0D8` | `#2A2740` | `#8177B6` | diamond/star, dot-dash line |

Semantic colors never tint large parts of the world or panel without a text/icon/shape legend. Error is a failed operation/system condition; danger is a meaningful risky/destructive action. Warning does not stand in for pending.

### Mineral category accents

Mineral accents are identifiers, never required meaning:

| Token | Value | Suggested category |
|---|---:|---|
| `color.category.quartz` | `#B9B6CF` | inspect/general |
| `color.category.ochre` | `#C49B59` | dig/terrain |
| `color.category.copper` | `#B77452` | build/production |
| `color.category.malachite` | `#6CA47A` | agriculture |
| `color.category.azurite` | `#688FB6` | designation/zone |
| `color.category.garnet` | `#A96870` | military/job urgency |

Use one accent on a 3dp category bar, small icon field, or active marker. Do not fill whole workbenches with category color. Magic has no neon violet treatment; while unavailable, it uses neutral disabled presentation.

## High-contrast palette behavior

High contrast is a variant of semantic roles, not a second layout.

- replace textured/transparent surfaces with opaque `#000000`, `#161616`, and `#242424` fields;
- primary text becomes `#FFFFFF`, secondary text `#E6E6E6`;
- standard borders become at least 2dp and `#BFBFBF`;
- focus uses a 3dp `#FFDE59` outer ring plus 1dp black separator;
- selected state adds both 3dp border and leading bar/pattern, not fill alone;
- semantic strong colors are adjusted to meet 4.5:1 for normal text or 3:1 for large text/graphics against adjacent fills;
- shadows, gradients, transparency-dependent separators, and texture are removed;
- world-overlay opacity increases only with pattern/outline redundancy and never obscures essential map geometry.

Because the baseline has no persisted high-contrast setting, implementation may initially follow an accepted platform/config signal. The Settings UI must not show a dead switch.

## Typography hierarchy

Use a highly legible UI sans family already licensed/staged or an equivalently licensed replacement selected during asset review. The shared package stages the pinned RmlUi sample `LatoLatin-Regular.ttf` under the SIL Open Font License 1.1, with its authoritative notice beside the binary. Its family name and runtime load path are covered by the RmlUi spike; glyph coverage and long-string fixtures remain part of the localization gate. Decorative/pixel faces may be used only for the title wordmark at display size after legibility review. No rune-like or ornamental font is used for body, buttons, tables, numbers, or tooltips.

| Role/token | Size | Weight | Line height | Use |
|---|---:|---:|---:|---|
| `type.display` | 36dp | 700 | 44dp | title only |
| `type.route_title` | 26dp | 700 | 34dp | screen/workbench heading |
| `type.section_title` | 20dp | 650 | 28dp | panel section / modal heading |
| `type.subheading` | 17dp | 650 | 24dp | tabs, grouped editor title |
| `type.body` | 15dp | 400 | 22dp | default prose/value |
| `type.body_strong` | 15dp | 650 | 22dp | row primary value |
| `type.control` | 14dp | 600 | 20dp | button/tab/input label |
| `type.meta` | 13dp | 400 | 18dp | timestamps/secondary metadata |
| `type.compact` | 12dp | 550 | 16dp | dense table metadata only |
| `type.numeric` | 14dp | 550 | 20dp | tabular numbers; enable tabular figures if font supports them |

### Body-text legibility rules

- Default body is never below 15dp; dense metadata never below 12dp at 100% accepted UI scale.
- Paragraph measure is 45-75 Latin characters, capped near 70 for dialogs/help. Tables are exempt but cell copy remains concise.
- Left align prose. Do not justify. Headings may use slight positive letter spacing; body tracking remains normal.
- Use real font weights, not outlined/glowing text or color changes as a substitute.
- Body copy wraps. Table primary labels wrap to two lines in standard density when needed; dense mode ellipsizes and exposes full text on focus/tooltip.
- Use tabular figures for quantities, time, coordinates, counts, and aligned columns. Never rely on color to distinguish negative/positive values.
- Avoid all caps except a short development-only banner. Buttons and headings use sentence/title case according to localization convention.
- A panel may not place small text directly over the world; it requires an opaque-enough ledge/panel backing with verified contrast.
- Player-authored text is untrusted layout input: preserve UTF-8, escape markup, wrap, and cap visual extent without truncating stored values.

## Spacing scale

Base rhythm is 4dp. Components use only named steps unless a reviewed optical correction is documented.

| Token | Value | Typical use |
|---|---:|---|
| `space.0` | 0 | reset |
| `space.1` | 2dp | icon optical adjustment, not target separation |
| `space.2` | 4dp | tight inline gap |
| `space.3` | 8dp | compact control gap/cell padding |
| `space.4` | 12dp | standard inline/field gap |
| `space.5` | 16dp | panel padding, section gap |
| `space.6` | 24dp | major group separation |
| `space.7` | 32dp | route section separation |
| `space.8` | 48dp | title/menu composition only |

Rules:

- Panel outer padding: 16dp compact, 20-24dp standard/wide.
- Form label-to-control gap: 8dp; field-to-field: 16dp; section-to-section: 24dp.
- Table cell horizontal padding: 12dp standard, 8dp compact, 16dp comfortable.
- Adjacent icon-only controls have at least 4dp visual gap and independent >=36dp hit boxes.
- Indented trees use 16dp per level but cap visible indentation at 48dp; deeper ancestry uses connector/label breadcrumbs so content is not squeezed away.

## Size, target, radius, border, and shadow tokens

### Controls and targets

| Token | Value |
|---|---:|
| `size.control.compact` | 32dp visual / 36dp hit target |
| `size.control.standard` | 40dp |
| `size.control.primary` | 44dp minimum |
| `size.row.compact` | 32dp minimum |
| `size.row.standard` | 40dp minimum |
| `size.row.comfortable` | 48dp minimum |
| `size.scrollbar` | 12dp visual / 20dp interaction region |
| `size.inspector.min` | 320dp |
| `size.inspector.max` | 460dp |
| `size.modal.min` | 320dp |
| `size.modal.max_text` | 680dp |

### Radius

| Token | Value | Use |
|---|---:|---|
| `radius.sharp` | 2dp | table cells, structural insets |
| `radius.control` | 4dp | buttons/fields/chips |
| `radius.panel` | 8dp | chambers/modals |
| `radius.pill` | 999dp | status badge only; never major navigation |

The silhouette stays squared and weighty. Excessively soft, floating, consumer-app pill layouts are not part of the cave language.

### Borders and depth

- `border.hairline`: 1dp `iron.dark` for internal rules.
- `border.standard`: 1dp `iron` plus an optional 1dp inner dark keyline.
- `border.strong`: 2dp `iron.light` for active structural separation.
- `border.selected`: 2dp `selected.border` plus a 3dp leading category/selection bar.
- `border.focus`: 2dp `focus.ring` outside + 1dp `focus.inner` separator; 3dp outer in high contrast.
- `shadow.ledge`: `0 2dp 6dp rgba(0,0,0,.32)`.
- `shadow.panel`: `0 8dp 24dp rgba(0,0,0,.42)`.
- `shadow.modal`: `0 16dp 48dp rgba(0,0,0,.58)`.

Do not stack shadow and glow. Shadows indicate layer; borders define interaction/structure. Inset wells use a top/left dark keyline and bottom/right subtle slate line, never a high-gloss bevel.

## Iconography

Icons are original or properly licensed project assets. No Dwarf Fortress, RimWorld, or other third-party art/icon is copied or traced.

| Role | Glyph box | Hit target |
|---|---:|---:|
| inline/status | 16dp | inherited row target |
| standard control | 20dp | 36-40dp |
| primary tool/category | 24dp | 44dp |
| modal severity/empty state | 32dp | noninteractive or 44dp if action |

Rules:

- Use consistent 2dp optical stroke at the standard 20-24dp size, squared joins with limited chamfering, and simple filled/outlined shapes that survive small rendering.
- An icon-only interactive control requires an accessible name, keyboard focus, and tooltip. Primary workflows prefer icon + text.
- Do not stretch raster icons. Provide appropriate-resolution source or vector support only if the selected RmlUi rendering path supports it and its license/build cost is accepted.
- The pinned GL3 backend directly guarantees uncompressed TGA, not PNG/JPEG. Runtime imagery must use proven supported TGA or an accepted decoder/plugin. Legacy cross-thread PNG buffers are not an asset contract.
- Missing icons fall back to a neutral category mark plus text; they never render an invisible control.

## Texture and decorative detail

Texture budget is deliberately low:

- Basalt/slate texture contrast variation <=4% luminance and scale >=64dp; no high-frequency speckle.
- Texture may appear on depth-0/1 large surfaces or panel headers, never behind body/table text, fields, tooltips, or semantic messages.
- One subtle hammered/engraved detail per major panel edge is the maximum. No corner filigree grid.
- Bronze wear/patina is structural trim only, not text coloring or full-panel noise.
- Gradients, if supported and visually verified, are <=6% luminance range and serve depth only.
- Glow is reserved for a brief focus acquisition or critical event at low radius/opacity; the steady state is a border, not a halo.
- High contrast and reduced visual complexity variants remove all texture.

Any new texture must be original or license-approved, listed in asset notices, and tested from the installed `content/rmlui` layout. This document creates none.

## Component state grammar

All interactive components implement a compatible state set. If a state is impossible for a component, its test is marked not applicable rather than silently omitted.

### State priority

When states combine, presentation priority is:

```text
disabled > error/danger > pending > pressed > focus-visible > selected/checked/active > hover > rest
```

Focus-visible is never hidden by selected/checked; it composes as an outer ring. Error may compose with focus using error border plus the focus outer ring. Pending preserves a readable selected value but suppresses repeat activation.

### Buttons

| State | Visual | Behavior |
|---|---|---|
| rest | raised panel, iron border, primary text | action available |
| hover | `hover.surface`, border lightens one step | pointer only; no layout movement |
| focus-visible | rest/selected visual + dual focus ring | shown for keyboard/spatial focus |
| pressed | inset surface, content translates <=1dp if supported | fires on valid activation/release per component contract |
| disabled | flat basalt/disabled text, dashed or muted border, no shadow | absent from tab order where framework semantics support it; blocker remains accessible nearby/tooltip |
| pending | pending surface + small activity mark; label retained | duplicate request suppressed; not claimed successful |
| selected/toggle on | selected surface, bronze border, check/state label | state exposed semantically |
| danger | neutral rest with danger border/icon; danger fill on hover/confirm | avoids making destructive action visually primary by default |

Primary buttons use bronze structure and warm text, not a solid neon fill. Only one primary action per modal/form region.

### Fields, selectors, sliders, and steppers

- Rest: inset surface, standard border, explicit persistent label.
- Hover: border lightens; label does not move.
- Focus-visible: dual focus ring around the entire control.
- Invalid/error: danger border + icon + inline text connected to the field; value is preserved.
- Disabled: semantics and reason remain readable; never masquerades as ordinary muted text.
- Read-only: inset surface with lock/read-only label, selectable text where useful; visually distinct from disabled.
- Dirty draft: 3dp bronze leading marker and route-level unsaved summary.
- Slider always has a numeric value, unit, valid range, and keyboard step behavior. Color is not the only indication of track position.
- Combo/listbox exposes current selected text; unknown IDs display an explicit missing-data state and do not silently select the first option.

### Checkboxes, radios, and tri-state trees

- Checkbox states are unchecked, checked, indeterminate, disabled, pending, and focused.
- Check uses a visible mark; indeterminate uses a horizontal bar; both have text labels.
- Inventory hierarchy checking means watched state. Stockpile hierarchy checking means allowed contents. Labels must make those different meanings explicit.
- Parent indeterminate state is computed from stable child state. Updating one branch does not visually reset unrelated expansion/focus.
- Radio groups use arrow navigation and expose one selected value; no selection is permitted only when the schema contract permits it.

### Tabs

- Use a horizontal bar at standard/wide, vertical/step rail when compact composition needs it.
- Selected tab uses a 3dp bronze underline/leading bar plus stronger text; hover uses surface only; focus adds outer ring.
- Tabs are not pill chips. Arrow keys move focus; activation behavior (automatic or manual) is consistent within all workbenches and documented in component tests.
- Hidden/unavailable tabs are omitted, not empty. Applicable inspector sections appear only for actual variant data.

### Tool controls

Active tool is stronger than ordinary selection:

- category accent bar + selected bronze border;
- persistent text `Active: <name>` in the hint strip;
- phase label (choosing, materials, preview, dragging);
- visible Cancel/Inspect action;
- repeat/rotation state when contracted;
- world overlay/cursor uses matching category shape/pattern, not color alone.

Unavailable tool shows a blocker if authoritative; commands without proven handlers are omitted or clearly unavailable and cannot dispatch.

## Information density modes

Density changes row/cell padding and secondary disclosure, never base text below its minimum or target semantics.

| Mode | Row | Cell padding | Default use |
|---|---:|---:|---|
| comfortable | 48dp | 12dp x 16dp | forms, main menu, first-run, compact sequential detail |
| standard | 40dp | 8dp x 12dp | default workbench and inspector |
| compact | 32dp | 4dp x 8dp | expert tables/queues only, with >=36dp interactive hit region |

Density preference has no baseline persistence contract, so implementation begins with standard and may choose comfortable automatically at compact/high-scale composition. A Settings row appears only after persistence is added. Modals and destructive actions never use compact targets.

## Tables, lists, trees, matrices, and queues

### Shared table/list style

- Header uses raised/inset contrast, `type.control`, 2dp bottom rule, and remains sticky where RmlUi support is proven.
- Rows alternate only a <=3% surface delta; zebra striping never carries meaning.
- Selected row: bronze leading bar + selected fill + selected border where contiguous layout allows.
- Hover: row surface delta. Keyboard focus: cell/row focus ring according to navigation mode.
- Numeric columns right-align; names left-align; boolean/state columns center only with a text/accessible name.
- Sort header shows direction icon plus accessible “ascending/descending.” No icon means unsorted.
- Loading/empty/error is one full-width semantic row or panel, not a blank table.
- Stable selection remains by ID through patch, sort, and filter. If filtered out, the selection status says so; it never silently selects a new row.
- Row actions are visible on focus as well as hover and remain keyboard reachable. Common actions may be persistent; destructive actions are not hidden behind hover alone.

### Tree rules

- Disclosure chevron has 36dp target, expanded/collapsed semantics, and arrow key navigation.
- Indentation increments 16dp and caps at 48dp. A connector/compact ancestor label preserves hierarchy beyond that.
- Parent check state is separate from expansion state.
- Long labels may wrap to two lines in standard/comfortable; compact uses ellipsis with focus tooltip.
- Lazy/load state appears inline beneath the parent and maintains focus identity.

### 24-hour schedule matrix

- Sticky gnome-name column and sticky 0-23 hour header; horizontal scroll is required rather than microscopic cells.
- Each cell is >=32dp visual and >=36dp interactive, carries activity text abbreviation plus redundant pattern/icon, and exposes full label on focus.
- Arrow keys move cell; Home/End first/last hour; Page Up/Down previous/next gnome; Space chooses activity; row/column batch operations use explicit commands.
- Focus and current row/column headers highlight together. Selection coloring does not obscure activity pattern.
- At compact composition, the matrix remains a full-width dedicated tab with no side detail pane.

### Workshop queues and reorderable lists

- Queue order number is visible. Move Up/Down buttons are always available; pointer drag is optional enhancement only.
- Drag handle, if implemented, has a keyboard equivalent and does not become the only grab target.
- Pending move retains old authoritative position with a pending marker until confirmed; it does not optimistically reorder domain state.
- Craft mode (Number/To/Repeat), count/target, materials, pause, and blocker are text-visible. Progress/ETA remains absent until contracted.

## Panels, inspectors, workbenches, and cards

- Inspector width is 320-460dp according to effective width. It docks right at standard/wide and becomes sequential/full-height at compact.
- Workbench uses one chamber with internal panes, never multiple overlapping floating windows.
- Pane split handles, if supported, are 8dp visual/20dp hit targets and keyboard adjustable. Otherwise use defined breakpoint widths.
- Close/Back stays in the top trailing corner and in document order after heading, never off-screen.
- Section dividers use spacing + one hairline; avoid nested boxes for every field.
- Cards are limited to main-menu choices, preset summaries, modal options, or empty/error callouts. Dense data uses rows/tables.
- Full-viewport HUD document roots are visually empty and `pointer-events: none`; ledges/panels opt into interaction.

## Modal behavior and visual rules

- Scrim is full viewport, opaque enough to separate world motion, and always owns pointer/wheel input.
- Modal chamber width is `min(680dp, viewport - 32dp)` for text and may reach `960dp` for an explicitly accepted structured decision. Height is clamped with an internally scrollable body.
- Heading and response group never scroll out together; on short viewports, body scrolls between sticky header/footer.
- Event prompt uses neutral/information frame unless its typed data gains severity in a future contract. Do not infer severity from prose.
- Destructive confirmation uses danger icon/border, names object/consequence, and makes Cancel the safe initial focus. Confirm requires deliberate activation and may not be triggered by Escape.
- Required events cannot dismiss by backdrop or Escape. Dismissible error/confirm dialogs may use Escape according to `ModalEntry.dismissOnEscape`.
- On open: save stable focus token, focus safe action/heading, announce heading/body. On close: restore token or deterministic route fallback.
- Background is input-inert and excluded from focus/navigation while the modal exists.

## Tooltip behavior

- Tooltips supplement, never replace, visible labels, field errors, or required blocker messages.
- Contents: name; consequence/description; blocker when provided; current hotkey/modifiers if actually active; units/source timestamp when relevant.
- Pointer delay: 450ms first tooltip, 100ms when moving among adjacent controls within a 1.5s grace period. Keyboard focus: show after 300ms or via a dedicated help key accepted by the input contract.
- Hide on pointer exit, focus loss, activation, route/modal change, scrolling the anchor out of view, or Escape when no higher-priority UI owns it.
- Clamp to viewport with >=8dp margin; prefer below/right, then flip. Never cover the focused control or active placement footprint when another side fits.
- Maximum width 360dp, body text 13-15dp, wrapped. Long content becomes inline Help/detail rather than a scrollable tooltip.
- Tooltips are noninteractive unless implemented as a separate popover component with explicit focus management.
- Reduced motion shows/hides immediately. High contrast uses opaque black/white with 2dp border.

## Feedback, status, and errors

- **Inline validation:** adjacent to field/action, danger border + icon + text.
- **Action pending:** retain label/value, add small activity mark and “Saving/Updating” accessible status; suppress duplicates.
- **Action rejected:** return control to enabled state, show typed reason near control, keep entered draft.
- **Action accepted:** authoritative state change is the primary proof. A brief success message may supplement it but does not replace it.
- **Route error:** bounded semantic panel with heading, typed detail, Retry only if supplied, Back/Close always.
- **Fatal document/resource error:** built-in `doc.error_fallback` with no external font/texture dependency, keyboard-operable Back/Exit, and logged logical path.
- **Stale/refreshing:** existing data remains but shows “Refreshing”/stale timestamp where available; destructive actions may be disabled according to contract.
- **World unload:** remove world actions/values immediately and show transition state. Dead controls never remain clickable.

The baseline has no general notification history. Do not create a permanent toast/history system under design-system authority.

## Motion and transition timing

Motion communicates layer/state and remains short.

| Token | Duration | Use |
|---|---:|---|
| `motion.instant` | 0ms | required input shield, focus, validation state |
| `motion.fast` | 80ms | hover/pressed/color/border |
| `motion.standard` | 140ms | inspector section, dropdown/popover |
| `motion.route` | 180ms | route/workbench fade/short slide |
| `motion.modal` | 160ms | scrim/modal opacity + <=8dp settle |
| `motion.emphasis` | 240ms max | one-shot accepted action/critical attention |

Default easing is a decelerating curve approximating `cubic-bezier(.2,.8,.2,1)` only if pinned RmlUi supports it; otherwise use a supported ease-out. Press uses immediate/fast response. Nothing loops except a compact loading/pending indicator. No parallax, bouncing panels, torch flicker behind text, camera-like overshoot, or perpetual glow.

### Reduced motion

- Set travel distance to zero and duration to `instant`, except optional <=80ms opacity where it prevents disorientation.
- Loading uses a static/progressively updated textual or stepped indicator rather than rotation/pulse when possible.
- Focus, selection, error, and modal blocking appear immediately.
- World overlay changes crossfade only if <=80ms; otherwise swap.
- Title presentation skips directly to the actionable menu.

## Responsive breakpoints and composition tokens

Breakpoints are based on effective dp width after density ratio and UI scale:

| Token | Threshold | Composition |
|---|---:|---|
| `breakpoint.compact` | `<800dp` | sequential panes; maximized workbench; wrapped two-row shell/tool rail |
| `breakpoint.standard` | `800-1399dp` | normal shell; one dock; one/two workbench panes |
| `breakpoint.wide` | `1400-2199dp` | expanded dock; two/three panes if minimum widths survive |
| `breakpoint.ultrawide` | `>=2200dp` | centered capped edge groups/workbench; map receives extra width |

Composition constraints:

- minimum useful list pane 280dp; minimum detail pane 320dp; minimum main map width with dock 640dp at standard mode;
- top ledge preferred height 48-64dp, may wrap to 96dp at compact but must not cover center excessively;
- bottom hint strip 32-48dp; expanded tool selector may reach 240dp but collapses after tool activation;
- inspector never exceeds 40% of viewport width in standard/wide; at compact it becomes sequential rather than a tiny map + tiny dock;
- workbench caps near 1600dp effective width unless a schedule/table explicitly benefits from more; readable prose panels cap lower;
- modal viewport margin 16dp compact / 32dp standard;
- tooltip margin 8dp.

## UI scale and high-DPI behavior

- RmlUi context dimensions, viewport, clipping, and pointer injection use physical framebuffer pixels.
- `dp_ratio = devicePixelRatio * user_ui_scale`; authored component/text/layout values use `dp`.
- UI scale triggers recomposition at effective breakpoints; it is not a screenshot enlargement of a fixed 1400x700 layout.
- On resize/screen/DPR/scale change, preserve selected stable IDs and sensible scroll anchors, reflow, clamp overlays/tooltips/modals, then restore focus. Never retain raw element pointers.
- Pixel snapping may be applied to 1dp borders/icons after physical scaling, but must not shift hit testing away from rendered geometry.
- Images choose native resolution/fit without aspect distortion. Font rendering and pointer alignment require actual 100%, 125%, 150%, 200% monitor/runtime proof where available.
- Required screenshots: 1280x720, 1600x900, 1920x1080, 2560x1440, 3440x1440 at relevant accepted scale values. Specifically test 1280x720 at 200% and ultrawide at 100%/150%.
- Current upstream exposes 50/75/100/150/200 while the plan requests 80/100/125/150/200. The settings contract owner must resolve supported options; the design system validates every exposed option and may not assume the discrepancy away.

## Keyboard navigation behavior by component

| Component | Keys |
|---|---|
| global document | Tab / Shift+Tab follow semantic order; Escape follows the normative stack; F6-like region cycling only if added to the active input contract |
| buttons | Enter/Space activate; no repeat for destructive/non-idempotent action |
| tabs | Left/Right (or Up/Down vertical), Home/End; consistent manual/automatic activation |
| menu/tool rail | arrows move spatially; Enter/Space choose; Escape collapses/cancels; typeahead/search only in a real text field |
| list/table | Up/Down rows; Left/Right cells/panes as declared; Home/End first/last; Page keys viewport; Enter opens/details; Space toggles selection/check where applicable |
| tree | Up/Down visible node; Right expand/child; Left collapse/parent; Space check/watch/filter; Enter open/details |
| slider/stepper | arrows step; Page keys larger step; Home/End min/max when safe; numeric value announced |
| schedule grid | arrows cell; Home/End first/last hour; Page Up/Down gnome; Space activity chooser; typed batch commands for row/column |
| queue | arrows select; explicit Move Up/Down shortcuts/buttons; no pointer-only drag |
| modal | focus trapped; Tab cycles; Escape only when dismissible; safe initial focus; Enter activates focused action only |

Spatial navigation uses `nav: auto` only as a baseline. Dense toolbars, tables, schedules, and split panes require explicit neighbor behavior and wrap/no-wrap decisions. Tabbable elements use `tab-index: auto`; focus appearance uses `:focus-visible`. Hidden/inert/background elements are removed from navigation.

## Debug visual system

Development-only UI uses the same legible components with unmistakable separation:

- `c-panel--debug`: 3dp alternating ochre/danger top rail, opaque charcoal surface;
- persistent `DEVELOPMENT TOOLS - MUTATES SAVE` heading;
- spawn/mutation actions use danger variant and explicit target text;
- diagnostic values may use a licensed monospace face at >=13dp, never the normal player body face switched globally;
- no release route/document/action registration, no hidden zero-opacity debug button, and no reuse of debug accent for player categories.

## RmlUi and asset implementation constraints

- Verify every RCSS property, pseudo-class, transition, gradient, shadow, filter, mask, and navigation feature against pinned 6.2; replace unsupported browser assumptions with supported structure/style.
- Expressions in this document such as `min(viewport, cap)`, sticky headers, custom-property-like token names, shadows, gradients, and cubic-bezier easing describe outcomes, not literal browser CSS. Implement unsupported outcomes with registered breakpoints, controller-computed dimensions, extra structure, or supported RCSS primitives.
- Use templates for shared panel chrome, button state grammar, table headers, modal shell, tooltip, empty/error state, and focus ring, without hiding document order.
- Centralize all values in the token source/shared RCSS. Screen files do not introduce arbitrary hex values, dimensions, timings, radii, or z/layer numbers.
- Runtime images resolve by logical `AssetId` beneath staged `content/rmlui`; no absolute paths or traversal.
- Built-in renderer support does not imply PNG/JPEG. TGA is the current guaranteed route; any decoder or plugin needs an explicit architecture/license/test decision.
- Font faces load before documents. Missing required face/glyph logs clearly and falls back to a tested legible family rather than blank squares.
- Asset failures preserve text labels and interaction. Decorative imagery never controls layout size or route availability.
- Accessible names, roles, states, modal exposure, and announcements require an accepted RmlUi/Qt/platform accessibility capability or bridge. RML element labels alone are not proof of operating-system accessibility; screen-reader behavior must be tested before it is claimed.

## Visual acceptance matrix

For each component/screen family, capture and inspect:

| Axis | Required cases |
|---|---|
| states | rest, hover, pressed, focus-visible, disabled, selected/checked/indeterminate, pending, error, loading, empty, stale |
| composition | compact, standard, wide, ultrawide; inspector open/closed; tool collapsed/expanded; workbench list/detail |
| scale | every exposed setting, with mandatory 100/125/150/200-equivalent validation and current 50/75 discrepancy resolved |
| text | shortest/longest locale strings, UTF-8 player name, missing glyph, multiline event, long item/material/save name |
| data | zero, one, many, 100+ rows, deep tree, 24-hour matrix, queue reorder, rapidly patched count |
| access | keyboard-only focus path, grayscale, common color-vision simulations, high contrast, reduced motion |
| world boundary | opaque controls block map; intended gaps click through; wheel/drag/right-click ownership; tooltip/modal clamp |
| failure | missing document, font, icon, texture, localization key, rejected action, stale epoch, load error |

Acceptance checks:

- body and secondary text contrast measured from actual captures;
- focus ring unmistakable on every surface and when selected;
- no meaning from color/texture/icon alone;
- no clipping, overlap, hidden Back/Close, off-screen dialog/tooltip, microscopic text, stretched icon, or giant unused panel;
- no excessive map obstruction at default play and no transparent HUD capture;
- no giant repeated card grid, neon sci-fi color, excessive glow, decorative clutter behind text, or borrowed/copyrighted composition;
- transition timing and reduced-motion substitutions verified, not inferred from static RCSS;
- framebuffer/render proof includes real font/icon/texture decode and installed-package paths.

## Token implementation checklist

- [ ] one canonical token source generates/imports all shared RCSS values;
- [ ] neutral, text, interaction, semantic, category, high-contrast, and debug roles implemented;
- [ ] typography faces/weights/glyph fallbacks licensed, staged, and runtime-proven;
- [ ] spacing, targets, rows, icons, radii, borders, shadows, layers, and timings centralized;
- [ ] buttons, fields, toggles, tabs, tools, rows, trees, schedules, queues, modals, tooltips, and errors cover full states;
- [ ] compact/standard/comfortable densities preserve text and target minimums;
- [ ] breakpoints recompose by effective dp under UI scale/DPR;
- [ ] modal focus/scrim, tooltip clamp, and pointer-transparent HUD rules exercised;
- [ ] high contrast and reduced motion replace visual behavior even before settings persistence is surfaced;
- [ ] semantic markers retain meaning in grayscale/color-vision simulations;
- [ ] release package contains only accepted/licensed runtime assets in supported formats;
- [ ] missing-resource fallback remains legible and operable.

## Discovery handoff

1. **Assigned objective.** Define a complete cave visual system with typography, legibility, color, texture, spacing, structure, component states, density, tables/lists, modal/tooltips, motion, accessibility, breakpoints, UI scale, and keyboard behavior.
2. **Files and systems inspected.** `plan.md`; migration documents 01-05; RmlUi architecture constraints; upstream asset/font inventory and contract-described input/state/lifecycle boundaries.
3. **Findings.** A restrained material hierarchy and semantic state grammar can create a distinctive cave identity without sacrificing map dominance or text legibility. Physical-pixel/DPR, pointer transparency, limited guaranteed texture formats, and missing preference backing constrain implementation.
4. **Decisions and rationale.** Semantic roles replace arbitrary screen colors; four material depths replace card sprawl; squared/chamfered structure, warm focus, limited bronze/mineral accents, and sparse texture provide identity; focus/selection/severity remain redundant and accessible.
5. **Files changed.** `docs/ui-migration/06-design-system.md` only for this artifact.
6. **Commands run.** Read-only source/doc searches and documentation patch tooling; final Markdown/contract/diff checks are required at handoff.
7. **Build/test result.** Documentation-only. No RmlUi render, contrast capture, runtime asset decode, keyboard test, or build is claimed.
8. **Visual evidence.** Token tables and component/state specifications only. No third-party screenshot, texture, icon, or copyrighted visual was copied or created.
9. **Unresolved risks.** RCSS feature support must be checked against pinned RmlUi; the pinned Lato face, TGA/asset paths, and dependency notices now have source/static plus explicit-MSVC spike proof; glyph coverage, preference backing, and UI scale discrepancy remain; token values require framebuffer contrast and usability iteration.
10. **Patch boundary.** This file only; no commit requested or created.
11. **Unrelated changes.** None intentionally made; shared discovery/spike/source artifacts were preserved.
