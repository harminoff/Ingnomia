/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 19: Settings as a Close-only property sheet, Pause as a dialog box, Loading as a progress message box, the
// shell's Exit / Main Menu message boxes, and the pending state after a world transition.
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
using namespace ingnomia::ui;
// Label text without the access-key underline markup (Stage 20).
inline std::string plain( const Rml::String& rml ) { std::string out; bool tag = false; for( char c : rml ) { if( c == '<' ) tag = true; else if( c == '>' ) tag = false; else if( !tag ) out += c; } return out; }
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
struct Commands : shell::ShellCommandPort
{
	std::vector<UiActionEnvelope> sent;
	bool pending = false;
	shell::CommandResult dispatch( const UiActionEnvelope& action ) override { sent.push_back( action ); shell::CommandResult r; r.pending = pending; return r; }
	size_t count( const char* id ) const { return std::count_if( sent.begin(), sent.end(), [&]( const auto& a ) { return a.id.value == id; } ); }
};
void update( Rml::Context& c )
{
	c.Update();
	if ( connected_tabs::reconcile( c ) ) c.Update();
}
shell::SettingRow row( const char* id, SettingValue value )
{
	shell::SettingRow r;
	r.id = SettingId { id };
	r.authoritative = value;
	r.draft = value;
	return r;
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
	auto* c = Rml::CreateContext( "stage19", { 1280, 800 } );
	{
		RmlUiQtInputAdapter input( c );
		Commands commands;
		shell::ShellRmlBinding view( *c );
		shell::ShellController controller( commands, view );
		check( view.initialize( controller ), "shell initializes" );
		update( *c );
		auto el    = [&]( const char* id ) { auto* e = view.routeDocument()->GetElementById( id ); check( e != nullptr, id ); return e; };
		auto shown = [&]( const char* id ) { return el( id )->IsVisible( true ); };
		auto off   = [&]( const char* id ) { return el( id )->HasAttribute( "disabled" ); };
		auto inner = [&]( const char* id ) { return plain( el( id )->GetInnerRML() ); };
		auto click = [&]( const char* id ) { el( id )->Click(); update( *c ); };
		auto modal = [&]() -> Rml::ElementDocument* {
			for ( int i = 0; i < c->GetNumDocuments(); ++i )
				if ( auto* d = c->GetDocument( i ); d->HasAttribute( "data-modal-dialog" ) && d->IsVisible() ) return d;
			return nullptr;
		};

		// ---------------------------------------------------------------- Settings property sheet
		click( "shell-settings" );
		check( controller.state().route.value == "shell.settings", "Settings opens" );
		shell::SettingsState settings;
		settings.status = shell::RequestStatus::Ready;
		settings.rows = { row( "display.fullscreen", false ), row( "display.follow_monitor_refresh", true ), row( "display.frame_rate_limit", std::int32_t( 60 ) ),
			row( "interface.ui_scale", 1.0f ), row( "display.minimum_light", std::int32_t( 0 ) ), row( "camera.keyboard_pan_speed", std::int32_t( 50 ) ),
			row( "camera.wheel_changes_level", true ), row( "audio.master_volume", std::int32_t( 80 ) ), row( "game.autosave_interval", std::int32_t( 3 ) ),
			row( "game.autosave_continue", false ) };
		controller.setSettingsState( settings );
		update( *c );
		check( el( "settings-sheet" )->IsClassSet( "w98-sheet" ) && shown( "settings-page-display" ) && !shown( "settings-page-sound" ), "Settings is a property sheet on Display" );
		check( inner( "setting-frame-rate-value" ) == "60 FPS" && inner( "setting-ui-scale-value" ) == "100%" && off( "setting-frame-rate" ), "slider values are shown; the frame rate waits for the monitor setting" );
		check( el( "shell-back" )->GetInnerRML() == "Close" && view.routeDocument()->GetElementById( "settings-apply" ) == nullptr, "Close only: settings apply at once" );
		const auto drafts = commands.count( "settings.set_draft" );
		click( "setting-fullscreen" );
		check( commands.count( "settings.set_draft" ) == drafts + 1, "a check box applies at once" );
		for ( const char* tab : { "settings-tab-controls", "settings-tab-sound", "settings-tab-saving", "settings-tab-display" } )
		{
			click( tab );
			const std::string page = std::string( "settings-page-" ) + ( tab + 13 );
			check( shown( page.c_str() ), "each tab shows its page" );
		}
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			c->SetDimensions( { int( 1280 * scale ), int( 800 * scale ) } );
			for ( const char* tab : { "settings-tab-display", "settings-tab-controls", "settings-tab-sound", "settings-tab-saving" } )
			{
				click( tab );
				const std::string page = std::string( "settings-page-" ) + ( tab + 13 );
				auto* p = el( page.c_str() );
				auto* close = el( "shell-back" );
				auto* sheet = el( "settings-sheet" );
				check( p->GetScrollHeight() <= p->GetClientHeight() + 1.f && close->GetAbsoluteTop() + close->GetOffsetHeight() <= sheet->GetAbsoluteTop() + sheet->GetOffsetHeight() + 1.f, "settings pages fit without scrolling" );
			}
		}
		c->SetDensityIndependentPixelRatio( 1.f );
		c->SetDimensions( { 1280, 800 } );
		click( "shell-back" );
		check( controller.state().route.value == "shell.main_menu", "Close returns to the main menu" );

		// ---------------------------------------------------------------- Exit asks in a Windows 98 message box
		click( "shell-exit" );
		auto* box = modal();
		check( box && box->GetElementById( "confirm-title" )->GetInnerRML() == "Ingnomia" && plain( box->GetElementById( "confirm-accept" )->GetInnerRML() ) == "Yes" && plain( box->GetElementById( "confirm-cancel" )->GetInnerRML() ) == "No", "Exit asks with a Yes / No message box captioned Ingnomia" );
		box->GetElementById( "confirm-cancel" )->Click();
		update( *c );
		check( modal() == nullptr && commands.count( "app.exit" ) == 0, "No keeps the game open" );

		// ---------------------------------------------------------------- Loading and a failed load
		controller.beginWorldTransition( false );
		update( *c );
		check( controller.state().route.value == "shell.loading" && view.routeDocument()->IsClassSet( "is-loading" ) && !shown( "loading-error-actions" ), "loading shows a progress message box without buttons" );
		controller.finishWorldTransition( false );
		update( *c );
		check( shown( "loading-error" ) && shown( "loading-retry" ) && shown( "shell-back" ) && view.routeDocument()->IsClassSet( "is-failed" ) && !el( "loading-symbol" )->IsClassSet( "is-info" ), "a failed load becomes a Warning message box with Retry and Cancel" );
		check( inner( "loading-error-detail" ).back() == '.', "the failure is one sentence" );
		click( "shell-back" );
		check( controller.state().route.value == "shell.main_menu", "Cancel returns to the main menu" );

		// ---------------------------------------------------------------- Continue, then Pause
		controller.setContinueAvailability( true, std::nullopt, "Valley", "today" );
		commands.pending = true;
		click( "shell-continue" );
		check( controller.state().pendingRequest.has_value(), "Continue is pending while the world loads" );
		controller.setWorld( WorldEpoch { 19 } );
		controller.finishWorldTransition( true );
		update( *c );
		check( controller.state().route.value == "game.hud" && !controller.state().pendingRequest, "the finished transition answers its own request" );
		commands.pending = false;
		controller.onPauseState( true, PauseReason::Player );
		check( controller.handleEscape(), "Esc in the game" );
		update( *c );
		check( controller.state().route.value == "game.pause" && view.routeDocument()->IsClassSet( "is-in-game" ), "Pause opens over the map" );
		check( !off( "pause-resume" ) && !off( "pause-save" ) && !off( "pause-settings" ) && !off( "pause-menu" ), "Pause commands are available" );
		click( "pause-settings" );
		check( controller.state().route.value == "game.settings" && view.routeDocument()->IsClassSet( "is-in-game" ), "Settings from Pause sits over the map" );
		click( "shell-back" );
		check( controller.state().route.value == "game.pause", "Close returns to Pause" );
		click( "pause-menu" );
		box = modal();
		check( box && std::string( box->GetElementById( "confirm-detail" )->GetInnerRML() ).find( "main menu" ) != std::string::npos, "Main Menu asks first" );
		box->GetElementById( "confirm-cancel" )->Click();
		update( *c );
		check( controller.state().route.value == "game.pause" && commands.count( "app.end_world" ) == 0, "No stays in the kingdom" );
		check( controller.handleEscape(), "Esc on Pause" );
		check( commands.count( "sim.set_paused" ) >= 1, "Esc on Pause resumes the game" );
		input.setContext( nullptr );
		view.shutdown();
	}
	Rml::RemoveContext( "stage19" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
