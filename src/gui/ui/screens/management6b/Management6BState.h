/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../actions/UiActions.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ingnomia::ui::management6b
{
enum class View : std::uint8_t
{
	Citizens,
	Professions,
	Schedules,
	Inventory,
	Creature
};
enum class Sort : std::uint8_t
{
	Name,
	Profession,
	Total,
	Stock
};
enum class ManagedScheduleActivity : std::uint8_t
{
	None,
	Eat,
	Sleep,
	Training
};

struct SkillRow
{
	CatalogId id;
	std::string name, group;
	std::int32_t level {};
	float experience {};
	bool active {};
	bool operator==( const SkillRow& ) const = default;
};

struct PopulationRow
{
	CreatureId id;
	std::string name;
	ProfessionId profession;
	std::vector<SkillRow> skills;
	bool operator==( const PopulationRow& ) const = default;
};

struct ProfessionRow
{
	ProfessionId id;
	std::string name;
	std::vector<CatalogId> skills;
	bool operator==( const ProfessionRow& ) const = default;
};

struct ScheduleRow
{
	CreatureId creature;
	std::string name;
	std::array<ManagedScheduleActivity, 24> hours {};
	bool operator==( const ScheduleRow& ) const = default;
};

struct InventoryRow
{
	InventoryRowId id;
	std::string name;
	std::uint32_t total {}, inJobs {}, stockpiled {}, equipped {}, constructed {}, loose {}, totalValue {};
	std::string spriteSheet;
	int spriteX {};
	int spriteY {};
	int spriteWidth {};
	int spriteHeight {};
	int spriteSheetWidth {};
	int spriteSheetHeight {};
	bool watched {};
	bool operator==( const InventoryRow& ) const = default;
};

struct InventoryHistoryPoint
{
	std::int32_t dayIndex {}, total {}, created {}, destroyed {};
	bool operator==( const InventoryHistoryPoint& ) const = default;
};

struct EquipmentSlotRow
{
	std::string slot, item, material;
	bool operator==( const EquipmentSlotRow& ) const = default;
};

struct CreatureDetail
{
	CreatureId id;
	std::string name, profession, activity;
	std::int32_t strength {}, dexterity {}, constitution {}, intelligence {}, wisdom {}, charisma {};
	std::int32_t hunger {}, thirst {}, sleep {}, happiness {};
	std::vector<EquipmentSlotRow> equipment;
	bool operator==( const CreatureDetail& ) const = default;
};

template <class T>
struct Snapshot
{
	WorldEpoch world;
	Revision revision;
	T value;
};

template <class T>
struct RowPatch
{
	WorldEpoch world;
	Revision baseRevision, revision;
	T row;
};

struct Management6BState
{
	WorldEpoch world;
	Revision revision, populationRevision, professionRevision, scheduleRevision, inventoryRevision, creatureRevision;
	bool acceptsWorldActions {}, open {}, populationOpen {}, inventoryOpen {}, loadingPopulation {}, loadingInventory {}, stalePopulation {}, staleInventory {};
	View view { View::Citizens };
	View populationView { View::Citizens };
	Sort populationSort { Sort::Name }, inventorySort { Sort::Name };
	std::string populationFilter, inventoryFilter, inventoryCategory, status;
	bool inventoryOwnedOnly {};
	std::vector<PopulationRow> population;
	std::vector<ProfessionRow> professions;
	std::vector<ScheduleRow> schedules;
	std::vector<InventoryRow> inventory;
	std::optional<InventoryRowId> historyTarget;
	std::vector<InventoryHistoryPoint> inventoryHistory;
	bool inventoryHistoryLoading {};
	std::vector<InventoryRowId> collapsedInventory;
	std::optional<CreatureDetail> creature;
	std::optional<CreatureId> selectedCreature;
	std::optional<InventoryRowId> selectedInventory;
	std::optional<ScheduleCellId> selectedScheduleCell;
	std::optional<ProfessionId> selectedProfession;
	std::size_t populationPage {}, inventoryPage {};
	static constexpr std::size_t pageSize = 64;
	std::optional<RequestId> pendingAction;
	bool operator==( const Management6BState& ) const = default;
};
} // namespace ingnomia::ui::management6b
