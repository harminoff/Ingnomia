/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6CController.h"

#include <algorithm>
#include <cctype>
#include <tuple>

namespace ingnomia::ui::management6c
{
namespace
{
std::string folded( std::string value )
{
	std::ranges::transform( value, value.begin(), []( unsigned char character ) {
		return static_cast<char>( std::tolower( character ) );
	} );
	return value;
}

bool contains( const std::string& value, const std::string& query )
{
	return folded( value ).find( query ) != std::string::npos;
}

template<class Row, class Id, class Projection>
std::size_t indexOf( const std::vector<Row>& rows, const std::optional<Id>& selected, Projection projection )
{
	if( !selected ) return 0;
	const auto found = std::ranges::find_if( rows, [&]( const Row& row ) { return projection( row ) == *selected; } );
	return found == rows.end() ? rows.size() : static_cast<std::size_t>( std::distance( rows.begin(), found ) );
}

template<class Row, class Id, class Projection>
std::optional<Id> retainedOrNearest( const std::vector<Row>& rows, const std::optional<Id>& previous,
	std::size_t previousIndex, Projection projection )
{
	if( previous && std::ranges::any_of( rows, [&]( const Row& row ) { return projection( row ) == *previous; } ) )
		return previous;
	if( rows.empty() ) return std::nullopt;
	return projection( rows[std::min( previousIndex, rows.size() - 1 )] );
}

bool isEmissaryAction( MissionAction action )
{
	switch( action )
	{
		case MissionAction::Improve:
		case MissionAction::Insult:
		case MissionAction::InviteTrader:
		case MissionAction::InviteAmbassador: return true;
		case MissionAction::None: return false;
	}
	return false;
}

bool supportsMission( const NeighborRow& neighbor, MissionType type, MissionAction action )
{
	if( !neighbor.discovered ) return false;
	switch( type )
	{
		case MissionType::Spy: return neighbor.canSpy && action == MissionAction::None;
		case MissionType::Emissary: return neighbor.canSendEmissary && isEmissaryAction( action );
		case MissionType::Raid: return neighbor.canRaid && action == MissionAction::None;
		case MissionType::Sabotage: return neighbor.canSabotage && action == MissionAction::None;
		case MissionType::None:
		case MissionType::Explore: return false;
	}
	return false;
}

const char* missionTypeName( MissionType type )
{
	switch( type )
	{
		case MissionType::None: return "None";
		case MissionType::Explore: return "Explore";
		case MissionType::Spy: return "Spy";
		case MissionType::Emissary: return "Emissary";
		case MissionType::Raid: return "Raid";
		case MissionType::Sabotage: return "Sabotage";
	}
	return "Unknown";
}
} // namespace

Management6CController::Management6CController( CommandPort& commands, ViewPort& view ) :
	commands_( commands ), views_{ &view }
{
	notify();
}

void Management6CController::addViewPort( ViewPort& view )
{
	if ( std::ranges::find( views_, &view ) != views_.end() ) return;
	views_.push_back( &view );
	view.stateChanged( state_ );
}
void Management6CController::removeViewPort( ViewPort& view )
{
	std::erase( views_, &view );
}
void Management6CController::notify()
{
	++state_.revision.value;
	const auto views = views_;
	for ( auto* view : views )
		if ( view ) view->stateChanged( state_ );
}

void Management6CController::beginWorld( WorldEpoch world )
{
	state_ = {};
	filtersByView_ = {};
	sortsByView_ = {};
	state_.world = world;
	state_.acceptsWorldActions = static_cast<bool>( world );
	notify();
}

void Management6CController::endWorld()
{
	state_ = {};
	filtersByView_ = {};
	sortsByView_ = {};
	notify();
}

void Management6CController::activateViewForInput( View view )
{
    const bool wasMilitary = state_.view == View::Squads || state_.view == View::Roles || state_.view == View::Priorities;
    const auto previous = static_cast<std::size_t>( state_.view );
    filtersByView_[previous] = wasMilitary ? state_.militaryFilter : state_.diplomacyFilter;
    sortsByView_[previous] = wasMilitary ? state_.militarySort : state_.diplomacySort;
    const auto next = static_cast<std::size_t>( view );
    if( view == View::Squads || view == View::Roles || view == View::Priorities )
    {
        state_.militaryFilter = filtersByView_[next];
        state_.militarySort = sortsByView_[next];
    }
    else
    {
        state_.diplomacyFilter = filtersByView_[next];
        state_.diplomacySort = sortsByView_[next];
    }
    state_.view = view;
}
void Management6CController::open(View view)
{
    activateViewForInput(view);
    state_.open = true;
    if(view == View::Squads || view == View::Roles || view == View::Priorities) { state_.militaryOpen = true; state_.militaryView = view; }
    else { state_.diplomacyOpen = true; state_.diplomacyView = view; }
	state_.status.clear();
	updateHiddenSelectionFlags();
	notify();
	refresh();
}

void Management6CController::close()
{
	state_.open = false;
	state_.militaryOpen = false;
	state_.diplomacyOpen = false;
	state_.destructive.reset();
	state_.status.clear();
	notify();
}

void Management6CController::closeMilitary()
{
	state_.militaryOpen = false;
	state_.open = state_.diplomacyOpen;
	state_.destructive.reset();
	notify();
}

void Management6CController::closeDiplomacy()
{
	state_.diplomacyOpen = false;
	state_.open = state_.militaryOpen;
	state_.destructive.reset();
	notify();
}

bool Management6CController::dispatch( std::string_view id, UiActionPayload payload, DispatchOrigin origin )
{
	if( !state_.acceptsWorldActions || !state_.world ) return false;
	const bool refresh = id == "military.refresh" || id == "diplomacy.refresh"
		|| id == "diplomacy.refresh_available_gnomes";
	if( state_.pendingAction && !refresh ) return false;
	UiActionEnvelope action{ ActionId{ id }, RequestId{ nextRequest_++ }, state_.world, std::nullopt, std::move( payload ) };
	const auto result = commands_.dispatch( action, origin );
	if( result.status == CommandStatus::Rejected )
	{
		state_.status = result.error.empty() ? "ui.error.action_rejected" : result.error;
		notify();
		return false;
	}
	state_.status.clear();
	if( result.pending ) state_.pendingAction = action.request;
	notify();
	return true;
}

void Management6CController::refresh()
{
	if( state_.view == View::Squads || state_.view == View::Roles || state_.view == View::Priorities )
	{
		state_.militaryLoad = LoadState::Loading;
		notify();
		dispatch( "military.refresh", NoPayload{} );
	}
	else
	{
		state_.diplomacyLoad = LoadState::Loading;
		state_.missionLoad = LoadState::Loading;
		state_.availableGnomeLoad = LoadState::Idle;
		notify();
		dispatch( "diplomacy.refresh", NoPayload{} );
	}
}

void Management6CController::setMilitaryFilter( std::string value )
{
	state_.militaryFilter = std::move( value );
	updateHiddenSelectionFlags();
	notify();
}

void Management6CController::setDiplomacyFilter( std::string value )
{
	state_.diplomacyFilter = std::move( value );
	updateHiddenSelectionFlags();
	notify();
}

void Management6CController::setMilitarySort( Sort value )
{
	state_.militarySort = value;
	updateHiddenSelectionFlags();
	notify();
}

void Management6CController::setDiplomacySort( Sort value )
{
	state_.diplomacySort = value;
	updateHiddenSelectionFlags();
	notify();
}

void Management6CController::setMilitaryError( std::string value )
{
	state_.militaryLoad = LoadState::Error;
	state_.status = std::move( value );
	notify();
}

void Management6CController::setDiplomacyError( std::string value )
{
	state_.diplomacyLoad = LoadState::Error;
	state_.missionLoad = LoadState::Error;
	state_.status = std::move( value );
	notify();
}

std::vector<SquadRow> Management6CController::visibleSquads() const
{
	auto rows = state_.roster.squads;
	const auto query = folded( state_.militaryFilter );
	if( !query.empty() )
		std::erase_if( rows, [&]( const SquadRow& row ) { return !contains( row.name, query ); } );
	if( state_.militarySort == Sort::Name )
		std::ranges::stable_sort( rows, []( const SquadRow& left, const SquadRow& right ) {
			return std::tie( left.name, left.id.value ) < std::tie( right.name, right.id.value );
		} );
	return rows;
}

std::vector<MilitaryRoleRow> Management6CController::visibleRoles() const
{
	auto rows = state_.roles;
	const auto query = folded( state_.militaryFilter );
	if( !query.empty() )
		std::erase_if( rows, [&]( const MilitaryRoleRow& row ) { return !contains( row.name, query ); } );
	if( state_.militarySort == Sort::Name )
		std::ranges::stable_sort( rows, []( const MilitaryRoleRow& left, const MilitaryRoleRow& right ) {
			return std::tie( left.name, left.id.value ) < std::tie( right.name, right.id.value );
		} );
	return rows;
}

std::vector<NeighborRow> Management6CController::visibleNeighbors() const
{
	auto rows = state_.neighbors;
	const auto query = folded( state_.diplomacyFilter );
	if( !query.empty() )
		std::erase_if( rows, [&]( const NeighborRow& row ) {
			return !row.discovered || !row.name || !contains( *row.name, query );
		} );
	if( state_.diplomacySort == Sort::Name )
		std::ranges::stable_sort( rows, []( const NeighborRow& left, const NeighborRow& right ) {
			if( left.discovered != right.discovered ) return left.discovered;
			if( !left.discovered ) return false;
			return std::tie( left.name, left.id.value ) < std::tie( right.name, right.id.value );
		} );
	return rows;
}

std::vector<MissionRow> Management6CController::visibleMissions() const
{
	auto rows = state_.missions;
	const auto query = folded( state_.diplomacyFilter );
	if( !query.empty() )
		std::erase_if( rows, [&]( const MissionRow& row ) { return !contains( missionTypeName( row.type ), query ); } );
	if( state_.diplomacySort == Sort::Name )
		std::ranges::stable_sort( rows, []( const MissionRow& left, const MissionRow& right ) {
			return std::tie( left.type, left.id.value ) < std::tie( right.type, right.id.value );
		} );
	return rows;
}

void Management6CController::updateHiddenSelectionFlags()
{
	state_.militarySelectionHidden = false;
	if( state_.militaryView == View::Squads || state_.militaryView == View::Priorities )
	{
		const auto visible = visibleSquads();
		state_.militarySelectionHidden = state_.selectedSquad
			&& std::ranges::none_of( visible, [&]( const SquadRow& row ) { return row.id == *state_.selectedSquad; } );
	}
	else if( state_.militaryView == View::Roles )
	{
		const auto visible = visibleRoles();
		state_.militarySelectionHidden = state_.selectedRole
			&& std::ranges::none_of( visible, [&]( const MilitaryRoleRow& row ) { return row.id == *state_.selectedRole; } );
	}
	state_.diplomacySelectionHidden = false;
	if( state_.diplomacyView == View::Neighbors )
	{
		const auto visible = visibleNeighbors();
		state_.diplomacySelectionHidden = state_.selectedNeighbor
			&& std::ranges::none_of( visible, [&]( const NeighborRow& row ) { return row.id == *state_.selectedNeighbor; } );
	}
	else if( state_.view == View::Missions )
	{
		const auto visible = visibleMissions();
		state_.diplomacySelectionHidden = state_.selectedMission
			&& std::ranges::none_of( visible, [&]( const MissionRow& row ) { return row.id == *state_.selectedMission; } );
	}
}

void Management6CController::selectSquad( SquadId id )
{
	const auto found = std::ranges::find_if( state_.roster.squads, [&]( const SquadRow& row ) { return row.id == id; } );
	if( !id || found == state_.roster.squads.end() ) return;
	state_.selectedSquad = id;
	state_.selectedMember = found->members.empty() ? std::nullopt : std::optional{ found->members.front().id };
	state_.selectedPriority = found->priorities.empty() ? std::nullopt : std::optional{ found->priorities.front().targetType };
	updateHiddenSelectionFlags();
	notify();
}

void Management6CController::selectRole( MilitaryRoleId id )
{
	const auto found = std::ranges::find_if( state_.roles, [&]( const MilitaryRoleRow& row ) { return row.id == id; } );
	if( !id || found == state_.roles.end() ) return;
	state_.selectedRole = id;
	state_.selectedUniformSlot = found->uniform.empty() ? std::nullopt : std::optional{ found->uniform.front().slot };
	updateHiddenSelectionFlags();
	notify();
}

void Management6CController::selectMember( CreatureId id )
{
	bool found = std::ranges::any_of( state_.roster.unassigned, [&]( const SquadMemberRow& row ) { return row.id == id; } );
	for( const auto& squad : state_.roster.squads )
		found = found || std::ranges::any_of( squad.members, [&]( const SquadMemberRow& row ) { return row.id == id; } );
	if( id && found )
	{
		state_.selectedMember = id;
		notify();
	}
}

void Management6CController::selectPriority( CatalogId id )
{
	if( !state_.selectedSquad || !id ) return;
	const auto squad = std::ranges::find_if( state_.roster.squads,
		[&]( const SquadRow& row ) { return row.id == *state_.selectedSquad; } );
	if( squad == state_.roster.squads.end()
		|| std::ranges::none_of( squad->priorities, [&]( const TargetPriorityRow& row ) { return row.targetType == id; } ) ) return;
	state_.selectedPriority = std::move( id );
	notify();
}

void Management6CController::selectUniformSlot( UniformSlot slot )
{
	if( !state_.selectedRole ) return;
	const auto role = std::ranges::find_if( state_.roles,
		[&]( const MilitaryRoleRow& row ) { return row.id == *state_.selectedRole; } );
	if( role == state_.roles.end()
		|| std::ranges::none_of( role->uniform, [&]( const UniformSlotRow& row ) { return row.slot == slot; } ) ) return;
	state_.selectedUniformSlot = slot;
	notify();
}

void Management6CController::selectNeighbor( NeighborId id )
{
	const auto found = std::ranges::find_if( state_.neighbors, [&]( const NeighborRow& row ) { return row.id == id; } );
	if( !id || found == state_.neighbors.end() ) return;
	state_.selectedNeighbor = id;
	configureMissionDraft();
	updateHiddenSelectionFlags();
	notify();
	if( state_.missionDraft.type != MissionType::None ) requestAvailableGnomes();
}

void Management6CController::selectMission( MissionId id )
{
	if( !id || std::ranges::none_of( state_.missions, [&]( const MissionRow& row ) { return row.id == id; } ) ) return;
	state_.selectedMission = id;
	updateHiddenSelectionFlags();
	notify();
}

void Management6CController::selectNext()
{
	auto advance = []( const auto& rows, auto selected, auto projection, bool forward ) {
		using Id = std::decay_t<decltype( projection( rows.front() ) )>;
		if( rows.empty() ) return std::optional<Id>{};
		const auto found = selected ? std::ranges::find_if( rows, [&]( const auto& row ) { return projection( row ) == *selected; } ) : rows.end();
		if( found == rows.end() ) return std::optional<Id>{ projection( forward ? rows.front() : rows.back() ) };
		const auto index = static_cast<std::size_t>( std::distance( rows.begin(), found ) );
		const auto next = forward ? std::min( index + 1, rows.size() - 1 ) : ( index == 0 ? 0 : index - 1 );
		return std::optional<Id>{ projection( rows[next] ) };
	};
	if( state_.view == View::Squads || state_.view == View::Priorities )
	{
		const auto rows = visibleSquads(); if( !rows.empty() ) if( auto id = advance( rows, state_.selectedSquad, []( const SquadRow& row ){ return row.id; }, true ) ) selectSquad( *id );
	}
	else if( state_.view == View::Roles )
	{
		const auto rows = visibleRoles(); if( !rows.empty() ) if( auto id = advance( rows, state_.selectedRole, []( const MilitaryRoleRow& row ){ return row.id; }, true ) ) selectRole( *id );
	}
	else if( state_.view == View::Neighbors )
	{
		const auto rows = visibleNeighbors(); if( !rows.empty() ) if( auto id = advance( rows, state_.selectedNeighbor, []( const NeighborRow& row ){ return row.id; }, true ) ) selectNeighbor( *id );
	}
	else
	{
		const auto rows = visibleMissions(); if( !rows.empty() ) if( auto id = advance( rows, state_.selectedMission, []( const MissionRow& row ){ return row.id; }, true ) ) selectMission( *id );
	}
}

void Management6CController::selectPrevious()
{
	auto retreat = []( const auto& rows, auto selected, auto projection ) {
		using Id = std::decay_t<decltype( projection( rows.front() ) )>;
		if( rows.empty() ) return std::optional<Id>{};
		const auto found = selected ? std::ranges::find_if( rows, [&]( const auto& row ) { return projection( row ) == *selected; } ) : rows.end();
		if( found == rows.end() ) return std::optional<Id>{ projection( rows.back() ) };
		const auto index = static_cast<std::size_t>( std::distance( rows.begin(), found ) );
		return std::optional<Id>{ projection( rows[index == 0 ? 0 : index - 1] ) };
	};
	if( state_.view == View::Squads || state_.view == View::Priorities )
	{
		const auto rows = visibleSquads(); if( !rows.empty() ) if( auto id = retreat( rows, state_.selectedSquad, []( const SquadRow& row ){ return row.id; } ) ) selectSquad( *id );
	}
	else if( state_.view == View::Roles )
	{
		const auto rows = visibleRoles(); if( !rows.empty() ) if( auto id = retreat( rows, state_.selectedRole, []( const MilitaryRoleRow& row ){ return row.id; } ) ) selectRole( *id );
	}
	else if( state_.view == View::Neighbors )
	{
		const auto rows = visibleNeighbors(); if( !rows.empty() ) if( auto id = retreat( rows, state_.selectedNeighbor, []( const NeighborRow& row ){ return row.id; } ) ) selectNeighbor( *id );
	}
	else
	{
		const auto rows = visibleMissions(); if( !rows.empty() ) if( auto id = retreat( rows, state_.selectedMission, []( const MissionRow& row ){ return row.id; } ) ) selectMission( *id );
	}
}

void Management6CController::moveMemberSelection( std::int32_t delta )
{
	const std::vector<SquadMemberRow>* rows = &state_.roster.unassigned;
	const bool selectedUnassigned = state_.selectedMember && std::ranges::any_of( state_.roster.unassigned,
		[&]( const SquadMemberRow& row ){ return row.id == *state_.selectedMember; } );
	if( !selectedUnassigned && state_.selectedSquad )
	{
		const auto squad = std::ranges::find_if( state_.roster.squads,
			[&]( const SquadRow& row ){ return row.id == *state_.selectedSquad; } );
		if( squad != state_.roster.squads.end() ) rows = &squad->members;
	}
	if( rows->empty() ) return;
	const auto found = state_.selectedMember ? std::ranges::find_if( *rows,
		[&]( const SquadMemberRow& row ){ return row.id == *state_.selectedMember; } ) : rows->end();
	std::size_t index = found == rows->end() ? 0 : static_cast<std::size_t>( std::distance( rows->begin(), found ) );
	if( delta < 0 ) index = index == 0 ? 0 : index - 1;
	else if( delta > 0 ) index = std::min( index + 1, rows->size() - 1 );
	selectMember( ( *rows )[index].id );
}

void Management6CController::movePrioritySelection( std::int32_t delta )
{
	if( !state_.selectedSquad ) return;
	const auto squad = std::ranges::find_if( state_.roster.squads,
		[&]( const SquadRow& row ){ return row.id == *state_.selectedSquad; } );
	if( squad == state_.roster.squads.end() || squad->priorities.empty() ) return;
	const auto found = state_.selectedPriority ? std::ranges::find_if( squad->priorities,
		[&]( const TargetPriorityRow& row ){ return row.targetType == *state_.selectedPriority; } ) : squad->priorities.end();
	std::size_t index = found == squad->priorities.end() ? 0
		: static_cast<std::size_t>( std::distance( squad->priorities.begin(), found ) );
	if( delta < 0 ) index = index == 0 ? 0 : index - 1;
	else if( delta > 0 ) index = std::min( index + 1, squad->priorities.size() - 1 );
	selectPriority( squad->priorities[index].targetType );
}

void Management6CController::moveUniformSelection( std::int32_t delta )
{
	if( !state_.selectedRole ) return;
	const auto role = std::ranges::find_if( state_.roles,
		[&]( const MilitaryRoleRow& row ){ return row.id == *state_.selectedRole; } );
	if( role == state_.roles.end() || role->uniform.empty() ) return;
	const auto found = state_.selectedUniformSlot ? std::ranges::find_if( role->uniform,
		[&]( const UniformSlotRow& row ){ return row.slot == *state_.selectedUniformSlot; } ) : role->uniform.end();
	std::size_t index = found == role->uniform.end() ? 0
		: static_cast<std::size_t>( std::distance( role->uniform.begin(), found ) );
	if( delta < 0 ) index = index == 0 ? 0 : index - 1;
	else if( delta > 0 ) index = std::min( index + 1, role->uniform.size() - 1 );
	selectUniformSlot( role->uniform[index].slot );
}

void Management6CController::moveMissionGnomeSelection( std::int32_t delta )
{
	if( state_.availableGnomes.empty() ) return;
	const auto found = state_.missionDraft.creature ? std::ranges::find_if( state_.availableGnomes,
		[&]( const AvailableGnomeRow& row ){ return row.id == *state_.missionDraft.creature; } ) : state_.availableGnomes.end();
	std::size_t index = found == state_.availableGnomes.end() ? 0
		: static_cast<std::size_t>( std::distance( state_.availableGnomes.begin(), found ) );
	if( delta < 0 ) index = index == 0 ? 0 : index - 1;
	else if( delta > 0 ) index = std::min( index + 1, state_.availableGnomes.size() - 1 );
	selectMissionGnome( state_.availableGnomes[index].id );
}

bool Management6CController::accepts( WorldEpoch world, Revision incoming, Revision current ) const
{
	return state_.acceptsWorldActions && world == state_.world && incoming.value > current.value;
}

void Management6CController::reconcileMilitarySelection( const std::optional<SquadId>& previousSquad,
	std::size_t previousSquadIndex, const std::optional<MilitaryRoleId>& previousRole, std::size_t previousRoleIndex )
{
	state_.selectedSquad = retainedOrNearest( state_.roster.squads, previousSquad, previousSquadIndex,
		[]( const SquadRow& row ){ return row.id; } );
	state_.selectedRole = retainedOrNearest( state_.roles, previousRole, previousRoleIndex,
		[]( const MilitaryRoleRow& row ){ return row.id; } );
	bool memberFound = false;
	for( const auto& squad : state_.roster.squads )
		memberFound = memberFound || ( state_.selectedMember && std::ranges::any_of( squad.members,
			[&]( const SquadMemberRow& row ){ return row.id == *state_.selectedMember; } ) );
	memberFound = memberFound || ( state_.selectedMember && std::ranges::any_of( state_.roster.unassigned,
		[&]( const SquadMemberRow& row ){ return row.id == *state_.selectedMember; } ) );
	if( !memberFound ) state_.selectedMember.reset();
	if( state_.selectedSquad )
	{
		const auto squad = std::ranges::find_if( state_.roster.squads,
			[&]( const SquadRow& row ){ return row.id == *state_.selectedSquad; } );
		if( squad != state_.roster.squads.end() )
		{
			if( !state_.selectedMember && !squad->members.empty() ) state_.selectedMember = squad->members.front().id;
			if( !state_.selectedPriority || std::ranges::none_of( squad->priorities,
				[&]( const TargetPriorityRow& row ){ return row.targetType == *state_.selectedPriority; } ) )
				state_.selectedPriority = squad->priorities.empty() ? std::nullopt : std::optional{ squad->priorities.front().targetType };
		}
	}
	else state_.selectedPriority.reset();
	if( state_.selectedRole )
	{
		const auto role = std::ranges::find_if( state_.roles,
			[&]( const MilitaryRoleRow& row ){ return row.id == *state_.selectedRole; } );
		if( role != state_.roles.end() && ( !state_.selectedUniformSlot || std::ranges::none_of( role->uniform,
			[&]( const UniformSlotRow& row ){ return row.slot == *state_.selectedUniformSlot; } ) ) )
			state_.selectedUniformSlot = role->uniform.empty() ? std::nullopt : std::optional{ role->uniform.front().slot };
	}
	else state_.selectedUniformSlot.reset();
	updateHiddenSelectionFlags();
}

void Management6CController::reconcileDiplomacySelection( const std::optional<NeighborId>& previousNeighbor,
	std::size_t previousNeighborIndex, const std::optional<MissionId>& previousMission, std::size_t previousMissionIndex )
{
	state_.selectedNeighbor = retainedOrNearest( state_.neighbors, previousNeighbor, previousNeighborIndex,
		[]( const NeighborRow& row ){ return row.id; } );
	state_.selectedMission = retainedOrNearest( state_.missions, previousMission, previousMissionIndex,
		[]( const MissionRow& row ){ return row.id; } );
	if( state_.missionDraft.creature && std::ranges::none_of( state_.availableGnomes,
		[&]( const AvailableGnomeRow& row ){ return row.id == *state_.missionDraft.creature; } ) )
		state_.missionDraft.creature.reset();
	if( !state_.missionDraft.creature && !state_.availableGnomes.empty() )
		state_.missionDraft.creature = state_.availableGnomes.front().id;
	configureMissionDraft();
	updateHiddenSelectionFlags();
}

bool Management6CController::applyMilitary( Snapshot<MilitaryRoster> snapshot )
{
	if( !accepts( snapshot.world, snapshot.revision, state_.squadRevision ) ) return false;
	const auto previousSquad = state_.selectedSquad;
	const auto previousSquadIndex = indexOf( state_.roster.squads, previousSquad, []( const SquadRow& row ){ return row.id; } );
	const auto previousRole = state_.selectedRole;
	const auto previousRoleIndex = indexOf( state_.roles, previousRole, []( const MilitaryRoleRow& row ){ return row.id; } );
	state_.roster = std::move( snapshot.value );
	state_.squadRevision = snapshot.revision;
	state_.pendingAction.reset();
	state_.militaryLoad = state_.roster.squads.empty() && state_.roster.unassigned.empty() ? LoadState::Empty : LoadState::Ready;
	reconcileMilitarySelection( previousSquad, previousSquadIndex, previousRole, previousRoleIndex );
	notify();
	return true;
}

bool Management6CController::applyPriorityPatch( PriorityPatch patch )
{
	if( !state_.acceptsWorldActions || !state_.world || patch.world != state_.world ) return false;
	if( patch.baseRevision != state_.squadRevision || patch.revision.value <= patch.baseRevision.value )
	{
		requestMilitaryResync();
		return false;
	}
	const auto squad = std::ranges::find_if( state_.roster.squads,
		[&]( const SquadRow& row ){ return row.id == patch.squad; } );
	if( squad == state_.roster.squads.end() )
	{
		requestMilitaryResync();
		return false;
	}
	squad->priorities = std::move( patch.priorities );
	state_.squadRevision = patch.revision;
	state_.pendingAction.reset();
	if( state_.selectedSquad == squad->id && ( !state_.selectedPriority || std::ranges::none_of( squad->priorities,
		[&]( const TargetPriorityRow& row ){ return row.targetType == *state_.selectedPriority; } ) ) )
		state_.selectedPriority = squad->priorities.empty() ? std::nullopt : std::optional{ squad->priorities.front().targetType };
	state_.militaryLoad = LoadState::Ready;
	notify();
	return true;
}

bool Management6CController::applyRoles( Snapshot<std::vector<MilitaryRoleRow>> snapshot )
{
	if( !accepts( snapshot.world, snapshot.revision, state_.roleRevision ) ) return false;
	const auto previousSquad = state_.selectedSquad;
	const auto previousSquadIndex = indexOf( state_.roster.squads, previousSquad, []( const SquadRow& row ){ return row.id; } );
	const auto previousRole = state_.selectedRole;
	const auto previousRoleIndex = indexOf( state_.roles, previousRole, []( const MilitaryRoleRow& row ){ return row.id; } );
	state_.roles = std::move( snapshot.value );
	state_.roleRevision = snapshot.revision;
	state_.pendingAction.reset();
	state_.militaryLoad = state_.roles.empty() && state_.roster.squads.empty() && state_.roster.unassigned.empty()
		? LoadState::Empty : LoadState::Ready;
	reconcileMilitarySelection( previousSquad, previousSquadIndex, previousRole, previousRoleIndex );
	notify();
	return true;
}

bool Management6CController::applyMaterialOptions( MaterialOptionsPatch patch )
{
	if( !state_.acceptsWorldActions || !state_.world || patch.world != state_.world ) return false;
	if( patch.baseRevision != state_.roleRevision || patch.revision.value <= patch.baseRevision.value )
	{
		requestMilitaryResync();
		return false;
	}
	const auto role = std::ranges::find_if( state_.roles,
		[&]( const MilitaryRoleRow& row ){ return row.id == patch.role; } );
	if( role == state_.roles.end() ) { requestMilitaryResync(); return false; }
	const auto slot = std::ranges::find_if( role->uniform,
		[&]( const UniformSlotRow& row ){ return row.slot == patch.slot; } );
	if( slot == role->uniform.end() ) { requestMilitaryResync(); return false; }
	slot->possibleMaterials = std::move( patch.materials );
	state_.roleRevision = patch.revision;
	state_.pendingAction.reset();
	notify();
	return true;
}

bool Management6CController::applyNeighbors( Snapshot<std::vector<NeighborRow>> snapshot )
{
	if( !accepts( snapshot.world, snapshot.revision, state_.neighborRevision ) ) return false;
	const auto previousNeighbor = state_.selectedNeighbor;
	const auto previousNeighborIndex = indexOf( state_.neighbors, previousNeighbor, []( const NeighborRow& row ){ return row.id; } );
	const auto previousMission = state_.selectedMission;
	const auto previousMissionIndex = indexOf( state_.missions, previousMission, []( const MissionRow& row ){ return row.id; } );
	state_.neighbors = std::move( snapshot.value );
	state_.neighborRevision = snapshot.revision;
	state_.pendingAction.reset();
	state_.diplomacyLoad = state_.neighbors.empty() ? LoadState::Empty : LoadState::Ready;
	reconcileDiplomacySelection( previousNeighbor, previousNeighborIndex, previousMission, previousMissionIndex );
    if( state_.diplomacyOpen && state_.missionDraft.type != MissionType::None
        && state_.availableGnomeLoad == LoadState::Idle ) requestAvailableGnomes();
	notify();
	return true;
}

bool Management6CController::applyAvailableGnomes( Snapshot<std::vector<AvailableGnomeRow>> snapshot )
{
	if( !accepts( snapshot.world, snapshot.revision, state_.availableGnomeRevision ) ) return false;
	state_.availableGnomes = std::move( snapshot.value );
	state_.availableGnomeRevision = snapshot.revision;
	state_.availableGnomeLoad = state_.availableGnomes.empty() ? LoadState::Empty : LoadState::Ready;
	state_.pendingAction.reset();
	if( state_.missionDraft.creature && std::ranges::none_of( state_.availableGnomes,
		[&]( const AvailableGnomeRow& row ){ return row.id == *state_.missionDraft.creature; } ) )
		state_.missionDraft.creature.reset();
	if( !state_.missionDraft.creature && !state_.availableGnomes.empty() )
		state_.missionDraft.creature = state_.availableGnomes.front().id;
	notify();
	return true;
}

bool Management6CController::applyMissions( Snapshot<std::vector<MissionRow>> snapshot )
{
	if( !accepts( snapshot.world, snapshot.revision, state_.missionRevision ) ) return false;
	const auto previousNeighbor = state_.selectedNeighbor;
	const auto previousNeighborIndex = indexOf( state_.neighbors, previousNeighbor, []( const NeighborRow& row ){ return row.id; } );
	const auto previousMission = state_.selectedMission;
	const auto previousMissionIndex = indexOf( state_.missions, previousMission, []( const MissionRow& row ){ return row.id; } );
	state_.missions = std::move( snapshot.value );
	state_.missionRevision = snapshot.revision;
	state_.pendingAction.reset();
	state_.missionLoad = state_.missions.empty() ? LoadState::Empty : LoadState::Ready;
	reconcileDiplomacySelection( previousNeighbor, previousNeighborIndex, previousMission, previousMissionIndex );
	notify();
	return true;
}

bool Management6CController::applyMissionPatch( RowPatch<MissionRow> patch )
{
	if( !state_.acceptsWorldActions || !state_.world || patch.world != state_.world ) return false;
	if( patch.baseRevision != state_.missionRevision || patch.revision.value <= patch.baseRevision.value || !patch.row.id )
	{
		requestMissionResync();
		return false;
	}
	const auto found = std::ranges::find_if( state_.missions,
		[&]( const MissionRow& row ){ return row.id == patch.row.id; } );
	if( found == state_.missions.end() ) state_.missions.push_back( std::move( patch.row ) );
	else *found = std::move( patch.row );
	state_.missionRevision = patch.revision;
	state_.pendingAction.reset();
	state_.missionLoad = LoadState::Ready;
	updateHiddenSelectionFlags();
	notify();
	return true;
}

void Management6CController::requestMilitaryResync()
{
	if( state_.militaryLoad == LoadState::Stale ) return;
	state_.militaryLoad = LoadState::Stale;
	notify();
	dispatch( "military.refresh", NoPayload{} );
}

void Management6CController::requestDiplomacyResync()
{
	if( state_.diplomacyLoad == LoadState::Stale ) return;
	state_.diplomacyLoad = LoadState::Stale;
	notify();
	dispatch( "diplomacy.refresh", NoPayload{} );
}

void Management6CController::requestMissionResync()
{
	if( state_.missionLoad == LoadState::Stale ) return;
	state_.missionLoad = LoadState::Stale;
	notify();
	dispatch( "diplomacy.refresh", NoPayload{} );
}

void Management6CController::addSquad() { dispatch( "military.add_squad", NoPayload{} ); }

void Management6CController::renameSelectedSquad( std::string name )
{
	if( state_.selectedSquad && !name.empty() )
		dispatch( "military.rename_squad", RenameSquadPayload{ *state_.selectedSquad, std::move( name ) } );
}

void Management6CController::moveSelectedSquad( MoveDirection direction )
{
	if( !state_.selectedSquad || ( direction != MoveDirection::Up && direction != MoveDirection::Down ) ) return;
	const auto squad = std::ranges::find_if( state_.roster.squads,
		[&]( const SquadRow& row ){ return row.id == *state_.selectedSquad; } );
	if( squad == state_.roster.squads.end() || ( direction == MoveDirection::Up && !squad->canMoveUp )
		|| ( direction == MoveDirection::Down && !squad->canMoveDown ) ) return;
	dispatch( "military.move_squad", MoveSquadPayload{ squad->id, direction } );
}

void Management6CController::requestRemoveSelectedSquad()
{
	if( !state_.selectedSquad ) return;
	const auto squad = std::ranges::find_if( state_.roster.squads,
		[&]( const SquadRow& row ){ return row.id == *state_.selectedSquad; } );
	if( squad == state_.roster.squads.end() ) return;
	state_.destructive = DestructiveRequest{ ModalInstanceId{ nextModal_++ }, DestructiveKind::Squad, squad->id, squad->name };
	notify();
}

void Management6CController::removeSelectedMember()
{
	if( !state_.selectedMember ) return;
	const bool assigned = std::ranges::any_of( state_.roster.squads, [&]( const SquadRow& squad ) {
		return std::ranges::any_of( squad.members,
			[&]( const SquadMemberRow& member ){ return member.id == *state_.selectedMember; } );
	} );
	if( assigned ) dispatch( "military.remove_gnome", GnomeTargetPayload{ *state_.selectedMember } );
}

void Management6CController::assignSelectedMemberToSelectedSquad()
{
	if( !state_.selectedMember || !state_.selectedSquad ) return;
	const auto destination = std::ranges::find_if( state_.roster.squads,
		[&]( const SquadRow& row ){ return row.id == *state_.selectedSquad; } );
	if( destination == state_.roster.squads.end() ) return;
	const bool alreadyAssigned = std::ranges::any_of( destination->members,
		[&]( const SquadMemberRow& member ){ return member.id == *state_.selectedMember; } );
	if( !alreadyAssigned )
		dispatch( "military.assign_squad", AssignSquadPayload{ *state_.selectedMember, *state_.selectedSquad } );
}

void Management6CController::assignMemberToSquad( CreatureId creature, SquadId squad )
{
	const bool known = std::ranges::any_of( state_.roster.unassigned, [&]( const SquadMemberRow& m ){ return m.id == creature; } )
		|| std::ranges::any_of( state_.roster.squads, [&]( const SquadRow& s ){ return std::ranges::any_of( s.members, [&]( const SquadMemberRow& m ){ return m.id == creature; } ); } );
	const auto destination = std::ranges::find_if( state_.roster.squads, [&]( const SquadRow& row ){ return row.id == squad; } );
	if( !known || destination == state_.roster.squads.end() ) return;
	if( std::ranges::any_of( destination->members, [&]( const SquadMemberRow& m ){ return m.id == creature; } ) ) return;
	dispatch( "military.assign_squad", AssignSquadPayload{ creature, squad } );
}
void Management6CController::moveSelectedMember( MoveDirection direction )
{
	if( !state_.selectedMember || ( direction != MoveDirection::Up && direction != MoveDirection::Down ) ) return;
	const bool assigned = std::ranges::any_of( state_.roster.squads, [&]( const SquadRow& squad ) {
		return std::ranges::any_of( squad.members,
			[&]( const SquadMemberRow& member ){ return member.id == *state_.selectedMember; } );
	} );
	if( assigned ) dispatch( "military.move_gnome", MoveGnomePayload{ *state_.selectedMember, direction } );
}

void Management6CController::setSelectedAttitude( MilitaryAttitude attitude )
{
	if( state_.selectedSquad && state_.selectedPriority )
		dispatch( "military.set_attitude", SetAttitudePayload{ *state_.selectedSquad, *state_.selectedPriority, attitude } );
}

void Management6CController::moveSelectedPriority( MoveDirection direction )
{
	if( state_.selectedSquad && state_.selectedPriority
		&& ( direction == MoveDirection::Up || direction == MoveDirection::Down ) )
		dispatch( "military.move_priority", MovePriorityPayload{ *state_.selectedSquad, *state_.selectedPriority, direction } );
}

void Management6CController::addRole() { dispatch( "military.add_role", NoPayload{} ); }

void Management6CController::renameSelectedRole( std::string name )
{
	if( state_.selectedRole && !name.empty() )
		dispatch( "military.rename_role", RenameRolePayload{ *state_.selectedRole, std::move( name ) } );
}

void Management6CController::requestRemoveSelectedRole()
{
	if( !state_.selectedRole ) return;
	const auto role = std::ranges::find_if( state_.roles,
		[&]( const MilitaryRoleRow& row ){ return row.id == *state_.selectedRole; } );
	if( role == state_.roles.end() ) return;
	state_.destructive = DestructiveRequest{ ModalInstanceId{ nextModal_++ }, DestructiveKind::Role, role->id, role->name };
	notify();
}

void Management6CController::assignSelectedMemberToRole()
{
	if( state_.selectedMember && state_.selectedRole )
		dispatch( "military.assign_role", AssignRolePayload{ *state_.selectedMember, *state_.selectedRole } );
}

void Management6CController::assignMemberRole( CreatureId creature, MilitaryRoleId role )
{
    if( !creature || !role || std::ranges::none_of( state_.roles,
        [&]( const MilitaryRoleRow& row ){ return row.id == role; } ) ) return;
    bool present = std::ranges::any_of( state_.roster.unassigned,
        [&]( const SquadMemberRow& row ){ return row.id == creature; } );
    for( const auto& squad : state_.roster.squads )
        present = present || std::ranges::any_of( squad.members,
            [&]( const SquadMemberRow& row ){ return row.id == creature; } );
    if( present ) dispatch( "military.assign_role", AssignRolePayload{ creature, role } );
}

void Management6CController::setSelectedRoleCivilian( bool civilian )
{
	if( state_.selectedRole )
		dispatch( "military.set_role_civilian", SetRoleCivilianPayload{ *state_.selectedRole, civilian } );
}

void Management6CController::setSelectedUniform( CatalogId type, std::optional<CatalogId> material )
{
	if( !state_.selectedRole || !state_.selectedUniformSlot || !type ) return;
	const auto role = std::ranges::find_if( state_.roles,
		[&]( const MilitaryRoleRow& row ){ return row.id == *state_.selectedRole; } );
	if( role == state_.roles.end() ) return;
	const auto slot = std::ranges::find_if( role->uniform,
		[&]( const UniformSlotRow& row ){ return row.slot == *state_.selectedUniformSlot; } );
	if( slot == role->uniform.end() || std::ranges::find( slot->possibleTypes, type ) == slot->possibleTypes.end() ) return;
	if( material && material->value != "any" && !slot->possibleMaterials.empty()
		&& std::ranges::find( slot->possibleMaterials, *material ) == slot->possibleMaterials.end() ) return;
	dispatch( "military.set_uniform_slot", SetUniformSlotPayload{ role->id, slot->slot, std::move( type ), std::move( material ) } );
}

void Management6CController::cancelDestructive()
{
	state_.destructive.reset();
	notify();
}

void Management6CController::confirmDestructive( ModalInstanceId modal )
{
	if( !state_.destructive || state_.destructive->modal != modal ) return;
	bool accepted = false;
	if( state_.destructive->kind == DestructiveKind::Squad )
		accepted = dispatch( "military.remove_squad", SquadTargetPayload{ std::get<SquadId>( state_.destructive->target ) },
			DispatchOrigin{ state_.destructive->modal } );
	else
		accepted = dispatch( "military.remove_role", RoleTargetPayload{ std::get<MilitaryRoleId>( state_.destructive->target ) },
			DispatchOrigin{ state_.destructive->modal } );
	if( accepted )
	{
		state_.destructive.reset();
		notify();
	}
}

void Management6CController::requestAvailableGnomes()
{
    state_.availableGnomeLoad = LoadState::Loading;
    notify();
    if( !dispatch( "diplomacy.refresh_available_gnomes", NoPayload{} ) )
    {
        state_.availableGnomeLoad = LoadState::Error;
        notify();
    }
}

void Management6CController::configureMissionDraft()
{
	const auto neighbor = state_.selectedNeighbor ? std::ranges::find_if( state_.neighbors,
		[&]( const NeighborRow& row ){ return row.id == *state_.selectedNeighbor; } ) : state_.neighbors.end();
	if( neighbor == state_.neighbors.end() || !neighbor->discovered )
	{
		state_.missionDraft.type = MissionType::None;
		state_.missionDraft.action = MissionAction::None;
		return;
	}
	if( supportsMission( *neighbor, state_.missionDraft.type, state_.missionDraft.action ) ) return;
	if( neighbor->canSendEmissary )
	{
		state_.missionDraft.type = MissionType::Emissary;
		state_.missionDraft.action = MissionAction::Improve;
	}
	else if( neighbor->canSpy )
	{
		state_.missionDraft.type = MissionType::Spy;
		state_.missionDraft.action = MissionAction::None;
	}
	else if( neighbor->canRaid )
	{
		state_.missionDraft.type = MissionType::Raid;
		state_.missionDraft.action = MissionAction::None;
	}
	else if( neighbor->canSabotage )
	{
		state_.missionDraft.type = MissionType::Sabotage;
		state_.missionDraft.action = MissionAction::None;
	}
	else
	{
		state_.missionDraft.type = MissionType::None;
		state_.missionDraft.action = MissionAction::None;
	}
}

void Management6CController::setMissionType( MissionType type )
{
	if( !state_.selectedNeighbor ) return;
	const auto neighbor = std::ranges::find_if( state_.neighbors,
		[&]( const NeighborRow& row ){ return row.id == *state_.selectedNeighbor; } );
	if( neighbor == state_.neighbors.end() ) return;
	const auto action = type == MissionType::Emissary ? MissionAction::Improve : MissionAction::None;
	if( supportsMission( *neighbor, type, action ) )
	{
		state_.missionDraft.type = type;
		state_.missionDraft.action = action;
		notify();
	}
}

void Management6CController::setMissionAction( MissionAction action )
{
	if( !state_.selectedNeighbor ) return;
	const auto neighbor = std::ranges::find_if( state_.neighbors,
		[&]( const NeighborRow& row ){ return row.id == *state_.selectedNeighbor; } );
	if( neighbor != state_.neighbors.end() && supportsMission( *neighbor, state_.missionDraft.type, action ) )
	{
		state_.missionDraft.action = action;
		notify();
	}
}

void Management6CController::selectMissionGnome( CreatureId creature )
{
	if( creature && std::ranges::any_of( state_.availableGnomes,
		[&]( const AvailableGnomeRow& row ){ return row.id == creature; } ) )
	{
		state_.missionDraft.creature = creature;
		notify();
	}
}

bool Management6CController::canStartDraftMission() const
{
	if( state_.availableGnomeLoad != LoadState::Ready || state_.pendingAction
		|| !state_.selectedNeighbor || !state_.missionDraft.creature ) return false;
	const auto neighbor = std::ranges::find_if( state_.neighbors,
		[&]( const NeighborRow& row ){ return row.id == *state_.selectedNeighbor; } );
	if( neighbor == state_.neighbors.end() || !supportsMission( *neighbor, state_.missionDraft.type, state_.missionDraft.action ) ) return false;
	return std::ranges::any_of( state_.availableGnomes,
		[&]( const AvailableGnomeRow& row ){ return row.id == *state_.missionDraft.creature; } );
}

void Management6CController::startMission()
{
	if( !canStartDraftMission() ) return;
	if( dispatch( "diplomacy.start_mission", StartMissionPayload{ state_.missionDraft.type,
		state_.missionDraft.action, *state_.selectedNeighbor, *state_.missionDraft.creature } ) )
		requestAvailableGnomes();
}

void Management6CController::onActionFinished( RequestId request, CommandResult result )
{
	if( state_.pendingAction != request ) return;
	state_.pendingAction.reset();
	state_.status = result.status == CommandStatus::Rejected ? result.error : std::string{};
	notify();
}

} // namespace ingnomia::ui::management6c
