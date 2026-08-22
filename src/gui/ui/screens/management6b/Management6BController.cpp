/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6BController.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <tuple>

namespace ingnomia::ui::management6b
{
namespace
{
std::string folded( std::string value )
{
	std::ranges::transform( value, value.begin(), []( unsigned char c )
							{ return static_cast<char>( std::tolower( c ) ); } );
	return value;
}
bool same( const InventoryRowId& a, const InventoryRowId& b )
{
	return a == b;
}
bool isSection( InventoryDepth depth )
{
	return depth == InventoryDepth::Category || depth == InventoryDepth::Group;
}
bool isChildOf( const InventoryRowId& child, const InventoryRowId& parent )
{
	if ( child.depth == InventoryDepth::Group )
		return parent.depth == InventoryDepth::Category && child.category == parent.category;
	if ( child.depth == InventoryDepth::Item )
		return parent.depth == InventoryDepth::Group && child.category == parent.category && child.group == parent.group;
	if ( child.depth == InventoryDepth::Material )
		return parent.depth == InventoryDepth::Item && child.category == parent.category && child.group == parent.group && child.item == parent.item;
	return false;
}
} // namespace
Management6BController::Management6BController( CommandPort& c, ViewPort& v ) :
	commands_( c ), view_( v )
{
	notify();
}
void Management6BController::notify()
{
	++state_.revision.value;
	view_.stateChanged( state_ );
}
void Management6BController::beginWorld( WorldEpoch w )
{
	state_                     = {};
	state_.world               = w;
	state_.acceptsWorldActions = static_cast<bool>( w );
	notify();
}
void Management6BController::endWorld()
{
	state_ = {};
	notify();
}
void Management6BController::open( View v )
{
	state_.open = true;
	state_.view = v;
	if ( v == View::Inventory )
		state_.inventoryOpen = true;
	else
	{
		state_.populationOpen = true;
		state_.populationView = v;
	}
	state_.status.clear();
	notify();
	refresh();
}
void Management6BController::close()
{
	state_.open           = false;
	state_.populationOpen = false;
	state_.inventoryOpen  = false;
	state_.pendingAction.reset();
	state_.status.clear();
	notify();
}
void Management6BController::closePopulation()
{
	state_.populationOpen = false;
	state_.open           = state_.inventoryOpen;
	state_.pendingAction.reset();
	if ( !state_.open )
		state_.status.clear();
	notify();
}
void Management6BController::closeInventory()
{
	state_.inventoryOpen = false;
	state_.open          = state_.populationOpen;
	state_.pendingAction.reset();
	if ( !state_.open )
		state_.status.clear();
	notify();
}
bool Management6BController::dispatch( std::string_view id, UiActionPayload payload )
{
	if ( !state_.acceptsWorldActions || !state_.world )
		return false;
	UiActionEnvelope a { ActionId { id }, RequestId { nextRequest_++ }, state_.world, std::nullopt, std::move( payload ) };
	auto r = commands_.dispatch( a );
	if ( r.status == CommandStatus::Rejected )
	{
		state_.status = r.error;
		notify();
		return false;
	}
	state_.status.clear();
	if ( r.pending )
		state_.pendingAction = a.request;
	notify();
	return true;
}
void Management6BController::refresh()
{
	if ( state_.view == View::Inventory )
	{
		state_.loadingInventory = true;
		notify();
		dispatch( "inventory.refresh", NoPayload {} );
	}
	else if ( state_.view == View::Creature && state_.selectedCreature )
	{
		dispatch( "inspect.select", SelectPayload { { state_.world, EntityKind::Creature, state_.selectedCreature->value, {} } } );
	}
	else
	{
		state_.loadingPopulation = true;
		notify();
		dispatch( state_.view == View::Professions ? "profession.refresh" : "population.refresh", NoPayload {} );
	}
}
void Management6BController::setPopulationFilter( std::string v )
{
	state_.populationFilter = std::move( v );
	state_.populationPage   = 0;
	notify();
}
void Management6BController::setInventoryFilter( std::string v )
{
	state_.inventoryFilter = std::move( v );
	state_.inventoryPage   = 0;
	notify();
}
void Management6BController::setInventoryOwnedOnly( bool v )
{
	if ( state_.inventoryOwnedOnly == v )
		return;
	state_.inventoryOwnedOnly = v;
	state_.inventoryPage      = 0;
	state_.selectedInventory.reset();
	notify();
}
void Management6BController::setInventoryCategory( std::string v )
{
	state_.inventoryCategory = std::move( v );
	state_.inventoryPage     = 0;
	state_.selectedInventory.reset();
	notify();
}
void Management6BController::toggleInventoryExpanded( InventoryRowId id )
{
	if ( !isSection( id.depth ) )
		return;
	const auto it = std::ranges::find( state_.collapsedInventory, id );
	if ( it == state_.collapsedInventory.end() )
		state_.collapsedInventory.push_back( std::move( id ) );
	else
		state_.collapsedInventory.erase( it );
	state_.inventoryPage = 0;
	state_.selectedInventory.reset();
	notify();
}
bool Management6BController::inventoryExpanded( const InventoryRowId& id ) const
{
	const bool collapsed = std::ranges::any_of( state_.collapsedInventory, [&]( const auto& value ) { return value == id; } );
	return !isSection( id.depth ) || !collapsed;
}
void Management6BController::setPopulationSort( Sort v )
{
	state_.populationSort = v;
	state_.populationPage = 0;
	state_.selectedCreature.reset();
	notify();
}
void Management6BController::setInventorySort( Sort v )
{
	state_.inventorySort = v;
	state_.inventoryPage = 0;
	state_.selectedInventory.reset();
	notify();
}
std::vector<PopulationRow> Management6BController::visiblePopulation() const
{
	auto out     = state_.population;
	const auto q = folded( state_.populationFilter );
	if ( !q.empty() )
		std::erase_if( out, [&]( const auto& r )
					   { return folded( r.name ).find( q ) == std::string::npos && folded( r.profession.value ).find( q ) == std::string::npos; } );
	std::ranges::stable_sort( out, [&]( const auto& a, const auto& b )
							  { return state_.populationSort == Sort::Profession ? std::tie( a.profession.value, a.name ) < std::tie( b.profession.value, b.name ) : std::tie( a.name, a.id.value ) < std::tie( b.name, b.id.value ); } );
	return out;
}
std::vector<InventoryRow> Management6BController::visibleInventory() const
{
	auto candidates = state_.inventory;
	const auto q = folded( state_.inventoryFilter );
	if ( !state_.inventoryCategory.empty() )
		std::erase_if( candidates, [&]( const auto& r ) { return r.id.category.value != state_.inventoryCategory || r.id.depth == InventoryDepth::Category; } );
	if ( !q.empty() )
		std::erase_if( candidates, [&]( const auto& r )
					   { return folded( r.name ).find( q ) == std::string::npos || r.id.depth == InventoryDepth::Category; } );
	if ( state_.inventoryOwnedOnly )
		std::erase_if( candidates, [&]( const auto& r )
					   {
						   if ( r.total > 0 )
							   return false;
						   if ( r.id.depth == InventoryDepth::Category )
							   return std::ranges::none_of( state_.inventory, [&]( const auto& child ) { return child.total > 0 && child.id.category == r.id.category && child.id.depth != InventoryDepth::Category; } );
						   if ( r.id.depth == InventoryDepth::Group )
							   return std::ranges::none_of( state_.inventory, [&]( const auto& child ) { return child.total > 0 && child.id.category == r.id.category && child.id.group == r.id.group && child.id.depth != InventoryDepth::Group; } );
						   return true;
					   } );
	auto before = [&]( const InventoryRow& a, const InventoryRow& b )
	{
		if ( state_.inventorySort == Sort::Total )
			return std::tie( a.total, a.name, a.id.category.value, a.id.group.value, a.id.item.value, a.id.material.value ) > std::tie( b.total, b.name, b.id.category.value, b.id.group.value, b.id.item.value, b.id.material.value );
		if ( state_.inventorySort == Sort::Value )
			return std::tie( a.totalValue, a.name, a.id.category.value, a.id.group.value, a.id.item.value, a.id.material.value ) > std::tie( b.totalValue, b.name, b.id.category.value, b.id.group.value, b.id.item.value, b.id.material.value );
		return std::tie( a.name, a.id.category.value, a.id.group.value, a.id.item.value, a.id.material.value ) < std::tie( b.name, b.id.category.value, b.id.group.value, b.id.item.value, b.id.material.value );
	};
	std::vector<InventoryRow> out;
	std::vector<bool> emitted( candidates.size(), false );
	std::function<void( std::size_t )> append = [&]( std::size_t index )
	{
		if ( emitted[index] )
			return;
		emitted[index] = true;
		const auto& row = candidates[index];
		out.push_back( row );
		if ( !inventoryExpanded( row.id ) )
			return;
		std::vector<std::size_t> children;
		for ( std::size_t i = 0; i < candidates.size(); ++i )
			if ( !emitted[i] && isChildOf( candidates[i].id, row.id ) )
				children.push_back( i );
		std::ranges::stable_sort( children, [&]( std::size_t a, std::size_t b ) { return before( candidates[a], candidates[b] ); } );
		for ( const auto child : children )
			append( child );
	};
	std::vector<std::size_t> roots;
	for ( std::size_t i = 0; i < candidates.size(); ++i )
	{
		const auto hasParent = std::ranges::any_of( candidates, [&]( const auto& parent ) { return isChildOf( candidates[i].id, parent.id ); } );
		if ( !hasParent )
			roots.push_back( i );
	}
	std::ranges::stable_sort( roots, [&]( std::size_t a, std::size_t b ) { return before( candidates[a], candidates[b] ); } );
	for ( const auto root : roots )
		append( root );
	for ( std::size_t i = 0; i < candidates.size(); ++i )
		if ( !emitted[i] && !std::ranges::any_of( candidates, [&]( const auto& parent ) { return isChildOf( candidates[i].id, parent.id ); } ) )
			append( i );
	return out;
}
template <class T>
static std::vector<T> page( std::vector<T> rows, std::size_t index )
{
	const auto first = std::min( index * Management6BState::pageSize, rows.size() );
	const auto last  = std::min( first + Management6BState::pageSize, rows.size() );
	return { rows.begin() + static_cast<std::ptrdiff_t>( first ), rows.begin() + static_cast<std::ptrdiff_t>( last ) };
}
std::vector<PopulationRow> Management6BController::populationPage() const
{
	return page( visiblePopulation(), state_.populationPage );
}
std::vector<InventoryRow> Management6BController::inventoryPage() const
{
	return page( visibleInventory(), state_.inventoryPage );
}
void Management6BController::changePopulationPage( std::int32_t d )
{
	const auto count      = visiblePopulation().size();
	const auto max        = count ? ( ( count - 1 ) / Management6BState::pageSize ) : 0;
	state_.populationPage = static_cast<std::size_t>( std::clamp<std::int64_t>( static_cast<std::int64_t>( state_.populationPage ) + d, 0, static_cast<std::int64_t>( max ) ) );
	notify();
}
void Management6BController::changeInventoryPage( std::int32_t d )
{
	const auto count     = visibleInventory().size();
	const auto max       = count ? ( ( count - 1 ) / Management6BState::pageSize ) : 0;
	state_.inventoryPage = static_cast<std::size_t>( std::clamp<std::int64_t>( static_cast<std::int64_t>( state_.inventoryPage ) + d, 0, static_cast<std::int64_t>( max ) ) );
	notify();
}
void Management6BController::reconcileSelection()
{
	if ( state_.selectedCreature && std::ranges::none_of( state_.population, [&]( const auto& r )
														  { return r.id == *state_.selectedCreature; } ) )
		state_.selectedCreature = state_.population.empty() ? std::nullopt : std::optional { state_.population.front().id };
	if ( state_.selectedInventory && std::ranges::none_of( state_.inventory, [&]( const auto& r )
														   { return same( r.id, *state_.selectedInventory ); } ) )
		state_.selectedInventory = state_.inventory.empty() ? std::nullopt : std::optional { state_.inventory.front().id };
}
void Management6BController::selectCreature( CreatureId id )
{
	if ( !id )
		return;
	const auto rows = visiblePopulation();
	const auto i    = std::ranges::find_if( rows, [&]( const auto& r )
											{ return r.id == id; } );
	if ( i == rows.end() )
		return;
	state_.selectedCreature = id;
	state_.populationPage   = static_cast<std::size_t>( i - rows.begin() ) / Management6BState::pageSize;
	state_.view             = View::Creature;
	state_.populationView   = View::Creature;
	notify();
	dispatch( "inspect.select", SelectPayload { { state_.world, EntityKind::Creature, id.value, {} } } );
}
void Management6BController::selectInventory( InventoryRowId id )
{
	const auto rows = visibleInventory();
	const auto i    = std::ranges::find_if( rows, [&]( const auto& r )
											{ return r.id == id; } );
	if ( i == rows.end() )
		return;
	state_.selectedInventory = std::move( id );
	state_.inventoryPage     = static_cast<std::size_t>( i - rows.begin() ) / Management6BState::pageSize;
	notify();
}
void Management6BController::movePopulationSelection( std::int32_t delta )
{
	const auto rows = visiblePopulation();
	if ( rows.empty() )
		return;
	const auto i            = state_.selectedCreature ? std::ranges::find_if( rows, [&]( const auto& r )
																			  { return r.id == *state_.selectedCreature; } )
													  : rows.end();
	const auto current      = i == rows.end() ? ( delta < 0 ? static_cast<std::int64_t>( rows.size() ) : -1 ) : static_cast<std::int64_t>( i - rows.begin() );
	const auto next         = std::clamp<std::int64_t>( current + delta, 0, static_cast<std::int64_t>( rows.size() - 1 ) );
	state_.selectedCreature = rows[static_cast<std::size_t>( next )].id;
	state_.populationPage   = static_cast<std::size_t>( next ) / Management6BState::pageSize;
	notify();
}
void Management6BController::moveInventorySelection( std::int32_t delta )
{
	const auto rows = visibleInventory();
	if ( rows.empty() )
		return;
	const auto i             = state_.selectedInventory ? std::ranges::find_if( rows, [&]( const auto& r )
																				{ return r.id == *state_.selectedInventory; } )
														: rows.end();
	const auto current       = i == rows.end() ? ( delta < 0 ? static_cast<std::int64_t>( rows.size() ) : -1 ) : static_cast<std::int64_t>( i - rows.begin() );
	const auto next          = std::clamp<std::int64_t>( current + delta, 0, static_cast<std::int64_t>( rows.size() - 1 ) );
	state_.selectedInventory = rows[static_cast<std::size_t>( next )].id;
	state_.inventoryPage     = static_cast<std::size_t>( next ) / Management6BState::pageSize;
	notify();
}
void Management6BController::selectScheduleCell( ScheduleCellId id )
{
	if ( id.hour >= 24 || std::ranges::none_of( state_.schedules, [&]( const auto& r )
												{ return r.creature == id.creature; } ) )
		return;
	state_.selectedScheduleCell = id;
	notify();
}
void Management6BController::moveScheduleFocus( std::int32_t hourDelta, std::int32_t rowDelta )
{
	if ( state_.schedules.empty() )
		return;
	std::size_t row   = 0;
	std::int32_t hour = 0;
	if ( state_.selectedScheduleCell )
	{
		hour         = state_.selectedScheduleCell->hour;
		const auto i = std::ranges::find_if( state_.schedules, [&]( const auto& r )
											 { return r.creature == state_.selectedScheduleCell->creature; } );
		if ( i != state_.schedules.end() )
			row = static_cast<std::size_t>( i - state_.schedules.begin() );
	}
	hour                        = std::clamp( hour + hourDelta, 0, 23 );
	const auto moved            = std::clamp<std::int64_t>( static_cast<std::int64_t>( row ) + rowDelta, 0, static_cast<std::int64_t>( state_.schedules.size() - 1 ) );
	state_.selectedScheduleCell = ScheduleCellId { state_.schedules[static_cast<std::size_t>( moved )].creature, static_cast<std::uint8_t>( hour ) };
	notify();
}
bool Management6BController::accepts( WorldEpoch w, Revision in, Revision current ) const
{
	return state_.acceptsWorldActions && w == state_.world && in.value > current.value;
}
bool Management6BController::applyPopulation( Snapshot<std::vector<PopulationRow>> s )
{
	if ( !accepts( s.world, s.revision, state_.populationRevision ) )
		return false;
	state_.population         = std::move( s.value );
	state_.populationRevision = s.revision;
	state_.loadingPopulation  = false;
	state_.stalePopulation    = false;
	state_.pendingAction.reset();
	reconcileSelection();
	notify();
	return true;
}
bool Management6BController::applyPopulationPatch( RowPatch<PopulationRow> s )
{
	if ( s.world != state_.world || s.baseRevision != state_.populationRevision || s.revision.value <= s.baseRevision.value )
	{
		requestPopulationRefresh();
		return false;
	}
	auto i = std::ranges::find_if( state_.population, [&]( const auto& r )
								   { return r.id == s.row.id; } );
	if ( i == state_.population.end() )
		state_.population.push_back( std::move( s.row ) );
	else
		*i = std::move( s.row );
	state_.populationRevision = s.revision;
	state_.pendingAction.reset();
	reconcileSelection();
	notify();
	return true;
}
bool Management6BController::applyProfessions( Snapshot<std::vector<ProfessionRow>> s )
{
	if ( !accepts( s.world, s.revision, state_.professionRevision ) )
		return false;
	state_.professions        = std::move( s.value );
	state_.professionRevision = s.revision;
	state_.loadingPopulation  = false;
	state_.pendingAction.reset();
	notify();
	return true;
}
bool Management6BController::applyProfessionSkills( WorldEpoch w, ProfessionId id, std::vector<CatalogId> skills )
{
	if ( w != state_.world )
		return false;
	const auto i = std::ranges::find_if( state_.professions, [&]( const auto& r )
										 { return r.id == id; } );
	if ( i == state_.professions.end() )
		return false;
	i->skills = std::move( skills );
	notify();
	return true;
}
bool Management6BController::applySchedules( Snapshot<std::vector<ScheduleRow>> s )
{
	if ( !accepts( s.world, s.revision, state_.scheduleRevision ) )
		return false;
	state_.schedules         = std::move( s.value );
	state_.scheduleRevision  = s.revision;
	state_.loadingPopulation = false;
	state_.pendingAction.reset();
	notify();
	return true;
}
bool Management6BController::applySchedulePatch( RowPatch<ScheduleRow> s )
{
	if ( s.world != state_.world || s.baseRevision != state_.scheduleRevision || s.revision.value <= s.baseRevision.value )
	{
		requestPopulationRefresh();
		return false;
	}
	auto i = std::ranges::find_if( state_.schedules, [&]( const auto& r )
								   { return r.creature == s.row.creature; } );
	if ( i == state_.schedules.end() )
		state_.schedules.push_back( std::move( s.row ) );
	else
		*i = std::move( s.row );
	state_.scheduleRevision = s.revision;
	state_.pendingAction.reset();
	notify();
	return true;
}
bool Management6BController::applyInventory( Snapshot<std::vector<InventoryRow>> s )
{
	if ( !accepts( s.world, s.revision, state_.inventoryRevision ) )
		return false;
	state_.inventory         = std::move( s.value );
	state_.inventoryRevision = s.revision;
	state_.loadingInventory  = false;
	state_.staleInventory    = false;
	state_.pendingAction.reset();
	reconcileSelection();
	notify();
	return true;
}
bool Management6BController::applyInventoryPatch( RowPatch<InventoryRow> s )
{
	if ( s.world != state_.world || s.baseRevision != state_.inventoryRevision || s.revision.value <= s.baseRevision.value )
	{
		requestInventoryRefresh();
		return false;
	}
	auto i = std::ranges::find_if( state_.inventory, [&]( const auto& r )
								   { return r.id == s.row.id; } );
	if ( i == state_.inventory.end() )
		state_.inventory.push_back( std::move( s.row ) );
	else
		*i = std::move( s.row );
	state_.inventoryRevision = s.revision;
	state_.pendingAction.reset();
	reconcileSelection();
	notify();
	return true;
}
bool Management6BController::applyCreature( Snapshot<CreatureDetail> s )
{
	if ( !accepts( s.world, s.revision, state_.creatureRevision ) || !s.value.id )
		return false;
	if ( state_.selectedCreature && *state_.selectedCreature != s.value.id )
		return false;
	state_.selectedCreature = s.value.id;
	const bool changed        = !state_.creature || *state_.creature != s.value;
	state_.creature         = std::move( s.value );
	state_.creatureRevision = s.revision;
	state_.pendingAction.reset();
	if ( changed )
		notify();
	return true;
}
void Management6BController::requestPopulationRefresh()
{
	if ( state_.stalePopulation )
		return;
	state_.stalePopulation = true;
	dispatch( "population.refresh", NoPayload {} );
}
void Management6BController::requestInventoryRefresh()
{
	if ( state_.staleInventory )
		return;
	state_.staleInventory = true;
	dispatch( "inventory.refresh", NoPayload {} );
}
void Management6BController::setSkill( CreatureId c, CatalogId s, bool a )
{
	dispatch( "population.set_skill", SetSkillPayload { c, std::move( s ), a } );
}
void Management6BController::setAllSkills( CreatureId c, bool a )
{
	dispatch( "population.set_all_skills_for_gnome", SetGnomeSkillsPayload { c, a } );
}
void Management6BController::setSkillForAll( CatalogId s, bool a )
{
	dispatch( "population.set_skill_for_all", SetSkillForAllPayload { std::move( s ), a } );
}
void Management6BController::setProfession( CreatureId c, ProfessionId p )
{
	dispatch( "population.set_profession", SetProfessionPayload { c, std::move( p ) } );
}
void Management6BController::updateProfession( ProfessionId p, std::string n, std::vector<CatalogId> s )
{
	dispatch( "profession.update", UpdateProfessionPayload { std::move( p ), std::move( n ), std::move( s ) } );
}
void Management6BController::setScheduleCell( CreatureId c, std::uint8_t h, ScheduleActivity a )
{
	if ( h < 24 )
		dispatch( "population.set_schedule_cell", SetScheduleCellPayload { { c, h }, a } );
}
void Management6BController::setScheduleRow( CreatureId c, ScheduleActivity a )
{
	dispatch( "population.set_schedule_row", SetScheduleRowPayload { c, a } );
}
void Management6BController::setScheduleColumn( std::uint8_t h, ScheduleActivity a )
{
	if ( h < 24 )
		dispatch( "population.set_schedule_column", SetScheduleColumnPayload { h, a } );
}
void Management6BController::activateScheduleCell( ScheduleActivity a )
{
	if ( state_.selectedScheduleCell )
		setScheduleCell( state_.selectedScheduleCell->creature, state_.selectedScheduleCell->hour, a );
}
void Management6BController::setWatched( InventoryRowId r, bool w )
{
	const auto i = std::ranges::find_if( state_.inventory, [&]( const auto& row ) { return row.id == r; } );
	if ( i == state_.inventory.end() )
		return;
	const bool previous = i->watched;
	i->watched          = w;
	if ( !dispatch( "watch.set", WatchPayload { std::move( r ), w } ) )
	{
		i->watched = previous;
		notify();
	}
}
void Management6BController::requestSelectedInventoryHistory()
{
	if ( !state_.selectedInventory || isSection( state_.selectedInventory->depth ) )
		return;
	const auto it = std::ranges::find_if( state_.inventory, [&]( const auto& r )
		{ return r.id == *state_.selectedInventory; } );
	if ( it == state_.inventory.end() )
		return;
	state_.historyTarget = it->id;
	state_.inventoryHistory.clear();
	state_.inventoryHistoryLoading = true;
	state_.status.clear();
	notify();
	if ( !dispatch( "inventory.request_history", InventoryHistoryPayload { CatalogId { it->id.item.value }, CatalogId { it->id.material.value }, HistoryRange::Month } ) )
	{
		// Do not leave the panel spinning forever when a world transition or bridge
		// validation rejects the request. The authoritative error remains visible
		// and the player can retry after the new snapshot arrives.
		state_.historyTarget.reset();
		state_.inventoryHistoryLoading = false;
		notify();
	}
}
bool Management6BController::applyInventoryHistory( WorldEpoch world, InventoryRowId target, std::vector<InventoryHistoryPoint> points )
{
	if ( world != state_.world || !state_.historyTarget || *state_.historyTarget != target )
		return false;
	state_.inventoryHistory = std::move( points );
	state_.inventoryHistoryLoading = false;
	notify();
	return true;
}
void Management6BController::toggleSelectedWatch()
{
	if ( !state_.selectedInventory )
		return;
	if ( std::ranges::none_of( visibleInventory(), [&]( const auto& r ) { return r.id == *state_.selectedInventory; } ) )
		return;
	const auto i = std::ranges::find_if( state_.inventory, [&]( const auto& r )
										 { return r.id == *state_.selectedInventory; } );
	if ( i != state_.inventory.end() )
		setWatched( i->id, !i->watched );
}
void Management6BController::onActionFinished( RequestId id, CommandResult r )
{
	if ( state_.pendingAction != id )
		return;
	state_.pendingAction.reset();
	state_.status = r.status == CommandStatus::Rejected ? r.error : std::string {};
	notify();
}
} // namespace ingnomia::ui::management6b
