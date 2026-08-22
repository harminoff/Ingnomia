/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "UiNavigation.h"

#include "UiRegistries.h"

#include <algorithm>

namespace ingnomia::ui
{

UiRouter::UiRouter( bool developmentBuild ) : developmentBuild_( developmentBuild )
{
	state_.primary = RouteId{ "shell.main_menu" };
}

bool UiRouter::gameContext() const noexcept
{
	return state_.primary.value == "game.hud";
}

NavigationMutation UiRouter::changed()
{
	NavigationMutation mutation{ NavigationResult::Changed };
	mutation.dirty.add( DirtyVariable::Navigation );
	return mutation;
}

NavigationMutation UiRouter::open( const RouteId& route )
{
	const auto* definition = UiRegistries::findRoute( route.value, developmentBuild_ );
	if( !definition )
	{
		const auto* anyDefinition = UiRegistries::findRoute( route.value, true );
		return { anyDefinition ? NavigationResult::UnavailableRoute : NavigationResult::UnknownRoute };
	}

	switch( definition->kind )
	{
	case RouteKind::Primary:
		if( state_.primary == route && !state_.workbench && !state_.dock && state_.overlays.empty() )
			return { NavigationResult::Unchanged };
		state_.primary = route;
		state_.workbench.reset();
		state_.workbenches.clear();
		state_.dock.reset();
		state_.overlays.clear();
		return changed();

	case RouteKind::Workbench:
		if( !gameContext() ) return { NavigationResult::IllegalContext };
		if( state_.workbench == route ) return { NavigationResult::Unchanged };
		if( std::ranges::find( state_.workbenches, route ) != state_.workbenches.end() )
		{
			state_.workbench = route;
			return changed();
		}
		state_.workbenches.push_back( route );
		state_.workbench = route;
		return changed();

	case RouteKind::Dock:
		if( !gameContext() ) return { NavigationResult::IllegalContext };
		if( state_.dock == route ) return { NavigationResult::Unchanged };
		state_.dock = route;
		return changed();

	case RouteKind::Overlay:
		if( !gameContext() ) return { NavigationResult::IllegalContext };
		if( !state_.overlays.empty() && state_.overlays.back() == route ) return { NavigationResult::Unchanged };
		if( std::ranges::find( state_.overlays, route ) != state_.overlays.end() )
			return { NavigationResult::AlreadyOpen };
		state_.overlays.push_back( route );
		return changed();
	}

	return { NavigationResult::UnknownRoute };
}

NavigationMutation UiRouter::close( const RouteId& route )
{
	const auto* definition = UiRegistries::findRoute( route.value, developmentBuild_ );
	if( !definition )
	{
		const auto* anyDefinition = UiRegistries::findRoute( route.value, true );
		return { anyDefinition ? NavigationResult::UnavailableRoute : NavigationResult::UnknownRoute };
	}

	switch( definition->kind )
	{
	case RouteKind::Primary:
		return { NavigationResult::CannotClosePrimary };
	case RouteKind::Workbench:
		if( std::ranges::find( state_.workbenches, route ) == state_.workbenches.end() ) return { NavigationResult::Unchanged };
		std::erase( state_.workbenches, route );
		if( state_.workbench == route )
			state_.workbench = state_.workbenches.empty() ? std::nullopt : std::optional<RouteId>{ state_.workbenches.back() };
		return changed();
	case RouteKind::Dock:
		if( state_.dock != route ) return { NavigationResult::Unchanged };
		state_.dock.reset();
		return changed();
	case RouteKind::Overlay:
		if( state_.overlays.empty() ) return { NavigationResult::Unchanged };
		if( state_.overlays.back() != route ) return { NavigationResult::NotTopmost };
		state_.overlays.pop_back();
		return changed();
	}

	return { NavigationResult::UnknownRoute };
}

NavigationMutation UiRouter::back()
{
	if( !state_.overlays.empty() )
	{
		state_.overlays.pop_back();
		return changed();
	}
	if( state_.workbench )
	{
		return close( *state_.workbench );
	}
	if( state_.dock )
	{
		state_.dock.reset();
		return changed();
	}
	return { NavigationResult::Unchanged };
}

const ModalEntry* ModalController::top() const noexcept
{
	return stack_.empty() ? nullptr : &stack_.back();
}

bool ModalController::blocksWorldInput() const noexcept
{
	const auto* entry = top();
	return entry && entry->blocksWorldInput;
}

bool ModalController::canInteract( ModalInstanceId id ) const noexcept
{
	const auto* entry = top();
	return entry && entry->id == id;
}

bool ModalController::documentMatchesKind( const ModalEntry& entry ) noexcept
{
	switch( entry.kind )
	{
	case ModalKind::EventPrompt: return entry.document.value == "doc.event_prompt";
	case ModalKind::DestructiveConfirmation: return entry.document.value == "doc.confirm_destructive";
	case ModalKind::Error: return entry.document.value == "doc.error_fallback";
	}
	return false;
}

ModalMutation ModalController::changed( const ModalEntry* removed )
{
	ModalMutation mutation{ ModalResult::Changed };
	mutation.dirty.add( DirtyVariable::ModalStack );
	if( removed && removed->kind == ModalKind::EventPrompt )
		mutation.dirty.add( DirtyVariable::ModalEventPrompt );
	if( removed ) mutation.restoreFocus = removed->returnFocus;
	return mutation;
}

ModalMutation ModalController::push( ModalEntry entry )
{
	if( !entry.id || !entry.returnFocus ) return { ModalResult::InvalidId };
	if( !entry.blocksWorldInput ) return { ModalResult::MissingInputBlocker };
	if( !documentMatchesKind( entry ) || !UiRegistries::findDocument( entry.document.value, true ) )
		return { ModalResult::InvalidDocument };
	if( std::ranges::any_of( stack_, [&]( const ModalEntry& current ) { return current.id == entry.id; } ) )
		return { ModalResult::DuplicateId };

	const bool eventPrompt = entry.kind == ModalKind::EventPrompt;
	stack_.push_back( std::move( entry ) );
	auto mutation = changed();
	if( eventPrompt ) mutation.dirty.add( DirtyVariable::ModalEventPrompt );
	return mutation;
}

ModalMutation ModalController::closeTop( ModalInstanceId id )
{
	if( stack_.empty() ) return { ModalResult::Unchanged };
	if( stack_.back().id != id ) return { ModalResult::NotTopmost };
	const ModalEntry removed = stack_.back();
	stack_.pop_back();
	return changed( &removed );
}

ModalMutation ModalController::dismissTopOnEscape()
{
	if( stack_.empty() ) return { ModalResult::Unchanged };
	if( !stack_.back().dismissOnEscape ) return { ModalResult::NotDismissible };
	const ModalEntry removed = stack_.back();
	stack_.pop_back();
	return changed( &removed );
}

ModalMutation ModalController::clear()
{
	if( stack_.empty() ) return { ModalResult::Unchanged };
	stack_.clear();
	auto mutation = changed();
	// Clearing a stack can remove an event below the former top as well.
	mutation.dirty.add( DirtyVariable::ModalEventPrompt );
	return mutation;
}

} // namespace ingnomia::ui
