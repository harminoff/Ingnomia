/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "../state/UiFoundationTypes.h"

#include <limits>

namespace ingnomia::ui
{

enum class OverlayKind : std::uint8_t { Designations, Jobs, LoweredWalls, Axles };
enum class EntityKind : std::uint8_t { Tile, Creature, Item, Workshop, Stockpile, Farm, Pasture, Grove, Room, Mechanism };
enum class BuildKind : std::uint8_t { Workshop, Terrain, Item };
// The legacy BuildItemTemplate exposes these as distinct commands. Keep the
// command typed at the RmlUi boundary; EventConnector remains the authority
// for the resulting selection action and placement validation.
enum class BuildAction : std::uint8_t { Build, FillHole, Replace };
enum class EventResponse : std::uint8_t { Acknowledge, Yes, No };
enum class TileContextAction : std::uint8_t { Mine, Harvest, Manage, CancelJob, RaisePriority, LowerPriority, Deconstruct, RemoveFloor, FellTree, RemovePlant };
enum class Gender : std::uint8_t { Female, Male };
enum class CraftRepeatMode : std::uint8_t { Once, Repeat, Maintain };
enum class MoveDirection : std::uint8_t { Up, Down, Front, Back };
enum class AgricultureKind : std::uint8_t { Farm, Pasture, Grove };
// Keep this value set aligned explicitly with the authoritative domain enum in
// src/base/enums.h. Conversion code must still switch by name rather than cast
// ordinals across the UI boundary.
enum class ScheduleActivity : std::uint8_t { None, Eat, Sleep, Training };
enum class HistoryRange : std::uint8_t { Week, Month, Season, Year, All };
enum class MilitaryAttitude : std::uint8_t { Flee, Defend, Attack, Hunt };
enum class UniformSlot : std::uint8_t {
	HeadArmor,
	ChestArmor,
	ArmArmor,
	HandArmor,
	LegArmor,
	FootArmor,
	LeftHandHeld,
	RightHandHeld,
	Back
};
enum class MissionType : std::uint8_t { None, Explore, Spy, Emissary, Raid, Sabotage };
enum class MissionAction : std::uint8_t { None, Improve, Insult, InviteTrader, InviteAmbassador };
enum class InventoryDepth : std::uint8_t { Category, Group, Item, Material };
enum class FilterDepth : std::uint8_t { Category, Group, Item, Material };
enum class TradeParty : std::uint8_t { Player, Trader };

struct EntityRef
{
	WorldEpoch world;
	EntityKind kind{ EntityKind::Tile };
	std::uint32_t id{};
	std::optional<WorldPosition> position;
	bool operator==( const EntityRef& ) const = default;
};

struct InventoryRowId
{
	CatalogId category, group, item, material;
	InventoryDepth depth{ InventoryDepth::Item };
	bool operator==( const InventoryRowId& ) const = default;
};

struct TradeRowId
{
	TradeParty party{ TradeParty::Player };
	CatalogId item, materialOrGender;
	std::uint8_t quality{};
	bool operator==( const TradeRowId& ) const = default;
};

struct ScheduleCellId
{
	CreatureId creature;
	std::uint8_t hour{};
	bool operator==( const ScheduleCellId& ) const = default;
};

struct StockpileFilterRowId
{
	StockpileId stockpile;
	CatalogId category, group, item, material;
	FilterDepth depth{ FilterDepth::Item };
	bool operator==( const StockpileFilterRowId& ) const = default;
};

struct AgricultureTarget
{
	AgricultureKind kind{ AgricultureKind::Farm };
	DesignationId designation;
	bool operator==( const AgricultureTarget& ) const = default;
};

using SettingValue = std::variant<bool, std::int32_t, float, std::string>;
using NewGameScalarValue = std::variant<bool, std::int32_t, std::uint32_t, float, std::string, CatalogId>;

struct NewGameFieldValue
{
	NewGameFieldId field;
	NewGameScalarValue value;
	bool operator==( const NewGameFieldValue& ) const = default;
};

struct NewGameDraft
{
	std::vector<NewGameFieldValue> fields;
	bool operator==( const NewGameDraft& ) const = default;
};

#define UI_PAYLOAD_EQUALITY( Type ) bool operator==( const Type& ) const = default

struct NoPayload { UI_PAYLOAD_EQUALITY( NoPayload ); };
struct StartNewGamePayload { NewGameDraft draft; UI_PAYLOAD_EQUALITY( StartNewGamePayload ); };
struct LoadGamePayload { SaveSlotId slot; UI_PAYLOAD_EQUALITY( LoadGamePayload ); };
struct RoutePayload { RouteId route; UI_PAYLOAD_EQUALITY( RoutePayload ); };
struct SetPausedPayload { bool paused{}; UI_PAYLOAD_EQUALITY( SetPausedPayload ); };
struct SetSpeedPayload { GameSpeed speed{ GameSpeed::Normal }; UI_PAYLOAD_EQUALITY( SetSpeedPayload ); };
struct OverlayPayload { OverlayKind overlay{ OverlayKind::Designations }; bool enabled{}; UI_PAYLOAD_EQUALITY( OverlayPayload ); };
struct CenterPayload { std::variant<EntityRef, WorldPosition> target; UI_PAYLOAD_EQUALITY( CenterPayload ); };
struct ChangeLevelPayload { std::int32_t absoluteLevel{}; UI_PAYLOAD_EQUALITY( ChangeLevelPayload ); };
struct ActivateToolPayload { ToolId tool; std::optional<CatalogId> item; std::vector<CatalogId> materials; UI_PAYLOAD_EQUALITY( ActivateToolPayload ); };
struct ChooseBuildPayload { CatalogId item; BuildKind kind{ BuildKind::Item }; std::vector<CatalogId> materials; BuildAction action{ BuildAction::Build }; UI_PAYLOAD_EQUALITY( ChooseBuildPayload ); };
struct SetBuildMaterialPayload { std::uint32_t componentIndex{}; CatalogId material; UI_PAYLOAD_EQUALITY( SetBuildMaterialPayload ); };
struct SelectPayload { EntityRef target; UI_PAYLOAD_EQUALITY( SelectPayload ); };
struct TileContextPayload { TileId tile; TileContextAction action{ TileContextAction::Manage }; UI_PAYLOAD_EQUALITY( TileContextPayload ); };
struct EventResponsePayload { PromptInstanceId prompt; EventResponse response{ EventResponse::Acknowledge }; UI_PAYLOAD_EQUALITY( EventResponsePayload ); };
struct WatchPayload { InventoryRowId row; bool watched{}; UI_PAYLOAD_EQUALITY( WatchPayload ); };
struct SetSettingDraftPayload { SettingId setting; SettingValue value; UI_PAYLOAD_EQUALITY( SetSettingDraftPayload ); };
struct SelectKingdomPayload { SaveKingdomId kingdom; UI_PAYLOAD_EQUALITY( SelectKingdomPayload ); };
struct SetNewGameFieldPayload { NewGameFieldId field; NewGameScalarValue value; UI_PAYLOAD_EQUALITY( SetNewGameFieldPayload ); };
struct PresetTargetPayload { PresetId preset; UI_PAYLOAD_EQUALITY( PresetTargetPayload ); };
struct SavePresetPayload { PresetId preset; NewGameDraft draft; UI_PAYLOAD_EQUALITY( SavePresetPayload ); };
struct SetSpeciesPayload { CatalogId species; bool enabled{}; std::uint32_t cap{}; UI_PAYLOAD_EQUALITY( SetSpeciesPayload ); };
struct SetStartingItemPayload { CatalogId item, material; std::uint32_t amount{}; UI_PAYLOAD_EQUALITY( SetStartingItemPayload ); };
struct StartingItemTargetPayload { CatalogId item, material; UI_PAYLOAD_EQUALITY( StartingItemTargetPayload ); };
struct SetStartingAnimalPayload { CatalogId species; Gender gender{ Gender::Female }; std::uint32_t amount{}; UI_PAYLOAD_EQUALITY( SetStartingAnimalPayload ); };
struct StartingAnimalTargetPayload { CatalogId species; Gender gender{ Gender::Female }; UI_PAYLOAD_EQUALITY( StartingAnimalTargetPayload ); };
struct StockpileTargetPayload { StockpileId stockpile; UI_PAYLOAD_EQUALITY( StockpileTargetPayload ); };
struct SetStockpileBasicsPayload { StockpileId stockpile; std::string name; std::int32_t priority{}; bool suspended{}, pull{}, allowPull{}; UI_PAYLOAD_EQUALITY( SetStockpileBasicsPayload ); };
struct SetStockpileFilterPayload { StockpileFilterRowId row; bool active{}; UI_PAYLOAD_EQUALITY( SetStockpileFilterPayload ); };
struct WorkshopTargetPayload { WorkshopId workshop; UI_PAYLOAD_EQUALITY( WorkshopTargetPayload ); };
struct SetWorkshopBasicsPayload { WorkshopId workshop; std::string name; std::int32_t priority{}; bool suspended{}, acceptGenerated{}, autoCraftMissing{}; std::optional<StockpileId> connectStockpile; std::optional<bool> linkStockpile; UI_PAYLOAD_EQUALITY( SetWorkshopBasicsPayload ); };
struct SetButcherOptionsPayload { WorkshopId workshop; bool butcherCorpses{}, butcherExcess{}; UI_PAYLOAD_EQUALITY( SetButcherOptionsPayload ); };
struct SetFisherOptionsPayload { WorkshopId workshop; bool catchFish{}, processFish{}; UI_PAYLOAD_EQUALITY( SetFisherOptionsPayload ); };
struct QueueCraftPayload { WorkshopId workshop; CatalogId craft; CraftRepeatMode mode{ CraftRepeatMode::Once }; std::uint32_t count{}; std::vector<CatalogId> materials; UI_PAYLOAD_EQUALITY( QueueCraftPayload ); };
struct SetCraftJobPayload { WorkshopId workshop; CraftJobId job; CraftRepeatMode mode{ CraftRepeatMode::Once }; std::uint32_t count{}; bool suspended{}, moveBack{}; UI_PAYLOAD_EQUALITY( SetCraftJobPayload ); };
struct MoveCraftJobPayload { WorkshopId workshop; CraftJobId job; MoveDirection direction{ MoveDirection::Up }; UI_PAYLOAD_EQUALITY( MoveCraftJobPayload ); };
struct CraftJobTargetPayload { WorkshopId workshop; CraftJobId job; UI_PAYLOAD_EQUALITY( CraftJobTargetPayload ); };
struct SetTradeOfferPayload { WorkshopId workshop; TradeRowId row; std::uint32_t count{}; UI_PAYLOAD_EQUALITY( SetTradeOfferPayload ); };
struct AgricultureTargetPayload { AgricultureTarget target; UI_PAYLOAD_EQUALITY( AgricultureTargetPayload ); };
struct SetAgricultureBasicsPayload { AgricultureTarget target; std::string name; std::int32_t priority{}; bool suspended{}; UI_PAYLOAD_EQUALITY( SetAgricultureBasicsPayload ); };
struct SetAgricultureProductPayload { AgricultureTarget target; CatalogId product; UI_PAYLOAD_EQUALITY( SetAgricultureProductPayload ); };
struct SetHarvestOptionsPayload { AgricultureTarget target; bool harvest{}, harvestHay{}, tame{}; UI_PAYLOAD_EQUALITY( SetHarvestOptionsPayload ); };
struct SetGroveOptionsPayload { DesignationId grove; bool pick{}, plant{}, fell{}; UI_PAYLOAD_EQUALITY( SetGroveOptionsPayload ); };
struct SetPastureCapPayload { DesignationId pasture; Gender gender{ Gender::Female }; std::uint32_t max{}; UI_PAYLOAD_EQUALITY( SetPastureCapPayload ); };
struct SetButcheringPayload { CreatureId creature; bool butcher{}; UI_PAYLOAD_EQUALITY( SetButcheringPayload ); };
struct SetPastureFoodPayload { DesignationId pasture; CatalogId item, material; bool allowed{}; UI_PAYLOAD_EQUALITY( SetPastureFoodPayload ); };
struct SetSkillPayload { CreatureId creature; CatalogId skill; bool active{}; UI_PAYLOAD_EQUALITY( SetSkillPayload ); };
struct SetGnomeSkillsPayload { CreatureId creature; bool active{}; UI_PAYLOAD_EQUALITY( SetGnomeSkillsPayload ); };
struct SetSkillForAllPayload { CatalogId skill; bool active{}; UI_PAYLOAD_EQUALITY( SetSkillForAllPayload ); };
struct SetProfessionPayload { CreatureId creature; ProfessionId profession; UI_PAYLOAD_EQUALITY( SetProfessionPayload ); };
struct SetScheduleCellPayload { ScheduleCellId cell; ScheduleActivity activity{ ScheduleActivity::None }; UI_PAYLOAD_EQUALITY( SetScheduleCellPayload ); };
struct SetScheduleRowPayload { CreatureId creature; ScheduleActivity activity{ ScheduleActivity::None }; UI_PAYLOAD_EQUALITY( SetScheduleRowPayload ); };
struct SetScheduleColumnPayload { std::uint8_t hour{}; ScheduleActivity activity{ ScheduleActivity::None }; UI_PAYLOAD_EQUALITY( SetScheduleColumnPayload ); };
struct CreateProfessionPayload { std::string name; UI_PAYLOAD_EQUALITY( CreateProfessionPayload ); };
struct ProfessionTargetPayload { ProfessionId profession; UI_PAYLOAD_EQUALITY( ProfessionTargetPayload ); };
struct UpdateProfessionPayload { ProfessionId current; std::string newName; std::vector<CatalogId> skills; UI_PAYLOAD_EQUALITY( UpdateProfessionPayload ); };
struct InventoryHistoryPayload { CatalogId item, material; HistoryRange range{ HistoryRange::Month }; UI_PAYLOAD_EQUALITY( InventoryHistoryPayload ); };
struct SquadTargetPayload { SquadId squad; UI_PAYLOAD_EQUALITY( SquadTargetPayload ); };
struct RenameSquadPayload { SquadId squad; std::string name; UI_PAYLOAD_EQUALITY( RenameSquadPayload ); };
struct MoveSquadPayload { SquadId squad; MoveDirection direction{ MoveDirection::Up }; UI_PAYLOAD_EQUALITY( MoveSquadPayload ); };
struct GnomeTargetPayload { CreatureId creature; UI_PAYLOAD_EQUALITY( GnomeTargetPayload ); };
struct MoveGnomePayload { CreatureId creature; MoveDirection direction{ MoveDirection::Up }; UI_PAYLOAD_EQUALITY( MoveGnomePayload ); };
struct SetAttitudePayload { SquadId squad; CatalogId targetType; MilitaryAttitude attitude{ MilitaryAttitude::Flee }; UI_PAYLOAD_EQUALITY( SetAttitudePayload ); };
struct MovePriorityPayload { SquadId squad; CatalogId targetType; MoveDirection direction{ MoveDirection::Up }; UI_PAYLOAD_EQUALITY( MovePriorityPayload ); };
struct RoleTargetPayload { MilitaryRoleId role; UI_PAYLOAD_EQUALITY( RoleTargetPayload ); };
struct RenameRolePayload { MilitaryRoleId role; std::string name; UI_PAYLOAD_EQUALITY( RenameRolePayload ); };
struct AssignRolePayload { CreatureId creature; MilitaryRoleId role; UI_PAYLOAD_EQUALITY( AssignRolePayload ); };
struct SetRoleCivilianPayload { MilitaryRoleId role; bool civilian{}; UI_PAYLOAD_EQUALITY( SetRoleCivilianPayload ); };
struct SetUniformSlotPayload { MilitaryRoleId role; UniformSlot slot{ UniformSlot::ChestArmor }; CatalogId type; std::optional<CatalogId> material; UI_PAYLOAD_EQUALITY( SetUniformSlotPayload ); };
struct StartMissionPayload { MissionType type{ MissionType::None }; MissionAction action{ MissionAction::None }; NeighborId neighbor; CreatureId creature; UI_PAYLOAD_EQUALITY( StartMissionPayload ); };
struct SetRoomTenantPayload { DesignationId room; std::optional<CreatureId> tenant; UI_PAYLOAD_EQUALITY( SetRoomTenantPayload ); };
struct SetRoomAlarmPayload { DesignationId room; bool alarm{}; UI_PAYLOAD_EQUALITY( SetRoomAlarmPayload ); };
struct SetMechanismStatePayload { MechanismId mechanism; bool enabled{}; UI_PAYLOAD_EQUALITY( SetMechanismStatePayload ); };
struct SetAutomatonRefuelPayload { AutomatonId automaton; bool refuel{}; UI_PAYLOAD_EQUALITY( SetAutomatonRefuelPayload ); };
struct SetAutomatonCorePayload { AutomatonId automaton; CatalogId core; UI_PAYLOAD_EQUALITY( SetAutomatonCorePayload ); };

#undef UI_PAYLOAD_EQUALITY

using UiActionPayload = std::variant<NoPayload, StartNewGamePayload, LoadGamePayload, RoutePayload,
	SetPausedPayload, SetSpeedPayload, OverlayPayload, CenterPayload, ChangeLevelPayload, ActivateToolPayload,
	ChooseBuildPayload, SetBuildMaterialPayload, SelectPayload, TileContextPayload, EventResponsePayload,
	WatchPayload, SetSettingDraftPayload, SelectKingdomPayload, SetNewGameFieldPayload, PresetTargetPayload,
	SavePresetPayload, SetSpeciesPayload, SetStartingItemPayload, StartingItemTargetPayload,
	SetStartingAnimalPayload, StartingAnimalTargetPayload, StockpileTargetPayload, SetStockpileBasicsPayload,
	SetStockpileFilterPayload, WorkshopTargetPayload, SetWorkshopBasicsPayload, SetButcherOptionsPayload,
	SetFisherOptionsPayload, QueueCraftPayload, SetCraftJobPayload, MoveCraftJobPayload, CraftJobTargetPayload,
	SetTradeOfferPayload, AgricultureTargetPayload, SetAgricultureBasicsPayload, SetAgricultureProductPayload,
	SetHarvestOptionsPayload, SetGroveOptionsPayload, SetPastureCapPayload, SetButcheringPayload,
	SetPastureFoodPayload, SetSkillPayload, SetGnomeSkillsPayload, SetSkillForAllPayload, SetProfessionPayload,
	SetScheduleCellPayload, SetScheduleRowPayload, SetScheduleColumnPayload, CreateProfessionPayload,
	ProfessionTargetPayload, UpdateProfessionPayload, InventoryHistoryPayload, SquadTargetPayload,
	RenameSquadPayload, MoveSquadPayload, GnomeTargetPayload, MoveGnomePayload, SetAttitudePayload,
	MovePriorityPayload, RoleTargetPayload, RenameRolePayload, AssignRolePayload, SetRoleCivilianPayload,
	SetUniformSlotPayload, StartMissionPayload, SetRoomTenantPayload, SetRoomAlarmPayload,
	SetMechanismStatePayload, SetAutomatonRefuelPayload, SetAutomatonCorePayload>;

struct UiActionEnvelope
{
	ActionId id;
	RequestId request;
	std::optional<WorldEpoch> world;
	std::optional<Revision> expectedRevision;
	UiActionPayload payload;
};

} // namespace ingnomia::ui
