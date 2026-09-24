/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6AQtCommandPort.h"

#include "../../../../base/position.h"
#include "../../../aggregatoragri.h"
#include "../../../aggregatorrenderer.h"
#include "../../../aggregatorstockpile.h"
#include "../../../aggregatorworkshop.h"
#include "../../../eventconnector.h"
#include "../../actions/UiActionRegistry.h"

#include <QMetaObject>
#include <QStringList>

namespace ingnomia::ui::management6a
{
namespace
{
int craftMode( CraftRepeatMode mode )
{
	return mode == CraftRepeatMode::Once ? 0 : mode == CraftRepeatMode::Maintain ? 1
																				 : 2;
}
AgriType agriType( AgricultureKind kind )
{
	return kind == AgricultureKind::Farm ? AgriType::Farm : kind == AgricultureKind::Pasture ? AgriType::Pasture
																							 : AgriType::Grove;
}
QStringList materials( const std::vector<CatalogId>& values )
{
	QStringList out;
	for ( const auto& value : values )
		out.push_back( QString::fromStdString( value.value ) );
	return out;
}
} // namespace

Management6AQtCommandPort::Management6AQtCommandPort( EventConnector* connector, ManagementView view ) :
	connector_( connector ), view_( view )
{
}
CommandResult Management6AQtCommandPort::reject( const char* error ) const
{
	return { CommandStatus::Rejected, false, error };
}
CommandResult Management6AQtCommandPort::queue( std::function<void()> fn ) const
{
	if ( !connector_ || !QMetaObject::invokeMethod( connector_, std::move( fn ), Qt::QueuedConnection ) )
		return reject( "ui.error.bridge_queue_failed" );
	return { CommandStatus::Accepted, true, {} };
}
void Management6AQtCommandPort::rememberTradeOffer( const TradeRowId& id, std::uint32_t offered )
{
	for ( auto& row : tradeOffers_ )
		if ( row.id == id )
		{
			row.offered = offered;
			return;
		}
	tradeOffers_.push_back( { id, offered } );
}
std::uint32_t Management6AQtCommandPort::rememberedTradeOffer( const TradeRowId& id ) const
{
	for ( const auto& row : tradeOffers_ )
		if ( row.id == id )
			return row.offered;
	return 0;
}

CommandResult Management6AQtCommandPort::dispatch( const UiActionEnvelope& action, DispatchOrigin origin )
{
	if ( !connector_ )
		return reject( "ui.error.bridge_unavailable" );
	ActionValidationContext validation;
	validation.activeWorld         = activeWorld_;
	validation.acceptsWorldActions = acceptsActions_;
	validation.primaryRoute        = RouteId { "game.hud" };
	if ( origin == DispatchOrigin::DestructiveConfirmation && action.id.value == "trade.execute" )
	{
		validation.topModal     = ModalInstanceId { 1 };
		validation.sourceModal  = ModalInstanceId { 1 };
		validation.topModalKind = ModalKind::DestructiveConfirmation;
	}
	if ( !UiActionRegistry {}.validate( action, validation ).valid() )
		return reject( "ui.error.invalid_or_stale_action" );

	if ( action.id.value == "nav.close" )
		return queue( [c = connector_, view = view_]
		{
			if ( !c ) return;
			if ( view == ManagementView::Workshop ) c->aggregatorWorkshop()->onCloseWindow();
			else if ( view == ManagementView::Stockpile ) c->aggregatorStockpile()->onCloseWindow();
			else if ( view == ManagementView::Agriculture ) c->aggregatorAgri()->onCloseWindow();
		} );
	if ( action.id.value == "view.center_on" )
	{
		const auto* payload = std::get_if<CenterPayload>( &action.payload );
		if ( !payload )
			return reject( "ui.error.invalid_payload" );
		std::optional<WorldPosition> pos;
		if ( const auto* world = std::get_if<WorldPosition>( &payload->target ) )
			pos = *world;
		else
			pos = std::get<EntityRef>( payload->target ).position;
		if ( !pos )
			return reject( "ui.error.target_has_no_position" );
		const Position target( pos->x, pos->y, pos->z );
		return queue( [c = connector_, target]
					  {if(c)c->aggregatorRenderer()->onCenterCamera(target); } );
	}
	if ( action.id.value == "workshop.refresh" || action.id.value == "trade.refresh" )
	{
		const auto* p = std::get_if<WorkshopTargetPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		const auto id = p->workshop.value;
		return queue( [c = connector_, id, trade = action.id.value == "trade.refresh"]
					  {if(!c)return;if(trade)c->aggregatorWorkshop()->onRequestAllTradeItems(id);else c->aggregatorWorkshop()->onUpdateWorkshopInfo(id); } );
	}
    if (action.id.value == "workshop.set_stockpile_link") {
        const auto* p=std::get_if<SetWorkshopStockpileLinkPayload>(&action.payload);
        if(!p) return reject("ui.error.invalid_payload");
        return queue([c=connector_,v=*p] { if(c) c->aggregatorWorkshop()->onSetStockpileLink(v.workshop.value,v.stockpile.value,v.linked); });
    }
	if ( action.id.value == "workshop.set_basics" )
	{
		const auto* p = std::get_if<SetWorkshopBasicsPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		const bool linked = p->linkStockpile.value_or( p->connectStockpile.has_value() || workshopLinks_.value( p->workshop.value, false ) );
		return queue( [c = connector_, v = *p, linked]
					  {if(!c)return;auto*a=c->aggregatorWorkshop();a->onSetBasicOptions(v.workshop.value,QString::fromStdString(v.name),v.priority,v.suspended,v.acceptGenerated,v.autoCraftMissing,linked,!v.linkStockpile.has_value() && !v.connectStockpile.has_value());a->onUpdateWorkshopInfo(v.workshop.value); } );
	}
	if ( action.id.value == "workshop.set_butcher_options" )
	{
		const auto* p = std::get_if<SetButcherOptionsPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c){c->aggregatorWorkshop()->onSetButcherOptions(v.workshop.value,v.butcherCorpses,v.butcherExcess);c->aggregatorWorkshop()->onUpdateWorkshopInfo(v.workshop.value);} } );
	}
	if ( action.id.value == "workshop.set_fisher_options" )
	{
		const auto* p = std::get_if<SetFisherOptionsPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c){c->aggregatorWorkshop()->onSetFisherOptions(v.workshop.value,v.catchFish,v.processFish);c->aggregatorWorkshop()->onUpdateWorkshopInfo(v.workshop.value);} } );
	}
	if ( action.id.value == "workshop.queue_craft" )
	{
		const auto* p = std::get_if<QueueCraftPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p, m = materials( p->materials )]
					  {if(c)c->aggregatorWorkshop()->onCraftItem(v.workshop.value,QString::fromStdString(v.craft.value),craftMode(v.mode),static_cast<int>(v.count),m); } );
	}
	if ( action.id.value == "workshop.set_job" )
	{
		const auto* p = std::get_if<SetCraftJobPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c){c->aggregatorWorkshop()->onCraftJobParams(v.workshop.value,v.job.value,craftMode(v.mode),static_cast<int>(v.count),v.suspended,v.moveBack);c->aggregatorWorkshop()->onCraftListChanged(v.workshop.value);} } );
	}
	if ( action.id.value == "workshop.move_job" )
	{
		const auto* p = std::get_if<MoveCraftJobPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		QString cmd = p->direction == MoveDirection::Up ? "Up" : p->direction == MoveDirection::Down ? "Down"
															 : p->direction == MoveDirection::Front  ? "Top"
																									 : "Bottom";
		return queue( [c = connector_, v = *p, cmd]
					  {if(c)c->aggregatorWorkshop()->onCraftJobCommand(v.workshop.value,v.job.value,cmd); } );
	}
	if ( action.id.value == "workshop.cancel_job" )
	{
		const auto* p = std::get_if<CraftJobTargetPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c)c->aggregatorWorkshop()->onCraftJobCommand(v.workshop.value,v.job.value,"Cancel"); } );
	}
	if ( action.id.value == "trade.set_offer_count" )
	{
		const auto* p = std::get_if<SetTradeOfferPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		const auto old = rememberedTradeOffer( p->row ), desired = p->count;
		if ( old == desired )
			return { CommandStatus::Accepted, false, {} };
		const int delta = static_cast<int>( desired ) - static_cast<int>( old );
		rememberTradeOffer( p->row, desired );
		return queue( [c = connector_, v = *p, delta]
					  {if(!c)return;auto*a=c->aggregatorWorkshop();const auto item=QString::fromStdString(v.row.item.value),material=QString::fromStdString(v.row.materialOrGender.value);if(v.row.party==TradeParty::Trader){if(delta>0)a->onTraderStocktoOffer(v.workshop.value,item,material,v.row.quality,delta);else a->onTraderOffertoStock(v.workshop.value,item,material,v.row.quality,-delta);}else{if(delta>0)a->onPlayerStocktoOffer(v.workshop.value,item,material,v.row.quality,delta);else a->onPlayerOffertoStock(v.workshop.value,item,material,v.row.quality,-delta);} } );
	}
	if ( action.id.value == "trade.execute" )
	{
		const auto* p = std::get_if<WorkshopTargetPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, id = p->workshop.value]
					  {if(c)c->aggregatorWorkshop()->onTrade(id); } );
	}

	if ( action.id.value == "stockpile.refresh" )
	{
		const auto* p = std::get_if<StockpileTargetPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, id = p->stockpile.value]
					  {if(c)c->aggregatorStockpile()->onUpdateStockpileInfo(id); } );
	}
	if ( action.id.value == "stockpile.set_basics" )
	{
		const auto* p = std::get_if<SetStockpileBasicsPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c){c->aggregatorStockpile()->onSetBasicOptions(v.stockpile.value,QString::fromStdString(v.name),v.priority,v.suspended,v.pull,v.allowPull);c->aggregatorStockpile()->onUpdateStockpileInfo(v.stockpile.value);} } );
	}
	if ( action.id.value == "stockpile.set_filter" )
	{
		const auto* p = std::get_if<SetStockpileFilterPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c)c->aggregatorStockpile()->onSetActive(v.row.stockpile.value,v.active,QString::fromStdString(v.row.category.value),QString::fromStdString(v.row.group.value),QString::fromStdString(v.row.item.value),QString::fromStdString(v.row.material.value)); } );
	}
	if ( action.id.value == "stockpile.set_filters" )
	{
		const auto* p = std::get_if<SetStockpileFiltersPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		QList<QStringList> paths;
		paths.reserve( static_cast<qsizetype>( p->rows.size() ) );
		for ( const auto& row : p->rows )
			paths.push_back( { QString::fromStdString( row.category.value ), QString::fromStdString( row.group.value ), QString::fromStdString( row.item.value ), QString::fromStdString( row.material.value ) } );
		return queue( [c = connector_, id = p->stockpile.value, active = p->active, paths = std::move( paths )]
					  {if(c)c->aggregatorStockpile()->onSetActiveBatch(id,active,paths); } );
	}
	if ( action.id.value == "stockpile.save_template" || action.id.value == "stockpile.apply_template" )
	{
		const auto* p = std::get_if<StockpileTemplatePayload>( &action.payload );
		if ( !p ) return reject( "ui.error.invalid_payload" );
		const bool save = action.id.value == "stockpile.save_template";
		return queue( [c = connector_, id = p->stockpile.value, name = QString::fromStdString( p->name ), save]
			{ if ( !c ) return; if ( save ) c->aggregatorStockpile()->onSaveFilterTemplate( id, name ); else c->aggregatorStockpile()->onApplyFilterTemplate( id, name ); } );
	}

	if ( action.id.value == "agriculture.refresh" )
	{
		const auto* p = std::get_if<AgricultureTargetPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, t = p->target]
					  {if(!c)return;auto*a=c->aggregatorAgri();if(t.kind==AgricultureKind::Farm)a->onUpdateFarm(t.designation.value);else if(t.kind==AgricultureKind::Pasture)a->onUpdatePasture(t.designation.value);else a->onUpdateGrove(t.designation.value); } );
	}
	if ( action.id.value == "agriculture.set_basics" )
	{
		const auto* p = std::get_if<SetAgricultureBasicsPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c){c->aggregatorAgri()->onSetBasicOptions(agriType(v.target.kind),v.target.designation.value,QString::fromStdString(v.name),v.priority,v.suspended);c->aggregatorAgri()->onUpdate(v.target.designation.value);} } );
	}
	if ( action.id.value == "agriculture.select_product" )
	{
		const auto* p = std::get_if<SetAgricultureProductPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c)c->aggregatorAgri()->onSelectProduct(agriType(v.target.kind),v.target.designation.value,QString::fromStdString(v.product.value)); } );
	}
	if ( action.id.value == "agriculture.set_plot_crop" )
	{
		const auto* p = std::get_if<SetFarmPlotCropPayload>( &action.payload );
		if ( !p ) return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p] {
			if ( !c ) return;
			QList<Position> plots;
			for ( const auto& plot : v.plots ) plots.append( Position( plot.x, plot.y, plot.z ) );
			c->aggregatorAgri()->onSetFarmPlotCrop( v.farm.value, plots, QString::fromStdString( v.crop.value ) );
		} );
	}
	if ( action.id.value == "agriculture.queue_plot_crop" )
	{
		const auto* p = std::get_if<QueueFarmPlotCropPayload>( &action.payload );
		if ( !p ) return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p] {
			if ( !c ) return;
			QList<Position> plots;
			for ( const auto& plot : v.plots ) plots.append( Position( plot.x, plot.y, plot.z ) );
			c->aggregatorAgri()->onQueueFarmPlotCrop( v.farm.value, plots, QString::fromStdString( v.crop.value ), static_cast<int>( v.count ), v.repeat );
		} );
	}
	if ( action.id.value == "agriculture.cancel_plot_order" )
	{
		const auto* p = std::get_if<FarmPlotOrderPayload>( &action.payload );
		if ( !p ) return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p] { if ( c ) c->aggregatorAgri()->onRemoveFarmPlotOrder( v.farm.value, Position( v.plot.x, v.plot.y, v.plot.z ), v.order ); } );
	}
	if ( action.id.value == "agriculture.move_plot_order" )
	{
		const auto* p = std::get_if<MoveFarmPlotOrderPayload>( &action.payload );
		if ( !p ) return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p] { if ( c ) c->aggregatorAgri()->onMoveFarmPlotOrder( v.farm.value, Position( v.plot.x, v.plot.y, v.plot.z ), v.order, v.direction == MoveDirection::Up ); } );
	}
	if ( action.id.value == "agriculture.set_harvest_options" )
	{
		const auto* p = std::get_if<SetHarvestOptionsPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c)c->aggregatorAgri()->onSetHarvestOptions(agriType(v.target.kind),v.target.designation.value,v.harvest,v.harvestHay,v.tame); } );
	}
	if ( action.id.value == "agriculture.set_grove_options" )
	{
		const auto* p = std::get_if<SetGroveOptionsPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c)c->aggregatorAgri()->onSetGroveOptions(v.grove.value,v.pick,v.plant,v.fell); } );
	}
	if ( action.id.value == "agriculture.set_population_caps" )
	{
		const auto* p = std::get_if<SetPastureCapPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(!c)return;if(v.gender==Gender::Male)c->aggregatorAgri()->onSetMaxMale(v.pasture.value,static_cast<int>(v.max));else c->aggregatorAgri()->onSetMaxFemale(v.pasture.value,static_cast<int>(v.max)); } );
	}
	if ( action.id.value == "agriculture.set_butchering" )
	{
		const auto* p = std::get_if<SetButcheringPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c)c->aggregatorAgri()->onSetButchering(v.creature.value,v.butcher); } );
	}
	if ( action.id.value == "agriculture.set_food_allowed" )
	{
		const auto* p = std::get_if<SetPastureFoodPayload>( &action.payload );
		if ( !p )
			return reject( "ui.error.invalid_payload" );
		return queue( [c = connector_, v = *p]
					  {if(c)c->aggregatorAgri()->onSetFoodItemChecked(v.pasture.value,QString::fromStdString(v.item.value),QString::fromStdString(v.material.value),v.allowed); } );
	}
	return reject( "ui.error.action_unavailable" );
}
} // namespace ingnomia::ui::management6a
