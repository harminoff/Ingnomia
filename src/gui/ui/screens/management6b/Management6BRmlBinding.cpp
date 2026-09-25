/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6BRmlBinding.h"
#include "../../runtime/CommandFeedback.h"

#include "../ManagementTooltip.h"
#include "../InventoryTableSchema.h"
#include "../../runtime/ReportControls.h"
#include "../../runtime/ConnectedTabs.h"
#include "../../runtime/SelectOptions.h"
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
	dialog_( c ), context_( c )
{
}
Management6BRmlBinding::~Management6BRmlBinding()
{
	shutdown();
}
bool Management6BRmlBinding::initialize( Management6BController& c )
{
	controller_ = &c;
	dialog_.setDocumentPath( "modals/win98_message_box.rml" );
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
	bind( population_, "population_close_button", [this] { closePopulation(); } );
	bind( population_, "population_refresh", [this]
		  { controller_->refresh(); } );
	// Column headings sort; clicking the sorted heading again reverses the order (PDF p.143).
	const auto sortBy = [this]( Sort key ) {
		const auto& s = controller_->state();
		controller_->setPopulationSort( key, s.populationSort == key && !s.populationSortDescending );
	};
	bind( population_, "population_sort_name", [sortBy] { sortBy( Sort::Name ); } );
	bind( population_, "population_sort_profession", [sortBy] { sortBy( Sort::Profession ); } );
	bind( population_, "population_open_citizen", [this] {
		if ( !controller_->state().selectedCreature ) return;
		controller_->selectCreature( *controller_->state().selectedCreature );
		if ( citizenSelectedHandler_ ) citizenSelectedHandler_();
	} );
	// F5 refreshes from any page; Ctrl+Tab / Ctrl+Shift+Tab switch pages (PDF p.147).
	bindEvent( population_, "population_workbench", "keydown", [this]( Rml::Event& e ) {
		const auto key  = e.GetParameter<int>( "key_identifier", 0 );
		const bool ctrl = e.GetParameter<int>( "ctrl_key", 0 ) != 0, shift = e.GetParameter<int>( "shift_key", 0 ) != 0;
		if ( key == Rml::Input::KI_F5 ) controller_->refresh();
		else if ( key == Rml::Input::KI_TAB && ctrl )
		{
			const std::array views { View::Citizens, View::Skills, View::Professions, View::Schedules };
			const auto current = controller_->state().populationView == View::Creature ? View::Citizens : controller_->state().populationView;
			const auto at      = std::ranges::find( views, current ) - views.begin();
			const auto next    = views[static_cast<std::size_t>( ( at + ( shift ? 3 : 1 ) ) % 4 )];
			reviewDraft( [this, next] { controller_->open( next ); } );
		}
		else return;
		e.StopPropagation();
	} );
	bind( population_, "population_page_previous", [this]
		  { controller_->changePopulationPage( -1 ); } );
	bind( population_, "population_page_next", [this]
		  { controller_->changePopulationPage( 1 ); } );
	// Activity option buttons: exactly one value is chosen; choosing it never changes a cell by itself.
	for ( const auto& [id, activity] : { std::pair { "schedule_set_none", ManagedScheduleActivity::None }, std::pair { "schedule_set_eat", ManagedScheduleActivity::Eat },
			  std::pair { "schedule_set_sleep", ManagedScheduleActivity::Sleep }, std::pair { "schedule_set_training", ManagedScheduleActivity::Training } } )
		bindEvent( population_, id, "change", [this, activity = activity]( Rml::Event& e ) { if ( !renderingPopulation_ && e.GetCurrentElement()->HasAttribute( "checked" ) ) controller_->setScheduleActivity( activity ); } );
	const auto selectedActivity = [this] {
		switch( controller_->state().scheduleActivity ) {
		case ManagedScheduleActivity::Eat: return ScheduleActivity::Eat;
		case ManagedScheduleActivity::Sleep: return ScheduleActivity::Sleep;
		case ManagedScheduleActivity::Training: return ScheduleActivity::Training;
		default: return ScheduleActivity::None;
		}
	};
	// Set Activity changes exactly the selected cells; more than one citizen asks first (proportionate review).
	bind( population_, "schedule_apply", [this, selectedActivity] { reviewScheduleScope( selectedActivity() ); } );
	bind( population_, "schedule_select_all", [this] { controller_->selectAllSchedule(); focusScheduleCell(); } );
	bindEvent( population_, "schedule_hours", "click", [this]( Rml::Event& e ) {
		for ( auto* x = e.GetTargetElement(); x && x != e.GetCurrentElement(); x = x->GetParentNode() )
		{
			const auto hour = x->GetAttribute<Rml::String>( "data-hour", "" );
			if ( hour.empty() ) continue;
			controller_->selectScheduleHour( static_cast<std::uint8_t>( std::stoul( hour ) ) );
			break;
		}
	} );
	bindEvent( population_, "schedule_names", "click", [this]( Rml::Event& e ) {
		for ( auto* x = e.GetTargetElement(); x && x != e.GetCurrentElement(); x = x->GetParentNode() )
		{
			const auto creature = x->GetAttribute<Rml::String>( "data-creature", "" );
			if ( creature.empty() ) continue;
			controller_->selectScheduleCitizen( CreatureId { static_cast<std::uint32_t>( std::stoul( creature ) ) } );
			break;
		}
	} );
	// Frozen headings: the hour row and citizen column follow the cell area's scroll position.
	bindEvent( population_, "schedule_rows", "scroll", [this]( Rml::Event& e ) {
		auto* cells = e.GetCurrentElement();
		if ( auto* hours = population_->GetElementById( "schedule_hours" ) ) hours->SetScrollLeft( cells->GetScrollLeft() );
		if ( auto* names = population_->GetElementById( "schedule_names" ) ) names->SetScrollTop( cells->GetScrollTop() );
	} );
	bind( population_, "skill_enable_all", [this]{ reviewSkillScope(true); } );
	bind( population_, "skill_disable_all", [this]{ reviewSkillScope(false); } );
	bindEvent( population_, "skills_catalog_rows", "click", [this]( Rml::Event& e ) { for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){ auto id=x->GetAttribute<Rml::String>("data-skill",""); if(!id.empty()){controller_->selectSkill(CatalogId{id});break;} } } );
	// Each citizen row carries a check box for the selected skill; checking it changes that citizen at once.
	bindEvent( population_, "skill_citizen_rows", "change", [this]( Rml::Event& e ) {
		if ( renderingPopulation_ || !controller_->state().selectedSkill ) return;
		auto* box    = e.GetTargetElement();
		const auto id = box ? box->GetAttribute<Rml::String>( "data-creature", "" ) : Rml::String();
		if ( id.empty() ) return;
		try { controller_->setSkill( CreatureId { static_cast<std::uint32_t>( std::stoul( id ) ) }, *controller_->state().selectedSkill, box->HasAttribute( "checked" ) ); } catch ( ... ) {}
		e.StopPropagation();
	} );
	// Choosing another profession in the drop-down list goes through the pending-changes review.
	bindEvent( population_, "profession_rows", "change", [this]( Rml::Event& e ) {
		if ( renderingPopulation_ ) return;
		auto* select = rmlui_dynamic_cast<Rml::ElementFormControl*>( e.GetCurrentElement() );
		const auto id = select ? select->GetValue() : Rml::String();
		if ( id.empty() || controller_->state().selectedProfession == ProfessionId { id } ) return;
		reviewDraft( [this, id] { controller_->selectProfession( ProfessionId { id } ); } );
		// If the review keeps the draft, show the unchanged selection again.
		if ( controller_->state().selectedProfession != ProfessionId { id } ) stateChanged( controller_->state() );
	} );
	bindEvent( population_, "profession_selected_skills", "click", [this]( Rml::Event& e ) { for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){ auto id=x->GetAttribute<Rml::String>("data-skill",""); if(!id.empty()){controller_->selectProfessionSkill(CatalogId{id});break;} } } );
	bindEvent( population_, "profession_available_skills", "click", [this]( Rml::Event& e ) { for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){ auto id=x->GetAttribute<Rml::String>("data-skill",""); if(!id.empty()){controller_->selectAvailableSkill(CatalogId{id});break;} } } );
	bind( population_, "profession_skill_add", [this]{ controller_->addProfessionSkill(); } );
	bind( population_, "profession_skill_remove", [this]{ controller_->removeProfessionSkill(); } );
	bind( population_, "profession_skill_up", [this]{ controller_->moveProfessionSkill(-1); } );
	bind( population_, "profession_skill_down", [this]{ controller_->moveProfessionSkill(1); } );
	// New creates "New Profession" (numbered when taken), like a new folder, and selects it for renaming.
	bind( population_, "profession_create", [this] {
		const auto& professions = controller_->state().professions;
		const auto taken = [&]( const std::string& name ) { return std::ranges::any_of( professions, [&]( const auto& p ) { return p.name == name || p.id.value == name; } ); };
		std::string name = "New Profession";
		for ( int n = 2; taken( name ); ++n ) name = "New Profession " + std::to_string( n );
		controller_->createProfession( name );
	} );
	bindEvent( population_, "profession_name", "input", [this]( Rml::Event& event ) { if ( auto* name = rmlui_dynamic_cast<Rml::ElementFormControl*>( event.GetCurrentElement() ) ) controller_->setProfessionDraftName( name->GetValue() ); } );
	bind( population_, "profession_save", [this]{ if(auto* e=rmlui_dynamic_cast<Rml::ElementFormControl*>(population_->GetElementById("profession_name")))controller_->setProfessionDraftName(e->GetValue()); controller_->saveProfession(); } );
    bind(population_, "profession_delete", [this]{reviewProfessionDelete();});
    bind(population_, "profession_discard", [this]{controller_->discardProfessionDraft();});
	// Click selects a citizen; double-click (like Enter or Properties) opens it.
	for ( const char* event : { "click", "dblclick" } )
		bindEvent( population_, "population_rows", event, [this, open = std::string_view( event ) == "dblclick"]( Rml::Event& e )
				   {for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto raw=x->GetAttribute<Rml::String>("data-creature","");if(raw.empty())continue;try{const CreatureId id{static_cast<std::uint32_t>(std::stoul(raw))};if(open){controller_->selectCreature(id);if(citizenSelectedHandler_)citizenSelectedHandler_();}else controller_->highlightCreature(id);}catch(...){}break;} } );
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
	// Click selects one cell; Shift+click extends the range from the anchor (PDF p.53-60, Excel 97).
	bindEvent( population_, "schedule_rows", "click", [this]( Rml::Event& e )
			   {const bool shift=e.GetParameter<int>("shift_key",0)!=0;for(auto*x=e.GetTargetElement();x&&x!=e.GetCurrentElement();x=x->GetParentNode()){const auto creature=x->GetAttribute<Rml::String>("data-creature","");const auto hour=x->GetAttribute<Rml::String>("data-hour","");if(creature.empty()||hour.empty())continue;try{const ScheduleCellId cell{CreatureId{static_cast<std::uint32_t>(std::stoul(creature))},static_cast<std::uint8_t>(std::stoul(hour))};if(shift)controller_->extendScheduleSelection(cell);else controller_->selectScheduleCell(cell);focusScheduleCell();}catch(...){}break;} } );
	// Keyboard: arrows move the active cell (Shift extends), Home/End go to the row ends (Ctrl: grid corners),
	// Shift+Space selects the citizen's day, Ctrl+Space the hour, Ctrl+A everything, Enter or Space sets the activity.
	bindEvent( population_, "schedule_rows", "keydown", [this, selectedActivity]( Rml::Event& e ) {
		const auto key   = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
		const bool shift = e.GetParameter<int>( "shift_key", 0 ) != 0, ctrl = e.GetParameter<int>( "ctrl_key", 0 ) != 0;
		const auto& s    = controller_->state();
		if ( key == Rml::Input::KI_LEFT ) controller_->moveScheduleFocus( -1, 0, shift );
		else if ( key == Rml::Input::KI_RIGHT ) controller_->moveScheduleFocus( 1, 0, shift );
		else if ( key == Rml::Input::KI_UP ) controller_->moveScheduleFocus( 0, -1, shift );
		else if ( key == Rml::Input::KI_DOWN ) controller_->moveScheduleFocus( 0, 1, shift );
		else if ( key == Rml::Input::KI_HOME ) controller_->moveScheduleFocus( -24, ctrl ? -100000 : 0, shift );
		else if ( key == Rml::Input::KI_END ) controller_->moveScheduleFocus( 24, ctrl ? 100000 : 0, shift );
		else if ( key == Rml::Input::KI_PRIOR ) controller_->moveScheduleFocus( 0, -8, shift );
		else if ( key == Rml::Input::KI_NEXT ) controller_->moveScheduleFocus( 0, 8, shift );
		else if ( key == Rml::Input::KI_SPACE && shift && s.selectedScheduleCell ) controller_->selectScheduleCitizen( s.selectedScheduleCell->creature );
		else if ( key == Rml::Input::KI_SPACE && ctrl && s.selectedScheduleCell ) controller_->selectScheduleHour( s.selectedScheduleCell->hour );
		else if ( key == Rml::Input::KI_A && ctrl ) controller_->selectAllSchedule();
		else if ( key == Rml::Input::KI_RETURN || ( key == Rml::Input::KI_SPACE && !shift && !ctrl ) ) reviewScheduleScope( selectedActivity() );
		else return;
		e.StopPropagation();
		focusScheduleCell();
	} );
	// ---- Inventory report window (Stage 21b): Close only; the row check box watches an item at once.
	const auto rowIdOf = []( Rml::Element* x ) {
		const auto d = x->GetAttribute<Rml::String>( "data-depth", "" );
		return InventoryRowId { CatalogId { x->GetAttribute<Rml::String>( "data-category", "" ) }, CatalogId { x->GetAttribute<Rml::String>( "data-group", "" ) },
			CatalogId { x->GetAttribute<Rml::String>( "data-item", "" ) }, CatalogId { x->GetAttribute<Rml::String>( "data-material", "" ) },
			d == "category" ? InventoryDepth::Category : d == "group" ? InventoryDepth::Group : d == "material" ? InventoryDepth::Material : InventoryDepth::Item };
	};
	const auto rowOf = []( Rml::Event& e ) -> Rml::Element* {
		for ( auto* x = e.GetTargetElement(); x && x != e.GetCurrentElement(); x = x->GetParentNode() )
			if ( x->HasAttribute( "data-depth" ) ) return x;
		return nullptr;
	};
	const auto openSelected = [this] {
		if ( const auto& selected = controller_->state().selectedInventory ) controller_->openInventoryDetail( *selected );
	};
	for ( const char* id : { "inventory_close", "inventory_close_button" } ) bind( inventory_, id, [this] { closeInventory(); } );
	bindEvent( inventory_, "inventory_workbench", "keydown", [this]( Rml::Event& e ) {
		if ( dialog_.active() || controller_->state().inventoryDetail ) return;
		const auto key = e.GetParameter<int>( "key_identifier", 0 );
		if ( key == Rml::Input::KI_ESCAPE ) { closeInventory(); e.StopPropagation(); }
	} );
	for ( const char* event : { "input", "change" } )
		bindEvent( inventory_, "inventory_search", event, [this]( Rml::Event& ) {
			if ( renderingInventory_ ) return;
			if ( auto* f = rmlui_dynamic_cast<Rml::ElementFormControl*>( inventory_->GetElementById( "inventory_search" ) ) ) controller_->setInventoryFilter( f->GetValue() );
		} );
	bindEvent( inventory_, "inventory_category", "change", [this]( Rml::Event& ) {
		if ( renderingInventory_ ) return;
		auto* f = rmlui_dynamic_cast<Rml::ElementFormControl*>( inventory_->GetElementById( "inventory_category" ) );
		if ( f && f->GetValue() != controller_->state().inventoryCategory ) controller_->setInventoryCategory( f->GetValue() );
	} );
	bindEvent( inventory_, "inventory_owned_only", "change", [this]( Rml::Event& ) {
		if ( renderingInventory_ ) return;
		if ( auto* box = inventory_->GetElementById( "inventory_owned_only" ) ) controller_->setInventoryOwnedOnly( box->HasAttribute( "checked" ) );
	} );
	for ( const auto& [id, sort] : std::array { std::pair { "inventory_sort_item", Sort::Item }, std::pair { "inventory_sort_material", Sort::Material },
			  std::pair { "inventory_sort_stock", Sort::Stock }, std::pair { "inventory_sort_total", Sort::Total } } )
		bind( inventory_, id, [this, sort = sort] { controller_->setInventorySort( sort ); } );
	// A click selects the row; the check box on a row watches the item (PDF p.136: list view with check box
	// state images). The box has already toggled when the click reaches the list.
	bindEvent( inventory_, "inventory_rows", "click", [this, rowIdOf, rowOf]( Rml::Event& e ) {
		auto* row = rowOf( e );
		if ( !row ) return;
		const auto id = rowIdOf( row );
		auto* target = e.GetTargetElement();
		const bool box = target && target->GetTagName() == "input";
		const bool watched = box && target->HasAttribute( "checked" );
		controller_->selectInventory( id );
		if ( box ) controller_->setWatched( id, watched );
		e.StopPropagation();
	} );
	bindEvent( inventory_, "inventory_rows", "dblclick", [this, rowIdOf, rowOf]( Rml::Event& e ) {
		if ( auto* row = rowOf( e ) ) { controller_->selectInventory( rowIdOf( row ) ); controller_->openInventoryDetail( rowIdOf( row ) ); e.StopPropagation(); }
	} );
	bindEvent( inventory_, "inventory_rows", "keydown", [this, openSelected]( Rml::Event& e ) {
		const auto key = static_cast<Rml::Input::KeyIdentifier>( e.GetParameter<int>( "key_identifier", 0 ) );
		if ( key == Rml::Input::KI_UP ) controller_->moveInventorySelection( -1 );
		else if ( key == Rml::Input::KI_DOWN ) controller_->moveInventorySelection( 1 );
		else if ( key == Rml::Input::KI_HOME ) controller_->moveInventorySelection( -2147483647 );
		else if ( key == Rml::Input::KI_END ) controller_->moveInventorySelection( 2147483647 );
		else if ( key == Rml::Input::KI_SPACE ) controller_->toggleSelectedWatch();
		else if ( key == Rml::Input::KI_RETURN ) openSelected();
		else return;
		e.StopPropagation();
		focusInventoryRow();
	} );
	bindEvent( inventory_, "inventory_rows", "scroll", [this]( Rml::Event& ) { renderInventoryViewport(); } );
	bind( inventory_, "inventory_open_item", openSelected );

	// ---- Item Properties: a subordinate property sheet with Close only.
	const auto closeDetail = [this] {
		controller_->closeInventoryDetail();
		focusInventoryRow();
	};
	for ( const char* id : { "inventory_detail_close", "inventory_detail_close_button" } ) bind( inventory_, id, closeDetail );
	bindEvent( inventory_, "inventory_detail", "keydown", [this, closeDetail]( Rml::Event& e ) {
		if ( dialog_.active() ) return;
		const auto key = e.GetParameter<int>( "key_identifier", 0 );
		if ( key == Rml::Input::KI_ESCAPE || ( key == Rml::Input::KI_RETURN && e.GetTargetElement() && e.GetTargetElement()->GetTagName() != "button" ) ) { closeDetail(); e.StopPropagation(); }
	} );
	for ( const auto& [id, pane] : std::array { std::pair { "inventory_detail_tab_general", 0 }, std::pair { "inventory_detail_tab_stockpiles", 1 },
			  std::pair { "inventory_detail_tab_recipes", 2 }, std::pair { "inventory_detail_tab_history", 3 } } )
		bind( inventory_, id, [this, pane = pane] { inventoryDetailPane_ = pane; stateChanged( controller_->state() ); } );
	bindEvent( inventory_, "inventory_detail_watch", "change", [this]( Rml::Event& ) {
		if ( renderingInventory_ ) return;
		const auto& state = controller_->state();
		if ( state.inventoryDetail ) controller_->setWatched( *state.inventoryDetail, inventory_->GetElementById( "inventory_detail_watch" )->HasAttribute( "checked" ) );
	} );
	const auto openStockpile = [this] {
		if ( inventoryDetailStockpile_ && stockpileOpenHandler_ ) stockpileOpenHandler_( *inventoryDetailStockpile_ );
	};
	const auto openProduct = [this] {
		if ( !inventoryDetailProduct_.empty() ) controller_->openRelatedInventoryItem( inventoryDetailProduct_ );
	};
	const auto pick = []( Rml::Event& e, const char* attribute ) -> std::string {
		for ( auto* x = e.GetTargetElement(); x && x != e.GetCurrentElement(); x = x->GetParentNode() )
			if ( x->HasAttribute( attribute ) ) return x->GetAttribute<Rml::String>( attribute, "" );
		return {};
	};
	bindEvent( inventory_, "inventory_detail_locations", "click", [this, pick]( Rml::Event& e ) {
		const auto id = pick( e, "data-stockpile-link" );
		if ( id.empty() ) return;
		inventoryDetailStockpile_ = static_cast<unsigned int>( std::stoul( id ) );
		stateChanged( controller_->state() );
	} );
	bindEvent( inventory_, "inventory_detail_locations", "dblclick", [openStockpile]( Rml::Event& ) { openStockpile(); } );
	bind( inventory_, "inventory_detail_open_stockpile", openStockpile );
	bindEvent( inventory_, "inventory_detail_used_in", "click", [this, pick]( Rml::Event& e ) {
		const auto id = pick( e, "data-item-link" );
		if ( id.empty() ) return;
		inventoryDetailProduct_ = id;
		stateChanged( controller_->state() );
	} );
	bindEvent( inventory_, "inventory_detail_used_in", "dblclick", [openProduct]( Rml::Event& ) { openProduct(); } );
	bind( inventory_, "inventory_detail_open_product", openProduct );
	for ( const char* id : { "population_close" } )
	{
		if ( auto* target = population_->GetElementById( id ) ) target->SetAttribute( "aria-describedby", "population_tooltip" );
		for ( const char* event : { "mouseover", "focus" } )
			bindEvent( population_, id, event, [this]( Rml::Event& e ) {
				showManagementTooltip( population_, context_, "population_tooltip", e.GetCurrentElement() );
			} );
		for ( const char* event : { "mouseout", "blur" } )
			bindEvent( population_, id, event, [this]( Rml::Event& ) { hideManagementTooltip( population_, "population_tooltip" ); } );
	}
    // RmlUi fixed positioning uses the positioned ancestor, not the viewport.
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
    dialog_.close(false);
    reportOptions_={};
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
	renderedInventoryDetailMarkup_.clear();
	renderedInventoryDetail_.reset();
	inventoryRowMarkup_.clear();
	inventoryRows_.clear();
	inventoryRowIds_.clear();
	inventoryCategoryOptions_.clear();
	inventoryFilterKey_.clear();
	inventoryViewportFirst_ = static_cast<std::size_t>( -1 );
	renderingInventoryViewport_ = false;
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
    if(controller_->state().professionDraftDirty) {
        reviewDraft([this]{closePopulation();}); return;
    }
    dialog_.close(false);
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
void Management6BRmlBinding::reviewDraft(std::function<void()> next)
{
    if(controller_->state().professionSavePending) return;
    if(!controller_->state().professionDraftDirty) { next(); return; }
    const auto tr=[this](const char* key){return textCatalog_.format(LocalizationKey{key});};
    dialog_.show(tr("editing.title"), textCatalog_.format(LocalizationKey{"editing.detail"},{{"name",controller_->state().selectedProfession ? controller_->state().selectedProfession->value : controller_->state().professionDraftName}}), tr("editing.apply"), tr("editing.keep"),
        [this,next]{controller_->saveProfession(); if(!controller_->state().professionDraftDirty) next();}, {},
        tr("editing.discard"), [this,next]{controller_->discardProfessionDraft(); next();});
}
void Management6BRmlBinding::reviewSkillScope(bool active)
{
    const auto& s=controller_->state(); if(!s.selectedSkill) return;
    const auto skill=*s.selectedSkill; const auto world=s.world; const auto revision=s.populationRevision;
    const auto tr=[this](const char* key){return textCatalog_.format(LocalizationKey{key});};
    dialog_.show(tr("editing.scope_title"),textCatalog_.format(LocalizationKey{"editing.scope_detail"},
        {{"count",std::to_string(s.population.size())},{"skill",skill.value}}),
        tr(active ? "editing.enable" : "editing.disable"),tr("common.cancel"),
        [this,skill,world,revision,active]{controller_->setSkillForAllReviewed(skill,active,world,revision);});
}
std::string Management6BRmlBinding::scheduleScopeText( const Management6BState& s, const ScheduleScope& scope ) const
{
	if ( scope.empty() ) return "Click a cell, a citizen or an hour. Letters: E eat, S sleep, T train.";
	const char* activity = s.scheduleActivity == ManagedScheduleActivity::Eat ? "Eat" : s.scheduleActivity == ManagedScheduleActivity::Sleep ? "Sleep" : s.scheduleActivity == ManagedScheduleActivity::Training ? "Train" : "None";
	const auto name = [&]( CreatureId id ) {
		const auto row = std::ranges::find_if( s.schedules, [&]( const auto& r ) { return r.creature == id; } );
		return row == s.schedules.end() ? std::string( "?" ) : row->name;
	};
	const auto hours = scope.fullDay() ? std::string( "all 24 hours" )
		: scope.firstHour == scope.lastHour ? "hour " + std::to_string( scope.firstHour ) : "hours " + std::to_string( scope.firstHour ) + "-" + std::to_string( scope.lastHour );
	const auto who = scope.allCitizens && scope.citizens.size() > 1 ? "all " + std::to_string( scope.citizens.size() ) + " citizens"
		: scope.citizens.size() == 1 ? name( scope.citizens.front() ) : std::to_string( scope.citizens.size() ) + " citizens (" + name( scope.citizens.front() ) + " to " + name( scope.citizens.back() ) + ")";
	return "Set Activity sets " + hours + " for " + who + " to " + activity + ".";
}
void Management6BRmlBinding::reviewScheduleScope( ScheduleActivity activity )
{
	const auto scope = controller_->scheduleScope();
	if ( scope.empty() ) return;
	if ( scope.citizens.size() == 1 )
	{
		controller_->applyScheduleScope( activity, scope );
		return;
	}
	// Several citizens: state the exact scope, including rows scrolled out of view, before changing anything.
	dialog_.show( "Population", scheduleScopeText( controller_->state(), scope ) + " This changes " + std::to_string( scope.cells() ) + " cells, including rows scrolled out of view.", "Set", "Cancel",
		[this, activity, scope] { controller_->applyScheduleScope( activity, scope ); } );
}
void Management6BRmlBinding::reviewProfessionDelete()
{
    if(controller_->state().professionDraftDirty) { reviewDraft([this]{reviewProfessionDelete();}); return; }
    const auto& state=controller_->state();
    const auto row=std::ranges::find_if(state.professions,[&](const auto& r){return state.selectedProfession==r.id;});
    if(row==state.professions.end() || row->id.value=="Gnomad") return;
    const auto reviewed=*row; const auto world=state.world;
    const auto tr=[this](const char* key){return textCatalog_.format(LocalizationKey{key});};
    const auto detail=textCatalog_.format(LocalizationKey{"editing.delete_detail"},{{"name",row->name}});
    dialog_.show(row->name,detail,
        tr("management.population.delete"),tr("common.cancel"),
        [this,reviewed,world]{controller_->deleteReviewedProfession(world,reviewed);});
}
void Management6BRmlBinding::closeRoute()
{
	if ( !controller_ || !controller_->state().open )
		return;
    if(controller_->state().professionDraftDirty) { reviewDraft([this]{closeRoute();}); return; }
    dialog_.close(false);
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
	if ( !inventory_ || !controller_ || !controller_->state().selectedInventory ) return;
	auto* list = inventory_->GetElementById( "inventory_rows" );
	if ( !list ) return;
	const auto selected = std::ranges::find( inventoryRowIds_, *controller_->state().selectedInventory );
	if ( selected == inventoryRowIds_.end() ) return;
	inventory_->UpdateDocument();
	const float height = inventoryRowHeight_ > 0.f ? inventoryRowHeight_ : 16.f;
	const float top = float( selected - inventoryRowIds_.begin() ) * height;
	if ( top < list->GetScrollTop() ) list->SetScrollTop( top );
	else if ( top + height > list->GetScrollTop() + list->GetClientHeight() ) list->SetScrollTop( top + height - list->GetClientHeight() );
	renderInventoryViewport();
	if ( auto* target = inventory_->GetElementById( stableId( "inventory_row_", inventoryKey( *selected ) ) ) ) target->Focus();
}
void Management6BRmlBinding::focusScheduleCell()
{
	if ( !population_ || !controller_->state().selectedScheduleCell )
		return;
	const auto& c = *controller_->state().selectedScheduleCell;
	if ( auto* e = population_->GetElementById( "schedule_" + std::to_string( c.creature.value ) + "_" + std::to_string( c.hour ) ) )
	{
		e->Focus();
		population_->UpdateDocument(); // the rows were just rebuilt; lay them out before scrolling
		e->ScrollIntoView( Rml::ScrollIntoViewOptions( Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest ) );
		if ( auto* cells = population_->GetElementById( "schedule_rows" ) )
		{
			if ( auto* hours = population_->GetElementById( "schedule_hours" ) ) hours->SetScrollLeft( cells->GetScrollLeft() );
			if ( auto* names = population_->GetElementById( "schedule_names" ) ) names->SetScrollTop( cells->GetScrollTop() );
		}
	}
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
		controller_->selectInventory( row->id );
		controller_->openInventoryDetail( row->id );
		return controller_->state().inventoryDetail.has_value();
	}
	return false;
}
bool Management6BRmlBinding::focusInventoryRowsForProbe()
{
	if ( !controller_ || !inventory_ || !controller_->state().inventoryOpen || controller_->state().inventoryDetail )
		return false;
	auto* rows = inventory_->GetElementById( "inventory_rows" );
	if ( !rows )
		return false;
	rows->Focus();
	return true;
}
void Management6BRmlBinding::stateChanged( const Management6BState& s )
{
	if ( !population_ || !inventory_ )
		return;
    if(!s.populationOpen) dialog_.close(false);
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
	visible( population_, "population_citizens", pop && ( s.populationView == View::Citizens || s.populationView == View::Creature ) );
	visible( population_, "population_skills", pop && ( s.populationView == View::Skills ) );
	visible( population_, "population_professions", pop && ( s.populationView == View::Professions ) );
	visible( population_, "population_schedules", pop && ( s.populationView == View::Schedules ) );
	visible( population_, "population_loading", s.loadingPopulation );
	if ( pop )
	{
	renderingPopulation_ = true;
	if ( renderedPopulationView_ != s.populationView )
	{
		hideManagementTooltip( population_, "population_tooltip" );
		renderedPopulationView_ = s.populationView;
	}
	const auto check = []( bool on ) { return std::string( on ? " checked='checked'" : "" ); };
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
	if ( auto* tabs = population_->GetElementById( "population_tabs" ) )
		connected_tabs::select( *tabs, population_->GetElementById( s.populationView == View::Skills ? "population_tab_skills" : s.populationView == View::Professions ? "population_tab_professions"
			: s.populationView == View::Schedules ? "population_tab_schedules" : "population_tab_citizens" ) );

	// ---- Citizens: list view in details view, sortable headings, the whole filtered roster scrolls.
	// The sorted heading carries a small arrow; pointing down means descending (PDF p.143).
	const auto heading = [&]( const char* id, const char* label, Sort key ) {
		const bool sorted = s.populationSort == key;
		rml( population_, id, esc( label ) + ( sorted ? std::string( "<span class='w98-sort-mark" ) + ( s.populationSortDescending ? " is-descending" : "" ) + "'></span>" : std::string() ) );
	};
	heading( "population_sort_name", "Name", Sort::Name );
	heading( "population_sort_profession", "Profession", Sort::Profession );
	const auto visibleRows = controller_->visiblePopulation();
	std::string rows;
	for ( const auto& r : visibleRows )
	{
		const bool selected = s.selectedCreature && *s.selectedCreature == r.id;
		rows += "<button id='population_creature_" + std::to_string( r.id.value ) + "' class='w98-list-item" + std::string( selected ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( selected ? "true" : "false" ) + "' data-creature='" + std::to_string( r.id.value )
			+ "'><span class='w98-cell w98-cell--grow'>" + esc( r.name ) + "</span><span class='w98-cell w98-w-profession'>" + esc( r.profession.value.empty() ? tr( "management.population.no_profession" ) : r.profession.value ) + "</span></button>";
	}
	rml( population_, "population_rows", rows.empty() ? "<div class='w98-list-empty'>" + esc( s.population.empty() ? "There are no citizens." : "No citizens match the text." ) + "</div>" : rows );
	text( population_, "population_page_status", std::to_string( visibleRows.size() ) + ( visibleRows.size() == s.population.size() ? std::string( visibleRows.size() == 1 ? " citizen" : " citizens" ) : " of " + std::to_string( s.population.size() ) + " citizens" ) );
	if ( auto* open = population_->GetElementById( "population_open_citizen" ) )
	{
		if ( s.selectedCreature ) open->RemoveAttribute( "disabled" );
		else open->SetAttribute( "disabled", "disabled" );
	}

	// ---- Skills: skill list box; citizens with that skill as check boxes; bulk commands name their scope.
	std::string catalog;
	for ( const auto& skill : s.skillCatalog )
		catalog += "<button id='" + stableId( "skill_catalog_", skill.id.value ) + "' class='w98-list-item" + std::string( s.selectedSkill == skill.id ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( s.selectedSkill == skill.id ? "true" : "false" ) + "' data-skill='" + esc( skill.id.value ) + "'><span class='w98-cell w98-cell--grow'>" + esc( skill.name ) + "</span></button>";
	rml( population_, "skills_catalog_rows", catalog.empty() ? "<div class='w98-list-empty'>" + esc( tr( "management.population.no_skill_catalog" ) ) + "</div>" : catalog );
	const auto selectedSkill = std::ranges::find_if( s.skillCatalog, [&]( const SkillCatalogRow& row ){ return s.selectedSkill == row.id; } );
	text( population_, "skill_focus_title", selectedSkill == s.skillCatalog.end() ? std::string( "Citizens:" ) : "Citizens who may use " + selectedSkill->name + ":" );
	std::string skillCitizens;
	std::size_t skillCount = 0;
	if ( s.selectedSkill ) for ( const auto& citizen : s.population )
	{
		const auto skill = std::ranges::find_if( citizen.skills, [&]( const SkillRow& row ){ return row.id == *s.selectedSkill; } );
		if ( skill == citizen.skills.end() ) continue;
		++skillCount;
		const auto id = std::to_string( citizen.id.value );
		skillCitizens += "<label id='skill_citizen_" + id + "' class='w98-list-item' for='skill_citizen_check_" + id + "'><input id='skill_citizen_check_" + id + "' class='checkbox' type='checkbox' data-creature='" + id + "'" + check( skill->active ) + "/>"
			+ "<span class='w98-cell w98-cell--grow'>" + esc( citizen.name ) + "</span><span class='w98-cell w98-cell--num w98-w-amount'>" + std::to_string( skill->level ) + "</span></label>";
	}
	rml( population_, "skill_citizen_rows", skillCitizens.empty() ? "<div class='w98-list-empty'>" + esc( s.selectedSkill ? tr( "management.population.no_skill_citizens" ) : std::string( "Select a skill." ) ) + "</div>" : skillCitizens );
	text( population_, "skill_focus_group", selectedSkill == s.skillCatalog.end() ? std::string( "Checked citizens may do this work." )
		: "Enable for All and Disable for All change " + selectedSkill->name + " for all " + std::to_string( skillCount ) + ( skillCount == 1 ? " citizen." : " citizens." ) );
	for ( const char* id : { "skill_enable_all", "skill_disable_all" } )
		if ( auto* button = population_->GetElementById( id ) )
		{
			if ( skillCitizens.empty() ) button->SetAttribute( "disabled", "disabled" );
			else button->RemoveAttribute( "disabled" );
		}

	// ---- Professions: drop-down list, name, two lists with transfer buttons between them.
	const bool professionEditable = s.selectedProfession && s.selectedProfession->value != "Gnomad" && !s.professionSavePending;
	std::vector<std::pair<std::string, std::string>> professionValues;
	for ( const auto& r : s.professions ) professionValues.emplace_back( r.id.value, r.name );
	std::string professionOptions = s.selectedProfession ? std::string() : std::string( "|" );
	for ( const auto& [value, label] : professionValues ) professionOptions += value + "=" + label + "\n";
	if ( professionOptions != renderedProfessionOptions_ )
	{
		renderedProfessionOptions_ = professionOptions;
		setSelectOptions( population_->GetElementById( "profession_rows" ), professionValues, !s.selectedProfession );
	}
	if ( auto* select = rmlui_dynamic_cast<Rml::ElementFormControl*>( population_->GetElementById( "profession_rows" ) ) )
	{
		const auto wanted = s.selectedProfession ? s.selectedProfession->value : std::string();
		if ( select->GetValue() != wanted ) select->SetValue( wanted );
	}
	text( population_, "profession_editor_help", professionEditable ? std::string( "Citizens with this profession use these skills, highest priority first." ) : s.selectedProfession ? tr( "management.population.gnomad_protected" ) : tr( "management.population.choose_profession_help" ) );
	if ( auto* name = rmlui_dynamic_cast<Rml::ElementFormControl*>( population_->GetElementById( "profession_name" ) ) )
	{
		const auto shownName = s.selectedProfession ? s.professionDraftName : std::string();
		if ( context_.GetFocusElement() != name && name->GetValue() != shownName ) name->SetValue( shownName );
		if ( professionEditable ) name->RemoveAttribute( "disabled" ); else name->SetAttribute( "disabled", "disabled" );
	}
	text( population_, "profession_feedback", s.professionFeedback.empty() ? ( s.professionSavePending ? std::string( "Saving..." ) : s.professionDraftDirty ? std::string( "You have unsaved changes." ) : std::string() ) : commandFeedbackText( textCatalog_, s.professionFeedback ) );
	std::string selectedSkills;
	for ( const auto& id : s.selectedProfession ? s.professionDraftSkills : std::vector<CatalogId> {} )
	{
		const auto skill = std::ranges::find_if( s.skillCatalog, [&]( const SkillCatalogRow& row ){ return row.id == id; } );
		selectedSkills += "<button id='" + stableId( "profession_selected_skill_", id.value ) + "' class='w98-list-item" + std::string( s.selectedProfessionSkill == id ? " is-selected" : "" ) + "' data-skill='" + esc( id.value ) + "'><span class='w98-cell w98-cell--grow'>" + esc( skill == s.skillCatalog.end() ? id.value : skill->name ) + "</span></button>";
	}
	rml( population_, "profession_selected_skills", selectedSkills.empty() ? "<div class='w98-list-empty'>" + esc( tr( "management.population.no_profession_skills" ) ) + "</div>" : selectedSkills );
	std::string availableSkills;
	for ( const auto& skill : s.skillCatalog )
	{
		if ( std::ranges::find( s.professionDraftSkills, skill.id ) != s.professionDraftSkills.end() ) continue;
		availableSkills += "<button id='" + stableId( "profession_available_skill_", skill.id.value ) + "' class='w98-list-item" + std::string( s.selectedAvailableSkill == skill.id ? " is-selected" : "" ) + "' data-skill='" + esc( skill.id.value ) + "'><span class='w98-cell w98-cell--grow'>" + esc( skill.name ) + "</span></button>";
	}
	rml( population_, "profession_available_skills", availableSkills.empty() ? "<div class='w98-list-empty'>" + esc( tr( "management.population.no_available_skills" ) ) + "</div>" : availableSkills );
	// Commands are unavailable when they cannot act (PDF p.323).
	const auto selectedIndex = s.selectedProfessionSkill ? std::ranges::find( s.professionDraftSkills, *s.selectedProfessionSkill ) - s.professionDraftSkills.begin() : -1;
	const bool inProfession  = selectedIndex >= 0 && selectedIndex < static_cast<std::ptrdiff_t>( s.professionDraftSkills.size() );
	const bool availableOk   = s.selectedAvailableSkill && std::ranges::find( s.professionDraftSkills, *s.selectedAvailableSkill ) == s.professionDraftSkills.end();
	const auto enable = [this]( const char* id, bool on ) { if ( auto* b = population_->GetElementById( id ) ) { if ( on ) b->RemoveAttribute( "disabled" ); else b->SetAttribute( "disabled", "disabled" ); } };
	enable( "profession_skill_add", professionEditable && availableOk );
	enable( "profession_skill_remove", professionEditable && inProfession );
	enable( "profession_skill_up", professionEditable && inProfession && selectedIndex > 0 );
	enable( "profession_skill_down", professionEditable && inProfession && selectedIndex + 1 < static_cast<std::ptrdiff_t>( s.professionDraftSkills.size() ) );
	enable( "profession_save", professionEditable && s.professionDraftDirty );
	enable( "profession_discard", s.professionDraftDirty && !s.professionSavePending );
	enable( "profession_delete", professionEditable );
	enable( "profession_create", !s.professionDraftDirty && !s.professionSavePending );

	// ---- Schedules: Excel 97 grid. Headings stay in place; cells in the selected range are highlighted,
	// the active cell keeps a white face with a heavy border, and each cell shows a letter code.
	const auto scope = controller_->scheduleScope();
	const auto inScope = [&]( CreatureId id, std::size_t hour ) { return std::ranges::find( scope.citizens, id ) != scope.citizens.end() && hour >= scope.firstHour && hour <= scope.lastHour; };
	std::string hoursMarkup;
	for ( std::size_t h = 0; h < 24; ++h )
		hoursMarkup += "<button id='schedule_hour_" + std::to_string( h ) + "' class='w98-grid__colhead" + std::string( !scope.empty() && h >= scope.firstHour && h <= scope.lastHour ? " is-highlighted" : "" ) + "' data-hour='" + std::to_string( h ) + "' title='Select hour " + std::to_string( h ) + " for all citizens'>" + std::to_string( h ) + "</button>";
	rml( population_, "schedule_hours", hoursMarkup );
	std::string names, schedules;
	for ( const auto& r : s.schedules )
	{
		const auto id = std::to_string( r.creature.value );
		names += "<button id='schedule_citizen_" + id + "' class='w98-grid__rowhead" + std::string( std::ranges::find( scope.citizens, r.creature ) != scope.citizens.end() ? " is-highlighted" : "" ) + "' data-creature='" + id + "' title='Select this citizen&apos;s day'>" + esc( r.name ) + "</button>";
		schedules += "<div id='population_schedule_" + id + "' class='w98-grid__row' role='row'>";
		for ( std::size_t h = 0; h < r.hours.size(); ++h )
		{
			const bool active   = s.selectedScheduleCell && s.selectedScheduleCell->creature == r.creature && s.selectedScheduleCell->hour == h;
			const bool selected = inScope( r.creature, h );
			const char* code    = r.hours[h] == ManagedScheduleActivity::Eat ? "E" : r.hours[h] == ManagedScheduleActivity::Sleep ? "S" : r.hours[h] == ManagedScheduleActivity::Training ? "T" : "";
			const char* name    = r.hours[h] == ManagedScheduleActivity::Eat ? "eat" : r.hours[h] == ManagedScheduleActivity::Sleep ? "sleep" : r.hours[h] == ManagedScheduleActivity::Training ? "train" : "none";
			schedules += "<button id='schedule_" + id + "_" + std::to_string( h ) + "' class='w98-grid__cell" + ( selected ? " is-selected" : "" ) + ( active ? " is-active" : "" ) + "' role='gridcell' aria-selected='" + ( selected ? std::string( "true" ) : std::string( "false" ) )
				+ "' data-creature='" + id + "' data-hour='" + std::to_string( h ) + "' aria-colindex='" + std::to_string( h + 1 ) + "' aria-label='" + esc( r.name ) + ", hour " + std::to_string( h ) + ", " + name + "'>" + code + "</button>";
		}
		schedules += "</div>";
	}
	rml( population_, "schedule_names", names );
	rml( population_, "schedule_rows", schedules.empty() ? "<div class='w98-list-empty'>" + esc( tr( "management.population.no_schedules" ) ) + "</div>" : schedules );
	if ( auto* cells = population_->GetElementById( "schedule_rows" ) )
	{
		if ( auto* hours = population_->GetElementById( "schedule_hours" ) ) hours->SetScrollLeft( cells->GetScrollLeft() );
		if ( auto* rowheads = population_->GetElementById( "schedule_names" ) ) rowheads->SetScrollTop( cells->GetScrollTop() );
	}
	for ( const auto& [id, activity] : { std::pair { "schedule_set_none", ManagedScheduleActivity::None }, { "schedule_set_eat", ManagedScheduleActivity::Eat }, { "schedule_set_sleep", ManagedScheduleActivity::Sleep }, { "schedule_set_training", ManagedScheduleActivity::Training } } )
		if ( auto* radio = population_->GetElementById( id ) )
		{
			const bool on = s.scheduleActivity == activity;
			if ( radio->HasAttribute( "checked" ) != on ) { if ( on ) radio->SetAttribute( "checked", "checked" ); else radio->RemoveAttribute( "checked" ); }
		}
	text( population_, "schedule_scope_label", scheduleScopeText( s, scope ) );
	enable( "schedule_apply", !scope.empty() );

	// Citizen detail: Properties opens the one creature inspector (Stage 16), so this sheet has no detail page.
	renderingPopulation_ = false;
	}
	if ( inv )
	{
		renderingInventory_ = true;
		const InventoryLabelLookup inventoryLabels( s.inventory );
		if ( auto* f = rmlui_dynamic_cast<Rml::ElementFormControl*>( inventory_->GetElementById( "inventory_search" ) ); f && context_.GetFocusElement() != f && f->GetValue() != s.inventoryFilter )
			f->SetValue( s.inventoryFilter );
		// Options are rebuilt through the select API only when the categories change (SetInnerRML would append).
		std::vector<std::pair<std::string, std::string>> categories { { "", "(All)" } };
		std::string categoryKey;
		for ( const auto& row : s.inventory )
			if ( row.id.depth == InventoryDepth::Category ) { categories.emplace_back( row.id.category.value, row.name ); categoryKey += row.id.category.value + '\x1f' + row.name + '\x1e'; }
		if ( auto* select = rmlui_dynamic_cast<Rml::ElementFormControl*>( inventory_->GetElementById( "inventory_category" ) ) )
		{
			if ( categoryKey != inventoryCategoryOptions_ ) { setSelectOptions( select, categories, false ); inventoryCategoryOptions_ = categoryKey; }
			if ( select->GetValue() != s.inventoryCategory ) select->SetValue( s.inventoryCategory );
		}
		if ( auto* box = inventory_->GetElementById( "inventory_owned_only" ); box && box->HasAttribute( "checked" ) != s.inventoryOwnedOnly )
		{
			if ( s.inventoryOwnedOnly ) box->SetAttribute( "checked", "checked" );
			else box->RemoveAttribute( "checked" );
		}
		// The arrow sits on the sorted heading; down means descending (PDF p.143).
		const auto mark = [this, &s]( const char* id, bool shown ) {
			if ( auto* e = inventory_->GetElementById( id ) ) { e->SetClass( "is-hidden", !shown ); e->SetClass( "is-descending", shown && s.inventorySortDescending ); }
		};
		mark( "inventory_mark_item", s.inventorySort != Sort::Material && s.inventorySort != Sort::Stock && s.inventorySort != Sort::Total );
		mark( "inventory_mark_material", s.inventorySort == Sort::Material );
		mark( "inventory_mark_stock", s.inventorySort == Sort::Stock );
		mark( "inventory_mark_total", s.inventorySort == Sort::Total );

		const auto visibleRows = controller_->inventoryPage();
		std::vector<std::string> markup;
		markup.reserve( visibleRows.size() );
		for ( const auto& r : visibleRows )
		{
			const auto labels = inventoryLabels.labels( r );
			const auto id     = stableId( "inventory_row_", inventoryKey( r.id ) );
			markup.push_back( "<button id='" + id + "' class='w98-list-item' role='row' data-depth='" + depth( r.id.depth ) + "' data-category='" + esc( r.id.category.value ) + "' data-group='" + esc( r.id.group.value )
				+ "' data-item='" + esc( r.id.item.value ) + "' data-material='" + esc( r.id.material.value ) + "'><input id='" + id + "_watch' class='checkbox' type='checkbox'/><span class='w98-cell w98-cell--grow'>"
				+ esc( labels.item ) + "</span><span class='w98-cell l-sp-w-material'>" + esc( labels.material ) + "</span><span class='w98-cell w98-cell--num l-sp-w-number'>" + std::to_string( r.stockpiled )
				+ "</span><span class='w98-cell w98-cell--num l-sp-w-number'>" + std::to_string( r.total ) + "</span></button>" );
		}
		if ( markup.empty() )
			markup.push_back( "<div class='w98-list-empty'>" + esc( s.loadingInventory ? std::string( "Loading items..." ) : s.inventory.empty() ? std::string( "The settlement has no items." ) : std::string( "No items match the text." ) ) + "</div>" );
		// A new Find, Category or owned-only choice starts the list at the top.
		const auto filterKey = s.inventoryFilter + '\x1f' + s.inventoryCategory + '\x1f' + ( s.inventoryOwnedOnly ? "1" : "0" );
		if ( filterKey != inventoryFilterKey_ )
		{
			inventoryFilterKey_ = filterKey;
			if ( auto* rows = inventory_->GetElementById( "inventory_rows" ) ) rows->SetScrollTop( 0.f );
		}
		inventoryRows_ = visibleRows;
		inventoryRowIds_.clear();
		for ( const auto& r : visibleRows ) inventoryRowIds_.push_back( r.id );
		if ( markup != inventoryRowMarkup_ )
		{
			inventoryRowMarkup_ = std::move( markup );
			inventoryViewportFirst_ = static_cast<std::size_t>( -1 );
		}
		renderInventoryViewport();
		syncInventoryRows();
		std::size_t leaves = 0;
		{
			std::unordered_set<std::string> parents;
			for ( const auto& row : s.inventory )
				if ( row.id.depth != InventoryDepth::Category ) parents.insert( inventoryParentKey( row.id ) );
			for ( const auto& row : s.inventory )
				if ( !parents.contains( inventoryKey( row.id ) ) ) ++leaves;
		}
		text( inventory_, "inventory_matches", s.loadingInventory ? std::string( "Loading items..." )
			: visibleRows.size() == leaves ? std::to_string( leaves ) + ( leaves == 1 ? " item" : " items" ) : std::to_string( visibleRows.size() ) + " of " + std::to_string( leaves ) + " items" );
		const bool canOpen = s.selectedInventory && std::ranges::any_of( visibleRows, [&]( const auto& r ) { return r.id == *s.selectedInventory; } );
		if ( auto* button = inventory_->GetElementById( "inventory_open_item" ) )
		{
			if ( canOpen ) button->RemoveAttribute( "disabled" );
			else button->SetAttribute( "disabled", "" );
		}

		// ---- Item Properties
		const auto detail = s.inventoryDetail ? std::ranges::find_if( s.inventory, [&]( const InventoryRow& row ) { return row.id == *s.inventoryDetail; } ) : s.inventory.end();
		const bool showingDetail = detail != s.inventory.end();
		visible( inventory_, "inventory_detail", showingDetail );
		if ( showingDetail )
		{
			if ( !renderedInventoryDetail_ || *renderedInventoryDetail_ != detail->id )
			{
				renderedInventoryDetail_ = detail->id;
				inventoryDetailStockpile_.reset();
				inventoryDetailProduct_.clear();
			}
			std::string name = detail->name;
			if ( detail->id.depth == InventoryDepth::Material )
			{
				const auto parent = std::ranges::find_if( s.inventory, [&]( const InventoryRow& row ) { return row.id.depth == InventoryDepth::Item && row.id.item == detail->id.item; } );
				if ( parent != s.inventory.end() ) name += " " + parent->name;
			}
			if ( !name.empty() ) name[0] = static_cast<char>( std::toupper( static_cast<unsigned char>( name[0] ) ) );
			text( inventory_, "inventory_detail_title", name + " Properties" );
			text( inventory_, "inventory_detail_name", name );
			text( inventory_, "inventory_detail_total", std::to_string( detail->total ) );
			text( inventory_, "inventory_detail_stock", std::to_string( detail->stockpiled ) );
			text( inventory_, "inventory_detail_loose", std::to_string( detail->loose ) );
			text( inventory_, "inventory_detail_equipped", std::to_string( detail->equipped ) );
			text( inventory_, "inventory_detail_jobs", std::to_string( detail->inJobs ) );
			if ( auto* box = inventory_->GetElementById( "inventory_detail_watch" ); box && box->HasAttribute( "checked" ) != detail->watched )
			{
				if ( detail->watched ) box->SetAttribute( "checked", "checked" );
				else box->RemoveAttribute( "checked" );
			}
			const char* tabs[] = { "inventory_detail_tab_general", "inventory_detail_tab_stockpiles", "inventory_detail_tab_recipes", "inventory_detail_tab_history" };
			const char* pages[] = { "inventory_detail_general", "inventory_detail_stockpiles", "inventory_detail_recipes", "inventory_detail_history_page" };
			connected_tabs::select( *inventory_->GetElementById( "inventory_detail_tabs" ), inventory_->GetElementById( tabs[inventoryDetailPane_] ) );
			for ( int page = 0; page < 4; ++page ) visible( inventory_, pages[page], page == inventoryDetailPane_ );
			const auto detailRml = [this]( const char* id, const std::string& value ) {
				auto& previous = renderedInventoryDetailMarkup_[id];
				if ( previous == value ) return;
				rml( inventory_, id, value );
				previous = value;
			};
			std::string locations;
			for ( const auto& location : detail->locations )
				locations += "<button id='inventory_stockpile_" + std::to_string( location.id ) + "' class='w98-list-item' data-stockpile-link='" + std::to_string( location.id ) + "'><span class='w98-cell w98-cell--grow'>"
					+ esc( location.name ) + "</span><span class='w98-cell w98-cell--num l-sp-w-number'>" + std::to_string( location.count ) + "</span></button>";
			detailRml( "inventory_detail_locations", locations.empty() ? "<div class='w98-list-empty'>No stockpile holds this item.</div>" : locations );
			if ( inventoryDetailStockpile_ && std::ranges::none_of( detail->locations, [&]( const auto& l ) { return l.id == *inventoryDetailStockpile_; } ) ) inventoryDetailStockpile_.reset();
			for ( const auto& location : detail->locations )
				if ( auto* row = inventory_->GetElementById( "inventory_stockpile_" + std::to_string( location.id ) ) ) row->SetClass( "is-selected", inventoryDetailStockpile_ == location.id );
			if ( auto* button = inventory_->GetElementById( "inventory_detail_open_stockpile" ) )
			{
				if ( inventoryDetailStockpile_ && stockpileOpenHandler_ ) button->RemoveAttribute( "disabled" );
				else button->SetAttribute( "disabled", "" );
			}
			std::string madeBy;
			for ( const auto& recipe : detail->madeBy )
			{
				std::string line = ( recipe.workshop.empty() ? std::string( "Made" ) : recipe.workshop ) + ": " + std::to_string( recipe.amount ) + " from ";
				for ( std::size_t index = 0; index < recipe.ingredients.size(); ++index )
				{
					const auto& ingredient = recipe.ingredients[index];
					line += ( index ? ", " : "" ) + std::to_string( ingredient.amount ) + " " + ingredient.name;
					if ( !ingredient.allowedMaterial.empty() ) line += " (" + ingredient.allowedMaterial + ")";
					else if ( !ingredient.allowedMaterialType.empty() ) line += " (" + ingredient.allowedMaterialType + ")";
				}
				if ( recipe.ingredients.empty() ) line += "nothing";
				if ( !recipe.skill.empty() ) line += " - " + recipe.skill;
				madeBy += "<div class='w98-list-item' title='" + esc( line ) + "'><span class='w98-cell w98-cell--grow'>" + esc( line ) + "</span></div>";
			}
			detailRml( "inventory_detail_made_by", madeBy.empty() ? "<div class='w98-list-empty'>No workshop makes this item.</div>" : madeBy );
			std::unordered_map<std::string, const InventoryRow*> catalogItems;
			for ( const auto& row : s.inventory )
				if ( row.id.depth == InventoryDepth::Item ) catalogItems.try_emplace( row.id.item.value, &row );
			std::vector<std::pair<std::string, std::string>> products;
			std::unordered_set<std::string> seenProducts;
			for ( const auto& recipe : detail->usedIn )
			{
				if ( !seenProducts.insert( recipe.outputItemID ).second ) continue;
				const auto product = catalogItems.find( recipe.outputItemID );
				if ( product != catalogItems.end() ) products.emplace_back( recipe.outputItemID, product->second->name );
			}
			std::ranges::sort( products, {}, []( const auto& product ) { return foldedLabel( product.second ); } );
			std::string usedIn;
			for ( const auto& [id, productName] : products )
				usedIn += "<button id='" + stableId( "inventory_product_", id ) + "' class='w98-list-item' data-item-link='" + esc( id ) + "'><span class='w98-cell w98-cell--grow'>" + esc( productName ) + "</span></button>";
			detailRml( "inventory_detail_used_in", usedIn.empty() ? "<div class='w98-list-empty'>Nothing is made from this item.</div>" : usedIn );
			if ( !inventoryDetailProduct_.empty() && std::ranges::none_of( products, [&]( const auto& p ) { return p.first == inventoryDetailProduct_; } ) ) inventoryDetailProduct_.clear();
			for ( const auto& [id, productName] : products )
				if ( auto* row = inventory_->GetElementById( stableId( "inventory_product_", id ) ) ) row->SetClass( "is-selected", id == inventoryDetailProduct_ );
			if ( auto* button = inventory_->GetElementById( "inventory_detail_open_product" ) )
			{
				if ( !inventoryDetailProduct_.empty() ) button->RemoveAttribute( "disabled" );
				else button->SetAttribute( "disabled", "" );
			}
			std::string history;
			if ( s.inventoryHistoryLoading ) history = "<div class='w98-list-empty'>Loading history...</div>";
			else if ( s.inventoryHistory.empty() ) history = "<div class='w98-list-empty'>No history has been recorded yet.</div>";
			else
				for ( const auto& point : s.inventoryHistory )
					history += "<div class='w98-list-item'><span class='w98-cell w98-cell--grow'>Day " + std::to_string( point.dayIndex + 1 ) + "</span><span class='w98-cell w98-cell--num l-sp-w-number'>"
						+ std::to_string( point.total ) + "</span><span class='w98-cell w98-cell--num l-sp-w-number'>" + std::to_string( point.created ) + "</span><span class='w98-cell w98-cell--num l-sp-w-number'>"
						+ std::to_string( point.destroyed ) + "</span></div>";
			detailRml( "inventory_detail_history", history );
		}
		else renderedInventoryDetail_.reset();
		renderingInventory_ = false;
	}
	const auto status = s.pendingAction ? tr( "status.updating" ) : (s.status=="management.inventory.target_removed"?tr("management.inventory.target_removed"):commandFeedbackText(textCatalog_,s.status));
	if ( pop ) text( population_, "population_status", status );
}
void Management6BRmlBinding::renderInventoryViewport()
{
	if ( renderingInventoryViewport_ || !inventory_ ) return;
	auto* rows = inventory_->GetElementById( "inventory_rows" );
	if ( !rows ) return;
	// A row is 16 px of the snapped 11 px system font (w98-list-item), so its height follows the font at every scale.
	const float rowHeight = std::max( 1.f, std::round( rows->GetComputedValues().font_size() * 16.f / 11.f ) );
	if ( rowHeight != inventoryRowHeight_ ) { inventoryRowHeight_ = rowHeight; inventoryViewportFirst_ = static_cast<std::size_t>( -1 ); }
	constexpr std::size_t overscan = 8;
	const float scrollTop = std::clamp( rows->GetScrollTop(), 0.f, std::max( 0.f, static_cast<float>( inventoryRowMarkup_.size() ) * rowHeight - rows->GetClientHeight() ) );
	const std::size_t anchor = static_cast<std::size_t>( scrollTop / rowHeight );
	const std::size_t first = std::min( anchor > overscan ? anchor - overscan : 0, inventoryRowMarkup_.size() );
	if ( first == inventoryViewportFirst_ ) return;
	const std::size_t viewportRows = static_cast<std::size_t>( std::max( rows->GetClientHeight(), 240.f ) / rowHeight ) + overscan * 3;
	const std::size_t last = std::min( first + viewportRows, inventoryRowMarkup_.size() );
	const auto spacer = [&]( std::size_t count ) { return "<div class='l-sp-spacer' style='height:" + std::to_string( static_cast<float>( count ) * rowHeight ) + "px;'></div>"; };
	std::string markup;
	if ( first ) markup += spacer( first );
	for ( std::size_t index = first; index < last; ++index ) markup += inventoryRowMarkup_[index];
	if ( last < inventoryRowMarkup_.size() ) markup += spacer( inventoryRowMarkup_.size() - last );
	renderingInventoryViewport_ = true;
	rows->SetInnerRML( markup );
	rows->SetScrollTop( scrollTop );
	inventoryViewportFirst_ = first;
	syncInventoryRows();
	renderingInventoryViewport_ = false;
}
void Management6BRmlBinding::syncInventoryRows()
{
	if ( !inventory_ || !controller_ ) return;
	const auto& selected = controller_->state().selectedInventory;
	for ( const auto& r : inventoryRows_ )
	{
		const auto id = stableId( "inventory_row_", inventoryKey( r.id ) );
		auto* row = inventory_->GetElementById( id );
		if ( !row ) continue;
		row->SetClass( "is-selected", selected && *selected == r.id );
		if ( auto* box = inventory_->GetElementById( id + "_watch" ); box && box->HasAttribute( "checked" ) != r.watched )
		{
			const bool previous = renderingInventoryViewport_;
			renderingInventoryViewport_ = true;
			if ( r.watched ) box->SetAttribute( "checked", "checked" );
			else box->RemoveAttribute( "checked" );
			renderingInventoryViewport_ = previous;
		}
	}
}
} // namespace ingnomia::ui::management6b
