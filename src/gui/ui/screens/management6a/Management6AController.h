/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "Management6AState.h"

namespace ingnomia::ui::management6a
{
enum class CommandStatus : std::uint8_t { Accepted, Rejected };
enum class DispatchOrigin : std::uint8_t { Workbench, DestructiveConfirmation };
struct CommandResult { CommandStatus status{ CommandStatus::Accepted }; bool pending{}; std::string error; };
class CommandPort { public: virtual ~CommandPort() = default; virtual CommandResult dispatch( const UiActionEnvelope&, DispatchOrigin ) = 0; };
class ViewPort { public: virtual ~ViewPort() = default; virtual void stateChanged( const Management6AState& ) = 0; };

class Management6AController
{
public:
	Management6AController( CommandPort&, ViewPort& );
	[[nodiscard]] const Management6AState& state() const noexcept { return state_; }
	void beginWorld( WorldEpoch );
	void endWorld();
	void showLoading( ManagementView );
	void showError( ManagementView, std::string );
	void showWorkshop( WorkshopSnapshot, Revision, std::optional<WorldPosition> = {} );
	void showStockpile( StockpileSnapshot, Revision, std::optional<WorldPosition> = {} );
	void showAgriculture( AgricultureSnapshot, Revision, std::optional<WorldPosition> = {} );
	void setTradeRows( TradeParty, std::vector<TradeRow> );
	void updateTradeRow( TradeRow );
	void setTradeValues( std::int32_t trader, std::int32_t player );
	void setAgricultureCatalog( AgricultureKind, std::vector<AgricultureCatalogRow> );
	bool patchWorkshopQueue( Revision base, Revision next, const std::vector<StableRowPatch<CraftJobId,CraftQueueRow>>& );
	bool patchStockpileContents( Revision base, Revision next, const std::vector<StableRowPatch<StockpileContentRowId,StockpileContentRow>>& );
	bool patchAgricultureAnimals( Revision base, Revision next, const std::vector<StableRowPatch<CreatureId,PastureAnimalRow>>& );

	void setSearch( std::string );
	void toggleSort();
	void selectWorkshopProduct( CatalogId );
	void selectWorkshopJob( CraftJobId );
	void selectTradeRow( TradeRowId );
	void selectStockpileFilter( StockpileFilterRowId );
	void selectStockpileContent( StockpileContentRowId );
	void selectAgricultureProduct( CatalogId );
	void selectAgricultureAnimal( CreatureId );
	void nextWorkshopProduct();
	void nextWorkshopJob();
	void nextTradeRow();
	void moveStockpileFilterSelection( std::int32_t delta );
	void nextStockpileFilter();
	void nextStockpileContent();
	void nextAgricultureProduct();
	void nextAgricultureAnimal();

	void refresh();
	void locate();
	void close();
	void setWorkshopBasics( std::string, std::int32_t, bool, bool, bool, std::optional<bool> linkStockpile = std::nullopt );
	void setButcherOptions( bool, bool );
	void setFisherOptions( bool, bool );
	void queueSelectedCraft( CraftRepeatMode, std::uint32_t, std::vector<CatalogId> );
	void queueSelectedCraftDefault();
	void setSelectedJob( CraftRepeatMode, std::uint32_t, bool, bool );
	void moveSelectedJob( MoveDirection );
	void cancelSelectedJob();
	void refreshTrade();
	void setTradeOffer( TradeRowId, std::uint32_t );
	void adjustSelectedTradeOffer( std::int32_t );
	void executeTrade();
	void confirmTrade();
	void cancelTrade();
	void setStockpileBasics( std::string, std::int32_t, bool, bool, bool );
	void toggleSelectedStockpileFilter();
	void setAgricultureBasics( std::string, std::int32_t, bool );
	void applySelectedAgricultureProduct();
	void setHarvestOptions( bool, bool, bool );
	void setGroveOptions( bool, bool, bool );
	void setPastureCap( Gender, std::uint32_t );
	void toggleSelectedAnimalButchering();
	void setPastureFood( CatalogId, CatalogId, bool );
	void onActionFinished( RequestId, CommandResult );

private:
	bool dispatch( std::string_view, UiActionPayload, DispatchOrigin = DispatchOrigin::Workbench );
	void rebuildWorkshop();
	void rebuildStockpile();
	void rebuildAgriculture();
	void notify();
	template<class Id, class Row> static bool applyPatches( std::vector<Row>&, const std::vector<StableRowPatch<Id,Row>>& );
	CommandPort& commands_;
	ViewPort& view_;
	Management6AState state_;
	std::uint64_t nextRequest_{ 1 };
};
} // namespace ingnomia::ui::management6a
