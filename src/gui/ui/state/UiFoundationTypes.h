/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include <array>
#include <bitset>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <utility>

namespace ingnomia::ui
{

template<class Tag, class Rep>
struct StrongId
{
	Rep value{};

	[[nodiscard]] constexpr explicit operator bool() const noexcept
	{
		return value != Rep{};
	}

	auto operator<=>( const StrongId& ) const = default;
};

template<class Tag>
struct NamedId
{
	std::string value;

	NamedId() = default;
	explicit NamedId( const char* id ) : value( id ) {}
	explicit NamedId( std::string id ) : value( std::move( id ) ) {}
	explicit NamedId( std::string_view id ) : value( id ) {}

	[[nodiscard]] explicit operator bool() const noexcept
	{
		return !value.empty();
	}

	auto operator<=>( const NamedId& ) const = default;
};

using WorldEpoch = StrongId<struct WorldEpochTag, std::uint64_t>;
using Revision = StrongId<struct RevisionTag, std::uint64_t>;
using RequestId = StrongId<struct RequestIdTag, std::uint64_t>;
using TileId = StrongId<struct TileIdTag, std::uint32_t>;
using CreatureId = StrongId<struct CreatureIdTag, std::uint32_t>;
using DesignationId = StrongId<struct DesignationIdTag, std::uint32_t>;
using WorkshopId = StrongId<struct WorkshopIdTag, std::uint32_t>;
using StockpileId = StrongId<struct StockpileIdTag, std::uint32_t>;
using CraftJobId = StrongId<struct CraftJobIdTag, std::uint32_t>;
using MechanismId = StrongId<struct MechanismIdTag, std::uint32_t>;
using AutomatonId = StrongId<struct AutomatonIdTag, std::uint32_t>;
using SquadId = StrongId<struct SquadIdTag, std::uint32_t>;
using MilitaryRoleId = StrongId<struct MilitaryRoleIdTag, std::uint32_t>;
using NeighborId = StrongId<struct NeighborIdTag, std::uint32_t>;
using MissionId = StrongId<struct MissionIdTag, std::uint32_t>;
using EventResponseTargetId = StrongId<struct EventResponseTargetIdTag, std::uint32_t>;
using PromptInstanceId = StrongId<struct PromptInstanceIdTag, std::uint64_t>;
using ModalInstanceId = StrongId<struct ModalInstanceIdTag, std::uint64_t>;
using FocusToken = StrongId<struct FocusTokenTag, std::uint64_t>;

using CatalogId = NamedId<struct CatalogIdTag>;
using AssetId = NamedId<struct AssetIdTag>;
using LocalizationKey = NamedId<struct LocalizationKeyTag>;
using RouteId = NamedId<struct RouteIdTag>;
using DocumentId = NamedId<struct DocumentIdTag>;
using ActionId = NamedId<struct ActionIdTag>;
using ToolId = NamedId<struct ToolIdTag>;
using SettingId = NamedId<struct SettingIdTag>;
using ProfessionId = NamedId<struct ProfessionIdTag>;
using PresetId = NamedId<struct PresetIdTag>;
using NewGameFieldId = NamedId<struct NewGameFieldIdTag>;

struct SaveSlotId
{
	std::string relativeKey;
	auto operator<=>( const SaveSlotId& ) const = default;
};

struct SaveKingdomId
{
	std::string relativeKey;
	auto operator<=>( const SaveKingdomId& ) const = default;
};

struct WorldPosition
{
	std::int32_t x{};
	std::int32_t y{};
	std::int32_t z{};
	auto operator<=>( const WorldPosition& ) const = default;
};

enum class RouteKind : std::uint8_t
{
	Primary,
	Workbench,
	Dock,
	Overlay
};

enum class WorldPhase : std::uint8_t
{
	NoWorld,
	Generating,
	Loading,
	Ready,
	Saving,
	Unloading,
	Failed
};

enum class PauseReason : std::uint8_t
{
	None,
	Player,
	PauseMenu,
	EventPrompt,
	Save,
	WorldTransition
};

enum class GameSpeed : std::uint8_t
{
	Normal,
	Fast
};

enum class Season : std::uint8_t
{
	Spring,
	Summer,
	Autumn,
	Winter,
	Unknown
};

enum class DaylightPhase : std::uint8_t
{
	Night,
	Dawn,
	Day,
	Dusk,
	Unknown
};

enum class ModalKind : std::uint8_t
{
	EventPrompt,
	DestructiveConfirmation,
	Error
};

enum class ChannelId : std::uint8_t
{
	Lifecycle,
	Clock,
	Settlement,
	View,
	Tool,
	Tile,
	Creature,
	StockpileInfo,
	StockpileContent,
	WorkshopInfo,
	WorkshopQueue,
	Trade,
	Population,
	Schedule,
	Inventory,
	Military,
	Diplomacy,
	Mission,
	EventPrompt,
	Settings,
	NewGame,
	LoadGame,
	Count
};

enum class UpdateKind : std::uint8_t
{
	Snapshot,
	Patch,
	Clear
};

struct NavigationState
{
	RouteId primary;
	std::optional<RouteId> workbench;
	// Workbenches are independent floating windows. `workbench` remains the
	// frontmost/active route for Escape and compatibility with older callers.
	std::vector<RouteId> workbenches;
	std::optional<RouteId> dock;
	std::vector<RouteId> overlays;

	bool operator==( const NavigationState& ) const = default;
};

struct ModalEntry
{
	ModalInstanceId id;
	ModalKind kind{ ModalKind::Error };
	DocumentId document;
	FocusToken returnFocus;
	bool dismissOnEscape{};
	bool blocksWorldInput{ true };

	bool operator==( const ModalEntry& ) const = default;
};

struct UiLifecycleState
{
	WorldEpoch world;
	WorldPhase phase{ WorldPhase::NoWorld };
	bool acceptsWorldActions{};

	bool operator==( const UiLifecycleState& ) const = default;
};

struct SettlementSummary
{
	std::string kingdomName;
	std::uint32_t gnomes{};
	std::uint32_t animals{};
	std::uint32_t items{};

	bool operator==( const SettlementSummary& ) const = default;
};

struct ClockCalendarState
{
	std::uint8_t minute{};
	std::uint8_t hour{};
	std::uint16_t day{};
	std::uint16_t year{};
	Season season{ Season::Unknown };
	DaylightPhase daylight{ DaylightPhase::Unknown };
	std::optional<std::uint16_t> nextSunEventMinute;
	bool paused{};
	PauseReason pauseReason{ PauseReason::None };
	GameSpeed speed{ GameSpeed::Normal };

	bool operator==( const ClockCalendarState& ) const = default;
};

struct CameraState
{
	WorldPosition center;
	std::int32_t viewLevel{};
	std::int32_t minLevel{};
	std::int32_t maxLevel{};
	std::uint8_t rotation{};
	float zoom{ 1.0f };

	bool operator==( const CameraState& ) const = default;
};

struct RenderOverlayState
{
	bool designations{};
	bool jobs{};
	bool loweredWalls{};
	bool axles{};

	bool operator==( const RenderOverlayState& ) const = default;
};

enum class ToolCategory : std::uint8_t
{
	Inspect,
	Dig,
	Build,
	Agriculture,
	Designation,
	Job,
	Magic
};

enum class ToolPhase : std::uint8_t
{
	Inactive,
	ChoosingCatalog,
	ChoosingMaterials,
	Preview,
	Dragging
};

struct ToolState
{
	std::optional<ToolId> active;
	ToolCategory category{ ToolCategory::Inspect };
	ToolPhase phase{ ToolPhase::Inactive };
	std::optional<CatalogId> item;
	std::vector<CatalogId> materials;
	bool repeat{};
	bool canRotate{};
	std::uint8_t rotation{};

	bool operator==( const ToolState& ) const = default;
};

struct SelectionSummary
{
	std::optional<WorldPosition> cursor;
	std::optional<WorldPosition> anchor;
	std::uint32_t width{};
	std::uint32_t height{};
	std::uint32_t depth{};
	std::uint32_t validTiles{};
	std::uint32_t invalidTiles{};
	bool hollow{};

	bool operator==( const SelectionSummary& ) const = default;
};

enum class DirtyVariable : std::uint8_t
{
	Lifecycle,
	Navigation,
	ModalStack,
	ModalEventPrompt,
	Settlement,
	Clock,
	Camera,
	Overlays,
	Tool,
	Selection,
	PendingActions,
	Count
};

class DirtySet
{
public:
	void add( DirtyVariable variable ) noexcept
	{
		bits_.set( static_cast<std::size_t>( variable ) );
	}

	void merge( const DirtySet& other ) noexcept
	{
		bits_ |= other.bits_;
	}

	[[nodiscard]] bool contains( DirtyVariable variable ) const noexcept
	{
		return bits_.test( static_cast<std::size_t>( variable ) );
	}

	[[nodiscard]] bool empty() const noexcept
	{
		return bits_.none();
	}

	[[nodiscard]] std::size_t size() const noexcept
	{
		return bits_.count();
	}

	[[nodiscard]] std::vector<std::string_view> bindingNames() const;
	[[nodiscard]] static constexpr std::string_view bindingName( DirtyVariable variable ) noexcept;
	[[nodiscard]] static DirtySet worldReset();

private:
	std::bitset<static_cast<std::size_t>( DirtyVariable::Count )> bits_;
};

constexpr std::string_view DirtySet::bindingName( DirtyVariable variable ) noexcept
{
	switch( variable )
	{
	case DirtyVariable::Lifecycle: return "ui_shell.lifecycle";
	case DirtyVariable::Navigation: return "ui_shell.navigation";
	case DirtyVariable::ModalStack: return "ui_modal.stack";
	case DirtyVariable::ModalEventPrompt: return "ui_modal.event_prompt";
	case DirtyVariable::Settlement: return "ui_hud.settlement";
	case DirtyVariable::Clock: return "ui_hud.clock";
	case DirtyVariable::Camera: return "ui_hud.camera";
	case DirtyVariable::Overlays: return "ui_hud.overlays";
	case DirtyVariable::Tool: return "ui_tools.tool";
	case DirtyVariable::Selection: return "ui_tools.selection";
	case DirtyVariable::PendingActions: return "ui_shell.pending_actions";
	case DirtyVariable::Count: break;
	}
	return {};
}

inline std::vector<std::string_view> DirtySet::bindingNames() const
{
	std::vector<std::string_view> names;
	names.reserve( size() );
	for( std::size_t index = 0; index < static_cast<std::size_t>( DirtyVariable::Count ); ++index )
	{
		const auto variable = static_cast<DirtyVariable>( index );
		if( contains( variable ) ) names.push_back( bindingName( variable ) );
	}
	return names;
}

inline DirtySet DirtySet::worldReset()
{
	DirtySet dirty;
	dirty.add( DirtyVariable::Lifecycle );
	dirty.add( DirtyVariable::Settlement );
	dirty.add( DirtyVariable::Clock );
	dirty.add( DirtyVariable::Camera );
	dirty.add( DirtyVariable::Overlays );
	dirty.add( DirtyVariable::Tool );
	dirty.add( DirtyVariable::Selection );
	dirty.add( DirtyVariable::PendingActions );
	return dirty;
}

} // namespace ingnomia::ui
