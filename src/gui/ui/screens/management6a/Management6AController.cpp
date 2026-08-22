/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6AController.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace ingnomia::ui::management6a
{
namespace
{
std::string folded( std::string value )
{
	std::transform( value.begin(), value.end(), value.begin(), []( unsigned char c )
					{ return static_cast<char>( std::tolower( c ) ); } );
	return value;
}
bool contains( const std::string& value, const std::string& query )
{
	return query.empty() || folded( value ).find( folded( query ) ) != std::string::npos;
}
template <class Id, class Row>
auto findRow( std::vector<Row>& rows, const Id& id )
{
	return std::find_if( rows.begin(), rows.end(), [&]( const Row& row )
						 { return row.id == id; } );
}
template <class Id, class Row>
bool hasRow( const std::vector<Row>& rows, const Id& id )
{
	return std::any_of( rows.begin(), rows.end(), [&]( const Row& row )
						{ return row.id == id; } );
}
template <class Row, class Label>
void sortRows( std::vector<Row>& rows, SortDirection direction, Label label )
{
	std::stable_sort( rows.begin(), rows.end(), [&]( const Row& a, const Row& b )
					  { return direction == SortDirection::Ascending ? label( a ) < label( b ) : label( b ) < label( a ); } );
}
} // namespace

Management6AController::Management6AController( CommandPort& commands, ViewPort& view ) :
	commands_( commands ), view_( view )
{
	notify();
}
void Management6AController::notify()
{
	++state_.revision.value;
	view_.stateChanged( state_ );
}
void Management6AController::beginWorld( WorldEpoch world )
{
	state_                     = {};
	state_.world               = world;
	state_.acceptsWorldActions = static_cast<bool>( world );
	notify();
}
void Management6AController::endWorld()
{
	state_ = {};
	notify();
}

void Management6AController::showLoading( ManagementView view )
{
	if ( !state_.acceptsWorldActions )
		return;
	state_.view     = view;
	auto* request   = view == ManagementView::Workshop ? &state_.workshop.request : view == ManagementView::Stockpile ? &state_.stockpile.request
																													  : &state_.agriculture.request;
	request->status = RequestStatus::Loading;
	request->message.clear();
	request->refreshInProgress = false;
	notify();
}
void Management6AController::showError( ManagementView view, std::string message )
{
	if ( !state_.acceptsWorldActions )
		return;
	state_.view                = view;
	auto* request              = view == ManagementView::Workshop ? &state_.workshop.request : view == ManagementView::Stockpile ? &state_.stockpile.request
																																 : &state_.agriculture.request;
	request->status            = RequestStatus::Error;
	request->message           = std::move( message );
	request->refreshInProgress = false;
	notify();
}

void Management6AController::showWorkshop( WorkshopSnapshot value, Revision revision, std::optional<WorldPosition> position )
{
	if ( !state_.acceptsWorldActions || !value.id || ( state_.workshop.value.id == value.id && revision.value <= state_.workshop.revision.value ) )
		return;
	if ( state_.workshop.value.id != value.id )
		state_.workshop.tradeConfirmationRequired = false;
	const auto product              = state_.workshop.value.id == value.id ? state_.workshop.selectedProduct : std::optional<CatalogId> {};
	const auto job                  = state_.workshop.value.id == value.id ? state_.workshop.selectedJob : std::optional<CraftJobId> {};
	state_.view                     = ManagementView::Workshop;
	state_.workshop.value           = std::move( value );
	state_.workshop.revision        = revision;
	state_.workshop.position        = position;
	state_.workshop.request         = { state_.workshop.value.products.empty() && state_.workshop.value.queue.empty() ? RequestStatus::Empty : RequestStatus::Ready, {}, {}, false };
	state_.workshop.selectedProduct = product && hasRow( state_.workshop.value.products, *product ) ? product : ( state_.workshop.value.products.empty() ? std::optional<CatalogId> {} : std::optional<CatalogId> { state_.workshop.value.products.front().id } );
	state_.workshop.selectedJob     = job && hasRow( state_.workshop.value.queue, *job ) ? job : ( state_.workshop.value.queue.empty() ? std::optional<CraftJobId> {} : std::optional<CraftJobId> { state_.workshop.value.queue.front().id } );
	state_.pendingAction.reset();
	state_.status.clear();
	rebuildWorkshop();
	notify();
}
void Management6AController::showStockpile( StockpileSnapshot value, Revision revision, std::optional<WorldPosition> position )
{
	if ( !state_.acceptsWorldActions || !value.id || ( state_.stockpile.value.id == value.id && revision.value <= state_.stockpile.revision.value ) )
		return;
	const auto filter                = state_.stockpile.value.id == value.id ? state_.stockpile.selectedFilter : std::optional<StockpileFilterRowId> {};
	const auto content               = state_.stockpile.value.id == value.id ? state_.stockpile.selectedContent : std::optional<StockpileContentRowId> {};
	state_.view                      = ManagementView::Stockpile;
	state_.stockpile.value           = std::move( value );
	state_.stockpile.revision        = revision;
	state_.stockpile.position        = position;
	state_.stockpile.request         = { state_.stockpile.value.filters.empty() && state_.stockpile.value.contents.empty() ? RequestStatus::Empty : RequestStatus::Ready, {}, {}, false };
	state_.stockpile.selectedFilter  = filter && hasRow( state_.stockpile.value.filters, *filter ) ? filter : ( state_.stockpile.value.filters.empty() ? std::optional<StockpileFilterRowId> {} : std::optional<StockpileFilterRowId> { state_.stockpile.value.filters.front().id } );
	state_.stockpile.selectedContent = content && hasRow( state_.stockpile.value.contents, *content ) ? content : ( state_.stockpile.value.contents.empty() ? std::optional<StockpileContentRowId> {} : std::optional<StockpileContentRowId> { state_.stockpile.value.contents.front().id } );
	state_.pendingAction.reset();
	state_.status.clear();
	rebuildStockpile();
	notify();
}
void Management6AController::showAgriculture( AgricultureSnapshot value, Revision revision, std::optional<WorldPosition> position )
{
	if ( !state_.acceptsWorldActions || !value.target.designation || ( state_.agriculture.value.target == value.target && revision.value <= state_.agriculture.revision.value ) )
		return;
	const bool same                    = state_.agriculture.value.target == value.target;
	const auto product                 = same ? state_.agriculture.selectedProduct : std::optional<CatalogId> {};
	const auto animal                  = same ? state_.agriculture.selectedAnimal : std::optional<CreatureId> {};
	state_.view                        = ManagementView::Agriculture;
	state_.agriculture.value           = std::move( value );
	state_.agriculture.revision        = revision;
	state_.agriculture.position        = position;
	state_.agriculture.request         = { RequestStatus::Ready, {}, {}, false };
	state_.agriculture.selectedProduct = product && hasRow( state_.agriculture.value.catalog, *product ) ? product : ( !state_.agriculture.value.product.value.empty() ? std::optional<CatalogId> { state_.agriculture.value.product } : ( state_.agriculture.value.catalog.empty() ? std::optional<CatalogId> {} : std::optional<CatalogId> { state_.agriculture.value.catalog.front().id } ) );
	state_.agriculture.selectedAnimal  = animal && hasRow( state_.agriculture.value.animals, *animal ) ? animal : ( state_.agriculture.value.animals.empty() ? std::optional<CreatureId> {} : std::optional<CreatureId> { state_.agriculture.value.animals.front().id } );
	state_.pendingAction.reset();
	state_.status.clear();
	rebuildAgriculture();
	notify();
}
void Management6AController::setTradeRows( TradeParty party, std::vector<TradeRow> rows )
{
	auto& target                = party == TradeParty::Trader ? state_.workshop.traderRows : state_.workshop.playerRows;
	target                      = std::move( rows );
	state_.workshop.tradeLoaded = true;
	if ( state_.workshop.selectedTradeRow )
	{
		const auto exists = [&]( const auto& values )
		{ return std::any_of( values.begin(), values.end(), [&]( const auto& r )
							  { return r.id == *state_.workshop.selectedTradeRow; } ); };
		if ( !exists( state_.workshop.traderRows ) && !exists( state_.workshop.playerRows ) )
			state_.workshop.selectedTradeRow.reset();
	}
	notify();
}
void Management6AController::updateTradeRow( TradeRow row )
{
	auto& target = row.id.party == TradeParty::Trader ? state_.workshop.traderRows : state_.workshop.playerRows;
	auto it      = findRow( target, row.id );
	if ( it == target.end() )
		target.push_back( std::move( row ) );
	else
		*it = std::move( row );
	notify();
}
void Management6AController::setTradeValues( std::int32_t trader, std::int32_t player )
{
	state_.workshop.traderOfferValue = trader;
	state_.workshop.playerOfferValue = player;
	notify();
}
void Management6AController::setAgricultureCatalog( AgricultureKind kind, std::vector<AgricultureCatalogRow> rows )
{
	if ( state_.agriculture.value.target.kind != kind )
		return;
	state_.agriculture.value.catalog = std::move( rows );
	if ( state_.agriculture.selectedProduct && !hasRow( state_.agriculture.value.catalog, *state_.agriculture.selectedProduct ) )
		state_.agriculture.selectedProduct.reset();
	rebuildAgriculture();
	notify();
}

template <class Id, class Row>
bool Management6AController::applyPatches( std::vector<Row>& rows, const std::vector<StableRowPatch<Id, Row>>& patches )
{
	for ( const auto& patch : patches )
	{
		auto found = findRow( rows, patch.id );
		if ( patch.kind == PatchKind::Insert )
		{
			if ( found != rows.end() || !patch.row )
				return false;
			auto before = patch.before ? findRow( rows, *patch.before ) : rows.end();
			rows.insert( before, *patch.row );
		}
		else if ( patch.kind == PatchKind::Update )
		{
			if ( found == rows.end() || !patch.row )
				return false;
			*found = *patch.row;
		}
		else if ( patch.kind == PatchKind::Remove )
		{
			if ( found == rows.end() )
				return false;
			rows.erase( found );
		}
		else
		{
			if ( found == rows.end() )
				return false;
			Row moving = *found;
			rows.erase( found );
			auto before = patch.before ? findRow( rows, *patch.before ) : rows.end();
			rows.insert( before, std::move( moving ) );
		}
	}
	return true;
}
bool Management6AController::patchWorkshopQueue( Revision base, Revision next, const std::vector<StableRowPatch<CraftJobId, CraftQueueRow>>& rows )
{
	if ( base != state_.workshop.revision || next.value <= base.value )
	{
		state_.workshop.request.status            = RequestStatus::Stale;
		state_.workshop.request.refreshInProgress = true;
		state_.status                             = "ui.error.stale_revision";
		notify();
		return false;
	}
	auto copy = state_.workshop.value.queue;
	if ( !applyPatches( copy, rows ) )
	{
		state_.workshop.request.status = RequestStatus::Stale;
		state_.status                  = "ui.error.invalid_patch";
		notify();
		return false;
	}
	state_.workshop.value.queue = std::move( copy );
	state_.workshop.revision    = next;
	if ( state_.workshop.selectedJob && !hasRow( state_.workshop.value.queue, *state_.workshop.selectedJob ) )
		state_.workshop.selectedJob.reset();
	rebuildWorkshop();
	notify();
	return true;
}
bool Management6AController::patchStockpileContents( Revision base, Revision next, const std::vector<StableRowPatch<StockpileContentRowId, StockpileContentRow>>& rows )
{
	if ( base != state_.stockpile.revision || next.value <= base.value )
	{
		state_.stockpile.request.status            = RequestStatus::Stale;
		state_.stockpile.request.refreshInProgress = true;
		state_.status                              = "ui.error.stale_revision";
		notify();
		return false;
	}
	auto copy = state_.stockpile.value.contents;
	if ( !applyPatches( copy, rows ) )
	{
		state_.status = "ui.error.invalid_patch";
		notify();
		return false;
	}
	state_.stockpile.value.contents = std::move( copy );
	state_.stockpile.revision       = next;
	if ( state_.stockpile.selectedContent && !hasRow( state_.stockpile.value.contents, *state_.stockpile.selectedContent ) )
		state_.stockpile.selectedContent.reset();
	rebuildStockpile();
	notify();
	return true;
}
bool Management6AController::patchAgricultureAnimals( Revision base, Revision next, const std::vector<StableRowPatch<CreatureId, PastureAnimalRow>>& rows )
{
	if ( base != state_.agriculture.revision || next.value <= base.value )
	{
		state_.agriculture.request.status            = RequestStatus::Stale;
		state_.agriculture.request.refreshInProgress = true;
		state_.status                                = "ui.error.stale_revision";
		notify();
		return false;
	}
	auto copy = state_.agriculture.value.animals;
	if ( !applyPatches( copy, rows ) )
	{
		state_.status = "ui.error.invalid_patch";
		notify();
		return false;
	}
	state_.agriculture.value.animals = std::move( copy );
	state_.agriculture.revision      = next;
	if ( state_.agriculture.selectedAnimal && !hasRow( state_.agriculture.value.animals, *state_.agriculture.selectedAnimal ) )
		state_.agriculture.selectedAnimal.reset();
	rebuildAgriculture();
	notify();
	return true;
}

void Management6AController::rebuildWorkshop()
{
	auto& s = state_.workshop;
	s.visibleProducts.clear();
	for ( const auto& r : s.value.products )
		if ( contains( r.id.value, s.search ) )
			s.visibleProducts.push_back( r );
	sortRows( s.visibleProducts, s.sort, []( const auto& r )
			  { return r.id.value; } );
	s.visibleQueue = s.value.queue;
	sortRows( s.visibleQueue, s.sort, []( const auto& r )
			  { return r.craft.value; } );
	s.selectionFiltered = ( s.selectedProduct && !hasRow( s.visibleProducts, *s.selectedProduct ) ) || ( s.selectedJob && !hasRow( s.visibleQueue, *s.selectedJob ) );
}
void Management6AController::rebuildStockpile()
{
	auto& s = state_.stockpile;
	s.visibleFilters.clear();
	std::array<bool, 4> ancestorsExpanded{ true, true, true, true };
	for ( const auto& r : s.value.filters )
	{
		const auto depth = static_cast<std::size_t>( r.id.depth );
		for ( std::size_t i = depth; i < ancestorsExpanded.size(); ++i )
			ancestorsExpanded[i] = true;
		const bool matches = contains( r.label, s.search ) || contains( r.id.item.value, s.search ) || contains( r.id.material.value, s.search );
		const bool visible = !s.search.empty() || std::all_of( ancestorsExpanded.begin(), ancestorsExpanded.begin() + depth, []( bool expanded )
																			{ return expanded; } );
		if ( matches && visible )
			s.visibleFilters.push_back( r );
		if ( depth < ancestorsExpanded.size() )
			ancestorsExpanded[depth] = r.state == TriState::Mixed;
	}
	s.visibleContents.clear();
	for ( const auto& r : s.value.contents )
		if ( contains( r.itemName, s.search ) || contains( r.materialName, s.search ) )
			s.visibleContents.push_back( r );
	// Filter rows are an authoritative category -> group -> item -> material
	// preorder from the Filter contract. Sorting this flattened projection by
	// label destroys the original tree's information hierarchy; contents remain
	// sortable below.
	sortRows( s.visibleContents, s.sort, []( const auto& r )
			  { return r.itemName + "\n" + r.materialName; } );
	s.selectionFiltered = ( s.selectedFilter && !hasRow( s.visibleFilters, *s.selectedFilter ) ) || ( s.selectedContent && !hasRow( s.visibleContents, *s.selectedContent ) );
}
void Management6AController::rebuildAgriculture()
{
	auto& s = state_.agriculture;
	s.visibleCatalog.clear();
	for ( const auto& r : s.value.catalog )
		if ( contains( r.name, s.search ) || contains( r.id.value, s.search ) )
			s.visibleCatalog.push_back( r );
	s.visibleAnimals.clear();
	for ( const auto& r : s.value.animals )
		if ( contains( r.name, s.search ) || contains( r.species.value, s.search ) )
			s.visibleAnimals.push_back( r );
	sortRows( s.visibleCatalog, s.sort, []( const auto& r )
			  { return r.name; } );
	sortRows( s.visibleAnimals, s.sort, []( const auto& r )
			  { return r.name; } );
	s.selectionFiltered = ( s.selectedProduct && !hasRow( s.visibleCatalog, *s.selectedProduct ) ) || ( s.selectedAnimal && !hasRow( s.visibleAnimals, *s.selectedAnimal ) );
}
void Management6AController::setSearch( std::string value )
{
	if ( state_.view == ManagementView::Workshop )
	{
		state_.workshop.search = std::move( value );
		rebuildWorkshop();
	}
	else if ( state_.view == ManagementView::Stockpile )
	{
		state_.stockpile.search = std::move( value );
		rebuildStockpile();
	}
	else if ( state_.view == ManagementView::Agriculture )
	{
		state_.agriculture.search = std::move( value );
		rebuildAgriculture();
	}
	notify();
}
void Management6AController::toggleSort()
{
	auto flip = []( auto& v )
	{ v = v == SortDirection::Ascending ? SortDirection::Descending : SortDirection::Ascending; };
	if ( state_.view == ManagementView::Workshop )
	{
		flip( state_.workshop.sort );
		rebuildWorkshop();
	}
	else if ( state_.view == ManagementView::Stockpile )
	{
		flip( state_.stockpile.sort );
		rebuildStockpile();
	}
	else if ( state_.view == ManagementView::Agriculture )
	{
		flip( state_.agriculture.sort );
		rebuildAgriculture();
	}
	notify();
}
void Management6AController::selectWorkshopProduct( CatalogId id )
{
	if ( hasRow( state_.workshop.value.products, id ) )
	{
		state_.workshop.selectedProduct = std::move( id );
		rebuildWorkshop();
		notify();
	}
}
void Management6AController::selectWorkshopJob( CraftJobId id )
{
	if ( hasRow( state_.workshop.value.queue, id ) )
	{
		state_.workshop.selectedJob = id;
		rebuildWorkshop();
		notify();
	}
}
void Management6AController::selectTradeRow( TradeRowId id )
{
	const auto exists = [&]( const auto& rows )
	{ return std::any_of( rows.begin(), rows.end(), [&]( const auto& r )
						  { return r.id == id; } ); };
	if ( exists( state_.workshop.traderRows ) || exists( state_.workshop.playerRows ) )
	{
		state_.workshop.selectedTradeRow = std::move( id );
		notify();
	}
}
void Management6AController::selectStockpileFilter( StockpileFilterRowId id )
{
	if ( hasRow( state_.stockpile.value.filters, id ) )
	{
		state_.stockpile.selectedFilter = std::move( id );
		rebuildStockpile();
		notify();
	}
}
void Management6AController::selectStockpileContent( StockpileContentRowId id )
{
	if ( hasRow( state_.stockpile.value.contents, id ) )
	{
		state_.stockpile.selectedContent = std::move( id );
		rebuildStockpile();
		notify();
	}
}
void Management6AController::selectAgricultureProduct( CatalogId id )
{
	if ( hasRow( state_.agriculture.value.catalog, id ) )
	{
		state_.agriculture.selectedProduct = std::move( id );
		rebuildAgriculture();
		notify();
	}
}
void Management6AController::selectAgricultureAnimal( CreatureId id )
{
	if ( hasRow( state_.agriculture.value.animals, id ) )
	{
		state_.agriculture.selectedAnimal = id;
		rebuildAgriculture();
		notify();
	}
}
void Management6AController::nextWorkshopProduct()
{
	auto& s = state_.workshop.visibleProducts;
	if ( s.empty() )
		return;
	auto i = state_.workshop.selectedProduct ? std::find_if( s.begin(), s.end(), [&]( const auto& r )
															 { return r.id == *state_.workshop.selectedProduct; } )
											 : s.end();
	selectWorkshopProduct( ( i == s.end() || ++i == s.end() ? s.begin() : i )->id );
}
void Management6AController::nextWorkshopJob()
{
	auto& s = state_.workshop.visibleQueue;
	if ( s.empty() )
		return;
	auto i = state_.workshop.selectedJob ? std::find_if( s.begin(), s.end(), [&]( const auto& r )
														 { return r.id == *state_.workshop.selectedJob; } )
										 : s.end();
	selectWorkshopJob( ( i == s.end() || ++i == s.end() ? s.begin() : i )->id );
}
void Management6AController::nextTradeRow()
{
	std::vector<TradeRow> rows = state_.workshop.traderRows;
	rows.insert( rows.end(), state_.workshop.playerRows.begin(), state_.workshop.playerRows.end() );
	if ( rows.empty() )
		return;
	auto i                           = state_.workshop.selectedTradeRow ? std::find_if( rows.begin(), rows.end(), [&]( const auto& r )
																						{ return r.id == *state_.workshop.selectedTradeRow; } )
																		: rows.end();
	state_.workshop.selectedTradeRow = ( i == rows.end() || ++i == rows.end() ? rows.begin() : i )->id;
	notify();
}
void Management6AController::moveStockpileFilterSelection( std::int32_t delta )
{
	auto& rows = state_.stockpile.visibleFilters;
	if ( rows.empty() )
		return;
	auto current = state_.stockpile.selectedFilter ? std::find_if( rows.begin(), rows.end(), [&]( const auto& r )
																			 { return r.id == *state_.stockpile.selectedFilter; } )
																			: rows.end();
	const auto start = current == rows.end() ? ( delta < 0 ? static_cast<std::int32_t>( rows.size() - 1 ) : 0 ) : static_cast<std::int32_t>( std::distance( rows.begin(), current ) );
	const auto last  = static_cast<std::int32_t>( rows.size() - 1 );
	selectStockpileFilter( rows[static_cast<std::size_t>( std::clamp( start + delta, 0, last ) )].id );
}
void Management6AController::nextStockpileFilter()
{
	auto& s = state_.stockpile.visibleFilters;
	if ( s.empty() )
		return;
	auto i = state_.stockpile.selectedFilter ? std::find_if( s.begin(), s.end(), [&]( const auto& r )
															 { return r.id == *state_.stockpile.selectedFilter; } )
											 : s.end();
	selectStockpileFilter( ( i == s.end() || ++i == s.end() ? s.begin() : i )->id );
}
void Management6AController::nextStockpileContent()
{
	auto& s = state_.stockpile.visibleContents;
	if ( s.empty() )
		return;
	auto i = state_.stockpile.selectedContent ? std::find_if( s.begin(), s.end(), [&]( const auto& r )
															  { return r.id == *state_.stockpile.selectedContent; } )
											  : s.end();
	selectStockpileContent( ( i == s.end() || ++i == s.end() ? s.begin() : i )->id );
}
void Management6AController::nextAgricultureProduct()
{
	auto& s = state_.agriculture.visibleCatalog;
	if ( s.empty() )
		return;
	auto i = state_.agriculture.selectedProduct ? std::find_if( s.begin(), s.end(), [&]( const auto& r )
																{ return r.id == *state_.agriculture.selectedProduct; } )
												: s.end();
	selectAgricultureProduct( ( i == s.end() || ++i == s.end() ? s.begin() : i )->id );
}
void Management6AController::nextAgricultureAnimal()
{
	auto& s = state_.agriculture.visibleAnimals;
	if ( s.empty() )
		return;
	auto i = state_.agriculture.selectedAnimal ? std::find_if( s.begin(), s.end(), [&]( const auto& r )
															   { return r.id == *state_.agriculture.selectedAnimal; } )
											   : s.end();
	selectAgricultureAnimal( ( i == s.end() || ++i == s.end() ? s.begin() : i )->id );
}

bool Management6AController::dispatch( std::string_view id, UiActionPayload payload, DispatchOrigin origin )
{
	if ( state_.workshop.tradeConfirmationRequired && origin != DispatchOrigin::DestructiveConfirmation )
	{
		state_.status = "ui.error.input_blocked_by_confirmation";
		notify();
		return false;
	}
	if ( !state_.acceptsWorldActions || !state_.world )
	{
		state_.status = "ui.error.world_unavailable";
		notify();
		return false;
	}
	UiActionEnvelope action { ActionId { id }, RequestId { nextRequest_++ }, state_.world, std::nullopt, std::move( payload ) };
	auto result = commands_.dispatch( action, origin );
	if ( result.status == CommandStatus::Rejected )
	{
		state_.status = result.error;
		notify();
		return false;
	}
	state_.status.clear();
	if ( result.pending )
		state_.pendingAction = action.request;
	notify();
	return true;
}
void Management6AController::refresh()
{
	if ( state_.view == ManagementView::Workshop && state_.workshop.value.id )
		dispatch( "workshop.refresh", WorkshopTargetPayload { state_.workshop.value.id } );
	else if ( state_.view == ManagementView::Stockpile && state_.stockpile.value.id )
		dispatch( "stockpile.refresh", StockpileTargetPayload { state_.stockpile.value.id } );
	else if ( state_.view == ManagementView::Agriculture && state_.agriculture.value.target.designation )
		dispatch( "agriculture.refresh", AgricultureTargetPayload { state_.agriculture.value.target } );
}
void Management6AController::locate()
{
	std::optional<EntityRef> target;
	if ( state_.view == ManagementView::Workshop && state_.workshop.value.id )
		target = EntityRef { state_.world, EntityKind::Workshop, state_.workshop.value.id.value, state_.workshop.position };
	else if ( state_.view == ManagementView::Stockpile && state_.stockpile.value.id )
		target = EntityRef { state_.world, EntityKind::Stockpile, state_.stockpile.value.id.value, state_.stockpile.position };
	else if ( state_.view == ManagementView::Agriculture && state_.agriculture.value.target.designation )
	{
		const auto& t = state_.agriculture.value.target;
		target        = EntityRef { state_.world, t.kind == AgricultureKind::Farm ? EntityKind::Farm : t.kind == AgricultureKind::Pasture ? EntityKind::Pasture
																																		  : EntityKind::Grove,
                             t.designation.value, state_.agriculture.position };
	}
	if ( target && target->position )
		dispatch( "view.center_on", CenterPayload { *target } );
	else
	{
		state_.status = "ui.error.target_has_no_position";
		notify();
	}
}
void Management6AController::close()
{
	if ( dispatch( "nav.close", NoPayload {} ) )
	{
		state_.view = ManagementView::None;
		notify();
	}
}
void Management6AController::setWorkshopBasics( std::string name, std::int32_t priority, bool suspended, bool generated, bool autoMissing, std::optional<bool> linkStockpile )
{
	const auto& id = state_.workshop.value.id;
	if ( id )
		dispatch( "workshop.set_basics", SetWorkshopBasicsPayload { id, std::move( name ), priority, suspended, generated, autoMissing, std::nullopt, linkStockpile } );
}
void Management6AController::setButcherOptions( bool corpses, bool excess )
{
	if ( state_.workshop.value.id )
		dispatch( "workshop.set_butcher_options", SetButcherOptionsPayload { state_.workshop.value.id, corpses, excess } );
}
void Management6AController::setFisherOptions( bool catchFish, bool processFish )
{
	if ( state_.workshop.value.id )
		dispatch( "workshop.set_fisher_options", SetFisherOptionsPayload { state_.workshop.value.id, catchFish, processFish } );
}
void Management6AController::queueSelectedCraft( CraftRepeatMode mode, std::uint32_t count, std::vector<CatalogId> materials )
{
	if ( state_.workshop.value.id && state_.workshop.selectedProduct && count > 0 )
		dispatch( "workshop.queue_craft", QueueCraftPayload { state_.workshop.value.id, *state_.workshop.selectedProduct, mode, count, std::move( materials ) } );
}
void Management6AController::queueSelectedCraftDefault()
{
	if ( !state_.workshop.selectedProduct )
		return;
	auto product = findRow( state_.workshop.value.products, *state_.workshop.selectedProduct );
	if ( product == state_.workshop.value.products.end() )
		return;
	std::vector<CatalogId> materials;
	for ( const auto& component : product->components )
	{
		auto choice = std::find_if( component.materials.begin(), component.materials.end(), [&]( const auto& v )
									{ return v.second >= component.amount; } );
		if ( choice == component.materials.end() )
		{
			state_.status = "ui.workshop.missing_components";
			notify();
			return;
		}
		materials.push_back( choice->first );
	}
	queueSelectedCraft( CraftRepeatMode::Once, 1, std::move( materials ) );
}
void Management6AController::setSelectedJob( CraftRepeatMode mode, std::uint32_t count, bool suspended, bool moveBack )
{
	if ( state_.workshop.value.id && state_.workshop.selectedJob && count > 0 )
		dispatch( "workshop.set_job", SetCraftJobPayload { state_.workshop.value.id, *state_.workshop.selectedJob, mode, count, suspended, moveBack } );
}
void Management6AController::moveSelectedJob( MoveDirection direction )
{
	if ( state_.workshop.value.id && state_.workshop.selectedJob )
		dispatch( "workshop.move_job", MoveCraftJobPayload { state_.workshop.value.id, *state_.workshop.selectedJob, direction } );
}
void Management6AController::cancelSelectedJob()
{
	if ( state_.workshop.value.id && state_.workshop.selectedJob )
		dispatch( "workshop.cancel_job", CraftJobTargetPayload { state_.workshop.value.id, *state_.workshop.selectedJob } );
}
void Management6AController::refreshTrade()
{
	if ( state_.workshop.value.id )
		dispatch( "trade.refresh", WorkshopTargetPayload { state_.workshop.value.id } );
}
void Management6AController::setTradeOffer( TradeRowId row, std::uint32_t count )
{
	if ( state_.workshop.value.id )
		dispatch( "trade.set_offer_count", SetTradeOfferPayload { state_.workshop.value.id, std::move( row ), count } );
}
void Management6AController::adjustSelectedTradeOffer( std::int32_t delta )
{
	if ( !state_.workshop.selectedTradeRow )
		return;
	const TradeRow* row = nullptr;
	for ( const auto& v : state_.workshop.traderRows )
		if ( v.id == *state_.workshop.selectedTradeRow )
			row = &v;
	for ( const auto& v : state_.workshop.playerRows )
		if ( v.id == *state_.workshop.selectedTradeRow )
			row = &v;
	if ( !row )
		return;
	const auto desired = std::clamp<std::int64_t>( static_cast<std::int64_t>( row->offered ) + delta, 0, static_cast<std::int64_t>( row->stock ) + row->offered );
	setTradeOffer( row->id, static_cast<std::uint32_t>( desired ) );
}
void Management6AController::executeTrade()
{
	if ( state_.workshop.playerOfferValue < state_.workshop.traderOfferValue )
	{
		state_.status = "ui.trade.offer_value_too_low";
		notify();
		return;
	}
	state_.workshop.tradeConfirmationRequired = true;
	state_.status                             = "ui.trade.confirmation_required";
	notify();
}
void Management6AController::confirmTrade()
{
	if ( state_.workshop.tradeConfirmationRequired && state_.workshop.value.id && state_.workshop.playerOfferValue >= state_.workshop.traderOfferValue )
	{
		const auto workshop = state_.workshop.value.id;
		if ( dispatch( "trade.execute", WorkshopTargetPayload { workshop }, DispatchOrigin::DestructiveConfirmation ) )
		{
			state_.workshop.tradeConfirmationRequired = false;
			notify();
		}
	}
}
void Management6AController::cancelTrade()
{
	if ( !state_.workshop.tradeConfirmationRequired )
		return;
	state_.workshop.tradeConfirmationRequired = false;
	state_.status.clear();
	notify();
}
void Management6AController::setStockpileBasics( std::string name, std::int32_t priority, bool suspended, bool pull, bool allow )
{
	if ( state_.stockpile.value.id )
		dispatch( "stockpile.set_basics", SetStockpileBasicsPayload { state_.stockpile.value.id, std::move( name ), priority, suspended, pull, allow } );
}
void Management6AController::toggleSelectedStockpileFilter()
{
	if ( !state_.stockpile.selectedFilter )
		return;
	auto it = findRow( state_.stockpile.value.filters, *state_.stockpile.selectedFilter );
	if ( it != state_.stockpile.value.filters.end() )
		// The original StockpileModel sends false when a tri-state parent is
		// indeterminate: Nullable<bool>::HasValue() is false in that case.
		dispatch( "stockpile.set_filter", SetStockpileFilterPayload { it->id, it->state == TriState::Off } );
}
void Management6AController::setAgricultureBasics( std::string name, std::int32_t priority, bool suspended )
{
	if ( state_.agriculture.value.target.designation )
		dispatch( "agriculture.set_basics", SetAgricultureBasicsPayload { state_.agriculture.value.target, std::move( name ), priority, suspended } );
}
void Management6AController::applySelectedAgricultureProduct()
{
	if ( state_.agriculture.selectedProduct )
		dispatch( "agriculture.select_product", SetAgricultureProductPayload { state_.agriculture.value.target, *state_.agriculture.selectedProduct } );
}
void Management6AController::setHarvestOptions( bool harvest, bool hay, bool tame )
{
	dispatch( "agriculture.set_harvest_options", SetHarvestOptionsPayload { state_.agriculture.value.target, harvest, hay, tame } );
}
void Management6AController::setGroveOptions( bool pick, bool plant, bool fell )
{
	dispatch( "agriculture.set_grove_options", SetGroveOptionsPayload { state_.agriculture.value.target.designation, pick, plant, fell } );
}
void Management6AController::setPastureCap( Gender gender, std::uint32_t max )
{
	dispatch( "agriculture.set_population_caps", SetPastureCapPayload { state_.agriculture.value.target.designation, gender, max } );
}
void Management6AController::toggleSelectedAnimalButchering()
{
	if ( !state_.agriculture.selectedAnimal )
		return;
	auto it = findRow( state_.agriculture.value.animals, *state_.agriculture.selectedAnimal );
	if ( it != state_.agriculture.value.animals.end() )
		dispatch( "agriculture.set_butchering", SetButcheringPayload { it->id, !it->butcher } );
}
void Management6AController::setPastureFood( CatalogId item, CatalogId material, bool allowed )
{
	dispatch( "agriculture.set_food_allowed", SetPastureFoodPayload { state_.agriculture.value.target.designation, std::move( item ), std::move( material ), allowed } );
}
void Management6AController::onActionFinished( RequestId id, CommandResult result )
{
	if ( state_.pendingAction != id )
		return;
	state_.pendingAction.reset();
	state_.status = result.status == CommandStatus::Rejected ? result.error : std::string {};
	notify();
}
} // namespace ingnomia::ui::management6a
