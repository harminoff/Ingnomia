/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../actions/UiActions.h"

#include <optional>

#include <array>

namespace ingnomia::ui::inspector
{
enum class InspectorKind : std::uint8_t { None, Tile, Creature, Workshop, Stockpile, Agriculture };

struct TextCountRow { std::string label; std::string detail; std::uint32_t count{}; bool available{}; std::string icon; bool operator==( const TextCountRow& ) const = default; };
struct CreatureRow { CreatureId id; std::string label; EntityKind kind{ EntityKind::Creature }; bool operator==( const CreatureRow& ) const = default; };

struct EquipmentTypeChoice
{
	CatalogId type;
	std::vector<CatalogId> materials;
	bool operator==( const EquipmentTypeChoice& ) const = default;
};

struct EquipmentSlotState
{
	UniformSlot slot{ UniformSlot::ChestArmor };
	std::string label, item, material, icon;
	CatalogId desiredType;
	std::optional<CatalogId> desiredMaterial;
	std::vector<EquipmentTypeChoice> choices;
	bool operator==( const EquipmentSlotState& ) const = default;
};

struct TileInspectorState
{
	TileId id;
	WorldPosition position;
	std::string wall, floor, embedded, plant, water, construction;
	std::vector<TextCountRow> items;
	std::vector<CreatureRow> creatures;
	std::string jobName, jobWorker, jobPriority, requiredSkill, requiredTool, requiredToolAvailable, workPositions;
	std::vector<TextCountRow> requiredItems;
	std::optional<DesignationId> designation;
	std::string designationName, roomSummary, mechanismSummary;
	bool plantIsTree{}, plantIsHarvestable{}, hasJob{}, canRaisePriority{}, canLowerPriority{};
	bool canMine{}, canRemoveFloor{}, canFell{}, canHarvest{}, canRemovePlant{}, canManage{}, canDeleteStockpile{};
	bool operator==( const TileInspectorState& ) const = default;
};

struct CreatureInspectorState
{
	CreatureId id;
	std::string name, profession, activity;
	bool professionReported{};
	std::int32_t strength{}, dexterity{}, constitution{}, intelligence{}, wisdom{}, charisma{};
	std::array<bool, 6> attributesReported{};
	std::int32_t hunger{}, thirst{}, sleep{}, happiness{};
	std::array<bool, 4> needsReported{};
	std::vector<TextCountRow> skills, equipment, inventory;
	MilitaryRoleId equipmentRole;
	std::string equipmentRoleName;
	std::vector<EquipmentSlotState> equipmentSlots;
	bool equipmentReported{};
	bool skillsReported{};
	bool inventoryReported{};
	bool operator==( const CreatureInspectorState& ) const = default;
};

struct WorkshopInspectorState
{
	WorkshopId id;
	std::string name, subtype;
	std::int32_t priority{ 1 }, maxPriority{ 1 };
	bool suspended{}, acceptGenerated{}, autoCraftMissing{}, linkedStockpile{};
	bool butcherCorpses{}, butcherExcess{}, catchFish{}, processFish{};
	std::uint32_t productCount{}, queuedJobs{};
	bool operator==( const WorkshopInspectorState& ) const = default;
};

struct StockpileInspectorState
{
	StockpileId id;
	std::string name;
	std::int32_t priority{ 1 }, maxPriority{ 1 }, capacity{}, itemCount{}, reserved{};
	bool suspended{}, pullFromOthers{}, allowPullFromHere{};
	std::vector<TextCountRow> contents;
	bool operator==( const StockpileInspectorState& ) const = default;
};

struct AgricultureInspectorState
{
	AgricultureTarget target;
	std::string name, product;
	std::int32_t priority{ 1 }, maxPriority{ 1 }, plots{}, planted{}, ready{};
	std::int32_t male{}, female{}, total{}, maxMale{}, maxFemale{}, foodCurrent{}, foodMax{}, hayCurrent{}, hayMax{};
	bool suspended{}, harvest{}, harvestHay{}, tame{}, pick{}, plant{}, fell{};
	bool operator==( const AgricultureInspectorState& ) const = default;
};

struct PointerPosition
{
	std::int32_t x{}, y{};
	bool operator==( const PointerPosition& ) const = default;
};

struct SelectionConfigurationState
{
	std::string actionLabel;
	std::optional<WorldPosition> cursor, anchor;
	std::optional<PointerPosition> pointer;
	std::string sizeLabel;
	bool active{}, canRotate{};
	std::uint8_t rotation{};
	bool operator==( const SelectionConfigurationState& ) const = default;
};

struct InspectorState
{
	WorldEpoch world;
	Revision revision;
	bool acceptsWorldActions{};
	InspectorKind kind{ InspectorKind::None };
	std::optional<EntityRef> selected, previous;
	std::vector<std::string> professionChoices;
	std::optional<TileInspectorState> tile;
	std::optional<CreatureInspectorState> creature;
	std::optional<WorkshopInspectorState> workshop;
	std::optional<StockpileInspectorState> stockpile;
	std::optional<AgricultureInspectorState> agriculture;
	SelectionConfigurationState selection;
	bool creatureStatsOpen{};
	bool creatureSkillsOpen{};
	bool creatureDetailsOpen{};
	std::optional<UniformSlot> equipmentSlotEditor;
	CatalogId equipmentDraftType;
	std::optional<CatalogId> equipmentDraftMaterial;
	std::optional<RequestId> pendingAction;
	std::string status;
	bool operator==( const InspectorState& ) const = default;
};
} // namespace ingnomia::ui::inspector
