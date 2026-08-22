# UI visual and functionality test

Date: 2026-08-12  
Build: MSVC 19.50 / Ninja, Qt 6.8.3, OpenGL 4.3, RmlUi 6.2  
Executable: `build-wave8-root-msvc/Ingnomia.exe`

## Verified

- The normal root target builds successfully with the acquired Steamworks and OpenAL SDKs.
- A fresh process creates a responsive `Ingnomia` window and initializes the NVIDIA OpenGL 4.3 context.
- RmlUi 6.2 loads the staged LatoLatin font and the shell main-menu document.
- An opt-in `INGNOMIA_UI_CAPTURE` path captures the actual post-composite framebuffer from `paintGL`; the inspected 1738x1285 menu capture is populated, readable, and shows focus/primary/danger states.
- Shell focused tests pass 3/3, including the new-game field dispatch test.
- Design-system and shell RML contract verifiers pass.

## Fixes made during this pass

1. Continue was hard-disabled in `MainWindow::initializeRmlUi`. It now scans the normal save root and enables Continue when an `IO::saveCompatible` save exists.
2. The shell's New Game controller started with `Idle` state, so both New Game buttons could silently do nothing. It now starts in `Ready` using the authoritative legacy defaults.
3. New Game text, checkbox, and range controls now emit typed `new_game.set_field` actions. `ShellQtCommandPort` queues them to the simulation-thread `EventConnector`, which validates the field name, mutates `NewGameSettings`, and publishes a value-only snapshot back to the UI.
4. Randomize name/seed now have queued EventConnector handlers and refresh the projected form.
5. Load-game kingdom/save rows are now generated as stable-ID buttons with closed C++ listeners; selecting a row updates the controller instead of rendering an inert label.
6. The compositor shell is click-through while the active route document owns pointer input, preventing the hidden app-shell layer from swallowing menu gestures.
7. Settings Revert now re-requests authoritative values, and Reset restores the five exposed settings to documented defaults through `AggregatorSettings` instead of rejecting the visible buttons.

## Saved-game inspection

The normal `GameManager::loadGame` path was exercised against
`C:/Users/harmi/Documents/My Games/Ingnomia/save/TheFragmentedLand/1/`.
The loader reached `IO::load complete`, entered the in-game HUD, and remained
responsive. The captured frame is
`build-wave8-root-msvc/saved-game-world-delayed.bmp`; it shows the real Resume,
speed, z-level, designation, jobs, and workbench controls.

The active content tree was missing the runtime tilesheets referenced by
`BaseSprites`. All 17 available upstream deployment sheets were restored under
`content/tilesheet/` byte-for-byte from `origin/gh-pages`; hashes and provenance
are recorded in `content/tilesheet/ASSET_PROVENANCE.md`. `SpriteFactory` now skips
an unavailable optional sheet with a clear warning instead of aborting the
entire factory.

The inspected save still renders a black colony viewport. Its loaded tile
records are predominantly undiscovered/fog records, and eight DB-referenced
optional sheets are not present in the upstream deployment tree. This is an
explicit remaining asset/save-state limitation, not evidence that the RmlUi
buttons failed. A discovered save or the remaining licensed sheets is required
before claiming full world-art parity.

## Limits of this evidence

The desktop sandbox does not provide reliable cross-process physical mouse injection, so this pass does not claim a full human click-through of every route. The focused controller tests, typed bridge compilation, real-process startup, and real framebuffer capture are confirmed; remaining broad gameplay parity still requires a normal interactive desktop session and a save/new-world run.

## Classic park visual pass

The RmlUi skin now uses a compact classic park-management vocabulary inspired by
RollerCoaster Tycoon 2: sky-blue workspace, pale steel panels, navy title bars,
hard one-pixel bevels, dense 13dp labels, yellow primary/hover states, and orange
danger actions. The palette is centralized in `content/rmlui/styles/tokens.json`
under `visual_theme: classic-park`, with shared base/component overrides and
shell/HUD-specific chrome. No RCT2 assets were copied; this is a visual-language
adaptation of the existing Ingnomia controls.

The rebuilt executable produced and visually inspected these captures:

- `build-wave8-root-msvc/rct2-menu2.bmp` — main menu with Continue, New Game,
  Load, Settings, and Exit states.
- `build-wave8-root-msvc/rct2-hud.bmp` — saved-game HUD with top status rail,
  left workbench shelf, speed controls, and bottom hint strip.

The saved-game frame is intentionally documented with the fog limitation above:
the chrome is live and readable, while the colony viewport remains blue because
the inspected save contains predominantly undiscovered tiles.

## New-game regression

The reported New Game failure was reproduced with the same queued
`EventConnector::onStartNewGame` slot used by the menu. World creation completed
successfully (`init`, `generateWorld`, `postCreationInit`, and `resume` all
completed) and the process remained responsive. The visible failure was a stale
shell overlay: the menu route document was never hidden when the game emitted
`signalInMenu(false)`, so the new HUD/world was rendered underneath the menu.

`ShellRmlBinding::setInMenu` now explicitly hides the app-shell and route
documents on game entry and shows them again on return to the menu. The verified
post-generation capture is `build-wave8-root-msvc/new-game4.bmp`; it shows the
live HUD rails and workbench controls rather than the main menu. The opt-in
`INGNOMIA_AUTOMATE_NEW_GAME=1` path and lifecycle trace remain available for
future regression runs.

The saved-game blue-screen report had a separate compositor cause. The RmlUi
classic-park pass originally applied the sky-blue `body` background to every
document, including the full-screen HUD document, so it covered the OpenGL map
even though the renderer was submitting valid tile batches. The base document
background is now transparent and the sky-blue fill is scoped to shell/menu
surfaces; the HUD explicitly remains transparent. A new-world capture,
`build-wave8-root-msvc/new-game-visible.bmp`, shows generated terrain, trees,
creatures, and HUD. The saved-game capture,
`build-wave8-root-msvc/saved-game-visible.bmp`, shows the loaded colony; its
darkness is the save's nighttime/light state rather than a missing world
framebuffer.
