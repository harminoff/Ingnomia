/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6AController.h"
#include "../InventoryTableSchema.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <unordered_map>
#include <unordered_set>

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
std::string trimmed( std::string value )
{
	const auto notSpace = []( unsigned char c ) { return std::isspace( c ) == 0; };
	value.erase( value.begin(), std::find_if( value.begin(), value.end(), notSpace ) );
	value.erase( std::find_if( value.rbegin(), value.rend(), notSpace ).base(), value.end() );
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
bool stockpileFilterRowMatchesFolded( const StockpileFilterRow& row, const std::string& search )
{
	if ( search.empty() )
		return true;
	const auto matches = [&]( const std::string& value ) { return folded( value ).find( search ) != std::string::npos; };
	return matches( row.label ) || matches( row.id.category.value ) || matches( row.id.group.value ) || matches( row.id.item.value ) || matches( row.id.material.value );
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
	const bool sameWorkshop = state_.workshop.value.id == value.id;
	if ( !sameWorkshop )
	{
		state_.workshop.tradeConfirmationRequired = false;
		state_.workshop.orderPending=false;
		state_.workshop.orderFeedback.clear();
		state_.workshop.search.clear();
		state_.workshop.pane = value.products.empty() ? WorkshopPane::Settings : WorkshopPane::Craft;
	}
	const auto previousProduct      = state_.workshop.selectedProduct;
	const auto product              = state_.workshop.value.id == value.id ? state_.workshop.selectedProduct : std::optional<CatalogId> {};
	const auto job                  = state_.workshop.value.id == value.id ? state_.workshop.selectedJob : std::optional<CraftJobId> {};
	state_.view                     = ManagementView::Workshop;
	state_.workshop.value           = std::move( value );
	state_.workshop.revision        = revision;
	state_.workshop.position        = position;
	state_.workshop.request         = { RequestStatus::Ready, {}, {}, false };
	state_.workshop.selectedProduct = product && hasRow( state_.workshop.value.products, *product ) ? product : ( state_.workshop.value.products.empty() ? std::optional<CatalogId> {} : std::optional<CatalogId> { state_.workshop.value.products.front().id } );
	state_.workshop.selectedJob     = job && hasRow( state_.workshop.value.queue, *job ) ? job : ( state_.workshop.value.queue.empty() ? std::optional<CraftJobId> {} : std::optional<CraftJobId> { state_.workshop.value.queue.front().id } );
	if ( !sameWorkshop || previousProduct != state_.workshop.selectedProduct )
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
	const auto expandedContents      = same ? state_.stockpile.expandedContents : std::vector<StockpileContentRowId> {};
	state_.view                      = ManagementView::Stockpile;
	state_.stockpile.value           = std::move( value );
	if ( !state_.stockpile.filterCategory.value.empty() && std::none_of( state_.stockpile.value.filters.begin(), state_.stockpile.value.filters.end(), [&]( const auto& row ) { return row.id.depth == FilterDepth::Category && row.id.category == state_.stockpile.filterCategory; } ) )
		state_.stockpile.filterCategory = {};
	state_.stockpile.revision        = revision;
	state_.stockpile.position        = position;
	state_.stockpile.request         = { state_.stockpile.value.filters.empty() && state_.stockpile.value.contents.empty() ? RequestStatus::Empty : RequestStatus::Ready, {}, {}, false };
	const auto firstListRow = std::find_if( state_.stockpile.value.filters.begin(), state_.stockpile.value.filters.end(), []( const auto& row ) { return row.id.depth != FilterDepth::Category; } );
	state_.stockpile.selectedFilter  = filter && hasRow( state_.stockpile.value.filters, *filter ) && filter->depth != FilterDepth::Category ? filter : ( firstListRow == state_.stockpile.value.filters.end() ? std::optional<StockpileFilterRowId> {} : std::optional<StockpileFilterRowId> { firstListRow->id } );
	state_.stockpile.selectedContent = content && hasRow( state_.stockpile.value.contents, *content ) ? content : ( state_.stockpile.value.contents.empty() ? std::optional<StockpileContentRowId> {} : std::optional<StockpileContentRowId> { state_.stockpile.value.contents.front().id } );
	if ( !same )
	{
		state_.stockpile.search.clear();
			state_.stockpile.filterSearch.clear();
			state_.stockpile.contentSearch.clear();
		state_.stockpile.filterSearchBeforeReveal.clear();
		state_.stockpile.templateName.clear();
		state_.stockpile.pendingTemplateOverwrite.clear();
		state_.stockpile.filterCategory = {};
		state_.stockpile.filterSearchRevealed = false;
		state_.stockpile.templateMenuOpen = false;
		state_.stockpile.templateOverwriteConfirmationRequired = false;
		state_.stockpile.expandedFilters.clear();
		state_.stockpile.expandedContents.clear();
		state_.stockpile.pane = StockpilePane::Contents;
	}
	else
	{
		state_.stockpile.expandedFilters = expanded;
		state_.stockpile.expandedContents = expandedContents;
	}
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
	if ( !same ) state_.agriculture.pane = AgriculturePane::Overview;
	const auto product                 = same ? state_.agriculture.selectedProduct : std::optional<CatalogId> {};
	const auto selectedPlots = same ? state_.agriculture.selectedPlots : std::vector<WorldPosition> {};
	const auto animal                  = same ? state_.agriculture.selectedAnimal : std::optional<CreatureId> {};
	state_.view                        = ManagementView::Agriculture;
	state_.agriculture.value           = std::move( value );
	state_.agriculture.revision        = revision;
	state_.agriculture.position        = position;
	state_.agriculture.request         = { RequestStatus::Ready, {}, {}, false };
	state_.agriculture.selectedProduct = product && hasRow( state_.agriculture.value.catalog, *product ) ? product : ( !state_.agriculture.value.product.value.empty() ? std::optional<CatalogId> { state_.agriculture.value.product } : std::optional<CatalogId> {} );
	state_.agriculture.selectedPlots.clear();
	for ( const auto& plot : selectedPlots )
		if ( std::ranges::any_of( state_.agriculture.value.fields, [&]( const auto& field ) { return field.position == plot; } ) )
			state_.agriculture.selectedPlots.push_back( plot );
	state_.agriculture.selectedAnimal  = animal && hasRow( state_.agriculture.value.animals, *animal ) ? animal : ( state_.agriculture.value.animals.empty() ? std::optional<CreatureId> {} : std::optional<CreatureId> { state_.agriculture.value.animals.front().id } );
	state_.pendingAction.reset();
	state_.status.clear();
	rebuildAgriculture();
	notify();
}
void Management6AController::setAgriculturePane( AgriculturePane pane )
{
	if ( state_.view != ManagementView::Agriculture || state_.agriculture.pane == pane )
		return;
	state_.agriculture.pane = pane;
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
		s.visibleQueue.push_back( r );
	s.selectionFiltered = ( s.selectedProduct && !hasRow( s.visibleProducts, *s.selectedProduct ) ) || ( s.selectedJob && !hasRow( s.visibleQueue, *s.selectedJob ) );
}
void Management6AController::rebuildStockpile()
{
	auto& s = state_.stockpile;
	s.visibleFilters.clear();
	s.matchingFilterLeaves.clear();
	const auto foldedSearch = folded( s.filterSearch );
	std::vector<bool> matchingLeaves( s.value.filters.size(), false );
	std::vector<bool> pathMatches( s.value.filters.size(), false );
	std::vector<bool> hasChildren( s.value.filters.size(), false );
	std::vector<std::array<std::string, 4>> rowLabels( s.value.filters.size() );
	std::array<std::optional<std::size_t>, 4> ancestors;
	const auto key = []( const CatalogId& category, const CatalogId& group, const CatalogId& item, const CatalogId& material )
	{ return category.value + '\x1f' + group.value + '\x1f' + item.value + '\x1f' + material.value; };
	std::unordered_map<std::string, std::array<std::string, 4>> leafLabels;
	for ( std::size_t index = 0; index < s.value.filters.size(); ++index )
	{
		const auto depth = static_cast<std::size_t>( s.value.filters[index].id.depth );
		if ( depth > 0 && ancestors[depth - 1] )
			hasChildren[*ancestors[depth - 1]] = true;
		for ( std::size_t level = 0; level < depth; ++level )
			if ( ancestors[level] ) rowLabels[index][level] = s.value.filters[*ancestors[level]].label;
		rowLabels[index][depth] = s.value.filters[index].label;
		for ( std::size_t level = depth; level < ancestors.size(); ++level )
			ancestors[level].reset();
		bool inheritedMatch = false;
		for ( std::size_t level = 0; level < depth; ++level )
			if ( ancestors[level] )
				inheritedMatch = inheritedMatch || pathMatches[*ancestors[level]];
		pathMatches[index] = inheritedMatch || stockpileFilterRowMatchesFolded( s.value.filters[index], foldedSearch );
		ancestors[depth] = index;
	}
	for ( std::size_t index = 0; index < s.value.filters.size(); ++index )
	{
		matchingLeaves[index] = !hasChildren[index] && pathMatches[index];
		if ( !hasChildren[index] )
		{
			auto labels = rowLabels[index];
			normalizeInventoryTableLabels( labels[1], labels[2], labels[3], s.value.filters[index].id.item.value );
			leafLabels[key( s.value.filters[index].id.category, s.value.filters[index].id.group, s.value.filters[index].id.item, s.value.filters[index].id.material )] = std::move( labels );
		}
	}
	const auto columnMatches = []( const std::string& value, const std::string& filter, const auto& selected )
	{
		if ( !filter.empty() && folded( value ).find( folded( filter ) ) == std::string::npos ) return false;
		return selected.empty() || std::ranges::any_of( selected, [&]( const auto& candidate ) { return folded( candidate ) == folded( value ); } );
	};
	const auto textColumnsMatch = [&]( const auto& labels, const auto& filters, const auto& selections, std::size_t count )
	{
		for ( std::size_t column = 0; column < count; ++column )
			if ( !columnMatches( labels[column], filters[column], selections[column] ) ) return false;
		return true;
	};
	std::unordered_set<std::string> seenRules, seenContents;
	const auto status = []( TriState value ) { return value == TriState::On ? std::string( "Allowed" ) : value == TriState::Mixed ? std::string( "Mixed" ) : std::string( "Blocked" ); };
	for ( std::size_t index = 0; index < s.value.filters.size(); ++index )
	{
		const auto& row = s.value.filters[index];
		if ( !matchingLeaves[index] ) continue;
		if ( !s.filterCategory.value.empty() && row.id.category != s.filterCategory ) continue;
		const auto& labels = leafLabels.at( key( row.id.category, row.id.group, row.id.item, row.id.material ) );
		if ( !textColumnsMatch( labels, s.allowColumnFilters, s.allowColumnSelections, 4 ) ) continue;
		if ( !columnMatches( status( row.state ), s.allowColumnFilters[4], s.allowColumnSelections[4] ) ) continue;
		if ( !seenRules.insert( key( row.id.category, row.id.group, row.id.item, row.id.material ) ).second ) continue;
		s.matchingFilterLeaves.push_back( row.id );
		s.visibleFilters.push_back( row );
	}
	std::ranges::stable_sort( s.visibleFilters, [&]( const auto& left, const auto& right )
	{
		const auto& leftLabels = leafLabels.at( key( left.id.category, left.id.group, left.id.item, left.id.material ) );
		const auto& rightLabels = leafLabels.at( key( right.id.category, right.id.group, right.id.item, right.id.material ) );
		if ( s.allowSort == StockpileSortKey::Status && left.state != right.state )
			return s.allowSortDirection == SortDirection::Ascending ? left.state < right.state : left.state > right.state;
		const std::size_t column = s.allowSort == StockpileSortKey::Category ? 0 : s.allowSort == StockpileSortKey::Group ? 1 : s.allowSort == StockpileSortKey::Material ? 3 : 2;
		return s.allowSortDirection == SortDirection::Ascending ? inventoryTableSortKey( leftLabels, column ) < inventoryTableSortKey( rightLabels, column ) : inventoryTableSortKey( leftLabels, column ) > inventoryTableSortKey( rightLabels, column );
	} );
	s.visibleContents.clear();
	const auto contentPathMatches = [&]( const StockpileContentRow& content )
	{
		if ( s.contentSearch.empty() ) return true;
		for ( const auto& row : s.value.filters )
		{
			if ( row.id.category != content.id.category ) continue;
			if ( row.id.depth >= FilterDepth::Group && row.id.group != content.id.group ) continue;
			if ( row.id.depth >= FilterDepth::Item && row.id.item != content.id.item ) continue;
			if ( row.id.depth == FilterDepth::Material && row.id.material != content.id.material ) continue;
			if ( contains( row.label, s.contentSearch ) ) return true;
		}
		return contains( content.name, s.contentSearch );
	};
	for ( std::size_t index = 0; index < s.value.contents.size(); ++index )
	{
		const auto& row = s.value.contents[index];
		const auto depth = static_cast<std::size_t>( row.id.depth );
		const bool hasChild = index + 1 < s.value.contents.size()
			&& static_cast<std::size_t>( s.value.contents[index + 1].id.depth ) > depth;
		if ( hasChild ) continue;
		if ( !s.filterCategory.value.empty() && row.id.category != s.filterCategory ) continue;
		const auto found = leafLabels.find( key( row.id.category, row.id.group, row.id.item, row.id.material ) );
		if ( found == leafLabels.end() || !textColumnsMatch( found->second, s.contentColumnFilters, s.contentColumnSelections, 4 ) ) continue;
		if ( !inventoryQuantityMatches( row.stockpiled, s.contentColumnFilters[4], s.contentColumnSelections[4] ) ) continue;
		if ( !inventoryQuantityMatches( row.total, s.contentColumnFilters[5], s.contentColumnSelections[5] ) ) continue;
		if ( contentPathMatches( row ) && seenContents.insert( key( row.id.category, row.id.group, row.id.item, row.id.material ) ).second ) s.visibleContents.push_back( row );
	}
	std::ranges::stable_sort( s.visibleContents, [&]( const auto& left, const auto& right )
	{
		if ( s.contentSort == StockpileSortKey::Quantity && left.stockpiled != right.stockpiled )
			return s.sort == SortDirection::Ascending ? left.stockpiled < right.stockpiled : left.stockpiled > right.stockpiled;
		if ( s.contentSort == StockpileSortKey::Total && left.total != right.total )
			return s.sort == SortDirection::Ascending ? left.total < right.total : left.total > right.total;
		const auto& leftLabels = leafLabels.at( key( left.id.category, left.id.group, left.id.item, left.id.material ) );
		const auto& rightLabels = leafLabels.at( key( right.id.category, right.id.group, right.id.item, right.id.material ) );
		const std::size_t column = s.contentSort == StockpileSortKey::Category ? 0 : s.contentSort == StockpileSortKey::Group ? 1 : s.contentSort == StockpileSortKey::Material ? 3 : 2;
		return s.sort == SortDirection::Ascending ? inventoryTableSortKey( leftLabels, column ) < inventoryTableSortKey( rightLabels, column ) : inventoryTableSortKey( leftLabels, column ) > inventoryTableSortKey( rightLabels, column );
	} );
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
		state_.stockpile.filterSearch = trimmed( std::move( value ) );
		state_.stockpile.filterSearchRevealed = false;
		rebuildStockpile();
		notify();
	}
}
void Management6AController::setStockpileFilterCategory( CatalogId value )
{
	if ( state_.view != ManagementView::Stockpile )
		return;
	state_.stockpile.filterCategory = std::move( value );
	rebuildStockpile();
	state_.stockpile.selectedFilter = state_.stockpile.visibleFilters.empty() ? std::optional<StockpileFilterRowId> {} : std::optional<StockpileFilterRowId> { state_.stockpile.visibleFilters.front().id };
	notify();
}
void Management6AController::setStockpileContentSearch( std::string value )
{
	if ( state_.view == ManagementView::Stockpile )
	{
		state_.stockpile.contentSearch = trimmed( std::move( value ) );
		rebuildStockpile();
		notify();
	}
}
void Management6AController::setStockpileColumnFilter( bool allowList, std::size_t column, std::string value )
{
	if ( state_.view != ManagementView::Stockpile ) return;
	if ( allowList )
	{
		if ( column >= state_.stockpile.allowColumnFilters.size() ) return;
		state_.stockpile.allowColumnFilters[column] = std::move( value );
	}
	else
	{
		if ( column >= state_.stockpile.contentColumnFilters.size() ) return;
		state_.stockpile.contentColumnFilters[column] = std::move( value );
	}
	rebuildStockpile();
	notify();
}
void Management6AController::toggleStockpileColumnSelection( bool allowList, std::size_t column, std::string value )
{
	if ( state_.view != ManagementView::Stockpile ) return;
	const auto toggle = [&]( auto& columns )
	{
		if ( column >= columns.size() ) return false;
		auto& selected = columns[column];
		if ( value.empty() )
			selected.clear();
		else if ( const auto found = std::ranges::find_if( selected, [&]( const auto& candidate ) { return folded( candidate ) == folded( value ); } ); found != selected.end() )
			selected.erase( found );
		else
			{
			if ( !allowList && column >= 4 ) selected.clear();
			selected.push_back( value );
		}
		return true;
	};
	if ( !( allowList ? toggle( state_.stockpile.allowColumnSelections ) : toggle( state_.stockpile.contentColumnSelections ) ) ) return;
	rebuildStockpile();
	notify();
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
void Management6AController::setStockpileAllowSort( StockpileSortKey key )
{
	if ( state_.view != ManagementView::Stockpile ) return;
	if ( state_.stockpile.allowSort == key )
		state_.stockpile.allowSortDirection = state_.stockpile.allowSortDirection == SortDirection::Ascending ? SortDirection::Descending : SortDirection::Ascending;
	else
	{
		state_.stockpile.allowSort = key;
		state_.stockpile.allowSortDirection = SortDirection::Ascending;
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
void Management6AController::setWorkshopPane( WorkshopPane pane )
{
	state_.workshop.pane = pane;
	notify();
}
void Management6AController::setWorkshopOrderMaterial( std::size_t index, CatalogId material )
{
	if ( !state_.workshop.selectedProduct ) return;
	const auto product = findRow( state_.workshop.value.products, *state_.workshop.selectedProduct );
	if ( product == state_.workshop.value.products.end() || index >= product->components.size() ) return;
	const auto& choices = product->components[index].materials;
	if ( std::none_of( choices.begin(), choices.end(), [&]( const auto& choice ) { return choice.first == material; } ) ) return;
	normalizeWorkshopOrderDraft();
	state_.workshop.orderMaterials[index] = std::move( material );
	notify();
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
		state_.stockpile.selectedContent = id;
		const auto selected = std::find_if( state_.stockpile.value.contents.begin(), state_.stockpile.value.contents.end(), [&]( const auto& row ) { return row.id == *state_.stockpile.selectedContent; } );
		if ( selected != state_.stockpile.value.contents.end() )
		{
			state_.stockpile.filterSearchBeforeReveal = state_.stockpile.filterSearch;
			state_.stockpile.filterSearch = selected->name;
			state_.stockpile.filterSearchRevealed = true;
			state_.stockpile.pane = StockpilePane::AllowList;
		}
		rebuildStockpile();
		notify();
	}
}
void Management6AController::toggleStockpileContentExpansion( StockpileContentRowId id )
{
	if ( id.depth == FilterDepth::Material || !hasRow( state_.stockpile.value.contents, id ) ) return;
	const auto found = std::find( state_.stockpile.expandedContents.begin(), state_.stockpile.expandedContents.end(), id );
	if ( found == state_.stockpile.expandedContents.end() ) state_.stockpile.expandedContents.push_back( std::move( id ) );
	else state_.stockpile.expandedContents.erase( found );
	rebuildStockpile();
	notify();
}
void Management6AController::setStockpileTemplateName( std::string name )
{
	state_.stockpile.templateName = trimmed( std::move( name ) );
	notify();
}
void Management6AController::toggleStockpileTemplateMenu()
{
	if ( state_.stockpile.templateOverwriteConfirmationRequired ) return;
	state_.stockpile.templateMenuOpen = !state_.stockpile.templateMenuOpen;
	notify();
}
void Management6AController::selectStockpileTemplate( std::string name )
{
	name = trimmed( std::move( name ) );
	if ( name.empty() ) return;
	state_.stockpile.templateName = name;
	state_.stockpile.templateMenuOpen = false;
	applyStockpileTemplate( std::move( name ) );
}
void Management6AController::saveStockpileTemplate()
{
	if ( !state_.stockpile.value.id || state_.stockpile.templateName.empty() ) return;
	const auto requested = folded( state_.stockpile.templateName );
	const auto existing = std::ranges::find_if( state_.stockpile.value.templateNames, [&]( const auto& name ) { return folded( name ) == requested; } );
	state_.stockpile.templateMenuOpen = false;
	if ( existing != state_.stockpile.value.templateNames.end() )
	{
		state_.stockpile.pendingTemplateOverwrite = *existing;
		state_.stockpile.templateOverwriteConfirmationRequired = true;
		notify();
		return;
	}
	dispatch( "stockpile.save_template", StockpileTemplatePayload { state_.stockpile.value.id, state_.stockpile.templateName } );
}
void Management6AController::applyStockpileTemplate( std::string name )
{
	name = trimmed( std::move( name ) );
	if ( state_.stockpile.value.id && !name.empty() )
		dispatch( "stockpile.apply_template", StockpileTemplatePayload { state_.stockpile.value.id, std::move( name ) } );
}
void Management6AController::confirmStockpileTemplateOverwrite()
{
	if ( !state_.stockpile.templateOverwriteConfirmationRequired || !state_.stockpile.value.id || state_.stockpile.pendingTemplateOverwrite.empty() ) return;
	const auto name = state_.stockpile.pendingTemplateOverwrite;
	if ( dispatch( "stockpile.save_template", StockpileTemplatePayload { state_.stockpile.value.id, name }, DispatchOrigin::DestructiveConfirmation ) )
	{
		state_.stockpile.templateOverwriteConfirmationRequired = false;
		state_.stockpile.pendingTemplateOverwrite.clear();
		notify();
	}
}
void Management6AController::cancelStockpileTemplateOverwrite()
{
	if ( !state_.stockpile.templateOverwriteConfirmationRequired ) return;
	state_.stockpile.templateOverwriteConfirmationRequired = false;
	state_.stockpile.pendingTemplateOverwrite.clear();
	state_.status.clear();
	notify();
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
	if ( ( state_.workshop.tradeConfirmationRequired || state_.stockpile.templateOverwriteConfirmationRequired ) && origin != DispatchOrigin::DestructiveConfirmation )
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
void Management6AController::setWorkshopStockpileLink(StockpileId stockpile, bool linked)
{
    if(state_.workshop.value.id && stockpile)
        dispatch("workshop.set_stockpile_link",SetWorkshopStockpileLinkPayload{state_.workshop.value.id,stockpile,linked});
}
void Management6AController::onWorkshopOrderResult(WorkshopId workshop, bool accepted)
{
    if(workshop!=state_.workshop.value.id) return;
    state_.workshop.orderPending=false;
    state_.workshop.orderFeedback=accepted ? "Order added to queue." : "Could not add order. Check the recipe and materials.";
    state_.pendingAction.reset();
    state_.status.clear();
    notify();
}
void Management6AController::queueSelectedCraft( CraftRepeatMode mode, std::uint32_t count, std::vector<CatalogId> materials )
{
    if(state_.workshop.orderPending) return;
    if(state_.workshop.value.id && state_.workshop.selectedProduct && count>0) {
        if(dispatch("workshop.queue_craft",QueueCraftPayload{state_.workshop.value.id,*state_.workshop.selectedProduct,mode,count,std::move(materials)})) {
            state_.workshop.orderPending=true;
            state_.workshop.orderFeedback="Adding order...";
        } else state_.workshop.orderFeedback="Could not add order.";
        notify();
    }
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
	std::vector<StockpileFilterRowId> matches;
	matches = state_.stockpile.matchingFilterLeaves;
	if ( !matches.empty() )
		dispatch( "stockpile.set_filters", SetStockpileFiltersPayload { state_.stockpile.value.id, std::move( matches ), active } );
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

void Management6AController::toggleFarmPlot( WorldPosition plot )
{
	auto& s = state_.agriculture;
	if ( s.value.target.kind != AgricultureKind::Farm ||
		!std::ranges::any_of( s.value.fields, [&]( const auto& field ) { return field.position == plot; } ) ) return;
	auto selected = std::find( s.selectedPlots.begin(), s.selectedPlots.end(), plot );
	if ( selected == s.selectedPlots.end() ) s.selectedPlots.push_back( plot );
	else s.selectedPlots.erase( selected );
	notify();
}

void Management6AController::selectAllFarmPlots()
{
	auto& s = state_.agriculture;
	if ( s.value.target.kind != AgricultureKind::Farm ) return;
	s.selectedPlots.clear();
	for ( const auto& field : s.value.fields ) s.selectedPlots.push_back( field.position );
	notify();
}

void Management6AController::clearFarmPlotSelection()
{
	state_.agriculture.selectedPlots.clear();
	notify();
}

void Management6AController::assignSelectedFarmPlotCrop()
{
	const auto& s = state_.agriculture;
	if ( s.value.target.kind == AgricultureKind::Farm && s.selectedProduct && !s.selectedPlots.empty() )
		dispatch( "agriculture.set_plot_crop", SetFarmPlotCropPayload { s.value.target.designation, s.selectedPlots, *s.selectedProduct } );
}

void Management6AController::useFarmDefaultForSelectedPlots()
{
	const auto& s = state_.agriculture;
	if ( s.value.target.kind == AgricultureKind::Farm && !s.selectedPlots.empty() )
		dispatch( "agriculture.set_plot_crop", SetFarmPlotCropPayload { s.value.target.designation, s.selectedPlots, {} } );
}

void Management6AController::queueSelectedFarmPlotCrop( std::uint32_t count, bool repeat )
{
	const auto& s = state_.agriculture;
	if ( s.value.target.kind == AgricultureKind::Farm && s.selectedProduct && !s.selectedPlots.empty() && count > 0 )
		dispatch( "agriculture.queue_plot_crop", QueueFarmPlotCropPayload { s.value.target.designation, s.selectedPlots, *s.selectedProduct, count, repeat } );
}

void Management6AController::cancelFarmPlotOrder( WorldPosition plot, std::uint32_t order )
{
	const auto& s = state_.agriculture;
	if ( s.value.target.kind == AgricultureKind::Farm )
		dispatch( "agriculture.cancel_plot_order", FarmPlotOrderPayload { s.value.target.designation, plot, order } );
}

void Management6AController::moveFarmPlotOrder( WorldPosition plot, std::uint32_t order, MoveDirection direction )
{
	const auto& s = state_.agriculture;
	if ( s.value.target.kind == AgricultureKind::Farm )
		dispatch( "agriculture.move_plot_order", MoveFarmPlotOrderPayload { s.value.target.designation, plot, order, direction } );
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
