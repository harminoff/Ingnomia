/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 12: population roster, skills and profession editor as a Windows 98 property sheet.
#include "../ui-common/SheetFit.h"
#include "gui/ui/runtime/RmlUiQtInputAdapter.h"
#include "gui/ui/runtime/ConnectedTabs.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/screens/shell/ShellRmlBinding.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include "gui/ui/screens/management6b/Management6BRmlBinding.h"
using namespace ingnomia::ui;
int checks = 0;
void check( bool value, const char* label )
{
	++checks;
	if ( !value )
	{
		std::cerr << "FAIL " << label << std::endl;
		std::exit( 1 );
	}
}
struct Files : Rml::FileInterface
{
	std::filesystem::path root;
	Rml::FileHandle Open( const Rml::String& name ) override
	{
		auto path = std::filesystem::path( name );
		if ( !path.has_root_name() ) path = root / path.relative_path();
		return reinterpret_cast<Rml::FileHandle>( std::fopen( path.string().c_str(), "rb" ) );
	}
	void Close( Rml::FileHandle f ) override { std::fclose( reinterpret_cast<FILE*>( f ) ); }
	size_t Read( void* p, size_t size, Rml::FileHandle f ) override { return std::fread( p, 1, size, reinterpret_cast<FILE*>( f ) ); }
	bool Seek( Rml::FileHandle f, long n, int from ) override { return std::fseek( reinterpret_cast<FILE*>( f ), n, from ) == 0; }
	size_t Tell( Rml::FileHandle f ) override { return static_cast<size_t>( std::ftell( reinterpret_cast<FILE*>( f ) ) ); }
};
struct Renderer : Rml::RenderInterface
{
	Rml::CompiledGeometryHandle CompileGeometry( Rml::Span<const Rml::Vertex>, Rml::Span<const int> ) override { return 1; }
	void RenderGeometry( Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle ) override {}
	void ReleaseGeometry( Rml::CompiledGeometryHandle ) override {}
	Rml::TextureHandle LoadTexture( Rml::Vector2i&, const Rml::String& ) override { return 0; }
	Rml::TextureHandle GenerateTexture( Rml::Span<const Rml::byte>, Rml::Vector2i ) override { return 1; }
	void ReleaseTexture( Rml::TextureHandle ) override {}
	void EnableScissorRegion( bool ) override {}
	void SetScissorRegion( Rml::Rectanglei ) override {}
};
void update( Rml::Context& c )
{
	c.Update();
	if ( connected_tabs::reconcile( c ) ) c.Update();
}
struct Port : management6b::CommandPort
{
	std::vector<UiActionEnvelope> sent;
	bool pending = false, reject = false;
	management6b::CommandResult dispatch( const UiActionEnvelope& a ) override
	{
		sent.push_back( a );
		return { reject ? management6b::CommandStatus::Rejected : management6b::CommandStatus::Accepted, pending, "Rejected by test" };
	}
	management6b::CommandResult dispatchConfirmed( const UiActionEnvelope& a ) override { return dispatch( a ); }
	int count( const char* id ) { return static_cast<int>( std::count_if( sent.begin(), sent.end(), [&]( auto& a ) { return a.id.value == id; } ) ); }
};

using namespace management6b;

int main( int argc, char** argv )
{
	check( argc == 2, "assets" );
	Files files;
	files.root = argv[1];
	Renderer renderer;
	Rml::SystemInterface system;
	Rml::SetFileInterface( &files );
	Rml::SetRenderInterface( &renderer );
	Rml::SetSystemInterface( &system );
	check( Rml::Initialise(), "initialize" );
	ClassicFocusInstancer focus;
	Rml::Factory::RegisterDecoratorInstancer( "win98-focus", &focus );
	Rml::LoadFontFace( "fonts/LatoLatin-Regular.ttf" );
	Rml::LoadFontFace( "fonts/MSW98UI-Regular.ttf" );
	Rml::LoadFontFace( "fonts/MSW98UI-Bold.ttf" );
	auto* c = Rml::CreateContext( "stage12", { 384, 380 } );
	{
		Port port;
		Management6BRmlBinding binding( *c );
		Management6BController controller( port, binding );
		check( binding.initialize( controller ), "binding loads" );
		bool closed = false;
		binding.setRouteCloseHandler( [&]( RouteId, FocusToken ) { closed = true; } );
		controller.beginWorld( WorldEpoch { 12 } );
		check( binding.openPopulation( FocusToken { 1 } ), "population opens" );
		auto* doc   = binding.populationDocument();
		auto el     = [&]( const char* id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id ); return e; };
		auto click  = [&]( const char* id ) { el( id )->DispatchEvent( "click", Rml::Dictionary {} ); update( *c ); };
		auto off    = [&]( const char* id ) { return el( id )->HasAttribute( "disabled" ); };
		auto inner  = [&]( const char* id ) { return std::string( el( id )->GetInnerRML() ); };
		auto dialog = [&]( const char* id ) -> Rml::Element* {
			for ( int i = 0; i < c->GetNumDocuments(); ++i )
				if ( auto* e = c->GetDocument( i )->GetElementById( id ); e && c->GetDocument( i ) != doc && c->GetDocument( i )->IsVisible() ) return e;
			return nullptr;
		};
		auto row = [&]( const char* list, const char* skill ) {
			auto* l = el( list );
			for ( int i = 0; i < l->GetNumChildren(); ++i )
				if ( l->GetChild( i )->GetAttribute<Rml::String>( "data-skill", "" ) == skill ) { l->GetChild( i )->DispatchEvent( "click", Rml::Dictionary {} ); update( *c ); return; }
			check( false, skill );
		};
		auto answer = [&]( const char* id ) { auto* e = dialog( id ); check( e != nullptr, id ); e->Click(); update( *c ); };
		auto value  = [&]( const char* id, const char* v ) { auto* e = rmlui_dynamic_cast<Rml::ElementFormControl*>( el( id ) ); e->SetValue( v ); e->DispatchEvent( "change", Rml::Dictionary {} ); update( *c ); };

		// ---------------------------------------------------------------- Data
		const auto skill = []( const char* id, bool active, int level ) { return SkillRow { CatalogId { id }, id, "Group", level, 0.f, active }; };
		PopulationRow ada { CreatureId { 7 }, "Ada", ProfessionId { "Miner" }, { skill( "Mining", true, 3 ), skill( "Farming", false, 1 ) } };
		PopulationRow bert { CreatureId { 9 }, "Bert", ProfessionId { "Farmer" }, { skill( "Mining", false, 0 ), skill( "Farming", true, 4 ) } };
		PopulationRow cara { CreatureId { 11 }, "Cara", ProfessionId { "Gnomad" }, { skill( "Mining", true, 2 ), skill( "Farming", true, 2 ) } };
		check( controller.applyPopulation( { WorldEpoch { 12 }, Revision { 1 }, { bert, cara, ada } } ), "population snapshot" );
		controller.applySkillCatalog( WorldEpoch { 12 }, { { CatalogId { "Mining" }, "Mining", "Mining" }, { CatalogId { "Farming" }, "Farming", "Agriculture" }, { CatalogId { "Cooking" }, "Cooking", "Food" } } );
		const ProfessionRow miner { ProfessionId { "Miner" }, "Miner", { CatalogId { "Mining" } } }, farmer { ProfessionId { "Farmer" }, "Farmer", { CatalogId { "Farming" } } }, gnomad { ProfessionId { "Gnomad" }, "Gnomad", {} };
		controller.applyProfessions( { WorldEpoch { 12 }, Revision { 1 }, { gnomad, miner, farmer } } );
		update( *c );

		// ---------------------------------------------------------------- Sheet
		check( el( "population_tabs" )->IsClassSet( "c-connected-tabs" ) && el( "population_workbench" )->IsClassSet( "w98-sheet" ), "connected tabs on a property sheet" );
		check( doc->GetElementById( "population_views_toggle" ) == nullptr && doc->GetElementById( "population_rail" ) == nullptr, "no side rail or views toggle" );
		check( doc->GetElementById( "population_page_previous" ) == nullptr && doc->GetElementById( "population_refresh_view" ) == nullptr, "one Refresh and no paging buttons" );
		check( el( "population_close_button" )->IsVisible( true ) && doc->GetElementById( "population_ok" ) == nullptr, "changes apply at once: Close is the only commit button" );

		// ---------------------------------------------------------------- Citizens
		const auto order = [&] {
			std::string out;
			auto* list = el( "population_rows" );
			for ( int i = 0; i < list->GetNumChildren(); ++i ) out += list->GetChild( i )->GetAttribute<Rml::String>( "data-creature", "" ) + ",";
			return out;
		};
		check( order() == "7,9,11,", "roster sorted by name" );
		check( !el( "population_skills" )->IsVisible( true ), "only the chosen page is shown" );
		check( inner( "population_sort_name" ).find( "Name" ) != std::string::npos && inner( "population_sort_name" ) != "Name", "sorted heading shows its direction" );
		click( "population_creature_9" );
		check( controller.state().selectedCreature == CreatureId { 9 } && controller.state().populationView == View::Citizens, "click selects a citizen without opening it" );
		click( "population_sort_name" );
		check( order() == "11,9,7," && controller.state().populationSortDescending, "clicking the sorted heading reverses the order" );
		check( controller.state().selectedCreature == CreatureId { 9 } && el( "population_creature_9" )->IsClassSet( "is-selected" ), "selection kept by ID across sorting" );
		click( "population_sort_profession" );
		check( order() == "9,11,7," && !controller.state().populationSortDescending, "Profession heading sorts ascending first" );
		value( "population_search", "ada" );
		controller.setPopulationFilter( "ada" );
		update( *c );
		check( order() == "7," && inner( "population_page_status" ) == "1 of 3 citizens", "filter shows matching citizens and the count" );
		check( controller.state().selectedCreature == CreatureId { 7 }, "a filter that hides the selection moves it to the first match" );
		controller.setPopulationFilter( "" );
		update( *c );
		click( "population_creature_9" );
		check( !off( "population_open_citizen" ), "Properties available with a selection" );
		auto bert2 = bert;
		bert2.name = "Bertha";
		check( controller.applyPopulation( { WorldEpoch { 12 }, Revision { 2 }, { bert2, cara, ada } } ), "refresh snapshot" );
		update( *c );
		check( controller.state().selectedCreature == CreatureId { 9 }, "refresh keeps the selected citizen by ID" );
		check( controller.applyPopulation( { WorldEpoch { 12 }, Revision { 3 }, { cara, ada } } ), "citizen removed" );
		update( *c );
		check( !controller.state().selectedCreature && off( "population_open_citizen" ), "a removed citizen is not replaced by another" );
		check( controller.applyPopulation( { WorldEpoch { 12 }, Revision { 4 }, { bert2, cara, ada } } ), "citizen back" );
		update( *c );
		click( "population_creature_9" );
		click( "population_open_citizen" );
		check( controller.state().populationView == View::Creature && controller.state().selectedCreature == CreatureId { 9 }, "Properties opens the selected citizen" );
		// Stage 16 (POP-05): the citizen opens in the one creature inspector; the roster stays on screen.
		check( port.count( "inspect.select" ) >= 1 && el( "population_citizens" )->IsVisible( true ), "Properties asks for the creature inspector and keeps the roster" );
		controller.open( View::Citizens );
		update( *c );
		const auto refreshes = port.count( "population.refresh" );
		click( "population_refresh" );
		check( port.count( "population.refresh" ) == refreshes + 1, "one Refresh command" );

		// ---------------------------------------------------------------- Skills
		controller.open( View::Skills );
		update( *c );
		check( el( "population_skills" )->IsVisible( true ) && el( "population_tab_skills" )->IsClassSet( "is-selected" ), "Skills page" );
		check( controller.state().selectedSkill == CatalogId { "Mining" } && !off( "skill_enable_all" ), "the first skill starts selected, as in a list box" );
		row( "skills_catalog_rows", "Mining" );
		check( controller.state().selectedSkill == CatalogId { "Mining" }, "skill chosen" );
		check( inner( "skill_focus_group" ).find( "for all 3 citizens" ) != std::string::npos, "bulk scope names the skill and citizen count" );
		auto* bertBox = el( "skill_citizen_check_9" );
		check( !bertBox->HasAttribute( "checked" ) && el( "skill_citizen_check_7" )->HasAttribute( "checked" ), "check boxes show each citizen's state" );
		click( "skill_citizen_check_9" );
		{
			const auto p = std::get<SetSkillPayload>( port.sent.back().payload );
			check( port.sent.back().id.value == "population.set_skill" && p.creature == CreatureId { 9 } && p.skill == CatalogId { "Mining" } && p.active, "checking a citizen enables only that citizen's skill" );
		}
		const auto bulk = port.count( "population.set_skill_for_all" );
		click( "skill_disable_all" );
		check( dialog( "confirm-accept" ) != nullptr && port.count( "population.set_skill_for_all" ) == bulk, "bulk change asks first" );
		answer( "confirm-cancel" );
		check( port.count( "population.set_skill_for_all" ) == bulk, "Cancel changes nothing" );
		click( "skill_disable_all" );
		answer( "confirm-accept" );
		{
			const auto p = std::get<SetSkillForAllPayload>( port.sent.back().payload );
			check( port.count( "population.set_skill_for_all" ) == bulk + 1 && p.skill == CatalogId { "Mining" } && !p.active, "confirmed bulk change targets the reviewed skill" );
		}

		// ---------------------------------------------------------------- Professions
		controller.open( View::Professions );
		update( *c );
		check( el( "profession_rows" )->GetTagName() == "select", "profession chosen from a drop-down list" );
		value( "profession_rows", "Miner" );
		check( controller.state().selectedProfession == ProfessionId { "Miner" }, "drop-down selects the profession" );
		check( off( "profession_skill_add" ) && off( "profession_skill_remove" ) && off( "profession_save" ), "transfer and save need a selection or change" );
		row( "profession_available_skills", "Farming" );
		check( !off( "profession_skill_add" ), "Add available for an available skill" );
		click( "profession_skill_add" );
		check( controller.state().professionDraftSkills.size() == 2 && controller.state().professionDraftDirty && !off( "profession_save" ), "Add stages the skill" );
		row( "profession_selected_skills", "Farming" );
		check( !off( "profession_skill_up" ) && off( "profession_skill_down" ), "Move Up/Down follow the position" );
		click( "profession_skill_up" );
		check( controller.state().professionDraftSkills.front() == CatalogId { "Farming" }, "Move Up reorders" );
		check( inner( "profession_feedback" ).find( "unsaved" ) != std::string::npos, "unsaved changes are stated" );
		// Switching with unsaved changes asks; Cancel keeps the draft and the drop-down.
		value( "profession_rows", "Farmer" );
		check( dialog( "confirm-accept" ) != nullptr && controller.state().selectedProfession == ProfessionId { "Miner" }, "switching with unsaved changes asks" );
		answer( "confirm-cancel" );
		check( controller.state().professionDraftDirty && rmlui_dynamic_cast<Rml::ElementFormControl*>( el( "profession_rows" ) )->GetValue() == "Miner", "keeping the draft keeps the profession" );
		const auto updates = port.count( "profession.update" );
		click( "profession_save" );
		{
			const auto p = std::get<UpdateProfessionPayload>( port.sent.back().payload );
			check( port.count( "profession.update" ) == updates + 1 && p.current == ProfessionId { "Miner" } && p.skills.size() == 2 && p.skills.front() == CatalogId { "Farming" }, "Save sends the ordered skills for the selected profession" );
		}
		controller.applyProfessions( { WorldEpoch { 12 }, Revision { 2 }, { gnomad, ProfessionRow { ProfessionId { "Miner" }, "Miner", { CatalogId { "Farming" }, CatalogId { "Mining" } } }, farmer } } );
		controller.applyProfessionSkills( WorldEpoch { 12 }, ProfessionId { "Miner" }, { CatalogId { "Farming" }, CatalogId { "Mining" } } );
		update( *c );
		check( !controller.state().professionDraftDirty, "authoritative skills confirm the save" );
		// Delete is separate and confirms.
		const auto deletes = port.count( "profession.delete" );
		click( "profession_delete" );
		check( dialog( "confirm-accept" ) != nullptr, "Delete asks first" );
		answer( "confirm-cancel" );
		check( port.count( "profession.delete" ) == deletes, "Cancel keeps the profession" );
		// New profession names like a new folder.
		const auto creates = port.count( "profession.create" );
		click( "profession_create" );
		check( port.count( "profession.create" ) == creates + 1 && std::get<CreateProfessionPayload>( port.sent.back().payload ).name == "New Profession", "New creates New Profession" );
		controller.applyProfessions( { WorldEpoch { 12 }, Revision { 3 }, { gnomad, miner, farmer, ProfessionRow { ProfessionId { "New Profession" }, "New Profession", {} } } } );
		update( *c );
		click( "profession_create" );
		check( std::get<CreateProfessionPayload>( port.sent.back().payload ).name == "New Profession 2", "a second New is numbered" );
		value( "profession_rows", "Gnomad" );
		check( off( "profession_delete" ) && off( "profession_name" ), "Gnomad is protected" );

		// ---------------------------------------------------------------- Schedules
		ScheduleRow sa { CreatureId { 7 }, "Ada", {} }, sb { CreatureId { 9 }, "Bertha", {} };
		sa.hours[3] = ManagedScheduleActivity::Sleep;
		controller.applySchedules( { WorldEpoch { 12 }, Revision { 1 }, { sa, sb } } );
		controller.open( View::Schedules );
		update( *c );
		check( el( "schedule_7_3" )->GetInnerRML() == "S" && el( "schedule_7_4" )->GetInnerRML().empty(), "cells show letter codes, not colour alone" );
		el( "schedule_set_eat" )->SetAttribute( "checked", "checked" );
		el( "schedule_set_eat" )->DispatchEvent( "change", Rml::Dictionary {} );
		update( *c );
		check( controller.state().scheduleActivity == ManagedScheduleActivity::Eat, "option button chooses the activity" );
		const auto cells = port.count( "population.set_schedule_cell" );
		check( port.count( "population.set_schedule_cell" ) == cells && off( "schedule_apply" ), "choosing an activity changes no cell" );
		click( "schedule_9_5" );
		click( "schedule_apply" );
		{
			const auto p = std::get<SetScheduleCellPayload>( port.sent.back().payload );
			check( p.cell.creature == CreatureId { 9 } && p.cell.hour == 5 && p.activity == ScheduleActivity::Eat, "Set Cell targets the chosen citizen and hour" );
		}

		// ---------------------------------------------------------------- Fit and close
		// A full skill catalog, as in the game, so long lists must scroll inside their field instead of growing the page.
		{
			std::vector<SkillCatalogRow> skills;
			for ( int i = 0; i < 24; ++i ) skills.push_back( { CatalogId { "Skill" + std::to_string( i ) }, "Skill " + std::to_string( i ), "Group" } );
			controller.applySkillCatalog( WorldEpoch { 12 }, skills );
		}
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			const int k = std::max( 1, int( scale + 0.5f ) );
			c->SetDimensions( { 384 * k, 380 * k } );
			for ( auto view : { View::Citizens, View::Skills, View::Professions, View::Schedules } )
			{
				controller.open( view );
				update( *c );
				const char* ids[] = { "population_citizens", "population_skills", "population_professions", "population_schedules" };
				auto* page  = el( ids[static_cast<int>( view )] );
				auto* close = el( "population_close_button" );
				if ( page->GetScrollHeight() > page->GetClientHeight() + 1.f || close->GetAbsoluteTop() + close->GetOffsetHeight() > 380.f * k + 1.f || el( "population_tabs" )->GetOffsetWidth() > 384.f * k )
				{
					std::cerr << "clipped view=" << int( view ) << " scale=" << scale << " page=" << page->GetScrollHeight() << "/" << page->GetClientHeight() << " close=" << close->GetAbsoluteTop() + close->GetOffsetHeight() << std::endl;
					check( false, "page, tabs and Close stay inside the sheet" );
				}
				++checks;
				const auto overflow = ingnomia::ui::test::pageOverflow( el( "population_scroll" ) );
				if ( !overflow.empty() ) std::cerr << "view=" << int( view ) << " scale=" << scale << overflow << std::endl;
				check( overflow.empty(), "every control of the page lies inside the page frame" );
			}
		}
		c->SetDensityIndependentPixelRatio( 1.f );
		c->SetDimensions( { 384, 380 } );
		controller.open( View::Citizens );
		update( *c );
		click( "population_close_button" );
		check( closed && !controller.state().populationOpen, "Close closes the window" );
		binding.shutdown();
	}
	Rml::RemoveContext( "stage12" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
