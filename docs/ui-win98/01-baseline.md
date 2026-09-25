# Production UI baseline

Recorded 2026-09-23 against `feat/separate-ui-windows` at `bca0bee3acf79d460360c5f6b96fde08db784585`, the same revision named in the attached historical audit.

## Build and runtime

- Host: Windows; exact OS version query returned Access Denied. PowerShell is the working shell.
- Build directory: `build-wave8-root-msvc-link-priority2`.
- Generator: Ninja. Configuration: `RelWithDebInfo`. MSVC: Visual Studio 2026 18.5.3, x64. Qt: 6.8.3, `msvc2022_64`.
- RmlUi: pinned 6.2 source revision `2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`; FreeType revision `42608f77f20749dd6ddc9e0536788eaad70ea4b5`.
- RmlUi uses Ingnomia’s OpenGL 3 renderer (`IngnomiaRmlUiRenderer`) with Qt/OpenGL 4.3 core contexts. Primary UI uses the game’s main OpenGL window; management/inspector surfaces can use detached, independently movable QWindows sharing the game context.
- The primary RmlUi font configured in `MainWindow::initializeRmlUi()` is the packaged `LatoLatin-Regular.ttf`. License notices for Lato, RmlUi, and FreeType are present.
- `INGNOMIA_UI_DESIGNER=ON`; design-system UI foundation tests are configured. All 13 registered CTest cases were built and run in this checkpoint: 11 passed, with two current source-contract mismatches documented in [status](00-status.md).
- Successful production build command from the nested checkout:

  ```powershell
  cmd.exe /d /s /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build build-wave8-root-msvc-link-priority2 --target Ingnomia --config RelWithDebInfo --parallel 8'
  ```

  Result: `Ingnomia.exe` linked; Qt runtime deployment reported current. Existing unrelated warnings include MSVC `/Ob1`/`/Ob2` override and pre-existing C4715/C4702 warnings.

## Route registry

The source registry has 17 standard production route IDs and one developer-only route. This is a registry inventory, not proof that every route or prerequisite state has been opened at runtime.

| Area | Route IDs |
|---|---|
| Shell | `shell.main_menu`, `shell.new_game`, `shell.load_game`, `shell.settings`, `shell.loading` |
| Game shell | `game.hud`, `game.pause`, `game.settings` |
| Workbenches | `workbench.population`, `workbench.inventory`, `workbench.military`, `workbench.diplomacy` |
| Inspectors and object managers | `panel.tile`, `panel.creature`, `panel.stockpile`, `panel.workshop`, `panel.agriculture` |
| Developer only | `debug.panel` |

Additional registered documents and dynamic panels include orders/tools, popups and modal documents, inspectors, load/new-game screens, settings, and management windows. The route registry is in `src/gui/ui/state/UiRegistries.cpp`; active route documents are resolved by `UiRegistries::findRoute`/`findDocument`.

## Runtime evidence

The app was launched from the production build with a disposable `INGNOMIA_DATA_FOLDER` under `%LOCALAPPDATA%\Temp`. No normal user save folder was used.

- [Main menu on the primary host](evidence/stage-00-main-menu.png) — captured by the primary RmlUi framebuffer.
- [Inventory & Resources on the detached host](evidence/stage-01-inventory-detached.png) — captured by the detached RmlUi renderer; rows and counts are fixture data from `MainWindow::showInventoryFixture()`.
- [Repeated detached inventory capture](evidence/stage-01-inventory-detached-repeat.png) — a second launch of the same fixture, byte-identical to the first.
- [Primary host during inventory fixture setup](evidence/stage-01-inventory-primary.png) — remains on the main-menu route while the separate Inventory window is shown.
- [Component gallery on the primary host](evidence/stage-01-component-verified-1.png) — production RmlUi renderer showing the fixture controls. The single-column 560dp fixture extends beyond the captured viewport; its modal preview is static markup.
- [Repeated component gallery capture](evidence/stage-01-component-verified-2.png) — second production launch; byte-identical (SHA-256 `F0615EA5385D5416F8A070A074DE26B341D760749CD9CCE29B54C22D6DA94BFD`).
- Fixture launch trace: [first run](evidence/stage-01-inventory-fixture-trace.log), [repeat run](evidence/stage-01-inventory-fixture-repeat-trace.log).
- Generated-part probe traces: [first run](evidence/stage-01-component-verified-1-trace.log), [repeat run](evidence/stage-01-component-verified-2-trace.log). Both report the engine-created `selectarrow`, `selectvalue`, `selectbox`, `slidertrack`, `sliderbar`, `sliderarrowdec`, and `sliderarrowinc` parts.
- [Copied disposable tutorial world](evidence/stage-00-disposable-copy-load-surface.png) — a fresh production process loaded the copied 21-file save and captured the surface at z=70. The source and copy captures are byte-identical. The [manifest](evidence/stage-00-disposable-save-manifest.txt) records provenance and hashes; [source save/load traces](evidence/stage-00-disposable-source-automation.log) and [copied-load traces](evidence/stage-00-disposable-copy-load-trace.log) show the production EventConnector path and `IO::load complete`.

The initial fixture hook captured only the primary window, which is why its original evidence file was renamed to `stage-00-main-menu.png`. A fixture-only detached capture environment variable now requests the Inventory window’s own renderer capture. The gallery and runtime probe prove that the production renderer loaded a real `<select>` with three options and RmlUi-created select/scrollbar children. They do not prove physical dragging, keyboard traversal, modal interaction, or gameplay command effects.

## Surfaces still to inventory at runtime

The route IDs above do not enumerate every dynamic state. Remaining runtime work includes each manager’s pages/tabs, generated lists and filter popups, inspector subpages, HUD tool shelf and event dialogs, settings controls, confirmations, empty/error/loading states, and focus/selection/disabled states. Exact entry paths and legal prerequisites belong in the verification matrix as those states are opened.

No normal user save was opened or altered. The test world and both data roots were created under a unique `%TEMP%` directory; SHA-256 manifests for all 21 copied save files still match after the copied world was loaded. The session’s native computer-use API reports native APIs disabled, so physical input evidence remains unavailable; existing in-app probes can still exercise specific states after their prerequisites are prepared.
