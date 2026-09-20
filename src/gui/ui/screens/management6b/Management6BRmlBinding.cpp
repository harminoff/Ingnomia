/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6BRmlBinding.h"

#include "../../localization/RmlText.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <sstream>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>

namespace ingnomia::ui::management6b
{
namespace
{
std::string esc( const std::string& v )
{
	return Rml::StringUtilities::EncodeRml( v );
}
std::string stableId( std::string_view prefix, std::string_view value )
{
	std::uint64_t h = 14695981039346656037ull;
	for ( const auto c : value )
	{
		h ^= static_cast<unsigned char>( c );
		h *= 1099511628211ull;
	}
	std::ostringstream out;
	out << prefix << std::hex << std::setfill( '0' ) << std::setw( 16 ) << h;
	return out.str();
}
std::string inventoryKey( const InventoryRowId& i )
{
	return std::to_string( static_cast<unsigned>( i.depth ) ) + "\n" + i.category.value + "\n" + i.group.value + "\n" + i.item.value + "\n" + i.material.value;
}
const char* depth( InventoryDepth d )
{
	switch ( d )
	{
		case InventoryDepth::Category:
			return "category";
		case InventoryDepth::Group:
			return "group";
		case InventoryDepth::Item:
			return "item";
		case InventoryDepth::Material:
			return "material";
	}
	return "item";
}
std::string activityText( std::string value, bool active )
{
	if ( value.rfind( "[ ] ", 0 ) == 0 || value.rfind( "[x] ", 0 ) == 0 )
		value.erase( 0, 4 );
	return std::string( active ? "[x] " : "[ ] " ) + value;
}
std::string inventoryFallbackSprite( const InventoryRow& r )
{
	if ( r.id.depth == InventoryDepth::Category || r.id.depth == InventoryDepth::Group )
		return {};
	char glyph = '?';
	for ( const unsigned char c : r.name )
	{
		if ( std::isalnum( c ) )
		{
			glyph = static_cast<char>( std::toupper( c ) );
			break;
		}
	}
	return "<span class='l-m6b-row__sprite l-m6b-row__sprite--fallback' aria-label='" + esc( r.name ) + "'><strong>" + std::string( 1, glyph ) + "</strong></span>";
}
std::string inventorySprite( const InventoryRow& r )
{
	if ( r.id.depth == InventoryDepth::Category || r.id.depth == InventoryDepth::Group )
		return {};
	if ( r.spriteSheet.empty() || r.spriteWidth <= 0 || r.spriteHeight <= 0 )
		return inventoryFallbackSprite( r );
	for ( const unsigned char c : r.spriteSheet )
		if ( !( std::isalnum( c ) || c == '_' || c == '-' || c == '.' ) )
			return inventoryFallbackSprite( r );
	constexpr double frame = 40.0;
	constexpr double cell = 40.0;
	constexpr double cellHeight = 40.0;
	const int sheetWidth = r.spriteSheetWidth > 0 ? r.spriteSheetWidth : r.spriteWidth;
	const int sheetHeight = r.spriteSheetHeight > 0 ? r.spriteSheetHeight : r.spriteHeight;
	const double scale = std::min( frame / static_cast<double>( r.spriteWidth ), frame / static_cast<double>( r.spriteHeight ) );
	char style[160] {};
	const double left = ( cell - frame ) * 0.5 + ( frame - static_cast<double>( r.spriteWidth ) * scale ) * 0.5 - static_cast<double>( r.spriteX ) * scale;
	const double top = ( cellHeight - frame ) * 0.5 + ( frame - static_cast<double>( r.spriteHeight ) * scale ) * 0.5 - static_cast<double>( r.spriteY ) * scale;
	std::snprintf( style, sizeof style, "width:%.2fdp;height:%.2fdp;left:%.2fdp;top:%.2fdp;", static_cast<double>( sheetWidth ) * scale, static_cast<double>( sheetHeight ) * scale, left, top );
	return "<span class='l-m6b-row__sprite'><img class='l-m6b-row__sprite-image' src='/tilesheet/" + r.spriteSheet + "' style='" + style + "' /></span>";
}
ScheduleActivity next( ManagedScheduleActivity a )
{
	switch ( a )
	{
		case ManagedScheduleActivity::None:
			return ScheduleActivity::Eat;
		case ManagedScheduleActivity::Eat:
			return ScheduleActivity::Sleep;
		case ManagedScheduleActivity::Sleep:
			return ScheduleActivity::Training;
		case ManagedScheduleActivity::Training:
			return ScheduleActivity::None;
	}
	return ScheduleActivity::None;
}
} // namespace
Management6BRmlBinding::Management6BRmlBinding( Rml::Context& c ) :
	context_( c )
{
}
Management6BRmlBinding::~Management6BRmlBinding()
{
	shutdown();
}
bool Management6BRmlBinding::initialize( Management6BController& c )
{
	controller_ = &c;
	const auto load = [this]( const char* path )
		{ return documentLoader_ ? documentLoader_( path ) : context_.LoadDocument( path ); };
	population_ = load( "windows/population_manager.rml" );
	inventory_  = load( "windows/inventory_browser.rml" );
	if ( !population_ || !inventory_ )
	{
		shutdown();
		return false;
	}
	localization::applyRmlText( *population_, textCatalog_ );
	localization::applyRmlText( *inventory_, textCatalog_ );
	bind( population_, "population_close", [this]
		  { closePopulation(); } );
	bind( population_, "population_tab_citizens", [this]
		  { controller_->open( View::Citizens ); } );
	bind( population_, "population_tab_professions", [this]
		  { controller_->open( View::Professions ); } );
	bind( population_, "population_tab_schedules", [this]
		  { controller_->open( View::Schedules ); } );
	bind( population_, "population_refresh", [this]
		  { controller_->refresh(); } );
	bind( population_, "population_sort_name", [this]
		  { controller_->setPopulationSort( Sort::Name ); } );
	bind( population_, "population_sort_profession", [this]
		  { controller_->setPopulationSort( Sort::Profession ); } );
	bind( population_, "creature_back", [this]
		  { controller_->open( View::Citizens ); } );
	bind( population_, "population_page_previous", [this]
		  { controller_->changePopulationPage( -1 ); } );
	bind( population_, "population_page_next", [this]
		  { controller_->changePopulationPage( 1 ); } );
	bind( population_, "schedule_set_none", [this]
		  { controller_->activateScheduleCell( ScheduleActivity::None ); } );
	bind( population_, "schedule_set_eat", [this]
		  { controller_->activateScheduleCell( ScheduleActivity::Eat ); } );
	bind( population_, "schedule_set_sleep", [this]
		  { controller_->activateScheduleCell( ScheduleActivity::Sleep ); } );
	bind( population_, "schedule_set_training", [this]
		  { controller_->activateScheduleCell( ScheduleActivity::Training ); } );
	bindEvent( population_, "creature_skills", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto skill=x->GetAttribute<Rml::String>("data-skill","");if(skill.empty()||!controller_->state().selectedCreature)continue;controller_->setSkill(*controller_->state().selectedCreature,CatalogId{skill},x->GetAttribute<Rml::String>("data-active","")!="true");break;} } );
	bindEvent( population_, "creature_professions", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto profession=x->GetAttribute<Rml::String>("data-profession","");if(profession.empty()||!controller_->state().selectedCreature)continue;controller_->setProfession(*controller_->state().selectedCreature,ProfessionId{profession});break;} } );
	bindEvent( population_, "population_rows", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto raw=x->GetAttribute<Rml::String>("data-creature","");if(raw.empty())continue;try{controller_->selectCreature(CreatureId{static_cast<std::uint32_t>(std::stoul(raw))});}catch(...){}break;} } );
	bindEvent( population_, "population_rows", "keydown", [this]( Rml::Event& e )
			   {const auto key=static_cast<Rml::Input::KeyIdentifier>(e.GetParameter<int>("key_identifier",0));if(key==Rml::Input::KI_UP){controller_->movePopulationSelection(-1);focusPopulationRow();}else if(key==Rml::Input::KI_DOWN){controller_->movePopulationSelection(1);focusPopulationRow();}else if(key==Rml::Input::KI_RETURN&&controller_->state().selectedCreature)controller_->selectCreature(*controller_->state().selectedCreature);else return;e.StopPropagation(); } );
	auto populationSearchChanged = [this]( Rml::Event& e )
	{
		if ( auto* x = e.GetCurrentElement() )
		{
			auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( x );
			controller_->setPopulationFilter( control ? control->GetValue() : x->GetAttribute<Rml::String>( "value", "" ) );
			if ( auto* search = population_->GetElementById( "population_search" ) )
				search->Focus();
		}
	};
	bindEvent( population_, "population_search", "input", populationSearchChanged );
	bindEvent( population_, "population_search", "change", populationSearchChanged );
	bindEvent( population_, "schedule_rows", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto creature=x->GetAttribute<Rml::String>("data-creature","");const auto hour=x->GetAttribute<Rml::String>("data-hour","");if(creature.empty()||hour.empty())continue;try{controller_->selectScheduleCell({CreatureId{static_cast<std::uint32_t>(std::stoul(creature))},static_cast<std::uint8_t>(std::stoul(hour))});focusScheduleCell();}catch(...){}break;} } );
	bindEvent( population_, "schedule_rows", "keydown", [this]( Rml::Event& e )
			   {const auto key=static_cast<Rml::Input::KeyIdentifier>(e.GetParameter<int>("key_identifier",0));if(key==Rml::Input::KI_LEFT)controller_->moveScheduleFocus(-1,0);else if(key==Rml::Input::KI_RIGHT)controller_->moveScheduleFocus(1,0);else if(key==Rml::Input::KI_UP)controller_->moveScheduleFocus(0,-1);else if(key==Rml::Input::KI_DOWN)controller_->moveScheduleFocus(0,1);else if(key==Rml::Input::KI_HOME)controller_->moveScheduleFocus(-24,0);else if(key==Rml::Input::KI_END)controller_->moveScheduleFocus(24,0);else if(key==Rml::Input::KI_PRIOR)controller_->moveScheduleFocus(0,-1);else if(key==Rml::Input::KI_NEXT)controller_->moveScheduleFocus(0,1);else if((key==Rml::Input::KI_RETURN||key==Rml::Input::KI_SPACE)&&controller_->state().selectedScheduleCell){const auto cell=*controller_->state().selectedScheduleCell;const auto row=std::ranges::find_if(controller_->state().schedules,[&](const auto&r){return r.creature==cell.creature;});if(row!=controller_->state().schedules.end())controller_->activateScheduleCell(next(row->hours[cell.hour]));}else return;e.StopPropagation();focusScheduleCell(); } );
	bind( inventory_, "inventory_close", [this]
		  { closeInventory(); } );
	bind( inventory_, "inventory_filter_owned", [this]
		  { controller_->setInventoryOwnedOnly( !controller_->state().inventoryOwnedOnly ); } );
	bind( inventory_, "inventory_sort_name", [this]
		  { controller_->setInventorySort( Sort::Name ); } );
	bind( inventory_, "inventory_sort_total", [this]
		  { controller_->setInventorySort( Sort::Total ); } );
	bind( inventory_, "inventory_sort_stock", [this]
		  { controller_->setInventorySort( Sort::Stock ); } );
	bindEvent( inventory_, "inventory_rows", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto d=x->GetAttribute<Rml::String>("data-depth","");if(d.empty())continue;const InventoryRowId id{CatalogId{x->GetAttribute<Rml::String>("data-category","")},CatalogId{x->GetAttribute<Rml::String>("data-group","")},CatalogId{x->GetAttribute<Rml::String>("data-item","")},CatalogId{x->GetAttribute<Rml::String>("data-material","")},d=="category"?InventoryDepth::Category:d=="group"?InventoryDepth::Group:d=="material"?InventoryDepth::Material:InventoryDepth::Item};if(id.depth==InventoryDepth::Category||id.depth==InventoryDepth::Group||id.depth==InventoryDepth::Item)controller_->toggleInventoryExpanded(id);else{controller_->selectInventory(id);focusInventoryRow();}break;} } );
	bindEvent( inventory_, "inventory_rows", "keydown", [this]( Rml::Event& e )
	{
		const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
		if ( key == Rml::Input::KI_UP )
		{
			controller_->moveInventorySelection( -1 );
			focusInventoryRow();
		}
		else if ( key == Rml::Input::KI_DOWN )
		{
			controller_->moveInventorySelection( 1 );
			focusInventoryRow();
		}
		else if ( key == Rml::Input::KI_HOME )
		{
			controller_->moveInventorySelection( -2147483647 );
			focusInventoryRow();
		}
		else if ( key == Rml::Input::KI_END )
		{
			controller_->moveInventorySelection( 2147483647 );
			focusInventoryRow();
		}
		else if ( key == Rml::Input::KI_RETURN || key == Rml::Input::KI_SPACE )
		{
			bool section = false;
			for ( auto* x = e.GetTargetElement(); x && x != e.GetCurrentElement(); x = x->GetParentNode() )
			{
				const auto d = x->GetAttribute<Rml::String>( "data-depth", "" );
				if ( d.empty() )
					continue;
				section = d == "category" || d == "group" || d == "item";
				if ( section )
				{
					const InventoryRowId id { CatalogId { x->GetAttribute<Rml::String>( "data-category", "" ) }, CatalogId { x->GetAttribute<Rml::String>( "data-group", "" ) }, CatalogId { x->GetAttribute<Rml::String>( "data-item", "" ) }, CatalogId { x->GetAttribute<Rml::String>( "data-material", "" ) }, d == "category" ? InventoryDepth::Category : d == "group" ? InventoryDepth::Group : InventoryDepth::Item };
					controller_->toggleInventoryExpanded( id );
				}
				break;
			}
			if ( !section && key == Rml::Input::KI_SPACE )
				controller_->toggleSelectedWatch();
		}
		else
			return;
		e.StopPropagation();
	} );
	auto inventorySearchChanged = [this]( Rml::Event& e )
	{
		if ( auto* x = e.GetCurrentElement() )
		{
			auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( x );
			controller_->setInventoryFilter( control ? control->GetValue() : x->GetAttribute<Rml::String>( "value", "" ) );
			// Updating the filter synchronously rebuilds the result rows and tabs.
			// RmlUi may clear focus during that DOM work, so restore the existing
			// native input after the controller notification has completed.
			if ( auto* search = inventory_->GetElementById( "inventory_search" ); search && context_.GetFocusElement() != search )
				search->Focus();
		}
	};
	bindEvent( inventory_, "inventory_search", "input", inventorySearchChanged );
	bindEvent( inventory_, "inventory_search", "change", inventorySearchChanged );
	bindEvent( inventory_, "inventory_category_tabs", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto category=x->GetAttribute<Rml::String>("data-category","");if(category.empty()&&!x->HasAttribute("data-category"))continue;controller_->setInventoryCategory(std::string(category));e.StopPropagation();break;} } );
	population_->Show();
	inventory_->Show();
	stateChanged( c.state() );
	return true;
}
void Management6BRmlBinding::shutdown()
{
	for ( auto& l : listeners_ )
		if ( l.target )
			l.target->RemoveEventListener( l.event, l.callback.get() );
	listeners_.clear();
	if ( population_ )
	{
		context_.UnloadDocument( population_ );
		population_ = nullptr;
	}
	if ( inventory_ )
	{
		context_.UnloadDocument( inventory_ );
		inventory_ = nullptr;
	}
	controller_ = nullptr;
}
bool Management6BRmlBinding::openPopulation( FocusToken focus )
{
	if ( !controller_ || !focus )
		return false;
	activeRoute_     = RouteId { "workbench.population" };
	returnFocus_     = focus;
	populationFocus_ = focus;
	controller_->openPopulation();
	if ( auto* e = population_->GetElementById( "population_heading" ) )
		e->Focus();
	return true;
}
bool Management6BRmlBinding::openInventory( FocusToken focus )
{
	if ( !controller_ || !focus )
		return false;
	activeRoute_    = RouteId { "workbench.inventory" };
	returnFocus_    = focus;
	inventoryFocus_ = focus;
	controller_->openInventory();
	if ( auto* e = inventory_->GetElementById( "inventory_heading" ) )
		e->Focus();
	return true;
}
void Management6BRmlBinding::closePopulation()
{
	if ( !controller_ || !controller_->state().populationOpen )
		return;
	controller_->closePopulation();
	if ( routeCloseHandler_ )
		routeCloseHandler_( RouteId { "workbench.population" }, populationFocus_ );
	populationFocus_ = {};
}
void Management6BRmlBinding::closeInventory()
{
	if ( !controller_ || !controller_->state().inventoryOpen )
		return;
	controller_->closeInventory();
	if ( routeCloseHandler_ )
		routeCloseHandler_( RouteId { "workbench.inventory" }, inventoryFocus_ );
	inventoryFocus_ = {};
}
void Management6BRmlBinding::closeRoute()
{
	if ( !controller_ || !controller_->state().open )
		return;
	const auto route = activeRoute_;
	const auto focus = returnFocus_;
	controller_->close();
	activeRoute_.reset();
	returnFocus_ = {};
	if ( route && routeCloseHandler_ )
		routeCloseHandler_( *route, focus );
}
void Management6BRmlBinding::bind( Rml::ElementDocument* d, const char* id, std::function<void()> fn )
{
	bindEvent( d, id, "click", [fn = std::move( fn )]( Rml::Event& )
			   { fn(); } );
}
void Management6BRmlBinding::bindEvent( Rml::ElementDocument* d, const char* id, const char* event, std::function<void( Rml::Event& )> fn )
{
	if ( auto* e = d->GetElementById( id ) )
	{
		auto cb = std::make_unique<Callback>( std::move( fn ) );
		e->AddEventListener( event, cb.get() );
		listeners_.push_back( { e, event, std::move( cb ) } );
	}
}
void Management6BRmlBinding::focusPopulationRow()
{
	if ( !population_ || !controller_->state().selectedCreature )
		return;
	if ( auto* e = population_->GetElementById( "population_creature_" + std::to_string( controller_->state().selectedCreature->value ) ) )
		e->Focus();
}
void Management6BRmlBinding::focusInventoryRow()
{
	if ( !inventory_ || !controller_->state().selectedInventory )
		return;
	if ( auto* e = inventory_->GetElementById( stableId( "inventory_row_", inventoryKey( *controller_->state().selectedInventory ) ) ) )
		e->Focus();
}
void Management6BRmlBinding::focusScheduleCell()
{
	if ( !population_ || !controller_->state().selectedScheduleCell )
		return;
	const auto& c = *controller_->state().selectedScheduleCell;
	if ( auto* e = population_->GetElementById( "schedule_" + std::to_string( c.creature.value ) + "_" + std::to_string( c.hour ) ) )
		e->Focus();
}
void Management6BRmlBinding::text( Rml::ElementDocument* d, const char* id, const std::string& v )
{
	if ( d )
		if ( auto* e = d->GetElementById( id ) )
			e->SetInnerRML( esc( v ) );
}
void Management6BRmlBinding::rml( Rml::ElementDocument* d, const char* id, const std::string& v )
{
	if ( d )
		if ( auto* e = d->GetElementById( id ) )
			e->SetInnerRML( v );
}
void Management6BRmlBinding::visible( Rml::ElementDocument* d, const char* id, bool v )
{
	if ( d )
		if ( auto* e = d->GetElementById( id ) )
		{
			e->SetClass( "is-hidden", !v );
			e->SetProperty( "display", v ? "block" : "none" );
		}
}
bool Management6BRmlBinding::activateElement( std::string_view id )
{
	for ( auto* d : { population_, inventory_ } )
		if ( d )
			if ( auto* e = d->GetElementById( std::string( id ) ) )
			{
				e->DispatchEvent( "click", Rml::Dictionary {} );
				return true;
			}
	return false;
}
bool Management6BRmlBinding::activateFirstDataElement( std::string_view kind )
{
	if ( !controller_ )
		return false;
	if ( kind == "population" )
	{
		const auto rows = controller_->populationPage();
		if ( rows.empty() )
			return false;
		return activateElement( "population_creature_" + std::to_string( rows.front().id.value ) );
	}
	if ( kind == "schedule" )
	{
		if ( controller_->state().schedules.empty() )
			return false;
		return activateElement( "schedule_" + std::to_string( controller_->state().schedules.front().creature.value ) + "_0" );
	}
	if ( kind == "inventory" )
	{
		const auto rows = controller_->inventoryPage();
		const auto row = std::ranges::find_if( rows, []( const auto& r )
			{ return r.id.depth != InventoryDepth::Category && r.id.depth != InventoryDepth::Group; } );
		if ( row == rows.end() )
			return false;
		return activateElement( stableId( "inventory_row_", inventoryKey( row->id ) ) );
	}
	return false;
}
void Management6BRmlBinding::stateChanged( const Management6BState& s )
{
	if ( !population_ || !inventory_ )
		return;
	if ( !presentationEnabled_ )
	{
		population_->Hide();
		inventory_->Hide();
		return;
	}
	auto tr = [this]( const char* key, std::initializer_list<localization::TextArgument> args = {} )
	{ return textCatalog_.format( LocalizationKey { key }, args ); };
	const bool pop = s.populationOpen;
	const bool inv = s.inventoryOpen;
	if ( pop )
		population_->Show();
	else
		population_->Hide();
	if ( inv )
		inventory_->Show();
	else
		inventory_->Hide();
	visible( population_, "population_root", pop );
	visible( inventory_, "inventory_root", inv );
	visible( population_, "population_citizens", pop && ( s.populationView == View::Citizens ) );
	visible( population_, "population_professions", pop && ( s.populationView == View::Professions ) );
	visible( population_, "population_schedules", pop && ( s.populationView == View::Schedules ) );
	visible( population_, "creature_detail", pop && ( s.populationView == View::Creature ) );
	visible( population_, "population_loading", s.loadingPopulation );
	visible( inventory_, "inventory_loading", s.loadingInventory );
	// Owned-only still presents the full category/group/item hierarchy.  Only
	// text and category searches use the compact filtered-row alignment.
	const bool filteredInventory = !s.inventoryCategory.empty() || !s.inventoryFilter.empty();
	if ( auto* e = inventory_->GetElementById( "inventory_workbench" ) )
	{
		e->SetClass( "is-filtered", filteredInventory );
	}
	// Keep the native form value synchronized with the pointer-free controller
	// state.  Inventory snapshots can arrive while the user is typing; without
	// this restore, the DOM may retain an old value (or clear it) and the next
	// input event appears to require a second click/focus.
	if ( auto* search = inventory_->GetElementById( "inventory_search" ) )
	{
		if ( auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( search ) )
			if ( control->GetValue() != s.inventoryFilter ) control->SetValue( s.inventoryFilter );
	}
	if ( auto* e = inventory_->GetElementById( "inventory_column_head" ) )
		e->SetProperty( "display", "flex" );
	std::string categoryTabs = "<button id='inventory_category_all' class='c-tabs__tab l-m6b-tab" + ( s.inventoryCategory.empty() ? std::string( " is-selected" ) : std::string {} ) + "' role='tab' aria-selected='" + ( s.inventoryCategory.empty() ? std::string( "true" ) : std::string( "false" ) ) + "' data-category=''>" + esc( tr( "management.inventory.tab_all" ) ) + "</button>";
	std::vector<std::pair<std::string, std::string>> categories;
	for ( const auto& r : s.inventory )
	{
		const auto& id = r.id.category.value;
		if ( id.empty() || std::ranges::any_of( categories, [&]( const auto& c ) { return c.first == id; } ) )
			continue;
		categories.emplace_back( id, r.id.depth == InventoryDepth::Category ? r.name : id );
	}
	for ( const auto& [id, name] : categories )
		categoryTabs += "<button id='" + stableId( "inventory_category_", id ) + "' class='c-tabs__tab l-m6b-tab" + ( s.inventoryCategory == id ? std::string( " is-selected" ) : std::string {} ) + "' role='tab' aria-selected='" + ( s.inventoryCategory == id ? std::string( "true" ) : std::string( "false" ) ) + "' data-category='" + esc( id ) + "'>" + esc( name ) + "</button>";
	rml( inventory_, "inventory_category_tabs", categoryTabs );
	if ( auto* e = inventory_->GetElementById( "inventory_sort_name" ) )
	{
		e->SetClass( "is-selected", s.inventorySort == Sort::Name );
		e->SetAttribute( "aria-pressed", s.inventorySort == Sort::Name ? "true" : "false" );
	}
	if ( auto* e = inventory_->GetElementById( "inventory_sort_total" ) )
	{
		e->SetClass( "is-selected", s.inventorySort == Sort::Total );
		e->SetAttribute( "aria-pressed", s.inventorySort == Sort::Total ? "true" : "false" );
	}
	if ( auto* e = inventory_->GetElementById( "inventory_sort_stock" ) )
	{
		e->SetClass( "is-selected", s.inventorySort == Sort::Stock );
		e->SetAttribute( "aria-pressed", s.inventorySort == Sort::Stock ? "true" : "false" );
	}
	if ( auto* e = inventory_->GetElementById( "inventory_filter_owned" ) )
	{
		e->SetClass( "is-checked", s.inventoryOwnedOnly );
		e->SetClass( "is-unchecked", !s.inventoryOwnedOnly );
		e->SetAttribute( "aria-pressed", s.inventoryOwnedOnly ? "true" : "false" );
	}
	const auto populationTab = [this]( const char* id, bool selected )
	{
		if ( auto* e = population_->GetElementById( id ) )
		{
			e->SetClass( "is-selected", selected );
			e->SetAttribute( "aria-selected", selected ? "true" : "false" );
		}
	};
	populationTab( "population_tab_citizens", s.populationView == View::Citizens || s.populationView == View::Creature );
	populationTab( "population_tab_professions", s.populationView == View::Professions );
	populationTab( "population_tab_schedules", s.populationView == View::Schedules );
	std::string rows;
	for ( const auto& r : controller_->populationPage() )
	{
		rows += "<button id='population_creature_" + std::to_string( r.id.value ) + "' class='c-list__row l-m6b-row";
		if ( s.selectedCreature && *s.selectedCreature == r.id )
			rows += " is-selected";
		rows += "' role='option' aria-selected='" + std::string( s.selectedCreature && *s.selectedCreature == r.id ? "true" : "false" ) + "' data-creature='" + std::to_string( r.id.value ) + "'><span class='c-list__primary'>" + esc( r.name ) + "</span><span class='c-list__meta'>" + esc( r.profession.value.empty() ? tr( "management.population.no_profession" ) : r.profession.value ) + "</span></button>";
	}
	rml( population_, "population_rows", rows.empty() ? esc( tr( "management.population.no_citizens" ) ) : rows );
	std::string professions;
	for ( const auto& r : s.professions )
		professions += "<div id='" + stableId( "population_profession_", r.id.value ) + "' class='c-list__row' data-profession='" + esc( r.id.value ) + "'><span class='c-list__primary'>" + esc( r.name ) + "</span><span class='c-list__meta'>" + esc( tr( "management.population.profession_skill_count", { { "count", std::to_string( r.skills.size() ) } } ) ) + "</span></div>";
	rml( population_, "profession_rows", professions.empty() ? esc( tr( "management.population.no_professions" ) ) : professions );
	std::string schedules;
	for ( const auto& r : s.schedules )
	{
		schedules += "<div id='population_schedule_" + std::to_string( r.creature.value ) + "' class='l-m6b-schedule-row' role='row'><span class='l-m6b-schedule-name'>" + esc( r.name ) + "</span>";
		for ( std::size_t h = 0; h < r.hours.size(); ++h )
		{
			const bool selected = s.selectedScheduleCell && s.selectedScheduleCell->creature == r.creature && s.selectedScheduleCell->hour == h;
			const bool active   = r.hours[h] != ManagedScheduleActivity::None;
			const char* key     = r.hours[h] == ManagedScheduleActivity::Eat ? "management.population.schedule_eat" : r.hours[h] == ManagedScheduleActivity::Sleep  ? "management.population.schedule_sleep"
																												  : r.hours[h] == ManagedScheduleActivity::Training ? "management.population.schedule_train"
																																									: "management.population.schedule_none";
			schedules += "<button id='schedule_" + std::to_string( r.creature.value ) + "_" + std::to_string( h ) + "' class='c-check-button l-m6b-schedule-cell" + ( selected ? " is-selected" : "" ) + ( active ? " is-checked" : "" ) + "' role='gridcell' aria-selected='" + ( selected ? std::string( "true" ) : std::string( "false" ) ) + "' data-creature='" + std::to_string( r.creature.value ) + "' data-hour='" + std::to_string( h ) + "'>" + std::to_string( h ) + " " + esc( activityText( tr( key ), active ) ) + "</button>";
		}
		schedules += "</div>";
	}
	rml( population_, "schedule_rows", schedules.empty() ? esc( tr( "management.population.no_schedules" ) ) : schedules );
	if ( s.creature )
	{
		const auto& c = *s.creature;
		text( population_, "creature_name", c.name );
		text( population_, "creature_profession", tr( "management.population.creature_profession", { { "profession", c.profession.empty() ? tr( "management.population.no_profession" ) : c.profession } } ) );
		text( population_, "creature_activity", c.activity.empty() ? tr( "management.population.activity_unknown" ) : tr( "management.population.activity", { { "activity", c.activity } } ) );
		text( population_, "creature_attributes", tr( "management.population.attribute_summary", { { "strength", std::to_string( c.strength ) }, { "dexterity", std::to_string( c.dexterity ) }, { "constitution", std::to_string( c.constitution ) }, { "intelligence", std::to_string( c.intelligence ) }, { "wisdom", std::to_string( c.wisdom ) }, { "charisma", std::to_string( c.charisma ) } } ) );
		text( population_, "creature_needs", tr( "management.population.needs_summary", { { "hunger", std::to_string( c.hunger ) }, { "thirst", std::to_string( c.thirst ) }, { "sleep", std::to_string( c.sleep ) }, { "happiness", std::to_string( c.happiness ) } } ) );
		std::string equipment;
		for ( const auto& e : c.equipment )
			equipment += "<div class='c-list__row'><span class='c-list__primary'>" + esc( e.slot ) + "</span><span class='c-list__meta'>" + esc( e.material + " " + e.item ) + "</span></div>";
		rml( population_, "creature_equipment", equipment.empty() ? esc( tr( "management.population.no_equipment" ) ) : equipment );
		const auto citizen = std::ranges::find_if( s.population, [&]( const auto& r )
												   { return r.id == c.id; } );
		std::string skills;
		if ( citizen != s.population.end() )
			for ( const auto& skill : citizen->skills )
			{
				const std::string skillId = stableId( "population_skill_", skill.id.value );
				const std::string active  = skill.active ? "true" : "false";
				skills += "<div id='" + skillId + "' class='m6b-skill-row" + ( skill.active ? " is-active" : "" ) + "' data-skill='" + esc( skill.id.value ) + "' data-active='" + active + "' role='checkbox' tab-index='0' aria-checked='" + active + "'><input type='checkbox' class='checkbox m6b-skill-checkbox' data-skill='" + esc( skill.id.value ) + "' data-active='" + active + "'" + ( skill.active ? " checked='checked'" : "" ) + "/><span class='m6b-skill-name'>" + esc( skill.name ) + "</span><span class='m6b-skill-level'>Level " + std::to_string( skill.level ) + "</span><span class='m6b-skill-state'>" + esc( tr( skill.active ? "common.on" : "common.off" ) ) + "</span></div>";
			}
		rml( population_, "creature_skills", skills.empty() ? esc( tr( "management.population.no_skills" ) ) : skills );
		std::string choices;
		for ( const auto& p : s.professions )
			choices += "<button id='" + stableId( "population_profession_choice_", p.id.value ) + "' class='c-dropdown-button' data-profession='" + esc( p.id.value ) + "'>" + esc( p.name ) + " v</button>";
		rml( population_, "creature_professions", choices.empty() ? esc( tr( "management.population.no_profession_choices" ) ) : choices );
	}
	std::string inventory;
	for ( const auto& r : controller_->inventoryPage() )
	{
		const bool selected = s.selectedInventory && *s.selectedInventory == r.id;
		const bool section = r.id.depth == InventoryDepth::Category || r.id.depth == InventoryDepth::Group || r.id.depth == InventoryDepth::Item;
		const std::string disclosure = section ? ( controller_->inventoryExpanded( r.id ) ? "[-]" : "[+]" ) : std::string {};
		inventory += "<button id='" + stableId( "inventory_row_", inventoryKey( r.id ) ) + "' class='c-list__row l-m6b-row l-m6b-inventory-row" + ( selected ? " is-selected" : "" ) + "' role='treeitem' aria-expanded='" + ( section ? ( controller_->inventoryExpanded( r.id ) ? "true" : "false" ) : "false" ) + "' aria-selected='" + ( selected ? std::string( "true" ) : std::string( "false" ) ) + "' data-depth='" + depth( r.id.depth ) + "' data-category='" + esc( r.id.category.value ) + "' data-group='" + esc( r.id.group.value ) + "' data-item='" + esc( r.id.item.value ) + "' data-material='" + esc( r.id.material.value ) + "'><span class='l-m6b-inventory__name l-m6b-depth-" + depth( r.id.depth ) + "'>" + inventorySprite( r ) + "<span class='l-m6b-inventory__content'><span class='l-m6b-disclosure' aria-hidden='true'>" + disclosure + "</span>" + esc( r.name ) + "</span></span><span class='l-m6b-inventory__metric'>" + std::to_string( r.stockpiled ) + "</span><span class='l-m6b-inventory__metric'>" + std::to_string( r.total ) + "</span></button>";
	}
	rml( inventory_, "inventory_rows", inventory.empty() ? esc( tr( "management.inventory.no_rows" ) ) : inventory );
	const auto populationTotal = controller_->visiblePopulation().size();
	const auto pageText = [&]( const char* key, std::size_t page, std::size_t total )
	{const auto first=total?std::min(page*Management6BState::pageSize+1,total):0;const auto last=std::min((page+1)*Management6BState::pageSize,total);return tr(key,{{"first",std::to_string(first)}, {"last",std::to_string(last)}, {"total",std::to_string(total)}}); };
	text( population_, "population_page_status", pageText( "management.population.page", s.populationPage, populationTotal ) );
	text( population_, "population_revision", tr( "management.population.revision" ) );
	const auto status = s.pendingAction ? tr( "status.updating" ) : s.status;
	text( population_, "population_status", status );
	text( inventory_, "inventory_status", status );
}
} // namespace ingnomia::ui::management6b
