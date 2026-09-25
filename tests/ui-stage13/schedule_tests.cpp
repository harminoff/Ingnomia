/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 13: the 24-hour schedule grid (Excel 97 model) with an exact scope before every change.
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
#include <set>
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
	auto* c = Rml::CreateContext( "stage13", { 384, 380 } );
	{
		Port port;
		Management6BRmlBinding binding( *c );
		Management6BController controller( port, binding );
		check( binding.initialize( controller ), "binding loads" );
		controller.beginWorld( WorldEpoch { 13 } );
		check( binding.openPopulation( FocusToken { 1 } ), "population opens" );
		auto* doc   = binding.populationDocument();
		auto el     = [&]( const std::string& id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id.c_str() ); return e; };
		auto click  = [&]( const std::string& id, const char* modifier = nullptr ) { Rml::Dictionary p; if ( modifier ) p[modifier] = 1; el( id )->DispatchEvent( "click", p ); update( *c ); };
		auto key    = [&]( int k, const char* modifier = nullptr ) { Rml::Dictionary p; p["key_identifier"] = k; if ( modifier ) p[modifier] = 1; el( "schedule_rows" )->DispatchEvent( "keydown", p ); update( *c ); };
		auto off    = [&]( const char* id ) { return el( id )->HasAttribute( "disabled" ); };
		auto inner  = [&]( const std::string& id ) { return std::string( el( id )->GetInnerRML() ); };
		auto dialog = [&]( const char* id ) -> Rml::Element* {
			for ( int i = 0; i < c->GetNumDocuments(); ++i )
				if ( auto* e = c->GetDocument( i )->GetElementById( id ); e && c->GetDocument( i ) != doc && c->GetDocument( i )->IsVisible() ) return e;
			return nullptr;
		};
		auto answer = [&]( const char* id ) { auto* e = dialog( id ); check( e != nullptr, id ); e->Click(); update( *c ); };
		auto since  = [&]( std::size_t n, const char* id ) { std::vector<UiActionEnvelope> out; for ( auto i = n; i < port.sent.size(); ++i ) if ( port.sent[i].id.value == id ) out.push_back( port.sent[i] ); return out; };
		auto active = [&] { const auto& s = controller.state().selectedScheduleCell; return s ? std::to_string( s->creature.value ) + ":" + std::to_string( s->hour ) : std::string( "-" ); };

		std::vector<ScheduleRow> rows;
		for ( std::uint32_t id : { 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u } )
			rows.push_back( ScheduleRow { CreatureId { id }, "Gnome " + std::to_string( id ), {} } );
		rows[1].hours[3] = ManagedScheduleActivity::Sleep;
		rows[1].hours[4] = ManagedScheduleActivity::Eat;
		controller.applySchedules( { WorldEpoch { 13 }, Revision { 1 }, rows } );
		controller.open( View::Schedules );
		update( *c );

		// ---------------------------------------------------------------- Structure
		check( el( "schedule_hours" )->GetNumChildren() == 24 && el( "schedule_names" )->GetNumChildren() == 12 && el( "schedule_select_all" )->GetTagName() == "button", "24 hour headings, a heading per citizen and a Select All corner" );
		check( inner( "schedule_22_3" ) == "S" && inner( "schedule_22_4" ) == "E" && inner( "schedule_22_5" ).empty(), "cells show letter codes" );
		check( el( "schedule_22_3" )->GetAttribute<Rml::String>( "aria-label", "" ).find( "Gnome 22, hour 3, sleep" ) != Rml::String::npos, "cells carry citizen, hour and activity text" );
		check( off( "schedule_apply" ), "Set Activity needs a selection" );
		auto n = port.sent.size();
		el( "schedule_set_eat" )->SetAttribute( "checked", "checked" );
		el( "schedule_set_eat" )->DispatchEvent( "change", Rml::Dictionary {} );
		update( *c );
		check( controller.state().scheduleActivity == ManagedScheduleActivity::Eat && port.sent.size() == n, "choosing an activity changes nothing" );

		// ---------------------------------------------------------------- Cell
		click( "schedule_22_5" );
		check( inner( "schedule_scope_label" ) == "Set Activity sets hour 5 for Gnome 22 to Eat.", "one cell scope is stated" );
		check( el( "schedule_22_5" )->IsClassSet( "is-active" ) && el( "schedule_22_5" )->IsClassSet( "is-selected" ), "active cell marked" );
		n = port.sent.size();
		click( "schedule_apply" );
		{
			const auto sent = since( n, "population.set_schedule_cell" );
			check( sent.size() == 1 && port.sent.size() == n + 1 && std::get<SetScheduleCellPayload>( sent[0].payload ).cell.creature == CreatureId { 22 } && std::get<SetScheduleCellPayload>( sent[0].payload ).cell.hour == 5 && std::get<SetScheduleCellPayload>( sent[0].payload ).activity == ScheduleActivity::Eat, "one cell, no review" );
		}

		// ---------------------------------------------------------------- Citizen's day (row heading)
		click( "schedule_citizen_24" );
		check( inner( "schedule_scope_label" ) == "Set Activity sets all 24 hours for Gnome 24 to Eat.", "row heading selects the citizen's day" );
		n = port.sent.size();
		click( "schedule_apply" );
		check( since( n, "population.set_schedule_row" ).size() == 1 && std::get<SetScheduleRowPayload>( port.sent.back().payload ).creature == CreatureId { 24 } && port.sent.size() == n + 1, "citizen's day uses one row command" );

		// ---------------------------------------------------------------- Hour for all (column heading)
		click( "schedule_hour_7" );
		check( inner( "schedule_scope_label" ) == "Set Activity sets hour 7 for all 12 citizens to Eat.", "column heading selects the hour for all" );
		n = port.sent.size();
		click( "schedule_apply" );
		check( dialog( "confirm-accept" ) != nullptr && port.sent.size() == n, "several citizens: review first" );
		check( std::string( dialog( "confirm-detail" )->GetInnerRML() ).find( "12 cells, including rows scrolled out of view" ) != std::string::npos, "review counts offscreen cells" );
		answer( "confirm-cancel" );
		check( port.sent.size() == n, "Cancel changes nothing" );
		click( "schedule_apply" );
		answer( "confirm-accept" );
		check( since( n, "population.set_schedule_column" ).size() == 1 && std::get<SetScheduleColumnPayload>( port.sent.back().payload ).hour == 7 && port.sent.size() == n + 1, "hour for all uses one column command" );

		// ---------------------------------------------------------------- Rectangle (Shift+click)
		click( "schedule_23_10" );
		click( "schedule_24_12", "shift_key" );
		check( inner( "schedule_scope_label" ) == "Set Activity sets hours 10-12 for 2 citizens (Gnome 23 to Gnome 24) to Eat.", "Shift+click extends the range" );
		check( el( "schedule_24_11" )->IsClassSet( "is-selected" ) && !el( "schedule_25_11" )->IsClassSet( "is-selected" ), "exactly the range is highlighted" );
		n = port.sent.size();
		click( "schedule_apply" );
		answer( "confirm-accept" );
		{
			const auto sent = since( n, "population.set_schedule_cell" );
			std::set<std::pair<std::uint32_t, int>> cells;
			for ( const auto& a : sent ) { const auto p = std::get<SetScheduleCellPayload>( a.payload ); cells.insert( { p.cell.creature.value, p.cell.hour } ); }
			check( sent.size() == 6 && cells == std::set<std::pair<std::uint32_t, int>> { { 23, 10 }, { 23, 11 }, { 23, 12 }, { 24, 10 }, { 24, 11 }, { 24, 12 } }, "a 2 x 3 range sends exactly those six cells" );
		}

		// ---------------------------------------------------------------- Keyboard
		click( "schedule_21_0" );
		key( Rml::Input::KI_RIGHT );
		key( Rml::Input::KI_DOWN );
		check( active() == "22:1", "arrows move the active cell" );
		key( Rml::Input::KI_END );
		check( active() == "22:23", "End reaches hour 23" );
		{
			auto* cells = el( "schedule_rows" );
			auto* cell  = el( "schedule_22_23" );
			check( cells->GetScrollLeft() > 0 && cell->GetAbsoluteLeft() + cell->GetOffsetWidth() <= cells->GetAbsoluteLeft() + cells->GetClientWidth() + 1.f, "the active cell scrolls into view" );
			check( std::abs( el( "schedule_hours" )->GetScrollLeft() - cells->GetScrollLeft() ) < 1.f, "hour headings stay aligned with the cells" );
			check( el( "schedule_citizen_22" )->IsVisible( true ), "citizen names stay visible while scrolling hours" );
		}
		key( Rml::Input::KI_HOME );
		check( active() == "22:0", "Home returns to hour 0" );
		key( Rml::Input::KI_END, "ctrl_key" );
		check( active() == "32:23", "Ctrl+End reaches the last cell" );
		key( Rml::Input::KI_HOME, "ctrl_key" );
		key( Rml::Input::KI_RIGHT, "shift_key" );
		key( Rml::Input::KI_DOWN, "shift_key" );
		{
			const auto scope = controller.scheduleScope();
			check( scope.citizens.size() == 2 && scope.firstHour == 0 && scope.lastHour == 1, "Shift+arrows extend the range" );
		}
		key( Rml::Input::KI_SPACE, "shift_key" );
		check( controller.scheduleScope().fullDay() && controller.scheduleScope().citizens.size() == 1, "Shift+Space selects the citizen's day" );
		key( Rml::Input::KI_SPACE, "ctrl_key" );
		check( controller.scheduleScope().allCitizens && controller.scheduleScope().firstHour == controller.scheduleScope().lastHour, "Ctrl+Space selects the hour for all" );
		key( Rml::Input::KI_A, "ctrl_key" );
		check( controller.scheduleScope().allCitizens && controller.scheduleScope().fullDay(), "Ctrl+A selects everything" );
		click( "schedule_21_2" );
		n = port.sent.size();
		key( Rml::Input::KI_RETURN );
		check( port.sent.size() == n + 1 && port.sent.back().id.value == "population.set_schedule_cell", "Enter sets the activity for the selection" );
		// Held Enter while a review is open cannot send twice.
		click( "schedule_select_all" );
		n = port.sent.size();
		key( Rml::Input::KI_RETURN );
		key( Rml::Input::KI_RETURN );
		check( dialog( "confirm-accept" ) != nullptr && port.sent.size() == n, "select all asks before changing every citizen" );
		answer( "confirm-cancel" );

		// ---------------------------------------------------------------- Stale review and removed citizens
		click( "schedule_hour_9" );
		click( "schedule_apply" );
		auto fewer = rows;
		fewer.erase( fewer.begin() + 5 );
		controller.applySchedules( { WorldEpoch { 13 }, Revision { 2 }, fewer } );
		update( *c );
		n = port.sent.size();
		answer( "confirm-accept" );
		check( port.sent.size() == n && controller.state().status == "editing.scope_changed", "a review made before citizens changed sends nothing" );
		click( "schedule_24_3" );
		click( "schedule_27_5", "shift_key" );
		auto without = fewer;
		std::erase_if( without, []( const auto& r ) { return r.creature == CreatureId { 24 }; } );
		controller.applySchedules( { WorldEpoch { 13 }, Revision { 3 }, without } );
		update( *c );
		check( controller.scheduleScope().citizens == std::vector<CreatureId> { CreatureId { 27 } } && controller.scheduleScope().firstHour == 5, "a removed range corner collapses to the active cell, never another citizen" );

		// ---------------------------------------------------------------- Fit
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			const int k = std::max( 1, int( scale + 0.5f ) );
			c->SetDimensions( { 384 * k, 380 * k } );
			update( *c );
			auto* page  = el( "population_schedules" );
			auto* close = el( "population_close_button" );
			check( page->GetScrollHeight() <= page->GetClientHeight() + 1.f && close->GetAbsoluteTop() + close->GetOffsetHeight() <= 380.f * k + 1.f, "schedule page and Close stay inside the sheet" );
			check( el( "schedule_rows" )->GetClientHeight() >= 16.f * 4 * ( scale >= 1.5f ? 2 : 1 ), "the grid shows at least four citizens" );
		}
		binding.shutdown();
	}
	Rml::RemoveContext( "stage13" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
