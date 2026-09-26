/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../actions/UiActions.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ingnomia::ui::management6a
{
enum class ManagementView : std::uint8_t { None, Workshop, Stockpile, Agriculture };
enum class RequestStatus : std::uint8_t { Idle, Loading, Ready, Empty, Error, Stale };
enum class SortDirection : std::uint8_t { Ascending, Descending };
enum class TriState : std::uint8_t { Off, On, Mixed };
enum class WorkshopPane : std::uint8_t { Craft, Queue, Settings, Trade, Stockpiles };
enum class StockpilePane : std::uint8_t { Contents, AllowList, Settings };
enum class StockpileSortKey : std::uint8_t { Category, Group, Item, Material, Quantity, Total, Status };

struct RequestState
{
	RequestStatus status{ RequestStatus::Idle };
	std::optional<RequestId> request;
	std::string message;
	bool refreshInProgress{};
	bool operator==( const RequestState& ) const = default;
};

struct WorkshopComponentRow
{
	CatalogId item;
	std::uint32_t amount{};
	bool requireSameMaterial{};
	std::vector<std::pair<CatalogId, std::uint32_t>> materials;
	bool operator==( const WorkshopComponentRow& ) const = default;
};

struct WorkshopProductRow
{
	CatalogId id;
	std::vector<WorkshopComponentRow> components;
	bool operator==( const WorkshopProductRow& ) const = default;
};

struct CraftQueueRow
{
	CraftJobId id;
	CatalogId craft;
	CatalogId item;
	CraftRepeatMode mode{ CraftRepeatMode::Once };
	std::uint32_t count{ 1 };
	std::uint32_t alreadyCrafted{}; // exact domain field; never presented as progress or ETA
	bool suspended{};
	bool moveBack{};
	std::vector<CatalogId> materials;
	bool operator==( const CraftQueueRow& ) const = default;
};

struct TradeRow
{
	TradeRowId id;
	std::string name;
	std::uint32_t stock{};
	std::uint32_t offered{};
	std::int32_t unitValue{};
	bool operator==( const TradeRow& ) const = default;
};

struct WorkshopStockpileRow { StockpileId id; std::string name; bool linked{}; bool operator==(const WorkshopStockpileRow&) const = default; };
struct WorkshopSnapshot
{
	WorkshopId id;
	std::string name;
	std::string subtype;
	std::int32_t priority{ 1 }, maxPriority{ 1 };
	bool suspended{}, acceptGenerated{}, autoCraftMissing{}, connectStockpile{};
	bool canLinkStockpile{};
	std::vector<WorkshopStockpileRow> stockpiles;
	bool butcherCorpses{}, butcherExcess{}, catchFish{}, processFish{};
	std::vector<WorkshopProductRow> products;
	std::vector<CraftQueueRow> queue;
	bool operator==( const WorkshopSnapshot& ) const = default;
};
// Special-GUI workshops (Butcher, Fishery, MarketStall) have no craft catalog; they keep
// Craft/Queue only while a legacy queue still holds orders that need editing.
inline bool workshopSupportsCrafting( const WorkshopSnapshot& value ) { return !value.products.empty() || !value.queue.empty(); }
inline bool workshopSupportsTrade( const WorkshopSnapshot& value ) { return value.subtype == "Trader" || value.subtype == "TradingPost"; }
inline bool workshopSupportsStockpileLinks( const WorkshopSnapshot& value ) { return value.canLinkStockpile && workshopSupportsCrafting( value ); }

// Property-sheet options that stay pending until Apply or OK (Windows property sheet model).
struct WorkshopOptions {
 bool suspended{}, acceptGenerated{}, autoCraftMissing{};
 bool butcherCorpses{}, butcherExcess{}, catchFish{}, processFish{};
 std::vector<std::uint32_t> linked; // sorted stockpile IDs
 bool operator==(const WorkshopOptions&) const = default;
};
inline WorkshopOptions workshopOptionsOf( const WorkshopSnapshot& value )
{
 WorkshopOptions o{ value.suspended, value.acceptGenerated, value.autoCraftMissing, value.butcherCorpses, value.butcherExcess, value.catchFish, value.processFish, {} };
 for ( const auto& row : value.stockpiles ) if ( row.linked ) o.linked.push_back( row.id.value );
 std::sort( o.linked.begin(), o.linked.end() );
 return o;
}
struct WorkshopDraft {
 std::string name, priority, baseName;
 int basePriority{}; bool dirty{}, pending{};
 WorkshopOptions options, baseOptions;
 bool operator==(const WorkshopDraft&) const = default;
};
struct WorkshopState
{
	WorkshopDraft draft;
 std::string feedback;
 std::uint64_t tradeRevision{}, tradeReviewRevision{};
 std::uint32_t traderId{};
 bool tradePending{};
 WorkshopPane pane{ WorkshopPane::Craft };
	bool orderPending{};
	std::string orderFeedback;
	RequestState request;
	Revision revision;
	WorkshopSnapshot value;
	std::optional<WorldPosition> position;
	std::string search;
	SortDirection sort{ SortDirection::Ascending };
	std::optional<CatalogId> selectedProduct;
	std::optional<CraftJobId> selectedJob;
	CraftRepeatMode orderMode{ CraftRepeatMode::Once };
	std::uint32_t orderCount{ 1 };
	std::vector<CatalogId> orderMaterials;
	std::vector<WorkshopProductRow> visibleProducts;
	std::vector<CraftQueueRow> visibleQueue;
	std::vector<TradeRow> traderRows, playerRows;
	std::optional<TradeRowId> selectedTradeRow;
	std::int32_t traderOfferValue{}, playerOfferValue{};
	bool selectionFiltered{};
	bool tradeLoaded{};
	bool tradeConfirmationRequired{};
	bool operator==( const WorkshopState& ) const = default;
};

struct StockpileContentRowId
{
	CatalogId category, group, item, material;
	FilterDepth depth{ FilterDepth::Category };
	bool operator==( const StockpileContentRowId& ) const = default;
};

struct StockpileFilterRow
{
	StockpileFilterRowId id;
	std::string label;
	TriState state{ TriState::Off };
	struct Icon
	{
		std::string sheet;
		std::int32_t width{}, height{};
		bool operator==( const Icon& ) const = default;
	} icon;
	bool operator==( const StockpileFilterRow& ) const = default;
};

struct StockpileContentRow
{
	StockpileContentRowId id;
	std::string name;
	std::uint32_t stockpiled{}, total{};
	StockpileFilterRow::Icon icon;
	bool operator==( const StockpileContentRow& ) const = default;
};

struct StockpileSnapshot
{
	StockpileId id;
	std::string name;
	std::int32_t priority{ 1 }, maxPriority{ 1 }, capacity{}, itemCount{}, reserved{};
	bool suspended{}, pullFromOthers{}, allowPullFromHere{};
	std::vector<StockpileFilterRow> filters;
	std::vector<StockpileContentRow> contents;
	std::vector<std::string> templateNames;
	bool operator==( const StockpileSnapshot& ) const = default;
};

struct StockpileOptions
{
	bool suspended{}, pull{}, allowPull{};
	bool operator==( const StockpileOptions& ) const = default;
};
inline StockpileOptions stockpileOptionsOf( const StockpileSnapshot& value ) { return { value.suspended, value.pullFromOthers, value.allowPullFromHere }; }
// A pending allow-list change: the rule's wanted state. Only rules whose wanted state differs from the
// game's are kept, keyed by the rule's path so lookups stay cheap on large allow lists.
struct StockpileRuleChange
{
	StockpileFilterRowId id;
	bool allowed{};
	bool operator==( const StockpileRuleChange& ) const = default;
};
struct StockpileDraft
{
    std::string name, priority, baseName;
    int basePriority{};
    bool dirty{}, pending{};
	StockpileOptions options, baseOptions;
	std::map<std::string, StockpileRuleChange> rules;
    bool operator==(const StockpileDraft&) const = default;
};
inline std::string stockpileRuleKey( const StockpileFilterRowId& id )
{
	return id.category.value + '\x1f' + id.group.value + '\x1f' + id.item.value + '\x1f' + id.material.value;
}
struct StockpileState
{
    StockpileDraft draft;
    std::string feedback;

	RequestState request;
	Revision revision;
	StockpileSnapshot value;
	std::optional<WorldPosition> position;
	std::string search;
	std::string filterSearch;
	std::string contentSearch;
	std::string filterSearchBeforeReveal;
	std::string templateName;
	std::array<std::string, 6> contentColumnFilters;
	std::array<std::string, 5> allowColumnFilters;
	std::array<std::vector<std::string>, 6> contentColumnSelections;
	std::array<std::vector<std::string>, 5> allowColumnSelections;
	CatalogId filterCategory;
	SortDirection sort{ SortDirection::Ascending };
	StockpileSortKey contentSort{ StockpileSortKey::Item };
	SortDirection allowSortDirection{ SortDirection::Ascending };
	StockpileSortKey allowSort{ StockpileSortKey::Item };
	StockpilePane pane{ StockpilePane::Contents };
	std::optional<StockpileFilterRowId> selectedFilter;
	std::optional<StockpileContentRowId> selectedContent;
	std::vector<StockpileFilterRowId> expandedFilters;
	std::vector<StockpileContentRowId> expandedContents;
	std::vector<StockpileFilterRowId> matchingFilterLeaves;
	std::vector<StockpileFilterRow> visibleFilters;
	std::vector<StockpileContentRow> visibleContents;
	bool selectionFiltered{};
	bool filterSearchRevealed{};
	bool templateMenuOpen{};
	bool templateOverwriteConfirmationRequired{};
	std::string pendingTemplateOverwrite;
	bool operator==( const StockpileState& ) const = default;
};

struct AgricultureCatalogRow
{
	CatalogId id;
	std::string name;
	std::uint32_t available{}, planted{}, harvested{};
	std::string iconSheet;
	bool operator==( const AgricultureCatalogRow& ) const = default;
};

struct FarmCropOrderRow
{
	std::uint32_t id{};
	CatalogId crop;
	std::int32_t remaining{ 1 };
	bool repeat{};
	bool operator==( const FarmCropOrderRow& ) const = default;
};

struct FarmPlotRow
{
	WorldPosition position;
	CatalogId assignedCrop, plantedCrop;
	bool tilled{}, planted{}, ready{}, busy{};
	std::vector<FarmCropOrderRow> orders;
	bool operator==( const FarmPlotRow& ) const = default;
};

struct PastureAnimalRow
{
	CreatureId id;
	std::string name;
	CatalogId species;
	Gender gender{ Gender::Female };
	bool young{}, butcher{};
	bool operator==( const PastureAnimalRow& ) const = default;
};

struct PastureFoodRow
{
	CatalogId item, material;
	std::string name;
	bool allowed{};
	bool operator==( const PastureFoodRow& ) const = default;
};

struct AgricultureSnapshot
{
	AgricultureTarget target;
	std::string name;
	CatalogId product;
	std::string productName;
	std::int32_t productSeeds{}, productItems{}, productPlants{};
	std::int32_t priority{ 1 }, maxPriority{ 1 };
	std::int32_t plots{}, tilled{}, planted{}, ready{};
	std::int32_t male{}, female{}, total{}, capacity{}, maxMale{}, maxFemale{};
	std::int32_t foodCurrent{}, foodMax{}, hayCurrent{}, hayMax{};
	bool suspended{}, harvest{}, harvestHay{}, tame{}, pick{}, plant{}, fell{};
	std::vector<AgricultureCatalogRow> catalog;
	std::vector<FarmPlotRow> fields;
	std::vector<PastureAnimalRow> animals;
	std::vector<PastureFoodRow> foods;
	bool operator==( const AgricultureSnapshot& ) const = default;
};

// Property-sheet pages. Plots and PlotQueue exist only for farms, Animals and Food only for pastures.
enum class AgriculturePane : std::uint8_t { General, Plots, PlotQueue, Crops, Animals, Food };
enum class PlotSelect : std::uint8_t { Only, Toggle, Range };

// Settings that stay pending until Apply or OK (Windows property sheet model).
struct AgricultureOptions
{
	bool suspended{}, harvest{}, harvestHay{}, tame{}, pick{}, plant{}, fell{};
	CatalogId product;
	std::int32_t maxMale{}, maxFemale{};
	std::vector<std::uint32_t> butcher; // sorted creature IDs marked for butchering
	std::vector<std::string> foods;      // sorted "item|material" keys that are allowed
	bool operator==( const AgricultureOptions& ) const = default;
};
inline std::string pastureFoodKey( const CatalogId& item, const CatalogId& material ) { return item.value + "|" + material.value; }
inline AgricultureOptions agricultureOptionsOf( const AgricultureSnapshot& v )
{
	AgricultureOptions o { v.suspended, v.harvest, v.harvestHay, v.tame, v.pick, v.plant, v.fell, v.product, v.maxMale, v.maxFemale, {}, {} };
	for ( const auto& a : v.animals ) if ( a.butcher ) o.butcher.push_back( a.id.value );
	for ( const auto& f : v.foods ) if ( f.allowed ) o.foods.push_back( pastureFoodKey( f.item, f.material ) );
	std::sort( o.butcher.begin(), o.butcher.end() );
	std::sort( o.foods.begin(), o.foods.end() );
	return o;
}
struct AgricultureDraft
{
	std::string name, baseName;
	AgricultureOptions options, baseOptions;
	bool dirty{}, pending{};
	bool operator==( const AgricultureDraft& ) const = default;
};
inline bool agricultureSupportsPane( AgricultureKind kind, AgriculturePane pane )
{
	switch ( pane )
	{
		case AgriculturePane::General: return true;
		case AgriculturePane::Plots:
		case AgriculturePane::PlotQueue: return kind == AgricultureKind::Farm;
		case AgriculturePane::Crops: return kind != AgricultureKind::Pasture;
		case AgriculturePane::Animals:
		case AgriculturePane::Food: return kind == AgricultureKind::Pasture;
	}
	return false;
}

struct AgricultureState
{
	RequestState request;
	Revision revision;
	AgricultureSnapshot value;
	AgricultureDraft draft;
	std::string feedback;
	AgriculturePane pane{ AgriculturePane::General };
	std::optional<WorldPosition> position;
	std::string search;
	SortDirection sort{ SortDirection::Ascending };
	std::optional<CatalogId> selectedProduct;
	std::vector<WorldPosition> selectedPlots;
	std::optional<WorldPosition> plotAnchor, focusedPlot;
	std::optional<CreatureId> selectedAnimal;
	std::optional<std::string> selectedFood;
	std::vector<AgricultureCatalogRow> visibleCatalog;
	std::vector<PastureAnimalRow> visibleAnimals;
	bool selectionFiltered{};
	bool operator==( const AgricultureState& ) const = default;
};

struct Management6AState
{
	WorldEpoch world;
	Revision revision;
	bool acceptsWorldActions{};
	ManagementView view{ ManagementView::None };
	WorkshopState workshop;
	StockpileState stockpile;
	AgricultureState agriculture;
	std::optional<RequestId> pendingAction;
	std::string status;
	bool operator==( const Management6AState& ) const = default;
};

enum class PatchKind : std::uint8_t { Insert, Update, Remove, Move };
template<class Id, class Row> struct StableRowPatch
{
	PatchKind kind{ PatchKind::Update };
	Id id;
	std::optional<Row> row;
	std::optional<Id> before;
};
} // namespace ingnomia::ui::management6a
