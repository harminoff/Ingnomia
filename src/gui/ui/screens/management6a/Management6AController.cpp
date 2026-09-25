/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6AController.h"
#include <charconv>
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
    stockpileDrafts_.clear();workshopDrafts_.clear();agricultureDrafts_.clear();
	state_.world               = world;
	state_.acceptsWorldActions = static_cast<bool>( world );
	notify();
}
void Management6AController::endWorld()
{
    stockpileDrafts_.clear();workshopDrafts_.clear();agricultureDrafts_.clear();
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

namespace
{
StockpileDraft freshStockpileDraft( const StockpileSnapshot& value )
{
	const auto options = stockpileOptionsOf( value );
	return StockpileDraft { value.name, std::to_string( value.priority + 1 ), value.name, value.priority, false, false, options, options, {} };
}
// Drops rule changes the game already shows (applied here or elsewhere) and rules that no longer exist.
void settleStockpileRules( StockpileDraft& draft, const StockpileSnapshot& value )
{
	if ( draft.rules.empty() ) return;
	std::map<std::string, bool> current;
	for ( const auto& row : value.filters )
		if ( row.id.depth == FilterDepth::Material ) current[stockpileRuleKey( row.id )] = row.state == TriState::On;
	for ( auto it = draft.rules.begin(); it != draft.rules.end(); )
	{
		const auto found = current.find( it->first );
		if ( found == current.end() || found->second == it->second.allowed ) it = draft.rules.erase( it );
		else ++it;
	}
}
bool stockpileDraftDiffers( const StockpileDraft& draft, const StockpileSnapshot& value )
{
	return draft.name != value.name || draft.priority != std::to_string( value.priority + 1 ) || draft.options != stockpileOptionsOf( value ) || !draft.rules.empty();
}
// A field conflicts when the game's value is neither the applied draft nor the value it replaced.
bool stockpileDraftConflicts( const StockpileDraft& draft, const StockpileSnapshot& value )
{
	const auto now = stockpileOptionsOf( value );
	const auto bad = [] ( auto current, auto wanted, auto base ) { return current != wanted && current != base; };
	return bad( value.name, draft.name, draft.baseName ) || bad( std::to_string( value.priority + 1 ), draft.priority, std::to_string( draft.basePriority + 1 ) )
		|| bad( now.suspended, draft.options.suspended, draft.baseOptions.suspended ) || bad( now.pull, draft.options.pull, draft.baseOptions.pull )
		|| bad( now.allowPull, draft.options.allowPull, draft.baseOptions.allowPull );
}
WorkshopDraft freshWorkshopDraft( const WorkshopSnapshot& value )
{
	const auto options = workshopOptionsOf( value );
	return WorkshopDraft { value.name, std::to_string( value.priority + 1 ), value.name, value.priority, false, false, options, options };
}
// Links to stockpiles missing from the snapshot (deleted) cannot be applied; ignore them.
std::vector<std::uint32_t> presentLinks( const std::vector<std::uint32_t>& links, const WorkshopSnapshot& value )
{
	std::vector<std::uint32_t> out;
	for ( auto id : links )
		if ( std::any_of( value.stockpiles.begin(), value.stockpiles.end(), [&]( const auto& row ) { return row.id.value == id; } ) ) out.push_back( id );
	return out;
}
bool workshopDraftDiffers( const WorkshopDraft& draft, const WorkshopSnapshot& value )
{
	auto wanted   = draft.options;
	wanted.linked = presentLinks( wanted.linked, value );
	return draft.name != value.name || draft.priority != std::to_string( value.priority + 1 ) || wanted != workshopOptionsOf( value );
}
// A field conflicts when the authoritative value is neither the applied draft nor the value it replaced.
bool workshopDraftConflicts( const WorkshopDraft& draft, const WorkshopSnapshot& value )
{
	const auto now  = workshopOptionsOf( value );
	const auto& a   = draft.options;
	const auto& b   = draft.baseOptions;
	const auto bad  = [] ( auto current, auto wanted, auto base ) { return current != wanted && current != base; };
	if ( bad( value.name, draft.name, draft.baseName ) || bad( std::to_string( value.priority + 1 ), draft.priority, std::to_string( draft.basePriority + 1 ) ) ) return true;
	if ( bad( now.suspended, a.suspended, b.suspended ) || bad( now.acceptGenerated, a.acceptGenerated, b.acceptGenerated ) || bad( now.autoCraftMissing, a.autoCraftMissing, b.autoCraftMissing )
		|| bad( now.butcherCorpses, a.butcherCorpses, b.butcherCorpses ) || bad( now.butcherExcess, a.butcherExcess, b.butcherExcess )
		|| bad( now.catchFish, a.catchFish, b.catchFish ) || bad( now.processFish, a.processFish, b.processFish ) ) return true;
	for ( const auto& row : value.stockpiles )
	{
		const auto in = []( const auto& list, auto id ) { return std::find( list.begin(), list.end(), id ) != list.end(); };
		if ( bad( row.linked, in( a.linked, row.id.value ), in( b.linked, row.id.value ) ) ) return true;
	}
	return false;
}

AgricultureDraft freshAgricultureDraft( const AgricultureSnapshot& value )
{
	const auto options = agricultureOptionsOf( value );
	return AgricultureDraft { value.name, value.name, options, options, false, false };
}
// Butchering marks and food rules for records missing from the snapshot cannot be applied.
AgricultureOptions presentAgricultureOptions( AgricultureOptions o, const AgricultureSnapshot& value )
{
	std::erase_if( o.butcher, [&]( auto id ) { return std::none_of( value.animals.begin(), value.animals.end(), [&]( const auto& a ) { return a.id.value == id; } ); } );
	std::erase_if( o.foods, [&]( const auto& key ) { return std::none_of( value.foods.begin(), value.foods.end(), [&]( const auto& f ) { return pastureFoodKey( f.item, f.material ) == key; } ); } );
	return o;
}
bool agricultureDraftDiffers( const AgricultureDraft& draft, const AgricultureSnapshot& value )
{
	return draft.name != value.name || presentAgricultureOptions( draft.options, value ) != agricultureOptionsOf( value );
}
// A field conflicts when the authoritative value is neither the applied draft nor the value it replaced.
bool agricultureDraftConflicts( const AgricultureDraft& draft, const AgricultureSnapshot& value )
{
	const auto now = agricultureOptionsOf( value );
	const auto& a  = draft.options;
	const auto& b  = draft.baseOptions;
	const auto bad = []( const auto& current, const auto& wanted, const auto& base ) { return current != wanted && current != base; };
	if ( bad( value.name, draft.name, draft.baseName ) || bad( now.suspended, a.suspended, b.suspended ) || bad( now.harvest, a.harvest, b.harvest )
		|| bad( now.harvestHay, a.harvestHay, b.harvestHay ) || bad( now.tame, a.tame, b.tame ) || bad( now.pick, a.pick, b.pick ) || bad( now.plant, a.plant, b.plant )
		|| bad( now.fell, a.fell, b.fell ) || bad( now.product, a.product, b.product ) ) return true;
	// A type change resets type-dependent pasture values in the game; those are not conflicts.
	const bool typeChanged = value.target.kind == AgricultureKind::Pasture && a.product != b.product;
	if ( typeChanged ) return false;
	if ( bad( now.maxMale, a.maxMale, b.maxMale ) || bad( now.maxFemale, a.maxFemale, b.maxFemale ) ) return true;
	const auto in = []( const auto& list, const auto& id ) { return std::find( list.begin(), list.end(), id ) != list.end(); };
	for ( const auto& row : value.animals )
		if ( bad( row.butcher, in( a.butcher, row.id.value ), in( b.butcher, row.id.value ) ) ) return true;
	for ( const auto& row : value.foods )
	{
		const auto key = pastureFoodKey( row.item, row.material );
		if ( bad( row.allowed, in( a.foods, key ), in( b.foods, key ) ) ) return true;
	}
	return false;
}
std::pair<int, std::uint32_t> agricultureKey( const AgricultureTarget& t ) { return { static_cast<int>( t.kind ), t.designation.value }; }
// Live updates refresh every field the user has not changed; the user's own edits stay pending and keep
// the value they replaced, so Apply can still detect a conflicting change made elsewhere.
void rebaseAgricultureDraft( AgricultureDraft& d, const AgricultureSnapshot& value )
{
	const auto now = agricultureOptionsOf( value );
	auto& o = d.options;
	auto& b = d.baseOptions;
	const auto field = [&]( auto member ) {
		if ( o.*member == b.*member ) o.*member = now.*member;
		if ( o.*member == now.*member || b.*member == o.*member ) b.*member = now.*member;
	};
	if ( d.name == d.baseName ) d.name = value.name;
	if ( d.name == value.name ) d.baseName = value.name;
	field( &AgricultureOptions::suspended );
	field( &AgricultureOptions::harvest );
	field( &AgricultureOptions::harvestHay );
	field( &AgricultureOptions::tame );
	field( &AgricultureOptions::pick );
	field( &AgricultureOptions::plant );
	field( &AgricultureOptions::fell );
	field( &AgricultureOptions::product );
	field( &AgricultureOptions::maxMale );
	field( &AgricultureOptions::maxFemale );
	const auto members = []( auto& draft, auto& base, const auto& current, const auto& ids ) {
		std::decay_t<decltype( draft )> nd, nb;
		const auto in = []( const auto& list, const auto& id ) { return std::find( list.begin(), list.end(), id ) != list.end(); };
		for ( const auto& id : ids )
		{
			const bool changed = in( draft, id ) != in( base, id );
			if ( changed ? in( draft, id ) : in( current, id ) ) nd.push_back( id );
			if ( changed ? in( base, id ) : in( current, id ) ) nb.push_back( id );
		}
		std::sort( nd.begin(), nd.end() );
		std::sort( nb.begin(), nb.end() );
		draft = std::move( nd );
		base  = std::move( nb );
	};
	std::vector<std::uint32_t> animals;
	for ( const auto& a : value.animals ) animals.push_back( a.id.value );
	std::vector<std::string> foods;
	for ( const auto& f : value.foods ) foods.push_back( pastureFoodKey( f.item, f.material ) );
	members( o.butcher, b.butcher, now.butcher, animals );
	members( o.foods, b.foods, now.foods, foods );
	d.dirty = agricultureDraftDiffers( d, value );
}
}

void Management6AController::showWorkshop( WorkshopSnapshot value, Revision revision, std::optional<WorldPosition> position )
{
	if ( !state_.acceptsWorldActions || !value.id || ( state_.workshop.value.id == value.id && revision.value <= state_.workshop.revision.value ) )
		return;
	const bool sameWorkshop = state_.workshop.value.id == value.id;
 auto& ws=state_.workshop;
 if(!sameWorkshop) {
  if(ws.value.id) workshopDrafts_[ws.value.id.value]=ws.draft;
  ws.draft=workshopDrafts_.contains(value.id.value)?workshopDrafts_[value.id.value]:WorkshopDraft{};
  ws.traderRows.clear();ws.playerRows.clear();ws.selectedTradeRow.reset();ws.tradeLoaded=false;ws.tradePending=false;ws.traderId=0;ws.tradeRevision=0;ws.feedback.clear();
 }
 if(ws.draft.pending && !workshopDraftDiffers(ws.draft,value)) ws.draft.dirty=ws.draft.pending=false;
 else if(ws.draft.pending && workshopDraftConflicts(ws.draft,value)) {ws.draft.pending=false;ws.feedback="The workshop returned different values. Review the changes or press Cancel.";}
 if(!ws.draft.dirty) ws.draft=freshWorkshopDraft(value);
	if ( !sameWorkshop )
	{
		state_.workshop.tradeConfirmationRequired = false;
		state_.workshop.orderPending=false;
		state_.workshop.orderFeedback.clear();
		state_.workshop.search.clear();
		state_.workshop.pane = workshopSupportsTrade( value ) ? WorkshopPane::Trade : !value.products.empty() ? WorkshopPane::Craft : WorkshopPane::Settings;
	}
	else if ( ( ( state_.workshop.pane == WorkshopPane::Craft || state_.workshop.pane == WorkshopPane::Queue ) && !workshopSupportsCrafting( value ) )
		|| ( state_.workshop.pane == WorkshopPane::Stockpiles && !workshopSupportsStockpileLinks( value ) ) )
		state_.workshop.pane = WorkshopPane::Settings;
	const auto previousProduct      = state_.workshop.selectedProduct;
	const auto product              = state_.workshop.value.id == value.id ? state_.workshop.selectedProduct : std::optional<CatalogId> {};
	const auto job                  = state_.workshop.value.id == value.id ? state_.workshop.selectedJob : std::optional<CraftJobId> {};
	state_.view                     = ManagementView::Workshop;
	state_.workshop.value           = std::move( value );
	state_.workshop.revision        = revision;
	state_.workshop.position        = position;
	state_.workshop.request         = { RequestStatus::Ready, {}, {}, false };
	state_.workshop.selectedProduct = product && hasRow( state_.workshop.value.products, *product ) ? product : ( state_.workshop.value.products.empty() ? std::optional<CatalogId> {} : std::optional<CatalogId> { state_.workshop.value.products.front().id } );
	state_.workshop.selectedJob     = job && hasRow( state_.workshop.value.queue, *job ) ? job : std::optional<CraftJobId> {};
	if ( !sameWorkshop || previousProduct != state_.workshop.selectedProduct )
		resetWorkshopOrderDraft();
	else
		normalizeWorkshopOrderDraft();
	state_.pendingAction.reset();
	state_.status.clear();
	rebuildWorkshop();
	if ( !sameWorkshop && state_.workshop.pane == WorkshopPane::Trade && !state_.workshop.tradeLoaded )
		refreshTrade();
	notify();
}
void Management6AController::showStockpile( StockpileSnapshot value, Revision revision, std::optional<WorldPosition> position )
{
	if ( !state_.acceptsWorldActions || !value.id || ( state_.stockpile.value.id == value.id && revision.value <= state_.stockpile.revision.value ) )
		return;
	const bool same                  = state_.stockpile.value.id == value.id;
    auto& sp = state_.stockpile;
    if(!same && sp.value.id) stockpileDrafts_[sp.value.id.value] = {sp.draft,sp.templateName};
    if(!same) {
        auto found=stockpileDrafts_.find(value.id.value);
        sp.draft=found==stockpileDrafts_.end()?StockpileDraft{}:found->second.first;
    }
    settleStockpileRules(sp.draft,value);
    if(sp.draft.pending && !stockpileDraftDiffers(sp.draft,value)) sp.draft.dirty=sp.draft.pending=false;
    else if(sp.draft.pending && stockpileDraftConflicts(sp.draft,value)) {sp.draft.pending=false;sp.feedback="The stockpile returned different values. Review the changes or press Cancel.";}
    if(!sp.draft.dirty) sp.draft=freshStockpileDraft(value);
    else sp.draft.dirty=stockpileDraftDiffers(sp.draft,value);

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
	state_.stockpile.request         = { RequestStatus::Ready, {}, {}, false };
	const auto firstListRow = std::find_if( state_.stockpile.value.filters.begin(), state_.stockpile.value.filters.end(), []( const auto& row ) { return row.id.depth != FilterDepth::Category; } );
	state_.stockpile.selectedFilter  = filter && hasRow( state_.stockpile.value.filters, *filter ) && filter->depth != FilterDepth::Category ? filter : ( firstListRow == state_.stockpile.value.filters.end() ? std::optional<StockpileFilterRowId> {} : std::optional<StockpileFilterRowId> { firstListRow->id } );
	state_.stockpile.selectedContent = content && hasRow( state_.stockpile.value.contents, *content ) ? content : ( state_.stockpile.value.contents.empty() ? std::optional<StockpileContentRowId> {} : std::optional<StockpileContentRowId> { state_.stockpile.value.contents.front().id } );
	if ( !same )
	{
		state_.stockpile.search.clear();
			state_.stockpile.filterSearch.clear();
			state_.stockpile.contentSearch.clear();
		state_.stockpile.filterSearchBeforeReveal.clear();
        const auto retained=stockpileDrafts_.find(sp.value.id.value);
        sp.templateName=retained==stockpileDrafts_.end()?std::string{}:retained->second.second;
        sp.feedback.clear();

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
	auto& ag                           = state_.agriculture;
	if ( !same )
	{
		if ( ag.value.target.designation ) agricultureDrafts_[agricultureKey( ag.value.target )] = ag.draft;
		const auto saved = agricultureDrafts_.find( agricultureKey( value.target ) );
		ag.draft         = saved != agricultureDrafts_.end() ? saved->second : AgricultureDraft {};
		ag.feedback.clear();
		ag.pane = value.target.kind == AgricultureKind::Farm ? AgriculturePane::Plots : AgriculturePane::General;
		ag.plotAnchor.reset();
		ag.focusedPlot.reset();
		ag.selectedFood.reset();
	}
	if ( ag.draft.dirty && !ag.draft.pending ) rebaseAgricultureDraft( ag.draft, value );
	// A confirmed pasture type change brings that type's own limits, food rules and (empty) roster.
	if ( value.target.kind == AgricultureKind::Pasture && ag.draft.options.product != ag.draft.baseOptions.product && value.product == ag.draft.options.product )
	{
		const auto now = agricultureOptionsOf( value );
		for ( auto* o : { &ag.draft.options, &ag.draft.baseOptions } )
		{
			o->maxMale   = now.maxMale;
			o->maxFemale = now.maxFemale;
			o->foods     = now.foods;
			o->butcher   = now.butcher;
		}
		ag.draft.baseOptions.product = value.product;
		ag.draft.dirty               = agricultureDraftDiffers( ag.draft, value );
	}
	if ( ag.draft.pending && !agricultureDraftDiffers( ag.draft, value ) ) ag.draft.dirty = ag.draft.pending = false;
	else if ( ag.draft.pending && agricultureDraftConflicts( ag.draft, value ) )
	{
		ag.draft.pending = false;
		ag.feedback      = "This designation returned different values. Review the changes or press Cancel.";
	}

	if ( !ag.draft.dirty ) ag.draft = freshAgricultureDraft( value );
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
	// A record that disappeared is never replaced by whatever now occupies its row.
	state_.agriculture.selectedAnimal  = animal && hasRow( state_.agriculture.value.animals, *animal ) ? animal : std::optional<CreatureId> {};
	const auto hasPlot                 = [&]( const std::optional<WorldPosition>& p ) { return p && std::ranges::any_of( ag.value.fields, [&]( const auto& f ) { return f.position == *p; } ); };
	if ( !hasPlot( ag.plotAnchor ) ) ag.plotAnchor.reset();
	if ( !hasPlot( ag.focusedPlot ) ) ag.focusedPlot.reset();
	if ( ag.selectedFood && std::none_of( ag.value.foods.begin(), ag.value.foods.end(), [&]( const auto& f ) { return pastureFoodKey( f.item, f.material ) == *ag.selectedFood; } ) ) ag.selectedFood.reset();
	if ( !agricultureSupportsPane( ag.value.target.kind, ag.pane ) ) ag.pane = AgriculturePane::General;
	state_.pendingAction.reset();
	state_.status.clear();
	rebuildAgriculture();
	notify();
}
void Management6AController::setAgriculturePane( AgriculturePane pane )
{
	if ( state_.view != ManagementView::Agriculture || state_.agriculture.pane == pane || !agricultureSupportsPane( state_.agriculture.value.target.kind, pane ) )
		return;
	state_.agriculture.pane = pane;
	notify();
}
void Management6AController::selectFarmPlot( WorldPosition plot, PlotSelect mode )
{
	auto& s = state_.agriculture;
	const auto has = [&]( const WorldPosition& p ) { return std::ranges::any_of( s.value.fields, [&]( const auto& f ) { return f.position == p; } ); };
	if ( s.value.target.kind != AgricultureKind::Farm || !has( plot ) ) return;
	s.focusedPlot = plot;
	if ( mode == PlotSelect::Range && s.plotAnchor && s.plotAnchor->z == plot.z )
	{
		// Shift+click selects the rectangle between the anchor and this plot (list view / Excel 97 range model).
		const auto a = *s.plotAnchor;
		s.selectedPlots.clear();
		for ( const auto& f : s.value.fields )
			if ( f.position.z == plot.z && f.position.x >= std::min( a.x, plot.x ) && f.position.x <= std::max( a.x, plot.x ) && f.position.y >= std::min( a.y, plot.y ) && f.position.y <= std::max( a.y, plot.y ) )
				s.selectedPlots.push_back( f.position );
	}
	else if ( mode == PlotSelect::Toggle )
	{
		auto found = std::find( s.selectedPlots.begin(), s.selectedPlots.end(), plot );
		if ( found == s.selectedPlots.end() ) s.selectedPlots.push_back( plot );
		else s.selectedPlots.erase( found );
		s.plotAnchor = plot;
	}
	else
	{
		s.selectedPlots = { plot };
		s.plotAnchor    = plot;
	}
	notify();
}
void Management6AController::moveFarmPlotFocus( std::int32_t dx, std::int32_t dy, bool extend )
{
	auto& s = state_.agriculture;
	if ( s.value.target.kind != AgricultureKind::Farm || s.value.fields.empty() ) return;
	if ( !s.focusedPlot )
	{
		s.focusedPlot = s.value.fields.front().position;
		notify();
		return;
	}
	// Move to the nearest plot in the pressed direction on the same level; gaps in the field are skipped.
	const auto from         = *s.focusedPlot;
	const FarmPlotRow* best = nullptr;
	int bestScore           = 0;
	for ( const auto& f : s.value.fields )
	{
		const auto& p = f.position;
		if ( p.z != from.z ) continue;
		const int along  = dx ? ( p.x - from.x ) * dx : ( p.y - from.y ) * dy;
		const int across = dx ? std::abs( p.y - from.y ) : std::abs( p.x - from.x );
		if ( along <= 0 ) continue;
		const int score = along + across * 1000;
		if ( !best || score < bestScore )
		{
			best      = &f;
			bestScore = score;
		}
	}
	if ( !best ) return;
	if ( extend ) selectFarmPlot( best->position, PlotSelect::Range );
	else
	{
		s.focusedPlot = best->position;
		notify();
	}
}
void Management6AController::selectPastureFood( std::string key )
{
	auto& s = state_.agriculture;
	if ( std::none_of( s.value.foods.begin(), s.value.foods.end(), [&]( const auto& f ) { return pastureFoodKey( f.item, f.material ) == key; } ) ) return;
	s.selectedFood = std::move( key );
	notify();
}
void Management6AController::editAgricultureName( std::string name )
{
	auto& s = state_.agriculture;
	if ( s.draft.pending || !s.value.target.designation ) return;
	s.draft.name  = std::move( name );
	s.draft.dirty = agricultureDraftDiffers( s.draft, s.value );
	notify();
}
void Management6AController::editAgricultureOptions( AgricultureOptions options )
{
	auto& s = state_.agriculture;
	if ( s.draft.pending || !s.value.target.designation ) return;
	std::sort( options.butcher.begin(), options.butcher.end() );
	options.butcher.erase( std::unique( options.butcher.begin(), options.butcher.end() ), options.butcher.end() );
	std::sort( options.foods.begin(), options.foods.end() );
	options.foods.erase( std::unique( options.foods.begin(), options.foods.end() ), options.foods.end() );
	options.maxMale   = std::max( 0, options.maxMale );
	options.maxFemale = std::max( 0, options.maxFemale );
	if ( !options.product.value.empty() && !hasRow( s.value.catalog, options.product ) ) options.product = s.draft.options.product;
	// The game keeps pasture limits and food rules per animal type and resets them when the type changes,
	// so a pending type change carries no limit or food edits; they are set after the type is applied.
	if ( s.value.target.kind == AgricultureKind::Pasture && options.product != s.value.product )
	{
		const auto current = agricultureOptionsOf( s.value );
		options.maxMale    = current.maxMale;
		options.maxFemale  = current.maxFemale;
		options.foods      = current.foods;
	}
	s.draft.options = std::move( options );
	s.draft.dirty   = agricultureDraftDiffers( s.draft, s.value );
	notify();
}
void Management6AController::revertAgricultureDraft()
{
	auto& s = state_.agriculture;
	if ( s.draft.pending ) return;
	s.draft = freshAgricultureDraft( s.value );
	s.feedback.clear();
	notify();
}
void Management6AController::agricultureFeedback( std::string text )
{
	state_.agriculture.feedback = std::move( text );
	notify();
}
bool Management6AController::applyAgricultureDraft()
{
	auto& s = state_.agriculture;
	if ( !s.value.target.designation || s.draft.pending || state_.pendingAction ) return false;
	if ( !s.draft.dirty ) return true;
	if ( trimmed( s.draft.name ).empty() )
	{
		agricultureFeedback( "Enter a name for this designation." );
		return false;
	}
	const auto current = agricultureOptionsOf( s.value );
	if ( s.draft.baseName != s.value.name || s.draft.baseOptions != current )
	{
		agricultureFeedback( "This designation was changed elsewhere. Press Cancel to see the latest values, then make your changes again." );
		return false;
	}
	s.draft.name  = trimmed( s.draft.name );
	const auto o  = presentAgricultureOptions( s.draft.options, s.value );
	const auto& t = s.value.target;
	bool sent = false, ok = true;
	const auto send = [&]( std::string_view id, UiActionPayload payload ) {
		if ( !ok ) return;
		sent = true;
		ok   = dispatch( id, std::move( payload ) );
	};
	// Priority is not implemented by FarmingManager, so the authoritative value is passed through unchanged.
	if ( s.draft.name != s.value.name || o.suspended != current.suspended )
		send( "agriculture.set_basics", SetAgricultureBasicsPayload { t, s.draft.name, s.value.priority, o.suspended } );
	if ( t.kind != AgricultureKind::Grove && ( o.harvest != current.harvest || o.harvestHay != current.harvestHay || o.tame != current.tame ) )
		send( "agriculture.set_harvest_options", SetHarvestOptionsPayload { t, o.harvest, o.harvestHay, o.tame } );
	if ( t.kind == AgricultureKind::Grove && ( o.pick != current.pick || o.plant != current.plant || o.fell != current.fell ) )
		send( "agriculture.set_grove_options", SetGroveOptionsPayload { t.designation, o.pick, o.plant, o.fell } );
	if ( o.product != current.product && !o.product.value.empty() )
		send( "agriculture.select_product", SetAgricultureProductPayload { t, o.product } );
	if ( t.kind == AgricultureKind::Pasture )
	{
		if ( o.maxMale != current.maxMale ) send( "agriculture.set_population_caps", SetPastureCapPayload { t.designation, Gender::Male, static_cast<std::uint32_t>( o.maxMale ) } );
		if ( o.maxFemale != current.maxFemale ) send( "agriculture.set_population_caps", SetPastureCapPayload { t.designation, Gender::Female, static_cast<std::uint32_t>( o.maxFemale ) } );
		for ( const auto& a : s.value.animals )
		{
			const bool wanted = std::binary_search( o.butcher.begin(), o.butcher.end(), a.id.value );
			if ( wanted != a.butcher ) send( "agriculture.set_butchering", SetButcheringPayload { a.id, wanted } );
		}
		for ( const auto& f : s.value.foods )
		{
			const bool wanted = std::binary_search( o.foods.begin(), o.foods.end(), pastureFoodKey( f.item, f.material ) );
			if ( wanted != f.allowed ) send( "agriculture.set_food_allowed", SetPastureFoodPayload { t.designation, f.item, f.material, wanted } );
		}
	}
	if ( !sent )
	{
		s.draft = freshAgricultureDraft( s.value );
		notify();
		return true;
	}
	// Most agriculture setters do not publish a new snapshot, so ask for one after the queued changes.
	if ( ok ) send( "agriculture.refresh", AgricultureTargetPayload { t } );
	s.draft.pending = ok;
	if ( !ok ) s.feedback = "The change could not be applied (" + state_.status + ").";
	notify();
	return ok;
}
void Management6AController::setTradeSnapshot(WorkshopId id,std::uint32_t trader,std::uint64_t revision,std::vector<TradeRow> seller,std::vector<TradeRow> buyer,int sellerValue,int buyerValue)
{
 auto& s=state_.workshop;if(s.value.id!=id || !state_.acceptsWorldActions || revision<=s.tradeRevision)return;
 s.tradeRevision=revision;s.traderId=trader;s.tradePending=false;state_.pendingAction.reset();
 if(s.tradeConfirmationRequired) {s.tradeConfirmationRequired=false;s.feedback="Trade offers changed. Review the current offers again.";}
 s.traderOfferValue=sellerValue;s.playerOfferValue=buyerValue;
 setTradeRows(TradeParty::Trader,std::move(seller));setTradeRows(TradeParty::Player,std::move(buyer));
 s.tradeLoaded=trader!=0;notify();
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
	if(pane==WorkshopPane::Trade && !workshopSupportsTrade(state_.workshop.value)) return;
	if((pane==WorkshopPane::Craft || pane==WorkshopPane::Queue) && !workshopSupportsCrafting(state_.workshop.value)) return;
	if(pane==WorkshopPane::Stockpiles && !workshopSupportsStockpileLinks(state_.workshop.value)) return;
 state_.workshop.pane = pane;
	// The ledger is loaded on first view; Refresh trade stays available for a later re-read.
	if(pane==WorkshopPane::Trade && !state_.workshop.tradeLoaded) refreshTrade();
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
	state_.stockpile.templateName = std::move( name );
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
void Management6AController::rejectStockpile(StockpileId id,std::string message)
{
    if(state_.stockpile.value.id!=id)return;
    state_.stockpile.draft.pending=false;state_.pendingAction.reset();
    if(message=="This stockpile no longer exists.") state_.stockpile.request={RequestStatus::Error,{},message,false};
    state_.stockpile.feedback=std::move(message);notify();
}
void Management6AController::stockpileFeedback(std::string message)
{
    state_.stockpile.feedback=std::move(message); notify();
}
void Management6AController::saveStockpileTemplate()
{
    auto& s=state_.stockpile;
    s.templateName=trimmed(s.templateName);
    if(!s.value.id || s.templateName.empty()) {stockpileFeedback("Enter a template name.");return;}
    if(std::ranges::any_of(s.value.templateNames,[&](const auto& n){return folded(n)==folded(s.templateName);})) {
        stockpileFeedback("A template named '"+s.templateName+"' already exists. Type a different name.");return;
    }
    dispatch("stockpile.save_template",StockpileTemplatePayload{s.value.id,s.templateName});
}
void Management6AController::updateStockpileTemplate()
{
    auto& s=state_.stockpile;
    const auto found=std::ranges::find_if(s.value.templateNames,[&](const auto& n){return folded(n)==folded(s.templateName);});
    if(found==s.value.templateNames.end()) {stockpileFeedback("Choose an existing template to update.");return;}
    s.pendingTemplateOverwrite=*found; s.templateOverwriteConfirmationRequired=true;s.templateMenuOpen=false;
    templateReviewId_=s.value.id;templateReviewRevision_=s.revision;notify();
}
void Management6AController::editStockpileDraft(std::string name,std::string priority)
{
    auto& s=state_.stockpile;
    if(s.draft.pending) return;
    s.draft.name=std::move(name);s.draft.priority=std::move(priority);
    s.draft.dirty=stockpileDraftDiffers(s.draft,s.value);
    notify();
}
void Management6AController::editStockpileOptions(StockpileOptions options)
{
    auto& s=state_.stockpile;
    if(s.draft.pending || !s.value.id) return;
    s.draft.options=options;
    s.draft.dirty=stockpileDraftDiffers(s.draft,s.value);
    notify();
}
void Management6AController::revertStockpileDraft()
{
    auto& s=state_.stockpile;if(s.draft.pending)return;
    s.draft=freshStockpileDraft(s.value);
    s.feedback.clear();notify();
}
bool Management6AController::applyStockpileDraft()
{
    auto& s=state_.stockpile;
    if(!s.value.id || s.draft.pending || state_.pendingAction)return false;
    if(!s.draft.dirty)return true;
    int priority=0;const auto& raw=s.draft.priority;auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),priority);
    if(trimmed(s.draft.name).empty() || parsed.ec!=std::errc{} || parsed.ptr!=raw.data()+raw.size() || priority<1 || priority>std::max(1,s.value.maxPriority)) {
        stockpileFeedback("Enter a name and a whole priority from 1 to "+std::to_string(std::max(1,s.value.maxPriority))+".");return false;
    }
    if(s.draft.baseName!=s.value.name || s.draft.basePriority!=s.value.priority || s.draft.baseOptions!=stockpileOptionsOf(s.value)) {
        stockpileFeedback("This stockpile was changed elsewhere. Press Cancel to see the latest values, then make your changes again.");return false;
    }
    s.draft.name=trimmed(s.draft.name);
    const auto& o=s.draft.options;
    bool sent=false,ok=true;
    const auto send=[&](std::string_view id,UiActionPayload payload){if(!ok)return;sent=true;ok=dispatch(id,std::move(payload));};
    if(s.draft.name!=s.value.name || priority-1!=s.value.priority || o!=stockpileOptionsOf(s.value))
        send("stockpile.set_basics",SetStockpileBasicsPayload{s.value.id,s.draft.name,priority-1,o.suspended,o.pull,o.allowPull});
    // Rule changes go as at most two batches: the rules to allow and the rules to block.
    for(const bool allowed:{true,false}) {
        std::vector<StockpileFilterRowId> rows;
        for(const auto& [key,change]:s.draft.rules) if(change.allowed==allowed) rows.push_back(change.id);
        if(!rows.empty()) send("stockpile.set_filters",SetStockpileFiltersPayload{s.value.id,std::move(rows),allowed});
    }
    if(!sent){s.draft=freshStockpileDraft(s.value);notify();return true;}
    s.draft.pending=ok;
    notify();return ok;
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
    if(templateReviewId_!=state_.stockpile.value.id || templateReviewRevision_!=state_.stockpile.revision) {
        cancelStockpileTemplateOverwrite();stockpileFeedback("The allow list changed. Review the template update again.");return;
    }
    const auto name = state_.stockpile.pendingTemplateOverwrite;
    state_.stockpile.templateOverwriteConfirmationRequired=false;
	if ( dispatch( "stockpile.save_template", StockpileTemplatePayload { state_.stockpile.value.id, name, true }, DispatchOrigin::DestructiveConfirmation ) )
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
    if(pane!=StockpilePane::AllowList)state_.stockpile.templateMenuOpen=false;
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
	if(id.starts_with("stockpile.") && id!="stockpile.refresh" && state_.stockpile.request.status==RequestStatus::Error) {
        state_.status="ui.error.target_missing";notify();return false;
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
		state_.stockpile.templateMenuOpen=false;
		state_.view = ManagementView::None;
		notify();
	}
}
void Management6AController::editWorkshopDraft(std::string name,std::string priority)
{
    auto& s=state_.workshop;
    if(s.draft.pending) return;
    s.draft.name=std::move(name);s.draft.priority=std::move(priority);
    s.draft.dirty=workshopDraftDiffers(s.draft,s.value);
    notify();
}
void Management6AController::editWorkshopOptions(WorkshopOptions options)
{
    auto& s=state_.workshop;
    if(s.draft.pending || !s.value.id) return;
    std::sort(options.linked.begin(),options.linked.end());
    options.linked.erase(std::unique(options.linked.begin(),options.linked.end()),options.linked.end());
    s.draft.options=std::move(options);
    s.draft.dirty=workshopDraftDiffers(s.draft,s.value);
    notify();
}
void Management6AController::revertWorkshopDraft()
{
    auto& s=state_.workshop;if(s.draft.pending)return;
    s.draft=freshWorkshopDraft(s.value);
    s.feedback.clear();notify();
}
bool Management6AController::applyWorkshopDraft()
{
    auto& s=state_.workshop;
    if(!s.value.id || s.draft.pending || state_.pendingAction)return false;
    if(!s.draft.dirty)return true;
    int priority=0;const auto& raw=s.draft.priority;auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),priority);
    if(trimmed(s.draft.name).empty() || parsed.ec!=std::errc{} || parsed.ptr!=raw.data()+raw.size() || priority<1 || priority>std::max(1,s.value.maxPriority)) {
        workshopFeedback("Enter a name and a whole priority from 1 to "+std::to_string(std::max(1,s.value.maxPriority))+".");return false;
    }
    const auto current=workshopOptionsOf(s.value);
    if(s.draft.baseName!=s.value.name || s.draft.basePriority!=s.value.priority || s.draft.baseOptions!=current) {
        workshopFeedback("This workshop was changed elsewhere. Press Cancel to see the latest values, then make your changes again.");return false;
    }
    s.draft.name=trimmed(s.draft.name);
    const auto& o=s.draft.options;
    bool sent=false,ok=true;
    const auto send=[&](std::string_view id,UiActionPayload payload){if(!ok)return;sent=true;ok=dispatch(id,std::move(payload));};
    if(s.draft.name!=s.value.name || priority-1!=s.value.priority || o.suspended!=current.suspended || o.acceptGenerated!=current.acceptGenerated || o.autoCraftMissing!=current.autoCraftMissing)
        send("workshop.set_basics",SetWorkshopBasicsPayload{s.value.id,s.draft.name,priority-1,o.suspended,o.acceptGenerated,o.autoCraftMissing});
    if(s.value.subtype=="Butcher" && (o.butcherCorpses!=current.butcherCorpses || o.butcherExcess!=current.butcherExcess))
        send("workshop.set_butcher_options",SetButcherOptionsPayload{s.value.id,o.butcherCorpses,o.butcherExcess});
    if((s.value.subtype=="Fisher" || s.value.subtype=="Fishery") && (o.catchFish!=current.catchFish || o.processFish!=current.processFish))
        send("workshop.set_fisher_options",SetFisherOptionsPayload{s.value.id,o.catchFish,o.processFish});
    if(s.value.canLinkStockpile)
        for(const auto& row:s.value.stockpiles) {
            const bool wanted=std::find(o.linked.begin(),o.linked.end(),row.id.value)!=o.linked.end();
            if(wanted!=row.linked) send("workshop.set_stockpile_link",SetWorkshopStockpileLinkPayload{s.value.id,row.id,wanted});
        }
    if(!sent){s.draft=freshWorkshopDraft(s.value);notify();return true;}
    s.draft.pending=ok;
    notify();return ok;
}
void Management6AController::workshopFeedback(std::string text) {state_.workshop.feedback=std::move(text);notify();}
void Management6AController::rejectWorkshop(WorkshopId id,std::string text) {if(id!=state_.workshop.value.id)return;state_.workshop.draft.pending=false;state_.workshop.tradePending=false;state_.pendingAction.reset();state_.workshop.tradeConfirmationRequired=false;workshopFeedback(std::move(text));}
void Management6AController::setWorkshopBasics( std::string name, std::int32_t priority, bool suspended, bool generated, bool autoMissing, std::optional<bool> linkStockpile )
{
	const auto& id = state_.workshop.value.id;
	if ( id && !state_.workshop.draft.pending && !state_.pendingAction )
		dispatch( "workshop.set_basics", SetWorkshopBasicsPayload { id, std::move( name ), priority, suspended, generated, autoMissing, std::nullopt, linkStockpile } );
}
void Management6AController::setButcherOptions( bool corpses, bool excess )
{
	if ( state_.workshop.value.id && state_.workshop.value.subtype=="Butcher" )
		dispatch( "workshop.set_butcher_options", SetButcherOptionsPayload { state_.workshop.value.id, corpses, excess } );
}
void Management6AController::setFisherOptions( bool catchFish, bool processFish )
{
	if ( state_.workshop.value.id && (state_.workshop.value.subtype=="Fisher" || state_.workshop.value.subtype=="Fishery") )
		dispatch( "workshop.set_fisher_options", SetFisherOptionsPayload { state_.workshop.value.id, catchFish, processFish } );
}
void Management6AController::setWorkshopStockpileLink(StockpileId stockpile, bool linked)
{
    if(state_.workshop.value.id && state_.workshop.value.canLinkStockpile && hasRow(state_.workshop.value.stockpiles,stockpile))
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
    if(state_.workshop.orderPending || count==0 || count>999) return;
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
	if ( state_.workshop.value.id && state_.workshop.selectedJob && hasRow(state_.workshop.value.queue,*state_.workshop.selectedJob) && count > 0 && count<=999 )
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
	auto& s=state_.workshop;
 auto& rows=row.party==TradeParty::Trader?s.traderRows:s.playerRows;
 auto it=findRow(rows,row);
 if(!s.value.id || !s.tradeLoaded || s.tradePending || s.tradeConfirmationRequired || it==rows.end() || count>it->stock)return;
 s.tradePending=true;
 if(!dispatch("trade.set_offer_count",SetTradeOfferPayload{s.value.id,row,count,s.tradeRevision,s.traderId}))s.tradePending=false;
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
	const auto desired = std::clamp<std::int64_t>( static_cast<std::int64_t>( row->offered ) + delta, 0, static_cast<std::int64_t>( row->stock ) );
	setTradeOffer( row->id, static_cast<std::uint32_t>( desired ) );
}
void Management6AController::executeTrade()
{
 auto& s=state_.workshop;
 if(!s.tradeLoaded || s.tradePending || s.tradeConfirmationRequired)return;
 bool offered=false;for(const auto& row:s.traderRows)offered|=row.offered>0;for(const auto& row:s.playerRows)offered|=row.offered>0;
 if(!offered){workshopFeedback("Choose items and quantities before reviewing trade.");return;}
 s.tradeReviewRevision=s.tradeRevision;
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
	if ( state_.workshop.tradeConfirmationRequired && !state_.workshop.tradePending && state_.workshop.tradeReviewRevision==state_.workshop.tradeRevision && state_.workshop.value.id && state_.workshop.playerOfferValue >= state_.workshop.traderOfferValue )
	{
		const auto workshop = state_.workshop.value.id;
        state_.workshop.tradePending=true;
        state_.workshop.tradeConfirmationRequired=false;
		if ( dispatch( "trade.execute", WorkshopTargetPayload { workshop,state_.workshop.tradeRevision,state_.workshop.traderId }, DispatchOrigin::DestructiveConfirmation ) )
		{
			state_.workshop.tradeConfirmationRequired = false;
			notify();
		} else {state_.workshop.tradePending=false;notify();}
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
bool Management6AController::stockpileRuleAllowed( const StockpileFilterRow& row ) const
{
	const auto& rules = state_.stockpile.draft.rules;
	const auto found  = rules.find( stockpileRuleKey( row.id ) );
	return found != rules.end() ? found->second.allowed : row.state == TriState::On;
}
void Management6AController::toggleStockpileRule( const StockpileFilterRowId& id )
{
	auto& s = state_.stockpile;
	if ( s.draft.pending || id.depth != FilterDepth::Material ) return;
	const auto it = findRow( s.value.filters, id );
	if ( it == s.value.filters.end() ) return;
	const bool wanted = !stockpileRuleAllowed( *it );
	const auto key    = stockpileRuleKey( id );
	if ( wanted == ( it->state == TriState::On ) ) s.draft.rules.erase( key );
	else s.draft.rules[key] = { it->id, wanted };
	s.draft.dirty = stockpileDraftDiffers( s.draft, s.value );
	notify();
}
void Management6AController::toggleSelectedStockpileFilter()
{
	if ( state_.stockpile.selectedFilter ) toggleStockpileRule( *state_.stockpile.selectedFilter );
}
void Management6AController::setStockpileRulesShown( bool allowed )
{
	auto& s = state_.stockpile;
	if ( s.draft.pending || !s.value.id ) return;
	for ( const auto& row : s.visibleFilters )
	{
		const auto key = stockpileRuleKey( row.id );
		if ( allowed == ( row.state == TriState::On ) ) s.draft.rules.erase( key );
		else s.draft.rules[key] = { row.id, allowed };
	}
	s.draft.dirty = stockpileDraftDiffers( s.draft, s.value );
	notify();
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
    if(result.status==CommandStatus::Rejected) state_.stockpile.draft.pending=false;
	if ( result.status == CommandStatus::Rejected && state_.agriculture.draft.pending )
	{
		state_.agriculture.draft.pending = false;
		state_.agriculture.feedback      = result.error.empty() ? std::string( "The designation rejected the change." ) : result.error;
	}
	notify();
}
} // namespace ingnomia::ui::management6a
