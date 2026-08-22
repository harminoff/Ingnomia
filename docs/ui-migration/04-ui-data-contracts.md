# UI data and action contracts

Status: **normative discovery contract; required before screen implementation**

Baseline: upstream commit `4f99266c0f95faa847ca0db2af18cf59aff07f4b`, branch `feat/rmlui-interface-rebuild`

Companion decisions: `01-current-ui-map.md` and `03-rmlui-architecture.md`
Scope: framework-neutral UI state, Qt/game-thread transport, presentation state, RmlUi binding, action dispatch, focus, modal, and world-lifecycle contracts

## Contract decision

The replacement UI uses one GUI-thread `UiStore` containing copied, typed presentation state. Game-thread aggregators remain the authoritative place to read or mutate the simulation, but their cross-thread payloads must become pointer-free DTOs. RML documents bind only to RmlUi adapter models owned by `RmlUiHost`; callbacks dispatch registered typed actions through a controller and the existing `EventConnector`/aggregator boundary.

The following are non-negotiable:

1. RML and RmlUi callbacks never hold or dereference `Game`, manager, entity, `QPointer<Game>`, `QSharedPointer<Job>`, or aggregator-owned row pointers.
2. A game-to-UI message is a value snapshot or patch carrying a `WorldEpoch` and revision. The GUI rejects stale epochs and out-of-order revisions.
3. A UI-to-game command is a registered `ActionId` plus a validated typed payload. RML never sends legacy selection/job strings directly to the simulation.
4. Domain IDs and values remain typed until the presentation/formatting edge. A displayed name is not an identity.
5. The store dirties only changed scalar variables or changed stable collections. It never reloads a document or calls blanket dirtying to refresh ordinary state.
6. World load/unload resets world-scoped state without destroying the RmlUi context. No queued payload from a former world may repopulate the new one.
7. The current aggregator semantics—game-thread snapshot/mutation followed by queued GUI delivery—are preserved while Noesis ownership and formatting are removed.

This document defines names other screen agents must use. Once implementation begins, changes to these names require orchestrator approval.

## Verified current boundary

### Thread and ownership reality

The source comments call `EventConnector` a GUI-side bus, but the live ownership is more precise:

- `src/main.cpp` constructs `GameManager`, which constructs `EventConnector` and all child aggregators, and then calls `gm->moveToThread(&gameThread)`. Qt moves the entire QObject child tree. `GameManager`, `EventConnector`, aggregators, `Game`, and simulation managers therefore run on the game thread after startup.
- `MainWindow`, the current Noesis view/models/proxies, and the future `RmlUiHost`, controllers, store, and adapters live on the Qt GUI/render thread.
- Proxy-to-aggregator and aggregator-to-proxy connections are generally explicit `Qt::QueuedConnection`. A queued argument must therefore be a registered, self-contained value.
- Connections among `Game`, `GameManager`, `EventConnector`, and aggregators normally execute on the game thread. `Game::loop()` also calls the creature aggregator directly on that thread.
- `EventConnector` owns all aggregators for process lifetime. Each world-aware aggregator stores a weak `QPointer<Game>` and is reinitialized after new/load. Its cached IDs and payloads currently survive until overwritten or explicitly closed; the new bridge must reset them on epoch change.
- `MainWindow` remains the authority for UI-versus-world gesture ownership, camera input, physical coordinates, and OpenGL presentation. Those concerns are not store actions.

The required transport is therefore:

```text
game thread
  Game / managers / Selection
       -> EventConnector + existing aggregators
       -> UiDomainBridge builds pointer-free DTO snapshot/patch
       -> queued Qt signal carrying {worldEpoch, revision, payload}

Qt GUI/render thread
  QtUiBridge receives and validates envelope
       -> UiStore applies equality/diff/reducer rules
       -> RmlUiBindingAdapter copies changed fields/rows
       -> DataModelHandle::DirtyVariable(changed_name)

user action
  RML callback -> UiController -> typed UiAction
       -> QtUiCommandBridge validates route/epoch/target
       -> queued call to EventConnector/aggregator on game thread
       -> authoritative snapshot/patch returns through the same path
```

`UiDomainBridge` and `QtUiCommandBridge` are logical roles; they may share one QObject implementation. They must not be folded into an RML document listener.

### Current update and query behavior

| Source | Current trigger and payload | Frequency / cost | Contract treatment |
|---|---|---|---|
| clock/calendar | `Game::sendClock()` emits minute/hour/day/localized season/year/sun string | every unpaused simulation loop; `postCreationInit()` currently connects the relay twice | coalesce by revision, emit typed calendar fields only when changed, fix duplicate connection during bridge migration |
| settlement pulse | kingdom name plus three preformatted strings | every heartbeat-accepted game loop, including unchanged values | replace with typed population/animal/item totals and equality suppression |
| heartbeat | integer request/ack | every game loop; simulation throttles when GUI is over 20 acknowledgements behind | keep as an internal health channel, not an RmlUi model variable and not a frame-dirty trigger |
| renderer tiles | changed tile set, projected into batches of up to 1000 `TileDataUpdate` rows | event-driven; full-world enumeration is expensive | remains renderer-only and outside `UiStore`; only view/camera/overlay summaries enter UI state |
| selection preview | cursor projection and preview grid rebuilt from mouse/action/camera changes | potentially every pointer/camera update; DB and world validation are expensive | renderer owns preview tiles; store receives compact typed tool/selection summary and validity counts only |
| selected tile | full `GuiTileInfo` snapshot when opened or when its tile is in the changed set | potentially expensive world/DB/manager traversal | request/event driven; snapshot only selected ID; no polling from render callback |
| selected creature | full `GuiCreatureInfo`, with PNG generation when equipment changes | current `Game::loop()` calls `update()` every loop while an ID is selected | split cheap live fields from equipment asset refs; rate-limit/coalesce and regenerate assets only on equipment revision |
| stockpile | info on open; content dirty signal on changes; every game loop calls `onUpdateAfterTick()` | content summary/filter aggregation can be large | retain dirty batching; emit basic and content revisions independently |
| workshop | info on open; craft list event; content dirty path; trade inventory requests | nested DB/inventory scans and trade list rebuilds can be expensive | retain separate info, craft queue, catalog, and trade revisions; row patches for offer counts/values |
| agriculture | farm/pasture signals plus on-demand global catalog/product/roster requests | cached catalogs at init; snapshots may include sprites and large rosters | keep catalogs world-scoped and cached; use asset IDs, not `QPixmap` |
| population | full population/schedule snapshots on open; single-gnome/schedule updates after commands | full rows include all skills; O(gnomes x skills) | request full snapshot on route entry, then stable row/cell patches |
| inventory | full nested tree on request; add/remove item triggers partial watch updates; build list on category request | full tree and build icon generation are expensive | cache immutable taxonomy, patch counts by composite ID, request catalog pages; asset refs replace PNG buffers |
| military | full squad/role rebuild on request and after most edits; priority subset update exists | moderate, DB-backed uniform catalogs | preserve subset updates and stable squad/role/gnome keys; avoid rebuilding unrelated tabs |
| neighbors/missions | full neighbor snapshot on request; mission list plus single mission events | moderate; discovery masking is authoritative | preserve masking; patch missions by ID; never infer undiscovered fields in presentation |
| load browser | directory scan and save metadata reads on request | blocking file I/O and potentially slow | execute off render path on current owner or worker; expose loading/error/empty states and relative stable IDs |
| settings | config read on request; setters persist immediately | small, but some current backing is incomplete | schema-driven state; expose only verified settings; application/persistence result returns to UI |

No RmlUi `Update()` or `Render()` callback may initiate any row in the expensive column.

### Current stringly and unsafe seams to remove

- `EventConnector::onTerrainCommand(tileID, QString cmd)`, `Selection::m_action`, `onSetSelectionAction(QString)`, craft-job command strings, population sort strings, IDs encoded in `BaseComponent::ToString()`, positions encoded as `"x y z"`, and visibility encoded as `"Visible"/"Collapsed"` are not target contracts.
- `GuiWorkshopProduct` and `GuiWorkshopComponent` contain `QPointer<Game>` and methods that query the game. They are domain-side builder objects, not cross-thread DTOs.
- `GuiAnimal`, `GuiPlant`, and pasture-food rows contain `QPixmap`; build and creature payloads contain encoded PNG buffers. Cross-thread UI DTOs instead carry `AssetId`/sprite descriptors resolved by the GUI resource service.
- `GuiWorkshopInfo` embeds domain `CraftJob`; `GuiTileInfo` embeds `MechanismData`, `Filter`, flags, and domain enums; neighbor signals expose domain `Mission`. The bridge must flatten these into UI DTOs with no behavior or ownership.
- Many payloads combine localized prose with values (`"Gnomes: 8"`, `"Sunset: 18:30"`, tile labels, watch labels, trade labels). Values and localization tokens must be separate.
- Zero is widely used as “none” for numeric IDs. Target contracts use `std::optional<StrongId>`; an ID's numeric zero is never overloaded at the presentation edge.

## Target layer boundary

| Layer | Proposed namespace / owner | May depend on | Must not depend on |
|---|---|---|---|
| game domain | existing `Game`, managers, `Selection`, aggregators | Qt and existing domain types as upstream already does | RmlUi, RML documents, presentation filters/sort state |
| framework-neutral contracts | `src/ui/contracts/` in namespace `ingnomia::ui` | C++ standard value types only | QObject, QString, QVariant, QPixmap, Noesis, RmlUi, `Game*` |
| game/Qt bridge | `src/gui/ui_bridge/` | contracts, Qt signals/slots, EventConnector/aggregators | RmlUi DOM or RML document IDs |
| presentation store/controllers | `src/ui/presentation/` | contracts and pure reducers/selectors | game pointers, QObject ownership, RmlUi types |
| RmlUi adapter | `src/gui/rmlui/bindings/` | presentation state and RmlUi data-model API | domain managers, DB/world queries, legacy XAML models |
| dispatch | `UiActionRegistry`, `UiController`, `QtUiCommandBridge` | typed actions, route/modal state, bridge | free-form `QString` commands from RML |

The standard-library-only contract layer uses UTF-8 `std::string`, `std::vector`, `std::optional`, `std::variant`, fixed-width integers, and `std::chrono` where wall-clock time is needed. Qt conversion happens once in the bridge. RmlUi conversion happens once in binding adapters.

## Canonical identifiers and naming

### Identifier primitives

```cpp
template<class Tag, class Rep> struct StrongId { Rep value; };

using WorldEpoch       = StrongId<struct WorldEpochTag, std::uint64_t>;
using Revision         = StrongId<struct RevisionTag, std::uint64_t>;
using RequestId        = StrongId<struct RequestIdTag, std::uint64_t>;
using TileId           = StrongId<struct TileIdTag, std::uint32_t>;
using CreatureId       = StrongId<struct CreatureIdTag, std::uint32_t>;
using DesignationId    = StrongId<struct DesignationIdTag, std::uint32_t>;
using WorkshopId       = StrongId<struct WorkshopIdTag, std::uint32_t>;
using StockpileId      = StrongId<struct StockpileIdTag, std::uint32_t>;
using CraftJobId       = StrongId<struct CraftJobIdTag, std::uint32_t>;
using MechanismId      = StrongId<struct MechanismIdTag, std::uint32_t>;
using AutomatonId      = StrongId<struct AutomatonIdTag, std::uint32_t>;
using SquadId          = StrongId<struct SquadIdTag, std::uint32_t>;
using MilitaryRoleId   = StrongId<struct MilitaryRoleIdTag, std::uint32_t>;
using NeighborId       = StrongId<struct NeighborIdTag, std::uint32_t>;
using MissionId        = StrongId<struct MissionIdTag, std::uint32_t>;
using EventResponseTargetId = StrongId<struct EventResponseTargetIdTag, std::uint32_t>;
using PromptInstanceId = StrongId<struct PromptInstanceIdTag, std::uint64_t>;
using AlertId          = StrongId<struct AlertIdTag, std::uint64_t>;
using NotificationId   = StrongId<struct NotificationIdTag, std::uint64_t>;
using ModalInstanceId  = StrongId<struct ModalInstanceIdTag, std::uint64_t>;
using FocusToken       = StrongId<struct FocusTokenTag, std::uint64_t>;

struct CatalogId { std::string value; };       // DB SID, case-sensitive
struct AssetId { std::string value; };         // logical resource ID, never an absolute path
struct LocalizationKey { std::string value; }; // starts with '$' for existing DB keys
struct SaveSlotId { std::string relativeKey; };// normalized kingdom/save relative key
struct SaveKingdomId { std::string relativeKey; };
struct RouteId { std::string value; };          // validated by NavigationRegistry
struct DocumentId { std::string value; };       // validated by DocumentRegistry
struct ActionId { std::string value; };         // validated by UiActionRegistry
struct ToolId { std::string value; };           // validated by ToolRegistry
struct SettingId { std::string value; };        // validated by SettingsSchema
struct ProfessionId { std::string value; };     // stable profession SID, never display text
struct PresetId { std::string value; };
struct NewGameFieldId { std::string value; };   // validated by NewGameSchema
struct WorldPosition { std::int32_t x, y, z; };
```

Strong numeric domain IDs are world-scoped and meaningful only with their `WorldEpoch`. `PromptInstanceId`, `ModalInstanceId`, and `FocusToken` are presentation-lifetime IDs generated monotonically by the GUI-thread store and never reused during the process. `CatalogId` and registry-backed string values are case-sensitive identifiers and are never translated. Display names may change with language. `SaveSlotId` is derived from normalized relative folder components plus slot metadata; absolute paths remain bridge-private.

Composite row IDs are lossless structs, not delimiter-joined strings:

```cpp
struct InventoryRowId { CatalogId category, group, item, material; InventoryDepth depth; };
struct TradeRowId { TradeParty party; CatalogId item, materialOrGender; std::uint8_t quality; };
struct SkillRowId { CreatureId creature; CatalogId skill; };
struct ScheduleCellId { CreatureId creature; std::uint8_t hour; };
struct UniformSlotId { MilitaryRoleId role; UniformSlot slot; };
struct StockpileFilterRowId { StockpileId stockpile; CatalogId category, group, item, material; FilterDepth depth; };
```

### Route IDs

Only `NavigationController` mutates route state. These IDs are exact:

| `RouteId` | Kind | Purpose |
|---|---|---|
| `shell.main_menu` | primary | title/main menu |
| `shell.new_game` | primary | new-game configuration |
| `shell.load_game` | primary | save browser |
| `shell.settings` | primary | settings workbench from title |
| `shell.loading` | primary | new/load transition and progress/error |
| `game.hud` | primary | map-first running-game shell |
| `game.pause` | overlay route | pause/save/settings/return menu; focus-contained |
| `game.settings` | overlay route | settings opened from a running world |
| `workbench.population` | workbench | population, professions, schedules |
| `workbench.inventory` | workbench | inventory browser plus request-gated history when its DTO adapter is available |
| `workbench.military` | workbench | squads, roles, uniforms |
| `workbench.diplomacy` | workbench | neighbors and missions |
| `panel.tile` | dock | selected tile inspector |
| `panel.creature` | dock | selected creature inspector |
| `panel.stockpile` | dock | stockpile manager for selected designation |
| `panel.workshop` | dock | workshop/craft/trade manager |
| `panel.agriculture` | dock | farm, pasture, or grove manager |
| `debug.panel` | development-only dock | debug controls; absent from release registry |

There is exactly one primary route, zero or one workbench, zero or one dock panel, an overlay-route stack, and a modal stack. `game.pause` and `game.settings` do not discard the underlying `game.hud` selection. Opening a workbench preserves the selected world reference so “locate” and close return to the same map context.

### Document IDs and paths

These `DocumentId` values are registered in one `DocumentRegistry`. Screen code does not load arbitrary paths.

| `DocumentId` | RML path |
|---|---|
| `doc.app_shell` | `documents/app_shell.rml` |
| `doc.main_menu` | `screens/main_menu.rml` |
| `doc.new_game` | `screens/new_game.rml` |
| `doc.load_game` | `screens/load_game.rml` |
| `doc.loading` | `screens/loading.rml` |
| `doc.settings` | `screens/settings.rml` |
| `doc.pause_menu` | `screens/pause_menu.rml` |
| `doc.game_hud` | `screens/game_hud.rml` |
| `doc.action_bar` | `screens/game_hud.rml` (consolidated HUD surface) |
| `doc.selection_status` | `screens/game_hud.rml` (consolidated HUD surface) |
| `doc.tile_inspector` | `screens/inspector.rml` (consolidated inspector surface) |
| `doc.creature_inspector` | `screens/inspector.rml` (consolidated inspector surface) |
| `doc.agriculture_manager` | `panels/agriculture_manager.rml` |
| `doc.stockpile_manager` | `windows/stockpile_manager.rml` |
| `doc.workshop_manager` | `windows/workshop_manager.rml` |
| `doc.population_manager` | `windows/population_manager.rml` |
| `doc.inventory_browser` | `windows/inventory_browser.rml` |
| `doc.military_manager` | `windows/military_manager.rml` |
| `doc.diplomacy_missions` | `windows/diplomacy_missions.rml` |
| `doc.event_prompt` | `screens/game_hud.rml` (consolidated modal layer) |
| `doc.confirm_destructive` | `modals/confirm_destructive.rml` |
| `doc.error_fallback` | built-in minimal fallback document, not file-dependent |
| `doc.debug_panel` | `developer_ui/debug_panel.rml`, development builds only |

Shared fragments use `components/<snake_case>.rml`; they are templates/components, not routable documents. An unavailable optional document produces `doc.error_fallback`, a logged error, and a recoverable close/back action.

### RmlUi data-model names

Each model has one owner and one top-level dirty namespace:

| Model name | Owner / major variables |
|---|---|
| `ui_shell` | `UiStore`: lifecycle, primary route, overlays, workbench, dock, pending/error state |
| `ui_modal` | `ModalController`: modal stack, active prompt/confirmation, focus return token |
| `ui_hud` | `HudState`: settlement, clock, camera, overlays, watch rows, alerts summary |
| `ui_tools` | `ToolState`: catalog, active tool, build catalog page, material choices, selection summary |
| `ui_inspector` | `InspectorState`: selected reference, tile/creature/building variants |
| `ui_stockpile` | `StockpileState` |
| `ui_workshop` | `WorkshopState`, craft queue/catalog, trade state |
| `ui_agriculture` | `AgricultureState` |
| `ui_population` | `PopulationState`, professions, schedules |
| `ui_inventory` | `InventoryState`, optional request-gated history series |
| `ui_military` | `MilitaryState` |
| `ui_diplomacy` | `DiplomacyState`, missions |
| `ui_settings` | `SettingsState`, schema, draft, validation |
| `ui_new_game` | `NewGameState`, schema, draft, catalogs, validation |
| `ui_load_game` | `LoadGameState` |
| `ui_debug` | development-only state |

RML variable names are `snake_case`. C++ types/fields are `PascalCase`/`camelCase`. CSS and design-token naming are owned by `05-ux-specification.md` and `06-design-system.md`; data/action code must not infer semantics from CSS classes.

## State envelope, load state, and errors

Every game-originated message uses:

```cpp
template<class Payload> struct UiUpdate {
    WorldEpoch world;
    Revision revision;        // monotonic per ChannelId within one world
    ChannelId channel;
    UpdateKind kind;          // Snapshot, Patch, Clear
    Payload payload;
};

enum class LoadStatus { Idle, Loading, Ready, Empty, Error, Stale };
enum class UpdateKind { Snapshot, Patch, Clear };
enum class ChannelId { Lifecycle, Clock, Settlement, View, Tool, Tile, Creature,
    StockpileInfo, StockpileContent, WorkshopInfo, WorkshopQueue, Trade,
    Population, Schedule, Inventory, Military, Diplomacy, Mission, EventPrompt,
    Settings, NewGame, LoadGame };
enum class UiErrorCode { InvalidAction, InvalidPayload, StaleWorld, StaleRevision,
    MissingTarget, Unavailable, Validation, Io, Bridge, MalformedEvent };
struct RequestState {
    LoadStatus status;
    std::optional<RequestId> request;
    std::optional<UiError> error;
    bool refreshInProgress;   // old valid data may remain visible
};
struct UiError {
    UiErrorCode code;
    LocalizationMessage message;
    bool retryable;
    std::optional<ActionId> retryAction;
};
```

The store applies an update only if its epoch equals the active epoch and its revision is newer than the channel revision. A response to a request also carries `RequestId`; the controller ignores superseded responses. Errors are state, not console-only side effects.

## Typed UI state

The declarations below define semantic fields. Implementation may split large structs into headers, but it may not collapse them into maps of strings.

### Shell, lifecycle, route, and modal state

```cpp
enum class WorldPhase { NoWorld, Generating, Loading, Ready, Saving, Unloading, Failed };
enum class PauseReason { None, Player, PauseMenu, EventPrompt, Save, WorldTransition };

struct UiLifecycleState {
    WorldEpoch world;
    WorldPhase phase;
    bool acceptsWorldActions;
    std::optional<LocalizationMessage> progress;
    std::optional<UiError> error;
};

struct NavigationState {
    RouteId primary;
    std::optional<RouteId> workbench;
    std::optional<RouteId> dock;
    std::vector<RouteId> overlays;
};

enum class ModalKind { EventPrompt, DestructiveConfirmation, Error };
struct ModalEntry {
    ModalInstanceId id;
    ModalKind kind;
    DocumentId document;
    FocusToken returnFocus;
    bool dismissOnEscape;
    bool blocksWorldInput;
};
```

`WorldPhase::Saving` preserves the current world snapshots but disables conflicting lifecycle actions. `Unloading` immediately clears selection, inspectors, workbenches, pending world commands, and world modals.

### Settlement, time, camera, overlays, and tools

```cpp
enum class Season { Spring, Summer, Autumn, Winter, Unknown };
enum class DaylightPhase { Night, Dawn, Day, Dusk, Unknown };
enum class GameSpeed { Normal, Fast };

struct SettlementSummary {
    std::string kingdomName; // player-authored raw text
    std::uint32_t gnomes;
    std::uint32_t animals;
    std::uint32_t items;
};
struct ClockCalendarState {
    std::uint8_t minute, hour;
    std::uint16_t day, year;
    Season season;
    DaylightPhase daylight;
    std::optional<std::uint16_t> nextSunEventMinute;
    bool paused;
    PauseReason pauseReason;
    GameSpeed speed;
};
struct CameraState {
    WorldPosition center;
    std::int32_t viewLevel;
    std::int32_t minLevel, maxLevel;
    std::uint8_t rotation; // 0..3
    float zoom;
};
struct RenderOverlayState {
    bool designations, jobs, loweredWalls, axles;
};

enum class ToolCategory { Inspect, Dig, Build, Agriculture, Designation, Job, Magic };
enum class ToolPhase { Inactive, ChoosingCatalog, ChoosingMaterials, Preview, Dragging };
struct ToolState {
    std::optional<ToolId> active;
    ToolCategory category;
    ToolPhase phase;
    std::optional<CatalogId> item;
    std::vector<CatalogId> materials;
    bool repeat;
    bool canRotate;
    std::uint8_t rotation;
    std::optional<LocalizationMessage> blocker;
};
struct SelectionSummary {
    std::optional<WorldPosition> cursor;
    std::optional<WorldPosition> anchor;
    std::uint32_t width, height, depth;
    std::uint32_t validTiles, invalidTiles;
    bool hollow;
};
```

`Season`, daylight, level, action, dimensions, counts, pause, and speed are not preformatted strings. Presentation selectors create localized labels. The current domain does not expose exact per-tile invalid reasons or estimated total cost; those fields remain absent until a game-thread projection is implemented.

### Build catalog

```cpp
enum class BuildKind { Workshop, Terrain, Item };
struct MaterialAvailability { CatalogId material; std::uint32_t available; };
struct RequiredComponent {
    CatalogId item;
    std::uint32_t amountPerBuild;
    bool requireSameMaterial;
    std::vector<MaterialAvailability> materials;
};
struct BuildCatalogRow {
    CatalogId id;
    LocalizationKey nameKey;
    BuildKind kind;
    AssetId icon;
    std::vector<RequiredComponent> required;
};
struct BuildCatalogState {
    RequestState request;
    ToolCategory category;
    std::optional<CatalogId> subcategory;
    std::string searchText;                 // presentation state
    std::vector<CatalogId> recent;          // session-local, confirmed actions only
    std::vector<CatalogId> favorites;       // config-backed only after persistence is added
    std::vector<BuildCatalogRow> rows;
    std::optional<CatalogId> selected;
};
```

Search is immediately available over loaded catalog IDs/localized names. “Recent” is session-local and may be implemented without simulation changes. “Favorites” must not be shown as persistent until a real config key and bridge are added. Costs are the authoritative required component amounts plus current availability; the UI must not claim reservation or successful placement before the returned selection state confirms it.

### Selected reference and inspectors

```cpp
enum class EntityKind { Tile, Creature, Item, Workshop, Stockpile, Farm, Pasture, Grove, Room, Mechanism };
struct EntityRef { WorldEpoch world; EntityKind kind; std::uint32_t id; std::optional<WorldPosition> position; };

struct SelectionState {
    std::optional<EntityRef> selected;
    std::optional<EntityRef> previous;
    bool offLevel;
};

struct TileInspectorState {
    RequestState request;
    TileId id;
    WorldPosition position;
    TerrainSummary terrain;
    std::vector<ItemStackRow> items;
    std::vector<CreatureRefRow> creatures;
    std::optional<JobSummary> job;
    std::optional<DesignationSummary> designation;
    std::optional<RoomSummary> room;
    std::optional<MechanismSummary> mechanism;
    std::vector<ContextAction> availableActions;
};

struct CreatureInspectorState {
    RequestState request;
    CreatureId id;
    std::string name;
    CatalogId species;
    std::optional<CatalogId> profession;
    AttributeValues attributes;
    NeedValues needs;
    LocalizationMessage activity;
    std::vector<EquipmentSlotRow> equipment;
};
```

`ContextAction` carries a registered `ActionId`, enabled flag, optional blocker message, and typed target; it never carries the current `Mine`/`Harvest` command text. The current typo `Tennant` is normalized to `Tenant` in target type/action names; bridge code may map the legacy function name internally.

### Management DTO families

The following are the minimum shared DTOs. All row containers also carry `RequestState`, collection revision, selected row ID, sort spec, and filter/search presentation state.

| State / stable row | Required typed fields | Current authoritative source |
|---|---|---|
| `StockpileState` / `StockpileFilterRowId`, `StockpileContentRowId` | ID, name, priority/range, suspended, pull flags, capacity, item/reserved counts, typed filter check state, item/material/count summary | `AggregatorStockpile`, `Stockpile`, `Filter` |
| `WorkshopState` / `CraftProductId`, `CraftJobId` | ID, name, priority/range, suspension/options, workshop subtype, required components/material availability, queue mode/count/progress/suspended/order | `AggregatorWorkshop`, `Workshop`, `CraftJob`, inventory/DB |
| `TradeState` / `TradeRowId` | party, item/material-or-gender, quality, stock count, offered count, unit value, offer totals, canTrade/blocker | workshop/trader and player inventory |
| `AgricultureState` / `DesignationId`, `CreatureId`, catalog composite | `AgricultureKind`, basic options, species/product ID, plot/ready counts, pasture sex/capacity/food, animal roster, grove/farm toggles | `AggregatorAgri`, farming managers |
| `PopulationState` / `CreatureId`, `SkillRowId` | name, profession ID/name, skill ID/group/level/xp/active, selected/sort state | `AggregatorPopulation`, gnome manager/DB |
| `ScheduleState` / `ScheduleCellId` | exactly 24 `ScheduleActivity` values per gnome | `AggregatorPopulation`, `Gnome::schedule` |
| `ProfessionState` / `ProfessionId` | name, selected skill IDs, rename/delete capability | gnome manager professions; current names act as IDs, so rename must be an atomic old/new action |
| `InventoryState` / `InventoryRowId` | IDs/names, totals, in-job, stockpiled, equipped, constructed, loose, total value, watched | `AggregatorInventory`, inventory indexes |
| `InventoryHistoryState` / item-material + day | calendar day and total/created/destroyed values | `ItemHistory`; real daily data exists, but its random test-data generator is never a production fallback |
| `MilitaryState` / `SquadId`, `MilitaryRoleId`, `CreatureId`, `UniformSlotId` | squad/role order, roster, target priority type and typed attitude, civilian, uniform type/material options | `AggregatorMilitary`, military manager/DB |
| `DiplomacyState` / `NeighborId` | discovered, optional distance/name/type/attitude/wealth/economy/military, active-mission flags | `AggregatorNeighbors`; undiscovered values remain `null`, not the string `"undiscovered"` |
| `MissionState` / `MissionId` | typed mission type/action/step/status, target, participant IDs, start/next tick, result if known | event/neighbor managers |
| `LoadGameState` / `SaveSlotId` | display name, relative key, version, timestamp, compatibility, selected kingdom/slot | `AggregatorLoadGame`/IO; no absolute path crosses into RML |
| `NewGameState` / catalog IDs | typed numeric fields, peaceful flag, starting item/material/amount rows, animal/gender/amount rows, species enable/cap, preset IDs, validation | `NewGameSettings`, DB-backed catalogs |

Domain enums such as `ScheduleActivity`, mission type/action, military attitude, gender, creature type, room type, and agriculture kind are copied to equivalent contract enums with explicit conversion switches. Do not expose integer ordinals to RML as the semantic API.

### Alerts, notifications, and event prompts

The three concepts are deliberately separate:

```cpp
enum class Severity { Info, Notice, Warning, Critical };
struct WorldLink { WorldPosition position; std::optional<EntityRef> subject; };

struct AlertItem {
    AlertId id;
    AlertKind kind;
    Severity severity;
    LocalizationMessage title, detail;
    std::optional<WorldLink> link;
    bool acknowledged, muted;
};
struct NotificationItem {
    NotificationId id;
    Severity severity;
    LocalizationMessage text;
    std::optional<WorldLink> link;
    std::uint64_t gameTick;
    bool read;
};
struct EventPrompt {
    PromptInstanceId instanceId; // bridge-generated FIFO identity; always unique
    std::optional<EventResponseTargetId> responseTarget; // domain answer target, YesNo only
    LocalizationMessage title, body;
    EventResponseKind responses; // Acknowledge or YesNo
    bool requestsPause;
    bool answered;
};
```

Only `EventPrompt` has a verified current upstream producer: `EventConnector::signalEvent(id,title,msg,pause,yesno)` and `onAnswer(id,bool)`. The emitted domain `id` is not FIFO identity: upstream deliberately emits acknowledge-only informational prompts with `id == 0`, so the GUI-thread bridge assigns a new `PromptInstanceId` to every received prompt. It maps a nonzero response/query ID to `EventResponseTargetId` only for `YesNo`; acknowledge prompts have no response target. A `YesNo` prompt with no valid nonzero response target is rejected as malformed rather than overloading zero. The current `GameModel` keeps a transient FIFO queue. There is no authoritative general alert stream, notification history, mute state, cause, or acknowledgment store. Screen implementations may show the active/pending event FIFO, but must label any session-only history as transient and may not fabricate alerts. New alert producers require separately accepted domain projections.

When an event with `requestsPause=true` arrives, the game remains authoritative for pause state. The modal may request pause, but does not locally assert that the simulation paused until `ClockCalendarState.paused` confirms it. `event.respond` must match the active `PromptInstanceId`. For `Acknowledge`, acceptance removes only that presentation FIFO instance and never calls `EventConnector::onAnswer`. For `YesNo`, the controller resolves its stored `EventResponseTargetId`, dispatches `onAnswer(target.value, answer)`, and removes the instance only after dispatch acceptance or authoritative invalidation—not on button-down.

### Settings schema

Settings are generated from typed schema rows rather than hand-coded controls:

```cpp
using SettingValue = std::variant<bool, std::int32_t, float, std::string>;
struct SettingDefinition {
    SettingId id;
    SettingsCategory category;
    LocalizationKey labelKey, descriptionKey;
    SettingValue current, defaultValue, draft;
    std::optional<NumericRange> range;
    ApplyMode applyMode; // Immediate, OnApply, RestartRequired
    bool supported, visible, debugOnly;
    std::optional<LocalizationMessage> validationError;
};
```

Verified player-facing backing at baseline:

| `SettingId` | Config / runtime backing | Type and current constraint | Contract status |
|---|---|---|---|
| `display.fullscreen` | `fullscreen`, `AggregatorSettings`, `MainWindow` | bool | supported, immediate |
| `interface.ui_scale` | lowercase `uiscale` | float, current request clamps minimum 0.5; upper bound needs UX/visual validation | supported after one canonical key is enforced |
| `camera.keyboard_pan_speed` | `keyboardMoveSpeed` | int, current request clamps 0..200 | supported, immediate |
| `display.minimum_light` | `lightMin` | percent 0..100 in UI, 0..1 config | supported after explicit conversion tests |
| `camera.wheel_changes_level` | `toggleMouseWheel` | bool | supported |
| `audio.master_volume` | `AudioMasterVolume` | normalized float is intended; current reader multiplies by 100 while setter stores the percent without dividing | blocked until unit/round-trip bug is fixed |
| `interface.language` | `language` | string | persistence exists, but live translation reload does not; available values conflict with baseline (`english` versus `en_US`/`fr_FR`) |

The checked-in `KeyBindings`/`keybindings.json` provide a catalog and persistence format, but current `MainWindow` dispatches hard-coded keys and does not use `KeyBindings::getCommand()`. Therefore `input.bindings` is **not supported** until runtime dispatch is unified and save/reset/conflict behavior is implemented.

Text scale, tooltip delay, high contrast, reduced motion, edge scrolling, notification behavior, event auto-pause rules, and persistent favorites have no verified baseline backing. Their schema rows remain absent—not disabled demo controls—until backing behavior lands. Debug settings/actions are compiled into `ui_debug` only.

## List identity, patches, selection, and dirtying

### Identity rules

1. A row key is a stable domain ID or an explicit composite ID from this document. Array index, localized name, current sort order, and RML element address are never identity.
2. Numeric world IDs are paired with `WorldEpoch`. The same numeric ID in a later world is a different identity.
3. Renames update a row's display field without changing identity. Profession names are an upstream exception; the action carries both old and new name and the returned full profession revision resolves identity atomically.
4. Selection stores the stable key. Sorting/filtering may hide a selected row but never silently retarget it. Removal clears selection and moves focus to the nearest surviving row or table header.
5. Catalog IDs are case-sensitive and never localized. Inventory composite IDs preserve empty path components explicitly; do not use pipe-joined keys from `GuiWatchedItem`.

### Patch protocol

```cpp
enum class RowPatchKind { Insert, Update, Remove, Move };
template<class Id, class Row> struct RowPatch {
    RowPatchKind kind;
    Id id;
    std::optional<Row> row;
    std::optional<Id> before;
};
template<class Id, class Row> struct CollectionPatch {
    Revision baseRevision, revision;
    std::vector<RowPatch<Id, Row>> rows;
};
```

Use a complete snapshot for initial load, epoch reset, cache miss, or when measured patch cost exceeds replacement. Otherwise use patches. A patch with the wrong `baseRevision` is rejected and triggers one refresh request. Reorder actions produce `Move`, not delete/insert.

### Store-to-RmlUi dirtying

The store reducer compares typed values before mutation and returns a `DirtySet`. The adapter batches all updates delivered before one `Context::Update()` and calls `DirtyVariable()` once per top-level changed variable. Required examples:

| Change | Dirty variable(s) | Must remain clean |
|---|---|---|
| minute advances | `ui_hud.clock` | settlement, watch rows, tools, routes |
| pause confirmation | `ui_hud.clock`, optionally `ui_shell.pending_actions` | all management collections |
| camera z-level | `ui_hud.camera`, `ui_tools.selection` if preview changed | inventory/population/etc. |
| one watched item count | `ui_hud.watch_rows` | full `ui_inventory.rows` unless that route is loaded and receives its own patch |
| one gnome skill toggle | `ui_population.gnome_rows` | schedules/professions and every other model |
| one schedule cell | `ui_population.schedule_rows` | population skill rows |
| workshop queue edit | `ui_workshop.queue_rows`, pending action | workshop product catalog/basic info |
| trade offer count | appropriate trade row collection and offer totals | craft queue and unrelated party rows |
| route change | `ui_shell.navigation` and model request state for entering route | loaded data owned by preserved routes |
| event arrives | `ui_modal.stack`, `ui_modal.event_prompt` | entire HUD/document tree |
| world epoch changes | all world-scoped variables once via explicit `resetWorld()` | app settings, static localization/catalog caches that are truly process-scoped |

`DirtyAllVariables()`, per-frame `DirtyVariable()` loops, document reloads, and wholesale DOM reconstruction are forbidden for normal updates. If RmlUi requires dirtying a whole bound array for a row patch, the in-memory array still updates by stable ID and the array variable is dirtied once per batch.

## Typed actions and dispatch

### Action envelope and result

```cpp
struct UiActionEnvelope {
    ActionId id;
    RequestId request;
    std::optional<WorldEpoch> world;
    std::optional<Revision> expectedRevision;
    UiActionPayload payload; // closed std::variant
};
enum class ActionResultKind { Accepted, Rejected, Superseded };
struct UiActionResult {
    RequestId request;
    ActionResultKind result;
    std::optional<UiError> error;
};
```

The controller validates action registration, active route/modal, target epoch, payload type, ranges, and target presence before queuing. The game side validates again against authoritative state. Unknown IDs and payload mismatches are logged and rejected visibly. UI state may mark an action pending, but may not optimistically mutate domain values. Idempotent setters carry the desired value; non-idempotent actions are deduplicated by `RequestId` for the lifetime of the bridge.

### Exact action IDs

The following IDs are the shared registry. Payload type names are normative even where the first bridge internally calls an existing many-parameter slot.

| Action IDs | Typed payload / destination |
|---|---|
| `app.exit` | `NoPayload` -> `EventConnector::onExit` |
| `app.start_new_game` | `StartNewGamePayload{NewGameDraft}` -> validate/apply `NewGameSettings`, then start |
| `app.continue_last_game` | `NoPayload` -> current continue path |
| `app.load_game` | `LoadGamePayload{SaveSlotId}` -> bridge resolves private absolute path |
| `app.save_game` | `NoPayload` -> save; lifecycle reflects Saving |
| `app.end_world` | `NoPayload` -> end current game after confirmation |
| `nav.open`, `nav.back`, `nav.close` | `RoutePayload{RouteId}` or `NoPayload`; GUI-thread navigation controller only |
| `sim.set_paused` | `SetPausedPayload{bool}` |
| `sim.set_speed` | `SetSpeedPayload{GameSpeed}` |
| `view.set_overlay` | `OverlayPayload{OverlayKind,bool}`; controller sends complete overlay state to existing setter |
| `view.center_on` | `CenterPayload{EntityRef or WorldPosition}` -> renderer bridge |
| `view.change_level` | `ChangeLevelPayload{absoluteLevel}` -> renderer/window bridge |
| `tool.activate` | `ActivateToolPayload{ToolId, optional item, materials}` -> strict legacy adapter/`Selection` |
| `tool.cancel` | `NoPayload` -> current selection clear/right-click semantics |
| `tool.rotate` | `NoPayload` -> current selection rotate path |
| `tool.choose_build` | `ChooseBuildPayload{CatalogId,BuildKind,materials}` |
| `tool.set_material` | `SetBuildMaterialPayload{componentIndex,CatalogId}`; presentation until choose-build dispatch |
| `inspect.select` | `SelectPayload{EntityRef}` -> existing selection/tile/creature request paths |
| `inspect.clear` | `NoPayload` |
| `tile.execute_context_action` | `TileContextPayload{TileId,TileContextAction}`; typed enum maps to current terrain/manage operations |
| `event.respond` | `EventResponsePayload{PromptInstanceId,EventResponse}`; acknowledge is presentation-only, while YesNo resolves the stored `EventResponseTargetId` and calls the domain bridge |
| `watch.set` | `WatchPayload{InventoryRowId,bool}` |
| `settings.set_draft` | `SetSettingDraftPayload{SettingId,SettingValue}` |
| `settings.apply`, `settings.revert`, `settings.reset` | `NoPayload`; dispatch only schema-supported IDs |
| `load.refresh`, `load.select_kingdom` | `NoPayload` or `SelectKingdomPayload{SaveKingdomId}`; bridge performs the file scan |
| `new_game.set_field` | `SetNewGameFieldPayload{NewGameFieldId,NewGameScalarValue}`; schema fixes the expected value type |
| `new_game.randomize_name`, `new_game.randomize_seed` | `NoPayload` |
| `new_game.select_preset`, `new_game.delete_preset` | `PresetTargetPayload{PresetId}` |
| `new_game.save_preset` | `SavePresetPayload{PresetId,NewGameDraft}` |
| `new_game.set_species` | `SetSpeciesPayload{CatalogId,bool enabled,std::uint32_t cap}` |
| `new_game.set_starting_item`, `new_game.remove_starting_item` | `SetStartingItemPayload{CatalogId item,CatalogId material,std::uint32_t amount}` or `StartingItemTargetPayload{item,material}` |
| `new_game.set_starting_animal`, `new_game.remove_starting_animal` | `SetStartingAnimalPayload{CatalogId species,Gender,std::uint32_t amount}` or `StartingAnimalTargetPayload{species,Gender}` |

Management action IDs and their closed payloads:

| Action IDs | Exact payload family |
|---|---|
| `stockpile.refresh` | `StockpileTargetPayload{StockpileId}` |
| `stockpile.set_basics` | `SetStockpileBasicsPayload{StockpileId,name,priority,suspended,pull,allowPull}` |
| `stockpile.set_filter` | `SetStockpileFilterPayload{StockpileFilterRowId,bool active}` |
| `workshop.refresh` | `WorkshopTargetPayload{WorkshopId}` |
| `workshop.set_basics` | `SetWorkshopBasicsPayload{WorkshopId,name,priority,suspended,acceptGenerated,autoCraftMissing,connectStockpile}` |
| `workshop.set_butcher_options`, `workshop.set_fisher_options` | `SetButcherOptionsPayload{WorkshopId,butcherCorpses,butcherExcess}` or `SetFisherOptionsPayload{WorkshopId,catchFish,processFish}` |
| `workshop.queue_craft` | `QueueCraftPayload{WorkshopId,CatalogId craft,CraftRepeatMode,count,materials}` |
| `workshop.set_job`, `workshop.move_job`, `workshop.cancel_job` | `SetCraftJobPayload{WorkshopId,CraftJobId,CraftRepeatMode,count,suspended,moveBack}`, `MoveCraftJobPayload{WorkshopId,CraftJobId,MoveDirection}`, or `CraftJobTargetPayload{WorkshopId,CraftJobId}` |
| `trade.refresh`, `trade.execute` | `WorkshopTargetPayload{WorkshopId}` |
| `trade.set_offer_count` | `SetTradeOfferPayload{WorkshopId,TradeRowId,std::uint32_t count}` |
| `agriculture.refresh` | `AgricultureTargetPayload{AgricultureKind,DesignationId}` |
| `agriculture.set_basics`, `agriculture.select_product` | `SetAgricultureBasicsPayload{target,name,priority,suspended}` or `SetAgricultureProductPayload{target,CatalogId}` |
| `agriculture.set_harvest_options`, `agriculture.set_grove_options` | `SetHarvestOptionsPayload{target,harvest,harvestHay,tame}` or `SetGroveOptionsPayload{DesignationId,pick,plant,fell}` |
| `agriculture.set_population_caps`, `agriculture.set_butchering`, `agriculture.set_food_allowed` | `SetPastureCapPayload{DesignationId,Gender,max}`, `SetButcheringPayload{CreatureId,bool}`, or `SetPastureFoodPayload{DesignationId,CatalogId item,CatalogId material,bool}` |
| `population.refresh` | `NoPayload` |
| `population.set_skill`, `population.set_all_skills_for_gnome`, `population.set_skill_for_all`, `population.set_profession` | `SetSkillPayload{CreatureId,CatalogId,bool}`, `SetGnomeSkillsPayload{CreatureId,bool}`, `SetSkillForAllPayload{CatalogId,bool}`, or `SetProfessionPayload{CreatureId,ProfessionId}` |
| `population.set_schedule_cell`, `population.set_schedule_row`, `population.set_schedule_column` | `SetScheduleCellPayload{ScheduleCellId,ScheduleActivity}`, `SetScheduleRowPayload{CreatureId,ScheduleActivity}`, or `SetScheduleColumnPayload{hour,ScheduleActivity}` |
| `profession.refresh`, `profession.create`, `profession.delete` | `NoPayload`, `CreateProfessionPayload{name}`, or `ProfessionTargetPayload{ProfessionId}` |
| `profession.update` | `UpdateProfessionPayload{ProfessionId current,std::string newName,std::vector<CatalogId> skills}`; rename is atomic |
| `inventory.refresh` | `NoPayload` |
| `inventory.request_history` | `InventoryHistoryPayload{CatalogId item,CatalogId material,HistoryRange}`; unavailable until the adapter exists |
| `military.refresh`, `military.add_squad`, `military.add_role` | `NoPayload` |
| `military.remove_squad`, `military.rename_squad`, `military.move_squad` | `SquadTargetPayload{SquadId}`, `RenameSquadPayload{SquadId,name}`, or `MoveSquadPayload{SquadId,MoveDirection}` |
| `military.remove_gnome`, `military.move_gnome` | `GnomeTargetPayload{CreatureId}` or `MoveGnomePayload{CreatureId,MoveDirection}` |
| `military.set_attitude`, `military.move_priority` | `SetAttitudePayload{SquadId,CatalogId targetType,MilitaryAttitude}` or `MovePriorityPayload{SquadId,CatalogId targetType,MoveDirection}` |
| `military.remove_role`, `military.rename_role`, `military.assign_role`, `military.set_role_civilian`, `military.set_uniform_slot` | `RoleTargetPayload{MilitaryRoleId}`, `RenameRolePayload{MilitaryRoleId,name}`, `AssignRolePayload{CreatureId,MilitaryRoleId}`, `SetRoleCivilianPayload{MilitaryRoleId,bool}`, or `SetUniformSlotPayload{MilitaryRoleId,UniformSlot,CatalogId type,std::optional<CatalogId> material}` |
| `diplomacy.refresh`, `diplomacy.refresh_available_gnomes` | `NoPayload` |
| `diplomacy.start_mission` | `StartMissionPayload{MissionType,MissionAction,NeighborId,CreatureId}` |
| `room.set_tenant`, `room.set_alarm` | `SetRoomTenantPayload{DesignationId,std::optional<CreatureId>}` or `SetRoomAlarmPayload{DesignationId,bool}` |
| `mechanism.set_active`, `mechanism.set_inverted` | `SetMechanismStatePayload{MechanismId,bool}`; bridge invokes the current toggle only when the revision-matched desired value differs |
| `automaton.set_refuel`, `automaton.set_core` | `SetAutomatonRefuelPayload{AutomatonId,bool}` or `SetAutomatonCorePayload{AutomatonId,CatalogId}` |

Destructive or exit-class actions (`app.exit`, `app.end_world`, `new_game.delete_preset`, profession/squad/role delete, trade execution where irreversible, and any future save delete) route through `modal.confirm_destructive` before domain dispatch. Their confirmation payload stores the already typed pending action, not a reparsed string. `app.exit` uses the required quit confirmation even when no world is loaded.

### Tool IDs and legacy adapter

`ToolId` is a validated strong registry value. Initial registered values cover the working current selection actions:

`inspect`, `mine`, `explorative_mine`, `remove_floor`, `dig_stairs_down`, `mine_stairs_up`, `dig_ramp_down`, `dig_hole`, `fell_tree`, `harvest_tree`, `forage`, `remove_plant`, `create_stockpile`, `create_farm`, `create_grove`, `create_pasture`, `create_personal_room`, `create_dormitory`, `create_dining_hall`, `create_hospital`, `create_forbidden_area`, `remove_designation`, `cancel_job`, `raise_job_priority`, `lower_job_priority`, `deconstruct`, and data-driven `build`.

`suspend_job`, `resume_job`, `create_guard_area`, and “Plant tree” appear in the current UI/catalog, but are disabled or have no observed `Selection::onSecondClick()` branch. They are not registered as enabled tools until their game-side handlers are proven. Existing `Selection`/DB action strings remain private to one exhaustive `LegacyToolActionAdapter` switch. Adding a tool requires a registered `ToolId`, localization key, authoritative capability/validation source, legacy/domain mapping, and a test; screen code cannot create one by string convention.

Craft-job commands similarly use typed `CraftJobEdit` fields. Population sort/search/filter is presentation state and does not dispatch to the game; the target does not preserve `AggregatorPopulation::m_sortMode` as domain state.

## Focus, modal, keyboard, and escape rules

1. `MainWindow`/input adapter determines gesture ownership as specified by `03-rmlui-architecture.md`. Store actions begin only after UI owns the gesture.
2. The top modal receives pointer, key, text, wheel, and navigation first. It has a full input blocker and a focus trap. World input and global gameplay hotkeys are disabled while it is present.
3. A focused text-edit control suppresses gameplay hotkeys. Committed UTF-8 text and physical keys remain separate.
4. Escape order is: cancel IME composition; close the top dismissible modal; close the top overlay route; close workbench/dock or cancel the active tool according to visible context; then emit the existing propagated game escape. An unanswered required event prompt is not dismissed by Escape.
5. Opening a modal records a stable `FocusToken`. Closing restores focus if the target still exists and is visible; otherwise focus moves to the opener's route heading or first actionable control.
6. Route change records per-route presentation state (selected tab, filters, sort, scroll anchor by row ID) but never retains RmlUi element pointers.
7. On window focus loss, clear held camera keys and gesture ownership, end/cancel IME state, but do not dispatch a domain cancel merely because the app lost focus.
8. On row removal, focus moves by stable list order. On epoch reset, focus moves to the loading/main route heading.
9. `game.pause` requests authoritative pause and shows pending state until confirmed. Closing it requests the appropriate prior pause state; it does not unpause an event-paused or save-paused game blindly.

## Localization and player-facing naming

```cpp
using LocalizationArg = std::variant<std::int64_t, double, std::string, CatalogId>;
struct LocalizationMessage {
    LocalizationKey key;
    std::vector<LocalizationArg> args;
    std::optional<std::string> sourceFallback;
};
```

- Static UI text and labels use keys consumed through `SystemInterface::TranslateString` or one presentation localization service.
- Domain catalog rows carry stable IDs and name keys such as `$ItemName_<sid>`, `$MaterialName_<sid>`, `$CreatureName_<sid>`, `$SkillName_<sid>`, `$CraftName_<sid>`. The adapter resolves at presentation time.
- Player-authored kingdom, creature, workshop, stockpile, profession, and squad names are raw UTF-8 and never treated as localization keys.
- Quantities, dates, percentages, time, priority, quality, and list summaries are formatted in presentation selectors. Do not concatenate English labels in aggregators.
- Transitional sources that only provide localized prose may use `sourceFallback`, but the DTO field must be named `localizedText` and must not double as an ID.
- Missing keys are visible and logged in development. They do not silently become empty strings.
- A live language change is not claimed at baseline. Once supported, it increments a `LocalizationRevision`, invalidates presentation formatting/name caches, and dirties localized variables without re-querying the world.
- Use “tenant,” not the legacy “tennant,” in all new player-facing and contract names.

## World, save/load, and process lifetime

### Required epoch protocol

1. Process startup creates `UiStore` with epoch 0, `NoWorld`, `shell.main_menu`.
2. Before new/load deletes or replaces a `Game`, the bridge emits `WorldWillUnload{oldEpoch}`. The GUI immediately sets `Unloading`, disables world actions, clears all world models, pending requests, selection, world routes, and world modals.
3. The game thread clears aggregator caches/current IDs and weak references. No cached DTO is reused for the next world.
4. The bridge allocates a strictly increasing epoch before generation/load begins and emits `WorldTransitionStarted{newEpoch, Generating|Loading}`.
5. All subsequent domain updates carry the new epoch. Old queued events are rejected even if they arrive later.
6. After `postCreationInit()` and authoritative initial snapshots, `WorldReady{newEpoch}` enables `game.hud` actions. A failed load emits a typed error, clears partial state, and returns to `shell.load_game` or `shell.main_menu`.
7. Save sets `Saving`, preserves snapshots, disables duplicate save/end/load, performs IO on the game side, and restores `Ready` with success/error. It preserves the pre-save paused state as the current implementation intends.
8. Returning to title performs the unload protocol, then routes to `shell.main_menu`. The RmlUi library/context and app/settings models remain alive.
9. Process shutdown first stops bridge delivery/callbacks and game production, then destroys documents/models/context in the order from `03-rmlui-architecture.md`. `QThread::terminate()` is not an accepted lifecycle mechanism.

The current `QPointer<Game>` values become null when the game QObject is deleted, but that alone is insufficient: cached IDs, queued value payloads, request completions, and RML selections also require epoch invalidation.

## Useful-data audit

New data is accepted only with a decision, source, frequency, cost, accuracy, stale behavior, and fallback. This table is the current boundary:

| Candidate / decision supported | Source of truth | Update / cost | Accuracy and stale behavior | Status / fallback |
|---|---|---|---|---|
| available vs reserved vs stockpiled/equipped/constructed/loose inventory | inventory indexes already projected | item add/remove patches; full tree on route entry | exact snapshot revision | supported; show loading before first snapshot |
| production/item trends | real `ItemHistory` daily totals/created/destroyed | daily or explicit history request; moderate series build | exact recorded history | supported after DTO adapter; never use `generateRandomData()` fallback |
| stockpile capacity/priority/reserved | `Stockpile`/aggregator | dirty event and tick-batched content | exact snapshot | supported |
| workshop inputs/queue/progress | workshop products, required components, `CraftJob`/`Job::progress()` where applicable | queue event and selected-workshop refresh; moderate | exact fields only; do not synthesize ETA | partial: component availability and queue supported; progress requires explicit flattening/proof |
| idle workers/unassigned professions | gnome job/action/profession state | derived scan on population pulse, O(gnomes) | derived and revision-labelled | proposed; omit summary until implemented |
| blocked/unreachable/missing-component jobs | `JobManager`, reachability and component flags | potentially expensive global job scan/path-related semantics | must define reason enum and observation point | not supported in initial slice; never infer from queue age |
| hauling backlog | JobManager/stockpile haul jobs | potentially O(jobs) | requires authoritative definition | not supported until projection measured |
| creature task/destination and urgent needs | creature/current action/task/needs/anatomy | selected creature pulse; cheap-to-moderate | exact snapshot, may be one pulse stale | current activity/needs supported; destination/health urgency require DTO proof |
| hostile presence/deaths/injuries | creature/event managers | event-driven producer needed | exact only | no current UI channel; omit rather than fabricate alert |
| selection validity | `Selection::testTileForJobSelection` | already computed for preview; potentially expensive | exact boolean per preview tile | support valid/invalid counts; reason unavailable |
| construction requirements/availability | DB build components plus inventory | catalog request and relevant inventory revision | exact at snapshot, may change before placement | supported; show “availability at last update,” game validates on confirm |
| food/drink trend | inventory category plus item history | derived query/series | depends on accepted catalog classification | proposed; no hard-coded food IDs in UI |

Presentation-derived values must carry a documented derivation and never alter gameplay. An unavailable field produces an omitted section or explicit unknown state, not `0`, fake demo data, or a permanent placeholder.

## Migration mapping and implementation gates

### Preserve, extract, replace

| Current seam | Migration decision |
|---|---|
| `EventConnector` high-level lifecycle/simulation/event forwarding | preserve behavior; wrap behind typed bridge and epoch envelopes |
| per-system aggregators | preserve game-thread query/mutation roles; split pointer-free DTO builders and patch emitters where needed |
| stockpile/workshop dirty batching | preserve and make revisioned |
| inventory/category caches and incremental watch updates | preserve useful cache; replace delimiter identities and formatted labels |
| selection validation/job creation | preserve authoritative semantics; isolate legacy action strings behind typed adapter |
| XAML proxies | remove after equivalent Qt bridge channels exist |
| Noesis models, `ObservableCollection`, visibility strings, `DelegateCommand` | replace with `UiStore`, pure controllers/selectors, RmlUi adapters |
| QPixmap/PNG generation inside DTO aggregation | replace with logical asset/sprite references and GUI-owned resource cache |
| domain structs passed across thread | flatten to contract DTOs |
| full-list rebuild after every edit | replace with stable patch or measured snapshot fallback |

### Required implementation order

1. Add standard-library contract types, compile-time strong IDs/enums, and equality tests without RmlUi.
2. Add lifecycle epoch and bridge envelopes; prove stale payload rejection across load/unload.
3. Add typed action registry/controller and unknown/wrong-payload rejection; implement the legacy tool adapter in one file.
4. Add `UiStore` reducers, collection patch application, selection retention, `DirtySet`, and tests.
5. Migrate app shell, lifecycle, clock/settlement, pause, event prompt, and save/load vertical slice.
6. Migrate selection/tool/build catalog and inspectors.
7. Migrate management surfaces one system at a time, preserving aggregator semantics and deleting each XAML proxy/model only after parity tests pass.
8. Add optional derived data producers only after source/cost/accuracy review.

### Contract acceptance tests

The shared layer is not accepted until tests demonstrate:

- every `RouteId`, `DocumentId`, data-model name, and `ActionId` is unique and registered;
- unknown actions, wrong payload variants, out-of-range values, missing targets, wrong epochs, and stale revisions fail closed and log a useful error;
- all queued DTOs are pointer-free value types and Qt metatypes are registered before connection;
- old-world snapshots, prompts, action results, and request completions cannot mutate a new epoch;
- repeated new/load/end cycles reset all world-scoped selections, rows, pending actions, and modals without resetting app settings;
- row selection survives update, move, sort, and filter by ID; removal has deterministic focus fallback;
- a one-field scalar change produces only its expected dirty variables;
- a one-row change does not reconstruct unrelated collections or reload a document;
- patch base-revision failure requests one refresh and does not partially apply;
- event prompts preserve FIFO identity, answer once, respect required/non-dismissible state, and reconcile authoritative pause;
- Escape/focus behavior matches the ordered rules with text input, modal, pause route, workbench, dock, active tool, and world fallback;
- localization uses IDs/arguments and never changes stable identity; UTF-8 player names round-trip;
- settings round-trip units/defaults/ranges and unsupported rows are absent;
- catalog, inventory, population, workshop, and trade workloads are profiled off the render callback and large representative lists do not dirty per frame;
- no target contract header includes Noesis or RmlUi, and no presentation/RmlUi file includes or dereferences `Game`/manager headers.

## Evidence boundaries and unresolved risks

- This is a source-verified contract, not runtime proof. The minimal RmlUi/Qt/OpenGL spike and production host remain gated by `03-rmlui-architecture.md`.
- Aggregator comments and current direct connections do not themselves prove every signal's runtime thread in all lifecycle moments; the implementation must assert QObject thread affinity in debug builds and use explicit queued connections at the UI boundary.
- Current game-to-UI clock relay is connected twice in `postCreationInit()`. Coalescing masks duplicate dirties, but the duplicate source connection should be corrected rather than relied upon.
- Current language selection persists a value but does not reload translation data, and its IDs conflict with the bundled default. Do not claim live localization switching.
- Current audio-volume conversion is not round-trip safe. Do not expose it through the schema until fixed and tested.
- Current checked-in keybinding catalog is not the active `MainWindow` dispatch path. A keybinding editor is blocked on unification.
- Some existing GUI DTOs contain game pointers, domain objects, or GUI resources despite crossing queued connections. They must not be reused unchanged.
- Current event text arrives as already formatted strings; key/argument event localization requires domain producer work. Preserve the text as an explicit fallback during transition.
- Alerts, notification history, favorites persistence, job-block reasons, hauling backlog, hostile/death/injury feeds, exact build cost totals, and selection invalidity reasons are useful but not currently contracted sources.
- Disabled `suspend_job`/`resume_job`, `create_guard_area`, and the blank plant-tree UI command are not proven working selection actions.
- The current shutdown ends with `gameThread.terminate()`. Clean cooperative simulation shutdown remains a release gate independent of UI document cleanup.

## Discovery handoff

**Assigned objective.** Audit real game/UI data and commands and define the shared typed state/action boundary for every RmlUi screen.

**Files and systems inspected.** `plan.md`; discovery documents 01–03 and generated current-UI map; `src/main.cpp`; `GameManager`, `Game`, `EventConnector`; all `Aggregator*` headers and relevant update/query implementations; `MainWindow` input; `Selection`; XAML root/game/new-game/settings models and all proxies; config, strings, keybinding, save-browser, inventory history, job, workshop, population, agriculture, military, neighbor, renderer, tile, and creature seams.

**Findings.** The actual thread boundary, update/cost table, unsafe DTOs, stringly actions, list identities, lifecycle hazards, settings limitations, and available/unavailable useful data are recorded above.

**Decisions and rationale.** One GUI-thread store, standard-library value contracts, queued epoch/revision envelopes, typed action registry, stable patches, narrow dirtying, centralized navigation/modal control, and legacy adapters preserve authoritative behavior without mechanically porting Noesis coupling.

**Files changed.** `docs/ui-migration/04-ui-data-contracts.md` only.

**Commands run.** Read-only PowerShell, `git status`, `Get-Content`, and `rg` source/inventory searches. No formatter, build, executable, network request, or implementation test was run because this assignment changes documentation only.

**Build/test result.** Not applicable to a documentation-only contract. Source consistency and identifier tables require orchestrator review; implementation acceptance tests are specified above.

**Visual evidence.** Not applicable; no UI implementation was changed.

**Unresolved risks.** Listed in “Evidence boundaries and unresolved risks.”

**Patch boundary.** This document only; no commit created.

**Unrelated changes.** None made. Pre-existing shared untracked discovery/spike artifacts and `src/base/beehive/` were preserved.
