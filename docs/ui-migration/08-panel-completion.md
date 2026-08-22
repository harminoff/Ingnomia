# Panel completion checklist

This is the working checklist for replacing the original Noesis/XAML panel
surface. “Covered” means an RmlUi document, typed controller, and Qt bridge
exist. It does not claim full gameplay parity until the corresponding runtime
mutation and save/reload gates are exercised against a real world.

| Original surface | Current RmlUi owner | Status | Remaining acceptance gate |
| --- | --- | --- | --- |
| Main/root shell | `documents/app_shell.rml`, shell controller | Covered | Physical navigation and lifecycle sweep |
| Main menu / new game / load / settings | `screens/main_menu.rml`, `new_game.rml`, `load_game.rml`, `settings.rml` | Covered | New-world and saved-world runtime proof |
| Loading / pause | `screens/loading.rml`, `pause_menu.rml` | Covered | Authoritative load, pause, and resume proof |
| In-game HUD / event prompt | `screens/game_hud.rml`, HUD controller | Covered | HUD producer coverage and event response proof |
| Build/designation catalog | `screens/game_hud.rml#hud_build_catalog`, typed `BuildCatalogRow` | Implemented in this pass | Populate from authoritative inventory/build snapshot; exercise placement |
| Selection status / active tool | HUD hint strip plus `screens/inspector.rml` selection section | Covered | Full designation/action matrix and world mutation proof |
| Tile / creature inspector | `screens/inspector.rml` | Covered | Runtime context refresh and locate proof |
| Agriculture | `panels/agriculture_manager.rml` and inspector agriculture section | Covered | Full farm/pasture/grove mutation sweep |
| Stockpile | `windows/stockpile_manager.rml` and inspector stockpile section | Covered | Full filter/content mutation sweep |
| Workshop / queue / trade | `windows/workshop_manager.rml` and management 6A | Covered | Full queue, craft, and trade save/reload sweep |
| Population / professions / schedules | `windows/population_manager.rml` and management 6B | Covered | Large roster and schedule mutation sweep |
| Inventory browser | `windows/inventory_browser.rml` and management 6B | Covered | Large inventory/history and paging sweep |
| Military | `windows/military_manager.rml` and management 6C | Covered | Squad/role/priority mutation sweep |
| Diplomacy / neighbors | `windows/diplomacy_missions.rml` and management 6C | Covered | Discovery masking and mission mutation sweep |
| Developer/debug panel | `developer_ui/debug_panel.rml` | Covered, developer-gated | Developer build only; no release registry entry |
| Configurable keybinding editor | None | Intentionally deferred | The upstream catalog is declared but not dispatched by `MainWindow`; unify runtime dispatch, conflict detection, persistence, and reset before exposing it |

## Work order

1. Feed the HUD build catalog from the authoritative inventory/build producer and
   exercise placement, cancel, rotate, and invalid-material paths.
2. Run the row-by-row runtime sweep for shell, saved/new worlds, pause/resume,
   event prompts, selection, and every management mutation, including save/reload.
3. Reconcile consolidated document registry paths and complete package/asset
   cleanup only after those runtime gates pass.
4. Revisit configurable keybindings as a separate feature after the existing
   dormant `KeyBindings` dispatch path is unified.
