/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6ARmlBinding.h"
#include "../../localization/RmlText.h"

#include <algorithm>
#include <charconv>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/StringUtilities.h>

namespace ingnomia::ui::management6a
{
namespace
{
std::string safe( const std::string& value )
{
	return Rml::StringUtilities::EncodeRml( value );
}
std::string mode( CraftRepeatMode value )
{
	return value == CraftRepeatMode::Once ? "Number" : value == CraftRepeatMode::Maintain ? "To"
																						  : "Repeat";
}
std::string check( TriState value )
{
	return value == TriState::On ? "[x]" : value == TriState::Mixed ? "[-]" : "[ ]";
}
std::string stateName( TriState value )
{
	return value == TriState::On ? "On" : value == TriState::Mixed ? "Mixed" : "Off";
}
std::string disclosure( const StockpileFilterRow& row )
{
	return row.id.depth == FilterDepth::Material ? std::string {} : row.state == TriState::Mixed ? "[v] " : "[>] ";
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
std::string stockpileFilterRowId( const StockpileFilterRowId& row )
{
	return "m6a_filter_" + hex( row.category.value ) + "_" + hex( row.group.value ) + "_" + hex( row.item.value ) + "_" + hex( row.material.value ) + "_" + std::to_string( static_cast<int>( row.depth ) );
}
std::string attr( const Rml::Element* element, const char* name )
{
	return element ? element->GetAttribute<Rml::String>( name, "" ) : std::string {};
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
	workshop_    = context_.LoadDocument( "windows/workshop_manager.rml" );
	stockpile_   = context_.LoadDocument( "windows/stockpile_manager.rml" );
	agriculture_ = context_.LoadDocument( "panels/agriculture_manager.rml" );
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
				   { controller_->close(); } );
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
	bindClick( "stockpile_page_previous", [this, rerender]
			   {stockpilePage_=stockpilePage_>=pageSize_?stockpilePage_-pageSize_:0;rerender(); } );
	bindClick( "stockpile_page_next", [this, rerender]
			   {stockpilePage_+=pageSize_;rerender(); } );
	bindClick( "agriculture_page_previous", [this, rerender]
			   {agriculturePage_=agriculturePage_>=pageSize_?agriculturePage_-pageSize_:0;rerender(); } );
	bindClick( "agriculture_page_next", [this, rerender]
			   {agriculturePage_+=pageSize_;rerender(); } );
	for ( const char* id : { "workshop_search", "stockpile_search", "agriculture_search" } )
	{
		auto search = [this, id]( Rml::Event& )
		{
			if ( std::string_view( id ) == "workshop_search" )
				workshopPage_ = 0;
			else if ( std::string_view( id ) == "stockpile_search" )
				stockpilePage_ = 0;
			else
				agriculturePage_ = 0;
			controller_->setSearch( formValue( id ) );
		};
		bind( id, "input", search );
		bind( id, "change", search );
	}
	bind( "workshop_products", "click", [this]( Rml::Event& e )
		  {const auto key=attr(e.GetTargetElement(),"data-catalog");if(!key.empty())controller_->selectWorkshopProduct(CatalogId{key}); } );
	bind( "workshop_queue", "click", [this]( Rml::Event& e )
		  {const auto key=attr(e.GetTargetElement(),"data-job");if(!key.empty())controller_->selectWorkshopJob({static_cast<std::uint32_t>(std::stoul(key))}); } );
	bind( "workshop_trade_rows", "click", [this]( Rml::Event& e )
		  {auto*target=e.GetTargetElement();const auto item=attr(target,"data-item"),material=attr(target,"data-material"),party=attr(target,"data-party"),quality=attr(target,"data-quality");if(!item.empty())controller_->selectTradeRow({party=="trader"?TradeParty::Trader:TradeParty::Player,CatalogId{item},CatalogId{material},static_cast<std::uint8_t>(std::stoul(quality))}); } );
	bind( "stockpile_filters", "click", [this]( Rml::Event& e )
			  {auto*t=e.GetTargetElement();const auto category=attr(t,"data-category");if(category.empty())return;controller_->selectStockpileFilter({controller_->state().stockpile.value.id,CatalogId{category},CatalogId{attr(t,"data-group")},CatalogId{attr(t,"data-item")},CatalogId{attr(t,"data-material")},static_cast<FilterDepth>(std::stoul(attr(t,"data-depth")))}); } );
	bind( "stockpile_filters", "keydown", [this]( Rml::Event& e )
			  {
				  const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
				  const auto* target = e.GetTargetElement();
				  const auto category = attr( target, "data-category" );
				  if ( !target || category.empty() )
					  return;
				  const StockpileFilterRowId id { controller_->state().stockpile.value.id, CatalogId { category }, CatalogId { attr( target, "data-group" ) }, CatalogId { attr( target, "data-item" ) }, CatalogId { attr( target, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( target, "data-depth" ) ) ) };
				  controller_->selectStockpileFilter( id );
				  if ( key == Rml::Input::KI_UP || key == Rml::Input::KI_DOWN || key == Rml::Input::KI_HOME || key == Rml::Input::KI_END )
				  {
					  const auto delta = key == Rml::Input::KI_UP ? -1 : key == Rml::Input::KI_DOWN ? 1 : key == Rml::Input::KI_HOME ? -2147483647 : 2147483647;
					  controller_->moveStockpileFilterSelection( delta );
				  }
				  else if ( key == Rml::Input::KI_RETURN || key == Rml::Input::KI_SPACE )
					  controller_->toggleSelectedStockpileFilter();
				  else
					  return;
				  e.StopPropagation();
				  if ( controller_->state().stockpile.selectedFilter )
					  if ( auto* row = element( stockpileFilterRowId( *controller_->state().stockpile.selectedFilter ).c_str() ) )
						  row->Focus();
			  } );
	bind( "stockpile_rows", "click", [this]( Rml::Event& e )
		  {auto*t=e.GetTargetElement();const auto item=attr(t,"data-item");if(!item.empty())controller_->selectStockpileContent({CatalogId{item},CatalogId{attr(t,"data-material")}}); } );
	bind( "agriculture_products", "click", [this]( Rml::Event& e )
		  {const auto key=attr(e.GetTargetElement(),"data-catalog");if(!key.empty())controller_->selectAgricultureProduct(CatalogId{key}); } );
	bind( "agriculture_animals", "click", [this]( Rml::Event& e )
		  {const auto key=attr(e.GetTargetElement(),"data-animal");if(!key.empty())controller_->selectAgricultureAnimal({static_cast<std::uint32_t>(std::stoul(key))}); } );

	bindClick( "workshop_apply_basics", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setWorkshopBasics(formValue("workshop_name"),priority("workshop_priority",s.priority,s.maxPriority),s.suspended,s.acceptGenerated,s.autoCraftMissing); } );
	bindClick( "workshop_toggle_suspended", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setWorkshopBasics(s.name,s.priority,!s.suspended,s.acceptGenerated,s.autoCraftMissing); } );
	bindClick( "workshop_toggle_generated", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setWorkshopBasics(s.name,s.priority,s.suspended,!s.acceptGenerated,s.autoCraftMissing); } );
	bindClick( "workshop_toggle_auto_missing", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setWorkshopBasics(s.name,s.priority,s.suspended,s.acceptGenerated,!s.autoCraftMissing); } );
	bindClick( "workshop_toggle_linked", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setWorkshopBasics(s.name,s.priority,s.suspended,s.acceptGenerated,s.autoCraftMissing,!s.connectStockpile); } );
	bindClick( "workshop_next_product", [this]
			   { controller_->nextWorkshopProduct(); } );
	bindClick( "workshop_queue_once", [this]
			   { controller_->queueSelectedCraftDefault(); } );
	bindClick( "workshop_next_job", [this]
			   { controller_->nextWorkshopJob(); } );
	bindClick( "workshop_toggle_job", [this]
			   {const auto&s=controller_->state().workshop;if(!s.selectedJob)return;auto i=std::find_if(s.value.queue.begin(),s.value.queue.end(),[&](const auto&r){return r.id==*s.selectedJob;});if(i!=s.value.queue.end())controller_->setSelectedJob(i->mode,i->count,!i->suspended,i->moveBack); } );
	bindClick( "workshop_move_up", [this]
			   { controller_->moveSelectedJob( MoveDirection::Up ); } );
	bindClick( "workshop_move_down", [this]
			   { controller_->moveSelectedJob( MoveDirection::Down ); } );
	bindClick( "workshop_cancel_job", [this]
			   { controller_->cancelSelectedJob(); } );
	bindClick( "workshop_toggle_corpses", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setButcherOptions(!s.butcherCorpses,s.butcherExcess); } );
	bindClick( "workshop_toggle_excess", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setButcherOptions(s.butcherCorpses,!s.butcherExcess); } );
	bindClick( "workshop_toggle_catch", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setFisherOptions(!s.catchFish,s.processFish); } );
	bindClick( "workshop_toggle_process", [this]
			   {const auto&s=controller_->state().workshop.value;controller_->setFisherOptions(s.catchFish,!s.processFish); } );
	bindClick( "workshop_trade_refresh", [this]
			   { controller_->refreshTrade(); } );
	bindClick( "workshop_next_trade", [this]
			   { controller_->nextTradeRow(); } );
	bindClick( "workshop_trade_less", [this]
			   { controller_->adjustSelectedTradeOffer( -1 ); } );
	bindClick( "workshop_trade_more", [this]
			   { controller_->adjustSelectedTradeOffer( 1 ); } );
	bindClick( "workshop_trade_execute", [this]
			   { controller_->executeTrade(); } );
	bindClick( "workshop_trade_confirm", [this]
			   { controller_->confirmTrade(); } );
	bindClick( "workshop_trade_cancel", [this]
			   { controller_->cancelTrade(); } );

	bindClick( "stockpile_apply_basics", [this]
			   {const auto&s=controller_->state().stockpile.value;controller_->setStockpileBasics(formValue("stockpile_name"),priority("stockpile_priority",s.priority+1,s.maxPriority)-1,s.suspended,s.pullFromOthers,s.allowPullFromHere); } );
	bindClick( "stockpile_toggle_suspended", [this]
			   {const auto&s=controller_->state().stockpile.value;controller_->setStockpileBasics(s.name,s.priority,!s.suspended,s.pullFromOthers,s.allowPullFromHere); } );
	bindClick( "stockpile_toggle_pull", [this]
			   {const auto&s=controller_->state().stockpile.value;controller_->setStockpileBasics(s.name,s.priority,s.suspended,!s.pullFromOthers,s.allowPullFromHere); } );
	bindClick( "stockpile_toggle_allow_pull", [this]
			   {const auto&s=controller_->state().stockpile.value;controller_->setStockpileBasics(s.name,s.priority,s.suspended,s.pullFromOthers,!s.allowPullFromHere); } );
	bindClick( "stockpile_next_filter", [this]
			   { controller_->nextStockpileFilter(); } );
	bindClick( "stockpile_toggle_filter", [this]
			   { controller_->toggleSelectedStockpileFilter(); } );
	bindClick( "stockpile_next_content", [this]
			   { controller_->nextStockpileContent(); } );

	bindClick( "agriculture_apply_basics", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setAgricultureBasics(formValue("agriculture_name"),priority("agriculture_priority",s.priority,s.maxPriority),s.suspended); } );
	bindClick( "agriculture_toggle_suspended", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setAgricultureBasics(s.name,s.priority,!s.suspended); } );
	bindClick( "agriculture_next_product", [this]
			   { controller_->nextAgricultureProduct(); } );
	bindClick( "agriculture_apply_product", [this]
			   { controller_->applySelectedAgricultureProduct(); } );
	bindClick( "agriculture_toggle_harvest", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setHarvestOptions(!s.harvest,s.harvestHay,s.tame); } );
	bindClick( "agriculture_toggle_pasture_harvest", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setHarvestOptions(!s.harvest,s.harvestHay,s.tame); } );
	bindClick( "agriculture_toggle_hay", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setHarvestOptions(s.harvest,!s.harvestHay,s.tame); } );
	bindClick( "agriculture_toggle_tame", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setHarvestOptions(s.harvest,s.harvestHay,!s.tame); } );
	bindClick( "agriculture_toggle_pick", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setGroveOptions(!s.pick,s.plant,s.fell); } );
	bindClick( "agriculture_toggle_plant", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setGroveOptions(s.pick,!s.plant,s.fell); } );
	bindClick( "agriculture_toggle_fell", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setGroveOptions(s.pick,s.plant,!s.fell); } );
	bindClick( "agriculture_next_animal", [this]
			   { controller_->nextAgricultureAnimal(); } );
	bindClick( "agriculture_toggle_butcher", [this]
			   { controller_->toggleSelectedAnimalButchering(); } );
	bindClick( "agriculture_male_cap_down", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setPastureCap(Gender::Male,static_cast<std::uint32_t>(std::max(0,s.maxMale-1))); } );
	bindClick( "agriculture_male_cap_up", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setPastureCap(Gender::Male,static_cast<std::uint32_t>(s.maxMale+1)); } );
	bindClick( "agriculture_female_cap_down", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setPastureCap(Gender::Female,static_cast<std::uint32_t>(std::max(0,s.maxFemale-1))); } );
	bindClick( "agriculture_female_cap_up", [this]
			   {const auto&s=controller_->state().agriculture.value;controller_->setPastureCap(Gender::Female,static_cast<std::uint32_t>(s.maxFemale+1)); } );
	bindClick( "agriculture_toggle_first_food", [this]
			   {const auto&s=controller_->state().agriculture.value;if(!s.foods.empty())controller_->setPastureFood(s.foods.front().item,s.foods.front().material,!s.foods.front().allowed); } );
	stateChanged( controller.state() );
	return true;
}
void Management6ARmlBinding::shutdown()
{
	for ( auto& l : listeners_ )
		if ( l.target )
			l.target->RemoveEventListener( l.event, l.callback.get() );
	listeners_.clear();
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
void Management6ARmlBinding::bind( const char* id, const char* event, std::function<void( Rml::Event& )> fn )
{
	if ( auto* e = element( id ) )
	{
		auto cb = std::make_unique<Callback>( std::move( fn ) );
		e->AddEventListener( event, cb.get() );
		listeners_.push_back( { e, event, std::move( cb ) } );
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
bool Management6ARmlBinding::activateElement( std::string_view id )
{
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
		return true;
	}
	return false;
}
bool Management6ARmlBinding::setStockpileSearchForProbe( std::string_view value )
{
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_search" ) ) )
	{
		e->SetValue( std::string( value ) );
		if ( controller_ )
		{
			stockpilePage_ = 0;
			controller_->setSearch( std::string( value ) );
		}
		return true;
	}
	return false;
}
bool Management6ARmlBinding::activateFirstStockpileFilterForProbe( TriState state, FilterDepth depth )
{
	if ( !controller_ )
		return false;
	for ( const auto& row : controller_->state().stockpile.visibleFilters )
	{
		if ( row.state != state || row.id.depth != depth )
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
		if ( row.id.depth != FilterDepth::Material || row.id.item.value != item || row.id.material.value != material || row.state != TriState::On )
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

void Management6ARmlBinding::renderWorkshop( const WorkshopState& s )
{
	const auto status = s.request.status;
	visible( "workshop_loading", status == RequestStatus::Loading );
	visible( "workshop_empty", status == RequestStatus::Empty );
	visible( "workshop_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "workshop_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "workshop_error", s.request.message );
	text( "workshop_title", s.value.name.empty() ? textCatalog_.format( LocalizationKey{"workshop.title"} ) : s.value.name );
	text( "workshop_subtype", s.value.subtype.empty() ? "Production workshop" : s.value.subtype );
	formValue( "workshop_name", s.value.name );
	formValue( "workshop_priority", std::to_string( s.value.priority ) );
	text( "workshop_summary", "Priority " + std::to_string( s.value.priority ) + " / " + std::to_string( s.value.maxPriority ) + " | " + ( s.value.suspended ? "Suspended" : "Active" ) + " | linked stockpile " + ( s.value.connectStockpile ? "yes" : "no" ) );
	text( "workshop_toggle_suspended", toggleText( s.value.suspended ? "Resume" : "Suspend", s.value.suspended ) );
	text( "workshop_toggle_generated", toggleText( "Accept generated", s.value.acceptGenerated ) );
	text( "workshop_toggle_auto_missing", toggleText( "Auto-craft missing", s.value.autoCraftMissing ) );
	text( "workshop_toggle_linked", toggleText( "Link stockpile", s.value.connectStockpile ) );
	checked( "workshop_toggle_suspended", s.value.suspended );
	checked( "workshop_toggle_generated", s.value.acceptGenerated );
	checked( "workshop_toggle_auto_missing", s.value.autoCraftMissing );
	checked( "workshop_toggle_linked", s.value.connectStockpile );
	text( "workshop_sort", std::string( "Sort " ) + ( s.sort == SortDirection::Ascending ? "A-Z v" : "Z-A v" ) );
	const auto workshopRows = std::max( { s.visibleProducts.size(), s.visibleQueue.size(), s.traderRows.size() + s.playerRows.size() } );
	if ( workshopPage_ >= workshopRows )
		workshopPage_ = workshopRows ? pageSize_ * ( ( workshopRows - 1 ) / pageSize_ ) : 0;
	enabled( "workshop_page_previous", workshopPage_ > 0 );
	enabled( "workshop_page_next", workshopPage_ + pageSize_ < workshopRows );
	std::string products;
	for ( std::size_t index = workshopPage_; index < std::min( workshopPage_ + pageSize_, s.visibleProducts.size() ); ++index )
	{
		const auto& r       = s.visibleProducts[index];
		const bool selected = s.selectedProduct && *s.selectedProduct == r.id;
		products += "<button id='" + rowId( "m6a_product_", r.id.value ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-catalog='" + safe( r.id.value ) + "'>" + safe( r.id.value ) + " | " + std::to_string( r.components.size() ) + " component groups</button>";
	}
	if ( products.empty() )
		products = "<div class='c-m6a-row'>No crafts match the filter.</div>";
	if ( auto* e = element( "workshop_products" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( products );
		if ( restore && s.selectedProduct )
			if ( auto* row = element( rowId( "m6a_product_", s.selectedProduct->value ).c_str() ) )
				row->Focus();
	}
	text( "workshop_product_selection", s.selectedProduct ? "Selected craft: " + s.selectedProduct->value + ( s.selectionFiltered ? " (hidden by filter)" : "" ) : "No craft selected" );
	std::string queue;
	for ( std::size_t n = workshopPage_; n < std::min( workshopPage_ + pageSize_, s.visibleQueue.size() ); ++n )
	{
		const auto& r       = s.visibleQueue[n];
		const bool selected = s.selectedJob && *s.selectedJob == r.id;
		queue += "<button id='m6a_job_" + std::to_string( r.id.value ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-job='" + std::to_string( r.id.value ) + "'>#" + std::to_string( n + 1 ) + " " + safe( r.craft.value ) + " | " + mode( r.mode ) + " " + std::to_string( r.count ) + ( r.suspended ? " | paused" : "" ) + "</button>";
	}
	if ( queue.empty() )
		queue = "<div class='c-m6a-row'>No production orders match the filter.</div>";
	if ( auto* e = element( "workshop_queue" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( queue );
		if ( restore && s.selectedJob )
			if ( auto* row = element( ( "m6a_job_" + std::to_string( s.selectedJob->value ) ).c_str() ) )
				row->Focus();
	}
	text( "workshop_job_selection", s.selectedJob ? "Selected order ID " + std::to_string( s.selectedJob->value ) + ( s.selectionFiltered ? " (hidden by filter)" : "" ) : "No order selected" );
	const bool butcher = s.value.subtype == "Butcher";
	const bool fisher  = s.value.subtype == "Fisher" || s.value.catchFish || s.value.processFish;
	const bool trade   = s.value.subtype == "TradingPost" || s.tradeLoaded;
	visible( "workshop_butcher_actions", butcher );
	visible( "workshop_fisher_actions", fisher );
	visible( "workshop_special", butcher || fisher );
	visible( "workshop_trade", trade );
	text( "workshop_toggle_corpses", toggleText( "Butcher corpses", s.value.butcherCorpses ) );
	text( "workshop_toggle_excess", toggleText( "Butcher excess", s.value.butcherExcess ) );
	text( "workshop_toggle_catch", toggleText( "Catch fish", s.value.catchFish ) );
	text( "workshop_toggle_process", toggleText( "Process fish", s.value.processFish ) );
	checked( "workshop_toggle_corpses", s.value.butcherCorpses );
	checked( "workshop_toggle_excess", s.value.butcherExcess );
	checked( "workshop_toggle_catch", s.value.catchFish );
	checked( "workshop_toggle_process", s.value.processFish );
	std::string trades;
	auto tradeRowId = []( const TradeRowId& r )
	{ return std::string( "m6a_trade_" ) + ( r.party == TradeParty::Trader ? "trader" : "player" ) + "_" + hex( r.item.value ) + "_" + hex( r.materialOrGender.value ) + "_" + std::to_string( r.quality ); };
	std::size_t tradeIndex = 0;
	auto add               = [&]( const auto& rows, const char* party )
	{for(const auto&r:rows){const auto index=tradeIndex++;if(index<workshopPage_||index>=workshopPage_+pageSize_)continue;const bool selected=s.selectedTradeRow&&*s.selectedTradeRow==r.id;trades+="<button id='"+tradeRowId(r.id)+"' class='c-m6a-row-button"+std::string(selected?" is-selected":"")+"' data-party='"+std::string(party)+"' data-item='"+safe(r.id.item.value)+"' data-material='"+safe(r.id.materialOrGender.value)+"' data-quality='"+std::to_string(r.id.quality)+"'>"+(r.id.party==TradeParty::Trader?"Trader":"Settlement")+" | "+safe(r.name)+" | stock "+std::to_string(r.stock)+" | offered "+std::to_string(r.offered)+" | value "+std::to_string(r.unitValue)+"</button>";} };
	add( s.traderRows, "trader" );
	add( s.playerRows, "player" );
	if ( trades.empty() )
		trades = "<div class='c-m6a-row'>Trade inventory has not been loaded.</div>";
	if ( auto* e = element( "workshop_trade_rows" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( trades );
		if ( restore && s.selectedTradeRow )
			if ( auto* row = element( tradeRowId( *s.selectedTradeRow ).c_str() ) )
				row->Focus();
	}
	text( "workshop_trade_totals", "Trader offer " + std::to_string( s.traderOfferValue ) + " | settlement offer " + std::to_string( s.playerOfferValue ) );
	visible( "workshop_trade_confirmation", s.tradeConfirmationRequired );
	enabled( "workshop_trade_execute", s.tradeLoaded && !s.tradeConfirmationRequired );
	text( "workshop_status", s.request.refreshInProgress ? textCatalog_.format( LocalizationKey{"status.refreshing"} ) : std::string {} );
}
void Management6ARmlBinding::renderStockpile( const StockpileState& s )
{
	const auto status = s.request.status;
	visible( "stockpile_loading", status == RequestStatus::Loading );
	visible( "stockpile_empty", status == RequestStatus::Empty );
	visible( "stockpile_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "stockpile_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "stockpile_error", s.request.message );
	text( "stockpile_title", s.value.name.empty() ? textCatalog_.format( LocalizationKey{"stockpile.title"} ) : s.value.name );
	formValue( "stockpile_name", s.value.name );
	const auto displayPriority = std::max( 0, s.value.priority ) + 1;
	formValue( "stockpile_priority", std::to_string( displayPriority ) );
	text( "stockpile_summary", textCatalog_.format( LocalizationKey{"management.stockpile_summary"}, {{"items",std::to_string(s.value.itemCount)},{"capacity",std::to_string(s.value.capacity)},{"reserved",std::to_string(s.value.reserved)},{"priority",std::to_string(displayPriority)},{"maximum",std::to_string(s.value.maxPriority)}} ) );
	text( "stockpile_toggle_suspended", toggleText( s.value.suspended ? "Resume" : "Suspend", s.value.suspended ) );
	text( "stockpile_toggle_pull", toggleText( "Pull from others", s.value.pullFromOthers ) );
	text( "stockpile_toggle_allow_pull", toggleText( "Allow pull from here", s.value.allowPullFromHere ) );
	checked( "stockpile_toggle_suspended", s.value.suspended );
	checked( "stockpile_toggle_pull", s.value.pullFromOthers );
	checked( "stockpile_toggle_allow_pull", s.value.allowPullFromHere );
	text( "stockpile_sort", std::string( "Sort contents " ) + ( s.sort == SortDirection::Ascending ? "A-Z v" : "Z-A v" ) );
	const auto stockpileRows = std::max( s.visibleFilters.size(), s.visibleContents.size() );
	if ( stockpilePage_ >= stockpileRows )
		stockpilePage_ = stockpileRows ? pageSize_ * ( ( stockpileRows - 1 ) / pageSize_ ) : 0;
	enabled( "stockpile_page_previous", stockpilePage_ > 0 );
	enabled( "stockpile_page_next", stockpilePage_ + pageSize_ < stockpileRows );
	std::string filters;
	for ( std::size_t index = stockpilePage_; index < std::min( stockpilePage_ + pageSize_, s.visibleFilters.size() ); ++index )
	{
		const auto& r       = s.visibleFilters[index];
		const bool selected = s.selectedFilter && *s.selectedFilter == r.id;
		const auto checked = r.state == TriState::On ? "true" : r.state == TriState::Off ? "false" : "mixed";
		filters += "<button id='" + stockpileFilterRowId( r.id ) + "' class='c-m6a-row-button c-check-row c-m6a-filter-depth-" + std::to_string( static_cast<int>( r.id.depth ) ) + std::string( selected ? " is-selected" : "" ) + "' data-category='" + safe( r.id.category.value ) + "' data-group='" + safe( r.id.group.value ) + "' data-item='" + safe( r.id.item.value ) + "' data-material='" + safe( r.id.material.value ) + "' data-depth='" + std::to_string( static_cast<int>( r.id.depth ) ) + "' aria-selected='" + ( selected ? "true" : "false" ) + "' aria-checked='" + checked + "' aria-expanded='" + ( r.id.depth == FilterDepth::Material || r.state == TriState::Mixed ? "true" : "false" ) + "'>" + disclosure( r ) + check( r.state ) + " " + safe( r.label ) + "</button>";
	}
	if ( filters.empty() )
		filters = "<div class='c-m6a-row'>No allowed-content rules match the filter.</div>";
	if ( auto* e = element( "stockpile_filters" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( filters );
		if ( restore && s.selectedFilter )
			if ( auto* row = element( stockpileFilterRowId( *s.selectedFilter ).c_str() ) )
				row->Focus();
	}
	std::string filterSelection = "No filter row selected";
	if ( s.selectedFilter )
	{
		const auto selected = std::find_if( s.value.filters.begin(), s.value.filters.end(), [&]( const auto& row ) { return row.id == *s.selectedFilter; } );
		if ( selected != s.value.filters.end() )
			filterSelection = "Selected: " + selected->label + " | " + stateName( selected->state ) + " | depth " + std::to_string( static_cast<int>( selected->id.depth ) );
		else
			filterSelection = "Selected filter row is unavailable";
		if ( s.selectionFiltered )
			filterSelection += " (hidden by filter)";
	}
	text( "stockpile_filter_selection", filterSelection );
	auto contentRowId = []( const StockpileContentRowId& r )
	{ return "m6a_content_" + hex( r.item.value ) + "_" + hex( r.material.value ); };
	std::string rows;
	for ( std::size_t index = stockpilePage_; index < std::min( stockpilePage_ + pageSize_, s.visibleContents.size() ); ++index )
	{
		const auto& r       = s.visibleContents[index];
		const bool selected = s.selectedContent && *s.selectedContent == r.id;
		rows += "<button id='" + contentRowId( r.id ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-item='" + safe( r.id.item.value ) + "' data-material='" + safe( r.id.material.value ) + "'>" + safe( r.itemName ) + " | " + safe( r.materialName ) + " | " + std::to_string( r.count ) + "</button>";
	}
	if ( rows.empty() )
		rows = "<div class='c-m6a-row'>No contents match the filter.</div>";
	if ( auto* e = element( "stockpile_rows" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( rows );
		if ( restore && s.selectedContent )
			if ( auto* row = element( contentRowId( *s.selectedContent ).c_str() ) )
				row->Focus();
	}
	text( "stockpile_content_selection", s.selectedContent ? "Selected: " + s.selectedContent->item.value + " / " + s.selectedContent->material.value : "No content row selected" );
	text( "stockpile_status", s.request.refreshInProgress ? textCatalog_.format( LocalizationKey{"status.refreshing"} ) : std::string {} );
}
void Management6ARmlBinding::renderAgriculture( const AgricultureState& s )
{
	const auto status = s.request.status;
	visible( "agriculture_loading", status == RequestStatus::Loading );
	visible( "agriculture_empty", status == RequestStatus::Empty );
	visible( "agriculture_error", status == RequestStatus::Error || status == RequestStatus::Stale );
	visible( "agriculture_content", status == RequestStatus::Ready || status == RequestStatus::Stale );
	text( "agriculture_error", s.request.message );
	text( "agriculture_title", s.value.name.empty() ? textCatalog_.format( LocalizationKey{"agriculture.title"} ) : s.value.name );
	text( "agriculture_kind", kind( s.value.target.kind ) );
	formValue( "agriculture_name", s.value.name );
	formValue( "agriculture_priority", std::to_string( s.value.priority ) );
	text( "agriculture_summary", textCatalog_.format( LocalizationKey{"management.agriculture_summary"}, {{"priority",std::to_string(s.value.priority)},{"maximum",std::to_string(s.value.maxPriority)},{"plots",std::to_string(s.value.plots)},{"planted",std::to_string(s.value.planted)},{"ready",std::to_string(s.value.ready)}} ) );
	text( "agriculture_toggle_suspended", toggleText( s.value.suspended ? "Resume" : "Suspend", s.value.suspended ) );
	text( "agriculture_sort", std::string( "Sort " ) + ( s.sort == SortDirection::Ascending ? "A-Z v" : "Z-A v" ) );
	checked( "agriculture_toggle_suspended", s.value.suspended );
	visible( "agriculture_farm", s.value.target.kind == AgricultureKind::Farm );
	visible( "agriculture_pasture", s.value.target.kind == AgricultureKind::Pasture );
	visible( "agriculture_grove", s.value.target.kind == AgricultureKind::Grove );
	const auto agricultureRows = std::max( s.visibleCatalog.size(), s.visibleAnimals.size() );
	if ( agriculturePage_ >= agricultureRows )
		agriculturePage_ = agricultureRows ? pageSize_ * ( ( agricultureRows - 1 ) / pageSize_ ) : 0;
	enabled( "agriculture_page_previous", agriculturePage_ > 0 );
	enabled( "agriculture_page_next", agriculturePage_ + pageSize_ < agricultureRows );
	std::string products;
	for ( std::size_t index = agriculturePage_; index < std::min( agriculturePage_ + pageSize_, s.visibleCatalog.size() ); ++index )
	{
		const auto& r       = s.visibleCatalog[index];
		const bool selected = s.selectedProduct && *s.selectedProduct == r.id;
		products += "<button id='" + rowId( "m6a_agri_product_", r.id.value ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-catalog='" + safe( r.id.value ) + "'>" + safe( r.name ) + " | available " + std::to_string( r.available ) + "</button>";
	}
	if ( products.empty() )
		products = "<div class='c-m6a-row'>No products match the filter.</div>";
	if ( auto* e = element( "agriculture_products" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( products );
		if ( restore && s.selectedProduct )
			if ( auto* row = element( rowId( "m6a_agri_product_", s.selectedProduct->value ).c_str() ) )
				row->Focus();
	}
	text( "agriculture_product_selection", s.selectedProduct ? "Selected product: " + s.selectedProduct->value + ( s.selectionFiltered ? " (hidden by filter)" : "" ) : "No product selected" );
	text( "agriculture_toggle_harvest", toggleText( "Harvest", s.value.harvest ) );
	text( "agriculture_toggle_pasture_harvest", toggleText( "Harvest products", s.value.harvest ) );
	text( "agriculture_toggle_hay", toggleText( "Harvest hay", s.value.harvestHay ) );
	text( "agriculture_toggle_tame", toggleText( "Tame wild animals", s.value.tame ) );
	text( "agriculture_toggle_pick", toggleText( "Pick fruit", s.value.pick ) );
	text( "agriculture_toggle_plant", toggleText( "Plant trees", s.value.plant ) );
	text( "agriculture_toggle_fell", toggleText( "Fell trees", s.value.fell ) );
	checked( "agriculture_toggle_harvest", s.value.harvest );
	checked( "agriculture_toggle_pasture_harvest", s.value.harvest );
	checked( "agriculture_toggle_hay", s.value.harvestHay );
	checked( "agriculture_toggle_tame", s.value.tame );
	checked( "agriculture_toggle_pick", s.value.pick );
	checked( "agriculture_toggle_plant", s.value.plant );
	checked( "agriculture_toggle_fell", s.value.fell );
	text( "agriculture_pasture_summary", "Animals " + std::to_string( s.value.total ) + " / " + std::to_string( s.value.capacity ) + " | male cap " + std::to_string( s.value.maxMale ) + " | female cap " + std::to_string( s.value.maxFemale ) + " | food " + std::to_string( s.value.foodCurrent ) + " / " + std::to_string( s.value.foodMax ) + " | hay " + std::to_string( s.value.hayCurrent ) + " / " + std::to_string( s.value.hayMax ) );
	std::string animals;
	for ( std::size_t index = agriculturePage_; index < std::min( agriculturePage_ + pageSize_, s.visibleAnimals.size() ); ++index )
	{
		const auto& r       = s.visibleAnimals[index];
		const bool selected = s.selectedAnimal && *s.selectedAnimal == r.id;
		animals += "<button id='m6a_animal_" + std::to_string( r.id.value ) + "' class='c-m6a-row-button" + std::string( selected ? " is-selected" : "" ) + "' data-animal='" + std::to_string( r.id.value ) + "'>" + safe( r.name ) + " | " + ( r.gender == Gender::Male ? "male" : "female" ) + ( r.young ? " | young" : "" ) + ( r.butcher ? " | marked for butchering" : "" ) + "</button>";
	}
	if ( animals.empty() )
		animals = "<div class='c-m6a-row'>No animals match the filter.</div>";
	if ( auto* e = element( "agriculture_animals" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( animals );
		if ( restore && s.selectedAnimal )
			if ( auto* row = element( ( "m6a_animal_" + std::to_string( s.selectedAnimal->value ) ).c_str() ) )
				row->Focus();
	}
	text( "agriculture_animal_selection", s.selectedAnimal ? "Selected animal ID " + std::to_string( s.selectedAnimal->value ) : "No animal selected" );
	std::string foods;
	for ( const auto& r : s.value.foods )
		foods += "<div class='c-m6a-row c-check-row'>" + check( r.allowed ? TriState::On : TriState::Off ) + " " + safe( r.name ) + "</div>";
	if ( foods.empty() )
		foods = "<div class='c-m6a-row'>No pasture food rules are available.</div>";
	if ( auto* e = element( "agriculture_foods" ) )
		e->SetInnerRML( foods );
	enabled( "agriculture_toggle_first_food", !s.value.foods.empty() );
	text( "agriculture_status", s.request.refreshInProgress ? textCatalog_.format( LocalizationKey{"status.refreshing"} ) : std::string {} );
}
void Management6ARmlBinding::stateChanged( const Management6AState& s )
{
	if ( !workshop_ || !stockpile_ || !agriculture_ )
		return;
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
	renderWorkshop( s.workshop );
	renderStockpile( s.stockpile );
	renderAgriculture( s.agriculture );
	if ( s.workshop.tradeConfirmationRequired && !tradeConfirmationVisible_ )
	{
		if ( auto* confirm = element( "workshop_trade_confirm" ) )
			confirm->Focus();
	}
	else if ( !s.workshop.tradeConfirmationRequired && tradeConfirmationVisible_ )
	{
		if ( auto* review = element( "workshop_trade_execute" ) )
			review->Focus();
	}
	tradeConfirmationVisible_ = s.workshop.tradeConfirmationRequired;
	const std::string status  = s.pendingAction ? textCatalog_.format( LocalizationKey{"status.updating"} ) : s.status;
	if ( s.view == ManagementView::Workshop )
		text( "workshop_status", status );
	else if ( s.view == ManagementView::Stockpile )
		text( "stockpile_status", status );
	else if ( s.view == ManagementView::Agriculture )
		text( "agriculture_status", status );
}
} // namespace ingnomia::ui::management6a
