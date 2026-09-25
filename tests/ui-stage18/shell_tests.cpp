/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Stage 18: the main menu as a dialog box, the Custom Game wizard and the Load Game dialog (Open dialog model).
#include "gui/ui/runtime/RmlUiQtInputAdapter.h"
#include "gui/ui/runtime/ClassicFocusDecorator.h"
#include "gui/ui/screens/shell/ShellRmlBinding.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
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
	shell::CommandResult dispatch( const UiActionEnvelope& action ) override { sent.push_back( action ); return {}; }
	size_t count( const char* id ) const { return std::count_if( sent.begin(), sent.end(), [&]( const auto& a ) { return a.id.value == id; } ); }
};

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
	auto* c = Rml::CreateContext( "stage18", { 1280, 800 } );
	{
		RmlUiQtInputAdapter input( c );
		Commands commands;
		shell::ShellRmlBinding view( *c );
		shell::ShellController controller( commands, view );
		check( view.initialize( controller ), "shell initializes" );
		controller.setVersion( "0.10.0.0" );
		c->Update();
		auto el    = [&]( const char* id ) { auto* e = view.routeDocument()->GetElementById( id ); check( e != nullptr, id ); return e; };
		auto shown = [&]( const char* id ) { return el( id )->IsVisible( true ); };
		auto off   = [&]( const char* id ) { return el( id )->HasAttribute( "disabled" ); };
		auto inner = [&]( const char* id ) { return plain( el( id )->GetInnerRML() ); };
		auto click = [&]( const char* id ) { el( id )->Click(); c->Update(); };
		auto key   = [&]( int k ) { input.keyDown( k, {} ); input.keyUp( k, {} ); c->Update(); };
		auto focused = [&] { return c->GetFocusElement() ? std::string( c->GetFocusElement()->GetId() ) : std::string{}; };

		// ---------------------------------------------------------------- Main menu dialog
		check( el( "shell-main-dialog" )->IsClassSet( "w98-sheet" ) && el( "shell-main-dialog" )->IsClassSet( "l-shell-dialog" ), "main menu is a dialog box" );
		check( inner( "shell-version" ) == "0.10.0.0", "version shown" );
		check( off( "shell-continue" ) && inner( "shell-continue-reason" ) == "No compatible save found", "Continue unavailable says why" );
		check( focused() == "shell-load", "without a save the first available command has the focus" );
		controller.setContinueAvailability( true, std::nullopt, "Land of Mice", "9/24/2026" );
		c->Update();
		check( !off( "shell-continue" ) && focused() == "shell-continue", "Continue becomes the default command once a save is found" );
		{
			const char* commandsInOrder[] = { "shell-continue", "shell-load", "shell-tutorial", "shell-new-default", "shell-new-setup", "shell-settings", "shell-exit" };
			float previousTop = -1.f, width = el( "shell-continue" )->GetOffsetWidth();
			for ( const char* id : commandsInOrder )
			{
				check( el( id )->GetAbsoluteTop() > previousTop && std::abs( el( id )->GetOffsetWidth() - width ) < 0.5f, "commands stacked in one column with equal widths" );
				previousTop = el( id )->GetAbsoluteTop();
			}
			check( inner( "shell-load" ) == "Load Game..." && inner( "shell-new-setup" ) == "Custom Game..." && inner( "shell-settings" ) == "Settings", "ellipsis only where more input follows" );
		}

		// ---------------------------------------------------------------- Custom Game wizard
		click( "shell-new-setup" );
		check( controller.state().route.value == "shell.new_game", "Custom Game opens the wizard" );
		check( shown( "new-panel-welcome" ) && off( "new-back" ) && shown( "new-next" ) && !shown( "new-start" ) && focused() == "new-next", "Welcome page, Back unavailable, Next has the focus" );
		key( Qt::Key_Return );
		check( shown( "new-panel-world" ) && !off( "new-back" ), "Enter chooses Next" );
		click( "new-next" );
		check( shown( "new-panel-settlement" ), "Settlement page" );
		click( "new-random-name" );
		check( commands.count( "new_game.randomize_name" ) == 1, "Random Name asks the game" );
		click( "new-next" );
		check( shown( "new-panel-terrain" ), "Terrain and Life page" );
		click( "new-next" );
		check( shown( "new-panel-review" ) && !shown( "new-next" ) && shown( "new-start" ) && focused() == "new-start", "Completion: Finish replaces Next and has the focus" );
		click( "new-back" );
		check( shown( "new-panel-terrain" ), "Back returns a page" );
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			c->SetDimensions( { int( 1280 * scale ), int( 800 * scale ) } );
			for ( const char* page : { "welcome", "world", "settlement", "terrain", "review" } )
			{
				check( view.activateElement( ( std::string( "new-tab-" ) + page ).c_str() ), "page" );
				c->Update();
				auto* panel = el( ( std::string( "new-panel-" ) + page ).c_str() );
				auto* buttons = el( "shell-back" );
				auto* wizard = el( "new-wizard" );
				check( panel->GetScrollHeight() <= panel->GetClientHeight() + 1.f && buttons->GetAbsoluteTop() + buttons->GetOffsetHeight() <= wizard->GetAbsoluteTop() + wizard->GetOffsetHeight() + 1.f, "wizard page and buttons fit" );
			}
		}
		c->SetDensityIndependentPixelRatio( 1.f );
		c->SetDimensions( { 1280, 800 } );
		click( "shell-back" );
		check( controller.state().route.value == "shell.main_menu" && focused() == "shell-new-setup", "Cancel returns to the command that opened the wizard (NEW-002)" );

		// ---------------------------------------------------------------- Load Game dialog
		click( "shell-load" );
		check( controller.state().route.value == "shell.load_game", "Load Game opens" );
		shell::LoadGameState load;
		load.kingdomsStatus = shell::RequestStatus::Ready;
		load.kingdoms = { { SaveKingdomId { "mice" }, "Land of Mice", 1758700000 }, { SaveKingdomId { "valley" }, "Tutorial Valley", 1758600000 } };
		controller.setLoadGameState( load );
		c->Update();
		auto* kingdoms = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>( el( "load-kingdoms" ) );
		check( kingdoms && kingdoms->GetNumOptions() == 3, "Look in lists the kingdoms after a blank entry" );
		kingdoms->SetValue( "1" );
		kingdoms->DispatchEvent( "change", Rml::Dictionary {} );
		c->Update();
		check( controller.state().loadGame.selectedKingdom && controller.state().loadGame.selectedKingdom->relativeKey == "valley" && commands.count( "load.select_kingdom" ) == 1, "choosing Look in asks for that kingdom's saves" );
		load.selectedKingdom = SaveKingdomId { "valley" };
		load.savesStatus = shell::RequestStatus::Ready;
		load.saves = { { SaveSlotId { "valley/2" }, "Autumn", "0.10.0.0", 1758700000, true }, { SaveSlotId { "valley/1" }, "Old", "0.8.0", 1758000000, false } };
		controller.setLoadGameState( load );
		c->Update();
		auto* rows = el( "load-saves" );
		check( rows->GetNumChildren() == 2 && rows->GetChild( 0 )->GetNumChildren() == 3, "saves listed with Name, Modified and Version" );
		check( off( "load-selected" ), "Open unavailable until a save is chosen" );
		auto* first = el( "save-row-0" );
		click( "save-row-0" );
		check( el( "save-row-0" ) == first && first->IsClassSet( "is-selected" ), "a click selects the row in place; the list is not rebuilt" );
		check( !off( "load-selected" ) && std::string( rmlui_dynamic_cast<Rml::ElementFormControlInput*>( el( "load-file-name" ) )->GetValue() ) == "Autumn", "the save name is shown and Open is available" );
		click( "save-row-1" );
		check( off( "load-selected" ) && inner( "load-status" ).find( "cannot be opened" ) != std::string::npos, "an incompatible save says why Open is unavailable" );
		{
			Rml::Dictionary p;
			p["key_identifier"] = static_cast<int>( Rml::Input::KI_F5 );
			const auto before = commands.count( "load.refresh" );
			el( "load-dialog" )->DispatchEvent( "keydown", p );
			c->Update();
			check( commands.count( "load.refresh" ) == before + 1, "F5 refreshes the list" );
		}
		const auto loads = commands.count( "app.load_game" );
		el( "save-row-0" )->DispatchEvent( "dblclick", Rml::Dictionary {} );
		c->Update();
		check( commands.count( "app.load_game" ) == loads + 1, "a double-click opens the save" );
		for ( float scale : { 1.f, 1.25f, 1.5f, 2.f } )
		{
			c->SetDensityIndependentPixelRatio( scale );
			c->SetDimensions( { int( 1280 * scale ), int( 800 * scale ) } );
			if ( controller.state().route.value != "shell.load_game" ) break;
			c->Update();
			auto* dialog = el( "load-dialog" );
			auto* cancel = el( "shell-back" );
			check( cancel->GetAbsoluteTop() + cancel->GetOffsetHeight() <= dialog->GetAbsoluteTop() + dialog->GetOffsetHeight() + 1.f, "Load Game buttons inside the dialog" );
		}
		input.setContext( nullptr );
		view.shutdown();
	}
	Rml::RemoveContext( "stage18" );
	Rml::Shutdown();
	std::cout << checks << " checks passed" << std::endl;
}
