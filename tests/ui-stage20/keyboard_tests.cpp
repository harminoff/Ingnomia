/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 20: access keys, tab page keys, High Contrast colours, fit on a 640 x 480 screen, and titles for every window.
#include "gui/ui/runtime/RmlUiQtInputAdapter.h"
#include "gui/ui/runtime/ConnectedTabs.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/screens/shell/ShellRmlBinding.h"
#include <RmlUi/Core.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
using namespace ingnomia::ui;
int checks = 0;
void check( bool value, const std::string& label )
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
struct Commands : shell::ShellCommandPort
{
	std::vector<UiActionEnvelope> sent;
	shell::CommandResult dispatch( const UiActionEnvelope& action ) override { sent.push_back( action ); return {}; }
	size_t count( const char* id ) const { return std::count_if( sent.begin(), sent.end(), [&]( const auto& a ) { return a.id.value == id; } ); }
};
void update( Rml::Context& c )
{
	c.Update();
	if ( connected_tabs::reconcile( c ) ) c.Update();
}
// Every visible access key in a document is unique (PDF p.46).
void uniqueKeys( Rml::ElementDocument* document, const std::string& where )
{
	Rml::ElementList keys;
	document->QuerySelectorAll( keys, "[accesskey]" );
	std::set<char> seen;
	for ( auto* e : keys )
	{
		if ( !e->IsVisible( true ) ) continue;
		const char key = static_cast<char>( std::tolower( static_cast<unsigned char>( e->GetAttribute<Rml::String>( "accesskey", " " )[0] ) ) );
		check( seen.insert( key ).second, "access key " + std::string( 1, key ) + " unique in " + where );
		Rml::ElementList marks;
		e->GetElementsByClassName( marks, "w98-ak" );
		check( marks.size() == 1, "access key underlined once in " + where );
	}
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
	auto* c = Rml::CreateContext( "stage20", { 640, 480 } );
	{
		RmlUiQtInputAdapter input( c );
		Commands commands;
		shell::ShellRmlBinding view( *c );
		shell::ShellController controller( commands, view );
		check( view.initialize( controller ), "shell initializes" );
		update( *c );
		auto el  = [&]( const char* id ) { auto* e = view.routeDocument()->GetElementById( id ); check( e != nullptr, id ); return e; };
		auto key = [&]( int k, Qt::KeyboardModifiers mods = Qt::NoModifier ) { const auto r = input.keyDown( k, mods ); input.keyUp( k, mods ); update( *c ); return r.uiConsumed; };
		auto fits = [&]( const char* id, const std::string& what )
		{
			auto* e = el( id );
			check( e->GetAbsoluteLeft() >= 0.f && e->GetAbsoluteTop() >= 0.f && e->GetAbsoluteLeft() + e->GetOffsetWidth() <= 640.f && e->GetAbsoluteTop() + e->GetOffsetHeight() <= 480.f, what + " fits a 640 x 480 screen at 1x" );
		};

		// ---------------------------------------------------------------- Main menu
		fits( "shell-main-dialog", "main menu" );
		uniqueKeys( view.routeDocument(), "main menu" );
		el( "shell-load" )->Focus( true );
		check( key( Qt::Key_U ), "a plain letter on a button is an access key" );
		check( controller.state().route.value == "shell.new_game", "U opens Custom Game" );

		// ---------------------------------------------------------------- Wizard
		fits( "new-wizard", "Custom Game wizard" );
		for ( const char* page : { "world", "settlement", "terrain", "review" } )
		{
			check( view.activateElement( ( std::string( "new-tab-" ) + page ).c_str() ), "page" );
			update( *c );
			uniqueKeys( view.routeDocument(), std::string( "wizard page " ) + page );
		}
		check( view.activateElement( "new-tab-settlement" ), "settlement" );
		update( *c );
		el( "new-next" )->Focus( true );
		check( key( Qt::Key_K, Qt::AltModifier ), "Alt+K consumed" );
		check( c->GetFocusElement() == el( "new-kingdom-name" ), "Alt+K moves to the Kingdom name box through its label" );
		const auto page = controller.state().route.value;
		check( !key( Qt::Key_N ) || c->GetFocusElement() == el( "new-kingdom-name" ), "a plain letter in a text box is typed, not an access key" );
		check( el( "new-panel-settlement" )->IsVisible( true ), "typing N did not press Next" );
		check( key( Qt::Key_N, Qt::AltModifier ) && el( "new-panel-terrain" )->IsVisible( true ), "Alt+N presses Next" );
		check( key( Qt::Key_B, Qt::AltModifier ) && el( "new-panel-settlement" )->IsVisible( true ), "Alt+B presses Back" );
		el( "new-random-name" )->Focus( true );
		const auto names = commands.count( "new_game.randomize_name" );
		check( key( Qt::Key_R ) && commands.count( "new_game.randomize_name" ) == names + 1, "R presses Random Name" );
		const auto peaceful = el( "new-peaceful" )->HasAttribute( "checked" );
		check( key( Qt::Key_P ) && el( "new-peaceful" )->HasAttribute( "checked" ) != peaceful, "P toggles the Peaceful beginning check box" );
		(void)page;
		controller.handleEscape();
		update( *c );

		// ---------------------------------------------------------------- Settings: tab page keys and access keys
		controller.activate( shell::ShellControl::OpenSettings );
		{
			shell::SettingsState settings;
			settings.status = shell::RequestStatus::Ready;
			controller.setSettingsState( settings );
		}
		update( *c );
		fits( "settings-sheet", "Settings" );
		el( "settings-tab-display" )->Focus( true );
		check( key( Qt::Key_PageDown, Qt::ControlModifier ) && el( "settings-page-controls" )->IsVisible( true ), "Ctrl+Page Down goes to the next tab" );
		check( key( Qt::Key_PageUp, Qt::ControlModifier ) && el( "settings-page-display" )->IsVisible( true ), "Ctrl+Page Up goes to the previous tab" );
		check( key( Qt::Key_Right ) && el( "settings-page-controls" )->IsVisible( true ), "Right moves between tabs" );
		for ( const char* tab : { "settings-tab-display", "settings-tab-controls", "settings-tab-sound", "settings-tab-saving" } )
		{
			el( tab )->Click();
			update( *c );
			uniqueKeys( view.routeDocument(), tab );
		}
		controller.handleEscape();
		update( *c );

		// ---------------------------------------------------------------- Load Game
		controller.activate( shell::ShellControl::OpenLoadGame );
		update( *c );
		fits( "load-dialog", "Load Game" );
		uniqueKeys( view.routeDocument(), "Load Game" );
		controller.handleEscape();
		update( *c );

		// ---------------------------------------------------------------- Message box access keys
		el( "shell-exit" )->Click();
		update( *c );
		Rml::ElementDocument* box = nullptr;
		for ( int i = 0; i < c->GetNumDocuments(); ++i )
			if ( c->GetDocument( i )->HasAttribute( "data-modal-dialog" ) && c->GetDocument( i )->IsVisible() ) box = c->GetDocument( i );
		check( box && box->GetElementById( "confirm-accept" )->GetAttribute<Rml::String>( "accesskey", "" ) == "Y" && box->GetElementById( "confirm-cancel" )->GetAttribute<Rml::String>( "accesskey", "" ) == "N", "Yes and No have access keys" );
		{
			auto* dialog = box->GetFirstChild();
			check( dialog->GetAbsoluteLeft() + dialog->GetOffsetWidth() <= 640.f && dialog->GetAbsoluteTop() + dialog->GetOffsetHeight() <= 480.f, "message box fits a 640 x 480 screen" );
		}
		check( key( Qt::Key_N ) && commands.count( "app.exit" ) == 0, "N answers No" );

		// ---------------------------------------------------------------- High Contrast
		view.routeDocument()->SetClass( "is-high-contrast", true );
		update( *c );
		{
			auto* sheet = el( "shell-main-dialog" );
			const auto face = sheet->GetComputedValues().background_color();
			check( face.red == 255 && face.green == 255 && face.blue == 255, "High Contrast: white faces" );
			auto* caption = sheet->GetFirstChild();
			const auto bar = caption->GetComputedValues().background_color();
			check( bar.red == 0 && bar.green == 0 && bar.blue == 0, "High Contrast: a black caption" );
			const auto text = el( "shell-load" )->GetComputedValues().color();
			check( text.red == 0 && text.green == 0 && text.blue == 0, "High Contrast: black command text" );
		}
		view.routeDocument()->SetClass( "is-high-contrast", false );
		input.setContext( nullptr );
		view.shutdown();
	}

	// ---------------------------------------------------------------- Every window has a title (PDF p.372)
	for ( const char* path : { "screens/main_menu.rml", "screens/new_game.rml", "screens/load_game.rml", "screens/settings.rml", "screens/pause_menu.rml", "screens/loading.rml",
			"screens/inspector.rml", "screens/orders_tools.rml", "windows/population_manager.rml", "windows/military_manager.rml", "windows/diplomacy_missions.rml",
			"windows/workshop_manager.rml", "windows/stockpile_manager.rml", "panels/agriculture_manager.rml", "modals/win98_message_box.rml" } )
	{
		auto* document = c->LoadDocument( path );
		check( document != nullptr, path );
		check( !document->GetTitle().empty(), std::string( "window title in " ) + path );
		c->UnloadDocument( document );
	}
	c->Update();
	Rml::RemoveContext( "stage20" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
