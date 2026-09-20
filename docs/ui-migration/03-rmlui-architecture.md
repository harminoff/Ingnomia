# RmlUi integration architecture

Status: **proposed for orchestrator acceptance; broad UI conversion is blocked on this gate**  
Baseline: upstream `https://github.com/rschurade/Ingnomia.git`, branch `feat/rmlui-interface-rebuild`, commit `4f99266c0f95faa847ca0db2af18cf59aff07f4b`  
RmlUi target: release 6.2, immutable commit `2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`  
Scope: Qt 6 `QWindow`, Qt-owned OpenGL 4.3 core context, current world renderer, and the replacement RmlUi host

## Executive decision

Ingnomia should use one RmlUi context composited over the world in the existing Qt-owned OpenGL context. Qt remains the platform authority: it creates and owns the window and `QOpenGLContext`, receives native input, owns the clipboard/cursor/IME, schedules redraws, handles DPI changes, and swaps buffers. RmlUi owns UI document layout, UI focus, UI pointer interaction, and UI rendering only. The game renderer continues to own world rendering and camera/selection behavior.

Use RmlUi 6.2 from its exact tag commit through a pinned CMake `FetchContent` declaration, with an offline source override. Link `RmlUi::Core` and `RmlUi::Debugger`. Compile the tagged official `Backends/RmlUi_Renderer_GL3.cpp` behind a small Ingnomia adapter, using Ingnomia's existing GLAD loader rather than introducing a second OpenGL loader. Do not use an RmlUi window-system backend: none owns a Qt `QWindow`, and a second native backend would compete with Qt for the window, event loop, context, and presentation.

The minimal spike is isolated under `spikes/rmlui-qt-gl/`. It is not part of the normal game build and does not alter the root build. Its results are evidence for this ADR, not permission to mass-convert screens.

## Evidence from the current upstream application

This architecture is derived from the checked-out upstream sources, not from an assumed engine shape.

- Root `CMakeLists.txt` requires Qt 6 Core/Xml/Sql/Gui/Widgets and OpenGL, creates one `Ingnomia` executable, and currently links Noesis through `NoesisApp`.
- `src/main.cpp` sets a global OpenGL 4.3 core `QSurfaceFormat`, enables shared Qt contexts, opts into fractional high-DPI factors, creates a `QApplication`, and constructs one `MainWindow`.
- `src/gui/mainwindow.cpp:106` makes `MainWindow` a bare `QWindow` with an OpenGL surface. Its OpenGL context is created lazily on first exposure at `src/gui/mainwindow.cpp:925`.
- `MainWindow::initializeGL()` makes that context current, loads GL entry points through the repository's generated GLAD 4.3 loader, constructs `MainWindowRenderer`, initializes the world renderer, and only then initializes the current UI.
- `MainWindow::paintGL()` draws the world first, renders the current UI second, and calls `QOpenGLContext::swapBuffers()` once at the end (`src/gui/mainwindow.cpp:837`).
- `MainWindowRenderer::paintWorld()` renders to the active/default framebuffer, deliberately resets common state at frame start, and unbinds shader programs and the VAO after its passes (`src/gui/mainwindowrenderer.cpp:602`).
- Window-local mouse coordinates are currently used for GUI hit testing and selection routing, while `glViewport` uses `width * devicePixelRatio` and `height * devicePixelRatio` (`src/gui/mainwindow.cpp:880`). This mismatch must become an explicit coordinate contract.
- UI-versus-world routing currently lives in `MainWindow`: UI is offered keyboard and pointer events, and world camera/selection actions run only when the UI does not own the gesture (`src/gui/mainwindow.cpp:248`, `440`, `506`, `552`, `606`). This responsibility must stay in the window host.
- `EventConnector` and its aggregators are the existing Qt signal/slot boundary to the game thread. RmlUi callbacks should call a presentation/controller adapter that dispatches through that boundary; RmlUi must not invoke game objects directly.
- Baseline CMake configuration reached compiler detection but stopped at missing Qt 6. No RmlUi runtime result can honestly be claimed in this environment until Qt 6 is supplied.

## Selected component boundary

```text
Qt event loop / MainWindow
  |-- owns QWindow, QOpenGLContext, DPR, cursor, clipboard, IME, swap
  |-- translates native input to UiInputRouter
  |-- preserves world drag/click/camera routing
  |
  +-- MainWindowRenderer
  |     `-- draws world to default framebuffer
  |
  +-- RmlUiHost
        |-- QtRmlSystemInterface
        |-- QtRmlFileInterface
        |-- RmlUiQtInputAdapter
        |-- IngnomiaRmlUiRenderer (official GL3 renderer adapter)
        |-- Rml::Context + documents + data models
        `-- action callbacks -> presentation controllers -> EventConnector/aggregators
```

Ownership rules:

1. All RmlUi objects live on the Qt GUI/render thread.
2. `RmlUiHost` owns the system/file/render interfaces, context, documents, model handles, and debugger lifetime.
3. Interfaces are constructed before `Rml::Initialise()` and outlive `Rml::Shutdown()`.
4. The GL context must be current when constructing or destroying the GL3 renderer and while loading/unloading GPU-backed documents or fonts.
5. Game state reaches UI presentation objects through queued Qt signals where the producer is on the game thread. UI callbacks dispatch through `EventConnector`; they never retain `Game*`, entity pointers, or aggregator-owned row pointers.
6. `MainWindow` owns gesture routing. Neither an RML document nor a data-model callback decides whether a world click should occur.

## ADR-001: dependency management

### Decision

Fetch RmlUi 6.2 by immutable commit:

```cmake
include(FetchContent)

FetchContent_Declare(
    rmlui
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG        2230d1a6e8e0848ed87a5761e2a5160b2a175ba4
    GIT_SHALLOW    FALSE
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_FONT_ENGINE freetype CACHE STRING "" FORCE)
FetchContent_MakeAvailable(rmlui)
```

The production integration must expose `FETCHCONTENT_SOURCE_DIR_RMLUI` in developer/build documentation so CI, distro builds, and offline developers can point at a pre-fetched verified source tree. CI should set `FETCHCONTENT_FULLY_DISCONNECTED=ON` only after dependency population. A source archive mirror may be added later, but its SHA-256 must be pinned and recorded.

Use static RmlUi libraries for the first integration. This avoids another runtime DLL and makes the RmlUi version part of the game artifact. Keep FreeType as an explicit target available before RmlUi configuration (`Freetype::Freetype`). The production dependency owner must pin and document FreeType independently; do not silently use an arbitrary system copy. SVG, Lottie, Lua, Tracy, samples, and RmlUi's own test executables remain off in the game build unless separately accepted.

RmlUi 6.2 exports `RmlUi::Core`, `RmlUi::Debugger`, and the convenience `RmlUi::RmlUi` target. Link Core and Debugger explicitly so the release build can omit debugger code with a clear configuration decision.

The official GL3 renderer source is not added when RmlUi is consumed with samples/tests disabled. Create an Ingnomia-owned renderer target that compiles the two official tagged backend files from `${rmlui_SOURCE_DIR}/Backends` plus a tiny adapter. Define `RMLUI_GL3_CUSTOM_LOADER` to a shim that includes the repository's existing `<glad/gl.h>`, and link the existing `glad` target exactly once. Record this source-coupling in the version-upgrade checklist.

### Rationale

- The immutable SHA makes configure results reproducible even if the release tag is moved.
- FetchContent fits the current single CMake build and avoids requiring every contributor to install a matching RmlUi package manually.
- The source override preserves offline and distribution workflows.
- A static first integration reduces deployment change while the current Noesis runtime is being removed.
- Reusing the existing GLAD loader avoids duplicate GL symbols and keeps all world/UI calls on the same loaded function table.

### Rejected alternatives

**Git submodule.** It is reproducible but adds a second nested-repository lifecycle to a checkout that already requires submodule care. It is easy to clone without initialization, and updating the pinned revision requires a gitlink change rather than the dependency lock in CMake. Reconsider only if fully offline source distributions become the dominant build path.

**Required `find_package(RmlUi 6.2 CONFIG)`.** This is appropriate for Linux packagers but insufficient as the only developer path: package availability and compiled options vary, and an installation may not ship the backend sources needed for the GL3 adapter. Support it later as an explicit opt-in provider, with the pinned FetchContent path remaining the reference configuration.

**Floating release tag or `master`.** Rejected because it is not immutable and would make UI layout/render behavior drift without a project change.

**Copying RmlUi Core or the GL3 renderer into the source tree.** Rejected for Core because it obscures upstream provenance and makes security/bug-fix upgrades manual. The GL3 source is consumed from the exact fetched revision rather than copied; the Ingnomia adapter remains small and reviewable.

**Official prebuilt RmlUi binaries.** Rejected as the reference path because compiler runtime, C++ ABI, configuration, and FreeType options must match Ingnomia across Windows and Linux.

### Consequences

- Initial configuration needs network access or an offline source override.
- CMake cache options shared by RmlUi/FreeType must be scoped and documented carefully.
- A RmlUi version bump requires checking target names, GL3 backend APIs, custom-loader compatibility, layout changes, and visual baselines.
- RmlUi's MIT license and FreeType's license must be added to third-party notices before distribution.

## ADR-002: Qt/OpenGL integration boundary

### Decision

Qt owns the native platform and presentation boundary. RmlUi uses:

- a custom `QtRmlSystemInterface` for time, logs, translation, cursor, clipboard, and virtual-keyboard/IME activation;
- a custom `QtRmlFileInterface` rooted at the staged `content/rmlui` directory;
- an `RmlUiQtInputAdapter` for Qt-to-RmlUi key/modifier/text/pointer translation and normalized consumption results;
- an `IngnomiaRmlUiRenderer` adapting the tagged official GL3 renderer to the current GLAD table and Qt-provided framebuffer;
- one `Rml::Context` for the application window.

Do not create GLFW, SDL, Win32, or X11 windows through RmlUi. Do not create a second GL context for the primary UI. Do not let RmlUi swap buffers.

### Frame lifecycle

The accepted order for each visible frame is:

1. Qt delivers platform events to `MainWindow`.
2. `RmlUiQtInputAdapter` injects UI events before update and records whether UI or world owns each pointer gesture.
3. Queued game-to-UI updates are copied into presentation state; only changed model variables/rows are dirtied.
4. `Rml::Context::Update()` runs exactly once.
5. No RmlUi DOM/model mutation or input injection occurs until after render.
6. `MainWindowRenderer::paintWorld()` draws the world into the default framebuffer.
7. The host sets the GL3 viewport to the physical framebuffer size, calls `BeginFrame()`, `Context::Render()`, then `EndFrame()`.
8. Qt calls `swapBuffers()` exactly once.
9. The host uses `Context::GetNextUpdateDelay()` plus game-render demand to schedule the next update. Gameplay may remain continuous; menu-only screens should avoid an unconditional zero-delay loop.

The official 6.2 GL3 renderer supports basic geometry, transforms, stencil clip masks, layers, render textures, masks, filters, gradients, and shaders. It backs up and restores a defined set of enable flags, viewport/scissor, blend/stencil/color state, but its final composite targets framebuffer 0. Ingnomia already renders the world to framebuffer 0 and places UI last, which matches that assumption.

Do not interpret `EndFrame()` as restoration of arbitrary engine state. The integration test must snapshot at least framebuffer bindings, program, VAO, active texture, viewport/scissor, blend/depth/stencil enables, blend functions, color/depth masks, and `glGetError()`. Any state not restored by the official renderer must be either restored by the adapter or established explicitly at the start of the next world pass. If a later feature renders after the UI, it must add an explicit state boundary.

### Physical-pixel coordinate contract

RmlUi context dimensions, GL3 viewport dimensions, clipping rectangles, and injected pointer coordinates use **physical framebuffer pixels**.

```text
physical_size = round(window logical size * QWindow::devicePixelRatio())
pointer_px    = round(Qt local logical position * devicePixelRatio)
dp_ratio      = devicePixelRatio * user_ui_scale
```

RML/RCSS authors use `dp` for interface dimensions and font sizes. Raw `px` is reserved for true texture pixels, one-pixel diagnostics, and explicitly reviewed cases. This gives the official renderer one consistent coordinate system while retaining logical-size UI through the `dp` ratio.

On resize, screen change, or device-pixel-ratio change, in this order:

1. make the Qt GL context current;
2. compute and clamp the new physical size to at least 1x1;
3. call `context->SetDimensions(physical_size)`;
4. call `context->SetDensityIndependentPixelRatio(dpr * user_scale)`;
5. call `renderer.SetViewport(physical_width, physical_height)`;
6. update world renderer dimensions using its existing contract;
7. request an update.

Listen to `QWindow::screenChanged`, relevant `QScreen` DPI signals, resize events, and Qt's device-pixel-ratio change event where supported. The spike must move between monitors/scales when a second monitor is available; otherwise the limitation must be recorded.

### Rationale

- It preserves the actual Qt event loop and renderer rather than adding a competing platform backend.
- A shared context avoids texture transfer and synchronization between world and UI.
- UI-last compositing matches the official GL3 renderer's framebuffer-0 final pass.
- Physical pixels align RmlUi geometry, scissor/stencil clipping, the GL viewport, and Qt's framebuffer.
- The primary game surface remains a single RmlUi context, while opt-in detached windows use a deliberately separate context contract.

### Detached popup windows

Detached windows are a narrowly-scoped escape hatch for UI that benefits from independent top-level window ownership, such as creature inspectors. They are native Qt top-level windows with their own `QOpenGLContext`, sharing the main context's resources but using a renderer instance created in that context. Each window has its own RmlUi context, renderer, input adapter, dimensions, DPI scale, render timer, and close/focus lifecycle, so it can be moved, focused, resized, or placed on another monitor without moving the game's HUD or world camera.

The detached path must not create a second world renderer or duplicate game state. Commands continue through the existing UI command ports, and shared camera-preview textures remain owned by the main renderer. Management/workbench screens stay embedded until they have an explicit reason to become top-level windows. Every detached context must be destroyed before the primary RmlUi host shuts down.

### Rejected alternatives

**RmlUi GLFW/SDL backend embedded in Qt.** Rejected: those backends own a different windowing/event abstraction and are sample backends intended to be adapted, not stacked under a Qt `QWindow`.

**A second shared OpenGL context for the primary UI.** Rejected because it adds synchronization and ownership complexity, and the final composition still needs the Qt window context. The same mechanism is permitted for opt-in native detached windows where independent top-level ownership is the feature being delivered; those windows must use the lifecycle and resource-sharing contract above.

**A from-scratch OpenGL renderer.** Rejected because the official GL3 renderer is the only built-in renderer that implements all advanced 6.x effects and has RmlUi's visual-test coverage. The adapter should stay thin.

**Logical context dimensions with a physical GL viewport.** Rejected because the official GL3 renderer uses its viewport dimensions for both the orthographic projection and `glViewport`; mixing units would produce incorrect size, pointer, and clipping behavior.

**Letting RML callbacks call `Game` directly.** Rejected because it crosses the GUI/game thread and bypasses the existing signal/aggregator command boundary.

## Platform and resource interfaces

### `QtRmlSystemInterface`

- `GetElapsedTime()`: monotonic `QElapsedTimer`, not wall-clock time.
- `LogMessage()`: map RmlUi levels to `qDebug/qInfo/qWarning/qCritical`; retain source text and avoid recursive logging.
- `TranslateString()`: delegate tokens to the existing translation service and return UTF-8. Missing tokens should remain visibly diagnosable in development builds.
- `SetMouseCursor()`: map normal RML cursor names plus `rmlui-scroll-*` names to `Qt::CursorShape`; restore the game default for an empty name.
- Clipboard: use `QGuiApplication::clipboard()` and explicit UTF-8 conversion.
- `ActivateKeyboard/DeactivateKeyboard()`: use Qt input-method APIs and update the IME rectangle using the physical-to-logical caret conversion. This is required for composition, not just ASCII typing.
- The system interface holds `QPointer<QWindow>` and must tolerate the window being destroyed during shutdown.

### `QtRmlFileInterface`

The default C stdio interface is not accepted for production because it depends on process working directory. The custom interface:

- resolves RML, RCSS, fonts, and images beneath the staged `content/rmlui` root;
- uses `QFile`/`QFileInfo` and UTF-8 paths;
- canonicalizes paths and rejects traversal outside the root;
- supports relative references resolved by RmlUi from each document source URL;
- returns null on missing resources and logs the logical and resolved paths;
- closes every handle deterministically;
- never reads save files or arbitrary filesystem paths through RML-authored URLs.

Use `QCoreApplication::applicationDirPath() + "/content/rmlui"` for deployed builds and an explicit override for tests. CMake stages the same directory next to the isolated spike executable.

### Fonts and images

- Load all required font faces before documents. The spike and production content package use `fonts/LatoLatin-Regular.ttf`, copied unchanged from the pinned RmlUi 6.2 sample assets with the authoritative OFL 1.1 notice in `fonts/notices/`. No legacy XAML/theme font path is part of the RmlUi asset contract.
- Load regular/bold faces with stable family names and include a fallback face covering tested localization ranges.
- RmlUi's bundled GL3 texture loader directly supports uncompressed TGA. PNG/JPEG are not implied. Production must either standardize UI runtime textures on supported formats, add an image decoder in the renderer adapter, or add an accepted plugin/decoder with license and tests.
- Every texture-loading failure is an error in the smoke test. Image proof must use a real decoded texture, not a colored box.

## Input, focus, and world routing

RmlUi 6.2 deliberately separates physical keys from UTF-8 text. `QKeyEvent::key()` maps to `Rml::Input::KeyIdentifier`; modifier flags map to `KM_SHIFT`, `KM_CTRL`, `KM_ALT`, `KM_META`, and lock states where the platform exposes them. `QKeyEvent::text().toUtf8()` is sent through `ProcessTextInput()` only for actual committed text. `QInputMethodEvent::commitString()` handles composed text. Key auto-repeat is forwarded consistently and tested in a text field.

RmlUi's raw return values are easy to invert:

- pointer move/down/up return `true` when the mouse is **not** interacting with UI;
- wheel/key/text return `true` when the event remains unconsumed/propagating.

The adapter must expose positive application semantics such as `uiOwnsPointer` and `uiConsumedKey`; call sites must not interpret the raw booleans directly.

Pointer routing rules:

1. Always send mouse movement to RmlUi so hover state stays current.
2. On button-down, after injection, assign that button's gesture owner from `!result` / `Context::IsMouseInteracting()`.
3. A world-owned press remains world-owned until release even if the cursor crosses UI; a UI-owned press remains UI-owned until release even if it leaves the panel.
4. Only a world-owned left drag pans the camera. Only a world-owned click emits selection actions.
5. Inject wheel using the vector overload. RmlUi positive Y means down, so normal Qt angle-wheel input uses `-angleDelta.y() / 120.0f`; use `pixelDelta` for precision devices with a measured conversion. If RmlUi returns unconsumed, preserve the current world zoom/z-level behavior.
6. On leave or focus loss, call `ProcessMouseLeave()`, release recorded button/key ownership, clear camera movement flags, and cancel/hide IME state.
7. Full-screen transparent HUD roots use `pointer-events: none`; interactive descendants opt back in. Otherwise RmlUi considers transparent/empty elements interactive irrespective of visual opacity and would block the map.

Keyboard focus rules:

- Focused text inputs and modal documents get first refusal on keys and text.
- Escape first closes the top UI modal/route, then propagates through the existing game escape action only when unconsumed.
- Global gameplay hotkeys are disabled while a text-editing control owns keyboard focus, except explicitly whitelisted emergency/debug bindings.
- Tabbable elements use `tab-index: auto`; keyboard-visible focus uses `:focus-visible`.
- Spatial navigation documents set `nav: auto` on `body`, then narrow directional behavior on dense toolbars/tables where necessary.

## Data binding, templates, localization, and debugger

- Register struct and array types before binding model variables.
- Bind callbacks to presentation/controller objects with lifetimes owned by `RmlUiHost`.
- Dirty only changed variables or stable row collections with `DataModelHandle::DirtyVariable()`. Do not call blanket dirtying each frame and do not reload documents to refresh data.
- Preserve stable entity/row IDs separately from display strings. Selection survives sorting and incremental updates by ID.
- Use RML templates for shared window chrome, modal shells, table headers, and inspector sections. Avoid one monolithic document and avoid deep template magic that hides navigation behavior.
- Route player-facing static tokens through `SystemInterface::TranslateString`; dynamic values remain typed until formatting in presentation adapters. RmlUi strings are UTF-8.
- Link/initialize `RmlUi::Debugger` only in development configurations. Bind a debug-only hotkey and never expose it in a release build.
- Treat RmlUi's visual tests as renderer reference coverage, not as proof of this adapter. Ingnomia needs its own deterministic screenshots, input tests, and GL-state checks.

## Lifecycle and shutdown contract

Initialization, with the GL context current:

1. construct renderer, system interface, and file interface;
2. call `Rml::SetRenderInterface`, `SetSystemInterface`, and `SetFileInterface`;
3. call `Rml::Initialise()` and fail closed on false;
4. create one uniquely named context at physical framebuffer dimensions;
5. set the density-independent ratio;
6. initialize the debugger in development builds;
7. load fonts;
8. register data-model types, variables, and callbacks;
9. load and show the initial document;
10. perform `Update()` and request the first frame.

Shutdown, with event delivery stopped and the GL context current:

1. stop redraw timers and disconnect Qt signals into the host;
2. clear gesture ownership and IME state;
3. remove callbacks/model handles that refer to presentation controllers;
4. unload documents and remove the RmlUi context;
5. call `Rml::Shutdown()` while renderer/system/file interfaces still exist;
6. destroy the GL3 adapter and its GPU resources;
7. release the Qt GL context only after the world renderer is also clean.

The production host must be idempotent: a partially failed initialize can safely call shutdown, and shutdown can be called once after any successful prefix. World load/unload does not reinitialize the RmlUi library; it changes routes/models. Repeated document load/unload is tested independently from process shutdown.

## Minimal spike acceptance matrix

The isolated spike must use a Qt `QWindow`, a Qt-owned 4.3 core `QOpenGLContext`, the tagged GL3 renderer, and the same physical-pixel/DPR contract. A green compile alone is insufficient.

| Requirement | Proof method | Current result |
|---|---|---|
| Qt/OpenGL/RmlUi initialization | runtime log with actual GL vendor/version/profile and RmlUi 6.2 | Blocked: Qt 6 not found |
| Document loads | fail-fast return check plus visible screenshot | Blocked |
| Font renders | visible non-default text and font-load success | Blocked |
| Image renders | visible decoded TGA test image | Blocked |
| Button click | counter changes and event log | Blocked |
| Hover/focus | visible style change; Tab and spatial arrows | Blocked |
| UTF-8 text input | type non-ASCII committed text into input; verify model value | Blocked |
| Wheel scrolling | scroll a clipped region without invoking world fallback | Blocked |
| Resize | exercise at least 1200x675 and 1920x1080 | Blocked |
| UI scaling | exercise 100%, 125%, 150%, 200% or record unavailable scales | Blocked |
| Clipping | scissor overflow plus rounded/transformed clip-mask visual | Blocked |
| World/UI coexistence | moving world primitive remains intact behind UI; GL state snapshot | Blocked |
| Clean shutdown | close window; process exit code 0; no GL errors | Blocked |
| Repeated load/unload | 100 document cycles under sanitizer/debug layer where available | Blocked |

The current environment provides MSVC and CMake 3.29.3, but baseline configuration fails because `Qt6Config.cmake` is unavailable. The bounded standalone configure attempt from a Visual Studio developer environment was:

```text
cmake -S spikes/rmlui-qt-gl -B build-rmlui-qt-spike-dev -G Ninja
...
CMake Error at CMakeLists.txt:5 (find_package):
  Could not find a package configuration file provided by "Qt6"
  with any of the following names:
    Qt6Config.cmake
    qt6-config.cmake
```

This is an external prerequisite for runtime spike evidence, not evidence that the design works. `cmake -P spikes/rmlui-qt-gl/verify-spike.cmake` checks the dependency pin and required source seams without claiming compilation or runtime behavior.

## Production integration sequence after acceptance

1. Supply and verify Qt 6 and FreeType in the supported Windows toolchain.
2. Configure/build/run the isolated spike and close every acceptance row above.
3. Have the orchestrator accept ADR-001, ADR-002, and the spike evidence.
4. Add the pinned dependency provider and renderer adapter to the shared build in a single reviewed integration change.
5. Add `RmlUiHost` behind the current `MainWindow` boundary without converting screens.
6. Prove world rendering, input consumption, resizing/DPI, GL state, repeated host lifecycle, and clean process shutdown in the real executable.
7. Only then start vertical screen migration using the separately accepted UI data/action contracts.

## Risks and required mitigations

| Risk | Impact | Mitigation / gate |
|---|---|---|
| Input return semantics inverted | UI actions leak into world or hotkeys stop working | normalized adapter API plus unit tests for consumed/unconsumed paths |
| Transparent root captures map | camera/selection appears broken | `pointer-events: none` root and interactive descendant tests |
| DPR units disagree | offset clicks, small UI, broken scissor | physical-pixel contract and 100-200% screenshot/input matrix |
| GL3 adapter assumes framebuffer 0 | wrong target or missing UI | UI-last default-framebuffer contract and framebuffer assertions |
| GL state leaks | next-frame world corruption | state snapshot, GL debug callback, explicit next-frame reset |
| Duplicate GL loader | link/runtime symbol conflict | `RMLUI_GL3_CUSTOM_LOADER` shim to existing glad target |
| Unsupported PNG/JPEG assumption | invisible art | TGA proof or accepted decoder with tests/license |
| Game-thread pointer captured by callback | data race/use-after-free | presentation copies, queued Qt signals, controller-owned callbacks |
| Per-frame full model dirtying | layout/frame-time regression | variable-level dirtying and representative list profiling |
| Context destroyed after GL context | texture/geometry leaks or crash | explicit shutdown order and about-to-destroy hook |
| RmlUi master docs drift from 6.2 | wrong API/behavior assumptions | verify implementation against tagged 6.2 headers/source during build |
| Missing Qt/FreeType in CI | integration unbuildable | toolchain prerequisite gate before shared build changes |

## Decision gate

This document selects the architecture but does **not** claim the minimal spike passed. The orchestrator should reject broad RML screen work until the isolated target builds and all feasible runtime rows are evidenced. Any change to dependency provider, coordinate units, event-consumption semantics, renderer source, or window/context ownership reopens both ADRs.

## Primary references

- [RmlUi 6.2 release](https://github.com/mikke89/RmlUi/releases/tag/6.2)
- [RmlUi 6.2 tagged source](https://github.com/mikke89/RmlUi/tree/6.2)
- [RmlUi integration and main loop](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/main_loop.html)
- [RmlUi user input](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/input.html)
- [RmlUi contexts and on-demand rendering](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/contexts.html)
- [RmlUi render interface and feature table](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces/render.html)
- [RmlUi system interface](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces/system.html)
- [RmlUi file interface](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces/file.html)
- [RmlUi high-DPI guidance](https://mikke89.github.io/RmlUiDoc/pages/faq.html)
- [RmlUi localization](https://mikke89.github.io/RmlUiDoc/pages/localisation.html)
- [RmlUi templates](https://mikke89.github.io/RmlUiDoc/pages/rml/templates.html)
- [RmlUi spatial navigation and pointer events](https://mikke89.github.io/RmlUiDoc/pages/rcss/user_interface.html)
- [RmlUi CMake options and tests](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/building_with_cmake.html)
- [CMake dependency guide](https://cmake.org/cmake/help/latest/guide/using-dependencies/index.html)
