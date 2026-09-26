/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../../runtime/CaptionText.h"
#include "../ManagementTooltip.h"
#include "../../runtime/CommandFeedback.h"
#include "Management6ARmlBinding.h"
#include "../../runtime/SelectOptions.h"
#include "../../runtime/ConnectedTabs.h"
#include "../InventoryTableSchema.h"
#include "../../runtime/ReportControls.h"
#include "../../localization/RmlText.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <tuple>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include "../../runtime/SelectOptions.h"
#include <RmlUi/Core/StringUtilities.h>

namespace ingnomia::ui::management6a
{
namespace
{
std::string safe( const std::string& value )
{
	return Rml::StringUtilities::EncodeRml( value );
}
std::string displayCatalogLabel( const std::string& value )
{
	if ( value == "any" ) return "Any material";
	std::string result;
	for ( std::size_t i = 0; i < value.size(); ++i )
	{
		const auto ch = static_cast<unsigned char>( value[i] );
		if ( i && std::isupper( ch ) && ( std::islower( static_cast<unsigned char>( value[i-1] ) ) || ( i+1 < value.size() && std::islower( static_cast<unsigned char>( value[i+1] ) ) ) ) ) result += ' ';
		result += value[i] == '_' ? ' ' : value[i];
	}
	return result;
}
std::string foldedLabel( std::string value )
{
	std::transform( value.begin(), value.end(), value.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
	return value;
}
using report::filterOptionMarkup;

std::string mode( CraftRepeatMode value )
{
	return value == CraftRepeatMode::Once ? "Number" : value == CraftRepeatMode::Maintain ? "Stock limit"
																						  : "Repeat";
}
struct StockpilePathLabels
{
	std::string category, group, item, material;
};
template<class RowId>
StockpilePathLabels stockpilePathLabels( const RowId& leaf, const std::vector<StockpileFilterRow>& rows )
{
	StockpilePathLabels labels;
	for ( const auto& row : rows )
	{
		if ( row.id.category != leaf.category ) continue;
		if ( row.id.depth == FilterDepth::Category ) labels.category = row.label;
		else if ( row.id.depth == FilterDepth::Group && row.id.group == leaf.group ) labels.group = row.label;
		else if ( row.id.depth == FilterDepth::Item && row.id.group == leaf.group && row.id.item == leaf.item ) labels.item = row.label;
		else if ( row.id.depth == FilterDepth::Material && row.id.group == leaf.group && row.id.item == leaf.item && row.id.material == leaf.material ) labels.material = row.label;
	}
	normalizeInventoryTableLabels( labels.group, labels.item, labels.material, leaf.item.value );
	return labels;
}
Rml::Element* dataElement( Rml::Element* target, Rml::Element* boundary, const char* attribute )
{
	for ( auto* element = target; element && element != boundary; element = element->GetParentNode() )
		if ( element->HasAttribute( attribute ) )
			return element;
	return nullptr;
}
std::string toggleText( std::string_view label, bool value )
{
	return std::string( value ? "[x] " : "[ ] " ) + std::string( label );
}
std::string kind( AgricultureKind value )
{
	return value == AgricultureKind::Farm ? "Farm" : value == AgricultureKind::Pasture ? "Pasture"
																					   : "Grove";
}
std::string hex( std::string_view value )
{
	static constexpr char digits[] = "0123456789abcdef";
	std::string out;
	out.reserve( value.size() * 2 );
	for ( unsigned char c : value )
	{
		out.push_back( digits[c >> 4] );
		out.push_back( digits[c & 15] );
	}
	return out;
}
std::string rowId( std::string_view prefix, std::string_view value )
{
	return std::string( prefix ) + hex( value );
}
std::string farmPlotKey( WorldPosition plot )
{
	return std::to_string( plot.x ) + "," + std::to_string( plot.y ) + "," + std::to_string( plot.z );
}
std::optional<WorldPosition> farmPlotFromKey( const std::string& key )
{
	WorldPosition plot;
	char trailing{};
	if ( std::sscanf( key.c_str(), "%d,%d,%d%c", &plot.x, &plot.y, &plot.z, &trailing ) != 3 ) return std::nullopt;
	return plot;
}
std::string stockpileFilterRowId( const StockpileFilterRowId& row )
{
	return "m6a_filter_" + hex( row.category.value ) + "_" + hex( row.group.value ) + "_" + hex( row.item.value ) + "_" + hex( row.material.value ) + "_" + std::to_string( static_cast<int>( row.depth ) );
}
std::string attr( const Rml::Element* element, const char* name )
{
	return element ? element->GetAttribute<Rml::String>( name, "" ) : std::string {};
}
bool priorityDigit( Rml::Input::KeyIdentifier key )
{
	const auto value = static_cast<int>( key );
	return ( value >= static_cast<int>( Rml::Input::KI_0 ) && value <= static_cast<int>( Rml::Input::KI_9 ) ) ||
	       ( value >= static_cast<int>( Rml::Input::KI_NUMPAD0 ) && value <= static_cast<int>( Rml::Input::KI_NUMPAD9 ) );
}
bool priorityEditingKey( Rml::Input::KeyIdentifier key )
{
	return key == Rml::Input::KI_BACK || key == Rml::Input::KI_DELETE || key == Rml::Input::KI_LEFT || key == Rml::Input::KI_RIGHT ||
	       key == Rml::Input::KI_HOME || key == Rml::Input::KI_END || key == Rml::Input::KI_TAB || key == Rml::Input::KI_RETURN ||
	       key == Rml::Input::KI_ESCAPE;
}
bool priorityTextIsNumeric( const Rml::String& value )
{
	return std::all_of( value.begin(), value.end(), []( unsigned char c ) { return std::isdigit( c ) != 0; } );
}
} // namespace
void Management6ARmlBinding::Callback::ProcessEvent( Rml::Event& event )
{
	fn_( event );
}
Management6ARmlBinding::Management6ARmlBinding( Rml::Context& context ) :
	context_( context )
{
}
Management6ARmlBinding::~Management6ARmlBinding()
{
	shutdown();
}
bool Management6ARmlBinding::initialize( Management6AController& controller )
{
	controller_  = &controller;
    stockpileDialog_=std::make_unique<ModalDialog>(context_);
    stockpileDialog_->setDocumentPath("modals/win98_message_box.rml");
	const auto load = [this]( const char* path )
		{ return documentLoader_ ? documentLoader_( path ) : context_.LoadDocument( path ); };
	workshop_    = load( "windows/workshop_manager.rml" );
	stockpile_   = load( "windows/stockpile_manager.rml" );
	agriculture_ = load( "panels/agriculture_manager.rml" );
	if ( !workshop_ || !stockpile_ || !agriculture_ )
	{
		shutdown();
		return false;
	}
	localization::applyRmlText( *workshop_, textCatalog_ );
	localization::applyRmlText( *stockpile_, textCatalog_ );
	localization::applyRmlText( *agriculture_, textCatalog_ );
	for ( const char* id : { "workshop_close", "stockpile_close", "agriculture_close" } )
		bindClick( id, [this]
				   { if(!canClose())return; controller_->close(); if ( closeHandler_ ) closeHandler_(); } );
	// Enter activates OK, the default button; Escape is Cancel (secondary window keyboard rules).
	bind( "workshop_workbench", "keydown", [this]( Rml::Event& e )
		  {
			  if ( stockpileDialog_ && stockpileDialog_->active() ) return;
			  const auto key = e.GetParameter<int>( "key_identifier", 0 );
			  auto* target = e.GetTargetElement();
			  const bool onButton = target && target->GetTagName() == "button";
			  if ( key == Rml::Input::KI_RETURN && !onButton ) { (void)activateElement( "workshop_ok" ); e.StopPropagation(); }
			  else if ( key == Rml::Input::KI_ESCAPE ) { (void)activateElement( "workshop_cancel" ); e.StopPropagation(); }
		  } );
	for ( const char* id : { "workshop_locate", "stockpile_locate", "agriculture_locate" } )
		bindClick( id, [this]
				   { controller_->locate(); } );
	for ( const char* id : { "workshop_refresh", "stockpile_refresh", "agriculture_refresh" } )
		bindClick( id, [this]
				   { controller_->refresh(); } );
	for ( const char* id : { "workshop_sort", "stockpile_sort", "agriculture_sort" } )
		bindClick( id, [this]
				   { controller_->toggleSort(); } );
	auto rerender = [this]
	{ stateChanged( controller_->state() ); };
	bindClick( "workshop_page_previous", [this, rerender]
			   {workshopPage_=workshopPage_>=pageSize_?workshopPage_-pageSize_:0;rerender(); } );
	bindClick( "workshop_page_next", [this, rerender]
			   {workshopPage_+=pageSize_;rerender(); } );
	bindClick( "agriculture_page_previous", [this, rerender]
			   {agriculturePage_=agriculturePage_>=pageSize_?agriculturePage_-pageSize_:0;rerender(); } );
	bindClick( "agriculture_page_next", [this, rerender]
			   {agriculturePage_+=pageSize_;rerender(); } );
	for ( const char* id : { "workshop_search", "agriculture_search" } )
	{
		auto search = [this, id]( Rml::Event& )
		{
			if ( std::string_view( id ) == "workshop_search" )
				workshopPage_ = 0;
			else
				agriculturePage_ = 0;
			controller_->setSearch( formValue( id ) );
		};
		bind( id, "input", search );
		bind( id, "change", search );
	}
	bindStockpile();
	for ( const auto& [id, pane] : std::array {
		std::pair { "workshop_view_craft", WorkshopPane::Craft }, std::pair { "workshop_view_queue", WorkshopPane::Queue },
		std::pair { "workshop_view_settings", WorkshopPane::Settings }, std::pair { "workshop_view_stockpiles", WorkshopPane::Stockpiles },
		std::pair { "workshop_view_trade", WorkshopPane::Trade } } )
		bindClick( id, [this, pane] { controller_->setWorkshopPane( pane ); } );
	bind( "workshop_product_selection", "change", [this]( Rml::Event& e ) {
		if ( renderingWorkshop_ ) return;
		auto* select = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-material-index" );
		if ( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( select ) )
			controller_->setWorkshopOrderMaterial( static_cast<std::size_t>( std::stoul( attr( select, "data-material-index" ) ) ), CatalogId { control->GetValue() } );
	} );
	bind( "workshop_products", "click", [this]( Rml::Event& e )
		  {auto*target=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-catalog");const auto key=attr(target,"data-catalog");if(!key.empty())controller_->selectWorkshopProduct(CatalogId{key}); } );
	bind( "workshop_queue", "click", [this]( Rml::Event& e )
		  {auto*target=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-job");const auto key=attr(target,"data-job");if(!key.empty())controller_->selectWorkshopJob({static_cast<std::uint32_t>(std::stoul(key))}); } );
    bind("workshop_order_mode","change",[this](Rml::Event&) {
        if(renderingWorkshop_) return;
        const auto value=formValue("workshop_order_mode");
        controller_->setWorkshopOrderMode(value=="maintain"?CraftRepeatMode::Maintain:value=="repeat"?CraftRepeatMode::Repeat:CraftRepeatMode::Once);
    });
	// Selected order: option buttons, quantity and move-back are edited locally; Update Order sends them together.
	const auto selectedQueueRow = [this]() -> const CraftQueueRow*
	{
		const auto& state = controller_->state().workshop;
		if ( !state.selectedJob ) return nullptr;
		const auto row = std::find_if( state.value.queue.begin(), state.value.queue.end(), [&]( const auto& value ) { return value.id == *state.selectedJob; } );
		return row == state.value.queue.end() ? nullptr : &*row;
	};
	bindClick( "workshop_job_apply", [this, selectedQueueRow]
			   {
				   const auto* row = selectedQueueRow();
				   if ( !row ) return;
				   const auto count = NumericEditor::parse( formValue( "workshop_job_count" ), 1, 999 );
				   if ( !count ) { controller_->workshopFeedback( "Enter a whole order quantity from 1 to 999." ); return; }
				   const auto type = formValue( "workshop_job_mode" );
				   const auto mode = type == "maintain" ? CraftRepeatMode::Maintain : type == "repeat" ? CraftRepeatMode::Repeat : CraftRepeatMode::Once;
				   const bool moveBack = element( "workshop_job_move_back" ) && element( "workshop_job_move_back" )->HasAttribute( "checked" );
				   controller_->setSelectedJob( mode, static_cast<std::uint32_t>( *count ), row->suspended, moveBack );
			   } );
	bindClick( "workshop_job_suspend", [this, selectedQueueRow]
			   { if ( const auto* row = selectedQueueRow() ) controller_->setSelectedJob( row->mode, row->count, !row->suspended, row->moveBack ); } );
	bindClick( "workshop_job_top", [this] { controller_->moveSelectedJob( MoveDirection::Front ); } );
	bindClick( "workshop_job_up", [this] { controller_->moveSelectedJob( MoveDirection::Up ); } );
	bindClick( "workshop_job_down", [this] { controller_->moveSelectedJob( MoveDirection::Down ); } );
	bindClick( "workshop_job_bottom", [this] { controller_->moveSelectedJob( MoveDirection::Back ); } );
	bindClick( "workshop_job_cancel", [this] { controller_->cancelSelectedJob(); } );
	for ( const char* list : { "workshop_trade_merchant", "workshop_trade_settlement" } )
		bind( list, "click", [this]( Rml::Event& e )
			  {auto*target=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-item");const auto item=attr(target,"data-item"),material=attr(target,"data-material"),party=attr(target,"data-party"),quality=attr(target,"data-quality");if(!item.empty())controller_->selectTradeRow({party=="trader"?TradeParty::Trader:TradeParty::Player,CatalogId{item},CatalogId{material},static_cast<std::uint8_t>(std::stoul(quality))}); } );

    workshopPriorityEditor_=std::make_unique<NumericEditor>(*workshop_,"workshop_priority",[this](int n){controller_->editWorkshopDraft(formValue("workshop_name"),std::to_string(n));return true;},-1,"workshop_priority_up","workshop_priority_down");
    workshopOrderEditor_=std::make_unique<NumericEditor>(*workshop_,"workshop_order_count",[this](int n){controller_->setWorkshopOrderCount(n);return true;});
    workshopJobEditor_=std::make_unique<NumericEditor>(*workshop_,"workshop_job_count",[](int){return true;});
    workshopTradeEditor_=std::make_unique<NumericEditor>(*workshop_,"workshop_trade_count",[](int){return true;});
    for(const char* id:{"workshop_name","workshop_priority"})for(const char* event:{"input","change"})
        bind(id,event,[this](Rml::Event&){if(!renderingWorkshop_)controller_->editWorkshopDraft(formValue("workshop_name"),formValue("workshop_priority"));});
    // Every check box on the General and Stockpiles pages edits the pending sheet; nothing applies until Apply or OK.
    const auto optionBox=[this](const char* id,bool WorkshopOptions::*field){
        bind(id,"change",[this,id,field](Rml::Event&){
            if(syncingCheckbox_ || renderingWorkshop_)return;
            auto options=controller_->state().workshop.draft.options;
            options.*field=element(id)->HasAttribute("checked");
            controller_->editWorkshopOptions(std::move(options));
        });
    };
    optionBox("workshop_toggle_suspended",&WorkshopOptions::suspended);
    optionBox("workshop_toggle_generated",&WorkshopOptions::acceptGenerated);
    optionBox("workshop_toggle_auto_missing",&WorkshopOptions::autoCraftMissing);
    optionBox("workshop_toggle_corpses",&WorkshopOptions::butcherCorpses);
    optionBox("workshop_toggle_excess",&WorkshopOptions::butcherExcess);
    optionBox("workshop_toggle_catch",&WorkshopOptions::catchFish);
    optionBox("workshop_toggle_process",&WorkshopOptions::processFish);
    bind("workshop_linked_stockpiles","change",[this](Rml::Event& e){
        if(syncingCheckbox_ || renderingWorkshop_)return;
        auto* box=e.GetTargetElement();
        const auto key=attr(box,"data-link-id");if(key.empty())return;
        const auto id=static_cast<std::uint32_t>(std::stoul(key));
        auto options=controller_->state().workshop.draft.options;
        std::erase(options.linked,id);
        if(box->HasAttribute("checked"))options.linked.push_back(id);
        controller_->editWorkshopOptions(std::move(options));
        e.StopPropagation();
    });
    // Property sheet buttons: OK applies and closes, Cancel discards and closes, Apply applies and stays open.
    const auto workshopTitle=[this]{const auto& n=controller_->state().workshop.value.name;return captionName( n.empty()?std::string("Workshop"):n ) + " Properties";};
    bindClick("workshop_apply",[this,workshopTitle]{if(committed(*workshopPriorityEditor_,"Priority",workshopTitle()))controller_->applyWorkshopDraft();});
    bindClick("workshop_ok",[this,workshopTitle]{
        if(!committed(*workshopPriorityEditor_,"Priority",workshopTitle()))return;
        if(!controller_->state().workshop.draft.dirty){closeWorkshopWindow();return;}
        if(controller_->applyWorkshopDraft()) {
            if(controller_->state().workshop.draft.dirty) closeWorkshopWhenApplied_=true; else closeWorkshopWindow();
        }
    });
    bindClick("workshop_cancel",[this]{controller_->revertWorkshopDraft();workshopPriorityEditor_->cancel();closeWorkshopWindow();});
	bindClick( "workshop_queue_once", [this]
			   {if(workshopOrderEditor_->commit())controller_->queueSelectedCraftOrder(); } );
	bindClick( "workshop_trade_refresh", [this]
			   { controller_->refreshTrade(); } );
	bindClick("workshop_trade_set",[this]{const auto& s=controller_->state().workshop;if(!s.selectedTradeRow)return;const auto& rows=s.selectedTradeRow->party==TradeParty::Trader?s.traderRows:s.playerRows;for(const auto& row:rows)if(row.id==*s.selectedTradeRow){auto n=NumericEditor::parse(formValue("workshop_trade_count"),0,static_cast<int>(row.stock));if(n)controller_->setTradeOffer(row.id,*n);else controller_->workshopFeedback("Enter a whole quantity from 0 to "+std::to_string(row.stock)+".");}});
	bindClick( "workshop_trade_execute", [this]
			   { controller_->executeTrade(); } );

	// ---- Agriculture property sheet. Settings edit the pending sheet; plot commands act at once on named plots.
	for ( const auto& [id, pane] : std::array {
		std::pair { "agriculture_view_general", AgriculturePane::General }, std::pair { "agriculture_view_plots", AgriculturePane::Plots },
		std::pair { "agriculture_view_queue", AgriculturePane::PlotQueue }, std::pair { "agriculture_view_crops", AgriculturePane::Crops },
		std::pair { "agriculture_view_animals", AgriculturePane::Animals }, std::pair { "agriculture_view_food", AgriculturePane::Food } } )
		bindClick( id, [this, pane = pane] { controller_->setAgriculturePane( pane ); } );
	bind( "agriculture_workbench", "keydown", [this]( Rml::Event& e )
		  {
			  if ( stockpileDialog_ && stockpileDialog_->active() ) return;
			  const auto key = e.GetParameter<int>( "key_identifier", 0 );
			  auto* target = e.GetTargetElement();
			  const bool onButton = target && ( target->GetTagName() == "button" || target->GetTagName() == "select" );
			  if ( key == Rml::Input::KI_RETURN && !onButton ) { (void)activateElement( "agriculture_ok" ); e.StopPropagation(); }
			  else if ( key == Rml::Input::KI_ESCAPE ) { (void)activateElement( "agriculture_cancel" ); e.StopPropagation(); }
		  } );
	for ( const char* event : { "input", "change" } )
		bind( "agriculture_name", event, [this]( Rml::Event& ) { if ( !renderingAgriculture_ ) controller_->editAgricultureName( formValue( "agriculture_name" ) ); } );
	const auto agricultureBox = [this]( const char* id, bool AgricultureOptions::*field ) {
		bind( id, "change", [this, id, field]( Rml::Event& ) {
			if ( syncingCheckbox_ || renderingAgriculture_ ) return;
			auto options   = controller_->state().agriculture.draft.options;
			options.*field = element( id )->HasAttribute( "checked" );
			controller_->editAgricultureOptions( std::move( options ) );
		} );
	};
	agricultureBox( "agriculture_toggle_suspended", &AgricultureOptions::suspended );
	agricultureBox( "agriculture_toggle_harvest", &AgricultureOptions::harvest );
	agricultureBox( "agriculture_toggle_pasture_harvest", &AgricultureOptions::harvest );
	agricultureBox( "agriculture_toggle_hay", &AgricultureOptions::harvestHay );
	agricultureBox( "agriculture_toggle_tame", &AgricultureOptions::tame );
	agricultureBox( "agriculture_toggle_pick", &AgricultureOptions::pick );
	agricultureBox( "agriculture_toggle_plant", &AgricultureOptions::plant );
	agricultureBox( "agriculture_toggle_fell", &AgricultureOptions::fell );
	agricultureMaleEditor_ = std::make_unique<NumericEditor>( *agriculture_, "agriculture_male_cap", [this]( int n ) { auto o = controller_->state().agriculture.draft.options; o.maxMale = n; controller_->editAgricultureOptions( std::move( o ) ); return true; } );
	agricultureFemaleEditor_ = std::make_unique<NumericEditor>( *agriculture_, "agriculture_female_cap", [this]( int n ) { auto o = controller_->state().agriculture.draft.options; o.maxFemale = n; controller_->editAgricultureOptions( std::move( o ) ); return true; } );
	agriculturePlotCountEditor_ = std::make_unique<NumericEditor>( *agriculture_, "agriculture_plot_count", []( int ) { return true; } );
	// Default crop, tree or animal type: choosing a row in the list (or the drop-down list) is a pending setting.
	const auto chooseProduct = [this]( const std::string& id ) {
		if ( id.empty() || renderingAgriculture_ ) return;
		auto options    = controller_->state().agriculture.draft.options;
		options.product = CatalogId { id };
		controller_->editAgricultureOptions( std::move( options ) );
	};
	bind( "agriculture_farm_catalog", "click", [this, chooseProduct]( Rml::Event& e ) {
		if ( auto* row = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-catalog" ) ) chooseProduct( attr( row, "data-catalog" ) );
	} );
	bind( "agriculture_animal_type", "change", [this, chooseProduct]( Rml::Event& ) {
		const auto value = formValue( "agriculture_animal_type" );
		if ( value != controller_->state().agriculture.draft.options.product.value ) chooseProduct( value );
	} );
	// Plots page: list view selection model (click, Ctrl+click, Shift+click, arrows, Space, Ctrl+A).
	bind( "agriculture_plot_grid", "click", [this]( Rml::Event& e ) {
		auto* cell = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-plot" );
		if ( !cell ) return;
		const auto plot = farmPlotFromKey( attr( cell, "data-plot" ) );
		if ( !plot ) return;
		const bool shift = e.GetParameter<int>( "shift_key", 0 ) != 0, ctrl = e.GetParameter<int>( "ctrl_key", 0 ) != 0;
		controller_->selectFarmPlot( *plot, shift ? PlotSelect::Range : ctrl ? PlotSelect::Toggle : PlotSelect::Only );
	} );
	bind( "agriculture_plot_grid", "keydown", [this]( Rml::Event& e ) {
		const auto key   = e.GetParameter<int>( "key_identifier", 0 );
		const bool shift = e.GetParameter<int>( "shift_key", 0 ) != 0, ctrl = e.GetParameter<int>( "ctrl_key", 0 ) != 0;
		const auto& s    = controller_->state().agriculture;
		if ( key == Rml::Input::KI_LEFT ) controller_->moveFarmPlotFocus( -1, 0, shift );
		else if ( key == Rml::Input::KI_RIGHT ) controller_->moveFarmPlotFocus( 1, 0, shift );
		else if ( key == Rml::Input::KI_UP ) controller_->moveFarmPlotFocus( 0, -1, shift );
		else if ( key == Rml::Input::KI_DOWN ) controller_->moveFarmPlotFocus( 0, 1, shift );
		else if ( key == Rml::Input::KI_SPACE && s.focusedPlot ) controller_->selectFarmPlot( *s.focusedPlot, ctrl ? PlotSelect::Toggle : PlotSelect::Only );
		else if ( key == Rml::Input::KI_A && ctrl ) controller_->selectAllFarmPlots();
		else return;
		e.StopPropagation();
	} );
	bindClick( "agriculture_plot_select_all", [this] { controller_->selectAllFarmPlots(); } );
	bindClick( "agriculture_plot_clear", [this] { controller_->clearFarmPlotSelection(); } );
	bind( "agriculture_plot_crop", "change", [this]( Rml::Event& ) {
		if ( renderingAgriculture_ ) return;
		const auto id  = formValue( "agriculture_plot_crop" );
		const auto& s = controller_->state().agriculture;
		if ( !id.empty() && !( s.selectedProduct && s.selectedProduct->value == id ) ) controller_->selectAgricultureProduct( CatalogId { id } );
	} );
	bindClick( "agriculture_plot_assign", [this] { controller_->assignSelectedFarmPlotCrop(); } );
	bindClick( "agriculture_plot_default", [this] { controller_->useFarmDefaultForSelectedPlots(); } );
	bindClick( "agriculture_plot_queue", [this] {
		if ( !agriculturePlotCountEditor_->commit() ) return;
		if ( auto n = NumericEditor::parse( formValue( "agriculture_plot_count" ), 1, 99 ) ) controller_->queueSelectedFarmPlotCrop( static_cast<std::uint32_t>( *n ), false );
	} );
	bindClick( "agriculture_plot_repeat", [this] { controller_->queueSelectedFarmPlotCrop( 1, true ); } );
	// Plot Queue page: select a planting, then Move Up / Move Down / Remove act on that planting of the one selected plot.
	bind( "agriculture_plot_orders", "click", [this]( Rml::Event& e ) {
		if ( auto* row = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-order" ) )
		{
			agricultureSelectedOrder_ = static_cast<std::uint32_t>( std::stoul( attr( row, "data-order" ) ) );
			renderAgriculture( controller_->state().agriculture );
		}
	} );
	const auto orderCommand = [this]( const char* action ) {
		const auto& s = controller_->state().agriculture;
		if ( s.selectedPlots.size() != 1 || !agricultureSelectedOrder_ ) return;
		if ( std::string_view( action ) == "remove" ) controller_->cancelFarmPlotOrder( s.selectedPlots.front(), *agricultureSelectedOrder_ );
		else controller_->moveFarmPlotOrder( s.selectedPlots.front(), *agricultureSelectedOrder_, std::string_view( action ) == "up" ? MoveDirection::Up : MoveDirection::Down );
	};
	bindClick( "agriculture_order_up", [orderCommand] { orderCommand( "up" ); } );
	bindClick( "agriculture_order_down", [orderCommand] { orderCommand( "down" ); } );
	bindClick( "agriculture_order_remove", [orderCommand] { orderCommand( "remove" ); } );
	// Animals page: a row selects the animal; its check box stages the butchering mark for that animal ID.
	bind( "agriculture_animals", "click", [this]( Rml::Event& e ) {
		if ( auto* row = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-animal" ) )
			controller_->selectAgricultureAnimal( { static_cast<std::uint32_t>( std::stoul( attr( row, "data-animal" ) ) ) } );
	} );
	bind( "agriculture_animals", "change", [this]( Rml::Event& e ) {
		if ( syncingCheckbox_ || renderingAgriculture_ ) return;
		auto* box      = e.GetTargetElement();
		const auto key = attr( box, "data-butcher" );
		if ( key.empty() ) return;
		const auto id = static_cast<std::uint32_t>( std::stoul( key ) );
		auto options  = controller_->state().agriculture.draft.options;
		std::erase( options.butcher, id );
		if ( box->HasAttribute( "checked" ) ) options.butcher.push_back( id );
		controller_->editAgricultureOptions( std::move( options ) );
		e.StopPropagation();
	} );
	// Food page: flat check boxes; each stages the rule for its own item and material.
	bind( "agriculture_foods", "change", [this]( Rml::Event& e ) {
		if ( syncingCheckbox_ || renderingAgriculture_ ) return;
		auto* box      = e.GetTargetElement();
		const auto key = attr( box, "data-food" );
		if ( key.empty() ) return;
		auto options = controller_->state().agriculture.draft.options;
		std::erase( options.foods, key );
		if ( box->HasAttribute( "checked" ) ) options.foods.push_back( key );
		controller_->selectPastureFood( key );
		controller_->editAgricultureOptions( std::move( options ) );
		e.StopPropagation();
	} );
	// Property sheet buttons: OK applies and closes, Cancel discards and closes, Apply applies and stays open.
	// Typed limits are committed first; an unavailable (disabled) spin box has nothing to commit.
	const auto limitsCommitted = [this] {
		const auto& v    = controller_->state().agriculture.value;
		const auto title = captionName( v.name.empty() ? kind( v.target.kind ) : v.name ) + " Properties";
		return committed( *agricultureMaleEditor_, "Males", title ) && committed( *agricultureFemaleEditor_, "Females", title );
	};
	bindClick( "agriculture_apply", [this, limitsCommitted] { if ( limitsCommitted() ) controller_->applyAgricultureDraft(); } );
	bindClick( "agriculture_ok", [this, limitsCommitted] {
		if ( !limitsCommitted() ) return;
		if ( !controller_->state().agriculture.draft.dirty ) { closeAgricultureWindow(); return; }
		if ( controller_->applyAgricultureDraft() )
		{
			if ( controller_->state().agriculture.draft.dirty ) closeAgricultureWhenApplied_ = true;
			else closeAgricultureWindow();
		}
	} );
	bindClick( "agriculture_cancel", [this] {
		controller_->revertAgricultureDraft();
		agricultureMaleEditor_->cancel();
		agricultureFemaleEditor_->cancel();
		closeAgricultureWindow();
	} );
	stateChanged( controller.state() );
	return true;
}
bool Management6ARmlBinding::reloadDocuments()
{
	auto* controller = controller_;
	if ( !controller ) return false;
	shutdown();
	return initialize( *controller );
}
void Management6ARmlBinding::shutdown()
{
    reportOptions_.clear();
    stockpileDialog_.reset();
    workshopPriorityEditor_.reset();workshopOrderEditor_.reset();workshopJobEditor_.reset();
    stockpilePriorityEditor_.reset(); numericStockpileId_=0;stockpileTemplateMarkup_.clear();
	for ( auto& l : listeners_ )
		if ( l.target )
			l.target->RemoveEventListener( l.event, l.callback.get(), l.capture );
	listeners_.clear();
	stockpileContentMarkup_.clear();
	stockpileCategoryOptions_.clear();
	stockpileFilterMarkup_.clear();
	stockpileFilterRows_.clear();
	stockpileFilterFirst_ = static_cast<std::size_t>( -1 );
	shownStockpileMessage_.clear();
	workshopMarkup_.clear();
	workshopTradeEditor_.reset();
	agricultureMaleEditor_.reset();
	agricultureFemaleEditor_.reset();
	agriculturePlotCountEditor_.reset();
	agricultureMarkup_.clear();
	agricultureCropOptions_.clear();
	agricultureAnimalOptions_.clear();
	agricultureSettingsTarget_ = {};
	agricultureSelectedOrder_.reset();
	workshopSettingsId_ = {};
	workshopEditingJob_.reset();
	tradeConfirmationVisible_ = false;
	stockpileTemplateConfirmationVisible_ = false;
	for ( auto** doc : { &agriculture_, &stockpile_, &workshop_ } )
		if ( *doc )
		{
			context_.UnloadDocument( *doc );
			*doc = nullptr;
		}
	controller_ = nullptr;
}
Rml::Element* Management6ARmlBinding::element( const char* id ) const
{
	for ( auto* doc : { workshop_, stockpile_, agriculture_ } )
		if ( doc )
			if ( auto* e = doc->GetElementById( id ) )
				return e;
	return nullptr;
}
void Management6ARmlBinding::bind( const char* id, const char* event, std::function<void( Rml::Event& )> fn, bool capture )
{
	if ( auto* e = element( id ) )
	{
		auto cb = std::make_unique<Callback>( std::move( fn ) );
		e->AddEventListener( event, cb.get(), capture );
		listeners_.push_back( { e, event, std::move( cb ), capture } );
	}
}
void Management6ARmlBinding::text( const char* id, const std::string& value )
{
	if ( auto* e = element( id ) )
		e->SetInnerRML( safe( value ) );
}
void Management6ARmlBinding::visible( const char* id, bool value )
{
	if ( auto* e = element( id ) )
		e->SetClass( "is-hidden", !value );
}
void Management6ARmlBinding::checked( const char* id, bool value )
{
	if ( auto* e = element( id ) )
	{
		if ( e->GetTagName() == "input" && e->HasAttribute( "checked" ) != value )
		{
			syncingCheckbox_ = true;
			if ( value ) e->SetAttribute( "checked", "checked" );
			else e->RemoveAttribute( "checked" );
			syncingCheckbox_ = false;
		}
		e->SetClass( "is-checked", value );
		e->SetClass( "is-unchecked", !value );
	}
}
void Management6ARmlBinding::enabled( const char* id, bool value )
{
	if ( auto* e = element( id ) )
	{
		if ( value )
			e->RemoveAttribute( "disabled" );
		else
			e->SetAttribute( "disabled", "" );
	}
}
std::string Management6ARmlBinding::formValue( const char* id ) const
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( id ) ) )
		return e->GetValue();
	return {};
}
void Management6ARmlBinding::formValue( const char* id, const std::string& value )
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( id ) ); e && context_.GetFocusElement() != e )
		e->SetValue( value );
}
std::int32_t Management6ARmlBinding::priority( const char* id, std::int32_t fallback, std::int32_t maximum ) const
{
	const auto value = formValue( id );
	std::int32_t out {};
	const auto result = std::from_chars( value.data(), value.data() + value.size(), out );
	return result.ec == std::errc {} && result.ptr == value.data() + value.size() ? std::clamp( out, 1, std::max( 1, maximum ) ) : fallback;
}
std::int32_t Management6ARmlBinding::normalizePriority( const char* id, std::int32_t fallback, std::int32_t maximum )
{
	const auto value = priority( id, fallback, maximum );
	const auto normalized = std::to_string( value );
	if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( id ) ) )
	{
		if ( input->GetValue() != normalized )
		{
			normalizingPriority_ = true;
			input->SetValue( normalized );
			normalizingPriority_ = false;
		}
	}
	return value;
}
void Management6ARmlBinding::closeWorkshopWindow()
{
	closeWorkshopWhenApplied_ = false;
	controller_->close();
	if ( closeHandler_ ) closeHandler_();
}
bool Management6ARmlBinding::committed( NumericEditor& editor, const char* field, const std::string& title )
{
	if ( editor.disabled() || editor.commit() ) return true;
	if ( stockpileDialog_ && !stockpileDialog_->active() )
		stockpileDialog_->show( title, "Type a whole number from " + std::to_string( editor.minimum() ) + " to " + std::to_string( editor.maximum() ) + " for " + field + ".", "", "OK", [] {}, [] {} );
	return false;
}
void Management6ARmlBinding::closeStockpileWindow()
{
	closeStockpileWhenApplied_ = false;
	controller_->close();
	if ( closeHandler_ ) closeHandler_();
}
void Management6ARmlBinding::closeAgricultureWindow()
{
	closeAgricultureWhenApplied_ = false;
	controller_->close();
	if ( closeHandler_ ) closeHandler_();
}
bool Management6ARmlBinding::canClose()
{
	if ( stockpileDialog_ && stockpileDialog_->active() ) return false;
	if ( controller_ && controller_->state().view == ManagementView::Agriculture && controller_->state().agriculture.draft.dirty )
	{
		if ( controller_->state().agriculture.draft.pending ) return false;
		// The Close button is not Cancel: with pending changes, ask whether to apply them.
		const auto& value = controller_->state().agriculture.value;
		const auto name   = value.name.empty() ? kind( value.target.kind ) : value.name;
		stockpileDialog_->show( captionName( name ) + " Properties", "Do you want to apply the changes you made to " + name + "?", "Yes", "Cancel",
			[this] { if ( controller_->applyAgricultureDraft() ) { if ( controller_->state().agriculture.draft.dirty ) closeAgricultureWhenApplied_ = true; else closeAgricultureWindow(); } },
			[] {},
			"No", [this] { controller_->revertAgricultureDraft(); closeAgricultureWindow(); } );
		return false;
	}
	if ( controller_ && controller_->state().view == ManagementView::Stockpile && controller_->state().stockpile.draft.dirty )
	{
		if ( controller_->state().stockpile.draft.pending ) return false;
		// The Close button is not Cancel: with pending changes, ask whether to apply them.
		const auto& value = controller_->state().stockpile.value;
		const auto name   = value.name.empty() ? std::string( "Stockpile" ) : value.name;
		stockpileDialog_->show( captionName( name ) + " Properties", "Do you want to apply the changes you made to " + name + "?", "Yes", "Cancel",
			[this, name] { if ( committed( *stockpilePriorityEditor_, "Priority", captionName( name ) + " Properties" ) && controller_->applyStockpileDraft() ) { if ( controller_->state().stockpile.draft.dirty ) closeStockpileWhenApplied_ = true; else closeStockpileWindow(); } },
			[] {},
			"No", [this] { controller_->revertStockpileDraft(); stockpilePriorityEditor_->cancel(); closeStockpileWindow(); } );
		return false;
	}
	if ( !controller_ || controller_->state().view != ManagementView::Workshop || !controller_->state().workshop.draft.dirty ) return true;
	if ( controller_->state().workshop.draft.pending ) return false;
	// The Close button is not Cancel: with pending changes, ask whether to apply them.
	const auto& name = controller_->state().workshop.value.name;
	stockpileDialog_->show( captionName( name ) + " Properties", "Do you want to apply the changes you made to " + name + "?", "Yes", "Cancel",
		[this] { if ( controller_->applyWorkshopDraft() ) { if ( controller_->state().workshop.draft.dirty ) closeWorkshopWhenApplied_ = true; else closeWorkshopWindow(); } },
		[] {},
		"No", [this] { controller_->revertWorkshopDraft(); closeWorkshopWindow(); } );
	return false;
}
bool Management6ARmlBinding::activateElement( std::string_view id )
{
    if(id=="stockpile_review_accept" || id=="stockpile_review_cancel" || id=="stockpile_review_alternate" || id=="workshop_review_accept" || id=="workshop_review_cancel" || id=="workshop_review_alternate"
       || id=="agriculture_review_accept" || id=="agriculture_review_cancel" || id=="agriculture_review_alternate") {
        if(!stockpileDialog_ || !stockpileDialog_->active())return false;
        stockpileDialog_->document()->GetElementById(id.ends_with("_accept")?"confirm-accept":id.ends_with("_alternate")?"confirm-alternate":"confirm-cancel")->Click();return true;
    }
	// Automation alias: toggles the selected allow-list rule (pending until Apply), as Space does.
	if ( id == "stockpile_toggle_filter" && controller_ && controller_->state().stockpile.selectedFilter )
	{
		controller_->toggleSelectedStockpileFilter();
		return true;
	}
	if ( auto* e = element( std::string( id ).c_str() ) )
	{
		e->DispatchEvent( "click", Rml::Dictionary {} );
		return true;
	}
	return false;
}
bool Management6ARmlBinding::setFormValueForProbe( std::string_view id, std::string_view value )
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( std::string( id ).c_str() ) ) )
	{
		e->SetValue( std::string( value ) );
		if(id=="stockpile_name" || id=="stockpile_priority" || id=="stockpile_template_name" || id=="stockpile_content_search" || id=="stockpile_allow_search") e->DispatchEvent("input",Rml::Dictionary{});
		if(id=="workshop_priority" || id=="workshop_name") e->DispatchEvent("input",Rml::Dictionary{});
        if(id=="workshop_order_count" || id=="workshop_job_count")e->DispatchEvent("change",Rml::Dictionary{});
		if(id=="agriculture_name") e->DispatchEvent("input",Rml::Dictionary{});
		if(id=="agriculture_plot_count" || id=="agriculture_male_cap" || id=="agriculture_female_cap") e->DispatchEvent("change",Rml::Dictionary{});
		return true;
	}
	return false;
}
bool Management6ARmlBinding::setStockpileSearchForProbe( std::string_view value )
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_allow_search" ) ) )
	{
		e->Focus();
		e->SetValue( std::string( value ) );
		e->DispatchEvent( "input", Rml::Dictionary {} );
		return controller_ && controller_->state().stockpile.filterSearch == std::string( value ) && context_.GetFocusElement() == e;
	}
	return false;
}
bool Management6ARmlBinding::activateFirstStockpileFilterForProbe( TriState state, FilterDepth depth )
{
	if ( !controller_ )
		return false;
	for ( const auto& row : controller_->state().stockpile.visibleFilters )
	{
		if ( ( controller_->stockpileRuleAllowed( row ) ? TriState::On : TriState::Off ) != state || row.id.depth != depth )
			continue;
		if ( auto* e = element( stockpileFilterRowId( row.id ).c_str() ) )
		{
			e->DispatchEvent( "click", Rml::Dictionary {} );
			return true;
		}
	}
	return false;
}
bool Management6ARmlBinding::activateStockpileFilterForProbe( std::string_view item, std::string_view material )
{
	if ( !controller_ )
		return false;
	for ( const auto& row : controller_->state().stockpile.visibleFilters )
	{
		if ( row.id.depth != FilterDepth::Material || row.id.item.value != item || row.id.material.value != material || !controller_->stockpileRuleAllowed( row ) )
			continue;
		if ( auto* e = element( stockpileFilterRowId( row.id ).c_str() ) )
		{
			e->DispatchEvent( "click", Rml::Dictionary {} );
			return true;
		}
	}
	return false;
}
bool Management6ARmlBinding::dispatchStockpileFilterKeyForProbe( int keyIdentifier )
{
	if ( !controller_ || !controller_->state().stockpile.selectedFilter )
		return false;
	if ( auto* row = element( stockpileFilterRowId( *controller_->state().stockpile.selectedFilter ).c_str() ) )
	{
		row->Focus();
		Rml::Dictionary parameters;
		parameters["key_identifier"] = keyIdentifier;
		row->DispatchEvent( "keydown", parameters );
		return true;
	}
	return false;
}

void Management6ARmlBinding::workshopMarkup( const char* id, const std::string& markup )
{
	// Keep live controls, focus and scroll stable across simulation snapshots.
	if ( auto* e = element( id ); e && workshopMarkup_[id] != markup )
	{
		workshopMarkup_[id] = markup;
		e->SetInnerRML( markup );
	}
}
void Management6ARmlBinding::renderWorkshop( const WorkshopState& s )
{
	renderingWorkshop_ = true;
	const auto status = s.request.status;
	visible( "workshop_loading", status == RequestStatus::Loading );
	visible( "workshop_empty", status == RequestStatus::Empty );
	visible( "workshop_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "workshop_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "workshop_error", s.request.message );
	// Property sheet caption: object name followed by "Properties".
	text( "workshop_title", captionName( s.value.name.empty() ? textCatalog_.format( LocalizationKey{"workshop.title"} ) : s.value.name ) + " Properties" );

	const bool crafting = workshopSupportsCrafting( s.value );
	const bool linking  = workshopSupportsStockpileLinks( s.value );
	const bool trade    = workshopSupportsTrade( s.value );
	const bool butcher  = s.value.subtype == "Butcher";
	const bool fisher   = s.value.subtype == "Fisher" || s.value.subtype == "Fishery";
	visible( "workshop_view_craft", crafting );
	visible( "workshop_view_queue", crafting );
	visible( "workshop_view_stockpiles", linking );
	visible( "workshop_view_trade", trade );
	text( "workshop_view_queue", "Queue (" + std::to_string( s.value.queue.size() ) + ")" );
	for ( const auto& [name, pane] : std::array {
		std::pair { "craft", WorkshopPane::Craft }, std::pair { "queue", WorkshopPane::Queue }, std::pair { "settings", WorkshopPane::Settings },
		std::pair { "stockpiles", WorkshopPane::Stockpiles }, std::pair { "trade", WorkshopPane::Trade } } )
	{
		visible( ( std::string( "workshop_" ) + name + "_pane" ).c_str(), s.pane == pane );
		if ( auto* tab = element( ( std::string( "workshop_view_" ) + name ).c_str() ) )
		{
			tab->SetClass( "is-selected", s.pane == pane );
			tab->SetAttribute( "aria-selected", s.pane == pane ? "true" : "false" );
		}
	}
	connected_tabs::select( *element( "workshop_tabs" ), element( s.pane == WorkshopPane::Craft ? "workshop_view_craft" : s.pane == WorkshopPane::Queue ? "workshop_view_queue"
		: s.pane == WorkshopPane::Trade ? "workshop_view_trade" : s.pane == WorkshopPane::Stockpiles ? "workshop_view_stockpiles" : "workshop_view_settings" ) );

	// ---- General page: every value shown is the pending sheet value.
	const bool changedWorkshop = workshopSettingsId_ != s.value.id;
	if ( changedWorkshop )
	{
		for ( const auto& field : { std::pair { "workshop_name", s.draft.name }, std::pair { "workshop_priority", s.draft.priority } } )
			if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( field.first ) ) ) input->SetValue( field.second );
		workshopEditingJob_.reset();
		editingTrade_.reset();
	}
	formValue( "workshop_name", s.draft.name );
	formValue( "workshop_priority", s.draft.priority );
	workshopPriorityEditor_->sync( NumericEditor::parse( s.draft.priority, 1, std::max( 1, s.value.maxPriority ) ).value_or( s.value.priority + 1 ), 1, std::max( 1, s.value.maxPriority ) );
	if ( changedWorkshop ) workshopPriorityEditor_->cancel();
	workshopSettingsId_ = s.value.id;
	text( "workshop_priority_range", "(1 is the highest, " + std::to_string( std::max( 1, s.value.maxPriority ) ) + " the lowest)" );
	const auto& o = s.draft.options;
	checked( "workshop_toggle_suspended", o.suspended );
	checked( "workshop_toggle_generated", o.acceptGenerated );
	checked( "workshop_toggle_auto_missing", o.autoCraftMissing );
	checked( "workshop_toggle_corpses", o.butcherCorpses );
	checked( "workshop_toggle_excess", o.butcherExcess );
	checked( "workshop_toggle_catch", o.catchFish );
	checked( "workshop_toggle_process", o.processFish );
	for ( const char* id : { "workshop_name", "workshop_priority", "workshop_toggle_suspended", "workshop_toggle_generated", "workshop_toggle_auto_missing",
			  "workshop_toggle_corpses", "workshop_toggle_excess", "workshop_toggle_catch", "workshop_toggle_process" } )
		enabled( id, !s.draft.pending );
	visible( "workshop_production_section", crafting );
	visible( "workshop_butcher_actions", butcher );
	visible( "workshop_fisher_actions", fisher );
	visible( "workshop_special", butcher || fisher );
	enabled( "workshop_apply", s.draft.dirty && !s.draft.pending );
	enabled( "workshop_ok", !s.draft.pending );

	// ---- Stockpiles page: multiple-selection list box drawn as flat check boxes.
	{
		std::string links;
		for ( const auto& row : s.value.stockpiles )
		{
			const auto id      = std::to_string( row.id.value );
			const bool checkedRow = std::find( o.linked.begin(), o.linked.end(), row.id.value ) != o.linked.end();
			links += "<label class='w98-list-item' for='workshop_link_" + id + "'><input id='workshop_link_" + id + "' class='checkbox' type='checkbox' data-link-id='" + id + "'"
				+ ( checkedRow ? " checked='checked'" : "" ) + ( s.draft.pending ? " disabled='disabled'" : "" ) + "/><span>" + safe( row.name ) + "</span></label>";
		}
		if ( links.empty() ) links = "<div class='w98-list-empty'>There are no stockpiles.</div>";
		auto* list    = element( "workshop_linked_stockpiles" );
		auto* focused = context_.GetFocusElement();
		const std::string focusedLink = list && focused && list->Contains( focused ) ? focused->GetId() : std::string();
		syncingCheckbox_ = true;
		workshopMarkup( "workshop_linked_stockpiles", links );
		syncingCheckbox_ = false;
		if ( !focusedLink.empty() )
			if ( auto* box = element( focusedLink.c_str() ) ) box->Focus();
	}

	// ---- Craft page.
	std::string products;
	for ( const auto& r : s.visibleProducts )
	{
		const bool selected = s.selectedProduct && *s.selectedProduct == r.id;
		products += "<button id='" + rowId( "m6a_product_", r.id.value ) + "' class='w98-list-item" + std::string( selected ? " is-selected" : "" ) + "' data-catalog='" + safe( r.id.value ) + "'><span class='w98-cell w98-cell--grow'>" + safe( displayCatalogLabel( r.id.value ) ) + "</span></button>";
	}
	if ( products.empty() ) products = "<div class='w98-list-empty'>No crafts match the text.</div>";
	if ( auto* e = element( "workshop_products" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		workshopMarkup( "workshop_products", products );
		if ( restore && s.selectedProduct )
			if ( auto* row = element( rowId( "m6a_product_", s.selectedProduct->value ).c_str() ) ) row->Focus();
	}
	const auto product = s.selectedProduct ? std::find_if( s.value.products.begin(), s.value.products.end(), [&]( const auto& row ) { return row.id == *s.selectedProduct; } ) : s.value.products.end();
	const bool hasProduct = product != s.value.products.end();
	text( "workshop_product_title", hasProduct ? displayCatalogLabel( product->id.value ) + ( s.selectionFiltered ? " (hidden by the search text)" : "" ) : "Select a craft." );
	if ( auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( element( "workshop_order_mode" ) ) )
	{
		const std::string wanted = s.orderMode == CraftRepeatMode::Maintain ? "maintain" : s.orderMode == CraftRepeatMode::Repeat ? "repeat" : "once";
		if ( select->GetValue() != wanted ) select->SetValue( wanted );
	}
	enabled( "workshop_order_mode", hasProduct );
	// Materials: a label and drop-down list per component. The select subtree is only rebuilt when the
	// recipe changes; rebuilding it inside a change event would free the open drop-down.
	std::string materials;
	if ( hasProduct )
		for ( std::size_t index = 0; index < product->components.size(); ++index )
		{
			const auto& component = product->components[index];
			materials += "<div class='w98-line'><label class='w98-label' for='workshop_material_" + std::to_string( index ) + "'>" + safe( displayCatalogLabel( component.item.value ) ) + " x" + std::to_string( component.amount ) + ":</label><select id='workshop_material_" + std::to_string( index ) + "' class='w98-select' data-material-index='" + std::to_string( index ) + "'>";
			for ( const auto& choice : component.materials )
				materials += "<option value='" + safe( choice.first.value ) + "'>" + safe( displayCatalogLabel( choice.first.value ) ) + "</option>";
			materials += "</select></div>";
		}
	workshopMarkup( "workshop_product_selection", materials );
	std::string shortages;
	if ( hasProduct )
		for ( std::size_t index = 0; index < product->components.size(); ++index )
		{
			const auto& component = product->components[index];
			const auto material   = index < s.orderMaterials.size() ? s.orderMaterials[index] : CatalogId { "any" };
			auto* select          = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( element( ( "workshop_material_" + std::to_string( index ) ).c_str() ) );
			if ( !select ) continue;
			for ( std::size_t n = 0; n < component.materials.size(); ++n )
				if ( auto* option = select->GetOption( static_cast<int>( n ) ) )
				{
					const auto& choice = component.materials[n];
					const auto label   = safe( displayCatalogLabel( choice.first.value ) ) + " (" + std::to_string( choice.second ) + " in stock)";
					if ( option->GetInnerRML() != label ) option->SetInnerRML( label );
				}
			if ( select->GetValue() != material.value ) select->SetValue( material.value );
			select->SetSelection( select->GetSelection() );
			const auto choice   = std::find_if( component.materials.begin(), component.materials.end(), [&]( const auto& entry ) { return entry.first == material; } );
			const bool shortage = choice == component.materials.end() || choice->second < component.amount;
			if ( shortage ) shortages += ( shortages.empty() ? "" : ", " ) + displayCatalogLabel( component.item.value );
		}
	workshopOrderEditor_->sync( s.orderCount, 1, 999 );
	if ( changedWorkshop ) workshopOrderEditor_->cancel();
	enabled( "workshop_order_count", hasProduct && s.orderMode != CraftRepeatMode::Repeat );
	text( "workshop_order_count_label", s.orderMode == CraftRepeatMode::Maintain ? "Limit:" : "Quantity:" );
	text( "workshop_order_help", !hasProduct ? "" : ( shortages.empty() ? std::string() : "Not enough " + shortages + " in stock; the order will wait. " )
		+ ( s.orderMode == CraftRepeatMode::Maintain ? "Crafts until the stock reaches this limit." : s.orderMode == CraftRepeatMode::Repeat ? "Crafts until you suspend or cancel it." : "Crafts this many, then finishes." ) );
	text( "workshop_queue_once", s.orderPending ? "Adding..." : "Add Order" );
	enabled( "workshop_queue_once", !s.orderPending && hasProduct );
	text( "workshop_order_feedback", s.orderFeedback );

	// ---- Queue page: list view in details view.
	std::string queue;
	for ( std::size_t n = 0; n < s.visibleQueue.size(); ++n )
	{
		const auto& r       = s.visibleQueue[n];
		const bool selected = s.selectedJob && *s.selectedJob == r.id;
		const std::string type = r.mode == CraftRepeatMode::Maintain ? "Stock limit" : r.mode == CraftRepeatMode::Repeat ? "Repeat" : "Craft number";
		queue += "<button id='m6a_job_" + std::to_string( r.id.value ) + "' class='w98-list-item" + std::string( selected ? " is-selected" : "" ) + "' data-job='" + std::to_string( r.id.value ) + "'>"
			+ "<span class='w98-cell w98-cell--grow'>" + std::to_string( n + 1 ) + ". " + safe( displayCatalogLabel( r.craft.value ) ) + "</span>"
			+ "<span class='w98-cell w98-cell--num w98-w-amount'>" + ( r.mode == CraftRepeatMode::Repeat ? std::string( "-" ) : std::to_string( r.count ) ) + "</span>"
			+ "<span class='w98-cell w98-w-state'>" + ( r.suspended ? "Suspended" : "Active" ) + "</span></button>";
	}
	if ( queue.empty() ) queue = "<div class='w98-list-empty'>There are no orders. Add one on the Craft page.</div>";
	if ( auto* e = element( "workshop_queue" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		workshopMarkup( "workshop_queue", queue );
		if ( restore && s.selectedJob )
			if ( auto* row = element( ( "m6a_job_" + std::to_string( s.selectedJob->value ) ).c_str() ) ) row->Focus();
	}
	const auto job = s.selectedJob ? std::find_if( s.value.queue.begin(), s.value.queue.end(), [&]( const auto& value ) { return value.id == *s.selectedJob; } ) : s.value.queue.end();
	const bool hasJob = job != s.value.queue.end();
	for ( const char* id : { "workshop_job_mode", "workshop_job_count", "workshop_job_move_back", "workshop_job_apply",
			  "workshop_job_top", "workshop_job_up", "workshop_job_down", "workshop_job_bottom", "workshop_job_suspend", "workshop_job_cancel" } )
		enabled( id, hasJob );
	text( "workshop_job_selection", hasJob ? displayCatalogLabel( job->craft.value ) + ( job->mode == CraftRepeatMode::Maintain ? " (stock limit)" : job->mode == CraftRepeatMode::Repeat ? " (repeat)" : "" ) : "Select an order." );
	text( "workshop_job_suspend", hasJob && job->suspended ? "Resume" : "Suspend" );
	if ( hasJob )
	{
		// Load the order's values when the selection or its authoritative values change; keep local edits otherwise.
		const auto signature = std::tuple { job->id.value, static_cast<int>( job->mode ), job->count, job->moveBack };
		if ( workshopEditingJob_ != s.selectedJob || signature != workshopJobSignature_ )
		{
			workshopEditingJob_   = s.selectedJob;
			workshopEditingCount_ = job->count;
			workshopJobSignature_ = signature;
			if ( auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( element( "workshop_job_mode" ) ) )
				select->SetValue( job->mode == CraftRepeatMode::Maintain ? "maintain" : job->mode == CraftRepeatMode::Repeat ? "repeat" : "once" );
			if ( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "workshop_job_count" ) ) ) control->SetValue( std::to_string( job->count ) );
			workshopJobEditor_->cancel();
			checked( "workshop_job_move_back", job->moveBack );
		}
		workshopJobEditor_->sync( static_cast<int>( job->count ), 1, 999 );
		std::string materialsText = "Crafted so far: " + std::to_string( job->alreadyCrafted ) + ". Materials: ";
		for ( std::size_t index = 0; index < job->materials.size(); ++index )
			materialsText += ( index ? ", " : "" ) + displayCatalogLabel( job->materials[index].value );
		text( "workshop_job_help", materialsText + "." );
	}
	else
	{
		workshopEditingJob_.reset();
		text( "workshop_job_help", "" );
	}

	// ---- Trade page: two list views, merchant goods and settlement goods.
	auto tradeRowId = []( const TradeRowId& r )
	{ return std::string( "m6a_trade_" ) + ( r.party == TradeParty::Trader ? "trader" : "player" ) + "_" + hex( r.item.value ) + "_" + hex( r.materialOrGender.value ) + "_" + std::to_string( r.quality ); };
	const auto tradeList = [&]( const char* listId, const auto& rows, const char* party, const char* empty )
	{
		std::string markup;
		for ( const auto& r : rows )
		{
			const bool selected = s.selectedTradeRow && *s.selectedTradeRow == r.id;
			markup += "<button id='" + tradeRowId( r.id ) + "' class='w98-list-item" + std::string( selected ? " is-selected" : "" ) + "' data-party='" + std::string( party ) + "' data-item='" + safe( r.id.item.value ) + "' data-material='" + safe( r.id.materialOrGender.value ) + "' data-quality='" + std::to_string( r.id.quality ) + "'>"
				+ "<span class='w98-cell w98-cell--grow'>" + safe( r.name ) + "</span><span class='w98-cell w98-cell--num w98-w-stock'>" + std::to_string( r.stock ) + "</span>"
				+ "<span class='w98-cell w98-cell--num w98-w-offer'>" + std::to_string( r.offered ) + "</span><span class='w98-cell w98-cell--num w98-w-value'>" + std::to_string( r.unitValue ) + "</span></button>";
		}
		if ( markup.empty() ) markup = "<div class='w98-list-empty'>" + std::string( s.tradeLoaded ? empty : "Loading..." ) + "</div>";
		if ( auto* e = element( listId ) )
		{
			const bool restore = e->Contains( context_.GetFocusElement() );
			workshopMarkup( listId, markup );
			if ( restore && s.selectedTradeRow )
				if ( auto* row = element( tradeRowId( *s.selectedTradeRow ).c_str() ) ) row->Focus();
		}
	};
	tradeList( "workshop_trade_merchant", s.traderRows, "trader", "The merchant has nothing to sell." );
	tradeList( "workshop_trade_settlement", s.playerRows, "player", "The settlement has nothing to trade." );
	const auto balance = static_cast<long long>( s.playerOfferValue ) - s.traderOfferValue;
	text( "workshop_trade_totals", "Receive " + std::to_string( s.traderOfferValue ) + ", give " + std::to_string( s.playerOfferValue ) + ", balance " + ( balance > 0 ? "+" : "" ) + std::to_string( balance ) + "." );
	bool anyOffer = false;
	for ( const auto& row : s.traderRows ) anyOffer |= row.offered > 0;
	for ( const auto& row : s.playerRows ) anyOffer |= row.offered > 0;
	const bool reviewable = s.tradeLoaded && anyOffer && balance >= 0;
	text( "workshop_trade_blocked", !s.tradeLoaded || s.tradePending ? "" : !anyOffer ? "Offer at least one item." : balance < 0 ? "The merchant wants " + std::to_string( -balance ) + " more value." : "" );
	const TradeRow* selected = nullptr;
	for ( const auto& row : s.traderRows ) if ( s.selectedTradeRow == row.id ) selected = &row;
	for ( const auto& row : s.playerRows ) if ( s.selectedTradeRow == row.id ) selected = &row;
	text( "workshop_trade_selection", selected ? std::string( selected->id.party == TradeParty::Trader ? "Receive " : "Give " ) + selected->name + " (" + std::to_string( selected->stock ) + " available)" : "Select an item in either list." );
	if ( editingTrade_ != s.selectedTradeRow )
	{
		editingTrade_ = s.selectedTradeRow;
		if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "workshop_trade_count" ) ) ) input->SetValue( selected ? std::to_string( selected->offered ) : "0" );
		workshopTradeEditor_->cancel();
		if ( selected )
			if ( auto* row = element( tradeRowId( selected->id ).c_str() ) ) row->ScrollIntoView( Rml::ScrollIntoViewOptions { Rml::ScrollAlignment::Nearest } );
	}
	workshopTradeEditor_->sync( selected ? static_cast<int>( selected->offered ) : 0, 0, selected ? static_cast<int>( selected->stock ) : 0 );
	for ( const char* id : { "workshop_trade_count", "workshop_trade_set" } )
		enabled( id, selected && !s.tradePending && !s.tradeConfirmationRequired );
	enabled( "workshop_trade_execute", reviewable && !s.tradePending && !s.tradeConfirmationRequired );
	if ( s.tradeConfirmationRequired && !stockpileDialog_->active() )
	{
		const auto list = []( const std::vector<TradeRow>& rows ) { std::string out; for ( const auto& row : rows ) if ( row.offered ) out += ( out.empty() ? "" : ", " ) + std::to_string( row.offered ) + " x " + row.name; return out.empty() ? std::string( "nothing" ) : out; };
		const std::string detail = "You receive " + list( s.traderRows ) + " (value " + std::to_string( s.traderOfferValue ) + "). You give " + list( s.playerRows ) + " (value " + std::to_string( s.playerOfferValue ) + "). This exchange cannot be undone. Do you want to trade?";
		stockpileDialog_->show( "Review Trade", detail, "Trade", "Cancel", [this] { controller_->confirmTrade(); }, [this] { controller_->cancelTrade(); } );
	}
	else if ( !s.tradeConfirmationRequired && tradeConfirmationVisible_ && stockpileDialog_->active() )
		stockpileDialog_->close();

	// ---- Messages use a message box; OK closes the sheet once the applied values are confirmed.
	if ( !s.feedback.empty() && s.feedback != shownWorkshopMessage_ && !stockpileDialog_->active() )
	{
		shownWorkshopMessage_ = s.feedback;
		stockpileDialog_->show( captionName( s.value.name.empty() ? std::string( "Workshop" ) : s.value.name ) + " Properties", s.feedback, "", "OK", [] {}, [this] { controller_->workshopFeedback( "" ); } );
	}
	if ( s.feedback.empty() ) shownWorkshopMessage_.clear();
	if ( closeWorkshopWhenApplied_ && !s.draft.pending )
	{
		if ( s.draft.dirty ) closeWorkshopWhenApplied_ = false; // rejected or conflicting: stay open
		else pendingWorkshopClose_ = true;
	}
	renderingWorkshop_ = false;
}
void Management6ARmlBinding::bindStockpile()
{
	// ---- Stockpile property sheet (Stage 21a). Settings and Allow List check boxes edit the pending sheet;
	// nothing reaches the game until Apply or OK. Templates act at once on the applied allow list.
	bind( "stockpile_workbench", "keydown", [this]( Rml::Event& e )
		  {
			  if ( stockpileDialog_ && stockpileDialog_->active() ) return;
			  const auto key = e.GetParameter<int>( "key_identifier", 0 );
			  auto* target = e.GetTargetElement();
			  const bool onControl = target && ( target->GetTagName() == "button" || target->GetTagName() == "select" );
			  if ( key == Rml::Input::KI_RETURN && !onControl ) { (void)activateElement( "stockpile_ok" ); e.StopPropagation(); }
			  else if ( key == Rml::Input::KI_ESCAPE ) { (void)activateElement( "stockpile_cancel" ); e.StopPropagation(); }
		  } );
	bindClick( "stockpile_view_contents", [this] { controller_->setStockpilePane( StockpilePane::Contents ); } );
	bindClick( "stockpile_view_allow", [this] { controller_->setStockpilePane( StockpilePane::AllowList ); } );
	bindClick( "stockpile_view_settings", [this] { controller_->setStockpilePane( StockpilePane::Settings ); } );
	for ( const char* event : { "input", "change" } )
	{
		bind( "stockpile_content_search", event, [this]( Rml::Event& ) { if ( !renderingStockpile_ ) controller_->setStockpileContentSearch( formValue( "stockpile_content_search" ) ); } );
		bind( "stockpile_allow_search", event, [this]( Rml::Event& )
			  {
				  if ( renderingStockpile_ ) return;
				  if ( auto* list = element( "stockpile_filters" ) ) list->SetScrollTop( 0.f );
				  controller_->setStockpileFilterSearch( formValue( "stockpile_allow_search" ) );
			  } );
	}
	// One category choice filters both lists.
	for ( const char* id : { "stockpile_content_category", "stockpile_allow_category" } )
		bind( id, "change", [this, id]( Rml::Event& )
			  {
				  if ( renderingStockpile_ ) return;
				  const auto value = formValue( id );
				  if ( value == controller_->state().stockpile.filterCategory.value ) return;
				  if ( auto* list = element( "stockpile_filters" ) ) list->SetScrollTop( 0.f );
				  controller_->setStockpileFilterCategory( CatalogId { value } );
			  } );
	for ( const auto& [id, key] : std::array { std::pair { "stockpile_content_sort_item", StockpileSortKey::Item }, std::pair { "stockpile_content_sort_material", StockpileSortKey::Material },
			  std::pair { "stockpile_content_sort_stock", StockpileSortKey::Quantity }, std::pair { "stockpile_content_sort_total", StockpileSortKey::Total } } )
		bindClick( id, [this, key = key] { controller_->setStockpileContentSort( key ); } );
	for ( const auto& [id, key] : std::array { std::pair { "stockpile_allow_sort_item", StockpileSortKey::Item }, std::pair { "stockpile_allow_sort_material", StockpileSortKey::Material },
			  std::pair { "stockpile_allow_sort_group", StockpileSortKey::Group } } )
		bindClick( id, [this, key = key] { controller_->setStockpileAllowSort( key ); } );
	const auto ruleOf = [this]( Rml::Element* row ) {
		return StockpileFilterRowId { controller_->state().stockpile.value.id, CatalogId { attr( row, "data-category" ) }, CatalogId { attr( row, "data-group" ) },
			CatalogId { attr( row, "data-item" ) }, CatalogId { attr( row, "data-material" ) }, FilterDepth::Material };
	};
	// A click selects the row; the check box toggles the rule's pending state.
	bind( "stockpile_filters", "click", [this, ruleOf]( Rml::Event& e )
		  {
			  auto* row = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-rule" );
			  if ( !row ) return;
			  controller_->selectStockpileFilter( ruleOf( row ) );
			  if ( e.GetTargetElement() && e.GetTargetElement()->GetTagName() == "input" ) controller_->toggleSelectedStockpileFilter();
			  e.StopPropagation();
		  } );
	bind( "stockpile_filters", "keydown", [this, ruleOf]( Rml::Event& e )
		  {
			  const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
			  if ( auto* row = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-rule" ) ) controller_->selectStockpileFilter( ruleOf( row ) );
			  if ( key == Rml::Input::KI_UP || key == Rml::Input::KI_DOWN || key == Rml::Input::KI_HOME || key == Rml::Input::KI_END || key == Rml::Input::KI_PRIOR || key == Rml::Input::KI_NEXT )
			  {
				  const int page = std::max( 1, static_cast<int>( element( "stockpile_filters" )->GetClientHeight() / std::max( 1.f, stockpileRowHeight_ ) ) - 1 );
				  const auto delta = key == Rml::Input::KI_UP ? -1 : key == Rml::Input::KI_DOWN ? 1 : key == Rml::Input::KI_PRIOR ? -page : key == Rml::Input::KI_NEXT ? page
					  : key == Rml::Input::KI_HOME ? -2147483647 : 2147483647;
				  controller_->moveStockpileFilterSelection( delta );
			  }
			  else if ( key == Rml::Input::KI_SPACE ) controller_->toggleSelectedStockpileFilter();
			  else return;
			  e.StopPropagation();
			  if ( controller_->state().stockpile.selectedFilter )
				  if ( auto* row = element( stockpileFilterRowId( *controller_->state().stockpile.selectedFilter ).c_str() ) ) row->Focus();
		  } );
	bind( "stockpile_filters", "scroll", [this]( Rml::Event& ) { renderStockpileFilterViewport(); } );
	bind( "stockpile_rows", "click", [this]( Rml::Event& e )
		  {
			  auto* t = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-depth" );
			  if ( !t ) return;
			  controller_->selectStockpileContent( { CatalogId { attr( t, "data-category" ) }, CatalogId { attr( t, "data-group" ) }, CatalogId { attr( t, "data-item" ) },
				  CatalogId { attr( t, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( t, "data-depth" ) ) ) } );
			  e.StopPropagation();
		  } );
	bindClick( "stockpile_allow_bulk", [this] { controller_->setStockpileRulesShown( true ); } );
	bindClick( "stockpile_block_bulk", [this] { controller_->setStockpileRulesShown( false ); } );

	// Template: a drop-down combo box (PDF p.140). Choosing a saved name fills the text box; Load and Save act on it.
	for ( const char* event : { "input", "change" } )
		bind( "stockpile_template_name", event, [this]( Rml::Event& ) { if ( !renderingStockpile_ ) controller_->setStockpileTemplateName( formValue( "stockpile_template_name" ) ); } );
	const auto focusTemplateOption = [this]( bool last )
	{
		context_.Update();
		Rml::ElementList options;
		element( "stockpile_template_options" )->GetElementsByTagName( options, "button" );
		if ( !options.empty() ) ( last ? options.back() : options.front() )->Focus( true );
	};
	const auto closeTemplate = [this]( bool restore )
	{
		if ( controller_->state().stockpile.templateMenuOpen ) controller_->toggleStockpileTemplateMenu();
		if ( restore ) element( "stockpile_template_name" )->Focus( true );
	};
	bind( "stockpile_template_name", "keydown", [this, focusTemplateOption, closeTemplate]( Rml::Event& e )
		  {
			  const int key = e.GetParameter<int>( "key_identifier", 0 );
			  if ( key == Rml::Input::KI_DOWN || key == Rml::Input::KI_UP )
			  {
				  if ( !controller_->state().stockpile.templateMenuOpen ) controller_->toggleStockpileTemplateMenu();
				  focusTemplateOption( key == Rml::Input::KI_UP );
			  }
			  else if ( key == Rml::Input::KI_ESCAPE && controller_->state().stockpile.templateMenuOpen ) closeTemplate( true );
			  else return;
			  e.StopPropagation();
		  } );
	bindClick( "stockpile_template_toggle", [this, focusTemplateOption]
			   {
				   controller_->toggleStockpileTemplateMenu();
				   if ( controller_->state().stockpile.templateMenuOpen ) focusTemplateOption( false );
			   } );
	bind( "stockpile_template_options", "click", [this]( Rml::Event& e )
		  {
			  if ( auto* option = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-template" ) )
			  {
				  controller_->setStockpileTemplateName( attr( option, "data-template" ) );
				  if ( controller_->state().stockpile.templateMenuOpen ) controller_->toggleStockpileTemplateMenu();
				  element( "stockpile_template_name" )->Focus( true );
				  e.StopPropagation();
			  }
		  } );
	bind( "stockpile_template_options", "keydown", [this, closeTemplate]( Rml::Event& e )
		  {
			  const int key = e.GetParameter<int>( "key_identifier", 0 );
			  if ( key == Rml::Input::KI_ESCAPE ) { closeTemplate( true ); e.StopPropagation(); return; }
			  if ( key == Rml::Input::KI_TAB ) { closeTemplate( true ); return; }
			  if ( key == Rml::Input::KI_RETURN )
			  {
				  if ( auto* option = context_.GetFocusElement(); option && option->HasAttribute( "data-template" ) )
				  {
					  controller_->setStockpileTemplateName( attr( option, "data-template" ) );
					  closeTemplate( true );
				  }
				  e.StopPropagation();
				  return;
			  }
			  if ( key != Rml::Input::KI_UP && key != Rml::Input::KI_DOWN && key != Rml::Input::KI_HOME && key != Rml::Input::KI_END ) return;
			  Rml::ElementList options;
			  element( "stockpile_template_options" )->GetElementsByTagName( options, "button" );
			  if ( options.empty() ) return;
			  auto found = std::find( options.begin(), options.end(), context_.GetFocusElement() );
			  int index = found == options.end() ? 0 : static_cast<int>( found - options.begin() ), count = static_cast<int>( options.size() );
			  index = key == Rml::Input::KI_HOME ? 0 : key == Rml::Input::KI_END ? count - 1 : std::clamp( index + ( key == Rml::Input::KI_UP ? -1 : 1 ), 0, count - 1 );
			  options[index]->Focus( true );
			  options[index]->ScrollIntoView( Rml::ScrollAlignment::Nearest );
			  e.StopPropagation();
		  } );
	bind( "stockpile_manager_root", "mousedown", [this, closeTemplate]( Rml::Event& e )
		  {
			  if ( !controller_->state().stockpile.templateMenuOpen ) return;
			  for ( auto* node = e.GetTargetElement(); node; node = node->GetParentNode() )
				  if ( node->IsClassSet( "w98-combo" ) || node->GetId() == "stockpile_template_options" ) return;
			  closeTemplate( false );
		  }, true );
	bindClick( "stockpile_template_load", [this]
			   {
				   const auto& s    = controller_->state().stockpile;
				   const auto found = std::ranges::find_if( s.value.templateNames, [&]( const auto& n ) { return foldedLabel( n ) == foldedLabel( s.templateName ); } );
				   if ( found == s.value.templateNames.end() ) return;
				   const auto name = *found, stockpile = s.value.name.empty() ? std::string( "Stockpile" ) : s.value.name;
				   const auto id = s.value.id;
				   const auto revision = s.revision;
				   stockpileDialog_->show( captionName( stockpile ) + " Properties", "Replace the allow list of " + stockpile + " with the saved template '" + name + "'?", "Yes", "No",
					   [this, id, revision, name] {
						   const auto& current = controller_->state().stockpile;
						   if ( current.value.id != id || current.revision != revision ) { controller_->stockpileFeedback( "The allow list changed. Choose Load again to use the template." ); return; }
						   controller_->selectStockpileTemplate( name );
						   element( "stockpile_template_name" )->Focus( true );
					   },
					   [this] { element( "stockpile_template_name" )->Focus( true ); } );
			   } );
	bindClick( "stockpile_template_save", [this]
			   {
				   controller_->setStockpileTemplateName( formValue( "stockpile_template_name" ) );
				   const auto& s = controller_->state().stockpile;
				   const bool exists = std::ranges::any_of( s.value.templateNames, [&]( const auto& n ) { return foldedLabel( n ) == foldedLabel( s.templateName ); } );
				   if ( exists ) controller_->updateStockpileTemplate();
				   else controller_->saveStockpileTemplate();
			   } );

	// General page.
	stockpilePriorityEditor_ = std::make_unique<NumericEditor>( *stockpile_, "stockpile_priority", [this]( int value ) {
		controller_->editStockpileDraft( formValue( "stockpile_name" ), std::to_string( value ) );
		return true; }, -1, "stockpile_priority_up", "stockpile_priority_down" );
	for ( const char* id : { "stockpile_name", "stockpile_priority" } )
		for ( const char* event : { "input", "change" } )
			bind( id, event, [this]( Rml::Event& ) { if ( !projectingStockpile_ ) controller_->editStockpileDraft( formValue( "stockpile_name" ), formValue( "stockpile_priority" ) ); } );
	const auto optionBox = [this]( const char* id, bool StockpileOptions::*field )
	{
		bind( id, "change", [this, id, field]( Rml::Event& ) {
			if ( syncingCheckbox_ || projectingStockpile_ ) return;
			auto options   = controller_->state().stockpile.draft.options;
			options.*field = element( id )->HasAttribute( "checked" );
			controller_->editStockpileOptions( options );
		} );
	};
	optionBox( "stockpile_toggle_pull", &StockpileOptions::pull );
	optionBox( "stockpile_toggle_allow_pull", &StockpileOptions::allowPull );
	optionBox( "stockpile_toggle_suspended", &StockpileOptions::suspended );
	// Property sheet buttons: OK applies and closes, Cancel discards and closes, Apply applies and stays open.
	const auto stockpileTitle = [this] { const auto& n = controller_->state().stockpile.value.name; return captionName( n.empty() ? std::string( "Stockpile" ) : n ) + " Properties"; };
	bindClick( "stockpile_apply", [this, stockpileTitle] { if ( committed( *stockpilePriorityEditor_, "Priority", stockpileTitle() ) ) controller_->applyStockpileDraft(); } );
	bindClick( "stockpile_ok", [this, stockpileTitle]
			   {
				   if ( !committed( *stockpilePriorityEditor_, "Priority", stockpileTitle() ) ) return;
				   if ( !controller_->state().stockpile.draft.dirty ) { closeStockpileWindow(); return; }
				   if ( controller_->applyStockpileDraft() )
				   {
					   if ( controller_->state().stockpile.draft.dirty ) closeStockpileWhenApplied_ = true;
					   else closeStockpileWindow();
				   }
			   } );
	bindClick( "stockpile_cancel", [this] { controller_->revertStockpileDraft(); stockpilePriorityEditor_->cancel(); closeStockpileWindow(); } );
}
void Management6ARmlBinding::renderStockpile( const StockpileState& s )
{
	if ( projectingStockpile_ ) return;
	projectingStockpile_ = renderingStockpile_ = true;
	const auto name = s.value.name.empty() ? std::string( "Stockpile" ) : s.value.name;
	if ( numericStockpileId_ != s.value.id.value )
		for ( const auto& [id, value] : { std::pair { "stockpile_name", s.draft.name }, std::pair { "stockpile_template_name", s.templateName },
				  std::pair { "stockpile_content_search", s.contentSearch }, std::pair { "stockpile_allow_search", s.filterSearch } } )
			if ( auto* field = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( id ) ) ) field->SetValue( value );
	const auto status = s.request.status;
	visible( "stockpile_loading", status == RequestStatus::Loading );
	visible( "stockpile_empty", status == RequestStatus::Empty );
	visible( "stockpile_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "stockpile_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "stockpile_error", s.request.message );
	text( "stockpile_title", captionName( name ) + " Properties" );

	// ---- tabs (the sheet reopens on the page last viewed)
	const auto pane = s.pane;
	connected_tabs::select( *element( "stockpile_tabs" ), element( pane == StockpilePane::Contents ? "stockpile_view_contents" : pane == StockpilePane::AllowList ? "stockpile_view_allow" : "stockpile_view_settings" ) );
	visible( "stockpile_contents_pane", pane == StockpilePane::Contents );
	visible( "stockpile_allow_pane", pane == StockpilePane::AllowList );
	visible( "stockpile_settings_pane", pane == StockpilePane::Settings );

	// ---- Find and Category
	formValue( "stockpile_content_search", s.contentSearch );
	formValue( "stockpile_allow_search", s.filterSearch );
	// Options are rebuilt through the select API only when the categories change (SetInnerRML would append).
	std::vector<std::pair<std::string, std::string>> categories { { "", "(All)" } };
	std::string categoryKey;
	for ( const auto& row : s.value.filters )
		if ( row.id.depth == FilterDepth::Category ) { categories.emplace_back( row.id.category.value, row.label ); categoryKey += row.id.category.value + '\x1f' + row.label + '\x1e'; }
	for ( const char* id : { "stockpile_content_category", "stockpile_allow_category" } )
		if ( auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( element( id ) ) )
		{
			if ( stockpileCategoryOptions_ != categoryKey ) setSelectOptions( select, categories, false );
			if ( select->GetValue() != s.filterCategory.value ) select->SetValue( s.filterCategory.value );
		}
	stockpileCategoryOptions_ = categoryKey;

	// ---- sort marks: the arrow sits on the sorted heading; down means descending (PDF p.143)
	const auto mark = [this]( const char* id, bool shown, bool descending )
	{
		if ( auto* e = element( id ) ) { e->SetClass( "is-hidden", !shown ); e->SetClass( "is-descending", shown && descending ); }
	};
	const bool contentDown = s.sort == SortDirection::Descending, allowDown = s.allowSortDirection == SortDirection::Descending;
	mark( "stockpile_content_mark_item", s.contentSort == StockpileSortKey::Item || s.contentSort == StockpileSortKey::Category || s.contentSort == StockpileSortKey::Group, contentDown );
	mark( "stockpile_content_mark_material", s.contentSort == StockpileSortKey::Material, contentDown );
	mark( "stockpile_content_mark_stock", s.contentSort == StockpileSortKey::Quantity, contentDown );
	mark( "stockpile_content_mark_total", s.contentSort == StockpileSortKey::Total, contentDown );
	mark( "stockpile_allow_mark_item", s.allowSort == StockpileSortKey::Item || s.allowSort == StockpileSortKey::Category || s.allowSort == StockpileSortKey::Status, allowDown );
	mark( "stockpile_allow_mark_material", s.allowSort == StockpileSortKey::Material, allowDown );
	mark( "stockpile_allow_mark_group", s.allowSort == StockpileSortKey::Group, allowDown );

	// ---- Contents: a read-only list view in details view
	std::string rows;
	for ( const auto& r : s.visibleContents )
	{
		const auto labels = stockpilePathLabels( r.id, s.value.filters );
		const auto id = "m6a_content_" + hex( r.id.category.value ) + "_" + hex( r.id.group.value ) + "_" + hex( r.id.item.value ) + "_" + hex( r.id.material.value ) + "_" + std::to_string( static_cast<int>( r.id.depth ) );
		rows += "<button id='" + id + "' class='w98-list-item' role='row' data-category='" + safe( r.id.category.value ) + "' data-group='" + safe( r.id.group.value ) + "' data-item='" + safe( r.id.item.value )
			+ "' data-material='" + safe( r.id.material.value ) + "' data-depth='" + std::to_string( static_cast<int>( r.id.depth ) ) + "'><span class='w98-cell w98-cell--grow'>" + safe( labels.item )
			+ "</span><span class='w98-cell l-sp-w-material'>" + safe( labels.material ) + "</span><span class='w98-cell w98-cell--num l-sp-w-number'>" + std::to_string( r.stockpiled )
			+ "</span><span class='w98-cell w98-cell--num l-sp-w-number'>" + std::to_string( r.total ) + "</span></button>";
	}
	std::size_t storedEntries = 0;
	for ( std::size_t index = 0; index < s.value.contents.size(); ++index )
		if ( index + 1 >= s.value.contents.size() || s.value.contents[index + 1].id.depth <= s.value.contents[index].id.depth ) ++storedEntries;
	if ( rows.empty() ) rows = "<div class='w98-list-empty'>" + std::string( storedEntries == 0 ? "Nothing is stored here." : "No items match the text." ) + "</div>";
	if ( auto* list = element( "stockpile_rows" ) )
	{
		const bool restore = list->Contains( context_.GetFocusElement() );
		if ( rows != stockpileContentMarkup_ ) { stockpileContentMarkup_ = rows; list->SetInnerRML( rows ); }
		for ( const auto& r : s.visibleContents )
		{
			const auto id = "m6a_content_" + hex( r.id.category.value ) + "_" + hex( r.id.group.value ) + "_" + hex( r.id.item.value ) + "_" + hex( r.id.material.value ) + "_" + std::to_string( static_cast<int>( r.id.depth ) );
			if ( auto* row = element( id.c_str() ) )
			{
				const bool selected = s.selectedContent && *s.selectedContent == r.id;
				row->SetClass( "is-selected", selected );
				if ( selected && restore ) row->Focus();
			}
		}
	}
	text( "stockpile_content_status", s.visibleContents.size() == storedEntries ? std::to_string( storedEntries ) + ( storedEntries == 1 ? " item. " : " items. " ) + "Total counts every stored item of that kind."
		: std::to_string( s.visibleContents.size() ) + " of " + std::to_string( storedEntries ) + " items. Total counts every stored item of that kind." );

	// ---- Allow List: check boxes show the pending state
	const bool rulesEditable = !s.draft.pending;
	std::vector<std::string> ruleMarkup;
	ruleMarkup.reserve( s.visibleFilters.size() );
	for ( const auto& r : s.visibleFilters )
	{
		const auto labels = stockpilePathLabels( r.id, s.value.filters );
		const auto id     = stockpileFilterRowId( r.id );
		ruleMarkup.push_back( "<button id='" + id + "' class='w98-list-item' role='row' data-rule='true' data-category='" + safe( r.id.category.value ) + "' data-group='" + safe( r.id.group.value ) + "' data-item='"
			+ safe( r.id.item.value ) + "' data-material='" + safe( r.id.material.value ) + "'><input id='" + id + "_check' class='checkbox' type='checkbox'/><span class='w98-cell w98-cell--grow'>"
			+ safe( labels.item ) + "</span><span class='w98-cell l-sp-w-material'>" + safe( labels.material ) + "</span><span class='w98-cell l-sp-w-type'>" + safe( labels.group ) + "</span></button>" );
	}
	if ( ruleMarkup.empty() ) ruleMarkup.push_back( "<div class='w98-list-empty'>No items match the text.</div>" );
	if ( ruleMarkup != stockpileFilterMarkup_ )
	{
		stockpileFilterMarkup_ = std::move( ruleMarkup );
		stockpileFilterFirst_  = static_cast<std::size_t>( -1 );
	}
	stockpileFilterRows_ = s.visibleFilters;
	if ( auto* list = element( "stockpile_filters" ) )
	{
		const bool restore = list->Contains( context_.GetFocusElement() );
		if ( restore && s.selectedFilter && stockpileRowHeight_ > 0.f )
		{
			const auto selected = std::ranges::find_if( s.visibleFilters, [&]( const auto& row ) { return row.id == *s.selectedFilter; } );
			if ( selected != s.visibleFilters.end() )
			{
				const float top = static_cast<float>( std::distance( s.visibleFilters.begin(), selected ) ) * stockpileRowHeight_;
				if ( top < list->GetScrollTop() || top + stockpileRowHeight_ > list->GetScrollTop() + list->GetClientHeight() ) list->SetScrollTop( top );
			}
		}
		renderStockpileFilterViewport();
		syncStockpileRules();
		if ( restore && s.selectedFilter )
			if ( auto* row = element( stockpileFilterRowId( *s.selectedFilter ).c_str() ) ) row->Focus();
	}
	const auto shown = s.visibleFilters.size();
	text( "stockpile_rule_scope", std::to_string( shown ) + ( shown == 1 ? " item shown" : " items shown" ) );
	enabled( "stockpile_allow_bulk", shown > 0 && rulesEditable );
	enabled( "stockpile_block_bulk", shown > 0 && rulesEditable );

	// ---- Template combo box
	formValue( "stockpile_template_name", s.templateName );
	std::string templates;
	for ( const auto& t : s.value.templateNames )
		templates += "<button type='button' class='w98-list-item' role='option' data-template='" + safe( t ) + "'><span class='w98-cell w98-cell--grow'>" + safe( t ) + "</span></button>";
	if ( templates.empty() ) templates = "<div class='w98-list-empty'>There are no saved templates.</div>";
	if ( auto* e = element( "stockpile_template_options" ); e && stockpileTemplateMarkup_ != templates ) { stockpileTemplateMarkup_ = templates; e->SetInnerRML( templates ); }
	visible( "stockpile_template_options", s.templateMenuOpen );
	if ( s.templateMenuOpen )
	{
		stockpile_->UpdateDocument();
		auto* popup = element( "stockpile_template_options" );
		auto* anchor = element( "stockpile_template_name" )->GetParentNode();
		const auto origin = element( "stockpile_workbench" )->GetAbsoluteOffset( Rml::BoxArea::Border );
		const auto at = anchor->GetAbsoluteOffset( Rml::BoxArea::Border );
		popup->SetProperty( "left", std::to_string( at.x - origin.x ) + "px" );
		popup->SetProperty( "top", std::to_string( at.y - origin.y + anchor->GetOffsetHeight() ) + "px" );
		popup->SetProperty( "width", std::to_string( anchor->GetOffsetWidth() ) + "px" );
	}
	for ( const char* id : { "stockpile_template_name", "stockpile_template_toggle" } )
		if ( auto* e = element( id ) ) e->SetAttribute( "aria-expanded", s.templateMenuOpen ? "true" : "false" );
	const bool rulesPending = !s.draft.rules.empty();
	const bool existingTemplate = std::ranges::any_of( s.value.templateNames, [&]( const auto& n ) { return foldedLabel( n ) == foldedLabel( s.templateName ); } );
	enabled( "stockpile_template_name", !rulesPending );
	enabled( "stockpile_template_toggle", !rulesPending );
	enabled( "stockpile_template_load", !rulesPending && existingTemplate );
	enabled( "stockpile_template_save", !rulesPending && s.templateName.find_first_not_of( " 	" ) != std::string::npos );
	text( "stockpile_template_note", rulesPending ? "Apply your changes before using templates." : "" );
	if ( s.templateOverwriteConfirmationRequired && !stockpileDialog_->active() )
		stockpileDialog_->show( captionName( name ) + " Properties", "Replace the saved template '" + s.pendingTemplateOverwrite + "' with the allow list of " + name + "?", "Yes", "No",
			[this] { controller_->confirmStockpileTemplateOverwrite(); }, [this] { controller_->cancelStockpileTemplateOverwrite(); } );

	// ---- General
	formValue( "stockpile_name", s.draft.name );
	const auto maximumPriority = std::max( 1, s.value.maxPriority );
	const auto displayPriority = std::clamp( std::max( 0, s.value.priority ) + 1, 1, maximumPriority );
	if ( stockpilePriorityEditor_ )
	{
		stockpilePriorityEditor_->sync( NumericEditor::parse( s.draft.priority, 1, maximumPriority ).value_or( displayPriority ), 1, maximumPriority );
		if ( numericStockpileId_ != s.value.id.value ) { numericStockpileId_ = s.value.id.value; stockpilePriorityEditor_->cancel(); }
	}
	formValue( "stockpile_priority", s.draft.priority );
	checked( "stockpile_toggle_pull", s.draft.options.pull );
	checked( "stockpile_toggle_allow_pull", s.draft.options.allowPull );
	checked( "stockpile_toggle_suspended", s.draft.options.suspended );
	for ( const char* id : { "stockpile_name", "stockpile_priority", "stockpile_priority_up", "stockpile_priority_down", "stockpile_toggle_pull", "stockpile_toggle_allow_pull", "stockpile_toggle_suspended" } )
		enabled( id, !s.draft.pending );
	text( "stockpile_summary", std::to_string( s.value.itemCount ) + ( s.value.itemCount == 1 ? " item is" : " items are" ) + " stored here, and " + std::to_string( s.value.reserved )
		+ ( s.value.reserved == 1 ? " item is" : " items are" ) + " on the way." );
	enabled( "stockpile_apply", s.draft.dirty && !s.draft.pending );
	enabled( "stockpile_ok", !s.draft.pending );

	// ---- messages: one message box per condition (PDF p.182-187)
	if ( !s.feedback.empty() && s.feedback != shownStockpileMessage_ && !stockpileDialog_->active() )
	{
		shownStockpileMessage_ = s.feedback;
		stockpileDialog_->show( captionName( name ) + " Properties", s.feedback, "", "OK", [] {}, [this] { controller_->stockpileFeedback( "" ); } );
	}
	if ( s.feedback.empty() ) shownStockpileMessage_.clear();
	// A command the game refused is reported the same way.
	const auto& refused = controller_->state().status;
	if ( !refused.empty() && refused != shownStockpileStatus_ && !stockpileDialog_->active() )
	{
		shownStockpileStatus_ = refused;
		stockpileDialog_->show( captionName( name ) + " Properties", commandFeedbackText( textCatalog_, refused ), "", "OK", [] {}, [] {} );
	}
	if ( refused.empty() ) shownStockpileStatus_.clear();
	if ( closeStockpileWhenApplied_ && !s.draft.pending )
	{
		if ( s.draft.dirty ) closeStockpileWhenApplied_ = false; // rejected or conflicting: stay open
		else pendingStockpileClose_ = true;
	}
	projectingStockpile_ = renderingStockpile_ = false;
}
void Management6ARmlBinding::renderStockpileFilterViewport()
{
	if ( renderingStockpileFilters_ ) return;
	auto* list = element( "stockpile_filters" );
	if ( !list ) return;
	// A row is 16 px of the snapped 11 px system font (w98-list-item), so its height follows the font at every scale.
	const float row = std::max( 1.f, std::round( list->GetComputedValues().font_size() * 16.f / 11.f ) );
	if ( row != stockpileRowHeight_ ) { stockpileRowHeight_ = row; stockpileFilterFirst_ = static_cast<std::size_t>( -1 ); }
	const float scroll = std::clamp( list->GetScrollTop(), 0.f, std::max( 0.f, static_cast<float>( stockpileFilterMarkup_.size() ) * row - list->GetClientHeight() ) );
	const auto anchor = static_cast<std::size_t>( scroll / row );
	const auto first = std::min( anchor > 8 ? anchor - 8 : 0, stockpileFilterMarkup_.size() );
	if ( first == stockpileFilterFirst_ ) return;
	const auto count = static_cast<std::size_t>( std::max( list->GetClientHeight(), 240.f ) / row ) + 24;
	const auto last = std::min( first + count, stockpileFilterMarkup_.size() );
	std::string markup;
	const auto spacer = [&]( std::size_t rows ) { return "<div class='l-sp-spacer' style='height:" + std::to_string( static_cast<float>( rows ) * row ) + "px;'></div>"; };
	if ( first ) markup += spacer( first );
	for ( auto index = first; index < last; ++index ) markup += stockpileFilterMarkup_[index];
	if ( last < stockpileFilterMarkup_.size() ) markup += spacer( stockpileFilterMarkup_.size() - last );
	renderingStockpileFilters_ = true;
	list->SetInnerRML( markup );
	stockpileFilterFirst_ = first;
	list->SetScrollTop( scroll );
	renderingStockpileFilters_ = false;
	syncStockpileRules();
}
void Management6ARmlBinding::syncStockpileRules()
{
	if ( !controller_ || stockpileFilterFirst_ == static_cast<std::size_t>( -1 ) ) return;
	const auto& s = controller_->state().stockpile;
	for ( const auto& r : stockpileFilterRows_ )
	{
		const auto id = stockpileFilterRowId( r.id );
		auto* row = element( id.c_str() );
		if ( !row ) continue;
		row->SetClass( "is-selected", s.selectedFilter && *s.selectedFilter == r.id );
		checked( ( id + "_check" ).c_str(), controller_->stockpileRuleAllowed( r ) );
		enabled( ( id + "_check" ).c_str(), !s.draft.pending );
	}
}
void Management6ARmlBinding::agricultureMarkup( const char* id, const std::string& markup )
{
	auto* container = element( id );
	if ( !container ) return;
	auto& previous = agricultureMarkup_[id];
	if ( markup == previous && container->GetNumChildren() > 0 ) return;
	std::string focused;
	if ( auto* focus = context_.GetFocusElement(); focus && container->Contains( focus ) ) focused = focus->GetId();
	const float top = container->GetScrollTop(), left = container->GetScrollLeft();
	syncingCheckbox_ = true;
	container->SetInnerRML( markup );
	syncingCheckbox_ = false;
	container->SetScrollTop( top );
	container->SetScrollLeft( left );
	previous = markup;
	if ( !focused.empty() )
		if ( auto* row = element( focused.c_str() ) ) row->Focus();
}
void Management6ARmlBinding::renderAgriculture( const AgricultureState& s )
{
	renderingAgriculture_ = true;
	const auto status = s.request.status;
	visible( "agriculture_loading", status == RequestStatus::Loading );
	visible( "agriculture_empty", status == RequestStatus::Empty );
	visible( "agriculture_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "agriculture_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "agriculture_error", s.request.message );
	const auto kindName = kind( s.value.target.kind );
	text( "agriculture_title", captionName( s.value.name.empty() ? kindName : s.value.name ) + " Properties" );
	const auto k       = s.value.target.kind;
	const bool farm    = k == AgricultureKind::Farm;
	const bool grove   = k == AgricultureKind::Grove;
	const bool pasture = k == AgricultureKind::Pasture;
	const auto& o      = s.draft.options;
	const auto catalogName = [&]( const CatalogId& id ) {
		const auto row = std::find_if( s.value.catalog.begin(), s.value.catalog.end(), [&]( const auto& r ) { return r.id == id; } );
		return row != s.value.catalog.end() && !row->name.empty() ? row->name : id.value;
	};

	// ---- Tabs: only the pages this designation supports.
	text( "agriculture_view_crops", grove ? "Trees" : "Crops" );
	for ( const auto& [name, pane] : std::array {
		std::pair { "general", AgriculturePane::General }, std::pair { "plots", AgriculturePane::Plots }, std::pair { "queue", AgriculturePane::PlotQueue },
		std::pair { "crops", AgriculturePane::Crops }, std::pair { "animals", AgriculturePane::Animals }, std::pair { "food", AgriculturePane::Food } } )
	{
		const auto tabId = std::string( "agriculture_view_" ) + name;
		visible( tabId.c_str(), agricultureSupportsPane( k, pane ) );
		visible( ( std::string( "agriculture_" ) + name + "_pane" ).c_str(), s.pane == pane );
		if ( auto* tab = element( tabId.c_str() ) )
		{
			tab->SetClass( "is-selected", s.pane == pane );
			tab->SetAttribute( "aria-selected", s.pane == pane ? "true" : "false" );
		}
	}
	const char* selectedTab = s.pane == AgriculturePane::Plots ? "agriculture_view_plots" : s.pane == AgriculturePane::PlotQueue ? "agriculture_view_queue"
		: s.pane == AgriculturePane::Crops ? "agriculture_view_crops" : s.pane == AgriculturePane::Animals ? "agriculture_view_animals"
		: s.pane == AgriculturePane::Food ? "agriculture_view_food" : "agriculture_view_general";
	connected_tabs::select( *element( "agriculture_tabs" ), element( selectedTab ) );

	// ---- General page: every value shown is the pending sheet value.
	if ( agricultureSettingsTarget_ != s.value.target )
	{
		agricultureSettingsTarget_ = s.value.target;
		agricultureSelectedOrder_.reset();
		if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "agriculture_name" ) ) ) input->SetValue( s.draft.name );
	}
	formValue( "agriculture_name", s.draft.name );
	visible( "agriculture_work_farm", farm );
	visible( "agriculture_work_grove", grove );
	visible( "agriculture_work_pasture", pasture );
	checked( "agriculture_toggle_suspended", o.suspended );
	checked( "agriculture_toggle_harvest", o.harvest );
	checked( "agriculture_toggle_pasture_harvest", o.harvest );
	checked( "agriculture_toggle_hay", o.harvestHay );
	checked( "agriculture_toggle_tame", o.tame );
	checked( "agriculture_toggle_pick", o.pick );
	checked( "agriculture_toggle_plant", o.plant );
	checked( "agriculture_toggle_fell", o.fell );
	for ( const char* id : { "agriculture_name", "agriculture_toggle_suspended", "agriculture_toggle_harvest", "agriculture_toggle_pasture_harvest", "agriculture_toggle_hay",
			  "agriculture_toggle_tame", "agriculture_toggle_pick", "agriculture_toggle_plant", "agriculture_toggle_fell", "agriculture_male_cap", "agriculture_female_cap", "agriculture_animal_type" } )
		enabled( id, !s.draft.pending );
	const auto plural = []( std::int32_t n, const char* one, const char* many ) { return std::to_string( n ) + " " + ( n == 1 ? one : many ); };
	std::string summary;
	// Tilled, planted and ready are subsets of the plots, never additive totals.
	if ( farm ) summary = plural( s.value.plots, "plot", "plots" ) + ": " + std::to_string( s.value.tilled ) + " tilled, " + std::to_string( s.value.planted ) + " planted, " + std::to_string( s.value.ready ) + " ready to harvest.";
	else if ( grove ) summary = plural( s.value.plots, "tree site", "tree sites" ) + ": " + std::to_string( s.value.planted ) + " planted, " + std::to_string( s.value.ready ) + " ready to pick.";
	else summary = std::to_string( s.value.total ) + " of " + plural( s.value.capacity, "animal", "animals" ) + " (" + std::to_string( s.value.male ) + " male, " + std::to_string( s.value.female ) + " female).";
	text( "agriculture_summary", summary );
	enabled( "agriculture_apply", s.draft.dirty && !s.draft.pending );
	enabled( "agriculture_ok", !s.draft.pending );

	// ---- Crops / Trees page: list view in details view; the highlighted row is the pending default.
	if ( !pasture )
	{
		text( "agriculture_crops_label", grove ? "Tree type planted in this grove:" : "Default crop (planted on plots with no assigned crop or queue):" );
		std::string rows;
		for ( const auto& r : s.value.catalog )
		{
			const bool selected = o.product == r.id;
			rows += "<button id='agriculture_farm_crop_" + hex( r.id.value ) + "' class='w98-list-item" + std::string( selected ? " is-selected" : "" ) + "' data-catalog='" + safe( r.id.value ) + "' aria-selected='" + ( selected ? "true" : "false" ) + "'>"
				+ "<span class='w98-cell w98-cell--grow'>" + safe( r.name.empty() ? r.id.value : r.name ) + "</span><span class='w98-cell w98-cell--num w98-w-stock'>" + std::to_string( r.available )
				+ "</span><span class='w98-cell w98-cell--num w98-w-stock'>" + std::to_string( r.planted ) + "</span><span class='w98-cell w98-cell--num w98-w-stock'>" + std::to_string( r.harvested ) + "</span></button>";
		}
		if ( rows.empty() ) rows = "<div class='w98-list-empty'>No " + std::string( grove ? "trees" : "crops" ) + " are known yet.</div>";
		agricultureMarkup( "agriculture_farm_catalog", rows );
		const bool changed = o.product != s.value.product;
		text( "agriculture_default_crop", o.product.value.empty() ? std::string( grove ? "No tree type is set." : "No default crop is set." )
			: ( grove ? "Tree type: " : "Default crop: " ) + catalogName( o.product ) + ( changed ? " (applies when you click Apply or OK)" : "" ) );
	}

	// ---- Plots page.
	if ( farm )
	{
		text( "agriculture_plot_selection_count", "Plots (" + std::to_string( s.selectedPlots.size() ) + " of " + std::to_string( s.value.fields.size() ) + " selected):" );
		std::string grid;
		if ( s.value.fields.empty() ) grid = "<div class='w98-list-empty'>This farm has no plots.</div>";
		else
		{
			std::map<int, std::map<std::pair<int, int>, const FarmPlotRow*>> layers;
			for ( const auto& field : s.value.fields ) layers[field.position.z][{ field.position.x, field.position.y }] = &field;
			for ( const auto& [z, cells] : layers )
			{
				int minX = cells.begin()->first.first, maxX = minX, minY = cells.begin()->first.second, maxY = minY;
				for ( const auto& [p, field] : cells )
				{
					minX = std::min( minX, p.first ); maxX = std::max( maxX, p.first );
					minY = std::min( minY, p.second ); maxY = std::max( maxY, p.second );
				}
				if ( layers.size() > 1 ) grid += "<div class='w98-plot-layer'>Level " + std::to_string( z ) + "</div>";
				for ( int y = minY; y <= maxY; ++y )
				{
					// 20 px cells at 1x; lengths are em of the snapped 11 px sheet font so rows and cells scale together.
					grid += "<div class='w98-plot-row' style='width: " + std::to_string( ( maxX - minX + 1 ) * 20.0 / 11.0 ) + "em;'>";
					for ( int x = minX; x <= maxX; ++x )
					{
						const auto found = cells.find( { x, y } );
						if ( found == cells.end() ) { grid += "<span class='w98-plot-gap'></span>"; continue; }
						const auto& field   = *found->second;
						const bool selected = std::find( s.selectedPlots.begin(), s.selectedPlots.end(), field.position ) != s.selectedPlots.end();
						const bool focused  = s.focusedPlot && *s.focusedPlot == field.position;
						const auto planned  = !field.orders.empty() ? field.orders.front().crop.value : !field.assignedCrop.value.empty() ? field.assignedCrop.value : s.value.product.value;
						const auto crop     = field.planted ? field.plantedCrop.value : planned;
						std::string icon;
						if ( !crop.empty() )
						{
							const auto row = std::find_if( s.value.catalog.begin(), s.value.catalog.end(), [&]( const auto& r ) { return r.id.value == crop; } );
							std::string sheet = row != s.value.catalog.end() && !row->iconSheet.empty() ? row->iconSheet : "build_Seed.tga";
							if ( !std::ranges::all_of( sheet, []( unsigned char c ) { return std::isalnum( c ) || c == '_' || c == '-' || c == '.'; } ) ) sheet = "build_Seed.tga";
							icon = "<img src='/tilesheet/" + sheet + "' alt=''" + std::string( field.planted ? "" : " class='is-planned'" ) + "/>";
						}
						const auto key   = farmPlotKey( field.position );
						const auto state = field.ready ? "ready to harvest" : field.planted ? "growing" : field.tilled ? "tilled" : "untilled";
						const auto label = "Plot " + std::to_string( field.position.x ) + ", " + std::to_string( field.position.y ) + ": " + ( crop.empty() ? std::string( "no crop" ) : catalogName( CatalogId { crop } ) ) + ", " + state + ( field.busy ? ", being worked" : "" );
						grid += "<button id='agriculture_plot_" + std::to_string( field.position.x ) + "_" + std::to_string( field.position.y ) + "_" + std::to_string( field.position.z ) + "' class='w98-plot"
							+ ( selected ? " is-selected" : "" ) + ( focused ? " is-focused" : "" ) + "' data-plot='" + key + "' role='option' aria-selected='" + ( selected ? "true" : "false" ) + "' aria-label='" + safe( label ) + "' title='" + safe( label ) + "'>"
							+ "<span class='w98-plot__ground" + ( field.tilled && !field.planted ? " is-tilled" : "" ) + "'>" + icon + ( field.ready ? "<span class='w98-plot__ready'></span>" : "" ) + "</span></button>";
					}
					grid += "</div>";
				}
			}
		}
		agricultureMarkup( "agriculture_plot_grid", grid );
		if ( s.focusedPlot )
			if ( auto* grid = element( "agriculture_plot_grid" ); grid && grid->Contains( context_.GetFocusElement() ) )
				if ( auto* cell = element( ( "agriculture_plot_" + std::to_string( s.focusedPlot->x ) + "_" + std::to_string( s.focusedPlot->y ) + "_" + std::to_string( s.focusedPlot->z ) ).c_str() ) )
				{
					cell->Focus();
					cell->ScrollIntoView( Rml::ScrollIntoViewOptions( Rml::ScrollAlignment::Nearest ) );
				}
		std::string detail = "Click a plot to select it. Ctrl+click adds or removes a plot; Shift+click selects a rectangle.";
		// With several plots selected the line states the command scope; with one it describes that plot.
		const auto describe = s.selectedPlots.size() == 1 ? std::optional<WorldPosition> { s.selectedPlots.front() } : s.selectedPlots.empty() ? s.focusedPlot : std::nullopt;
		if ( s.selectedPlots.size() > 1 )
			detail = "Commands change each of the " + std::to_string( s.selectedPlots.size() ) + " selected plots; plantings are per plot.";
		else if ( describe )
			if ( auto field = std::find_if( s.value.fields.begin(), s.value.fields.end(), [&]( const auto& r ) { return r.position == *describe; } ); field != s.value.fields.end() )
				detail = "Plot " + std::to_string( field->position.x ) + ", " + std::to_string( field->position.y ) + ": assigned " + ( field->assignedCrop.value.empty() ? std::string( "farm default" ) : catalogName( field->assignedCrop ) )
					+ "; growing " + ( field->planted ? catalogName( field->plantedCrop ) + ( field->ready ? " (ready)" : "" ) : std::string( "nothing" ) ) + "; " + std::to_string( field->orders.size() ) + " queued.";
		text( "agriculture_plot_detail", detail );
		// Crop drop-down list for the plot commands; rebuilt only when the catalog changes.
		// A blank first entry keeps the list empty until a crop is chosen (a drop-down list never invents a value).
		std::vector<std::pair<std::string, std::string>> cropValues;
		for ( const auto& r : s.value.catalog ) cropValues.emplace_back( r.id.value, ( r.name.empty() ? r.id.value : r.name ) + " (" + std::to_string( r.available ) + " seeds)" );
		std::string options = s.selectedProduct ? std::string() : std::string( "|" );
		for ( const auto& [value, label] : cropValues ) options += value + "=" + label + "\n";
		if ( options != agricultureCropOptions_ )
		{
			agricultureCropOptions_ = options;
			setSelectOptions( element( "agriculture_plot_crop" ), cropValues, !s.selectedProduct );
		}
		if ( auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( element( "agriculture_plot_crop" ) ) )
		{
			const auto wanted = s.selectedProduct ? s.selectedProduct->value : std::string();
			if ( select->GetValue() != wanted ) select->SetValue( wanted );
		}
		const auto n        = s.selectedPlots.size();
		const bool hasCrop  = s.selectedProduct && std::any_of( s.value.catalog.begin(), s.value.catalog.end(), [&]( const auto& r ) { return r.id == *s.selectedProduct; } );
		text( "agriculture_selected_title", n == 0 ? std::string( "Selected plots" ) : n == 1 ? std::string( "Selected plot (1)" ) : "Selected plots (" + std::to_string( n ) + ")" );
		enabled( "agriculture_plot_crop", n > 0 && !s.value.catalog.empty() );
		enabled( "agriculture_plot_assign", n > 0 && hasCrop );
		enabled( "agriculture_plot_count", n > 0 && hasCrop );
		enabled( "agriculture_plot_queue", n > 0 && hasCrop );
		enabled( "agriculture_plot_repeat", n > 0 && hasCrop );
		enabled( "agriculture_plot_default", n > 0 );
		enabled( "agriculture_plot_clear", n > 0 );
		enabled( "agriculture_plot_select_all", n < s.value.fields.size() );
		agriculturePlotCountEditor_->sync( 1, 1, 99 );

		// ---- Plot Queue page: the queue of exactly one selected plot.
		std::string orders;
		const FarmPlotRow* one = nullptr;
		if ( n == 1 )
			if ( auto field = std::find_if( s.value.fields.begin(), s.value.fields.end(), [&]( const auto& r ) { return r.position == s.selectedPlots.front(); } ); field != s.value.fields.end() ) one = &*field;
		if ( one && agricultureSelectedOrder_ && std::none_of( one->orders.begin(), one->orders.end(), [&]( const auto& r ) { return r.id == *agricultureSelectedOrder_; } ) ) agricultureSelectedOrder_.reset();
		if ( !one ) agricultureSelectedOrder_.reset();
		text( "agriculture_queue_plot", one ? "Plot " + std::to_string( one->position.x ) + ", " + std::to_string( one->position.y ) + ": assigned " + ( one->assignedCrop.value.empty() ? std::string( "farm default" ) : catalogName( one->assignedCrop ) ) + "."
			: std::string( "Select exactly one plot on the Plots page to see its queue." ) );
		if ( one )
			for ( std::size_t i = 0; i < one->orders.size(); ++i )
			{
				const auto& r       = one->orders[i];
				const bool selected = agricultureSelectedOrder_ && *agricultureSelectedOrder_ == r.id;
				orders += "<button id='agriculture_order_" + std::to_string( r.id ) + "' class='w98-list-item" + std::string( selected ? " is-selected" : "" ) + "' data-order='" + std::to_string( r.id ) + "'>"
					+ "<span class='w98-cell w98-cell--grow'>" + safe( catalogName( r.crop ) ) + "</span><span class='w98-cell w98-cell--num w98-w-state'>" + ( r.repeat ? std::string( "Repeat" ) : std::to_string( r.remaining ) ) + "</span></button>";
			}
		if ( orders.empty() ) orders = "<div class='w98-list-empty'>" + std::string( one ? "No plantings are queued." : "" ) + "</div>";
		agricultureMarkup( "agriculture_plot_orders", orders );
		const auto index = one && agricultureSelectedOrder_ ? std::find_if( one->orders.begin(), one->orders.end(), [&]( const auto& r ) { return r.id == *agricultureSelectedOrder_; } ) - one->orders.begin() : -1;
		enabled( "agriculture_order_up", index > 0 );
		enabled( "agriculture_order_down", index >= 0 && one && index + 1 < static_cast<std::ptrdiff_t>( one->orders.size() ) );
		enabled( "agriculture_order_remove", index >= 0 );
	}

	// ---- Animals and Food pages.
	if ( pasture )
	{
		std::vector<std::pair<std::string, std::string>> typeValues;
		for ( const auto& r : s.value.catalog ) typeValues.emplace_back( r.id.value, r.name.empty() ? r.id.value : r.name );
		std::string types = o.product.value.empty() ? std::string( "|" ) : std::string();
		for ( const auto& [value, label] : typeValues ) types += value + "=" + label + "\n";
		if ( types != agricultureAnimalOptions_ )
		{
			agricultureAnimalOptions_ = types;
			setSelectOptions( element( "agriculture_animal_type" ), typeValues, o.product.value.empty() );
		}
		if ( auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( element( "agriculture_animal_type" ) ) )
			if ( select->GetValue() != o.product.value ) select->SetValue( o.product.value );
		std::string animals;
		for ( const auto& r : s.value.animals )
		{
			const auto id       = std::to_string( r.id.value );
			const bool marked   = std::binary_search( o.butcher.begin(), o.butcher.end(), r.id.value );
			animals += "<label id='agriculture_animal_" + id + "' class='w98-list-item' data-animal='" + id + "' for='agriculture_butcher_" + id + "'>"
				+ "<input id='agriculture_butcher_" + id + "' class='checkbox' type='checkbox' data-butcher='" + id + "' aria-label='Butcher " + safe( r.name ) + "'" + ( marked ? " checked='checked'" : "" ) + ( s.draft.pending ? " disabled='disabled'" : "" ) + "/>"
				+ "<span class='w98-cell w98-cell--grow'>" + safe( r.name ) + "</span><span class='w98-cell w98-w-amount'>" + ( r.gender == Gender::Male ? "Male" : "Female" ) + "</span><span class='w98-cell w98-w-amount'>" + ( r.young ? "Young" : "Adult" ) + "</span></label>";
		}
		if ( animals.empty() ) animals = "<div class='w98-list-empty'>There are no animals in this pasture.</div>";
		agricultureMarkup( "agriculture_animals", animals );
		// Selection is a class on the existing rows so a click never rebuilds the check box it lands on.
		if ( auto* list = element( "agriculture_animals" ) )
			for ( int i = 0; i < list->GetNumChildren(); ++i )
			{
				auto* row = list->GetChild( i );
				row->SetClass( "is-selected", s.selectedAnimal && attr( row, "data-animal" ) == std::to_string( s.selectedAnimal->value ) );
			}
		agricultureMaleEditor_->sync( o.maxMale, 0, 99 );
		agricultureFemaleEditor_->sync( o.maxFemale, 0, 99 );
		// The game keeps limits and food rules per animal type, so they need a type first.
		const bool typed = !o.product.value.empty() && o.product == s.value.product;
		for ( const char* id : { "agriculture_male_cap", "agriculture_female_cap", "agriculture_male_cap-up", "agriculture_male_cap-down", "agriculture_female_cap-up", "agriculture_female_cap-down" } )
			enabled( id, typed && !s.draft.pending );
		text( "agriculture_pasture_summary", o.product.value.empty() ? std::string( "Choose an animal type to set the limits." )
			: !typed ? std::string( "Apply the new animal type to set its limits." )
			: std::to_string( s.value.male ) + " male and " + std::to_string( s.value.female ) + " female now; room for " + plural( s.value.capacity, "animal.", "animals." ) );
		std::string foods;
		for ( const auto& r : s.value.foods )
		{
			const auto key      = pastureFoodKey( r.item, r.material );
			const auto id       = hex( key );
			const bool allowed  = std::binary_search( o.foods.begin(), o.foods.end(), key );
			foods += "<label class='w98-list-item' data-food-row='" + safe( key ) + "' for='agriculture_food_" + id + "'><input id='agriculture_food_" + id + "' class='checkbox' type='checkbox' data-food='" + safe( key ) + "'"
				+ ( allowed ? " checked='checked'" : "" ) + ( s.draft.pending ? " disabled='disabled'" : "" ) + "/><span>" + safe( r.name ) + "</span></label>";
		}
		if ( foods.empty() ) foods = "<div class='w98-list-empty'>" + std::string( o.product.value.empty() ? "Choose an animal type on the Animals page first." : o.product != s.value.product ? "Apply the new animal type to see its foods." : "No foods are known for this animal type." ) + "</div>";
		agricultureMarkup( "agriculture_foods", foods );
		if ( auto* list = element( "agriculture_foods" ) )
			for ( int i = 0; i < list->GetNumChildren(); ++i )
			{
				auto* row = list->GetChild( i );
				row->SetClass( "is-selected", s.selectedFood && attr( row, "data-food-row" ) == *s.selectedFood );
			}
		text( "agriculture_food_summary", "Food stored: " + std::to_string( s.value.foodCurrent ) + " of " + std::to_string( s.value.foodMax ) + ". Hay stored: " + std::to_string( s.value.hayCurrent ) + " of " + std::to_string( s.value.hayMax ) + "." );
	}

	// ---- Messages use a message box; OK closes the sheet once the applied values are confirmed.
	if ( !s.feedback.empty() && s.feedback != shownAgricultureMessage_ && !stockpileDialog_->active() )
	{
		shownAgricultureMessage_ = s.feedback;
		stockpileDialog_->show( captionName( s.value.name.empty() ? kindName : s.value.name ) + " Properties", s.feedback, "", "OK", [] {}, [this] { controller_->agricultureFeedback( "" ); } );
	}
	if ( s.feedback.empty() ) shownAgricultureMessage_.clear();
	if ( closeAgricultureWhenApplied_ && !s.draft.pending )
	{
		if ( s.draft.dirty ) closeAgricultureWhenApplied_ = false; // rejected or conflicting: stay open
		else pendingAgricultureClose_ = true;
	}
	renderingAgriculture_ = false;
}

void Management6ARmlBinding::stateChanged( const Management6AState& s )
{
    if(stockpileDialog_ && (projectedStockpileWorld_!=s.world || projectedStockpileId_!=s.stockpile.value.id || projectedWorkshopId_!=s.workshop.value.id))stockpileDialog_->close(false);
    projectedWorkshopId_=s.workshop.value.id;projectedStockpileWorld_=s.world;projectedStockpileId_=s.stockpile.value.id;
	if ( !workshop_ || !stockpile_ || !agriculture_ )
		return;
	if ( !presentationEnabled_ )
	{
		workshop_->Hide();
		stockpile_->Hide();
		agriculture_->Hide();
		return;
	}
	if ( s.view != activeView_ )
	{
		if ( activeView_ == ManagementView::Workshop )
			workshop_->Hide();
		else if ( activeView_ == ManagementView::Stockpile )
			stockpile_->Hide();
		else if ( activeView_ == ManagementView::Agriculture )
			agriculture_->Hide();
		if ( s.view == ManagementView::Workshop )
			workshop_->Show();
		else if ( s.view == ManagementView::Stockpile )
			stockpile_->Show();
		else if ( s.view == ManagementView::Agriculture )
			agriculture_->Show();
		activeView_ = s.view;
	}
	visible( "workshop_manager_root", s.view == ManagementView::Workshop );
	visible( "stockpile_manager_root", s.view == ManagementView::Stockpile );
	visible( "agriculture_manager_root", s.view == ManagementView::Agriculture );
	// Only the active detached workbench needs DOM projection. Rebuilding all
	// three documents on every live-search keystroke made Stockpile text entry
	// pay for two hidden screens as well as its own large rule tree.
	if ( s.view == ManagementView::Workshop )
		renderWorkshop( s.workshop );
	else if ( s.view == ManagementView::Stockpile )
		renderStockpile( s.stockpile );
	else if ( s.view == ManagementView::Agriculture )
		renderAgriculture( s.agriculture );
	tradeConfirmationVisible_ = s.workshop.tradeConfirmationRequired;
	const std::string status  = s.pendingAction ? textCatalog_.format( LocalizationKey{"status.updating"} ) : commandFeedbackText(textCatalog_,s.status);
	// Close after OK only once rendering is finished: closing notifies the controller again.
	if ( pendingWorkshopClose_ )
	{
		pendingWorkshopClose_ = false;
		closeWorkshopWindow();
	}
	if ( pendingAgricultureClose_ )
	{
		pendingAgricultureClose_ = false;
		closeAgricultureWindow();
	}
	if ( pendingStockpileClose_ )
	{
		pendingStockpileClose_ = false;
		closeStockpileWindow();
	}
}
} // namespace ingnomia::ui::management6a
