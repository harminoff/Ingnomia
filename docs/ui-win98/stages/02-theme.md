# Stage 02 checkpoint — theme tokens and cascade

Date: 2026-09-24

## Decision

The production normal theme is `windows-98-classic`. Its shared defaults use the Windows 98 system-color pattern: a gray window face, white editable/inset surfaces, black text, navy active captions and selection, white bevel highlights, gray bevel shadows, and black dark edges. Feedback roles retain readable, distinct foreground/background pairs. These are the project's starting values, based on the supplied plan; they are not presented as a complete list of colors mandated by Windows 98.

The historical Microsoft interface guidance describes raised and sunken controls, pressed command buttons, disabled appearance, and visible keyboard focus. It also recommends using system-color roles so custom controls follow the chosen interface colors. This stage applies those principles with the generated palette below. [Microsoft: Design of Visual Elements](https://learn.microsoft.com/en-us/previous-versions/ms997615%28v%3Dmsdn.10%29)

## Single token source

`content/rmlui/styles/tokens.json` holds the named, flat `rcss_tokens` map. The authoring files `base.rcss.in`, `components.rcss.in`, `management_window.rcss.in`, and `accessibility.rcss.in` refer to semantic names such as `color.surface.window`, `color.text.selection`, and `color.border.highlight`.

`cmake/GenerateUiTheme.cmake` expands those names to ordinary RCSS. The four checked-in `.rcss` files are the deterministic generated source copies; the `stage_content` build step runs the same generator into the build's runtime `content/rmlui/styles` directory after copying assets. The generator uses CMake 3.16-compatible string parsing, so this does not raise the project's CMake minimum. RmlUi does not resolve browser CSS custom properties, so no runtime `var()` layer is used.

| Role | Token | Value |
|---|---|---|
| Desktop | `color.surface.desktop` | `#008080` |
| Window face | `color.surface.window` | `#c0c0c0` |
| Editable/inset surface | `color.surface.inset` | `#ffffff` |
| Active selection | `color.surface.selection` | `#000080` |
| Primary text | `color.text.primary` | `#000000` |
| Selected text / caption text | `color.text.selection` / `color.caption.text` | `#ffffff` |
| Bevel highlight / shadow / dark edge | `color.border.highlight` / `.shadow` / `.dark` | `#ffffff` / `#808080` / `#000000` |
| Active caption | `color.caption.active` | `#000080` |
| Disabled text | `color.text.disabled` | `#808080` |
| Hover | `color.interaction.hover` | `#d4d0c8` |
| Focus | `color.interaction.focus` | `#000000` |
| High-contrast focus | `accessibility.focus` | `#ffff00` |

Normal feedback tokens cover information, success, warning, danger, and notice roles. The feedback examples pair text and a marker with color so state does not depend on color alone. High-contrast and reduced-motion declarations stay in `accessibility.rcss.in`; they are opt-in root-class styles, not a claim that new Settings preferences were added.

## Cascade map

| Consumer | Ordered stylesheets | Purpose / owner |
|---|---|---|
| Main shell and registered shell screens | `base.rcss` → `components.rcss` → `screens/shell.rcss` where used → `accessibility.rcss` | Shared defaults; shell surfaces and wizard layouts remain owned by `screens/shell.rcss` until Stages 18–19. |
| Game HUD | `base.rcss` → `components.rcss` → `screens/hud.rcss` → `accessibility.rcss` | Shared defaults then HUD layout/route styling; Stage 17 owns remaining HUD palette migration. |
| Inspector | `base.rcss` → `components.rcss` → `screens/inspector.rcss` → `accessibility.rcss` | Shared defaults then tile/creature inspector; Stage 16 owns its remaining route palette. |
| Orders tools | `base.rcss` → `components.rcss` → `screens/hud.rcss` → `screens/orders_tools.rcss` → `accessibility.rcss` | HUD foundation, detached-tools layout, then accessibility overrides. |
| Management window | `base.rcss` → `components.rcss` → `styles/management_window.rcss` → owning route sheet → `accessibility.rcss` | Shared detached frame, then management-family layout and state. The route sheet is linked by the concrete window RML. |
| Shared fixtures and destructive modal | `base.rcss` → `components.rcss` → `accessibility.rcss` | Shared component and modal styles without a later route stylesheet. |

The link order is enforced by `verify-design-system.cmake`: any RML document that links `accessibility.rcss` must link it last, after its route sheets. Concrete management documents place it after `management6a.rcss`, `management6b.rcss`, or `management6c.rcss`; the reusable management template does not insert an earlier copy.

Bindings switch state classes such as `is-selected`, `is-active`, `is-disabled`, and `is-hidden`; the inspected production bindings do not generate RGB colors through inline styles. Inline `style` attributes in the component fixture set only test geometry and spacing. Route-specific authored stylesheets remain the source of their current route overrides.

## Remaining route owners

The older route palettes are still active in those owners and are not exposed as selectable alternate themes. They are explicit downstream migration work, not fallback declarations in the shared token files.

| Stylesheet owner | Route family | Planned stage |
|---|---|---:|
| `screens/shell.rcss` | Main menu, new/load game, pause, Settings | 18–19 |
| `screens/management6a.rcss` | Inventory, Stockpile, Workshop, Agriculture | 8–11 |
| `windows/management6b.rcss` | Population, schedule, inventory window-specific rules | 8, 12–13 |
| `windows/management6c.rcss` | Military and Diplomacy | 14–15 |
| `screens/inspector.rcss` | Tile and creature inspectors | 16 |
| `screens/hud.rcss`, `screens/orders_tools.rcss` | HUD and detached tool window | 17 |
| `developer_ui/designer_overlay.rcss` | Developer-only overlay | 21 cleanup |

The `classic_park` palette and inert legacy-token comment were removed from the shared token/base sources. The shared component stylesheet now uses the generated token set for its surfaces, text, selection, bevels, feedback, and control states. Route files listed above still contain explicit colors until their feature stages migrate them.

## Verification and evidence

- `cmake -DSOURCE_ROOT="$PWD" -P tests/ui-design-system/verify-design-system.cmake` passed. It regenerates each shared sheet twice, compares the outputs with the checked-in RCSS, changes `color.text.primary` in a disposable token copy, and confirms the changed value reaches base, component, and management-frame outputs.
- The production `RelWithDebInfo` build passed and staged generated RCSS into the runtime content directory.
- The component gallery opened in the production RmlUi host; RmlUi reported generated select parts `selectarrow,selectvalue,selectbox` and scrollbar parts `slidertrack,sliderbar,sliderarrowdec,sliderarrowinc`. See [capture](../evidence/stage-02-components-final-v3.png) and [trace](../evidence/stage-02-components-final-v3-trace.log).
- The shell Settings route transitioned from `shell.main_menu` to `shell.settings` and captured after the final shared and route styles loaded. See [capture](../evidence/stage-02-settings-route.png) and [trace](../evidence/stage-02-settings-route-trace.log).
- The detached Stockpile fixture rendered the shared management frame, route styling, real item icons, and report rows. See [capture](../evidence/stage-02-stockpile-detached.png) and [trace](../evidence/stage-02-stockpile-fixture-trace.log).
- The final matrix captured all 21 supported surfaces successfully; screenshots and traces are in [stage-02-route-smoke-final](../evidence/stage-02-route-smoke-final/). A separate 21/22 probe also attempted the `kingdom_panel` plan, but its configured `hud_open_kingdom` id did not activate (`activated=false`). This is an automation/action-coverage gap, not a failed stylesheet load; it remains open with HUD feature work. See the [matrix manifest](../evidence/stage-02-theme-manifest.txt).
- Accessibility precedence is source-checked across concrete RML documents. Physical keyboard/mouse operation and high-contrast/reduced-motion preference wiring were not tested in this stage.

## Exit gate

The shared token source, supported generation path, shared-sheet load order, and accessibility precedence are documented and verified. Route-specific palette migration remains explicit in the table above and is owned by the later feature stages.
