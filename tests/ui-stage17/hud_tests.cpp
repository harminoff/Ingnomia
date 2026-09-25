/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 17: the game window's toolbar, drop-down menus and status bar, the Build palette, the tutorial palette and the
// event message box.
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/screens/hud/HudRmlBinding.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
using namespace ingnomia::ui;
using namespace ingnomia::ui::hud;
int checks = 0;
void check( bool value, const char* label )
{
	++checks;
	if ( std::getenv( "STAGE_TRACE" ) ) std::cerr << "ok " << label << std::endl;
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
	Rml::TextureHandle LoadTexture( Rml::Vector2i& d, const Rml::String& ) override { d = { 2, 2 }; return 1; }
	Rml::TextureHandle GenerateTexture( Rml::Span<const Rml::byte>, Rml::Vector2i ) override { return 1; }
	void ReleaseTexture( Rml::TextureHandle ) override {}
	void EnableScissorRegion( bool ) override {}
	void SetScissorRegion( Rml::Rectanglei ) override {}
};
struct Port final : HudCommandPort
{
	std::vector<UiActionEnvelope> sent;
	int buildRequests = 0;
	CommandResult dispatch( const UiActionEnvelope& a ) override { sent.push_back( a ); return {}; }
	CommandResult requestBuildItems( BuildSelection, std::string_view ) override { ++buildRequests; return {}; }
	int count( const char* id ) { return static_cast<int>( std::count_if( sent.begin(), sent.end(), [&]( auto& a ) { return a.id.value == id; } ) ); }
	template <class T> T last( const char* id )
	{
		const auto it = std::find_if( sent.rbegin(), sent.rend(), [&]( auto& a ) { return a.id.value == id; } );
		check( it != sent.rend(), id );
		return std::get<T>( it->payload );
	}
};
BuildCatalogRow item( const char* id, const char* name, const char* type, bool available )
{
	BuildCatalogRow row;
	row.id = CatalogId { id };
	row.name = name;
	row.type = type;
	row.available = available;
	BuildCatalogRow::RequiredComponent part;
	part.item = CatalogId { "Plank" };
	part.amount = 2;
	part.options = { { CatalogId { "Oak" }, 5 }, { CatalogId { "Pine" }, 3 } };
	part.selected = CatalogId { "Oak" };
	row.components = { part };
	row.defaultMaterials = { CatalogId { "Oak" } };
	if ( !available ) row.unavailableReason = "Plank x2";
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
	auto* game = Rml::CreateContext( "stage17", { 1600, 900 } );
	auto* buildWindow = Rml::CreateContext( "stage17-build", { 384, 380 } );
	{
		Port port;
		HudRmlBinding hud( *game );
		HudController controller( port, hud );
		check( hud.initialize( controller ), "HUD loads" );
		HudRmlBinding build( *buildWindow, HudRmlBinding::Presentation::OrdersTools, HudRmlBinding::ToolPanel::Build );
		controller.addViewPort( build );
		check( build.initialize( controller ), "Build window loads" );
		std::vector<bool> cursor;
		hud.setToolCursorHandler( [&]( bool armed ) { cursor.push_back( armed ); } );
		auto* doc = hud.document();
		auto el    = [&]( const std::string& id ) { auto* e = doc->GetElementById( id ); check( e != nullptr, id.c_str() ); return e; };
		auto click = [&]( const std::string& id ) { el( id )->DispatchEvent( "click", Rml::Dictionary {} ); game->Update(); };
		auto inner = [&]( const std::string& id ) { return std::string( el( id )->GetInnerRML() ); };
		auto shown = [&]( const std::string& id ) { return el( id )->IsVisible( true ); };
		auto off   = [&]( const std::string& id ) { return el( id )->HasAttribute( "disabled" ); };
		auto set   = [&]( const std::string& id ) { return el( id )->IsClassSet( "is-selected" ); };
		auto hover = [&]( const std::string& id, bool on ) { el( id )->DispatchEvent( on ? "mouseover" : "mouseout", Rml::Dictionary {} ); game->Update(); };
		controller.beginWorld( WorldEpoch { 17 } );
		controller.setSettlement( { "Land of Mice", 5, 2, 996 } );
		CameraState camera;
		camera.viewLevel = 70;
		camera.minLevel = 0;
		camera.maxLevel = 70;
		controller.setCamera( camera );
		ClockCalendarState clock;
		clock.day = 8;
		clock.year = 1;
		clock.hour = 6;
		clock.minute = 5;
		clock.daylight = DaylightPhase::Day;
		controller.setClock( clock );
		game->Update();

		// ---------------------------------------------------------------- Toolbar and status bar
		check( el( "hud_top_rail" )->IsClassSet( "w98-toolbar" ) && el( "hud_status_bar" )->IsClassSet( "w98-statusbar" ), "toolbar and status bar" );
		check( el( "hud_top_rail" )->GetAbsoluteTop() == 0.f && el( "hud_status_bar" )->GetAbsoluteTop() + el( "hud_status_bar" )->GetOffsetHeight() <= 900.f + 0.5f, "toolbar along the top, status bar along the bottom" );
		check( el( "hud_top_rail" )->GetOffsetHeight() < 40.f, "the whole toolbar fits on one row at 1x" );
		// Labels are laid out (flex items need an element around their text), so every button shows its name.
		for ( const char* id : { "hud_pause", "hud_open_population", "hud_tool_mine", "hud_tool_view", "hud_mine_walls" } )
			check( el( id )->GetNumChildren() > 0 && el( id )->GetLastChild()->GetClientWidth() + el( id )->GetChild( 0 )->GetClientWidth() > 20.f || std::string( id ) == "hud_mine_walls", "toolbar labels have width" );
		check( inner( "hud_status" ) == "Ready" && inner( "hud_level" ) == "Level 70" && inner( "hud_date" ) == "Day 8, Year 1" && inner( "hud_clock" ) == "06:05" && inner( "hud_daylight" ) == "Day", "status bar panes" );
		check( inner( "hud_gnomes" ) == "5 gnomes" && inner( "hud_kingdom" ) == "Land of Mice", "settlement panes" );
		check( off( "hud_level_up" ) && !off( "hud_level_down" ), "Level Up unavailable at the top level" );
		check( off( "hud_tool_cancel" ) && off( "hud_tool_rotate" ), "Cancel Tool and Rotate unavailable without a tool" );
		hover( "hud_open_population", true );
		check( inner( "hud_status" ).rfind( "Opens the citizens", 0 ) == 0, "status bar describes the control under the pointer" );
		hover( "hud_open_population", false );
		hover( "hud_tool_cancel", true );
		check( inner( "hud_status" ).find( "Unavailable because no tool is active." ) != std::string::npos, "an unavailable control says why" );
		hover( "hud_tool_cancel", false );
		check( inner( "hud_status" ) == "Ready", "status returns to Ready" );
		check( set( "hud_speed_normal" ) && !set( "hud_speed_fast" ) && !set( "hud_pause" ), "Normal Speed is set" );
		clock.paused = true;
		controller.setClock( clock );
		game->Update();
		check( set( "hud_pause" ) && inner( "hud_pause" ).find( ">Pause</span>" ) != std::string::npos, "a paused game shows Pause set; the label does not change" );
		click( "hud_speed_fast" );
		check( port.count( "sim.set_speed" ) == 1, "Fast Speed asks the game" );

		// ---------------------------------------------------------------- Menus and armed tools
		click( "hud_tool_mine" );
		check( shown( "hud_mine_menu" ) && el( "hud_tool_mine" )->IsClassSet( "is-open" ) && el( "hud_tool_mine" )->GetAttribute<Rml::String>( "aria-expanded", "" ) == "true", "Mine opens its menu and stays pressed" );
		check( std::abs( el( "hud_mine_menu" )->GetAbsoluteLeft() - el( "hud_tool_mine" )->GetAbsoluteLeft() ) < 1.f && el( "hud_mine_menu" )->GetAbsoluteTop() >= el( "hud_tool_mine" )->GetAbsoluteTop() + el( "hud_tool_mine" )->GetOffsetHeight() - 1.f, "the menu drops down under its button" );
		click( "hud_tool_agriculture" );
		check( !shown( "hud_mine_menu" ) && shown( "hud_agriculture_menu" ), "only one menu is open" );
		{
			Rml::Dictionary p;
			p["key_identifier"] = static_cast<int>( Rml::Input::KI_ESCAPE );
			el( "hud_tool_agriculture" )->DispatchEvent( "keydown", p );
			game->Update();
			check( !shown( "hud_agriculture_menu" ), "Esc closes the menu" );
		}
		click( "hud_tool_mine" );
		click( "hud_mine_walls" );
		check( port.last<ActivateToolPayload>( "tool.activate" ).tool.value == "mine", "Mine Walls arms the mine tool" );
		check( !shown( "hud_mine_menu" ) && set( "hud_tool_mine" ), "choosing an item closes the menu; Mine shows the mode" );
		check( shown( "hud_active_tool" ) && inner( "hud_active_tool" ) == "Mine Walls" && inner( "hud_status" ).find( "Esc cancels" ) != std::string::npos, "status bar names the mode and how to leave it" );
		check( !cursor.empty() && cursor.back(), "the map pointer shows the armed tool" );
		check( !off( "hud_tool_cancel" ), "Cancel Tool available with a tool" );
		click( "hud_tool_mine" );
		check( el( "hud_mine_walls" )->IsClassSet( "is-chosen" ) && !el( "hud_mine_hole" )->IsClassSet( "is-chosen" ), "the menu marks the chosen tool with a dot" );
		click( "hud_tool_mine" );
		click( "hud_tool_cancel" );
		check( port.count( "tool.cancel" ) == 1 && !set( "hud_tool_mine" ) && !cursor.back(), "Cancel Tool leaves the mode and restores the pointer" );
		click( "hud_tool_designations" );
		click( "hud_tool_farm" );
		check( port.last<ActivateToolPayload>( "tool.activate" ).tool.value == "create_farm" && set( "hud_tool_designations" ) && !set( "hud_tool_mine" ), "Farm arms and Designations shows the mode" );
		click( "hud_tool_cancel" );

		// ---------------------------------------------------------------- View settings
		click( "hud_tool_view" );
		click( "hud_overlay_jobs" );
		check( port.last<OverlayPayload>( "view.set_overlay" ).overlay == OverlayKind::Jobs && port.last<OverlayPayload>( "view.set_overlay" ).enabled && !shown( "hud_view_menu" ), "Jobs turns on and the menu closes" );
		RenderOverlayState overlays;
		overlays.jobs = true;
		controller.setOverlays( overlays );
		game->Update();
		check( el( "hud_overlay_jobs" )->IsClassSet( "is-checked" ) && !el( "hud_overlay_walls" )->IsClassSet( "is-checked" ), "a setting that is on has a check mark" );

		// ---------------------------------------------------------------- Build palette
		{
			auto* bdoc = build.document();
			auto bel    = [&]( const std::string& id ) { auto* e = bdoc->GetElementById( id ); check( e != nullptr, id.c_str() ); return e; };
			auto bclick = [&]( const std::string& id ) { bel( id )->DispatchEvent( "click", Rml::Dictionary {} ); buildWindow->Update(); game->Update(); };
			buildWindow->Update();
			check( bel( "hud_build_panel" )->IsClassSet( "w98-palette" ) && bel( "hud_build_panel" )->IsVisible( true ), "Build is a palette window" );
			check( bel( "hud_build_hint" )->IsVisible( true ) && !bel( "hud_build_catalog" )->IsVisible( true ), "Build asks for a category first" );
			bclick( "hud_build_furniture" );
			check( port.buildRequests == 1 && bel( "hud_build_furniture" )->IsClassSet( "is-selected" ), "choosing a category asks the game for its items" );
			controller.setBuildCatalog( { item( "Chair", "Chair", "Wood", true ), item( "Table", "Table", "Wood", false ), item( "StoneChair", "Stone chair", "Stone", true ) } );
			buildWindow->Update();
			check( bel( "hud_build_catalog" )->IsVisible( true ) && bel( "hud_build_types_page" )->IsVisible( true ), "items and the Material drop-down appear" );
			check( bel( "hud_build_items" )->GetNumChildren() == 2 && bel( "hud_build_Chair" )->IsClassSet( "is-selected" ), "Wood items listed, the first one chosen" );
			check( std::string( bel( "hud_build_Table" )->GetChild( 1 )->GetInnerRML() ) == "Needs items", "a missing item says so" );
			bclick( "hud_build_Table" );
			check( bel( "hud_build_Table" )->IsClassSet( "is-selected" ) && std::string( bel( "hud_build_action_Build_Table" )->GetInnerRML() ) == "Place Blueprint", "choosing an item shows its commands; missing items place a blueprint" );
			check( port.count( "tool.choose_build" ) == 0, "choosing an item arms nothing" );
			auto* type = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( bel( "hud_build_type_select" ) );
			check( type != nullptr, "type drop-down" );
			type->SetValue( "Stone" );
			buildWindow->Update();
			check( bel( "hud_build_items" )->GetNumChildren() == 1 && bel( "hud_build_StoneChair" )->IsClassSet( "is-selected" ), "the Material drop-down filters the items" );
			auto* material = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( bel( "hud_build_material_StoneChair_0" ) );
			check( material != nullptr, "material drop-down" );
			material->SetValue( "Pine" );
			buildWindow->Update();
			check( controller.state().buildCatalog[2].components[0].selected.value == "Pine", "a component material is chosen" );
			bclick( "hud_build_action_Build_StoneChair" );
			check( port.count( "tool.choose_build" ) == 1 && port.last<ChooseBuildPayload>( "tool.choose_build" ).item.value == "StoneChair", "Build arms placement of the chosen item" );
			check( set( "hud_tool_build" ) && inner( "hud_active_tool" ) == "Build" && !off( "hud_tool_rotate" ), "the game window shows the Build mode; Rotate is available" );
			for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
			{
				buildWindow->SetDensityIndependentPixelRatio( scale );
				const int k = std::max( 1, int( scale + 0.5f ) );
				buildWindow->SetDimensions( { 384 * k, 380 * k } );
				buildWindow->Update();
				auto* panel = bel( "hud_build_panel" );
				auto* button = bel( "hud_build_action_Build_StoneChair" );
				check( panel->GetOffsetHeight() <= 380.f * k + 1.f && button->GetAbsoluteTop() + button->GetOffsetHeight() <= 380.f * k + 1.f && button->GetAbsoluteLeft() + button->GetOffsetWidth() <= 384.f * k + 1.f, "Build window content inside 384 x 380" );
			}
			buildWindow->SetDensityIndependentPixelRatio( 1.f );
			buildWindow->SetDimensions( { 384, 380 } );
			click( "hud_tool_cancel" );
		}

		// ---------------------------------------------------------------- Tutorial palette
		TutorialViewState tutorial;
		tutorial.active = true;
		tutorial.hintsEnabled = true;
		tutorial.step = 1;
		tutorial.completedMask = 3; // lessons 1 and 2 done: Continue is offered for the current lesson
		tutorial.title = "Interactive Tutorial";
		tutorial.objective = "Inspect a gnome.";
		tutorial.steps = { "Click Inspect.", "Click a gnome." };
		tutorial.completedSteps = { true, false };
		tutorial.progress = "2 of 9";
		controller.setTutorial( tutorial );
		game->Update();
		check( shown( "hud_tutorial_panel" ) && el( "hud_tutorial_panel" )->IsClassSet( "w98-palette" ), "tutorial is a palette window" );
		check( el( "tutorial_checklist" )->GetNumChildren() == 9 && el( "tutorial_steps" )->GetNumChildren() == 2, "lessons and steps listed" );
		check( el( "tutorial_steps" )->GetChild( 0 )->GetChild( 0 )->HasAttribute( "checked" ) && !el( "tutorial_steps" )->GetChild( 1 )->GetChild( 0 )->HasAttribute( "checked" ), "done steps are checked" );
		check( inner( "tutorial-hints" ) == "Hide Hints" && shown( "tutorial-continue" ), "tutorial commands" );
		click( "tutorial-continue" );
		check( port.count( "tutorial.advance" ) == 1, "Continue advances the tutorial" );
		tutorial.active = false;
		controller.setTutorial( tutorial );

		// ---------------------------------------------------------------- Event message box
		check( controller.enqueuePrompt( std::nullopt, "Caravan", "A caravan arrived.", false, false ), "prompt" );
		check( controller.enqueuePrompt( std::nullopt, "Second", "Another event.", false, false ), "second prompt" );
		game->Update();
		check( shown( "hud_event_blocker" ) && inner( "hud_event_title" ) == "Caravan" && shown( "hud_event_ack" ) && !shown( "hud_event_yes" ), "one information message box at a time" );
		check( game->GetFocusElement() == el( "hud_event_ack" ), "OK has the focus" );
		click( "hud_event_ack" );
		check( port.count( "event.respond" ) == 1 && inner( "hud_event_title" ) == "Second", "OK answers it and the next one follows" );
		click( "hud_event_ack" );
		check( !shown( "hud_event_blocker" ), "no message box left" );

		// ---------------------------------------------------------------- Fit of the toolbar at every scale
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			game->SetDensityIndependentPixelRatio( scale );
			game->SetDimensions( { int( 1600 * scale ), int( 900 * scale ) } );
			game->Update();
			auto* rail = el( "hud_top_rail" );
			auto* last = el( "hud_tool_view" );
			check( last->GetAbsoluteLeft() + last->GetOffsetWidth() <= 1600.f * scale + 1.f && rail->GetOffsetHeight() < 90.f * scale, "toolbar buttons all inside the window" );
			auto* bar = el( "hud_status_bar" );
			check( bar->GetAbsoluteTop() + bar->GetOffsetHeight() <= 900.f * scale + 1.f, "status bar inside the window" );
		}
		build.shutdown();
		hud.shutdown();
	}
	Rml::RemoveContext( "stage17" );
	Rml::RemoveContext( "stage17-build" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
