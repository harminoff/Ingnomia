/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../actions/UiActions.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ingnomia::ui::management6c
{

enum class View : std::uint8_t { Squads, Roles, Priorities, Neighbors, Missions };
enum class Sort : std::uint8_t { SourceOrder, Name };
enum class LoadState : std::uint8_t { Idle, Loading, Ready, Empty, Error, Stale };
enum class MissionStep : std::uint8_t { None, LeaveMap, Travel, Action, Return, Returned };

struct TargetPriorityRow
{
	CatalogId targetType;
	std::string name;
	MilitaryAttitude attitude{ MilitaryAttitude::Flee };
	bool operator==( const TargetPriorityRow& ) const = default;
};

struct SquadMemberRow
{
	CreatureId id;
	std::string name;
	std::optional<MilitaryRoleId> role;
	bool operator==( const SquadMemberRow& ) const = default;
};

struct SquadRow
{
	SquadId id;
	std::string name;
	bool canMoveUp{}, canMoveDown{};
	std::vector<TargetPriorityRow> priorities;
	std::vector<SquadMemberRow> members;
	bool operator==( const SquadRow& ) const = default;
};

struct MilitaryRoster
{
	std::vector<SquadRow> squads;
	std::vector<SquadMemberRow> unassigned;
	bool operator==( const MilitaryRoster& ) const = default;
};

struct UniformSlotRow
{
	UniformSlot slot{ UniformSlot::ChestArmor };
	std::string name;
	CatalogId type;
	std::optional<CatalogId> material;
	std::vector<CatalogId> possibleTypes;
	std::vector<CatalogId> possibleMaterials;
	bool operator==( const UniformSlotRow& ) const = default;
};

struct MilitaryRoleRow
{
	MilitaryRoleId id;
	std::string name;
	bool civilian{};
	std::vector<UniformSlotRow> uniform;
	bool operator==( const MilitaryRoleRow& ) const = default;
};

struct NeighborRow
{
	NeighborId id;
	bool discovered{};
	std::optional<std::string> name, distance, type, attitude, wealth, economy, military;
	bool canSpy{}, canSabotage{}, canRaid{}, canSendEmissary{};
	bool operator==( const NeighborRow& ) const = default;
};

struct AvailableGnomeRow
{
	CreatureId id;
	std::string name;
	bool operator==( const AvailableGnomeRow& ) const = default;
};

struct MissionResult
{
	std::optional<bool> success;
	std::optional<std::int32_t> totalHours;
	bool operator==( const MissionResult& ) const = default;
};

struct MissionRow
{
	MissionId id;
	MissionType type{ MissionType::None };
	MissionAction action{ MissionAction::None };
	MissionStep step{ MissionStep::None };
	NeighborId target;
	std::vector<CreatureId> participants;
	std::uint64_t startTick{}, nextCheckTick{};
	std::int32_t elapsedHours{};
	MissionResult result;
	bool operator==( const MissionRow& ) const = default;
};

template<class T>
struct Snapshot
{
	WorldEpoch world;
	Revision revision;
	T value;
};

template<class T>
struct RowPatch
{
	WorldEpoch world;
	Revision baseRevision, revision;
	T row;
};

struct PriorityPatch
{
	WorldEpoch world;
	Revision baseRevision, revision;
	SquadId squad;
	std::vector<TargetPriorityRow> priorities;
};

struct MaterialOptionsPatch
{
	WorldEpoch world;
	Revision baseRevision, revision;
	MilitaryRoleId role;
	UniformSlot slot{ UniformSlot::ChestArmor };
	std::vector<CatalogId> materials;
};

enum class DestructiveKind : std::uint8_t { Squad, Role };
struct DestructiveRequest
{
	ModalInstanceId modal;
	DestructiveKind kind{ DestructiveKind::Squad };
	std::variant<SquadId, MilitaryRoleId> target{ SquadId{} };
	std::string displayName;
	bool operator==( const DestructiveRequest& ) const = default;
};

struct MissionDraft
{
	MissionType type{ MissionType::None };
	MissionAction action{ MissionAction::None };
	std::optional<CreatureId> creature;
	bool operator==( const MissionDraft& ) const = default;
};

struct Management6CState
{
	WorldEpoch world;
	Revision revision, squadRevision, roleRevision, neighborRevision, availableGnomeRevision, missionRevision;
	bool acceptsWorldActions{}, open{}, militaryOpen{}, diplomacyOpen{};
	View view{ View::Squads };
	Sort militarySort{ Sort::SourceOrder }, diplomacySort{ Sort::SourceOrder };
	LoadState militaryLoad{ LoadState::Idle }, diplomacyLoad{ LoadState::Idle }, missionLoad{ LoadState::Idle };
	std::string militaryFilter, diplomacyFilter, status;
	MilitaryRoster roster;
	std::vector<MilitaryRoleRow> roles;
	std::vector<NeighborRow> neighbors;
	std::vector<AvailableGnomeRow> availableGnomes;
	std::vector<MissionRow> missions;
	std::optional<SquadId> selectedSquad;
	std::optional<MilitaryRoleId> selectedRole;
	std::optional<CreatureId> selectedMember;
	std::optional<CatalogId> selectedPriority;
	std::optional<UniformSlot> selectedUniformSlot;
	std::optional<NeighborId> selectedNeighbor;
	std::optional<MissionId> selectedMission;
	MissionDraft missionDraft;
	std::optional<DestructiveRequest> destructive;
	std::optional<RequestId> pendingAction;
	bool militarySelectionHidden{}, diplomacySelectionHidden{};
	bool operator==( const Management6CState& ) const = default;
};

} // namespace ingnomia::ui::management6c
