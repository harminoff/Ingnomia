/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6AIntegration.h"

#include "../../../aggregatoragri.h"
#include "../../../aggregatorstockpile.h"
#include "../../../aggregatorworkshop.h"
#include "../../../eventconnector.h"
#include "Management6AQtDataAdapter.h"

namespace ingnomia::ui::management6a
{
Management6AIntegration::Management6AIntegration( EventConnector* connector, Rml::Context& context, QObject* parent ) :
	QObject( parent ), connector_( connector ), context_( context )
{
}
Management6AIntegration::~Management6AIntegration()
{
	shutdown();
}
bool Management6AIntegration::initialize()
{
	if ( controller_ || !connector_ )
		return false;
	binding_    = std::make_unique<Management6ARmlBinding>( context_ );
	commands_   = std::make_unique<Management6AQtCommandPort>( connector_ );
	controller_ = std::make_unique<Management6AController>( *commands_, *binding_ );
	if ( !binding_->initialize( *controller_ ) )
	{
		shutdown();
		return false;
	}
	connectSignals();
	return true;
}
void Management6AIntegration::shutdown()
{
	if ( binding_ )
		binding_->shutdown();
	controller_.reset();
	commands_.reset();
	binding_.reset();
	connected_ = false;
}
void Management6AIntegration::beginWorld( WorldEpoch world )
{
	workshopRevision_    = {};
	stockpileRevision_   = {};
	agricultureRevision_ = {};
	selectedPosition_.reset();
	commands_->setWorld( world, true );
	controller_->beginWorld( world );
}
void Management6AIntegration::endWorld()
{
	if ( commands_ )
		commands_->setWorld( {}, false );
	if ( controller_ )
		controller_->endWorld();
	selectedPosition_.reset();
	plants_.clear();
	animals_.clear();
	trees_.clear();
}
bool Management6AIntegration::activateElement( std::string_view id )
{
	return binding_ && binding_->activateElement( id );
}
bool Management6AIntegration::setFormValueForProbe( std::string_view id, std::string_view value )
{
	return binding_ && binding_->setFormValueForProbe( id, value );
}
bool Management6AIntegration::setStockpileSearchForProbe( std::string_view value )
{
	return binding_ && binding_->setStockpileSearchForProbe( value );
}
bool Management6AIntegration::activateFirstStockpileFilterForProbe( TriState state, FilterDepth depth )
{
	return binding_ && binding_->activateFirstStockpileFilterForProbe( state, depth );
}
bool Management6AIntegration::activateStockpileFilterForProbe( std::string_view item, std::string_view material )
{
	return binding_ && binding_->activateStockpileFilterForProbe( item, material );
}
bool Management6AIntegration::dispatchStockpileFilterKeyForProbe( int keyIdentifier )
{
	return binding_ && binding_->dispatchStockpileFilterKeyForProbe( keyIdentifier );
}
void Management6AIntegration::connectSignals()
{
	if ( connected_ )
		return;
	connected_ = true;
	auto* ws   = connector_->aggregatorWorkshop();
	auto* sp   = connector_->aggregatorStockpile();
	auto* ag   = connector_->aggregatorAgri();
	connect( ws, &AggregatorWorkshop::signalOpenWorkshopWindow, this, [this]( unsigned int )
			 {if(controller_){if(viewHandler_)viewHandler_(ManagementView::Workshop);controller_->showLoading(ManagementView::Workshop);} }, Qt::QueuedConnection );
	auto workshop = [this]( const GuiWorkshopInfo& value )
	{if(!controller_)return;commands_->rememberWorkshopLink(WorkshopId{value.workshopID},value.linkStockpile);controller_->showWorkshop(Management6AQtDataAdapter::workshop(value),Revision{++workshopRevision_.value},selectedPosition_); };
	connect( ws, &AggregatorWorkshop::signalUpdateInfo, this, workshop, Qt::QueuedConnection );
	connect( ws, &AggregatorWorkshop::signalUpdateContent, this, workshop, Qt::QueuedConnection );
	connect( ws, &AggregatorWorkshop::signalUpdateCraftList, this, workshop, Qt::QueuedConnection );
	auto tradeRows = [this]( const QList<GuiTradeItem>& values, TradeParty party )
	{if(!controller_)return;std::vector<TradeRow> rows;for(const auto&value:values){auto row=Management6AQtDataAdapter::trade(value,party);commands_->rememberTradeOffer(row.id,row.offered);rows.push_back(std::move(row));}controller_->setTradeRows(party,std::move(rows)); };
	connect( ws, &AggregatorWorkshop::signalTraderStock, this, [tradeRows]( const QList<GuiTradeItem>& v )
			 { tradeRows( v, TradeParty::Trader ); }, Qt::QueuedConnection );
	connect( ws, &AggregatorWorkshop::signalPlayerStock, this, [tradeRows]( const QList<GuiTradeItem>& v )
			 { tradeRows( v, TradeParty::Player ); }, Qt::QueuedConnection );
	connect( ws, &AggregatorWorkshop::signalUpdateTraderStockItem, this, [this]( const GuiTradeItem& v )
			 {if(!controller_)return;auto row=Management6AQtDataAdapter::trade(v,TradeParty::Trader);commands_->rememberTradeOffer(row.id,row.offered);controller_->updateTradeRow(std::move(row)); }, Qt::QueuedConnection );
	connect( ws, &AggregatorWorkshop::signalUpdatePlayerStockItem, this, [this]( const GuiTradeItem& v )
			 {if(!controller_)return;auto row=Management6AQtDataAdapter::trade(v,TradeParty::Player);commands_->rememberTradeOffer(row.id,row.offered);controller_->updateTradeRow(std::move(row)); }, Qt::QueuedConnection );
	connect( ws, &AggregatorWorkshop::signalUpdateTraderValue, this, [this]( int value )
			 {if(controller_)controller_->setTradeValues(value,controller_->state().workshop.playerOfferValue); }, Qt::QueuedConnection );
	connect( ws, &AggregatorWorkshop::signalUpdatePlayerValue, this, [this]( int value )
			 {if(controller_)controller_->setTradeValues(controller_->state().workshop.traderOfferValue,value); }, Qt::QueuedConnection );
	connect( sp, &AggregatorStockpile::signalOpenStockpileWindow, this, [this]( unsigned int )
			 {if(controller_){if(viewHandler_)viewHandler_(ManagementView::Stockpile);controller_->showLoading(ManagementView::Stockpile);} }, Qt::QueuedConnection );
	auto stockpile = [this]( const GuiStockpileInfo& value )
	{if(controller_)controller_->showStockpile(Management6AQtDataAdapter::stockpile(value),Revision{++stockpileRevision_.value},selectedPosition_); };
	connect( sp, &AggregatorStockpile::signalUpdateInfo, this, stockpile, Qt::QueuedConnection );
	connect( sp, &AggregatorStockpile::signalUpdateContent, this, stockpile, Qt::QueuedConnection );
	connect( ag, &AggregatorAgri::signalShowAgri, this, [this]( unsigned int )
			 {if(controller_){if(viewHandler_)viewHandler_(ManagementView::Agriculture);controller_->showLoading(ManagementView::Agriculture);} }, Qt::QueuedConnection );
	connect( ag, &AggregatorAgri::signalGlobalPlantInfo, this, [this]( const QList<GuiPlant>& v )
			 {plants_=Management6AQtDataAdapter::plants(v);if(controller_)controller_->setAgricultureCatalog(AgricultureKind::Farm,plants_); }, Qt::QueuedConnection );
	connect( ag, &AggregatorAgri::signalGlobalTreeInfo, this, [this]( const QList<GuiPlant>& v )
			 {trees_=Management6AQtDataAdapter::plants(v);if(controller_)controller_->setAgricultureCatalog(AgricultureKind::Grove,trees_); }, Qt::QueuedConnection );
	connect( ag, &AggregatorAgri::signalGlobalAnimalInfo, this, [this]( const QList<GuiAnimal>& v )
			 {animals_=Management6AQtDataAdapter::animals(v);if(controller_)controller_->setAgricultureCatalog(AgricultureKind::Pasture,animals_); }, Qt::QueuedConnection );
	connect( ag, &AggregatorAgri::signalUpdateFarm, this, [this]( const GuiFarmInfo& v )
			 {if(!controller_)return;auto value=Management6AQtDataAdapter::farm(v);value.catalog=plants_;controller_->showAgriculture(std::move(value),Revision{++agricultureRevision_.value},selectedPosition_); }, Qt::QueuedConnection );
	connect( ag, &AggregatorAgri::signalUpdatePasture, this, [this]( const GuiPastureInfo& v )
			 {if(!controller_)return;auto value=Management6AQtDataAdapter::pasture(v);value.catalog=animals_;controller_->showAgriculture(std::move(value),Revision{++agricultureRevision_.value},selectedPosition_); }, Qt::QueuedConnection );
	connect( ag, &AggregatorAgri::signalUpdateGrove, this, [this]( const GuiGroveInfo& v )
			 {if(!controller_)return;auto value=Management6AQtDataAdapter::grove(v);value.catalog=trees_;controller_->showAgriculture(std::move(value),Revision{++agricultureRevision_.value},selectedPosition_); }, Qt::QueuedConnection );
}
void Management6AIntegration::loadSelfTestFixture()
{
	if ( !controller_ )
		return;
	beginWorld( WorldEpoch { 6001 } );
	WorkshopSnapshot ws;
	ws.id               = WorkshopId { 421 };
	ws.name             = "Deepvein Forge";
	ws.subtype          = "Production";
	ws.priority         = 2;
	ws.maxPriority      = 5;
	ws.acceptGenerated  = true;
	ws.connectStockpile = true;
	WorkshopProductRow p1;
	p1.id = CatalogId { "iron_pick" };
	WorkshopComponentRow pickHead;
	pickHead.item                = CatalogId { "pick_head" };
	pickHead.amount              = 1;
	pickHead.requireSameMaterial = true;
	pickHead.materials           = { { CatalogId { "iron" }, 14 }, { CatalogId { "steel" }, 2 } };
	WorkshopComponentRow handle;
	handle.item      = CatalogId { "handle" };
	handle.amount    = 1;
	handle.materials = { { CatalogId { "oak" }, 8 } };
	p1.components    = { pickHead, handle };
	WorkshopProductRow p2;
	p2.id = CatalogId { "bronze_gear" };
	WorkshopComponentRow bar;
	bar.item                = CatalogId { "metal_bar" };
	bar.amount              = 2;
	bar.requireSameMaterial = true;
	bar.materials           = { { CatalogId { "bronze" }, 23 } };
	p2.components           = { bar };
	ws.products             = { p1, p2 };
	CraftQueueRow gear;
	gear.id        = CraftJobId { 7001 };
	gear.craft     = CatalogId { "bronze_gear" };
	gear.item      = CatalogId { "gear" };
	gear.mode      = CraftRepeatMode::Maintain;
	gear.count     = 12;
	gear.alreadyCrafted = 3;
	gear.moveBack  = true;
	gear.materials = { CatalogId { "bronze" } };
	CraftQueueRow pick;
	pick.id        = CraftJobId { 7002 };
	pick.craft     = CatalogId { "iron_pick" };
	pick.item      = CatalogId { "pick" };
	pick.mode      = CraftRepeatMode::Repeat;
	pick.count     = 1;
	pick.suspended = true;
	pick.materials = { CatalogId { "iron" }, CatalogId { "oak" } };
	ws.queue       = { gear, pick };
	controller_->showWorkshop( std::move( ws ), Revision { 1 }, WorldPosition { 48, 32, -7 } );
	TradeRow traderRow;
	traderRow.id        = { TradeParty::Trader, CatalogId { "cloth" }, CatalogId { "wool" }, 1 };
	traderRow.name      = "Fine wool cloth";
	traderRow.stock     = 8;
	traderRow.offered   = 2;
	traderRow.unitValue = 14;
	TradeRow playerRow;
	playerRow.id        = { TradeParty::Player, CatalogId { "gear" }, CatalogId { "bronze" }, 2 };
	playerRow.name      = "Bronze gear";
	playerRow.stock     = 21;
	playerRow.offered   = 5;
	playerRow.unitValue = 9;
	controller_->setTradeRows( TradeParty::Trader, { traderRow } );
	controller_->setTradeRows( TradeParty::Player, { playerRow } );
	controller_->setTradeValues( 28, 45 );
}
} // namespace ingnomia::ui::management6a
