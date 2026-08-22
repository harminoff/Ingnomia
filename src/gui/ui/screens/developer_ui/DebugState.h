/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../state/UiFoundationTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace ingnomia::ui::debug
{
enum class Page : std::uint8_t
{
	Gnomes,
	Items,
	Game,
	Diagnostics
};
struct NamedId
{
	std::string name;
	std::uint32_t id {};
	bool operator==( const NamedId& ) const = default;
};
struct ItemCatalog
{
	std::vector<std::string> groups, items, materials1, materials2;
	std::int32_t componentCount {};
	bool operator==( const ItemCatalog& ) const = default;
};
struct UpdateCounters
{
	std::uint64_t notifications {}, renderedNotifications {}, dirtyBindings {}, fullSnapshots {}, rowPatches {}, staleEpochRejects {}, staleRevisionRejects {};
	bool operator==( const UpdateCounters& ) const = default;
};
struct DebugState
{
	WorldEpoch world;
	Revision revision;
	bool enabled {}, open {};
	Page page { Page::Diagnostics };
	std::string search, status;
	std::vector<NamedId> gnomes;
	ItemCatalog catalog;
	std::optional<std::uint32_t> selectedGnome;
	UpdateCounters counters;
	bool operator==( const DebugState& ) const = default;
};
enum class ActionKind : std::uint8_t
{
	RequestGnomes,
	RequestGroups,
	RequestItems,
	RequestMaterials,
	SpawnCreature,
	SetNeed,
	KillGnome,
	SpawnItem,
	SetWindowSize,
	SetNeedDecayMultiplier,
	SetDisableNeedDecay
};
struct DebugAction
{
	ActionKind kind {};
	std::uint32_t gnome {};
	std::string primary, secondary;
	std::vector<std::string> materials;
	float value {};
	std::int32_t count {}, x {}, y {}, z {}, width {}, height {};
	bool enabled {};
};
} // namespace ingnomia::ui::debug
