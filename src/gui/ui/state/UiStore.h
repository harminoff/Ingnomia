/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "UiFoundationTypes.h"

#include <array>

namespace ingnomia::ui
{

using UiStorePayload = std::variant<UiLifecycleState, SettlementSummary, ClockCalendarState, CameraState,
	RenderOverlayState, ToolState, SelectionSummary>;

struct UiStoreUpdate
{
	WorldEpoch world;
	Revision revision;
	ChannelId channel{ ChannelId::Lifecycle };
	UpdateKind kind{ UpdateKind::Snapshot };
	UiStorePayload payload;
};

enum class StoreApplyStatus : std::uint8_t
{
	Applied,
	Unchanged,
	InvalidEpoch,
	StaleEpoch,
	InactiveWorld,
	StaleRevision,
	InvalidRevision,
	UnsupportedChannel,
	PayloadMismatch,
	InvalidPayload
};

struct StoreApplyResult
{
	StoreApplyStatus status{ StoreApplyStatus::Unchanged };
	DirtySet dirty;

	StoreApplyResult() = default;
	StoreApplyResult( StoreApplyStatus statusValue ) : status( statusValue ) {}
	StoreApplyResult( StoreApplyStatus statusValue, DirtySet dirtyValue )
		: status( statusValue ), dirty( std::move( dirtyValue ) ) {}

	[[nodiscard]] bool accepted() const noexcept
	{
		return status == StoreApplyStatus::Applied || status == StoreApplyStatus::Unchanged;
	}
};

struct UiStoreState
{
	UiLifecycleState lifecycle;
	SettlementSummary settlement;
	ClockCalendarState clock;
	CameraState camera;
	RenderOverlayState overlays;
	ToolState tool;
	SelectionSummary selection;

	bool operator==( const UiStoreState& ) const = default;
};

class UiStore
{
public:
	UiStore();

	[[nodiscard]] const UiStoreState& state() const noexcept { return state_; }
	[[nodiscard]] WorldEpoch activeWorld() const noexcept { return state_.lifecycle.world; }
	[[nodiscard]] Revision channelRevision( ChannelId channel ) const noexcept;

	[[nodiscard]] StoreApplyResult beginWorld( WorldEpoch world, WorldPhase phase );
	[[nodiscard]] StoreApplyResult worldWillUnload( WorldEpoch world );
	[[nodiscard]] StoreApplyResult apply( const UiStoreUpdate& update );

private:
	[[nodiscard]] StoreApplyResult applyPayload( ChannelId channel, UpdateKind kind, const UiStorePayload& payload );
	void resetWorldValues();

	UiStoreState state_;
	std::array<Revision, static_cast<std::size_t>( ChannelId::Count )> revisions_{};
};

} // namespace ingnomia::ui
