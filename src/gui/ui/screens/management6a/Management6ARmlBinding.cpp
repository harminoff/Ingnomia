/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6ARmlBinding.h"
#include "../../localization/RmlText.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdio>

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
std::string filterStateLabel( TriState value )
{
	return value == TriState::On ? "Allowed" : value == TriState::Mixed ? "Mixed" : "Blocked";
}
std::string filterStateClass( TriState value )
{
	return value == TriState::On ? "allowed" : value == TriState::Mixed ? "mixed" : "blocked";
}
std::string filterDisclosure( const StockpileFilterRow& row )
{
	return row.id.depth == FilterDepth::Material ? std::string {} : row.state == TriState::Mixed ? "v" : ">";
}
std::string filterFallbackGlyph( const StockpileFilterRow& row )
{
	if ( row.id.depth == FilterDepth::Category )
		return "C";
	if ( row.id.depth == FilterDepth::Group )
		return "G";
	char glyph = '?';
	for ( const unsigned char c : row.label )
		if ( std::isalnum( c ) )
		{
			glyph = static_cast<char>( std::toupper( c ) );
			break;
		}
	return std::string( 1, glyph );
}
std::string filterIcon( const StockpileFilterRow& row )
{
	if ( row.id.depth != FilterDepth::Material )
		return "<span class='c-m6a-filter-icon c-m6a-filter-icon--generic' aria-hidden='true'><span class='c-m6a-filter-generic-glyph'><span></span><span></span><span></span></span></span>";
	if ( row.icon.sheet.empty() || row.icon.width <= 0 || row.icon.height <= 0 )
		return "<span class='c-m6a-filter-icon c-m6a-filter-icon--fallback' aria-hidden='true'><strong>" + filterFallbackGlyph( row ) + "</strong></span>";
	for ( const unsigned char c : row.icon.sheet )
		if ( !( std::isalnum( c ) || c == '_' || c == '-' || c == '.' ) )
			return "<span class='c-m6a-filter-icon c-m6a-filter-icon--fallback' aria-hidden='true'><strong>" + filterFallbackGlyph( row ) + "</strong></span>";
	constexpr double frame = 24.0;
	const double scale = std::min( frame / static_cast<double>( row.icon.width ), frame / static_cast<double>( row.icon.height ) );
	char style[128] {};
	std::snprintf( style, sizeof style, "width:%.2fdp;height:%.2fdp;left:%.2fdp;top:%.2fdp;", static_cast<double>( row.icon.width ) * scale, static_cast<double>( row.icon.height ) * scale, ( 28.0 - static_cast<double>( row.icon.width ) * scale ) * 0.5, ( 26.0 - static_cast<double>( row.icon.height ) * scale ) * 0.5 );
	return "<span class='c-m6a-filter-icon'><img class='c-m6a-filter-icon-image' src='/tilesheet/" + row.icon.sheet + "' style='" + style + "' /></span>";
}
Rml::Element* dataElement( Rml::Element* target, Rml::Element* boundary, const char* attribute )
{
	for ( auto* element = target; element && element != boundary; element = element->GetParentNode() )
		if ( element->HasAttribute( attribute ) )
			return element;
	return nullptr;
}
std::string stockpileToggleText( std::string_view label, bool value )
{
	return std::string( label ) + ": " + ( value ? "On" : "Off" );
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
				   { controller_->close(); if ( closeHandler_ ) closeHandler_(); } );
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
	const auto stockpileSearch = [this]( const char* id )
	{
		bind( id, "input", [this, id]( Rml::Event& )
			  { if ( std::string_view( id ) == "stockpile_filter_search" ) controller_->setStockpileFilterSearch( formValue( id ) ); else controller_->setStockpileContentSearch( formValue( id ) ); } );
		bind( id, "change", [this, id]( Rml::Event& )
			  { if ( std::string_view( id ) == "stockpile_filter_search" ) controller_->setStockpileFilterSearch( formValue( id ) ); else controller_->setStockpileContentSearch( formValue( id ) ); } );
	};
	stockpileSearch( "stockpile_filter_search" );
	stockpileSearch( "stockpile_content_search" );
	bindClick( "stockpile_content_sort_item", [this] { controller_->setStockpileContentSort( StockpileSortKey::Item ); } );
	bindClick( "stockpile_content_sort_qty", [this] { controller_->setStockpileContentSort( StockpileSortKey::Quantity ); } );
	const auto commitStockpileBasics = [this]
	{
		if ( normalizingPriority_ )
			return;
		const auto& s = controller_->state().stockpile.value;
		const auto maximum = std::max( 1, s.maxPriority );
		controller_->setStockpileBasics( formValue( "stockpile_name" ), normalizePriority( "stockpile_priority", s.priority + 1, maximum ) - 1, s.suspended, s.pullFromOthers, s.allowPullFromHere );
	};
	bind( "stockpile_name", "change", [commitStockpileBasics]( Rml::Event& ) { commitStockpileBasics(); } );
	bind( "stockpile_priority", "change", [commitStockpileBasics]( Rml::Event& ) { commitStockpileBasics(); } );
	const auto adjustStockpilePriority = [this]( int delta )
	{
		const auto& s = controller_->state().stockpile.value;
		const auto maximum = std::max( 1, s.maxPriority );
		const auto current = normalizePriority( "stockpile_priority", s.priority + 1, maximum );
		const auto next = std::clamp( current + delta, 1, maximum );
		controller_->setStockpileBasics( formValue( "stockpile_name" ), next - 1, s.suspended, s.pullFromOthers, s.allowPullFromHere );
		if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_priority" ) ) )
			input->SetValue( std::to_string( next ) );
	};
	bindClick( "stockpile_priority_up", [adjustStockpilePriority] { adjustStockpilePriority( -1 ); } );
	bindClick( "stockpile_priority_down", [adjustStockpilePriority] { adjustStockpilePriority( 1 ); } );
	bind( "stockpile_name", "keydown", [this, commitStockpileBasics]( Rml::Event& e )
		  { const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) ); if ( key == Rml::Input::KI_RETURN ) { commitStockpileBasics(); e.StopPropagation(); } else if ( key == Rml::Input::KI_ESCAPE ) { if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_name" ) ) ) input->SetValue( controller_->state().stockpile.value.name ); e.StopPropagation(); } } );
	bind( "stockpile_priority", "keydown", [this, commitStockpileBasics]( Rml::Event& e )
		  { const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) ); if ( key == Rml::Input::KI_RETURN ) { commitStockpileBasics(); e.StopPropagation(); } else if ( key == Rml::Input::KI_ESCAPE ) { if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_priority" ) ) ) input->SetValue( std::to_string( std::clamp( std::max( 0, controller_->state().stockpile.value.priority ) + 1, 1, std::max( 1, controller_->state().stockpile.value.maxPriority ) ) ) ); e.StopPropagation(); } } );
	bind( "stockpile_manager_root", "keydown", [this]( Rml::Event& e )
		  {
			if ( e.GetTargetElement() != element( "stockpile_priority" ) )
				return;
			const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
			const bool modifier = e.GetParameter<int>( "ctrl_key", 0 ) != 0 || e.GetParameter<int>( "meta_key", 0 ) != 0;
			const bool clipboardEdit = modifier && !e.GetParameter<int>( "alt_key", 0 ) && ( key == Rml::Input::KI_A || key == Rml::Input::KI_C || key == Rml::Input::KI_V || key == Rml::Input::KI_X );
			if ( !priorityDigit( key ) && !priorityEditingKey( key ) && !clipboardEdit )
				e.StopPropagation();
		}, true );
	bind( "stockpile_manager_root", "textinput", [this]( Rml::Event& e )
		  {
			if ( e.GetTargetElement() != element( "stockpile_priority" ) )
				return;
			const auto value = e.GetParameter<Rml::String>( "text", "" );
			if ( !priorityTextIsNumeric( value ) )
				e.StopPropagation();
		}, true );
	bindClick( "stockpile_view_contents", [this] { controller_->setStockpilePane( StockpilePane::Contents ); } );
	bindClick( "stockpile_view_allow", [this] { controller_->setStockpilePane( StockpilePane::AllowList ); } );
	bindClick( "stockpile_restore_filter_search", [this] { controller_->restoreStockpileFilterSearch(); } );
	bindClick( "stockpile_hauling_toggle", [this]
			   { haulingOptionsExpanded_ = !haulingOptionsExpanded_; visible( "stockpile_hauling_options", haulingOptionsExpanded_ ); text( "stockpile_hauling_disclosure", haulingOptionsExpanded_ ? "v" : ">" ); if ( auto* e = element( "stockpile_hauling_toggle" ) ) e->SetAttribute( "aria-expanded", haulingOptionsExpanded_ ? "true" : "false" ); } );
	bind( "workshop_products", "click", [this]( Rml::Event& e )
		  {auto*target=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-catalog");const auto key=attr(target,"data-catalog");if(!key.empty())controller_->selectWorkshopProduct(CatalogId{key}); } );
	bind( "workshop_queue", "click", [this]( Rml::Event& e )
		  {auto*target=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-job");const auto key=attr(target,"data-job");if(!key.empty())controller_->selectWorkshopJob({static_cast<std::uint32_t>(std::stoul(key))}); } );
	bind( "workshop_product_selection", "click", [this]( Rml::Event& e )
		  {
			  if ( auto* target = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-order-mode" ) )
			  {
				  const auto value = attr( target, "data-order-mode" );
				  controller_->setWorkshopOrderMode( value == "maintain" ? CraftRepeatMode::Maintain : value == "repeat" ? CraftRepeatMode::Repeat : CraftRepeatMode::Once );
				  e.StopPropagation();
				  return;
			  }
			  if ( auto* target = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-material-index" ) )
			  {
				  controller_->cycleWorkshopOrderMaterial( static_cast<std::size_t>( std::stoul( attr( target, "data-material-index" ) ) ) );
				  e.StopPropagation();
			  }
		  } );
	bind( "workshop_job_selection", "click", [this]( Rml::Event& e )
		  {
			  auto* target = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-job-action" );
			  if ( !target ) return;
			  const auto action = attr( target, "data-job-action" );
			  const auto& state = controller_->state().workshop;
			  if ( !state.selectedJob ) return;
			  const auto row = std::find_if( state.value.queue.begin(), state.value.queue.end(), [&]( const auto& value ) { return value.id == *state.selectedJob; } );
			  if ( row == state.value.queue.end() ) return;
			  if ( action == "mode-once" )
				  controller_->setSelectedJob( CraftRepeatMode::Once, row->count, row->suspended, row->moveBack );
			  else if ( action == "mode-maintain" )
				  controller_->setSelectedJob( CraftRepeatMode::Maintain, row->count, row->suspended, row->moveBack );
			  else if ( action == "mode-repeat" )
				  controller_->setSelectedJob( CraftRepeatMode::Repeat, row->count, row->suspended, row->moveBack );
			  else if ( action == "apply-count" )
				  controller_->setSelectedJob( row->mode, static_cast<std::uint32_t>( priority( "workshop_job_count", static_cast<std::int32_t>( row->count ), 999 ) ), row->suspended, row->moveBack );
			  else if ( action == "toggle-suspended" )
				  controller_->setSelectedJob( row->mode, row->count, !row->suspended, row->moveBack );
			  else if ( action == "toggle-move-back" )
				  controller_->setSelectedJob( row->mode, row->count, row->suspended, !row->moveBack );
			  else if ( action == "front" ) controller_->moveSelectedJob( MoveDirection::Front );
			  else if ( action == "up" ) controller_->moveSelectedJob( MoveDirection::Up );
			  else if ( action == "down" ) controller_->moveSelectedJob( MoveDirection::Down );
			  else if ( action == "back" ) controller_->moveSelectedJob( MoveDirection::Back );
			  else if ( action == "cancel" ) controller_->cancelSelectedJob();
			  e.StopPropagation();
		  } );
	bind( "workshop_trade_rows", "click", [this]( Rml::Event& e )
		  {auto*target=e.GetTargetElement();const auto item=attr(target,"data-item"),material=attr(target,"data-material"),party=attr(target,"data-party"),quality=attr(target,"data-quality");if(!item.empty())controller_->selectTradeRow({party=="trader"?TradeParty::Trader:TradeParty::Player,CatalogId{item},CatalogId{material},static_cast<std::uint8_t>(std::stoul(quality))}); } );
	bind( "stockpile_filters", "click", [this]( Rml::Event& e )
			  {
				  auto* target = e.GetTargetElement();
				  if ( auto* disclosure = dataElement( target, e.GetCurrentElement(), "data-disclosure" ) )
				  {
					  controller_->toggleStockpileFilterExpansion( { controller_->state().stockpile.value.id, CatalogId { attr( disclosure, "data-category" ) }, CatalogId { attr( disclosure, "data-group" ) }, CatalogId { attr( disclosure, "data-item" ) }, CatalogId { attr( disclosure, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( disclosure, "data-depth" ) ) ) } );
					  e.StopPropagation();
				  }
			  }, true );
	bind( "stockpile_filters", "click", [this]( Rml::Event& e )
			  {
				  auto* target = e.GetTargetElement();
				  if ( auto* disclosure = dataElement( target, e.GetCurrentElement(), "data-disclosure" ) )
				  {
					  controller_->toggleStockpileFilterExpansion( { controller_->state().stockpile.value.id, CatalogId { attr( disclosure, "data-category" ) }, CatalogId { attr( disclosure, "data-group" ) }, CatalogId { attr( disclosure, "data-item" ) }, CatalogId { attr( disclosure, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( disclosure, "data-depth" ) ) ) } );
					  e.StopPropagation();
					  return;
				  }
				  auto* rule = dataElement( target, e.GetCurrentElement(), "data-rule" );
				  auto* row = dataElement( target, e.GetCurrentElement(), "data-category" );
				  if ( rule )
				  {
					  const auto category = attr( row, "data-category" );
					  if ( !category.empty() )
					  {
						  controller_->selectStockpileFilter( { controller_->state().stockpile.value.id, CatalogId { category }, CatalogId { attr( row, "data-group" ) }, CatalogId { attr( row, "data-item" ) }, CatalogId { attr( row, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( row, "data-depth" ) ) ) } );
						  controller_->toggleSelectedStockpileFilter();
					  }
					  e.StopPropagation();
					  return;
				  }
				  const auto category = attr( row, "data-category" );
				  if ( category.empty() ) return;
				  controller_->selectStockpileFilter( { controller_->state().stockpile.value.id, CatalogId { category }, CatalogId { attr( row, "data-group" ) }, CatalogId { attr( row, "data-item" ) }, CatalogId { attr( row, "data-material" ) }, static_cast<FilterDepth>( std::stoul( attr( row, "data-depth" ) ) ) } );
			  } );
	bind( "stockpile_filters", "keydown", [this]( Rml::Event& e )
			  {
				  const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
				  const auto* target = dataElement( e.GetTargetElement(), e.GetCurrentElement(), "data-category" );
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
				  else if ( key == Rml::Input::KI_RIGHT || key == Rml::Input::KI_LEFT )
				  {
					  const bool isOpen = attr( target, "aria-expanded" ) == "true";
					  if ( ( key == Rml::Input::KI_RIGHT && !isOpen ) || ( key == Rml::Input::KI_LEFT && isOpen ) )
						  controller_->toggleStockpileFilterExpansion( id );
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
		  {auto*t=dataElement(e.GetTargetElement(),e.GetCurrentElement(),"data-item");const auto item=attr(t,"data-item");if(!item.empty()){controller_->selectStockpileContent({CatalogId{item},CatalogId{attr(t,"data-material")}});e.StopPropagation();} } );
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
			   {const auto&s=controller_->state().workshop;controller_->setWorkshopOrderCount(static_cast<std::uint32_t>(priority("workshop_order_count",static_cast<std::int32_t>(s.orderCount),999)));controller_->queueSelectedCraftOrder(); } );
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
	bindStockpileTooltip( "stockpile_toggle_suspended",
							 [this] { return textCatalog_.format( LocalizationKey { controller_->state().stockpile.value.suspended ? "management.stockpile.stockpile_resume_tooltip" : "management.stockpile.stockpile_suspend_tooltip" } ); } );
	bindStockpileTooltip( "stockpile_toggle_pull",
							 [this] { return textCatalog_.format( LocalizationKey { "management.stockpile.stockpile_pull_from_others_tooltip" } ); } );
	bindStockpileTooltip( "stockpile_toggle_allow_pull",
							 [this] { return textCatalog_.format( LocalizationKey { "management.stockpile.stockpile_allow_pull_from_here_tooltip" } ); } );
	bindStockpileTooltip( "stockpile_priority_up",
							 [this] { return textCatalog_.format( LocalizationKey { "management.stockpile.stockpile_priority_up_tooltip" } ); } );
	bindStockpileTooltip( "stockpile_priority_down",
							 [this] { return textCatalog_.format( LocalizationKey { "management.stockpile.stockpile_priority_down_tooltip" } ); } );
	bindClick( "stockpile_allow_bulk", [this] { controller_->setStockpileFilterMatches( true ); } );
	bindClick( "stockpile_block_bulk", [this] { controller_->setStockpileFilterMatches( false ); } );
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
			l.target->RemoveEventListener( l.event, l.callback.get(), l.capture );
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
void Management6ARmlBinding::bind( const char* id, const char* event, std::function<void( Rml::Event& )> fn, bool capture )
{
	if ( auto* e = element( id ) )
	{
		auto cb = std::make_unique<Callback>( std::move( fn ) );
		e->AddEventListener( event, cb.get(), capture );
		listeners_.push_back( { e, event, std::move( cb ), capture } );
	}
}
void Management6ARmlBinding::bindStockpileTooltip( const char* id, std::function<std::string()> textValue )
{
	if ( !stockpile_ ) return;
	if ( auto* target = stockpile_->GetElementById( id ) )
	{
		auto show = std::make_unique<Callback>( [this, textValue]( Rml::Event& event ) { showStockpileTooltip( textValue(), event.GetCurrentElement() ); } );
		target->AddEventListener( "mouseover", show.get() );
		listeners_.push_back( { target, "mouseover", std::move( show ) } );
		auto focus = std::make_unique<Callback>( [this, textValue]( Rml::Event& event ) { showStockpileTooltip( textValue(), event.GetCurrentElement() ); } );
		target->AddEventListener( "focus", focus.get() );
		listeners_.push_back( { target, "focus", std::move( focus ) } );
		auto hide = std::make_unique<Callback>( [this]( Rml::Event& ) { hideStockpileTooltip(); } );
		target->AddEventListener( "mouseout", hide.get() );
		listeners_.push_back( { target, "mouseout", std::move( hide ) } );
		auto blur = std::make_unique<Callback>( [this]( Rml::Event& ) { hideStockpileTooltip(); } );
		target->AddEventListener( "blur", blur.get() );
		listeners_.push_back( { target, "blur", std::move( blur ) } );
	}
}
void Management6ARmlBinding::showStockpileTooltip( const std::string& textValue, Rml::Element* source )
{
	if ( !stockpile_ || !source ) return;
	if ( auto* tooltip = stockpile_->GetElementById( "stockpile_tooltip" ) )
	{
		tooltip->SetInnerRML( safe( textValue ) );
		const auto offset = source->GetAbsoluteOffset();
		const auto size = source->GetBox().GetSize();
		const auto dimensions = context_.GetDimensions();
		const bool placeLeft = offset.x > dimensions.x * 0.5f;
		tooltip->SetProperty( "transform", placeLeft ? "translate(-100%, -50%)" : "translateY(-50%)" );
		tooltip->SetProperty( "left", std::to_string( placeLeft ? offset.x - 10.0f : offset.x + size.x + 10.0f ) + "px" );
		tooltip->SetProperty( "top", std::to_string( offset.y + size.y * 0.5f ) + "px" );
		tooltip->SetClass( "is-visible", true );
		tooltip->SetAttribute( "aria-hidden", "false" );
	}
}
void Management6ARmlBinding::hideStockpileTooltip()
{
	if ( !stockpile_ ) return;
	if ( auto* tooltip = stockpile_->GetElementById( "stockpile_tooltip" ) )
	{
		tooltip->SetClass( "is-visible", false );
		tooltip->SetAttribute( "aria-hidden", "true" );
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
	if ( auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( element( "stockpile_filter_search" ) ) )
	{
		e->SetValue( std::string( value ) );
		if ( controller_ )
			controller_->setStockpileFilterSearch( std::string( value ) );
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
	std::string productEditor = "<div class='c-workshop-editor__empty'>Select a craft to configure an order.</div>";
	if ( s.selectedProduct )
	{
		const auto product = std::find_if( s.value.products.begin(), s.value.products.end(), [&]( const auto& row ) { return row.id == *s.selectedProduct; } );
		if ( product != s.value.products.end() )
		{
			const auto modeButton = [&]( const char* value, CraftRepeatMode valueMode, const char* label )
			{ return "<button class='c-button c-workshop-mode-button" + std::string( s.orderMode == valueMode ? " is-selected" : "" ) + "' data-order-mode='" + value + "'>" + label + "</button>"; };
			productEditor = "<div class='c-workshop-editor__title'>New order: " + safe( product->id.value ) + ( s.selectionFiltered ? " (hidden by filter)" : "" ) + "</div>";
			productEditor += "<div class='c-workshop-editor__label'>Order type</div><div class='c-workshop-mode-row'>" + modeButton( "once", CraftRepeatMode::Once, "Craft number" ) + modeButton( "maintain", CraftRepeatMode::Maintain, "Craft to" ) + modeButton( "repeat", CraftRepeatMode::Repeat, "Repeat" ) + "</div>";
			productEditor += "<label class='c-workshop-count-row'><span>Quantity</span><input id='workshop_order_count' class='text' type='number' min='1' max='999' value='" + std::to_string( s.orderCount ) + "'/></label>";
			if ( product->components.empty() )
				productEditor += "<div class='c-workshop-requirements'>No material requirements.</div>";
			else
			{
				productEditor += "<div class='c-workshop-editor__label'>Required materials</div><div class='c-workshop-requirements'>";
				for ( std::size_t index = 0; index < product->components.size(); ++index )
				{
					const auto& component = product->components[index];
					const auto material = index < s.orderMaterials.size() ? s.orderMaterials[index] : CatalogId { "any" };
					const auto choice = std::find_if( component.materials.begin(), component.materials.end(), [&]( const auto& entry ) { return entry.first == material; } );
					const auto available = choice == component.materials.end() ? 0u : choice->second;
					const bool shortage = available < component.amount;
					productEditor += "<button class='c-button c-workshop-material-button" + std::string( shortage ? " is-shortage" : "" ) + "' data-material-index='" + std::to_string( index ) + "'><span>" + safe( component.item.value ) + " x" + std::to_string( component.amount ) + "</span><strong>" + safe( material.value ) + " | in stock " + std::to_string( available ) + ( shortage ? " | ! needed" : "" ) + "</strong></button>";
				}
				productEditor += "</div><div class='c-workshop-editor__hint'>Click a material row to use the next valid material. Missing stock will leave the order pending.</div>";
			}
		}
	}
	if ( auto* e = element( "workshop_product_selection" ) )
		e->SetInnerRML( productEditor );
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
	std::string jobEditor = "<div class='c-workshop-editor__empty'>Select an order to edit it.</div>";
	if ( s.selectedJob )
	{
		const auto row = std::find_if( s.value.queue.begin(), s.value.queue.end(), [&]( const auto& value ) { return value.id == *s.selectedJob; } );
		if ( row != s.value.queue.end() )
		{
			const auto modeButton = [&]( const char* action, CraftRepeatMode valueMode, const char* label )
			{ return "<button class='c-button c-workshop-mode-button" + std::string( row->mode == valueMode ? " is-selected" : "" ) + "' data-job-action='" + action + "'>" + label + "</button>"; };
			jobEditor = "<div class='c-workshop-editor__title'>Order #" + std::to_string( row->id.value ) + ": " + safe( row->craft.value ) + ( s.selectionFiltered ? " (hidden by filter)" : "" ) + "</div>";
			jobEditor += "<div class='c-workshop-editor__label'>Order type</div><div class='c-workshop-mode-row'>" + modeButton( "mode-once", CraftRepeatMode::Once, "Craft number" ) + modeButton( "mode-maintain", CraftRepeatMode::Maintain, "Craft to" ) + modeButton( "mode-repeat", CraftRepeatMode::Repeat, "Repeat" ) + "</div>";
			jobEditor += "<div class='c-workshop-count-row'><span>Quantity</span><input id='workshop_job_count' class='text' type='number' min='1' max='999' value='" + std::to_string( row->count ) + "'/><button class='c-button' data-job-action='apply-count'>Apply</button></div>";
			jobEditor += "<div class='c-workshop-editor__summary'>Crafted " + std::to_string( row->alreadyCrafted ) + " | materials: ";
			for ( std::size_t index = 0; index < row->materials.size(); ++index )
				jobEditor += ( index ? ", " : "" ) + safe( row->materials[index].value );
			jobEditor += "</div><div class='c-workshop-actions'>";
			jobEditor += "<button class='c-button' data-job-action='toggle-suspended'>" + std::string( row->suspended ? "Resume" : "Suspend" ) + "</button>";
			jobEditor += "<button class='c-button" + std::string( row->moveBack ? " is-selected" : "" ) + "' data-job-action='toggle-move-back'>Move back when done</button>";
			jobEditor += "<button class='c-button' data-job-action='front'>Top</button><button class='c-button' data-job-action='up'>Up</button><button class='c-button' data-job-action='down'>Down</button><button class='c-button' data-job-action='back'>Bottom</button>";
			jobEditor += "<button class='c-button c-button--danger' data-job-action='cancel'>Cancel order</button></div>";
		}
	}
	if ( auto* e = element( "workshop_job_selection" ) )
		e->SetInnerRML( jobEditor );
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
	const auto maximumPriority = std::max( 1, s.value.maxPriority );
	const auto displayPriority = std::clamp( std::max( 0, s.value.priority ) + 1, 1, maximumPriority );
	formValue( "stockpile_priority", std::to_string( displayPriority ) );
	enabled( "stockpile_priority_up", displayPriority > 1 );
	enabled( "stockpile_priority_down", displayPriority < maximumPriority );
	text( "stockpile_summary", textCatalog_.format( LocalizationKey{"management.stockpile_summary"}, {{"items",std::to_string(s.value.itemCount)},{"reserved",std::to_string(s.value.reserved)},{"status",s.value.suspended ? "Suspended" : "Active"}} ) );
	text( "stockpile_toggle_suspended", s.value.suspended ? "Resume stockpile" : "Suspend stockpile" );
	text( "stockpile_toggle_pull", std::string( s.value.pullFromOthers ? "[x] " : "[ ] " ) + "Take from lower-priority stockpiles" );
	text( "stockpile_toggle_allow_pull", std::string( s.value.allowPullFromHere ? "[x] " : "[ ] " ) + "Let higher-priority stockpiles take from here" );
	checked( "stockpile_toggle_suspended", s.value.suspended );
	checked( "stockpile_toggle_pull", s.value.pullFromOthers );
	checked( "stockpile_toggle_allow_pull", s.value.allowPullFromHere );
	const auto pane = s.pane;
	visible( "stockpile_contents_pane", pane == StockpilePane::Contents );
	visible( "stockpile_allow_pane", pane == StockpilePane::AllowList );
	for ( const auto& tab : { std::pair { "stockpile_view_contents", pane == StockpilePane::Contents }, std::pair { "stockpile_view_allow", pane == StockpilePane::AllowList } } )
		if ( auto* e = element( tab.first ) )
		{
			e->SetClass( "is-selected", tab.second );
			e->SetAttribute( "aria-selected", tab.second ? "true" : "false" );
		}
	text( "stockpile_content_sort_item", "Item" );
	text( "stockpile_content_sort_qty", "Qty" );
	if ( auto* e = element( "stockpile_content_sort_item" ) )
		e->SetClass( "is-selected", s.contentSort == StockpileSortKey::Item );
	if ( auto* e = element( "stockpile_content_sort_qty" ) )
		e->SetClass( "is-selected", s.contentSort == StockpileSortKey::Quantity );
	formValue( "stockpile_filter_search", s.filterSearch );
	formValue( "stockpile_content_search", s.contentSearch );
	const auto isExpanded = [&]( const StockpileFilterRowId& id )
	{ return std::find( s.expandedFilters.begin(), s.expandedFilters.end(), id ) != s.expandedFilters.end(); };
	std::size_t allowedRules = 0;
	std::size_t totalRules = 0;
	std::size_t matchingRules = 0;
	for ( const auto& r : s.value.filters )
		if ( r.id.depth == FilterDepth::Material )
		{
			++totalRules;
			allowedRules += r.state == TriState::On ? 1 : 0;
		}
	for ( const auto& r : s.visibleFilters )
		if ( r.id.depth == FilterDepth::Material )
			++matchingRules;
	text( "stockpile_filter_count", std::to_string( allowedRules ) + " / " + std::to_string( totalRules ) + " allowed" );
	text( "stockpile_content_count", std::to_string( s.visibleContents.size() ) + " entries" );
	text( "stockpile_bulk_caption", s.filterSearch.empty() ? "Entire allow list" : std::to_string( matchingRules ) + " matching rules" );
	enabled( "stockpile_allow_bulk", matchingRules > 0 );
	enabled( "stockpile_block_bulk", matchingRules > 0 );
	visible( "stockpile_restore_filter_search", s.filterSearchRevealed );
	visible( "stockpile_nothing_allowed", totalRules > 0 && allowedRules == 0 );
	std::string filters;
	for ( const auto& r : s.visibleFilters )
	{
		const bool selected = s.selectedFilter && *s.selectedFilter == r.id;
		const auto checked = r.state == TriState::On ? "true" : r.state == TriState::Off ? "false" : "mixed";
		const auto rowAttrs = " data-category='" + safe( r.id.category.value ) + "' data-group='" + safe( r.id.group.value ) + "' data-item='" + safe( r.id.item.value ) + "' data-material='" + safe( r.id.material.value ) + "' data-depth='" + std::to_string( static_cast<int>( r.id.depth ) ) + "'";
		const auto rowClass = std::string( "c-m6a-filter-row c-m6a-filter-depth-" ) + std::to_string( static_cast<int>( r.id.depth ) ) + ( r.state == TriState::Mixed ? " is-mixed" : r.state == TriState::Off ? " is-off" : " is-on" ) + ( selected ? " is-selected" : "" );
		const auto disclosure = r.id.depth == FilterDepth::Material ? std::string {} : ( isExpanded( r.id ) ? "v" : ">" );
		filters += "<div id='" + stockpileFilterRowId( r.id ) + "' class='" + rowClass + "' tabindex='0' data-rule='true'" + rowAttrs + " aria-selected='" + ( selected ? "true" : "false" ) + "' aria-checked='" + checked + "' aria-expanded='" + ( isExpanded( r.id ) ? "true" : "false" ) + "' aria-label='" + safe( r.label ) + ": " + filterStateLabel( r.state ) + "'><button type='button' class='c-m6a-rule-button' data-rule='true'" + rowAttrs + " aria-checked='" + checked + "'><span id='" + stockpileFilterRowId( r.id ) + "_disclosure' class='c-m6a-disclosure-button' data-disclosure='true'" + rowAttrs + " role='button' tabindex='-1' aria-label='" + ( isExpanded( r.id ) ? "Collapse " : "Expand " ) + safe( r.label ) + "'>" + disclosure + "</span><span class='c-m6a-rule-check'>" + check( r.state ) + "</span>" + filterIcon( r ) + "<span class='c-m6a-filter-label'>" + safe( r.label ) + "</span><span class='c-m6a-filter-state'>" + filterStateLabel( r.state ) + "</span></button></div>";
	}
	if ( filters.empty() )
		filters = "<div class='c-m6a-row'>" + std::string( s.filterSearch.empty() ? "No allow-list rules are available." : "No rules match this search." ) + "</div>";
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
			filterSelection = "Selected rule: " + selected->label + " — " + filterStateLabel( selected->state );
		else
			filterSelection = "Selected filter row is unavailable";
		if ( s.selectionFiltered )
			filterSelection += " (hidden by filter)";
	}
	text( "stockpile_filter_selection", filterSelection );
	auto contentRowId = []( const StockpileContentRowId& r )
	{ return "m6a_content_" + hex( r.item.value ) + "_" + hex( r.material.value ); };
	std::string rows;
	for ( const auto& r : s.visibleContents )
	{
		const bool selected = s.selectedContent && *s.selectedContent == r.id;
		const auto ruleClass = std::string( r.allowed ? "c-m6a-content-rule--allowed" : "c-m6a-content-rule--blocked" );
		rows += "<button id='" + contentRowId( r.id ) + "' class='c-m6a-row-button c-m6a-content-row" + std::string( selected ? " is-selected" : "" ) + "' data-item='" + safe( r.id.item.value ) + "' data-material='" + safe( r.id.material.value ) + "' aria-label='" + safe( r.itemName ) + ": " + safe( r.materialName ) + ", " + std::to_string( r.count ) + " stored, " + ( r.allowed ? "Allowed" : "Blocked" ) + "'><span class='c-m6a-content-name'>" + safe( r.itemName ) + "</span><span class='c-m6a-content-material'>" + safe( r.materialName ) + "</span><span class='c-m6a-content-count'>" + std::to_string( r.count ) + "</span><span class='c-m6a-content-rule " + ruleClass + "'>" + ( r.allowed ? "Allowed" : "Blocked" ) + "</span></button>";
	}
	if ( rows.empty() )
		rows = "<div class='c-m6a-row'>" + std::string( s.contentSearch.empty() ? "No items stored here yet." : "No stored items match this search." ) + "</div>";
	if ( auto* e = element( "stockpile_rows" ) )
	{
		const bool restore = e->Contains( context_.GetFocusElement() );
		e->SetInnerRML( rows );
		if ( restore && s.selectedContent )
			if ( auto* row = element( contentRowId( *s.selectedContent ).c_str() ) )
				row->Focus();
	}
	std::string contentSelection = "No content row selected";
	if ( s.selectedContent )
	{
		const auto selected = std::find_if( s.value.contents.begin(), s.value.contents.end(), [&]( const auto& row ) { return row.id == *s.selectedContent; } );
		contentSelection = selected == s.value.contents.end() ? "Selected content row is unavailable" : "Selected item: " + selected->itemName + " — " + selected->materialName + " — " + std::to_string( selected->count ) + " stored";
	}
	text( "stockpile_content_selection", contentSelection );
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
