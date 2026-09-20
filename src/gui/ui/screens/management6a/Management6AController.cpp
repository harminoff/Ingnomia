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
bool stockpileFilterIsAncestor( const StockpileFilterRowId& ancestor, const StockpileFilterRowId& child )
{
	if ( static_cast<int>( ancestor.depth ) >= static_cast<int>( child.depth ) || ancestor.category != child.category )
		return false;
	if ( ancestor.depth == FilterDepth::Category )
		return true;
	if ( ancestor.group != child.group )
		return false;
	return ancestor.depth == FilterDepth::Group || ancestor.item == child.item;
}
bool stockpileFilterRowMatches( const StockpileFilterRow& row, const std::string& search )
{
	return contains( row.label, search ) || contains( row.id.category.value, search ) || contains( row.id.group.value, search ) || contains( row.id.item.value, search ) || contains( row.id.material.value, search );
}
bool stockpileFilterLeafMatches( const std::vector<StockpileFilterRow>& filters, std::size_t leafIndex, const std::string& search )
{
	if ( search.empty() )
		return true;
	const auto& leaf = filters[leafIndex];
	if ( stockpileFilterRowMatches( leaf, search ) )
		return true;
	for ( std::size_t index = leafIndex; index-- > 0; )
	{
		const auto& ancestor = filters[index];
		if ( stockpileFilterIsAncestor( ancestor.id, leaf.id ) && stockpileFilterRowMatches( ancestor, search ) )
			return true;
	}
	return false;
}
} // namespace

Management6AController::Management6AController( CommandPort& commands, ViewPort& view ) :
	commands_( commands ), views_{ &view }
{
	notify();
}
void Management6AController::addViewPort( ViewPort& view )
{
	if ( std::ranges::find( views_, &view ) != views_.end() ) return;
	views_.push_back( &view );
	view.stateChanged( state_ );
}
void Management6AController::removeViewPort( ViewPort& view )
{
	std::erase( views_, &view );
}
void Management6AController::notify()
{
	++state_.revision.value;
	const auto views = views_;
	for ( auto* view : views )
		if ( view ) view->stateChanged( state_ );
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
	const auto previousProduct      = state_.workshop.selectedProduct;
	const auto product              = state_.workshop.value.id == value.id ? state_.workshop.selectedProduct : std::optional<CatalogId> {};
	const auto job                  = state_.workshop.value.id == value.id ? state_.workshop.selectedJob : std::optional<CraftJobId> {};
	state_.view                     = ManagementView::Workshop;
	state_.workshop.value           = std::move( value );
	state_.workshop.revision        = revision;
	state_.workshop.position        = position;
	state_.workshop.request         = { state_.workshop.value.products.empty() && state_.workshop.value.queue.empty() ? RequestStatus::Empty : RequestStatus::Ready, {}, {}, false };
	state_.workshop.selectedProduct = product && hasRow( state_.workshop.value.products, *product ) ? product : ( state_.workshop.value.products.empty() ? std::optional<CatalogId> {} : std::optional<CatalogId> { state_.workshop.value.products.front().id } );
	state_.workshop.selectedJob     = job && hasRow( state_.workshop.value.queue, *job ) ? job : ( state_.workshop.value.queue.empty() ? std::optional<CraftJobId> {} : std::optional<CraftJobId> { state_.workshop.value.queue.front().id } );
	if ( previousProduct != state_.workshop.selectedProduct )
		resetWorkshopOrderDraft();
	else
		normalizeWorkshopOrderDraft();
	state_.pendingAction.reset();
	state_.status.clear();
	rebuildWorkshop();
	notify();
}
void Management6AController::showStockpile( StockpileSnapshot value, Revision revision, std::optional<WorldPosition> position )
{
	if ( !state_.acceptsWorldActions || !value.id || ( state_.stockpile.value.id == value.id && revision.value <= state_.stockpile.revision.value ) )
		return;
	const bool same                  = state_.stockpile.value.id == value.id;
	const auto filter                = same ? state_.stockpile.selectedFilter : std::optional<StockpileFilterRowId> {};
	const auto content               = same ? state_.stockpile.selectedContent : std::optional<StockpileContentRowId> {};
	const auto expanded              = same ? state_.stockpile.expandedFilters : std::vector<StockpileFilterRowId> {};
	const bool hasAllowedRules       = std::any_of( value.filters.begin(), value.filters.end(), []( const auto& row )
																		 { return row.id.depth == FilterDepth::Material && row.state == TriState::On; } );
	state_.view                      = ManagementView::Stockpile;
	state_.stockpile.value           = std::move( value );
	state_.stockpile.revision        = revision;
	state_.stockpile.position        = position;
	state_.stockpile.request         = { state_.stockpile.value.filters.empty() && state_.stockpile.value.contents.empty() ? RequestStatus::Empty : RequestStatus::Ready, {}, {}, false };
	state_.stockpile.selectedFilter  = filter && hasRow( state_.stockpile.value.filters, *filter ) ? filter : ( state_.stockpile.value.filters.empty() ? std::optional<StockpileFilterRowId> {} : std::optional<StockpileFilterRowId> { state_.stockpile.value.filters.front().id } );
	state_.stockpile.selectedContent = content && hasRow( state_.stockpile.value.contents, *content ) ? content : ( state_.stockpile.value.contents.empty() ? std::optional<StockpileContentRowId> {} : std::optional<StockpileContentRowId> { state_.stockpile.value.contents.front().id } );
	if ( !same )
	{
		state_.stockpile.search.clear();
		state_.stockpile.filterSearch.clear();
		state_.stockpile.contentSearch.clear();
		state_.stockpile.filterSearchBeforeReveal.clear();
		state_.stockpile.filterSearchRevealed = false;
		state_.stockpile.expandedFilters.clear();
		state_.stockpile.pane = hasAllowedRules ? StockpilePane::Contents : StockpilePane::AllowList;
	}
	else
		state_.stockpile.expandedFilters = expanded;
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
	s.visibleQueue.clear();
	for ( const auto& r : s.value.queue )
		if ( contains( r.craft.value, s.search ) || contains( std::to_string( r.id.value ), s.search ) )
			s.visibleQueue.push_back( r );
	s.selectionFiltered = ( s.selectedProduct && !hasRow( s.visibleProducts, *s.selectedProduct ) ) || ( s.selectedJob && !hasRow( s.visibleQueue, *s.selectedJob ) );
}
void Management6AController::rebuildStockpile()
{
	auto& s = state_.stockpile;
	s.visibleFilters.clear();
	const auto expanded = [&]( const StockpileFilterRowId& id )
	{ return std::find( s.expandedFilters.begin(), s.expandedFilters.end(), id ) != s.expandedFilters.end(); };
	std::vector<bool> matchingLeaves( s.value.filters.size(), false );
	for ( std::size_t index = 0; index < s.value.filters.size(); ++index )
		if ( s.value.filters[index].id.depth == FilterDepth::Material )
			matchingLeaves[index] = stockpileFilterLeafMatches( s.value.filters, index, s.filterSearch );
	const auto hasMatchingDescendant = [&]( std::size_t index )
	{
		if ( s.filterSearch.empty() )
			return false;
		for ( std::size_t next = index + 1; next < s.value.filters.size(); ++next )
		{
			const auto& candidate = s.value.filters[next];
			if ( static_cast<int>( candidate.id.depth ) <= static_cast<int>( s.value.filters[index].id.depth ) )
				break;
			if ( candidate.id.depth == FilterDepth::Material && matchingLeaves[next] )
				return true;
		}
		return false;
	};
	for ( std::size_t index = 0; index < s.value.filters.size(); ++index )
	{
		const auto& row = s.value.filters[index];
		const bool searchMatch = row.id.depth == FilterDepth::Material ? matchingLeaves[index] : hasMatchingDescendant( index );
		bool open = row.id.depth == FilterDepth::Category;
		// A non-empty search temporarily reveals matching ancestor paths. The
		// saved disclosure state still controls the normal, unfiltered tree.
		if ( s.filterSearch.empty() && !open )
		{
			for ( const auto& ancestor : s.value.filters )
				if ( stockpileFilterIsAncestor( ancestor.id, row.id ) && !expanded( ancestor.id ) )
				{
					open = false;
					goto hidden_by_parent;
				}
			open = true;
		}
		if ( ( s.filterSearch.empty() && open ) || ( !s.filterSearch.empty() && searchMatch ) )
			s.visibleFilters.push_back( row );
	hidden_by_parent:;
	}
	s.visibleContents.clear();
	for ( const auto& r : s.value.contents )
		if ( contains( r.itemName, s.contentSearch ) || contains( r.materialName, s.contentSearch ) )
			s.visibleContents.push_back( r );
	// Filter rows are an authoritative category -> group -> item -> material
	// preorder from the Filter contract. Sorting this flattened projection by
	// label destroys the original tree's information hierarchy; contents remain
	// sortable below.
	if ( s.contentSort == StockpileSortKey::Quantity )
		std::stable_sort( s.visibleContents.begin(), s.visibleContents.end(), [&]( const auto& a, const auto& b )
																				 { return s.sort == SortDirection::Ascending ? ( a.count != b.count ? a.count < b.count : a.itemName < b.itemName ) : ( a.count != b.count ? a.count > b.count : a.itemName > b.itemName ); } );
	else
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
		state_.stockpile.filterSearch = state_.stockpile.search;
		state_.stockpile.contentSearch = state_.stockpile.search;
		rebuildStockpile();
	}
	else if ( state_.view == ManagementView::Agriculture )
	{
		state_.agriculture.search = std::move( value );
		rebuildAgriculture();
	}
	notify();
}
void Management6AController::setStockpileFilterSearch( std::string value )
{
	if ( state_.view == ManagementView::Stockpile )
	{
		state_.stockpile.filterSearch = std::move( value );
		state_.stockpile.filterSearchRevealed = false;
		rebuildStockpile();
		notify();
	}
}
void Management6AController::setStockpileContentSearch( std::string value )
{
	if ( state_.view == ManagementView::Stockpile )
	{
		state_.stockpile.contentSearch = std::move( value );
		rebuildStockpile();
		notify();
	}
}
void Management6AController::setStockpileContentSort( StockpileSortKey key )
{
	if ( state_.view != ManagementView::Stockpile )
		return;
	if ( state_.stockpile.contentSort == key )
		state_.stockpile.sort = state_.stockpile.sort == SortDirection::Ascending ? SortDirection::Descending : SortDirection::Ascending;
	else
	{
		state_.stockpile.contentSort = key;
		state_.stockpile.sort = SortDirection::Ascending;
	}
	rebuildStockpile();
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
		const bool changed = !state_.workshop.selectedProduct || *state_.workshop.selectedProduct != id;
		state_.workshop.selectedProduct = std::move( id );
		if ( changed )
			resetWorkshopOrderDraft();
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
void Management6AController::setWorkshopOrderMode( CraftRepeatMode mode )
{
	state_.workshop.orderMode = mode;
	notify();
}
void Management6AController::setWorkshopOrderCount( std::uint32_t count )
{
	state_.workshop.orderCount = std::clamp<std::uint32_t>( count, 1, 999 );
	notify();
}
void Management6AController::resetWorkshopOrderDraft()
{
	state_.workshop.orderMode = CraftRepeatMode::Once;
	state_.workshop.orderCount = 1;
	state_.workshop.orderMaterials.clear();
	normalizeWorkshopOrderDraft();
}
void Management6AController::normalizeWorkshopOrderDraft()
{
	if ( !state_.workshop.selectedProduct )
	{
		state_.workshop.orderMaterials.clear();
		return;
	}
	const auto product = findRow( state_.workshop.value.products, *state_.workshop.selectedProduct );
	if ( product == state_.workshop.value.products.end() )
	{
		state_.workshop.orderMaterials.clear();
		return;
	}
	state_.workshop.orderMaterials.resize( product->components.size() );
	for ( std::size_t index = 0; index < product->components.size(); ++index )
	{
		const auto& choices = product->components[index].materials;
		const bool valid = std::any_of( choices.begin(), choices.end(), [&]( const auto& choice )
									   { return choice.first == state_.workshop.orderMaterials[index]; } );
		if ( !valid )
			state_.workshop.orderMaterials[index] = choices.empty() ? CatalogId { "any" } : choices.front().first;
	}
}
void Management6AController::cycleWorkshopOrderMaterial( std::size_t componentIndex, std::int32_t direction )
{
	if ( !state_.workshop.selectedProduct )
		return;
	const auto product = findRow( state_.workshop.value.products, *state_.workshop.selectedProduct );
	if ( product == state_.workshop.value.products.end() || componentIndex >= product->components.size() )
		return;
	const auto& choices = product->components[componentIndex].materials;
	if ( choices.empty() )
		return;
	normalizeWorkshopOrderDraft();
	auto current = std::find_if( choices.begin(), choices.end(), [&]( const auto& choice )
								 { return choice.first == state_.workshop.orderMaterials[componentIndex]; } );
	const auto start = current == choices.end() ? 0 : static_cast<std::int32_t>( std::distance( choices.begin(), current ) );
	const auto size = static_cast<std::int32_t>( choices.size() );
	const auto next = ( start + ( direction < 0 ? -1 : 1 ) + size ) % size;
	state_.workshop.orderMaterials[componentIndex] = choices[static_cast<std::size_t>( next )].first;
	notify();
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
		const auto selected = std::find_if( state_.stockpile.value.contents.begin(), state_.stockpile.value.contents.end(), [&]( const auto& row ) { return row.id == *state_.stockpile.selectedContent; } );
		if ( selected != state_.stockpile.value.contents.end() )
		{
			state_.stockpile.filterSearchBeforeReveal = state_.stockpile.filterSearch;
			state_.stockpile.filterSearch = selected->materialName.empty() ? selected->itemName : selected->materialName;
			state_.stockpile.filterSearchRevealed = true;
			state_.stockpile.pane = StockpilePane::AllowList;
		}
		rebuildStockpile();
		notify();
	}
}
void Management6AController::toggleStockpileFilterExpansion( StockpileFilterRowId id )
{
	if ( id.depth == FilterDepth::Material || !hasRow( state_.stockpile.value.filters, id ) )
		return;
	const auto found = std::find( state_.stockpile.expandedFilters.begin(), state_.stockpile.expandedFilters.end(), id );
	if ( found == state_.stockpile.expandedFilters.end() )
		state_.stockpile.expandedFilters.push_back( std::move( id ) );
	else
		state_.stockpile.expandedFilters.erase( found );
	rebuildStockpile();
	notify();
}
void Management6AController::restoreStockpileFilterSearch()
{
	if ( !state_.stockpile.filterSearchRevealed )
		return;
	state_.stockpile.filterSearch = std::move( state_.stockpile.filterSearchBeforeReveal );
	state_.stockpile.filterSearchRevealed = false;
	rebuildStockpile();
	notify();
}
void Management6AController::setStockpilePane( StockpilePane pane )
{
	if ( state_.view != ManagementView::Stockpile )
		return;
	state_.stockpile.pane = pane;
	notify();
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
	resetWorkshopOrderDraft();
	queueSelectedCraftOrder();
}
void Management6AController::queueSelectedCraftOrder()
{
	normalizeWorkshopOrderDraft();
	queueSelectedCraft( state_.workshop.orderMode, state_.workshop.orderCount, state_.workshop.orderMaterials );
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
		dispatch( "stockpile.set_filter", SetStockpileFilterPayload { it->id, it->state != TriState::On } );
}
void Management6AController::setStockpileFilterMatches( bool active )
{
	if ( !state_.stockpile.value.id )
		return;
	for ( std::size_t index = 0; index < state_.stockpile.value.filters.size(); ++index )
	{
		const auto& row = state_.stockpile.value.filters[index];
		if ( row.id.depth == FilterDepth::Material && stockpileFilterLeafMatches( state_.stockpile.value.filters, index, state_.stockpile.filterSearch ) )
			dispatch( "stockpile.set_filter", SetStockpileFilterPayload { row.id, active } );
	}
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
