/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 15: diplomacy as a Windows 98 property sheet and the Send Mission wizard.
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
#include "gui/ui/screens/management6c/Management6CRmlBinding.h"
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
using namespace management6c;
struct Port final : CommandPort
{
	std::vector<UiActionEnvelope> sent;
	CommandResult dispatch( const UiActionEnvelope& a, DispatchOrigin ) override { sent.push_back( a ); return {}; }
	int count( const char* id ) { return static_cast<int>( std::count_if( sent.begin(), sent.end(), [&]( auto& a ) { return a.id.value == id; } ) ); }
};
NeighborRow kingdom( std::uint32_t id, const char* name, bool discovered, bool goblin )
{
	NeighborRow row;
	row.id         = NeighborId { id };
	row.discovered = discovered;
	if ( discovered )
	{
		row.name            = name;
		row.distance        = "Two days";
		row.attitude        = "Neutral";
		row.canSendEmissary = true;
		row.canSpy = row.canRaid = goblin;
	}
	return row;
}

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
	auto* c = Rml::CreateContext( "stage15", { 384, 380 } );
	{
		Port port;
		Management6CRmlBinding binding( *c );
		binding.setWindowSurface( true );
		Management6CController controller( port, binding );
		check( binding.initialize( controller ), "binding loads" );
		bool closed = false;
		binding.setRouteCloseHandler( [&]( RouteId, FocusToken ) { closed = true; } );
		controller.beginWorld( WorldEpoch { 15 } );
		check( binding.openDiplomacy( View::Neighbors, FocusToken { 1 } ), "diplomacy opens" );
		auto* doc  = binding.diplomacyDocument();
		auto el    = [&]( const std::string& id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id.c_str() ); return e; };
		auto click = [&]( const std::string& id ) { el( id )->DispatchEvent( "click", Rml::Dictionary {} ); update( *c ); };
		auto off   = [&]( const char* id ) { return el( id )->HasAttribute( "disabled" ); };
		auto inner = [&]( const std::string& id ) { return std::string( el( id )->GetInnerRML() ); };
		auto shown = [&]( const char* id ) { return el( id )->IsVisible( true ); };
		auto key   = [&]( int k ) { Rml::Dictionary p; p["key_identifier"] = k; el( "wizard_next" )->DispatchEvent( "keydown", p ); update( *c ); };

		check( controller.applyNeighbors( { WorldEpoch { 15 }, Revision { 1 }, { kingdom( 1, "Hill Gnomes", true, false ), kingdom( 2, "", false, false ), kingdom( 3, "Red Goblins", true, true ) } } ), "neighbors" );
		update( *c );

		// ---------------------------------------------------------------- Sheet and neighbors
		check( el( "diplomacy_tabs" )->IsClassSet( "c-connected-tabs" ) && el( "diplomacy_workbench" )->IsClassSet( "w98-sheet" ) && shown( "diplomacy_close_button" ), "property sheet with Close" );
		check( !shown( "diplomacy_tab_squads" ) && shown( "diplomacy_tab_neighbors" ) && shown( "diplomacy_tab_missions" ), "only diplomacy tabs" );
		{
			auto* caption = el( "diplomacy_workbench" )->GetFirstChild();
			check( caption->IsVisible( true ) && el( "diplomacy_tabs" )->GetAbsoluteTop() >= caption->GetAbsoluteTop() + caption->GetOffsetHeight(), "caption visible above the tabs" );
		}
		click( "diplomacy_neighbor_2" );
		check( shown( "neighbor_undiscovered_detail" ) && inner( "neighbor_distance" ) == "Unknown" && inner( "neighbor_wealth" ) == "Unknown", "undiscovered facts are Unknown, never zero" );
		check( off( "mission_wizard_open" ), "no mission to an undiscovered kingdom" );
		click( "diplomacy_neighbor_1" );
		check( inner( "neighbor_distance" ) == "Two days" && inner( "neighbor_wealth" ) == "Unknown", "reported facts shown, unreported ones Unknown" );
		check( !off( "mission_wizard_open" ), "Send Mission available for a discovered kingdom" );

		// ---------------------------------------------------------------- Wizard
		auto n = port.sent.size();
		click( "mission_wizard_open" );
		check( shown( "mission_wizard" ) && shown( "wizard_page_mission" ) && off( "wizard_back" ) && shown( "wizard_next" ) && !shown( "mission_start" ), "wizard opens on Mission with Back unavailable and no Finish" );
		check( inner( "mission_wizard_title" ) == "Send Mission to Hill Gnomes" && inner( "wizard_subtitle" ).find( "Which mission" ) != std::string::npos, "wizard names the kingdom and asks a question" );
		check( off( "mission_type_spy" ) && off( "mission_type_raid" ) && !off( "mission_type_emissary" ), "missions the kingdom does not allow are unavailable" );
		check( el( "mission_type_emissary" )->HasAttribute( "checked" ) && el( "mission_action_improve" )->HasAttribute( "checked" ), "a legal default is chosen" );
		click( "mission_action_ambassador" );
		check( controller.state().missionDraft.action == MissionAction::InviteAmbassador && port.count( "diplomacy.start_mission" ) == 0, "choosing a task starts nothing" );
		controller.applyAvailableGnomes( { WorldEpoch { 15 }, Revision { 1 }, { AvailableGnomeRow { CreatureId { 71 }, "Ada" }, AvailableGnomeRow { CreatureId { 72 }, "Bert" } } } );
		update( *c );
		click( "wizard_next" );
		check( shown( "wizard_page_citizen" ) && !off( "wizard_back" ) && controller.state().missionDraft.creature == CreatureId { 71 } && !off( "wizard_next" ), "Citizen page starts with a default citizen (every wizard page has defaults)" );
		click( "diplomacy_gnome_72" );
		check( controller.state().missionDraft.creature == CreatureId { 72 }, "clicking a citizen chooses them" );
		click( "wizard_next" );
		check( shown( "wizard_page_review" ) && !shown( "wizard_next" ) && shown( "mission_start" ) && !off( "mission_start" ), "Review shows Finish in place of Next" );
		check( inner( "review_destination" ) == "Hill Gnomes" && inner( "review_mission" ).find( "Emissary" ) != std::string::npos && inner( "review_citizen" ) == "Bert", "review states kingdom, mission and citizen" );
		click( "wizard_back" );
		check( shown( "wizard_page_citizen" ) && controller.state().missionDraft.creature == CreatureId { 72 }, "Back keeps the choices" );
		click( "wizard_next" );
		// A change after review is caught at Finish.
		controller.selectMissionGnome( CreatureId { 71 } );
		update( *c );
		n = port.sent.size();
		click( "mission_start" );
		check( port.count( "diplomacy.start_mission" ) == 0 && shown( "wizard_page_mission" ) && inner( "mission_types_note" ).find( "changed since you reviewed" ) != std::string::npos, "a changed mission must be reviewed again" );
		click( "wizard_next" );
		click( "wizard_next" );
		click( "mission_start" );
		{
			check( port.count( "diplomacy.start_mission" ) == 1, "Finish sends one mission" );
			const auto p = std::get<StartMissionPayload>( std::find_if( port.sent.begin(), port.sent.end(), []( const auto& a ) { return a.id.value == "diplomacy.start_mission"; } )->payload );
			check( p.neighbor == NeighborId { 1 } && p.creature == CreatureId { 71 } && p.type == MissionType::Emissary && p.action == MissionAction::InviteAmbassador, "the mission carries the reviewed kingdom, task and citizen" );
		}
		check( !shown( "mission_wizard" ) && controller.state().diplomacyView == View::Missions, "Finish closes the wizard and shows Missions" );
		click( "mission_start" );
		check( port.count( "diplomacy.start_mission" ) == 1, "a second Finish cannot send again" );
		// Cancel and Esc send nothing.
		controller.open( View::Neighbors );
		update( *c );
		click( "mission_wizard_open" );
		key( Rml::Input::KI_ESCAPE );
		check( !shown( "mission_wizard" ) && port.count( "diplomacy.start_mission" ) == 1, "Esc cancels the wizard" );
		// A kingdom that disappears closes the wizard.
		click( "mission_wizard_open" );
		controller.applyNeighbors( { WorldEpoch { 15 }, Revision { 2 }, { kingdom( 3, "Red Goblins", true, true ) } } );
		update( *c );
		check( !shown( "mission_wizard" ), "a removed kingdom closes the wizard" );

		// ---------------------------------------------------------------- Missions
		MissionRow running;
		running.id           = MissionId { 9 };
		running.type         = MissionType::Emissary;
		running.action       = MissionAction::InviteAmbassador;
		running.step         = MissionStep::Travel;
		running.target       = NeighborId { 3 };
		running.elapsedHours = 5;
		MissionRow done      = running;
		done.id              = MissionId { 10 };
		done.step            = MissionStep::Returned;
		done.result.success  = false;
		controller.applyMissions( { WorldEpoch { 15 }, Revision { 1 }, { running, done } } );
		controller.open( View::Missions );
		update( *c );
		click( "diplomacy_mission_9" );
		check( inner( "mission_result" ) == "Not reported yet" && inner( "mission_timing" ) == "5 hours so far" && inner( "mission_step" ) == "Travelling", "a running mission is not shown as finished" );
		click( "diplomacy_mission_10" );
		check( inner( "mission_result" ) == "Did not succeed" && inner( "mission_timing" ) == "Unknown", "a returned mission shows its reported result and Unknown duration" );

		// ---------------------------------------------------------------- Fit
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			const int k = std::max( 1, int( scale + 0.5f ) );
			c->SetDimensions( { 384 * k, 380 * k } );
			for ( auto view : { View::Neighbors, View::Missions } )
			{
				controller.open( view );
				update( *c );
				auto* page  = el( view == View::Neighbors ? "diplomacy_neighbors_page" : "diplomacy_missions_page" );
				auto* close = el( "diplomacy_close_button" );
				check( page->GetScrollHeight() <= page->GetClientHeight() + 1.f && close->GetAbsoluteTop() + close->GetOffsetHeight() <= 380.f * k + 1.f, "page and Close inside the sheet" );
			}
			controller.open( View::Neighbors );
			click( "diplomacy_neighbor_3" );
			click( "mission_wizard_open" );
			for ( int page = 0; page < 3; ++page )
			{
				auto* finish = el( page == 2 ? "mission_start" : "wizard_next" );
				auto* body   = el( page == 0 ? "wizard_page_mission" : page == 1 ? "wizard_page_citizen" : "wizard_page_review" );
				if ( !( body->GetScrollHeight() <= body->GetClientHeight() + 1.f && finish->GetAbsoluteTop() + finish->GetOffsetHeight() <= 380.f * k + 1.f ) )
					std::cerr << "wizard page " << page << " scale=" << scale << " body=" << body->GetScrollHeight() << "/" << body->GetClientHeight() << " button=" << finish->GetAbsoluteTop() + finish->GetOffsetHeight() << std::endl;
				check( body->GetScrollHeight() <= body->GetClientHeight() + 1.f && finish->GetAbsoluteTop() + finish->GetOffsetHeight() <= 380.f * k + 1.f, "wizard page and buttons inside the window" );
				if ( page == 0 ) click( "wizard_next" );
				else if ( page == 1 ) { click( "diplomacy_gnome_71" ); click( "wizard_next" ); }
			}
			click( "wizard_cancel" );
		}
		c->SetDensityIndependentPixelRatio( 1.f );
		c->SetDimensions( { 384, 380 } );
		click( "diplomacy_close_button" );
		check( closed, "Close closes the window" );
		binding.shutdown();
	}
	Rml::RemoveContext( "stage15" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
