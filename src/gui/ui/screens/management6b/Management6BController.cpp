/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6BController.h"
#include "../InventoryTableSchema.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

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
std::string singularLabel( std::string value )
{
	value = folded( std::move( value ) );
	if ( value.size() > 3 && value.ends_with( "ies" ) )
		value.replace( value.size() - 3, 3, "y" );
	else if ( value.size() > 1 && value.ends_with( 's' ) && !value.ends_with( "ss" ) )
		value.pop_back();
	return value;
}
bool redundantItemLabel( const InventoryRow& group, const InventoryRow& item )
{
	return group.id.depth == InventoryDepth::Group && item.id.depth == InventoryDepth::Item
		&& group.id.category == item.id.category && group.id.group == item.id.group
		&& singularLabel( group.name ) == singularLabel( item.name );
}
bool same( const InventoryRowId& a, const InventoryRowId& b )
{
	return a == b;
}
bool isSection( InventoryDepth depth )
{
	return depth == InventoryDepth::Category || depth == InventoryDepth::Group || depth == InventoryDepth::Item;
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
	commands_( c ), views_{ &v }
{
	notify();
}
void Management6BController::addViewPort( ViewPort& view )
{
	if ( std::ranges::find( views_, &view ) != views_.end() ) return;
	views_.push_back( &view );
	view.stateChanged( state_ );
}
void Management6BController::removeViewPort( ViewPort& view )
{
	std::erase( views_, &view );
}
void Management6BController::notify()
{
	++state_.revision.value;
	const auto views = views_;
	for ( auto* view : views )
		if ( view ) view->stateChanged( state_ );
}
void Management6BController::beginWorld( WorldEpoch w )
{
	inventoryExpansionInitialized_ = false;
	state_                     = {};
	state_.world               = w;
	state_.acceptsWorldActions = static_cast<bool>( w );
	notify();
}
void Management6BController::endWorld()
{
	inventoryExpansionInitialized_ = false;
	state_ = {};
	notify();
}
void Management6BController::open( View v )
{
	const bool wasPopulationOpen = state_.populationOpen;
	state_.open = true;
	state_.view = v;
	if ( v == View::Inventory )
	{
		state_.inventoryOpen = true;
		collapseInventorySections();
		inventoryExpansionInitialized_ = !state_.inventory.empty();
	}
	else
	{
		state_.populationOpen = true;
		state_.populationView = v;
	}
	state_.status.clear();
	notify();
	if ( v == View::Inventory || !wasPopulationOpen || !state_.populationRevision.value ) refresh();
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
	state_.inventoryDetail.reset();
	state_.inventoryDetailBack.clear();
	state_.historyTarget.reset();
	state_.inventoryHistory.clear();
	state_.inventoryHistoryLoading = false;
	state_.pendingAction.reset();
	if ( !state_.open )
		state_.status.clear();
	notify();
}
bool Management6BController::dispatch( std::string_view id, UiActionPayload payload, bool confirmed )
{
	if ( !state_.acceptsWorldActions || !state_.world )
		return false;
	UiActionEnvelope a { ActionId { id }, RequestId { nextRequest_++ }, state_.world, std::nullopt, std::move( payload ) };
	auto r = confirmed ? commands_.dispatchConfirmed( a ) : commands_.dispatch( a );
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
void Management6BController::inventoryChanged()
{
	if ( state_.inventoryOpen && !state_.loadingInventory && !state_.staleInventory )
		requestInventoryRefresh();
}
void Management6BController::setPopulationFilter( std::string v )
{
	state_.populationFilter = std::move( v );
	state_.populationPage   = 0;
	const auto filtered = visiblePopulation();
	if ( state_.selectedCreature && std::ranges::none_of( filtered, [&]( const PopulationRow& row ) { return row.id == *state_.selectedCreature; } ) )
		state_.selectedCreature = filtered.empty() ? std::nullopt : std::optional { filtered.front().id };
	notify();
}
void Management6BController::setInventoryFilter( std::string v )
{
	state_.inventoryFilter = std::move( v );
	state_.inventoryPage   = 0;
	notify();
}
void Management6BController::setInventoryColumnFilter( std::size_t column, std::string value )
{
	if ( column >= state_.inventoryColumnFilters.size() ) return;
	state_.inventoryColumnFilters[column] = std::move( value );
	state_.inventoryPage = 0;
	state_.selectedInventory.reset();
	notify();
}
void Management6BController::toggleInventoryColumnSelection( std::size_t column, std::string value )
{
	if ( column >= state_.inventoryColumnSelections.size() ) return;
	auto& selected = state_.inventoryColumnSelections[column];
	if ( value.empty() )
		selected.clear();
	else if ( const auto found = std::ranges::find_if( selected, [&]( const auto& candidate ) { return folded( candidate ) == folded( value ); } ); found != selected.end() )
		selected.erase( found );
	else
		{
		if ( column >= 4 ) selected.clear();
		selected.push_back( std::move( value ) );
	}
	state_.inventoryPage = 0;
	state_.selectedInventory.reset();
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
void Management6BController::collapseInventorySections()
{
	state_.collapsedInventory.clear();
	for ( const auto& row : state_.inventory )
		if ( isSection( row.id.depth ) ) state_.collapsedInventory.push_back( row.id );
	state_.inventoryPage = 0;
	state_.selectedInventory.reset();
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
	if ( state_.inventorySort == v )
		state_.inventorySortDescending = !state_.inventorySortDescending;
	else
	{
		state_.inventorySort = v;
		state_.inventorySortDescending = v == Sort::Total || v == Sort::Stock;
	}
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
	const auto pathKey = []( const InventoryRowId& id, InventoryDepth depth )
	{
		std::string key = id.category.value;
		if ( depth >= InventoryDepth::Group ) key += '\x1f' + id.group.value;
		if ( depth >= InventoryDepth::Item ) key += '\x1f' + id.item.value;
		if ( depth >= InventoryDepth::Material ) key += '\x1f' + id.material.value;
		return key;
	};
	std::unordered_set<std::string> parents;
	std::array<std::unordered_map<std::string, std::string>, 4> names;
	for ( const auto& row : state_.inventory )
	{
		names[static_cast<std::size_t>( row.id.depth )][pathKey( row.id, row.id.depth )] = row.name;
		if ( row.id.depth == InventoryDepth::Group ) parents.insert( pathKey( row.id, InventoryDepth::Category ) );
		else if ( row.id.depth == InventoryDepth::Item ) parents.insert( pathKey( row.id, InventoryDepth::Group ) );
		else if ( row.id.depth == InventoryDepth::Material ) parents.insert( pathKey( row.id, InventoryDepth::Item ) );
	}
	const auto pathLabels = [&]( const InventoryRow& row )
	{
		std::array<std::string, 4> labels;
		for ( std::size_t depth = 0; depth < labels.size(); ++depth )
		{
			const auto found = names[depth].find( pathKey( row.id, static_cast<InventoryDepth>( depth ) ) );
			if ( found != names[depth].end() ) labels[depth] = found->second;
		}
		normalizeInventoryTableLabels( labels[1], labels[2], labels[3], row.id.item.value );
		return labels;
	};
	struct Candidate
	{
		InventoryRow row;
		std::array<std::string, 4> labels;
		std::array<std::string, 4> foldedLabels;
	};
	std::unordered_set<std::string> seenLeaves;
	std::vector<Candidate> candidates;
	candidates.reserve( state_.inventory.size() );
	for ( const auto& row : state_.inventory )
	{
		if ( parents.contains( pathKey( row.id, row.id.depth ) ) ) continue;
		if ( !seenLeaves.insert( pathKey( row.id, row.id.depth ) ).second ) continue;
		auto labels = pathLabels( row );
		std::array<std::string, 4> foldedLabels;
		for ( std::size_t column = 0; column < labels.size(); ++column ) foldedLabels[column] = folded( labels[column] );
		candidates.push_back( { row, std::move( labels ), std::move( foldedLabels ) } );
	}
	const auto q = folded( state_.inventoryFilter );
	if ( !state_.inventoryCategory.empty() )
		std::erase_if( candidates, [&]( const auto& candidate ) { return candidate.row.id.category.value != state_.inventoryCategory; } );
	if ( !q.empty() )
		std::erase_if( candidates, [&]( const auto& candidate )
						   { return std::ranges::none_of( candidate.foldedLabels, [&]( const auto& label ) { return label.find( q ) != std::string::npos; } ); } );
	if ( state_.inventoryOwnedOnly )
		std::erase_if( candidates, []( const auto& candidate ) { return candidate.row.total == 0; } );
	std::array<std::string, 6> foldedFilters;
	std::array<std::vector<std::string>, 6> foldedSelections;
	for ( std::size_t column = 0; column < foldedFilters.size(); ++column )
	{
		foldedFilters[column] = folded( state_.inventoryColumnFilters[column] );
		foldedSelections[column].reserve( state_.inventoryColumnSelections[column].size() );
		for ( const auto& selection : state_.inventoryColumnSelections[column] ) foldedSelections[column].push_back( folded( selection ) );
	}
	std::erase_if( candidates, [&]( const auto& candidate )
	{
		const auto matches = [&]( std::size_t column, const std::string& foldedValue )
		{
			if ( !foldedFilters[column].empty() && foldedValue.find( foldedFilters[column] ) == std::string::npos ) return false;
			return foldedSelections[column].empty() || std::ranges::find( foldedSelections[column], foldedValue ) != foldedSelections[column].end();
		};
		for ( std::size_t column = 0; column < 4; ++column )
			if ( !matches( column, candidate.foldedLabels[column] ) ) return true;
		if ( !inventoryQuantityMatches( candidate.row.stockpiled, state_.inventoryColumnFilters[4], state_.inventoryColumnSelections[4] ) ) return true;
		return !inventoryQuantityMatches( candidate.row.total, state_.inventoryColumnFilters[5], state_.inventoryColumnSelections[5] );
	} );
	auto before = [&]( const Candidate& a, const Candidate& b )
	{
		if ( state_.inventorySort == Sort::Total && a.row.total != b.row.total ) return state_.inventorySortDescending ? a.row.total > b.row.total : a.row.total < b.row.total;
		if ( state_.inventorySort == Sort::Stock && a.row.stockpiled != b.row.stockpiled ) return state_.inventorySortDescending ? a.row.stockpiled > b.row.stockpiled : a.row.stockpiled < b.row.stockpiled;
		const std::size_t column = state_.inventorySort == Sort::Category ? 0 : state_.inventorySort == Sort::Group ? 1 : state_.inventorySort == Sort::Material ? 3 : 2;
		const auto& leftColumn = a.foldedLabels[column];
		const auto& rightColumn = b.foldedLabels[column];
		if ( leftColumn != rightColumn ) return state_.inventorySortDescending ? leftColumn > rightColumn : leftColumn < rightColumn;
		return state_.inventorySortDescending ? inventoryTableSortKey( a.labels, column ) > inventoryTableSortKey( b.labels, column ) : inventoryTableSortKey( a.labels, column ) < inventoryTableSortKey( b.labels, column );
	};
	std::ranges::stable_sort( candidates, before );
	std::vector<InventoryRow> out;
	out.reserve( candidates.size() );
	for ( auto& candidate : candidates ) out.push_back( std::move( candidate.row ) );
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
	return visibleInventory();
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
	if ( state_.inventoryDetail && std::ranges::none_of( state_.inventory, [&]( const auto& r ) { return r.id == *state_.inventoryDetail; } ) )
	{
		state_.inventoryDetail.reset();
		state_.inventoryDetailBack.clear();
		state_.historyTarget.reset();
		state_.inventoryHistory.clear();
		state_.inventoryHistoryLoading = false;
	}
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
void Management6BController::selectSkill( CatalogId id )
{
	if( std::ranges::none_of( state_.skillCatalog, [&]( const SkillCatalogRow& row ){ return row.id == id; } ) ) return;
	state_.selectedSkill = std::move( id );
	notify();
}
void Management6BController::selectProfession( ProfessionId id )
{
	const auto it = std::ranges::find_if( state_.professions, [&]( const ProfessionRow& row ){ return row.id == id; } );
	if( it == state_.professions.end() ) return;
	state_.selectedProfession = id;
	state_.professionDraftName = it->name;
	state_.professionDraftSkills = it->skills;
	state_.professionDraftDirty = false;
	state_.selectedProfessionSkill.reset();
	state_.selectedAvailableSkill.reset();
	notify();
	dispatch( "profession.request_skills", ProfessionTargetPayload{ id } );
}
void Management6BController::selectProfessionSkill( CatalogId id )
{
	if( std::ranges::find( state_.professionDraftSkills, id ) == state_.professionDraftSkills.end() ) return;
	state_.selectedProfessionSkill = std::move( id ); notify();
}
void Management6BController::selectAvailableSkill( CatalogId id )
{
	if( std::ranges::none_of( state_.skillCatalog, [&]( const SkillCatalogRow& row ){ return row.id == id; } ) ) return;
	state_.selectedAvailableSkill = std::move( id ); notify();
}
void Management6BController::setProfessionDraftName( std::string name )
{
	state_.professionDraftName = std::move( name ); state_.professionDraftDirty = true; notify();
}
void Management6BController::addProfessionSkill()
{
	if( !state_.selectedAvailableSkill || !state_.selectedProfession || state_.selectedProfession->value == "Gnomad" ) return;
	const auto id = *state_.selectedAvailableSkill;
	if( std::ranges::find( state_.professionDraftSkills, id ) == state_.professionDraftSkills.end() ) state_.professionDraftSkills.push_back( id );
	state_.professionDraftDirty = true;
	state_.selectedProfessionSkill = id; notify();
}
void Management6BController::removeProfessionSkill()
{
	if( !state_.selectedProfessionSkill || !state_.selectedProfession || state_.selectedProfession->value == "Gnomad" ) return;
	std::erase( state_.professionDraftSkills, *state_.selectedProfessionSkill );
	state_.professionDraftDirty = true;
	state_.selectedProfessionSkill.reset(); notify();
}
void Management6BController::moveProfessionSkill( std::int32_t delta )
{
	if( !state_.selectedProfessionSkill || !state_.selectedProfession || state_.selectedProfession->value == "Gnomad" ) return;
	const auto it = std::ranges::find( state_.professionDraftSkills, *state_.selectedProfessionSkill );
	if( it == state_.professionDraftSkills.end() ) return;
	const auto index = std::distance( state_.professionDraftSkills.begin(), it );
	const auto moved = index + delta;
	if( moved < 0 || moved >= static_cast<std::ptrdiff_t>( state_.professionDraftSkills.size() ) ) return;
	std::iter_swap( it, state_.professionDraftSkills.begin() + moved ); state_.professionDraftDirty = true; notify();
}
void Management6BController::createProfession( std::string name )
{
	const auto first = name.find_first_not_of( " \t\r\n" );
	const auto last = name.find_last_not_of( " \t\r\n" );
	if( first == std::string::npos ) { state_.status = "management.population.error_name_required"; notify(); return; }
	name = name.substr( first, last - first + 1 );
	if( std::ranges::any_of( state_.professions, [&]( const ProfessionRow& row ){ return row.name == name; } ) ) { state_.status = "management.population.error_duplicate_name"; notify(); return; }
	if( dispatch( "profession.create", CreateProfessionPayload{ name } ) ) state_.selectedProfession = ProfessionId{ name };
}
void Management6BController::saveProfession()
{
	if( !state_.selectedProfession || state_.selectedProfession->value == "Gnomad" ) return;
	const auto name = state_.professionDraftName;
	if( name.find_first_not_of( " \t\r\n" ) == std::string::npos ) { state_.status = "management.population.error_name_required"; notify(); return; }
	if( std::ranges::any_of( state_.professions, [&]( const ProfessionRow& row ){ return row.name == name && row.id != *state_.selectedProfession; } ) ) { state_.status = "management.population.error_duplicate_name"; notify(); return; }
	const auto old = *state_.selectedProfession;
	if( dispatch( "profession.update", UpdateProfessionPayload{ old, name, state_.professionDraftSkills } ) ) { state_.selectedProfession = ProfessionId{ name }; state_.professionDraftDirty = false; }
}
void Management6BController::deleteProfession()
{
	if( !state_.selectedProfession || state_.selectedProfession->value == "Gnomad" ) return;
	if ( dispatch( "profession.delete", ProfessionTargetPayload{ *state_.selectedProfession }, true ) ) state_.selectedProfession.reset();
}
void Management6BController::setScheduleActivity( ManagedScheduleActivity activity )
{
	state_.scheduleActivity = activity; notify();
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
void Management6BController::openInventoryDetail( InventoryRowId id )
{
	if ( id.depth != InventoryDepth::Item && id.depth != InventoryDepth::Material ) return;
	if ( std::ranges::none_of( state_.inventory, [&]( const auto& row ) { return row.id == id; } ) ) return;
	if ( state_.inventoryDetail && *state_.inventoryDetail != id ) state_.inventoryDetailBack.push_back( *state_.inventoryDetail );
	state_.inventoryDetail = std::move( id );
	requestSelectedInventoryHistory();
	notify();
}
void Management6BController::openRelatedInventoryItem( std::string itemID )
{
	const auto row = std::ranges::find_if( state_.inventory, [&]( const auto& value )
		{ return value.id.depth == InventoryDepth::Item && value.id.item.value == itemID; } );
	if ( row != state_.inventory.end() ) openInventoryDetail( row->id );
}
void Management6BController::backInventoryDetail()
{
	if ( state_.inventoryDetailBack.empty() ) { closeInventoryDetail(); return; }
	state_.inventoryDetail = state_.inventoryDetailBack.back();
	state_.inventoryDetailBack.pop_back();
	requestSelectedInventoryHistory();
	notify();
}
void Management6BController::closeInventoryDetail()
{
	state_.inventoryDetail.reset();
	state_.inventoryDetailBack.clear();
	state_.historyTarget.reset();
	state_.inventoryHistory.clear();
	state_.inventoryHistoryLoading = false;
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
	if( state_.selectedProfession )
	{
		const auto selected = std::ranges::find_if( state_.professions, [&]( const ProfessionRow& row ){ return row.id == *state_.selectedProfession; } );
		if( selected != state_.professions.end() )
		{
			if( !state_.professionDraftDirty ) { state_.professionDraftName = selected->name; state_.professionDraftSkills = selected->skills; }
			dispatch( "profession.request_skills", ProfessionTargetPayload{ selected->id } );
		}
		else { state_.selectedProfession.reset(); state_.professionDraftDirty = false; }
	}
	state_.professionRevision = s.revision;
	state_.loadingPopulation  = false;
	state_.pendingAction.reset();
	notify();
	return true;
}
bool Management6BController::applySkillCatalog( WorldEpoch world, std::vector<SkillCatalogRow> rows )
{
	if( world != state_.world ) return false;
	state_.skillCatalog = std::move( rows );
	if( state_.selectedSkill && std::ranges::none_of( state_.skillCatalog, [&]( const SkillCatalogRow& row ){ return row.id == *state_.selectedSkill; } ) ) state_.selectedSkill.reset();
	if( !state_.selectedSkill && !state_.skillCatalog.empty() ) state_.selectedSkill = state_.skillCatalog.front().id;
	notify(); return true;
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
	if( state_.selectedProfession == id && !state_.professionDraftDirty ) state_.professionDraftSkills = i->skills;
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
	if ( !inventoryExpansionInitialized_ )
	{
		collapseInventorySections();
		inventoryExpansionInitialized_ = true;
	}
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
	const auto target = state_.inventoryDetail ? state_.inventoryDetail : state_.selectedInventory;
	if ( !target || target->depth == InventoryDepth::Category || target->depth == InventoryDepth::Group )
		return;
	const auto it = std::ranges::find_if( state_.inventory, [&]( const auto& r )
		{ return r.id == *target; } );
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
