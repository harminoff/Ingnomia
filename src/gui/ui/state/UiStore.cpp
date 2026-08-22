/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "UiStore.h"

namespace ingnomia::ui
{
namespace
{

template<class Value>
StoreApplyResult replaceIfChanged( Value& target, const Value& replacement, DirtyVariable variable )
{
	if( target == replacement ) return { StoreApplyStatus::Unchanged };
	target = replacement;
	StoreApplyResult result{ StoreApplyStatus::Applied };
	result.dirty.add( variable );
	return result;
}

template<class Value>
StoreApplyResult clearIfChanged( Value& target, DirtyVariable variable )
{
	return replaceIfChanged( target, Value{}, variable );
}

bool validCamera( const CameraState& camera ) noexcept
{
	return camera.minLevel <= camera.maxLevel && camera.viewLevel >= camera.minLevel
		&& camera.viewLevel <= camera.maxLevel && camera.rotation <= 3 && camera.zoom > 0.0f;
}

bool validTool( const ToolState& tool ) noexcept
{
	return tool.rotation <= 3 && ( !tool.active || !tool.active->value.empty() );
}

} // namespace

UiStore::UiStore()
{
	state_.lifecycle.world = WorldEpoch{ 0 };
	state_.lifecycle.phase = WorldPhase::NoWorld;
	state_.lifecycle.acceptsWorldActions = false;
}

Revision UiStore::channelRevision( ChannelId channel ) const noexcept
{
	const auto index = static_cast<std::size_t>( channel );
	return index < revisions_.size() ? revisions_[index] : Revision{};
}

void UiStore::resetWorldValues()
{
	state_.settlement = {};
	state_.clock = {};
	state_.camera = {};
	state_.overlays = {};
	state_.tool = {};
	state_.selection = {};
	revisions_.fill( Revision{} );
}

StoreApplyResult UiStore::beginWorld( WorldEpoch world, WorldPhase phase )
{
	if( !world ) return { StoreApplyStatus::InvalidEpoch };
	if( world.value <= state_.lifecycle.world.value ) return { StoreApplyStatus::StaleEpoch };
	if( phase != WorldPhase::Generating && phase != WorldPhase::Loading )
		return { StoreApplyStatus::InvalidPayload };

	resetWorldValues();
	state_.lifecycle = UiLifecycleState{ world, phase, false };
	return { StoreApplyStatus::Applied, DirtySet::worldReset() };
}

StoreApplyResult UiStore::worldWillUnload( WorldEpoch world )
{
	if( !world ) return { StoreApplyStatus::InvalidEpoch };
	if( world != state_.lifecycle.world ) return { StoreApplyStatus::StaleEpoch };

	resetWorldValues();
	state_.lifecycle = UiLifecycleState{ world, WorldPhase::Unloading, false };
	return { StoreApplyStatus::Applied, DirtySet::worldReset() };
}

StoreApplyResult UiStore::apply( const UiStoreUpdate& update )
{
	if( !update.world ) return { StoreApplyStatus::InvalidEpoch };
	if( update.world != state_.lifecycle.world ) return { StoreApplyStatus::StaleEpoch };
	if( state_.lifecycle.phase == WorldPhase::Unloading || state_.lifecycle.phase == WorldPhase::NoWorld )
		return { StoreApplyStatus::InactiveWorld };
	if( !update.revision ) return { StoreApplyStatus::InvalidRevision };

	const auto index = static_cast<std::size_t>( update.channel );
	if( index >= revisions_.size() ) return { StoreApplyStatus::UnsupportedChannel };
	if( update.revision.value <= revisions_[index].value ) return { StoreApplyStatus::StaleRevision };

	auto result = applyPayload( update.channel, update.kind, update.payload );
	if( result.accepted() ) revisions_[index] = update.revision;
	return result;
}

StoreApplyResult UiStore::applyPayload( ChannelId channel, UpdateKind kind, const UiStorePayload& payload )
{
	if( kind == UpdateKind::Clear )
	{
		switch( channel )
		{
		case ChannelId::Settlement: return clearIfChanged( state_.settlement, DirtyVariable::Settlement );
		case ChannelId::Clock: return clearIfChanged( state_.clock, DirtyVariable::Clock );
		case ChannelId::View:
		{
			auto result = clearIfChanged( state_.camera, DirtyVariable::Camera );
			auto overlayResult = clearIfChanged( state_.overlays, DirtyVariable::Overlays );
			result.dirty.merge( overlayResult.dirty );
			if( overlayResult.status == StoreApplyStatus::Applied ) result.status = StoreApplyStatus::Applied;
			return result;
		}
		case ChannelId::Tool:
		{
			auto result = clearIfChanged( state_.tool, DirtyVariable::Tool );
			auto selectionResult = clearIfChanged( state_.selection, DirtyVariable::Selection );
			result.dirty.merge( selectionResult.dirty );
			if( selectionResult.status == StoreApplyStatus::Applied ) result.status = StoreApplyStatus::Applied;
			return result;
		}
		default: return { StoreApplyStatus::UnsupportedChannel };
		}
	}

	switch( channel )
	{
	case ChannelId::Lifecycle:
		if( const auto* value = std::get_if<UiLifecycleState>( &payload ) )
		{
			if( value->world != state_.lifecycle.world ) return { StoreApplyStatus::StaleEpoch };
			if( value->acceptsWorldActions != ( value->phase == WorldPhase::Ready ) )
				return { StoreApplyStatus::InvalidPayload };
			return replaceIfChanged( state_.lifecycle, *value, DirtyVariable::Lifecycle );
		}
		break;
	case ChannelId::Settlement:
		if( const auto* value = std::get_if<SettlementSummary>( &payload ) )
			return replaceIfChanged( state_.settlement, *value, DirtyVariable::Settlement );
		break;
	case ChannelId::Clock:
		if( const auto* value = std::get_if<ClockCalendarState>( &payload ) )
		{
			if( value->minute >= 60 || value->hour >= 24 || value->day == 0 )
				return { StoreApplyStatus::InvalidPayload };
			return replaceIfChanged( state_.clock, *value, DirtyVariable::Clock );
		}
		break;
	case ChannelId::View:
		if( const auto* value = std::get_if<CameraState>( &payload ) )
		{
			if( !validCamera( *value ) ) return { StoreApplyStatus::InvalidPayload };
			return replaceIfChanged( state_.camera, *value, DirtyVariable::Camera );
		}
		if( const auto* value = std::get_if<RenderOverlayState>( &payload ) )
			return replaceIfChanged( state_.overlays, *value, DirtyVariable::Overlays );
		break;
	case ChannelId::Tool:
		if( const auto* value = std::get_if<ToolState>( &payload ) )
		{
			if( !validTool( *value ) ) return { StoreApplyStatus::InvalidPayload };
			return replaceIfChanged( state_.tool, *value, DirtyVariable::Tool );
		}
		if( const auto* value = std::get_if<SelectionSummary>( &payload ) )
			return replaceIfChanged( state_.selection, *value, DirtyVariable::Selection );
		break;
	default: return { StoreApplyStatus::UnsupportedChannel };
	}

	return { StoreApplyStatus::PayloadMismatch };
}

} // namespace ingnomia::ui
