/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../actions/UiActions.h"

#include <optional>
#include <string>
#include <vector>

namespace ingnomia::ui::management6a
{
enum class ManagementView : std::uint8_t { None, Workshop, Stockpile, Agriculture };
enum class RequestStatus : std::uint8_t { Idle, Loading, Ready, Empty, Error, Stale };
enum class SortDirection : std::uint8_t { Ascending, Descending };
enum class TriState : std::uint8_t { Off, On, Mixed };

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

struct WorkshopSnapshot
{
	WorkshopId id;
	std::string name;
	std::string subtype;
	std::int32_t priority{ 1 }, maxPriority{ 1 };
	bool suspended{}, acceptGenerated{}, autoCraftMissing{}, connectStockpile{};
	bool butcherCorpses{}, butcherExcess{}, catchFish{}, processFish{};
	std::vector<WorkshopProductRow> products;
	std::vector<CraftQueueRow> queue;
	bool operator==( const WorkshopSnapshot& ) const = default;
};

struct WorkshopState
{
	RequestState request;
	Revision revision;
	WorkshopSnapshot value;
	std::optional<WorldPosition> position;
	std::string search;
	SortDirection sort{ SortDirection::Ascending };
	std::optional<CatalogId> selectedProduct;
	std::optional<CraftJobId> selectedJob;
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
	CatalogId item, material;
	bool operator==( const StockpileContentRowId& ) const = default;
};

struct StockpileFilterRow
{
	StockpileFilterRowId id;
	std::string label;
	TriState state{ TriState::Off };
	bool operator==( const StockpileFilterRow& ) const = default;
};

struct StockpileContentRow
{
	StockpileContentRowId id;
	std::string itemName, materialName;
	std::uint32_t count{};
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
	bool operator==( const StockpileSnapshot& ) const = default;
};

struct StockpileState
{
	RequestState request;
	Revision revision;
	StockpileSnapshot value;
	std::optional<WorldPosition> position;
	std::string search;
	SortDirection sort{ SortDirection::Ascending };
	std::optional<StockpileFilterRowId> selectedFilter;
	std::optional<StockpileContentRowId> selectedContent;
	std::vector<StockpileFilterRow> visibleFilters;
	std::vector<StockpileContentRow> visibleContents;
	bool selectionFiltered{};
	bool operator==( const StockpileState& ) const = default;
};

struct AgricultureCatalogRow
{
	CatalogId id;
	std::string name;
	std::uint32_t available{}, planted{}, harvested{};
	bool operator==( const AgricultureCatalogRow& ) const = default;
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
	std::int32_t priority{ 1 }, maxPriority{ 1 };
	std::int32_t plots{}, tilled{}, planted{}, ready{};
	std::int32_t male{}, female{}, total{}, capacity{}, maxMale{}, maxFemale{};
	std::int32_t foodCurrent{}, foodMax{}, hayCurrent{}, hayMax{};
	bool suspended{}, harvest{}, harvestHay{}, tame{}, pick{}, plant{}, fell{};
	std::vector<AgricultureCatalogRow> catalog;
	std::vector<PastureAnimalRow> animals;
	std::vector<PastureFoodRow> foods;
	bool operator==( const AgricultureSnapshot& ) const = default;
};

struct AgricultureState
{
	RequestState request;
	Revision revision;
	AgricultureSnapshot value;
	std::optional<WorldPosition> position;
	std::string search;
	SortDirection sort{ SortDirection::Ascending };
	std::optional<CatalogId> selectedProduct;
	std::optional<CreatureId> selectedAnimal;
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
