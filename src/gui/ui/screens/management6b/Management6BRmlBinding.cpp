/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6BRmlBinding.h"

#include "../ManagementTooltip.h"
#include "../InventoryTableSchema.h"
#include "../../localization/RmlText.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

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
std::string foldedLabel( std::string value )
{
	std::transform( value.begin(), value.end(), value.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
	return value;
}
std::string readableCatalogId( std::string_view value )
{
	std::string out;
	for ( const unsigned char c : value )
	{
		if ( c == '_' ) { out.push_back( ' ' ); continue; }
		if ( !out.empty() && std::isupper( c ) && std::islower( static_cast<unsigned char>( out.back() ) ) ) out.push_back( ' ' );
		out.push_back( static_cast<char>( c ) );
	}
	return out;
}
std::string filterOptionMarkup( std::vector<std::string> values, const std::vector<std::string>& selected, std::string_view idPrefix )
{
	std::ranges::stable_sort( values, []( const auto& left, const auto& right ) { return foldedLabel( left ) < foldedLabel( right ); } );
	values.erase( std::unique( values.begin(), values.end(), []( const auto& left, const auto& right ) { return foldedLabel( left ) == foldedLabel( right ); } ), values.end() );
	std::string out = "<button id='" + std::string( idPrefix ) + "_option_all' type='button' class='c-excel-filter-combo__option" + std::string( selected.empty() ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( selected.empty() ? std::string( "true" ) : std::string( "false" ) ) + "' data-filter-value=''><span class='c-excel-filter-combo__check'>" + ( selected.empty() ? std::string( "[x]" ) : std::string( "[ ]" ) ) + "</span> All</button>";
	std::size_t optionIndex = 0;
	for ( const auto& value : values )
	{
		if ( value.empty() ) continue;
		const bool active = std::ranges::any_of( selected, [&]( const auto& candidate ) { return foldedLabel( candidate ) == foldedLabel( value ); } );
		out += "<button id='" + std::string( idPrefix ) + "_option_" + std::to_string( ++optionIndex ) + "' type='button' class='c-excel-filter-combo__option" + std::string( active ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( active ? std::string( "true" ) : std::string( "false" ) ) + "' data-filter-value='" + esc( value ) + "'><span class='c-excel-filter-combo__check'>" + ( active ? std::string( "[x]" ) : std::string( "[ ]" ) ) + "</span> " + esc( value ) + "</button>";
	}
	return out;
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
std::string inventoryParentKey( InventoryRowId id )
{
	if ( id.depth == InventoryDepth::Material )
	{
		id.depth = InventoryDepth::Item;
		id.material = {};
	}
	else if ( id.depth == InventoryDepth::Item )
	{
		id.depth = InventoryDepth::Group;
		id.item = {};
		id.material = {};
	}
	else if ( id.depth == InventoryDepth::Group )
	{
		id.depth = InventoryDepth::Category;
		id.group = {};
		id.item = {};
		id.material = {};
	}
	return inventoryKey( id );
}
struct InventoryPathLabels
{
	std::string category, group, item, material;
};
class InventoryLabelLookup
{
	public:
	explicit InventoryLabelLookup( const std::vector<InventoryRow>& rows )
	{
		for ( const auto& row : rows ) names_[static_cast<std::size_t>( row.id.depth )][key( row.id, row.id.depth )] = row.name;
	}
	[[nodiscard]] InventoryPathLabels labels( const InventoryRow& leaf ) const
	{
		InventoryPathLabels out;
		std::array<std::string*, 4> destinations { &out.category, &out.group, &out.item, &out.material };
		for ( std::size_t depth = 0; depth < destinations.size(); ++depth )
		{
			const auto found = names_[depth].find( key( leaf.id, static_cast<InventoryDepth>( depth ) ) );
			if ( found != names_[depth].end() ) *destinations[depth] = found->second;
		}
		normalizeInventoryTableLabels( out.group, out.item, out.material, leaf.id.item.value );
		return out;
	}
	private:
	static std::string key( const InventoryRowId& id, InventoryDepth depth )
	{
		std::string out = id.category.value;
		if ( depth >= InventoryDepth::Group ) out += '\x1f' + id.group.value;
		if ( depth >= InventoryDepth::Item ) out += '\x1f' + id.item.value;
		if ( depth >= InventoryDepth::Material ) out += '\x1f' + id.material.value;
		return out;
	}
	std::array<std::unordered_map<std::string, std::string>, 4> names_;
};
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
	bind( population_, "population_tab_skills", [this]
		  { controller_->open( View::Skills ); } );
	bind( population_, "population_tab_professions", [this]
		  { controller_->open( View::Professions ); } );
	bind( population_, "population_tab_schedules", [this]
		  { controller_->open( View::Schedules ); } );
	bind( population_, "population_views_toggle", [this] {
		if( auto* shell = population_->GetElementById( "population_shell" ) ) {
			const bool open = !shell->IsClassSet( "is-rail-open" );
			shell->SetClass( "is-rail-open", open );
			if( auto* button = population_->GetElementById( "population_views_toggle" ) ) button->SetAttribute( "aria-expanded", open ? "true" : "false" );
		}
	} );
	bind( population_, "population_refresh", [this]
		  { controller_->refresh(); } );
	bind( population_, "population_refresh_view", [this] { controller_->refresh(); } );
	bind( population_, "population_sort_name", [this]
		  { controller_->setPopulationSort( Sort::Name ); } );
	bind( population_, "population_sort_profession", [this]
		  { controller_->setPopulationSort( Sort::Profession ); } );
	bind( population_, "creature_back", [this]
		  { controller_->open( View::Citizens ); focusPopulationRow(); } );
	bind( population_, "population_page_previous", [this]
		  { controller_->changePopulationPage( -1 ); } );
	bind( population_, "population_page_next", [this]
		  { controller_->changePopulationPage( 1 ); } );
	bind( population_, "schedule_set_none", [this]{ controller_->setScheduleActivity( ManagedScheduleActivity::None ); } );
	bind( population_, "schedule_set_eat", [this]{ controller_->setScheduleActivity( ManagedScheduleActivity::Eat ); } );
	bind( population_, "schedule_set_sleep", [this]{ controller_->setScheduleActivity( ManagedScheduleActivity::Sleep ); } );
	bind( population_, "schedule_set_training", [this]{ controller_->setScheduleActivity( ManagedScheduleActivity::Training ); } );
	const auto selectedActivity = [this] {
		switch( controller_->state().scheduleActivity ) {
		case ManagedScheduleActivity::Eat: return ScheduleActivity::Eat;
		case ManagedScheduleActivity::Sleep: return ScheduleActivity::Sleep;
		case ManagedScheduleActivity::Training: return ScheduleActivity::Training;
		default: return ScheduleActivity::None;
		}
	};
	bind( population_, "schedule_apply_cell", [this, selectedActivity]{ controller_->activateScheduleCell( selectedActivity() ); } );
	bind( population_, "schedule_apply_row", [this, selectedActivity]{ if( controller_->state().selectedScheduleCell ) controller_->setScheduleRow( controller_->state().selectedScheduleCell->creature, selectedActivity() ); } );
	bind( population_, "schedule_apply_column", [this, selectedActivity]{ if( controller_->state().selectedScheduleCell ) controller_->setScheduleColumn( controller_->state().selectedScheduleCell->hour, selectedActivity() ); } );
	bind( population_, "skill_enable_all", [this]{ if( controller_->state().selectedSkill ) controller_->setSkillForAll( *controller_->state().selectedSkill, true ); } );
	bind( population_, "skill_disable_all", [this]{ if( controller_->state().selectedSkill ) controller_->setSkillForAll( *controller_->state().selectedSkill, false ); } );
	bind( population_, "creature_skills_enable_all", [this]{ if( controller_->state().selectedCreature ) controller_->setAllSkills( *controller_->state().selectedCreature, true ); } );
	bind( population_, "creature_skills_disable_all", [this]{ if( controller_->state().selectedCreature ) controller_->setAllSkills( *controller_->state().selectedCreature, false ); } );
	bindEvent( population_, "skills_catalog_rows", "click", [this]( Rml::Event& e ) { for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){ auto id=x->GetAttribute<Rml::String>("data-skill",""); if(!id.empty()){controller_->selectSkill(CatalogId{id});break;} } } );
	bindEvent( population_, "skill_citizen_rows", "click", [this]( Rml::Event& e ) { for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){ auto id=x->GetAttribute<Rml::String>("data-creature",""); if(id.empty()||!controller_->state().selectedSkill)continue; try{controller_->setSkill(CreatureId{static_cast<std::uint32_t>(std::stoul(id))},*controller_->state().selectedSkill,x->GetAttribute<Rml::String>("data-active","")!="true");}catch(...){} break;} } );
	bindEvent( population_, "profession_rows", "click", [this]( Rml::Event& e ) { for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){ auto id=x->GetAttribute<Rml::String>("data-profession",""); if(!id.empty()){controller_->selectProfession(ProfessionId{id});break;} } } );
	bindEvent( population_, "profession_selected_skills", "click", [this]( Rml::Event& e ) { for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){ auto id=x->GetAttribute<Rml::String>("data-skill",""); if(!id.empty()){controller_->selectProfessionSkill(CatalogId{id});break;} } } );
	bindEvent( population_, "profession_available_skills", "click", [this]( Rml::Event& e ) { for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){ auto id=x->GetAttribute<Rml::String>("data-skill",""); if(!id.empty()){controller_->selectAvailableSkill(CatalogId{id});break;} } } );
	bind( population_, "profession_skill_add", [this]{ controller_->addProfessionSkill(); } );
	bind( population_, "profession_skill_remove", [this]{ controller_->removeProfessionSkill(); } );
	bind( population_, "profession_skill_up", [this]{ controller_->moveProfessionSkill(-1); } );
	bind( population_, "profession_skill_down", [this]{ controller_->moveProfessionSkill(1); } );
	bind( population_, "profession_create", [this]{ if(auto* e=rmlui_dynamic_cast<Rml::ElementFormControl*>(population_->GetElementById("profession_create_name"))){ const auto name=e->GetValue(); controller_->createProfession(name); if(controller_->state().selectedProfession && controller_->state().selectedProfession->value==name)e->SetValue(""); } } );
	bindEvent( population_, "profession_name", "input", [this]( Rml::Event& event ) { if ( auto* name = rmlui_dynamic_cast<Rml::ElementFormControl*>( event.GetCurrentElement() ) ) controller_->setProfessionDraftName( name->GetValue() ); } );
	bind( population_, "profession_save", [this]{ if(auto* e=rmlui_dynamic_cast<Rml::ElementFormControl*>(population_->GetElementById("profession_name")))controller_->setProfessionDraftName(e->GetValue()); controller_->saveProfession(); } );
	bind( population_, "profession_delete", [this]{ if(controller_->state().selectedProfession && controller_->state().selectedProfession->value!="Gnomad"){ text(population_,"population_delete_name",controller_->state().selectedProfession->value); visible(population_,"population_delete_confirm",true); if(auto* e=population_->GetElementById("population_delete_cancel"))e->Focus(); } } );
	bind( population_, "population_delete_cancel", [this]{ visible(population_,"population_delete_confirm",false); if(auto* e=population_->GetElementById("profession_delete"))e->Focus(); } );
	bind( population_, "population_delete_accept", [this]{ controller_->deleteProfession(); visible(population_,"population_delete_confirm",false); if(auto* e=population_->GetElementById("profession_create_name"))e->Focus(); } );
	bindEvent( population_, "population_delete_confirm", "keydown", [this]( Rml::Event& event ) { if ( static_cast<Rml::Input::KeyIdentifier>( event.GetParameter<int>( "key_identifier", 0 ) ) == Rml::Input::KI_ESCAPE ) { visible( population_, "population_delete_confirm", false ); if ( auto* button = population_->GetElementById( "profession_delete" ) ) button->Focus(); event.StopPropagation(); } } );
	bindEvent( population_, "creature_skills", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto skill=x->GetAttribute<Rml::String>("data-skill","");if(skill.empty()||!controller_->state().selectedCreature)continue;controller_->setSkill(*controller_->state().selectedCreature,CatalogId{skill},x->GetAttribute<Rml::String>("data-active","")!="true");break;} } );
	bindEvent( population_, "creature_professions", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto profession=x->GetAttribute<Rml::String>("data-profession","");if(profession.empty()||!controller_->state().selectedCreature)continue;controller_->setProfession(*controller_->state().selectedCreature,ProfessionId{profession});break;} } );
	bindEvent( population_, "population_rows", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto raw=x->GetAttribute<Rml::String>("data-creature","");if(raw.empty())continue;try{controller_->selectCreature(CreatureId{static_cast<std::uint32_t>(std::stoul(raw))});if(citizenSelectedHandler_)citizenSelectedHandler_();}catch(...){}break;} } );
	bindEvent( population_, "population_rows", "keydown", [this]( Rml::Event& e )
			   {const auto key=static_cast<Rml::Input::KeyIdentifier>(e.GetParameter<int>("key_identifier",0));if(key==Rml::Input::KI_UP){controller_->movePopulationSelection(-1);focusPopulationRow();}else if(key==Rml::Input::KI_DOWN){controller_->movePopulationSelection(1);focusPopulationRow();}else if(key==Rml::Input::KI_RETURN&&controller_->state().selectedCreature){controller_->selectCreature(*controller_->state().selectedCreature);if(citizenSelectedHandler_)citizenSelectedHandler_();}else return;e.StopPropagation(); } );
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
	bind( inventory_, "inventory_sort_category", [this]
		  { controller_->setInventorySort( Sort::Category ); } );
	bind( inventory_, "inventory_sort_group", [this]
		  { controller_->setInventorySort( Sort::Group ); } );
	bind( inventory_, "inventory_sort_item", [this]
		  { controller_->setInventorySort( Sort::Item ); } );
	bind( inventory_, "inventory_sort_material", [this]
		  { controller_->setInventorySort( Sort::Material ); } );
	bind( inventory_, "inventory_sort_total", [this]
		  { controller_->setInventorySort( Sort::Total ); } );
	bind( inventory_, "inventory_sort_stock", [this]
		  { controller_->setInventorySort( Sort::Stock ); } );
	const std::array inventoryFilters {
		"inventory_filter_category", "inventory_filter_group", "inventory_filter_item",
		"inventory_filter_material", "inventory_filter_stock", "inventory_filter_total"
	};
	const std::array inventoryFilterToggles {
		"inventory_filter_category_toggle", "inventory_filter_group_toggle", "inventory_filter_item_toggle",
		"inventory_filter_material_toggle", "inventory_filter_stock_toggle", "inventory_filter_total_toggle"
	};
	const std::array inventoryFilterOptions {
		"inventory_filter_category_options", "inventory_filter_group_options", "inventory_filter_item_options",
		"inventory_filter_material_options", "inventory_filter_stock_options", "inventory_filter_total_options"
	};
	for ( std::size_t column = 0; column < inventoryFilters.size(); ++column )
	{
		const auto* id = inventoryFilters[column];
		auto changed = [this, id, column]( Rml::Event& event, bool restoreFocus )
		{
			if ( column >= 4 ) return;
			auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>( event.GetCurrentElement() );
			handlingInventoryFilterInput_ = restoreFocus;
			controller_->setInventoryColumnFilter( column, control ? control->GetValue() : std::string {} );
			handlingInventoryFilterInput_ = false;
			if ( restoreFocus )
				if ( auto* input = inventory_->GetElementById( id ); input && context_.GetFocusElement() != input ) input->Focus();
		};
		bindEvent( inventory_, id, "input", [changed]( Rml::Event& event ) { changed( event, true ); } );
		bindEvent( inventory_, id, "change", [changed]( Rml::Event& event ) { changed( event, false ); } );
		bind( inventory_, inventoryFilterToggles[column], [this, column]
		{
			suppressNextInventoryFilterClick_ = false;
			inventoryFilterMenuColumn_ = inventoryFilterMenuColumn_ && *inventoryFilterMenuColumn_ == column ? std::nullopt : std::optional<std::size_t> { column };
			stateChanged( controller_->state() );
		} );
		auto toggleOption = [this, column]( Rml::Event& event )
		{
			for ( auto* option = event.GetTargetElement(); option && option != event.GetCurrentElement(); option = option->GetParentNode() )
			{
				if ( !option->HasAttribute( "data-filter-value" ) ) continue;
				const auto value = option->GetAttribute<Rml::String>( "data-filter-value", "" );
				handlingInventoryFilterOption_ = true;
				controller_->toggleInventoryColumnSelection( column, value );
				handlingInventoryFilterOption_ = false;
				const auto& selected = controller_->state().inventoryColumnSelections[column];
				for ( auto* candidate = event.GetCurrentElement()->GetFirstChild(); candidate; candidate = candidate->GetNextSibling() )
				{
					if ( !candidate->HasAttribute( "data-filter-value" ) ) continue;
					const auto candidateValue = candidate->GetAttribute<Rml::String>( "data-filter-value", "" );
					const bool active = candidateValue.empty() ? selected.empty() : std::ranges::any_of( selected, [&]( const auto& item ) { return foldedLabel( item ) == foldedLabel( candidateValue ); } );
					candidate->SetClass( "is-selected", active );
					candidate->SetAttribute( "aria-selected", active ? "true" : "false" );
					if ( auto* check = candidate->GetFirstChild() ) check->SetInnerRML( active ? "[x]" : "[ ]" );
				}
				event.StopPropagation();
				break;
			}
		};
		// Toggle on press so a list re-render cannot invalidate RmlUi's later
		// mouse-up/click target. This is also noticeably more responsive on long lists.
		bindEvent( inventory_, inventoryFilterOptions[column], "mousedown", [this, toggleOption]( Rml::Event& event )
		{
			suppressNextInventoryFilterClick_ = true;
			toggleOption( event );
		} );
		bindEvent( inventory_, inventoryFilterOptions[column], "click", [this, toggleOption]( Rml::Event& event )
		{
			if ( suppressNextInventoryFilterClick_ )
			{
				suppressNextInventoryFilterClick_ = false;
				event.StopPropagation();
				return;
			}
			toggleOption( event );
		} );
		bindEvent( inventory_, inventoryFilterOptions[column], "keydown", [this, toggleOption]( Rml::Event& event )
		{
			const auto key = static_cast<Rml::Input::KeyIdentifier>( event.GetParameter<int>( "key_identifier", 0 ) );
			if ( key == Rml::Input::KI_RETURN || key == Rml::Input::KI_SPACE )
			{
				suppressNextInventoryFilterClick_ = true;
				toggleOption( event );
			}
		} );
	}
	bindEvent( inventory_, "inventory_rows", "click", [this]( Rml::Event& e )
			   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto d=x->GetAttribute<Rml::String>("data-depth","");if(d.empty())continue;const InventoryRowId id{CatalogId{x->GetAttribute<Rml::String>("data-category","")},CatalogId{x->GetAttribute<Rml::String>("data-group","")},CatalogId{x->GetAttribute<Rml::String>("data-item","")},CatalogId{x->GetAttribute<Rml::String>("data-material","")},d=="category"?InventoryDepth::Category:d=="group"?InventoryDepth::Group:d=="material"?InventoryDepth::Material:InventoryDepth::Item};controller_->selectInventory(id);controller_->openInventoryDetail(id);if(auto* back=inventory_->GetElementById("inventory_detail_back"))back->Focus();break;} } );
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
			if ( key == Rml::Input::KI_SPACE ) controller_->toggleSelectedWatch();
			else if ( controller_->state().selectedInventory ) controller_->openInventoryDetail( *controller_->state().selectedInventory );
		}
		else
			return;
		e.StopPropagation();
	} );
	bindEvent( inventory_, "inventory_rows", "scroll", [this]( Rml::Event& ) { renderInventoryViewport(); } );
	bind( inventory_, "inventory_detail_back", [this] { controller_->backInventoryDetail(); if ( !controller_->state().inventoryDetail ) focusInventoryRow(); } );
	for ( const char* container : { "inventory_detail_made_by", "inventory_detail_used_in", "inventory_detail_locations" } )
		bindEvent( inventory_, container, "click", [this]( Rml::Event& event )
		{
			for ( auto* node = event.GetTargetElement(); node && node != event.GetCurrentElement(); node = node->GetParentNode() )
			{
				const auto item = node->GetAttribute<Rml::String>( "data-item-link", "" );
				if ( !item.empty() ) { controller_->openRelatedInventoryItem( item ); event.StopPropagation(); return; }
				const auto stockpile = node->GetAttribute<Rml::String>( "data-stockpile-link", "" );
				if ( !stockpile.empty() && stockpileOpenHandler_ )
				{
					stockpileOpenHandler_( static_cast<unsigned int>( std::stoul( stockpile ) ) );
					event.StopPropagation(); return;
				}
			}
		} );
	for ( const char* id : { "population_views_toggle", "population_tab_citizens", "population_tab_skills",
		"population_tab_professions", "population_tab_schedules", "skill_enable_all", "skill_disable_all",
		"creature_skills_enable_all", "creature_skills_disable_all", "schedule_apply_cell",
		"schedule_apply_row", "schedule_apply_column" } )
	{
		if ( auto* target = population_->GetElementById( id ) ) target->SetAttribute( "aria-describedby", "population_tooltip" );
		for ( const char* event : { "mouseover", "focus" } )
			bindEvent( population_, id, event, [this]( Rml::Event& e ) {
				showManagementTooltip( population_, context_, "population_tooltip", e.GetCurrentElement() );
			} );
		for ( const char* event : { "mouseout", "blur" } )
			bindEvent( population_, id, event, [this]( Rml::Event& ) { hideManagementTooltip( population_, "population_tooltip" ); } );
	}
	population_->Show();
	inventory_->Show();
	stateChanged( c.state() );
	return true;
}
bool Management6BRmlBinding::reloadDocuments()
{
	auto* controller = controller_;
	if ( !controller ) return false;
	shutdown();
	return initialize( *controller );
}
void Management6BRmlBinding::shutdown()
{
	for ( auto& l : listeners_ )
		if ( l.target )
			l.target->RemoveEventListener( l.event, l.callback.get() );
	listeners_.clear();
	inventoryFilterMenuColumn_.reset();
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
	renderedInventoryRows_.reset();
	renderedInventoryDetailMarkup_.clear();
	renderedInventoryBackLabel_.clear();
	inventoryRowMarkup_.clear();
	inventoryViewportFirst_ = static_cast<std::size_t>( -1 );
	renderingInventoryViewport_ = false;
	inventoryFilterMenuColumn_.reset();
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
		auto cb = std::make_unique<Callback>([this, d, fn=std::move(fn)](Rml::Event& event) {
            controller_->activateViewForInput(d == inventory_ ? View::Inventory : controller_->state().populationView);
            fn(event);
        });
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
	const auto id = stableId( "inventory_row_", inventoryKey( *controller_->state().selectedInventory ) );
	auto* target = inventory_->GetElementById( id );
	if ( !target )
	{
		const auto rows = controller_->inventoryPage();
		const auto selected = std::ranges::find_if( rows, [&]( const auto& row ) { return row.id == *controller_->state().selectedInventory; } );
		if ( selected != rows.end() )
			if ( auto* list = inventory_->GetElementById( "inventory_rows" ) )
			{
				list->SetScrollTop( static_cast<float>( std::distance( rows.begin(), selected ) ) * 48.f );
				inventoryViewportFirst_ = static_cast<std::size_t>( -1 );
				renderInventoryViewport();
				target = inventory_->GetElementById( id );
			}
	}
	if ( target ) target->Focus();
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
		{
			const float scrollTop = e->GetScrollTop();
			std::string focusId;
			bool focusWithin = false;
			for ( auto* focused = context_.GetFocusElement(); focused; focused = focused->GetParentNode() )
			{
				if ( focused == e ) { focusWithin = true; break; }
				if ( focusId.empty() ) focusId = focused->GetId();
			}
			e->SetInnerRML( v );
			e->SetScrollTop( scrollTop );
			if ( focusWithin && !focusId.empty() ) if ( auto* replacement = d->GetElementById( focusId ) ) replacement->Focus();
		}
}
void Management6BRmlBinding::visible( Rml::ElementDocument* d, const char* id, bool v )
{
	if ( d )
		if ( auto* e = d->GetElementById( id ) )
		{
			if ( e->IsClassSet( "is-hidden" ) == !v ) return;
			e->SetClass( "is-hidden", !v );
				if( v ) e->RemoveProperty( "display" );
				else e->SetProperty( "display", "none" );
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
	const bool pop = s.populationOpen && (!secondarySurface_ || !*secondarySurface_);
	const bool inv = s.inventoryOpen && (!secondarySurface_ || *secondarySurface_);
	if ( !pop ) hideManagementTooltip( population_, "population_tooltip" );
	if ( pop != population_->IsVisible() )
		pop ? population_->Show() : population_->Hide();
	if ( inv != inventory_->IsVisible() )
		inv ? inventory_->Show() : inventory_->Hide();
	visible( population_, "population_root", pop );
	visible( inventory_, "inventory_root", inv );
	visible( population_, "population_citizens", pop && ( s.populationView == View::Citizens ) );
	visible( population_, "population_skills", pop && ( s.populationView == View::Skills ) );
	visible( population_, "population_professions", pop && ( s.populationView == View::Professions ) );
	visible( population_, "population_schedules", pop && ( s.populationView == View::Schedules ) );
	visible( population_, "creature_detail", pop && ( s.populationView == View::Creature ) );
	visible( population_, "population_loading", s.loadingPopulation );
	visible( population_, "population_toolbar", pop && s.populationView == View::Citizens );
	visible( population_, "population_refresh_view", pop && s.populationView != View::Citizens );
	visible( inventory_, "inventory_loading", s.loadingInventory );
	if ( inv )
	{
	const bool filteredInventory = std::ranges::any_of( s.inventoryColumnFilters, []( const auto& value ) { return !value.empty(); } )
		|| std::ranges::any_of( s.inventoryColumnSelections, []( const auto& values ) { return !values.empty(); } );
	if ( auto* e = inventory_->GetElementById( "inventory_workbench" ) )
	{
		e->SetClass( "is-filtered", filteredInventory );
	}
	const std::array inventoryFilters {
		"inventory_filter_category", "inventory_filter_group", "inventory_filter_item",
		"inventory_filter_material", "inventory_filter_stock", "inventory_filter_total"
	};
	const std::array inventoryFilterToggles {
		"inventory_filter_category_toggle", "inventory_filter_group_toggle", "inventory_filter_item_toggle",
		"inventory_filter_material_toggle", "inventory_filter_stock_toggle", "inventory_filter_total_toggle"
	};
	const std::array inventoryFilterOptions {
		"inventory_filter_category_options", "inventory_filter_group_options", "inventory_filter_item_options",
		"inventory_filter_material_options", "inventory_filter_stock_options", "inventory_filter_total_options"
	};
	const InventoryLabelLookup inventoryLabels( s.inventory );
	for ( std::size_t column = 0; column < inventoryFilters.size(); ++column )
		if ( auto* input = rmlui_dynamic_cast<Rml::ElementFormControl*>( inventory_->GetElementById( inventoryFilters[column] ) ) )
			{
				const auto value = column >= 4 && !s.inventoryColumnSelections[column].empty() ? ( s.inventoryColumnSelections[column].front() == "Has (>0)" ? std::string( ">0" ) : std::string( "0" ) ) : s.inventoryColumnFilters[column];
				if ( input->GetValue() != value ) input->SetValue( value );
			}
	for ( std::size_t column = 0; column < inventoryFilters.size(); ++column )
	{
		const bool open = inventoryFilterMenuColumn_ && *inventoryFilterMenuColumn_ == column;
			if ( open && !handlingInventoryFilterOption_ )
		{
			std::unordered_set<std::string> parents;
			for ( const auto& row : s.inventory )
				if ( row.id.depth != InventoryDepth::Category ) parents.insert( inventoryParentKey( row.id ) );
			std::vector<std::string> values;
			values.reserve( s.inventory.size() );
			for ( const auto& row : s.inventory )
			{
				if ( parents.contains( inventoryKey( row.id ) ) ) continue;
					const auto labels = inventoryLabels.labels( row );
				if ( column == 0 ) values.push_back( labels.category );
				else if ( column == 1 ) values.push_back( labels.group );
				else if ( column == 2 ) values.push_back( labels.item );
				else if ( column == 3 ) values.push_back( labels.material );
				else if ( column == 4 ) values.push_back( inventoryQuantityLabel( row.stockpiled ) );
				else values.push_back( inventoryQuantityLabel( row.total ) );
			}
			if ( column >= 4 ) values = { "Has (>0)", "None (0)" };
			rml( inventory_, inventoryFilterOptions[column], filterOptionMarkup( std::move( values ), s.inventoryColumnSelections[column], inventoryFilterOptions[column] ) );
		}
		visible( inventory_, inventoryFilterOptions[column], open );
		if ( auto* input = inventory_->GetElementById( inventoryFilters[column] ) ) input->SetAttribute( "aria-expanded", open ? "true" : "false" );
		if ( auto* toggle = inventory_->GetElementById( inventoryFilterToggles[column] ) )
		{
			toggle->SetAttribute( "aria-expanded", open ? "true" : "false" );
			toggle->SetClass( "is-selected", open );
			toggle->SetClass( "has-selection", !s.inventoryColumnSelections[column].empty() );
		}
	}
	if ( auto* e = inventory_->GetElementById( "inventory_column_head" ) )
		e->SetProperty( "display", "flex" );
	const auto tableColumns = inventoryTableColumns( "" );
	text( inventory_, "inventory_column_category", tableColumns.category );
	text( inventory_, "inventory_column_group", tableColumns.group );
	text( inventory_, "inventory_column_item", tableColumns.item );
	text( inventory_, "inventory_column_material", tableColumns.material );
	const auto updateSort = [this, &s]( const char* id, const char* directionId, Sort key, bool selected = false )
	{
		selected = selected || s.inventorySort == key;
		if ( auto* e = inventory_->GetElementById( id ) )
		{
			e->SetClass( "is-selected", selected );
			e->SetAttribute( "aria-pressed", selected ? "true" : "false" );
		}
		text( inventory_, directionId, selected ? ( s.inventorySortDescending ? "v" : "^" ) : "" );
	};
	updateSort( "inventory_sort_category", "inventory_sort_category_direction", Sort::Category );
	updateSort( "inventory_sort_group", "inventory_sort_group_direction", Sort::Group );
	updateSort( "inventory_sort_item", "inventory_sort_item_direction", Sort::Item, s.inventorySort == Sort::Name );
	updateSort( "inventory_sort_material", "inventory_sort_material_direction", Sort::Material );
	updateSort( "inventory_sort_stock", "inventory_sort_stock_direction", Sort::Stock );
	updateSort( "inventory_sort_total", "inventory_sort_total_direction", Sort::Total );
	}
	if ( pop )
	{
	if ( renderedPopulationView_ != s.populationView )
	{
		hideManagementTooltip( population_, "population_tooltip" );
		bool railWasOpen = false;
		if ( auto* shell = population_->GetElementById( "population_shell" ) ) { railWasOpen = shell->IsClassSet( "is-rail-open" ); shell->SetClass( "is-rail-open", false ); }
		if ( auto* button = population_->GetElementById( "population_views_toggle" ) ) button->SetAttribute( "aria-expanded", "false" );
		if ( railWasOpen ) if ( auto* button = population_->GetElementById( "population_views_toggle" ) ) button->Focus();
		renderedPopulationView_ = s.populationView;
	}
	const char* viewKey = s.populationView == View::Skills ? "management.population.skills" : s.populationView == View::Professions ? "management.population.professions" : s.populationView == View::Schedules ? "management.population.schedules" : "management.population.citizens";
	text( population_, "population_views_toggle", tr( "management.navigation.views" ) + ": " + tr( viewKey ) );
	const auto populationTab = [this]( const char* id, bool selected )
	{
		if ( auto* e = population_->GetElementById( id ) )
		{
			e->SetClass( "is-selected", selected );
			e->SetAttribute( "aria-selected", selected ? "true" : "false" );
		}
	};
	populationTab( "population_tab_citizens", s.populationView == View::Citizens || s.populationView == View::Creature );
	populationTab( "population_tab_skills", s.populationView == View::Skills );
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
		professions += "<button id='" + stableId( "population_profession_", r.id.value ) + "' class='c-list__row" + ( s.selectedProfession == r.id ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( s.selectedProfession == r.id ? "true" : "false" ) + "' data-profession='" + esc( r.id.value ) + "'><span class='c-list__primary'>" + esc( r.name ) + "</span></button>";
	rml( population_, "profession_rows", professions.empty() ? esc( tr( "management.population.no_professions" ) ) : professions );
	std::string catalog;
	for ( const auto& skill : s.skillCatalog )
		catalog += "<button id='" + stableId( "skill_catalog_", skill.id.value ) + "' class='c-list__row" + std::string( s.selectedSkill == skill.id ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( s.selectedSkill == skill.id ? "true" : "false" ) + "' data-skill='" + esc( skill.id.value ) + "'><span class='c-list__primary'>" + esc( skill.name ) + "</span><span class='c-list__meta'>" + esc( skill.group ) + "</span></button>";
	rml( population_, "skills_catalog_rows", catalog.empty() ? esc( tr( "management.population.no_skill_catalog" ) ) : catalog );
	const auto selectedSkill = std::ranges::find_if( s.skillCatalog, [&]( const SkillCatalogRow& row ){ return s.selectedSkill == row.id; } );
	text( population_, "skill_focus_title", selectedSkill == s.skillCatalog.end() ? tr( "management.population.choose_skill" ) : selectedSkill->name );
	text( population_, "skill_focus_group", selectedSkill == s.skillCatalog.end() ? tr( "management.population.choose_skill_help" ) : selectedSkill->group );
	std::string skillCitizens;
	if ( s.selectedSkill ) for ( const auto& citizen : s.population )
	{
		const auto skill = std::ranges::find_if( citizen.skills, [&]( const SkillRow& row ){ return row.id == *s.selectedSkill; } );
		if ( skill == citizen.skills.end() ) continue;
		std::ostringstream xp;
		xp << skill->experience;
		const auto detail = tr( "management.population.skill_level_xp", { { "level", std::to_string( skill->level ) }, { "xp", xp.str() } } );
		skillCitizens += "<button id='skill_citizen_" + std::to_string( citizen.id.value ) + "' class='c-list__row l-population-skill-citizen' data-creature='" + std::to_string( citizen.id.value ) + "' data-active='" + ( skill->active ? "true" : "false" ) + "' aria-pressed='" + ( skill->active ? "true" : "false" ) + "' title='" + esc( tr( "management.population.skill_toggle_help" ) ) + "'><span class='c-list__primary'>" + esc( citizen.name ) + "</span><span class='c-list__meta'>" + esc( detail ) + "</span><span>" + esc( tr( skill->active ? "common.on" : "common.off" ) ) + "</span></button>";
	}
	rml( population_, "skill_citizen_rows", skillCitizens.empty() ? esc( tr( "management.population.no_skill_citizens" ) ) : skillCitizens );
	for ( const char* id : { "skill_enable_all", "skill_disable_all" } )
		if ( auto* button = population_->GetElementById( id ) )
		{
			button->SetClass( "is-disabled", skillCitizens.empty() );
			if ( skillCitizens.empty() ) button->SetAttribute( "disabled", "disabled" );
			else button->RemoveAttribute( "disabled" );
		}
	const bool professionEditable = s.selectedProfession && s.selectedProfession->value != "Gnomad";
	const auto selectedProfession = std::ranges::find_if( s.professions, [&]( const ProfessionRow& row ){ return s.selectedProfession == row.id; } );
	text( population_, "profession_editor_title", selectedProfession == s.professions.end() ? tr( "management.population.choose_profession" ) : selectedProfession->name );
	text( population_, "profession_editor_help", professionEditable ? tr( "management.population.profession_editor_help" ) : s.selectedProfession ? tr( "management.population.gnomad_protected" ) : tr( "management.population.choose_profession_help" ) );
	if ( auto* name = rmlui_dynamic_cast<Rml::ElementFormControl*>( population_->GetElementById( "profession_name" ) ) )
	{
		if ( context_.GetFocusElement() != name && name->GetValue() != s.professionDraftName ) name->SetValue( s.professionDraftName );
		if ( professionEditable ) name->RemoveAttribute( "disabled" ); else name->SetAttribute( "disabled", "disabled" );
	}
	std::string selectedSkills;
	for ( const auto& id : s.professionDraftSkills )
	{
		const auto skill = std::ranges::find_if( s.skillCatalog, [&]( const SkillCatalogRow& row ){ return row.id == id; } );
		const auto label = skill == s.skillCatalog.end() ? id.value : skill->name;
		selectedSkills += "<button id='" + stableId( "profession_selected_skill_", id.value ) + "' class='c-list__row" + std::string( s.selectedProfessionSkill == id ? " is-selected" : "" ) + "' data-skill='" + esc( id.value ) + "'><span class='c-list__primary'>" + esc( label ) + "</span></button>";
	}
	rml( population_, "profession_selected_skills", selectedSkills.empty() ? esc( tr( "management.population.no_profession_skills" ) ) : selectedSkills );
	std::string availableSkills;
	for ( const auto& skill : s.skillCatalog )
	{
		if ( std::ranges::find( s.professionDraftSkills, skill.id ) != s.professionDraftSkills.end() ) continue;
		availableSkills += "<button id='" + stableId( "profession_available_skill_", skill.id.value ) + "' class='c-list__row" + std::string( s.selectedAvailableSkill == skill.id ? " is-selected" : "" ) + "' data-skill='" + esc( skill.id.value ) + "'><span class='c-list__primary'>" + esc( skill.name ) + "</span></button>";
	}
	rml( population_, "profession_available_skills", availableSkills.empty() ? esc( tr( "management.population.no_available_skills" ) ) : availableSkills );
	for ( const char* id : { "profession_save", "profession_delete", "profession_skill_up", "profession_skill_down", "profession_skill_remove", "profession_skill_add" } )
		if ( auto* button = population_->GetElementById( id ) ) { button->SetClass( "is-disabled", !professionEditable ); if ( professionEditable ) button->RemoveAttribute( "disabled" ); else button->SetAttribute( "disabled", "disabled" ); }
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
	for ( const auto& [id, activity] : { std::pair { "schedule_set_none", ManagedScheduleActivity::None }, { "schedule_set_eat", ManagedScheduleActivity::Eat }, { "schedule_set_sleep", ManagedScheduleActivity::Sleep }, { "schedule_set_training", ManagedScheduleActivity::Training } } )
		if ( auto* button = population_->GetElementById( id ) ) { button->SetClass( "is-selected", s.scheduleActivity == activity ); button->SetAttribute( "aria-pressed", s.scheduleActivity == activity ? "true" : "false" ); }
	const auto scheduleSelected = s.selectedScheduleCell && std::ranges::any_of( s.schedules, [&]( const ScheduleRow& row ) { return row.creature == s.selectedScheduleCell->creature; } );
	std::string scheduleLabel = tr( "management.population.choose_cell" );
	if ( scheduleSelected )
	{
		const auto row = std::ranges::find_if( s.schedules, [&]( const ScheduleRow& item ) { return item.creature == s.selectedScheduleCell->creature; } );
		scheduleLabel = tr( "management.population.selected_cell", { { "citizen", row->name }, { "hour", std::to_string( s.selectedScheduleCell->hour ) } } );
	}
	text( population_, "schedule_scope_label", scheduleLabel );
	for ( const char* id : { "schedule_apply_cell", "schedule_apply_row", "schedule_apply_column" } )
		if ( auto* button = population_->GetElementById( id ) ) { button->SetClass( "is-disabled", !scheduleSelected ); if ( scheduleSelected ) button->RemoveAttribute( "disabled" ); else button->SetAttribute( "disabled", "disabled" ); }
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
				std::ostringstream xp;
				xp << skill.experience;
				const auto levelXp = tr( "management.population.skill_level_xp", { { "level", std::to_string( skill.level ) }, { "xp", xp.str() } } );
				skills += "<div id='" + skillId + "' class='m6b-skill-row" + ( skill.active ? " is-active" : "" ) + "' data-skill='" + esc( skill.id.value ) + "' data-active='" + active + "' role='checkbox' tab-index='0' aria-checked='" + active + "'><input type='checkbox' class='checkbox m6b-skill-checkbox' data-skill='" + esc( skill.id.value ) + "' data-active='" + active + "'" + ( skill.active ? " checked='checked'" : "" ) + "/><span class='m6b-skill-name'>" + esc( skill.name ) + " <span class='m6b-skill-group'>" + esc( skill.group ) + "</span></span><span class='m6b-skill-level'>" + esc( levelXp ) + "</span><span class='m6b-skill-state'>" + esc( tr( skill.active ? "common.on" : "common.off" ) ) + "</span></div>";
			}
		rml( population_, "creature_skills", skills.empty() ? esc( tr( "management.population.no_skills" ) ) : skills );
		std::string choices;
		for ( const auto& p : s.professions )
			choices += "<button id='" + stableId( "population_profession_choice_", p.id.value ) + "' class='c-dropdown-button' data-profession='" + esc( p.id.value ) + "'>" + esc( p.name ) + " v</button>";
		rml( population_, "creature_professions", choices.empty() ? esc( tr( "management.population.no_profession_choices" ) ) : choices );
	}
	}
	const InventoryRowsKey inventoryRowsKey { s.inventoryRevision, s.inventoryColumnFilters, s.inventoryColumnSelections, s.inventoryFilter, s.inventoryCategory, s.inventorySort, s.inventoryOwnedOnly, s.inventorySortDescending, s.selectedInventory };
	if ( inv && ( !renderedInventoryRows_ || *renderedInventoryRows_ != inventoryRowsKey ) )
	{
		if ( renderedInventoryRows_ && ( renderedInventoryRows_->filters != inventoryRowsKey.filters
			|| renderedInventoryRows_->selections != inventoryRowsKey.selections
			|| renderedInventoryRows_->legacyFilter != inventoryRowsKey.legacyFilter
			|| renderedInventoryRows_->category != inventoryRowsKey.category ) )
			if ( auto* rows = inventory_->GetElementById( "inventory_rows" ) ) rows->SetScrollTop( 0.f );
		const InventoryLabelLookup inventoryLabels( s.inventory );
		const auto visibleRows = controller_->inventoryPage();
		inventoryRowMarkup_.clear();
		inventoryRowMarkup_.reserve( visibleRows.size() );
		for ( const auto& r : visibleRows )
		{
			const bool selected = s.selectedInventory && *s.selectedInventory == r.id;
			const auto labels = inventoryLabels.labels( r );
			const auto cell = []( const std::string& value ) { return value.empty() ? std::string( "-" ) : esc( value ); };
			inventoryRowMarkup_.push_back( "<button id='" + stableId( "inventory_row_", inventoryKey( r.id ) ) + "' class='c-list__row l-m6b-row l-m6b-inventory-row l-inventory-flat-row" + ( selected ? " is-selected" : "" ) + "' role='row' aria-selected='" + ( selected ? std::string( "true" ) : std::string( "false" ) ) + "' data-depth='" + depth( r.id.depth ) + "' data-category='" + esc( r.id.category.value ) + "' data-group='" + esc( r.id.group.value ) + "' data-item='" + esc( r.id.item.value ) + "' data-material='" + esc( r.id.material.value ) + "' aria-label='" + cell( labels.category ) + ", " + cell( labels.group ) + ", " + cell( labels.item ) + ", " + cell( labels.material ) + "'><span class='l-inventory-flat__category'>" + cell( labels.category ) + "</span><span class='l-inventory-flat__group'>" + cell( labels.group ) + "</span><span class='l-inventory-flat__item'>" + inventorySprite( r ) + "<span class='l-inventory-flat__label'>" + cell( labels.item ) + "</span></span><span class='l-inventory-flat__material'>" + cell( labels.material ) + "</span><span class='l-m6b-inventory__metric'>" + std::to_string( r.stockpiled ) + "</span><span class='l-m6b-inventory__metric'>" + std::to_string( r.total ) + "</span></button>" );
		}
		if ( inventoryRowMarkup_.empty() ) inventoryRowMarkup_.push_back( "<div class='c-empty-state'>" + esc( tr( "management.inventory.no_rows" ) ) + "</div>" );
		inventoryViewportFirst_ = static_cast<std::size_t>( -1 );
		renderInventoryViewport();
		renderedInventoryRows_ = inventoryRowsKey;
	}
	if ( inv )
	{
		const auto detail = s.inventoryDetail ? std::ranges::find_if( s.inventory, [&]( const InventoryRow& row ) { return row.id == *s.inventoryDetail; } ) : s.inventory.end();
		const bool showingDetail = detail != s.inventory.end();
		visible( inventory_, "inventory_list", !showingDetail );
		visible( inventory_, "inventory_detail", showingDetail );
		if ( showingDetail )
		{
			const auto backLabel = tr( s.inventoryDetailBack.empty() ? "management.inventory.detail_back" : "management.inventory.detail_previous" );
			if ( backLabel != renderedInventoryBackLabel_ )
			{
				text( inventory_, "inventory_detail_back", backLabel );
				renderedInventoryBackLabel_ = backLabel;
			}
			const auto detailRml = [this]( const char* id, const std::string& markup )
			{
				auto& previous = renderedInventoryDetailMarkup_[id];
				if ( previous == markup ) return;
				rml( inventory_, id, markup );
				previous = markup;
			};
			std::string name = detail->name;
			if ( detail->id.depth == InventoryDepth::Material )
			{
				const auto parent = std::ranges::find_if( s.inventory, [&]( const InventoryRow& row ) { return row.id.depth == InventoryDepth::Item && row.id.item == detail->id.item; } );
				if ( parent != s.inventory.end() ) name += " " + parent->name;
			}
			text( inventory_, "inventory_detail_name", name );
			const auto metric = [&]( const char* key, std::uint32_t count )
				{ return "<span>" + esc( tr( key, { { "count", std::to_string( count ) } } ) ) + "</span>"; };
			detailRml( "inventory_detail_summary", inventorySprite( *detail ) + "<div class='l-m6b-item-detail__metrics'>" +
				metric( "management.inventory.detail_total", detail->total ) + metric( "management.inventory.detail_stock", detail->stockpiled ) +
				metric( "management.inventory.detail_loose", detail->loose ) + metric( "management.inventory.detail_equipped", detail->equipped ) +
				metric( "management.inventory.detail_jobs", detail->inJobs ) + "</div>" );
			std::unordered_map<std::string, const InventoryRow*> catalogItems;
			for ( const auto& row : s.inventory )
				if ( row.id.depth == InventoryDepth::Item ) catalogItems.try_emplace( row.id.item.value, &row );
			const auto recipeMarkup = [&]( const std::vector<ItemRecipe>& recipes )
			{
				std::string markup;
				for ( const auto& recipe : recipes )
				{
					markup += "<div class='l-m6b-item-detail__recipe'>";
					markup += "<strong>" + esc( recipe.id == recipe.outputItemID ? recipe.outputName : readableCatalogId( recipe.id ) ) + "</strong>";
					markup += "<span class='l-m6b-item-detail__meta'>" + esc( tr( "management.inventory.recipe_makes", { { "count", std::to_string( recipe.amount ) } } ) );
					if ( !recipe.workshop.empty() ) markup += " " + esc( tr( "management.inventory.recipe_at", { { "workshop", recipe.workshop } } ) );
					if ( !recipe.skill.empty() ) markup += " · " + esc( recipe.skill );
					markup += "</span><div class='l-m6b-item-detail__ingredients'>";
					for ( const auto& ingredient : recipe.ingredients )
					{
						const bool linkable = catalogItems.contains( ingredient.itemID ) && ( detail->id.depth != InventoryDepth::Item || ingredient.itemID != detail->id.item.value );
						markup += linkable ? "<button class='l-m6b-item-detail__link' data-item-link='" + esc( ingredient.itemID ) + "'>" : "<span class='l-m6b-item-detail__current'>";
						markup +=
							std::to_string( ingredient.amount ) + " × " + esc( ingredient.name );
						if ( !ingredient.allowedMaterial.empty() ) markup += " (" + esc( ingredient.allowedMaterial ) + ")";
						else if ( !ingredient.allowedMaterialType.empty() ) markup += " (" + esc( ingredient.allowedMaterialType ) + ")";
						markup += linkable ? "</button>" : "</span>";
					}
					markup += "</div></div>";
				}
				return markup.empty() ? "<p>" + esc( tr( "management.inventory.recipe_none" ) ) + "</p>" : markup;
			};
			detailRml( "inventory_detail_made_by", recipeMarkup( detail->madeBy ) );
			const InventoryLabelLookup outputLabels( s.inventory );
			std::map<std::string, std::vector<std::pair<std::string, std::string>>> productsByCategory;
			std::unordered_set<std::string> seenProducts;
			for ( const auto& recipe : detail->usedIn )
			{
				if ( !seenProducts.insert( recipe.outputItemID ).second ) continue;
				const auto product = catalogItems.find( recipe.outputItemID );
				if ( product == catalogItems.end() ) continue;
				std::string category = tr( "management.inventory.category_other" );
				const auto labels = outputLabels.labels( *product->second );
				if ( !labels.category.empty() ) category = labels.category;
				if ( !labels.group.empty() && labels.group != category ) category += " / " + labels.group;
				productsByCategory[category].emplace_back( recipe.outputItemID, product->second->name );
			}
			std::string productsMarkup;
			for ( auto& [category, products] : productsByCategory )
			{
				std::ranges::sort( products, {}, []( const auto& product ) { return foldedLabel( product.second ); } );
				productsMarkup += "<div class='l-m6b-item-detail__group'><h4>" + esc( category ) + "</h4>";
				for ( const auto& [id, productName] : products )
				{
					if ( detail->id.depth == InventoryDepth::Item && id == detail->id.item.value )
						productsMarkup += "<span class='l-m6b-item-detail__current'>" + esc( productName ) + "</span>";
					else productsMarkup += "<button class='l-m6b-item-detail__link' data-item-link='" + esc( id ) + "' data-product-link='" + esc( id ) + "'>" + esc( productName ) + "</button>";
				}
				productsMarkup += "</div>";
			}
			detailRml( "inventory_detail_used_in", productsMarkup.empty() ? "<p>" + esc( tr( "management.inventory.used_in_none" ) ) + "</p>" : productsMarkup );
			std::string locations;
			for ( const auto& location : detail->locations )
				locations += "<button id='inventory_stockpile_" + std::to_string( location.id ) + "' class='l-m6b-item-detail__link' data-stockpile-link='" + std::to_string( location.id ) + "'>" + esc( location.name ) + " <strong>" + std::to_string( location.count ) + "</strong></button>";
			detailRml( "inventory_detail_locations", locations.empty() ? "<p>" + esc( tr( "management.inventory.locations_none" ) ) + "</p>" : locations );
			std::string history;
			if ( s.inventoryHistoryLoading ) history = "<p>" + esc( tr( "management.inventory.history_loading" ) ) + "</p>";
			else if ( s.inventoryHistory.empty() ) history = "<p>" + esc( tr( "management.inventory.history_empty" ) ) + "</p>";
			else
			{
				int maximum = 1;
				for ( const auto& point : s.inventoryHistory ) maximum = std::max( maximum, point.total );
				for ( const auto& point : s.inventoryHistory )
				{
					const auto day = std::to_string( point.dayIndex + 1 );
					const auto label = tr( "management.inventory.history_point", { { "day", day }, { "total", std::to_string( point.total ) },
						{ "created", std::to_string( point.created ) }, { "destroyed", std::to_string( point.destroyed ) } } );
					history += "<div class='l-m6b-item-detail__history-row' title='" + esc( label ) + "'><span>" +
						esc( tr( "management.inventory.history_day", { { "day", day } } ) ) +
						"</span><div class='l-m6b-item-detail__history-track'><div style='display:block; width:" + std::to_string( std::clamp( point.total * 202 / maximum, 0, 202 ) ) +
						"dp; height:8dp; background-color:#235b37;'></div></div><strong>" + std::to_string( point.total ) + "</strong></div>";
				}
			}
			detailRml( "inventory_detail_history", history );
		}
	}
	if ( pop )
	{
	const auto populationTotal = controller_->visiblePopulation().size();
	const auto pageText = [&]( const char* key, std::size_t page, std::size_t total )
	{const auto first=total?std::min(page*Management6BState::pageSize+1,total):0;const auto last=std::min((page+1)*Management6BState::pageSize,total);return tr(key,{{"first",std::to_string(first)}, {"last",std::to_string(last)}, {"total",std::to_string(total)}}); };
	text( population_, "population_page_status", pageText( "management.population.page", s.populationPage, populationTotal ) );
	text( population_, "population_revision", tr( "management.population.revision" ) );
	}
	const auto status = s.pendingAction ? tr( "status.updating" ) : textCatalog_.contains( LocalizationKey { s.status } ) ? tr( s.status.c_str() ) : s.status;
	if ( pop ) text( population_, "population_status", status );
	if ( inv ) text( inventory_, "inventory_status", status );
}
void Management6BRmlBinding::renderInventoryViewport()
{
	if ( renderingInventoryViewport_ || !inventory_ ) return;
	auto* rows = inventory_->GetElementById( "inventory_rows" );
	if ( !rows ) return;
	constexpr float rowHeight = 48.f;
	constexpr std::size_t overscan = 6;
	const float scrollTop = std::clamp( rows->GetScrollTop(), 0.f,
		std::max( 0.f, static_cast<float>( inventoryRowMarkup_.size() ) * rowHeight - rows->GetClientHeight() ) );
	const std::size_t anchor = static_cast<std::size_t>( scrollTop / rowHeight );
	const std::size_t first = anchor > overscan ? anchor - overscan : 0;
	if ( first == inventoryViewportFirst_ ) return;
	const std::size_t viewportRows = std::max<std::size_t>( 20, static_cast<std::size_t>( std::ceil( std::max( rows->GetClientHeight(), 480.f ) / rowHeight ) ) + overscan * 2 );
	const std::size_t last = std::min( first + viewportRows, inventoryRowMarkup_.size() );
	std::string markup;
	markup.reserve( ( last - first ) * 720 + 160 );
	if ( first ) markup += "<div class='l-inventory-virtual-spacer' style='height:" + std::to_string( first * static_cast<std::size_t>( rowHeight ) ) + "dp;'></div>";
	for ( std::size_t index = first; index < last; ++index ) markup += inventoryRowMarkup_[index];
	if ( last < inventoryRowMarkup_.size() ) markup += "<div class='l-inventory-virtual-spacer' style='height:" + std::to_string( ( inventoryRowMarkup_.size() - last ) * static_cast<std::size_t>( rowHeight ) ) + "dp;'></div>";
	renderingInventoryViewport_ = true;
	rows->SetInnerRML( markup );
	rows->SetScrollTop( scrollTop );
	inventoryViewportFirst_ = first;
	renderingInventoryViewport_ = false;
}
} // namespace ingnomia::ui::management6b
